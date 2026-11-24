# BullMQ Integration and Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [01-queues-fundamentals](./01-queues-fundamentals.md), [05-advanced/01-caching/03-redis](../01-caching/03-redis.md)

---

## 1. Context & Purpose

**BullMQ** is the leading NodeJS distributed message queue library, built from the ground up in TypeScript on top of Redis Streams and Lua scripts. Compared to legacy Bull 3, BullMQ offers strictly typed jobs, parent-child job workflows (Flows), concurrency sandboxing via child processes, native rate limiting, and zero polling overhead.

Through **`@nestjs/bullmq`**, NestJS provides first-class declarative integration for BullMQ: `@Processor()`, `@Process()`, `@OnQueueEvent()`, and `@InjectQueue()`. This guide covers BullMQ configuration, Redis connection pooling, and queue setup in NestJS 11.

---

## 2. Theoretical Foundations

BullMQ organizes jobs across Redis data structures:
- **`wait`**: Redis Stream of jobs waiting for worker pickup.
- **`active`**: Set of currently executing jobs holding a lock.
- **`delayed`**: Sorted Set (ZSET) ordered by timestamp of future execution.
- **`completed` / `failed`**: Sets retaining job metadata and error traces for auditing.

### 2.1 Job Lock Renewal
When a worker picks up a job, BullMQ acquires a distributed lock in Redis with a default duration (e.g. 30 seconds). As long as the worker is alive, a background timer automatically renews the lock. If the worker process crashes abruptly, the lock expires and another worker reclaims the job.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   NestJS Application                   │
└──────────────┬──────────────────────────┬──────────────┘
               │                          │
        @InjectQueue('mail')     @Processor('mail')
               │                          │
               ▼                          ▼
┌───────────────────────────┐  ┌───────────────────────────┐
│     Queue (Producer)      │  │    Worker (Consumer)      │
│  await queue.add('send')  │  │   async process(job)      │
└──────────────┬────────────┘  └──────────▲────────────────┘
               │                          │
               ▼                          │
┌─────────────────────────────────────────┴──────────────┐
│                    Redis 7.x Server                    │
│   - bull:mail:wait (Stream)                            │
│   - bull:mail:active (Set with auto-renewing locks)    │
│   - bull:mail:delayed (ZSET for scheduled jobs)        │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/bullmq bullmq ioredis
```

### 4.2 Registering `BullModule` Globally

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { BullModule } from '@nestjs/bullmq';
import { ConfigModule, ConfigService } from '@nestjs/config';
import { MailModule } from './mail/mail.module';

@Module({
  imports: [
    ConfigModule.forRoot({ isGlobal: true }),
    // Global BullMQ connection configuration
    BullModule.forRootAsync({
      imports: [ConfigModule],
      useFactory: (config: ConfigService) => ({
        connection: {
          host: config.get<string>('REDIS_HOST', 'localhost'),
          port: config.get<number>('REDIS_PORT', 6379),
          password: config.get<string>('REDIS_PASSWORD'),
        },
        defaultJobOptions: {
          removeOnComplete: 1000, // Keep last 1,000 completed jobs for auditing
          removeOnFail: 5000,     // Keep last 5,000 failed jobs for debugging
          attempts: 3,            // Default retry attempts
          backoff: {
            type: 'exponential',
            delay: 1000,          // 1s, 2s, 4s...
          },
        },
      }),
      inject: [ConfigService],
    }),
    MailModule,
  ],
})
export class AppModule {}
```

---

### 4.3 Registering Feature Queues

```typescript
// src/mail/mail.module.ts
import { Module } from '@nestjs/common';
import { BullModule } from '@nestjs/bullmq';
import { MailProducerService } from './mail-producer.service';
import { MailProcessor } from './mail.processor';

@Module({
  imports: [
    BullModule.registerQueue({
      name: 'mail',
    }),
  ],
  providers: [MailProducerService, MailProcessor],
  exports: [MailProducerService],
})
export class MailModule {}
```

---

### 4.4 Queue Producer Service

```typescript
// src/mail/mail-producer.service.ts
import { Injectable } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

export interface SendWelcomeEmailJob {
  userId: string;
  email: string;
  name: string;
}

@Injectable()
export class MailProducerService {
  constructor(@InjectQueue('mail') private readonly mailQueue: Queue) {}

  async queueWelcomeEmail(data: SendWelcomeEmailJob) {
    const job = await this.mailQueue.add('send-welcome', data, {
      priority: 1, // Higher priority jobs are processed first
    });

    return { jobId: job.id, status: 'enqueued' };
  }
}
```

---

### 4.5 Queue Consumer (Worker Processor)

