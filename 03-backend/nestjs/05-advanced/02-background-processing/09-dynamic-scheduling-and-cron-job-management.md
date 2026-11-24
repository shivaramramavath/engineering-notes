# Dynamic Scheduling and Cron Job Management

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [08-scheduling-with-cron](./08-scheduling-with-cron.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

While static `@Cron()` decorators work well for system-level schedules hardcoded in source files, enterprise applications often require **Dynamic Scheduling**:
- User-configured alerts (*"Run report every Monday at 08:30"*).
- Tenant-specific data synchronizations configured in a database table.
- Pausing, resuming, or deleting scheduled jobs at runtime without redeploying code.

The **`SchedulerRegistry`** provided by `@nestjs/schedule` exposes full programmatic control over all registered cron jobs, intervals, and timeouts. It allows adding, inspecting, pausing, stopping, and deleting schedules dynamically at runtime.

---

## 2. Theoretical Foundations

`SchedulerRegistry` acts as an in-memory dictionary of running timer handles:
- **`CronJob` instances**: Managed via `schedulerRegistry.addCronJob(name, job)` and `schedulerRegistry.deleteCronJob(name)`.
- **Interval handles**: Managed via `schedulerRegistry.addInterval(name, id)` and `clearInterval()`.
- **Timeout handles**: Managed via `schedulerRegistry.addTimeout(name, id)` and `clearTimeout()`.

Dynamically registered jobs can be scheduled in response to HTTP API calls, loaded from database rows during bootstrap, or modified via administrative dashboards.

---

## 3. Architecture & Internal Mechanics

```
User API Request: POST /schedules (cron: "0 12 * * *", name: "user_alert_42")
                                │
                                ▼
                   DynamicSchedulerController
                                │
                                ▼
                    DynamicSchedulerService
                                │
          ┌─────────────────────┴─────────────────────┐
          ▼                                           ▼
Save to PostgreSQL / Database              SchedulerRegistry
INSERT INTO tenant_schedules ...          const job = new CronJob(...)
                                          schedulerRegistry.addCronJob(...)
                                          job.start()
                                                      │
                                                      ▼
                                          Active Dynamic Cron Runner
```

---

## 4. Concrete Implementation

### 4.1 Creating and Managing Dynamic Cron Jobs

```typescript
// src/schedules/dynamic-scheduler.service.ts
import { Injectable, Logger, NotFoundException, BadRequestException } from '@nestjs/common';
import { SchedulerRegistry } from '@nestjs/schedule';
import { CronJob } from 'cron';

@Injectable()
export class DynamicSchedulerService {
  private readonly logger = new Logger(DynamicSchedulerService.name);

  constructor(private readonly schedulerRegistry: SchedulerRegistry) {}

  // 1. Add a new dynamic cron job
  addCronJob(name: string, cronTime: string, task: () => void) {
    // Validate if job already exists
    if (this.schedulerRegistry.doesExist('cron', name)) {
      throw new BadRequestException(`Cron job [${name}] already exists.`);
    }

    const job = new CronJob(cronTime, () => {
      this.logger.log(`Executing dynamic job [${name}]...`);
      task();
    });

    this.schedulerRegistry.addCronJob(name, job);
    job.start();

    this.logger.log(`Dynamic cron job [${name}] scheduled with pattern: ${cronTime}`);
  }

  // 2. Stop/Pause a running job
  stopCronJob(name: string) {
    const job = this.getCronJobOrThrow(name);
    job.stop();
    this.logger.log(`Cron job [${name}] paused.`);
  }

  // 3. Resume a paused job
  resumeCronJob(name: string) {
    const job = this.getCronJobOrThrow(name);
    job.start();
    this.logger.log(`Cron job [${name}] resumed.`);
  }

  // 4. Delete and remove a job completely
  deleteCronJob(name: string) {
    this.stopCronJob(name);
    this.schedulerRegistry.deleteCronJob(name);
    this.logger.log(`Cron job [${name}] completely removed from registry.`);
  }

  // 5. List all active cron jobs and their next execution dates
  listCronJobs(): Array<{ name: string; running: boolean; nextRun: string | null }> {
    const jobs = this.schedulerRegistry.getCronJobs();
    const result: Array<{ name: string; running: boolean; nextRun: string | null }> = [];

    jobs.forEach((job, name) => {
      let nextRun = null;
      try {
        nextRun = job.nextDate().toISO();
      } catch {
        // Not running or invalid date
      }

      result.push({
        name,
        running: job.running,
        nextRun,
      });
    });

    return result;
  }

  private getCronJobOrThrow(name: string): CronJob {
    try {
      return this.schedulerRegistry.getCronJob(name);
    } catch {
      throw new NotFoundException(`Cron job [${name}] not found in registry.`);
    }
  }
}
```

---

### 4.2 Dynamic Intervals and Timeouts

```typescript
// src/schedules/dynamic-interval.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { SchedulerRegistry } from '@nestjs/schedule';

@Injectable()
export class DynamicIntervalService {
  private readonly logger = new Logger(DynamicIntervalService.name);

  constructor(private readonly schedulerRegistry: SchedulerRegistry) {}

  addDynamicInterval(name: string, milliseconds: number, callback: () => void) {
    const interval = setInterval(callback, milliseconds);
    this.schedulerRegistry.addInterval(name, interval);
    this.logger.log(`Interval [${name}] registered every ${milliseconds}ms`);
  }

  deleteDynamicInterval(name: string) {
    this.schedulerRegistry.deleteInterval(name);
    this.logger.log(`Interval [${name}] stopped and deleted.`);
  }
}
```

---

### 4.3 Database-Driven Dynamic Job Restoration on Startup

When the application boots, it loads dynamic user schedules from PostgreSQL into `SchedulerRegistry`:

```typescript
// src/schedules/schedule-bootstrapper.service.ts
import { Injectable, OnApplicationBootstrap, Logger } from '@nestjs/common';
import { DynamicSchedulerService } from './dynamic-scheduler.service';
import { DataSource } from 'typeorm';

@Injectable()
export class ScheduleBootstrapperService implements OnApplicationBootstrap {
  private readonly logger = new Logger(ScheduleBootstrapperService.name);

  constructor(
    private readonly dynamicScheduler: DynamicSchedulerService,
    private readonly dataSource: DataSource,
  ) {}

  async onApplicationBootstrap() {
    this.logger.log('Loading active dynamic schedules from database...');

    const schedules = await this.dataSource.query(
      `SELECT id, name, cron_expression FROM user_schedules WHERE is_active = true`,
    );

    for (const record of schedules) {
      try {
        this.dynamicScheduler.addCronJob(
          `tenant_${record.id}`,
          record.cron_expression,
          () => {
            console.log(`Running scheduled sync for tenant record: ${record.id}`);
          },
        );
      } catch (err: any) {
        this.logger.error(`Failed to register dynamic schedule [${record.id}]: ${err.message}`);
      }
    }

    this.logger.log(`Successfully bootstrapped ${schedules.length} dynamic schedules.`);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Invalid Cron Expression Injection
If users provide malformed cron expressions (e.g. `invalid * *`), the `new CronJob()` constructor throws a runtime parsing error.
- **Handling**: Validate cron strings using libraries like `cron-validator` before passing them to `CronJob`.

### 5.2 Node Restart Losing In-Memory State
Dynamically registered jobs exist only in Node process memory. If the server restarts, all dynamic jobs disappear unless reloaded from a database during `OnApplicationBootstrap`.
- **Rule**: Always persist dynamic schedule definitions to a relational database.

---

## 6. Performance & Memory Implications

- **Registry Scalability**: Managing 1,000 in-process cron jobs consumes ~5MB of memory. However, for 100,000+ dynamic schedules, offload to distributed schedulers like BullMQ or temporal engines.
- **Garbage Collection**: Always call `deleteCronJob(name)` to remove abandoned job references from the registry map, avoiding memory leaks.

---

## 7. Anti-Patterns & Common Mistakes

- **Not Calling `job.start()`**: Registering `schedulerRegistry.addCronJob(name, job)` and forgetting `job.start()`. By default, newly constructed `CronJob` instances are paused.
- **Forgetting to Persist to Database**: Registering dynamic jobs via an HTTP POST endpoint without saving them to a persistent table, losing user alerts on the next deployment.

---

## 8. Production Checklist & Operational Readiness

- [ ] Cron expressions validated against standard regex / parser before instantiation.
- [ ] Active schedules loaded from database during `onApplicationBootstrap`.
- [ ] Deleting a schedule clears the timer handle from `SchedulerRegistry`.
- [ ] Management endpoints secured with Admin authorization guards.

---

## 9. Migration & Version History

- **NestJS 8/9**: Added `doesExist()` helper to `SchedulerRegistry`.
- **NestJS 11.x**: Support for modern `cron` 3.x packages and typed `CronJob` interfaces.

---

## 10. Verification & Testing

Unit testing dynamic schedule registration:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { SchedulerRegistry } from '@nestjs/schedule';
import { DynamicSchedulerService } from './dynamic-scheduler.service';

describe('DynamicSchedulerService', () => {
  let service: DynamicSchedulerService;
  let registry: SchedulerRegistry;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [DynamicSchedulerService, SchedulerRegistry],
    }).compile();

    service = module.get<DynamicSchedulerService>(DynamicSchedulerService);
    registry = module.get<SchedulerRegistry>(SchedulerRegistry);
  });

  it('adds and lists a dynamic cron job', () => {
    const callback = vi.fn();
    service.addCronJob('test_job', '0 0 * * *', callback);

    const jobs = service.listCronJobs();
    expect(jobs.length).toBe(1);
    expect(jobs[0].name).toBe('test_job');
    expect(jobs[0].running).toBe(true);

    service.deleteCronJob('test_job');
    expect(service.listCronJobs().length).toBe(0);
  });
});
```

---

## 11. Comprehensive Examples

### Administrative Dynamic Scheduling Controller

```typescript
// src/schedules/dynamic-scheduler.controller.ts
import { Controller, Post, Get, Delete, Body, Param, UseGuards } from '@nestjs/common';
import { DynamicSchedulerService } from './dynamic-scheduler.service';

@Controller('admin/schedules')
export class DynamicSchedulerController {
  constructor(private readonly scheduler: DynamicSchedulerService) {}

  @Get()
  listSchedules() {
    return this.scheduler.listCronJobs();
  }

  @Post()
  createSchedule(@Body() body: { name: string; cron: string }) {
    this.scheduler.addCronJob(body.name, body.cron, () => {
      console.log(`Dynamic job [${body.name}] executed!`);
    });
    return { status: 'created', name: body.name };
  }

  @Post(':name/pause')
  pauseSchedule(@Param('name') name: string) {
    this.scheduler.stopCronJob(name);
    return { status: 'paused', name };
  }

  @Delete(':name')
  deleteSchedule(@Param('name') name: string) {
    this.scheduler.deleteCronJob(name);
    return { status: 'deleted', name };
  }
}
```

---

## 12. Cross-References & Further Reading

- [08-scheduling-with-cron.md](./08-scheduling-with-cron.md) - Declarative cron fundamentals
- [05-delayed-and-repeatable-jobs.md](./05-delayed-and-repeatable-jobs.md) - Repeatable jobs with BullMQ
- [SchedulerRegistry API Documentation](https://docs.nestjs.com/techniques/task-scheduling#dynamic-cron-jobs)
