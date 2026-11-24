# Queue Producers and Consumers Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [01-queues-fundamentals](./01-queues-fundamentals.md), [02-bullmq](./02-bullmq.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

In production microservice systems, scaling queue producers and consumers requires architectural separation. Ingesting jobs (producers) and executing jobs (consumers) have vastly different resource characteristics:
- **Producers**: Run within user-facing HTTP containers (I/O bound, low memory, fast autoscaling based on HTTP request concurrency).
- **Consumers**: Run in dedicated worker pods (CPU-intensive, memory-heavy, autoscale based on Redis queue depth or lag metrics).

This guide covers advanced patterns for designing clean, type-safe producers and resilient, decoupled consumers in NestJS 11 using `@nestjs/bullmq`.

---

## 2. Theoretical Foundations

### 2.1 The Producer Contract
Producers must guarantee:
1. **Schema Validation**: Validate job parameters before queue insertion (e.g. via Zod or DTOs).
2. **Determinism**: Assign deterministic job IDs (`jobId`) when duplicate submissions must be prevented.
3. **Low Latency**: Never execute heavy business logic before queue insertion.

### 2.2 The Consumer Lifecycle
A consumer passes through distinct states:
$$\text{Waiting} \longrightarrow \text{Active} \longrightarrow \begin{cases} \text{Completed} \\ \text{Failed (Retry)} \\ \text{Failed (Exhausted)} \end{cases}$$
Workers must report granular progress (`job.updateProgress(percent)`) so clients can track long-running jobs.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   HTTP API Pods (Producers)            │
│       OrdersController ──> OrdersProducerService       │
└───────────────────────────┬────────────────────────────┘
                            │
               queue.add('process-order', payload)
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│                    Redis Queue Broker                  │
│                     `bull:orders:wait`                 │
└───────────────────────────┬────────────────────────────┘
                            │
              Worker pulls job via BRPOPLPUSH
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│              Dedicated Worker Pods (Consumers)         │
│                 OrdersConsumer (WorkerHost)            │
│  - Executes Payment Settlement                         │
│  - Updates job progress: 25% -> 50% -> 100%            │
│  - Emits telemetry and completes job                   │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Type-Safe Job Definition

```typescript
// src/orders/jobs/order-jobs.interface.ts
export interface ProcessOrderJobData {
  orderId: string;
  userId: string;
  items: Array<{ sku: string; quantity: number; price: number }>;
  paymentToken: string;
}

export interface OrderJobResult {
  transactionId: string;
  settledAt: string;
}
```

---

### 4.2 Dedicated Producer Service

```typescript
// src/orders/orders-producer.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';
import { ProcessOrderJobData, OrderJobResult } from './jobs/order-jobs.interface';

@Injectable()
export class OrdersProducerService {
  private readonly logger = new Logger(OrdersProducerService.name);

  constructor(
    @InjectQueue('orders')
    private readonly ordersQueue: Queue<ProcessOrderJobData, OrderJobResult, string>,
  ) {}

  async submitOrderJob(data: ProcessOrderJobData) {
    this.logger.log(`Queueing order processing for orderId: ${data.orderId}`);

    // Set jobId = orderId to prevent duplicate processing
    const job = await this.ordersQueue.add('process-order', data, {
      jobId: `order:${data.orderId}`,
      attempts: 5,
      backoff: {
        type: 'exponential',
        delay: 2000,
      },
      removeOnComplete: 500,
    });

    return {
      jobId: job.id,
      queuedAt: new Date().toISOString(),
    };
  }
}
```

---

### 4.3 Production Consumer with Granular Progress Updates

