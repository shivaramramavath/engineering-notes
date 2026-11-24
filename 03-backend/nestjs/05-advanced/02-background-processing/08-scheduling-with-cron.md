# In-Process Scheduling with Cron

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md), [01-queues-fundamentals](./01-queues-fundamentals.md)

---

## 1. Context & Purpose

Many backend services require recurring, scheduled task execution:
- *"Sync currency exchange rates every 6 hours."*
- *"Purge expired sessions and temporary files every night at 03:00."*
- *"Send hourly telemetry heartbeats to an internal monitoring endpoint."*

For workloads that live within a single service without requiring heavy distributed message queues, NestJS provides the official **`@nestjs/schedule`** package. Built on top of the battle-tested `cron` engine, it provides declarative decorators (`@Cron()`, `@Interval()`, `@Timeout()`), dynamic schedule registration via `SchedulerRegistry`, and clean integration with the NestJS lifecycle.

---

## 2. Theoretical Foundations

### 2.1 Cron Expressions Syntax
Standard 5-part (Unix) and 6-part (extended seconds) cron syntax:
```
┌────────────── second (optional, 0 - 59)
│ ┌──────────── minute (0 - 59)
│ │ ┌────────── hour (0 - 23)
│ │ │ ┌──────── day of month (1 - 31)
│ │ │ │ ┌────── month (1 - 12 or JAN-DEC)
│ │ │ │ │ ┌──── day of week (0 - 7 or SUN-SAT)
│ │ │ │ │ │
* * * * * *
```
- `0 0 * * *`: Daily at midnight.
- `*/15 * * * *`: Every 15 minutes.
- `0 9 * * 1-5`: Mon–Fri at 09:00 AM.

### 2.2 In-Process vs Distributed Scheduling
- **In-Process (`@nestjs/schedule`)**: Runs inside the single Node.js process using `setTimeout`/timers. If multiple pods run the same code, **every pod runs the cron job**.
- **Distributed (BullMQ / Database Lock)**: A distributed lock or queue coordinates across pods so only one instance executes the task.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   ScheduleModule.forRoot()             │
│               Scans metadata on bootstrap              │
└───────────────────────────┬────────────────────────────┘
                            │
              Discovery & Metadata Scanning
                            │
              Registers Jobs in SchedulerRegistry
                            │
 ┌──────────────────────────┼───────────────────────────┐
 │                          │                           │
 ▼                          ▼                           ▼
@Cron('0 0 * * *')       @Interval(10000)            @Timeout(5000)
CronJob (Timed schedule) SetInterval timer          SetTimeout one-shot
                            │
                            ▼
              Node.js libuv Event Loop Timers
```

---

## 4. Concrete Implementation

### 4.1 Installing Dependencies

```bash
npm install --save @nestjs/schedule
npm install --save-dev @types/cron
```

### 4.2 Initializing `ScheduleModule`

```typescript
// src/app.module.ts
import { Module } from '@nestjs/common';
import { ScheduleModule } from '@nestjs/schedule';
import { TasksModule } from './tasks/tasks.module';

@Module({
  imports: [
    ScheduleModule.forRoot(), // Initializes internal timer registry
    TasksModule,
  ],
})
export class AppModule {}
```

---

### 4.3 Declarative Scheduling with `@Cron()`, `@Interval()`, and `@Timeout()`

```typescript
// src/tasks/tasks.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { Cron, CronExpression, Interval, Timeout } from '@nestjs/schedule';

@Injectable()
export class TasksService {
  private readonly logger = new Logger(TasksService.name);

  // 1. Cron Expression using Built-In Enum
  @Cron(CronExpression.EVERY_DAY_AT_MIDNIGHT, {
    name: 'nightly-cleanup',
    timeZone: 'UTC', // Explicit timezone
  })
  handleNightlyCleanup() {
    this.logger.log('Executing nightly cleanup of temporary files...');
  }

  // 2. Fixed Interval: Runs every 30 seconds after previous start
  @Interval('heartbeat', 30000)
  handleHeartbeat() {
    this.logger.log('Heartbeat: Service is healthy.');
  }

  // 3. Timeout: Runs once, 5 seconds after application bootstrap
  @Timeout('bootstrap-warmup', 5000)
  handleWarmup() {
    this.logger.log('Application cache warmed up 5s after startup.');
  }
}
```

---

### 4.4 Distributed Lock Protection for Multi-Pod Deployments

To safely run `@Cron()` in Kubernetes without duplicate execution across pods, guard the execution with a Redis lock:

```typescript
// src/tasks/distributed-task.service.ts
import { Injectable, Logger, Inject } from '@nestjs/common';
import { Cron } from '@nestjs/schedule';
import { Redis } from 'ioredis';
import { REDIS_CLIENT } from '../redis/redis.module';

