# Background Processing and Queue Fundamentals

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [00-prerequisites/03-nodejs-async-and-event-loop](../../00-prerequisites/03-nodejs-async-and-event-loop.md), [02-fundamentals/01-architecture-overview](../../02-fundamentals/01-architecture-overview.md)

---

## 1. Context & Purpose

The Node.js event loop runs on a single main thread. Executing heavy computations (video transcoding, large PDF generation, bulk data export, AI model prompting) or slow I/O (sending thousands of emails, charging payments with third-party webhooks) directly inside an HTTP request handler blocks the event loop, causing severe latency spikes or HTTP timeouts for all other users.

**Background Processing and Message Queues** decouple time-consuming tasks from the client-facing HTTP lifecycle. An HTTP endpoint enqueues a lightweight job payload and immediately returns `202 Accepted` to the client. Dedicated background worker processes consume and execute these jobs asynchronously, ensuring high throughput, resilience against process crashes, and automated retries.

---

## 2. Theoretical Foundations

### 2.1 The Producer-Consumer Architecture
- **Producer**: Ingests client requests, serializes job parameters into JSON, and pushes them onto the queue.
- **Queue (Broker)**: Persistent storage (typically Redis, RabbitMQ, or Amazon SQS) holding queued jobs in order (FIFO, LIFO, or priority).
- **Consumer (Worker)**: Autonomous background processes that pop jobs from the queue, execute domain logic, and report status (`completed`, `failed`, `delayed`).

### 2.2 Guarantees: At-Least-Once Delivery
In distributed queue systems, network partitions or worker crashes can occur midway through job execution. Queues use **Job Acknowledgments (ACK)**:
If a worker crashes before ACKing, the queue server re-delivers the job to another worker. This guarantees **at-least-once delivery**, making **job idempotency** a strict requirement.

---

## 3. Architecture & Internal Mechanics

```
Client (User)
      │
      ├─> POST /reports/generate (Heavy 30-second query)
      │
      ▼
NestJS HTTP Controller (Producer)
      │
      ├─> queue.add('generate-report', { userId: 1, type: 'CSV' })
      │
      ▼
HTTP Response: 202 Accepted (Job ID: job_12345) [< 10ms!]
      │
      ▼
Redis Queue Storage
[ job_1 ] ──> [ job_2 ] ──> [ job_12345 ]
                                  │
                                  ▼
                    NestJS Queue Consumer (Worker)
                    - Pulls job_12345
                    - Generates CSV
                    - Uploads to S3
                    - Sends Email notification
                    - Acknowledges Job Complete (ACK)
```

---

## 4. Concrete Implementation

### 4.1 Queue Abstraction Comparison

| Feature | In-Memory Async (`setTimeout`/`setImmediate`) | Database Queue Table | Distributed Queue (`BullMQ` / Redis) |
|---|---|---|---|
| Crash Persistence | ❌ Lost on process restart | ✅ Persisted in SQL | ✅ Persisted in Redis RDB/AOF |
| Multi-Worker Scaling | ❌ Bound to single process | ⚠️ High DB row lock contention | ✅ Scales to hundreds of workers |
| Priority & Delays | ❌ Primitive | ⚠️ Complex indexing required | ✅ Native Redis ZSET support |
| Backpressure Control | ❌ Memory bloat | ⚠️ DB I/O saturation | ✅ Concurrency & rate limiting |

---

### 4.2 Minimal In-Memory Job Pattern (Small Workloads)

For non-critical background fire-and-forget tasks (e.g. updating an in-memory metric counter):

```typescript
// src/common/workers/in-memory-task.service.ts
import { Injectable, Logger } from '@nestjs/common';

@Injectable()
export class InMemoryTaskService {
  private readonly logger = new Logger(InMemoryTaskService.name);

  // Executes asynchronously off the current call stack
  runInBackground(taskName: string, task: () => Promise<void>) {
    setImmediate(async () => {
      try {
        await task();
      } catch (err: any) {
        this.logger.error(`Background task [${taskName}] failed: ${err.message}`, err.stack);
      }
    });
  }
}
```

*Note: For production business transactions (payments, emails, exports), never rely on in-memory `setImmediate`; always use a persistent queue like BullMQ.*

