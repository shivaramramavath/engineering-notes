# Queue Job Idempotency and Deduplication

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [01-queues-fundamentals](./01-queues-fundamentals.md), [02-bullmq](./02-bullmq.md), [04-intermediate/08-api-design/06-idempotency](../../04-intermediate/08-api-design/06-idempotency.md)

---

## 1. Context & Purpose

In distributed message processing, **at-least-once delivery** is the industry standard. Due to network timeouts, worker crashes, or lock expiration races, the queue system may deliver the exact same job to a worker more than once. Furthermore, producers (such as webhook receivers or user interfaces) may enqueue duplicate jobs if clients double-click or retry requests.

If a worker processing payouts, charging credit cards, or provisioning licenses is not **idempotent**, duplicate executions cause catastrophic real-world errors (double charges, duplicate emails, inventory inconsistencies). Ensuring complete **Job Idempotency and Deduplication** across both producer and consumer layers is mandatory for production reliability.

---

## 2. Theoretical Foundations

An operation is idempotent if executing it multiple times produces the exact same outcome as executing it once:
$$f(f(x)) = f(x)$$

### 2.1 Defense in Depth (Two-Layer Idempotency)
1. **Producer-Level Deduplication**: Assigning a unique deterministic `jobId` in BullMQ. If a job with `jobId: "order:12345"` already exists in the queue (in `wait`, `active`, or `delayed` states), BullMQ ignores the duplicate `add()` call atomically.
2. **Consumer-Level Idempotency**: Before executing side effects, the worker checks an atomic database or Redis lock (`processed_jobs` table). If the job ID was already processed, the worker acknowledges the job and exits immediately.

---

## 3. Architecture & Internal Mechanics

```
PRODUCER DEDUPLICATION:
Client Submits Order #100
           │
           ▼
Producer: queue.add('charge', data, { jobId: 'charge:order:100' })
           │
  ┌────────┴────────┐
  ▼                 ▼
[First Submission]  [Duplicate Submission]
Job stored in Redis BullMQ ignores add()
                     (Returns existing job)

CONSUMER IDEMPOTENCY:
Worker pulls Job (jobId: 'charge:order:100')
           │
           ▼
Check Database: SELECT 1 FROM processed_jobs WHERE job_id = $1
           │
  ┌────────┴────────┐
  ▼                 ▼
[Record Exists]     [Record Absent]
Log: "Already done"   │
ACK and Exit!         ├─> BEGIN SQL TRANSACTION
                      ├─> Charge Customer Card
                      ├─> INSERT INTO processed_jobs (job_id, status)
                      ├─> COMMIT TRANSACTION
                      │
                      ▼
                   ACK Job
```

---

## 4. Concrete Implementation

### 4.1 Producer-Level Deduplication via Deterministic `jobId`

```typescript
// src/billing/billing-producer.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class BillingProducerService {
  private readonly logger = new Logger(BillingProducerService.name);

  constructor(@InjectQueue('billing') private readonly billingQueue: Queue) {}

  async queueSubscriptionCharge(invoiceId: string, amount: number) {
    // Deterministic jobId guarantees only one charge job exists for this invoice
    const jobId = `charge:invoice:${invoiceId}`;

    const job = await this.billingQueue.add(
      'charge-invoice',
      { invoiceId, amount },
      {
        jobId,
        removeOnComplete: 1000,
        attempts: 3,
        backoff: { type: 'exponential', delay: 2000 },
      },
    );

    this.logger.log(`Queued billing charge for invoice ${invoiceId} (JobID: ${job.id})`);
    return job;
  }
}
```

---

### 4.2 Consumer-Level Atomic Idempotency Pattern

Using an atomic database transaction table (`processed_jobs`):

```typescript
// src/billing/billing.consumer.ts
import { Processor, WorkerHost } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job } from 'bullmq';
import { DataSource } from 'typeorm';
import { PaymentGatewayService } from '../payments/payment-gateway.service';

@Processor('billing')
export class BillingConsumer extends WorkerHost {
  private readonly logger = new Logger(BillingConsumer.name);

  constructor(
    private readonly dataSource: DataSource,
    private readonly paymentGateway: PaymentGatewayService,
  ) {
    super();
  }

  async process(job: Job<{ invoiceId: string; amount: number }>): Promise<void> {
    const { invoiceId, amount } = job.data;
    const idempotencyKey = `job:${job.id}`;

    this.logger.log(`Processing invoice charge: ${invoiceId} (JobID: ${job.id})`);

    // Run within database transaction with atomic idempotency insertion
    await this.dataSource.transaction(async (manager) => {
      // 1. Attempt to insert into processed_jobs table
      // If key already exists, ON CONFLICT DO NOTHING returns 0 rows affected
      const insertResult = await manager.query(
        `INSERT INTO processed_jobs (job_id, created_at)
         VALUES ($1, NOW())
         ON CONFLICT (job_id) DO NOTHING`,
        [idempotencyKey],
      );

      // In Postgres, rowCount == 0 means record already existed (duplicate run!)
      if (insertResult.length === 0 && insertResult.rowCount === 0) {
        this.logger.warn(`Job ${job.id} was already executed previously. Skipping duplicate.`);
        return; // Idempotently exit with success!
      }

      // 2. Execute the actual side-effect (charging payment)
      await this.paymentGateway.chargeInvoice(invoiceId, amount);

      // 3. Update invoice status
      await manager.query(
        `UPDATE invoices SET status = 'PAID', paid_at = NOW() WHERE id = $1`,
        [invoiceId],
      );

      this.logger.log(`Successfully charged invoice: ${invoiceId}`);
    });
  }
}
```

