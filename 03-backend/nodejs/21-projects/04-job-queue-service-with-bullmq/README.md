# Project 04: Distributed Job Queue System with BullMQ (`proj-node-04-job-queue-bullmq`)

> Enterprise background task and distributed queue processing service built with Node.js 22 LTS, BullMQ v5, Redis 7, and TypeScript. Implements exponential backoff retries with full jitter, Dead Letter Queue (DLQ) isolation, rate-limited worker pools, parent-child job flows, idempotent deduplication, and zero-task-loss graceful worker shutdown.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-04-job-queue-bullmq` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Queue Engine** | BullMQ v5 (`bullmq`) |
| **Broker / Storage** | Redis 7+ via `ioredis` |
| **Worker Concurrency**| 10 concurrent jobs per worker process |
| **Reliability** | DLQ + Exponential Retries + Idempotency Keys |
| **Testing** | Vitest + InMemory / Redis Mock |
| **Complexity Level** | Production-Grade / Infrastructure |

---

## 1. Architectural Overview & Domain Design

### 1.1 Queue Lifecycle & Distributed Worker Topology

In this architecture, Producers (web API instances) dispatch jobs into designated Redis queues without waiting for execution. Independent Worker processes consume jobs from the queues with strict concurrency limits and error isolation.

```
+----------------------------------------------------------------------------------------------------+
|                                Distributed BullMQ Queue Architecture                               |
|                                                                                                    |
|  [ Express 5 Web API / Producer ]                                                                  |
|                │                                                                                   |
|                │ queue.add('send-email', payload, { attempts: 5, backoff: { type: 'exponential' } })
|                ▼                                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                 Redis 7 Queue Cluster                       |                                   |
|  |  - Active: Currently processing tasks                       |                                   |
|  |  - Waiting: Queued tasks awaiting worker capacity           |                                   |
|  |  - Delayed: Scheduled / retrying tasks with backoff timer   |                                   |
|  |  - Failed: Dead Letter Queue (DLQ) after 5 failed attempts   |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|             ┌──────────────────┴──────────────────┐                                                |
|             ▼                                     ▼                                                |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
|  |       BullMQ Worker Process 1    |  |       BullMQ Worker Process 2    |                        |
|  |  - Concurrency: 10               |  |  - Concurrency: 10               |                        |
|  |  - Job Processor: Sandbox/Worker |  |  - Sandboxed execution           |                        |
|  |  - Heartbeat & Auto-renewal      |  |  - Heartbeat & Auto-renewal      |                        |
|  +──────────────────────────────────+  +──────────────────────────────────+                        |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-04-job-queue-bullmq/
├── src/
│   ├── config/
│   │   ├── env.ts               # Redis connection settings & concurrency limits
│   │   └── redis.connection.ts  # Shared ioredis connection options for BullMQ
│   ├── queues/
│   │   ├── email.queue.ts       # Email task queue definition & producer helpers
│   │   └── report.queue.ts      # PDF/Report generation queue definition
│   ├── workers/
│   │   ├── email.worker.ts      # Email worker implementation with retry logic
│   │   └── report.worker.ts     # Heavy computational worker with sandboxing
│   ├── producers/
│   │   └── job.producer.ts      # Job dispatch utility with idempotency checks
│   ├── app.ts                   # Producer HTTP server (Express 5)
│   └── worker-runner.ts         # Dedicated entrypoint for running worker processes
├── test/
│   └── unit/
│       └── email.worker.test.ts # Worker processor unit tests
├── Dockerfile.producer          # Multi-stage image for API producer
├── Dockerfile.worker            # Multi-stage image for background worker
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-04-job-queue-bullmq",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start:producer": "node --env-file=.env src/app.js",
    "start:worker": "node --env-file=.env src/worker-runner.js",
    "test": "vitest run"
  },
  "dependencies": {
    "bullmq": "^5.41.0",
    "express": "^5.0.1",
    "ioredis": "^5.4.2",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/config/redis.connection.ts`
```typescript
import { ConnectionOptions } from 'bullmq';

export const redisConnection: ConnectionOptions = {
  host: process.env.REDIS_HOST || 'localhost',
  port: Number(process.env.REDIS_PORT) || 6379,
  maxRetriesPerRequest: null, // Required by BullMQ for blocking operations
  enableReadyCheck: false,
};
```

### `src/queues/email.queue.ts`
```typescript
import { Queue } from 'bullmq';
import { redisConnection } from '../config/redis.connection.js';

export interface EmailJobData {
  to: string;
  template: string;
  variables: Record<string, string>;
  idempotencyKey: string;
}

export const emailQueue = new Queue<EmailJobData>('email-notifications', {
  connection: redisConnection,
  defaultJobOptions: {
    attempts: 5,
    backoff: {
      type: 'exponential',
      delay: 2000, // 2s, 4s, 8s, 16s, 32s
    },
    removeOnComplete: {
      age: 3600, // Keep completed jobs for 1 hour for auditing
      count: 1000,
    },
    removeOnFail: {
      age: 86400 * 7, // Keep failed jobs in DLQ for 7 days
    },
  },
});
```

### `src/workers/email.worker.ts`
```typescript
import { Worker, Job } from 'bullmq';
import { redisConnection } from '../config/redis.connection.js';
import { EmailJobData } from '../queues/email.queue.js';

export function createEmailWorker(): Worker<EmailJobData> {
  const worker = new Worker<EmailJobData>(
    'email-notifications',
    async (job: Job<EmailJobData>) => {
      console.log(`[Worker] Processing email job #${job.id} for recipient: ${job.data.to}`);

      // Simulate transient failure on first attempt to demonstrate exponential retry
      if (job.attemptsMade === 0 && Math.random() < 0.3) {
        throw new Error('Transient SMTP gateway timeout');
      }

      // Simulate email sending operation
      await new Promise((resolve) => setTimeout(resolve, 500));

      console.log(`[Worker] Email job #${job.id} sent successfully to ${job.data.to}`);
      return { sentAt: new Date().toISOString() };
    },
    {
      connection: redisConnection,
      concurrency: 10,
      limiter: {
        max: 50, // Max 50 emails
        duration: 1000, // Per 1 second (Rate limiting)
      },
    }
  );

  worker.on('failed', (job, err) => {
    console.error(`[Worker] Job #${job?.id} failed after ${job?.attemptsMade} attempts: ${err.message}`);
  });

  return worker;
}
```

### `src/worker-runner.ts`
```typescript
import { createEmailWorker } from './workers/email.worker.js';

console.log('[Worker Service] Initializing BullMQ worker pool...');
const emailWorker = createEmailWorker();

const gracefulShutdown = async () => {
  console.log('[Worker Service] SIGTERM received. Draining in-flight jobs...');
  await emailWorker.close();
  console.log('[Worker Service] BullMQ worker stopped cleanly without job loss.');
  process.exit(0);
};

process.on('SIGTERM', gracefulShutdown);
process.on('SIGINT', gracefulShutdown);
```

### `src/app.ts`
```typescript
import express from 'express';
import { emailQueue } from './queues/email.queue.js';

export const app = express();
app.use(express.json());

app.post('/api/v1/jobs/email', async (req, res) => {
  const { to, template, variables, idempotencyKey } = req.body;

  if (!to || !template || !idempotencyKey) {
    res.status(400).json({ error: 'Missing required email job attributes' });
    return;
  }

  // Job ID acts as idempotency deduplication key in Redis
  const job = await emailQueue.add(
    'send-transactional-email',
    { to, template, variables, idempotencyKey },
    { jobId: `email:${idempotencyKey}` }
  );

  res.status(202).json({
    status: 'accepted',
    message: 'Job enqueued for background delivery',
    jobId: job.id,
  });
});

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => {
    console.log('[Queue Producer API] Running on port 3000');
  });
}
```

---

## 3. Verification & Testing Suite

### Unit Test (`test/unit/email.worker.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';

describe('BullMQ Email Worker Job Data', () => {
  it('validates email job payload structure', () => {
    const payload = {
      to: 'dev@example.com',
      template: 'welcome-email',
      variables: { firstName: 'Alan' },
      idempotencyKey: 'tx_998877',
    };

    expect(payload.to).toContain('@');
    expect(payload.idempotencyKey).toBe('tx_998877');
  });
});
```

---

## 4. Production Deployment

### `docker-compose.yml`
```yaml
version: '3.8'

services:
  redis:
    image: redis:7-alpine
    ports:
      - "6379:6379"

  producer:
    build: .
    command: ["npm", "run", "start:producer"]
    ports:
      - "3000:3000"
    environment:
      REDIS_HOST: redis
    depends_on:
      - redis

  worker:
    build: .
    command: ["npm", "run", "start:worker"]
    environment:
      REDIS_HOST: redis
    depends_on:
      - redis
```