```typescript
// src/mail/mail.processor.ts
import { Processor, WorkerHost, OnWorkerEvent } from '@nestjs/bullmq';
import { Logger } from '@nestjs/common';
import { Job } from 'bullmq';
import { SendWelcomeEmailJob } from './mail-producer.service';

@Processor('mail', {
  concurrency: 5, // Process up to 5 jobs simultaneously per pod
})
export class MailProcessor extends WorkerHost {
  private readonly logger = new Logger(MailProcessor.name);

  // Main execution entrypoint for all jobs in the 'mail' queue
  async process(job: Job<SendWelcomeEmailJob, void, string>): Promise<void> {
    this.logger.log(`Processing job ${job.id} of type [${job.name}] for user ${job.data.email}`);

    switch (job.name) {
      case 'send-welcome':
        await this.handleWelcomeEmail(job.data);
        break;
      default:
        this.logger.warn(`Unknown job name: ${job.name}`);
    }
  }

  private async handleWelcomeEmail(data: SendWelcomeEmailJob) {
    // Simulate sending email via SMTP / SendGrid
    await new Promise((resolve) => setTimeout(resolve, 500));
    this.logger.log(`Welcome email successfully sent to ${data.email}`);
  }

  @OnWorkerEvent('completed')
  onCompleted(job: Job) {
    this.logger.log(`Job ${job.id} completed successfully.`);
  }

  @OnWorkerEvent('failed')
  onFailed(job: Job, err: Error) {
    this.logger.error(`Job ${job.id} failed after attempts: ${err.message}`, err.stack);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Connection Limits in Managed Redis (e.g. AWS ElastiCache)
BullMQ creates multiple Redis connections per queue and worker (one for standard commands, one blocking connection for `BRPOPLPUSH`/Stream reads, and one for event listeners). Running 50 pods each with 10 queues can exhaust Redis connection limits.
- **Handling**: Use shared Redis connection objects via `BullModule.forRootAsync` rather than declaring unique connection objects per queue.

### 5.2 Worker Starvation on Unhandled Promise Hangs
If a third-party API call inside `process()` hangs without a timeout, the job never completes and keeps renewing its lock, blocking worker concurrency.
- **Rule**: Always enforce an explicit timeout on all external I/O within worker processors.

---

## 6. Performance & Memory Implications

- **Redis Stream Architecture**: BullMQ uses Redis Streams (`XADD`, `XREADGROUP`), resulting in microsecond job queuing and zero polling CPU load.
- **Memory Retention**: Omitting `removeOnComplete` and `removeOnFail` causes Redis memory to grow linearly with every executed job, eventually leading to OOM crashes. Always configure job removal limits.

---

## 7. Anti-Patterns & Common Mistakes

- **Importing Heavy NestJS Modules into Worker**: Creating heavy circular imports. Keep worker processors focused on executing specific business domain services.
- **Forgetting `extends WorkerHost`**: Omitting `extends WorkerHost` when declaring `@Processor()`. In `@nestjs/bullmq`, extending `WorkerHost` is mandatory for type-safe job routing.
- **Not Handling Graceful Shutdown**: Terminating the process while jobs are active. NestJS handles worker teardown automatically when shutdown hooks are enabled.

---

## 8. Production Checklist & Operational Readiness

- [ ] `removeOnComplete` and `removeOnFail` configured globally.
- [ ] Concurrency tuned to worker pod CPU and network constraints.
- [ ] Explicit retry policies with exponential backoff configured.
- [ ] Redis connection password and TLS secured in cloud environments.
- [ ] Worker events logged to centralized logging (Datadog, CloudWatch).

---

## 9. Migration & Version History

- **Bull 3 to BullMQ**: Replaced callback-based APIs with native Promises, strict TypeScript types, and Redis Stream foundations.
- **@nestjs/bullmq 10.x/11.x**: Standardized on `WorkerHost` abstract class and full NestJS 11 lifecycle compatibility.

---

## 10. Verification & Testing

Mocking BullMQ Queue in unit tests:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { getQueueToken } from '@nestjs/bullmq';
import { MailProducerService } from './mail-producer.service';

describe('MailProducerService', () => {
  let service: MailProducerService;
  let mockQueue: any;

  beforeEach(async () => {
    mockQueue = {
      add: vi.fn().mockResolvedValue({ id: 'job_42' }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        MailProducerService,
        { provide: getQueueToken('mail'), useValue: mockQueue },
      ],
    }).compile();

    service = module.get<MailProducerService>(MailProducerService);
  });

  it('enqueues welcome email with correct payload', async () => {
    const result = await service.queueWelcomeEmail({
      userId: 'u1',
      email: 'alex@example.com',
      name: 'Alex',
    });

    expect(result.jobId).toBe('job_42');
    expect(mockQueue.add).toHaveBeenCalledWith(
      'send-welcome',
      expect.objectContaining({ email: 'alex@example.com' }),
      expect.any(Object),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Bull Board Monitoring Setup

```typescript
// src/admin/bull-board.ts
import { createBullBoard } from '@bull-board/api';
import { BullMQAdapter } from '@bull-board/api/bullMQAdapter';
import { ExpressAdapter } from '@bull-board/express';
import { INestApplication } from '@nestjs/common';
import { Queue } from 'bullmq';

export function setupBullBoard(app: INestApplication, queues: Queue[]) {
  const serverAdapter = new ExpressAdapter();
  serverAdapter.setBasePath('/admin/queues');

  createBullBoard({
    queues: queues.map((q) => new BullMQAdapter(q)),
    serverAdapter,
  });

  app.use('/admin/queues', serverAdapter.getRouter());
}
```

---

## 12. Cross-References & Further Reading

- [01-queues-fundamentals.md](./01-queues-fundamentals.md) - Queue fundamentals
- [03-producers-and-consumers.md](./03-producers-and-consumers.md) - Advanced worker patterns
- [04-retries-and-dead-letter-queues.md](./04-retries-and-dead-letter-queues.md) - Error recovery
- [BullMQ Official Guide](https://docs.bullmq.io)