---

### 4.3 Redis-Based Fast Lock for Idempotent Workers

If using NoSQL or external services where an SQL transaction is unavailable:

```typescript
// src/common/workers/redis-idempotency.service.ts
import { Injectable } from '@nestjs/common';
import { Redis } from 'ioredis';

@Injectable()
export class RedisIdempotencyService {
  constructor(private readonly redis: Redis) {}

  async acquireExecutionLock(key: string, ttlSeconds = 86400): Promise<boolean> {
    // SET NX returns 'OK' if key did not exist, null if it already existed
    const result = await this.redis.set(`idemp:${key}`, 'COMPLETED', 'EX', ttlSeconds, 'NX');
    return result === 'OK';
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Producer Deduplication after Job Completion
In BullMQ, if `removeOnComplete: true` is configured, as soon as a job finishes, its record is deleted from Redis. If a producer re-adds the same `jobId` 10 seconds later, BullMQ treats it as a brand new job!
- **Rule**: If you need multi-day deduplication, either keep completed jobs (`removeOnComplete: { count: 10000, age: 86400 }`) or enforce consumer-level database idempotency checks.

### 5.2 Network Drops After External Payment but Before Database Commit
If the worker charges Stripe successfully, but the network drops before writing the `processed_jobs` record to PostgreSQL:
- On retry, the worker sees no DB record and attempts to charge Stripe again!
- **Remedy**: Pass the deterministic `job.id` as the `Idempotency-Key` header directly to the third-party API (Stripe, Twilio). The third party will also deduplicate the request!

---

## 6. Performance & Memory Implications

- **Producer Check**: `jobId` deduplication in BullMQ executes in Redis in $< 0.1\text{ms}$.
- **Database Indexing**: The `processed_jobs` table requires a single primary key or unique index on `job_id`, resulting in $O(1)$ B-Tree lookups.

---

## 7. Anti-Patterns & Common Mistakes

- **Generating Random UUIDs for Idempotent Jobs**: Calling `queue.add('charge', data, { jobId: randomUUID() })`. Every call gets a new UUID, defeating deduplication completely.
- **Checking Idempotency in Memory**: Using an in-memory `Set` in the worker process. The set is lost when the pod restarts and is not shared across other worker pods.
- **Failing the Job on Duplicate Detection**: Throwing an error when a duplicate is found. The queue will think the job failed and retry it! Return cleanly with a log statement.

---

## 8. Production Checklist & Operational Readiness

- [ ] Deterministic `jobId` configured on all mutation producers.
- [ ] Unique index exists on `processed_jobs(job_id)`.
- [ ] Third-party API calls forward `job.id` as their client `Idempotency-Key`.
- [ ] Completed job retention configured with appropriate count or time window.
- [ ] Duplicate executions logged with structured warning logs for auditing.

---

## 9. Migration & Version History

- **BullMQ 2.x/3.x**: Unified `jobId` deduplication across active, delayed, and wait sets.
- **BullMQ 5.x**: Improved atomic Lua script deduplication pipelines.

---

## 10. Verification & Testing

Testing worker idempotency logic:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { BillingConsumer } from './billing.consumer';
import { DataSource } from 'typeorm';
import { PaymentGatewayService } from '../payments/payment-gateway.service';

describe('BillingConsumer (Idempotency)', () => {
  let consumer: BillingConsumer;
  let mockPayment: any;
  let mockQuery: any;

  beforeEach(async () => {
    mockPayment = { chargeInvoice: vi.fn().mockResolvedValue(true) };
    mockQuery = vi.fn();

    const mockDataSource: any = {
      transaction: async (cb: any) => cb({ query: mockQuery }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        BillingConsumer,
        { provide: DataSource, useValue: mockDataSource },
        { provide: PaymentGatewayService, useValue: mockPayment },
      ],
    }).compile();

    consumer = module.get<BillingConsumer>(BillingConsumer);
  });

  it('skips payment execution if job was already processed', async () => {
    // Simulate duplicate run (rowCount = 0)
    mockQuery.mockResolvedValueOnce({ rowCount: 0 });

    const mockJob: any = {
      id: 'charge:invoice:123',
      data: { invoiceId: '123', amount: 50 },
    };

    await consumer.process(mockJob);

    // Payment gateway must NOT be called on duplicate
    expect(mockPayment.chargeInvoice).not.toHaveBeenCalled();
  });
});
```

---

## 11. Comprehensive Examples

### PostgreSQL `processed_jobs` Schema Migration

```sql
-- migrations/create_processed_jobs.sql
CREATE TABLE IF NOT EXISTS processed_jobs (
  job_id VARCHAR(255) PRIMARY KEY,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() NOT NULL
);

-- Automated cleanup index for records older than 30 days
CREATE INDEX IF NOT EXISTS idx_processed_jobs_created_at
ON processed_jobs (created_at);
```

---

## 12. Cross-References & Further Reading

- [01-queues-fundamentals.md](./01-queues-fundamentals.md) - Delivery guarantees
- [04-intermediate/08-api-design/06-idempotency.md](../../04-intermediate/08-api-design/06-idempotency.md) - HTTP API idempotency
- [BullMQ Job Deduplication Guide](https://docs.bullmq.io/patterns/job-deduplication)