@Injectable()
export class DistributedTaskService {
  private readonly logger = new Logger(DistributedTaskService.name);

  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  @Cron('0 */2 * * *') // Every 2 hours
  async handleExchangeRateSync() {
    const lockKey = 'lock:cron:exchange-rate-sync';
    const lockTtl = 60 * 1000; // 1 minute lock

    // Acquire lock atomically: only 1 pod in the cluster succeeds!
    const acquired = await this.redis.set(lockKey, 'locked', 'PX', lockTtl, 'NX');

    if (!acquired) {
      this.logger.log('Another pod is already executing exchange rate sync. Skipping.');
      return;
    }

    try {
      this.logger.log('Lock acquired. Syncing exchange rates...');
      await this.syncRates();
    } finally {
      // Optional: keep lock until TTL or release explicitly
    }
  }

  private async syncRates() {
    await new Promise((r) => setTimeout(r, 1000));
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Long-Running Tasks Overlapping
If a task scheduled `@Interval(10000)` takes 15 seconds to finish, the next execution fires before the previous one completes, causing overlapping executions and database lock contention.
- **Handling**: Track task execution state with an `isRunning` flag or use self-scheduling timeouts:
```typescript
private isRunning = false;

@Interval(10000)
async handleTask() {
  if (this.isRunning) return;
  this.isRunning = true;
  try {
    await this.doWork();
  } finally {
    this.isRunning = false;
  }
}
```

### 5.2 Timezone Mismatches on Daylight Saving Time (DST)
Tasks scheduled at 02:00 AM without an explicit timezone can run twice or be skipped during DST clock shifts.
- **Rule**: Always specify `timeZone: 'UTC'`.

---

## 6. Performance & Memory Implications

- **Event Loop Impact**: In-process cron callbacks run on the main Node.js event loop thread. Synchronous CPU-heavy logic blocks all incoming HTTP requests.
- **Timer Memory**: Node timers are registered in the internal timer wheel, consuming negligible RAM.

---

## 7. Anti-Patterns & Common Mistakes

- **Running Unprotected Crons in Autoscaled Kubernetes**: Deploying an uncoordinated `@Cron()` across 10 replicas, causing duplicate emails or duplicate billing charges.
- **Using Hardcoded String Cron Expressions Without Labels**: Using `* 3 * * *` with no `name` or comments, making it difficult to debug or manage dynamically.
- **Blocking the Event Loop Inside Cron Handlers**: Parsing 100MB CSV files synchronously inside `@Cron()`.

---

## 8. Production Checklist & Operational Readiness

- [ ] `timeZone: 'UTC'` specified on all `@Cron()` decorators.
- [ ] Distributed locks (Redis / DB) applied if running in multi-pod deployments.
- [ ] `isRunning` guards prevent overlapping task executions.
- [ ] Cron errors caught with `try/catch` and logged to alerting systems.
- [ ] Task names registered for visibility in `SchedulerRegistry`.

---

## 9. Migration & Version History

- **NestJS 7/8**: Introduced `SchedulerRegistry` for dynamic job manipulation.
- **NestJS 11.x**: Validated with modern `cron` 3.x engines and reversed lifecycle shutdown draining.

---

## 10. Verification & Testing

Unit testing a cron task service:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { DistributedTaskService } from './distributed-task.service';
import { REDIS_CLIENT } from '../redis/redis.module';

describe('DistributedTaskService', () => {
  let service: DistributedTaskService;
  let mockRedis: any;

  beforeEach(async () => {
    mockRedis = { set: vi.fn() };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        DistributedTaskService,
        { provide: REDIS_CLIENT, useValue: mockRedis },
      ],
    }).compile();

    service = module.get<DistributedTaskService>(DistributedTaskService);
  });

  it('skips task if lock could not be acquired', async () => {
    mockRedis.set.mockResolvedValue(null); // Lock failed

    const logSpy = vi.spyOn((service as any).logger, 'log');
    await service.handleExchangeRateSync();

    expect(logSpy).toHaveBeenCalledWith(
      expect.stringContaining('Another pod is already executing'),
    );
  });
});
```

---

## 11. Comprehensive Examples

### Cron Expression Reference Table

```typescript
// src/common/constants/schedules.ts
export const ScheduleConstants = {
  EVERY_MINUTE: '* * * * *',
  EVERY_10_MINUTES: '*/10 * * * *',
  EVERY_HOUR: '0 * * * *',
  MIDNIGHT_UTC: '0 0 * * *',
  FIRST_OF_MONTH: '0 0 1 * *',
  WEEKLY_SUNDAY: '0 0 * * 0',
} as const;
```

---

## 12. Cross-References & Further Reading

- [05-delayed-and-repeatable-jobs.md](./05-delayed-and-repeatable-jobs.md) - Distributed repeatable jobs with BullMQ
- [09-dynamic-scheduling-and-cron-job-management.md](./09-dynamic-scheduling-and-cron-job-management.md) - Runtime cron manipulation
- [NestJS Task Scheduling Guide](https://docs.nestjs.com/techniques/task-scheduling)
