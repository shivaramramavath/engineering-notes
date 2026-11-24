# Retries, Backoff, and Dead-Letter Queues (DLQ)

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [02-bullmq](./02-bullmq.md), [03-producers-and-consumers](./03-producers-and-consumers.md)

---

## 1. Context & Purpose

Background jobs frequently interact with volatile external services: sending transactional SMS via Twilio, charging payment cards via Stripe, or calling third-party webhooks. Transient network drops, rate limits (HTTP 429), or temporary third-party database locks cause jobs to fail unpredictably.

A production queue system must handle transient failures automatically without dropping jobs or bombarding failing services. When all retry attempts are exhausted, failed jobs must not simply disappear: they must be moved to a **Dead-Letter Queue (DLQ)** for inspection, alerting, and manual or automated replay once upstream issues resolve.

---

## 2. Theoretical Foundations

### 2.1 Backoff Strategies
When a job fails, retrying immediately is counter-productive (it intensifies thundering herds and triggers rate limits).
1. **Fixed Delay**: Retries after a constant interval ($T$).
2. **Exponential Backoff**: Delay doubles on each consecutive attempt:
   $$\text{Delay} = \text{InitialDelay} \times 2^{(\text{Attempt} - 1)}$$
3. **Jitter**: Adds pseudo-random variation to the delay to prevent synchronized retries across hundreds of failed workers.

### 2.2 Unrecoverable Failures
Not all errors are transient. If a job fails due to an invalid credit card number (400 Bad Request) or a non-existent database foreign key, retrying 5 times is pointless. Workers must distinguish between **retryable errors** (503 Service Unavailable, network timeouts) and **terminal errors** (validation failures).

---

## 3. Architecture & Internal Mechanics

```
Producer adds Job (attempts: 3, backoff: exponential)
               │
               ▼
┌───────────────────────────┐
│     Main Worker Loop      │
└──────────────┬────────────┘
               │
      Executes Attempt #1
               │
      ┌────────┴────────┐
      ▼                 ▼
   [Success]         [Failure] (Transient Error)
      │                 │
   Complete             ├─> Attempts < Max?
                        │   Yes: Move to Delayed Set (Delay = 2s)
                        │   Retry Attempt #2 (Delay = 4s)
                        │   Retry Attempt #3 (Delay = 8s)
                        │
                        ▼ (Attempts == 3 Exhausted)
┌────────────────────────────────────────────────────────┐
│               Dead-Letter Pipeline (DLQ)               │
│  - Captures full stack trace and input payload         │
│  - Emits Alert to Sentry / PagerDuty                   │
│  - Moves job to `mail:failed` or `mail-dlq` queue      │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Configuring Exponential Backoff with Jitter

```typescript
// src/notifications/notifications-producer.service.ts
import { Injectable } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class NotificationsProducerService {
  constructor(@InjectQueue('notifications') private readonly queue: Queue) {}

  async dispatchSms(phone: string, message: string) {
    return this.queue.add(
      'send-sms',
      { phone, message },
      {
        attempts: 5, // Maximum 5 attempts
        backoff: {
          type: 'exponential',
          delay: 2000, // 2s, 4s, 8s, 16s, 32s
        },
        removeOnComplete: 100,
        removeOnFail: false, // Keep exhausted jobs in Redis!
      },
    );
  }
}
```

---

### 4.2 Distinguishing Retryable vs Terminal Errors in Consumer

```typescript
// src/notifications/notifications.consumer.ts
import { Processor, WorkerHost, OnWorkerEvent } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job, UnrecoverableError } from 'bullmq';

@Processor('notifications')
export class NotificationsConsumer extends WorkerHost {
  private readonly logger = new Logger(NotificationsConsumer.name);

  async process(job: Job<{ phone: string; message: string }>): Promise<void> {
    this.logger.log(`Attempting SMS delivery to ${job.data.phone} (Attempt ${job.attemptsMade + 1}/${job.opts.attempts})`);

    // 1. Terminal Error Check: Invalid Phone Number format
    if (!job.data.phone.startsWith('+')) {
      this.logger.error(`Invalid phone number: ${job.data.phone}. Marking terminal.`);
      // BullMQ UnrecoverableError skips all remaining retries immediately!
      throw new UnrecoverableError('Malformed E.164 phone number. Retries aborted.');
    }

    // 2. Simulate Transient Network Call
    try {
      await this.callSmsProvider(job.data.phone, job.data.message);
    } catch (err: any) {
      if (err.status === 429) {
        this.logger.warn(`Rate limited by SMS provider. Exponential backoff will handle retry.`);
      }
      throw err; // Re-throw to trigger standard BullMQ retry backoff
    }
  }

  private async callSmsProvider(phone: string, msg: string) {
    // Simulated transient network failure
    if (Math.random() < 0.5) {
      const error: any = new Error('Gateway Timeout');
      error.status = 504;
      throw error;
    }
  }
}
```

---

### 4.3 Automated Dead-Letter Queue (DLQ) Transfer Listener

Listen for failed events and transfer exhausted jobs to a dedicated Dead-Letter Queue:

```typescript
// src/notifications/dlq-transfer.service.ts
import { Injectable, OnModuleInit, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue, QueueEvents } from 'bullmq';

@Injectable()
export class DlqTransferService implements OnModuleInit {
  private readonly logger = new Logger(DlqTransferService.name);
  private queueEvents!: QueueEvents;