---

## 5. Edge Cases & Failure Modes

### 5.1 OOM via Unbounded Queue Depth
If producers enqueue 500,000 jobs per minute while workers can only process 10,000 per minute, Redis memory will grow until the host runs Out-Of-Memory.
- **Handling**: Enforce rate limiting, queue size alerting, and dead-letter expiration.

### 5.2 Zombie Jobs on Hard SIGKILL
If a Kubernetes pod receives `SIGKILL` without a graceful `SIGTERM` draining window, active jobs remain locked in a pending state until their visibility timeout expires.
- **Handling**: Configure graceful shutdown hooks (`app.enableShutdownHooks()`) and proper lock duration renewers.

---

## 6. Performance & Memory Implications

- **Event Loop Health**: Moving a 500ms synchronous JSON parsing or PDF compilation task to a background worker keeps the HTTP event loop latency under 2ms.
- **Worker Concurrency**: Worker concurrency should match CPU core availability for CPU-bound tasks, or higher for I/O-bound tasks (e.g., 20–50 concurrent web requests).

---

## 7. Anti-Patterns & Common Mistakes

- **Awaiting Long Tasks Inside Route Handlers**: Calling `await this.mailService.sendTo1000Users()` inside a controller before returning the HTTP response.
- **Assuming Exact-Once Delivery**: Writing non-idempotent workers that re-charge credit cards when a job is re-delivered after a network hiccup.
- **Storing Large Blobs Directly in Job Payloads**: Passing 50MB image files directly in the job JSON payload instead of uploading to S3 and passing the S3 URL.

---

## 8. Production Checklist & Operational Readiness

- [ ] HTTP mutation endpoints return `202 Accepted` for background workloads.
- [ ] Job payloads contain only entity IDs and lightweight metadata (no large buffers).
- [ ] Graceful shutdown drains active jobs before process termination.
- [ ] Dead-letter queues (DLQ) configured for unrecoverable failures.

---

## 9. Migration & Version History

- **Historical**: NestJS originally supported `@nestjs/bull` (Bull 3).
- **Current Standard**: **BullMQ** (`@nestjs/bullmq`) is the modern standard, offering TypeScript-first APIs, parent-child job hierarchies, and improved Redis performance.

---

## 10. Verification & Testing

Testing asynchronous background execution:

```typescript
import { InMemoryTaskService } from './in-memory-task.service';

describe('InMemoryTaskService', () => {
  it('executes task asynchronously without blocking caller', async () => {
    const service = new InMemoryTaskService();
    let executed = false;

    service.runInBackground('test-job', async () => {
      executed = true;
    });

    // Caller completes before task executes
    expect(executed).toBe(false);

    // Wait for event loop turn
    await new Promise((r) => setTimeout(r, 10));
    expect(executed).toBe(true);
  });
});
```

---

## 11. Comprehensive Examples

### HTTP 202 Polling Pattern Architecture

```typescript
// src/reports/reports.controller.ts
import { Controller, Post, Get, Param, HttpStatus, HttpCode } from '@nestjs/common';
import { randomUUID } from 'crypto';

@Controller('reports')
export class ReportsController {
  // In-memory status tracker (use Redis in production)
  private readonly jobStatus = new Map<string, { status: string; url?: string }>();

  @Post('export')
  @HttpCode(HttpStatus.ACCEPTED)
  async triggerExport() {
    const jobId = randomUUID();
    this.jobStatus.set(jobId, { status: 'PENDING' });

    // Enqueue job to background worker...

    return {
      message: 'Report generation started',
      jobId,
      statusUrl: `/reports/status/${jobId}`,
    };
  }

  @Get('status/:id')
  checkStatus(@Param('id') id: string) {
    const record = this.jobStatus.get(id);
    return record || { status: 'NOT_FOUND' };
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-bullmq.md](./02-bullmq.md) - Production BullMQ implementation
- [03-producers-and-consumers.md](./03-producers-and-consumers.md) - Consumer design
- [06-job-idempotency.md](./06-job-idempotency.md) - Ensuring idempotent job execution
- [BullMQ Official Documentation](https://docs.bullmq.io)