```typescript
// src/orders/orders.consumer.ts
import { Processor, WorkerHost, OnWorkerEvent } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job } from 'bullmq';
import { ProcessOrderJobData, OrderJobResult } from './jobs/order-jobs.interface';
import { PaymentGatewayService } from '../payments/payment-gateway.service';

@Processor('orders', {
  concurrency: 10, // Process 10 orders concurrently per worker instance
})
export class OrdersConsumer extends WorkerHost {
  private readonly logger = new Logger(OrdersConsumer.name);

  constructor(private readonly paymentGateway: PaymentGatewayService) {
    super();
  }

  async process(
    job: Job<ProcessOrderJobData, OrderJobResult, string>,
  ): Promise<OrderJobResult> {
    this.logger.log(`Starting processing for order ${job.data.orderId} (Attempt #${job.attemptsMade + 1})`);

    // Step 1: Validate Inventory (25% progress)
    await job.updateProgress(25);
    await new Promise((r) => setTimeout(r, 200));

    // Step 2: Charge Customer (50% progress)
    await job.updateProgress(50);
    const totalAmount = job.data.items.reduce((sum, item) => sum + item.price * item.quantity, 0);
    const charge = await this.paymentGateway.charge(job.data.paymentToken, totalAmount);

    // Step 3: Generate Invoice (75% progress)
    await job.updateProgress(75);
    await new Promise((r) => setTimeout(r, 200));

    // Step 4: Finalize Order (100% progress)
    await job.updateProgress(100);

    return {
      transactionId: charge.transactionId,
      settledAt: new Date().toISOString(),
    };
  }

  @OnWorkerEvent('progress')
  onProgress(job: Job, progress: number) {
    this.logger.log(`Job ${job.id} progress updated: ${progress}%`);
  }

  @OnWorkerEvent('completed')
  onCompleted(job: Job, result: OrderJobResult) {
    this.logger.log(`Order ${job.data.orderId} finalized with TxID: ${result.transactionId}`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Long-Running Jobs Losing Lock
If a job takes 60 seconds to process a batch and the lock duration is 30 seconds, a stalled-job checker might mark the job as abandoned and reassign it to another worker.
- **Handling**: BullMQ automatically renews locks in the background, but if the Node.js event loop is completely blocked by synchronous CPU computations, the lock renewal timer cannot fire.
- **Remedy**: Never run synchronous blocking loops in workers; yield periodically via `setImmediate()` or offload CPU tasks to Node.js Worker Threads (`worker_threads`).

### 5.2 Deserialization of Dates and Complex Types
Redis serializes job data as JSON. JavaScript `Date` instances sent by producers arrive at consumers as ISO date strings (`"2026-03-30T..."`), not `Date` objects.
- **Handling**: Parse date strings explicitly in consumers or validate with Zod schemas.

---

## 6. Performance & Memory Implications

- **Concurrency Tuning**:
  $$\text{Concurrency} = \text{CPU Cores} \times 2 \quad (\text{for I/O heavy tasks})$$
  Setting concurrency to 500 on an I/O worker is feasible, but CPU-intensive transcoding must cap concurrency at CPU core counts to prevent thread thrashing.
- **Progress Throttling**: Avoid calling `job.updateProgress()` hundreds of times a second; each progress update triggers a Redis `HSET` command.

---

## 7. Anti-Patterns & Common Mistakes

- **Running Consumers in HTTP Pods**: Deploying workers on the same pods serving public HTTP requests. A CPU spike in the worker causes HTTP health checks to fail, triggering Kubernetes pod restarts.
- **Assuming Execution Order**: Enqueueing Job A and Job B and expecting Job A to always finish before Job B. Distributed consumers execute concurrently; order is not guaranteed unless parent-child Flows are used.

---

## 8. Production Checklist & Operational Readiness

- [ ] Producers and consumers decoupled into separate deployable application targets (or controlled via environment flags: `ENABLE_WORKER=true`).
- [ ] Concurrency explicitly configured on `@Processor()`.
- [ ] Payloads contain primitive identifiers, not complex class instances.
- [ ] External calls in workers wrapped in strict timeout boundaries.
- [ ] Job progress emitted at major milestone steps.

---

## 9. Migration & Version History

- **BullMQ 3 to 5**: Added sandboxed processor support and fine-grained lock renewal configurations.
- **@nestjs/bullmq**: Native support for `@Processor()` options and typed `WorkerHost`.

---

## 10. Verification & Testing

Testing worker processing logic:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { OrdersConsumer } from './orders.consumer';
import { PaymentGatewayService } from '../payments/payment-gateway.service';

describe('OrdersConsumer', () => {
  let consumer: OrdersConsumer;
  let mockPayment: any;

  beforeEach(async () => {
    mockPayment = {
      charge: vi.fn().mockResolvedValue({ transactionId: 'tx_999' }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        OrdersConsumer,
        { provide: PaymentGatewayService, useValue: mockPayment },
      ],
    }).compile();

    consumer = module.get<OrdersConsumer>(OrdersConsumer);
  });

  it('processes job and updates progress stages', async () => {
    const mockJob: any = {
      id: 'job_1',
      data: {
        orderId: 'ord_123',
        userId: 'u1',
        items: [{ sku: 'A', quantity: 1, price: 100 }],
        paymentToken: 'tok_123',
      },
      attemptsMade: 0,
      updateProgress: vi.fn().mockResolvedValue(undefined),
    };

    const result = await consumer.process(mockJob);
    expect(result.transactionId).toBe('tx_999');
    expect(mockJob.updateProgress).toHaveBeenCalledWith(25);
    expect(mockJob.updateProgress).toHaveBeenCalledWith(50);
    expect(mockJob.updateProgress).toHaveBeenCalledWith(100);
  });
});
```

---

## 11. Comprehensive Examples

### Parent-Child Job Flow (Order -> Payment -> Shipping)

```typescript
// src/orders/orders-flow.service.ts
import { Injectable } from '@nestjs/common';
import { FlowProducer } from 'bullmq';

@Injectable()
export class OrdersFlowService {
  private readonly flowProducer: FlowProducer;

  constructor() {
    this.flowProducer = new FlowProducer({
      connection: { host: 'localhost', port: 6379 },
    });
  }

  async createOrderWorkflow(orderId: string) {
    // Parent job (Shipping) runs ONLY after child jobs (Payment & Inventory) succeed
    const flow = await this.flowProducer.add({
      name: 'dispatch-shipping',
      queueName: 'shipping',
      data: { orderId },
      children: [
        { name: 'settle-payment', queueName: 'payments', data: { orderId } },
        { name: 'reserve-inventory', queueName: 'inventory', data: { orderId } },
      ],
    });

    return flow;
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-bullmq.md](./02-bullmq.md) - BullMQ setup
- [04-retries-and-dead-letter-queues.md](./04-retries-and-dead-letter-queues.md) - Retry strategies
- [07-queue-scaling-and-performance.md](./07-queue-scaling-and-performance.md) - Scaling workers
- [BullMQ Flows Documentation](https://docs.bullmq.io/guide/flows)