  constructor(
    @InjectQueue('notifications') private readonly mainQueue: Queue,
    @InjectQueue('notifications-dlq') private readonly dlqQueue: Queue,
  ) {}

  onModuleInit() {
    this.queueEvents = new QueueEvents('notifications', {
      connection: { host: 'localhost', port: 6379 },
    });

    // Listen to exhausted failures across the entire cluster
    this.queueEvents.on('failed', async ({ jobId, failedReason }) => {
      const job = await this.mainQueue.getJob(jobId);
      if (!job) return;

      // Check if all configured attempts have been exhausted
      if (job.attemptsMade >= (job.opts.attempts || 1)) {
        this.logger.error(
          `[DLQ ALERT] Job ${jobId} permanently failed: ${failedReason}. Moving to DLQ.`,
        );

        // Move payload to Dead-Letter Queue for operator review
        await this.dlqQueue.add('dead-job', {
          originalJobId: jobId,
          originalName: job.name,
          data: job.data,
          failedReason,
          stacktrace: job.stacktrace,
          failedAt: new Date().toISOString(),
        });

        // Trigger alert to PagerDuty or Sentry
        // await this.alertingService.sendAlert(...)
      }
    });
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Memory Bloat from Retained Failed Jobs
If `removeOnFail: false` is configured and an outage causes 100,000 jobs to fail, Redis memory fills up with error stack traces.
- **Handling**: Configure `removeOnFail: { count: 1000 }` to keep only the most recent failures, or stream failures to PostgreSQL / ElasticSearch.

### 5.2 Infinite Retries
Omitting `attempts` or setting `attempts: 0` causes jobs to retry indefinitely upon every failure, creating an infinite worker loop.
- **Rule**: Always specify a finite `attempts` count (e.g., 3–5).

---

## 6. Performance & Memory Implications

- **Backoff CPU Efficiency**: Backoff jobs reside in Redis Sorted Sets (`delayed`), consuming zero worker CPU while waiting for their retry window.
- **Redis Lua Performance**: Job transitions between active and delayed states occur via atomic Redis Lua scripts in < 1ms.

---

## 7. Anti-Patterns & Common Mistakes

- **Catching and Swallowing Errors in Workers**: Wrapping the entire `process()` body in `try/catch` and logging without re-throwing. BullMQ treats the job as **completed successfully** and never retries!
- **Retrying Validation Errors**: Allowing malformed payloads to retry 5 times before failing. Use `UnrecoverableError` immediately.
- **Ignoring the DLQ**: Accumulating thousands of jobs in a DLQ without building an administrative dashboard or replay mechanism.

---

## 8. Production Checklist & Operational Readiness

- [ ] `attempts: 3` to `5` configured on all background jobs.
- [ ] `backoff: { type: 'exponential', delay: ... }` configured.
- [ ] Non-retryable errors throw `UnrecoverableError`.
- [ ] DLQ alerting connected to monitoring systems (Slack, PagerDuty).
- [ ] CLI or API endpoint implemented to replay DLQ jobs (`job.retry()`).

---

## 9. Migration & Version History

- **BullMQ 1.x**: Added native `UnrecoverableError` class to short-circuit retries.
- **BullMQ 4.x/5.x**: Improved QueueEvents stream reliability and parent-child failure cascading.

---

## 10. Verification & Testing

Testing that `UnrecoverableError` aborts retries:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { NotificationsConsumer } from './notifications.consumer';
import { UnrecoverableError } from 'bullmq';

describe('NotificationsConsumer (Retries)', () => {
  let consumer: NotificationsConsumer;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [NotificationsConsumer],
    }).compile();

    consumer = module.get<NotificationsConsumer>(NotificationsConsumer);
  });

  it('throws UnrecoverableError on invalid phone number to abort retries', async () => {
    const mockJob: any = {
      data: { phone: 'invalid-number', message: 'Hello' },
      attemptsMade: 0,
      opts: { attempts: 5 },
    };

    await expect(consumer.process(mockJob)).rejects.toThrow(UnrecoverableError);
  });
});
```

---

## 11. Comprehensive Examples

### Administrative DLQ Replay Service

```typescript
// src/admin/dlq-management.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class DlqManagementService {
  private readonly logger = new Logger(DlqManagementService.name);

  constructor(
    @InjectQueue('notifications') private readonly mainQueue: Queue,
    @InjectQueue('notifications-dlq') private readonly dlqQueue: Queue,
  ) {}

  async replayAllDlqJobs(): Promise<{ replayed: number }> {
    const dlqJobs = await this.dlqQueue.getJobs(['waiting', 'completed']);
    let count = 0;

    for (const dlqJob of dlqJobs) {
      const { originalName, data } = dlqJob.data;

      // Re-insert into main queue for reprocessing
      await this.mainQueue.add(originalName, data, { attempts: 3 });
      await dlqJob.remove();
      count++;
    }

    this.logger.log(`Successfully replayed ${count} jobs from DLQ.`);
    return { replayed: count };
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-bullmq.md](./02-bullmq.md) - BullMQ setup
- [03-producers-and-consumers.md](./03-producers-and-consumers.md) - Producer patterns
- [06-job-idempotency.md](./06-job-idempotency.md) - Idempotent job handling
- [BullMQ Retries Guide](https://docs.bullmq.io/guide/retries)
