# Standalone Applications and CLI Apps

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 01-getting-started
- **Prerequisites**: [01-what-is-nestjs](./01-what-is-nestjs.md), [02-first-steps](./02-first-steps.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Not all backend workloads are HTTP servers or continuous WebSocket listeners. Many operational workloads are one-off tasks: database migration scripts, cron job runners, data backfills, message queue polling workers, or administrative command-line tools (CLIs).

NestJS supports **Standalone Applications** (`INestApplicationContext`). A standalone application initializes the entire NestJS DI container, configuration modules, database connections, and logging without starting an HTTP server or binding to a network port. Combining this with tools like `nest-commander` enables building robust, type-safe CLI applications powered by NestJS dependency injection.

---

## 2. Theoretical Foundations

A standard NestJS HTTP application initializes two layers:
1. **The Application Context** (`INestApplicationContext`): Loads modules, scans metadata, builds the dependency graph, instantiates singletons, and executes `onModuleInit` / `onApplicationBootstrap` hooks.
2. **The HTTP Adapter Layer** (`INestApplication`): Wraps Express/Fastify, binds route handlers to HTTP verbs, registers middleware, and listens on a TCP port.

A **Standalone Application** instantiates only the first layer:
```
NestFactory.createApplicationContext(AppModule)
```
Once the task finishes, calling `await app.close()` triggers reverse-topological shutdown hooks (`onModuleDestroy`, `beforeApplicationShutdown`, `onApplicationShutdown`), cleanly draining database pools and socket connections before terminating the Node.js process.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│        NestFactory.createApplicationContext(Module)    │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
              ┌───────────────────────────┐
              │ Dependency Injection Tree │
              │ - ConfigService           │
              │ - DatabaseService         │
              │ - MigrationService        │
              └─────────────┬─────────────┘
                            │
               (No HTTP Engine / No Port)
                            │
                            ▼
              ┌───────────────────────────┐
              │   Execute Task / Script   │
              │ migrationService.run()    │
              └─────────────┬─────────────┘
                            │
                            ▼
              ┌───────────────────────────┐
              │     await app.close()     │
              │ Clean DB pool / teardown  │
              └───────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Basic Standalone Task Script

A script to run one-time data processing or maintenance tasks:

```typescript
// src/scripts/purge-inactive-users.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from '../app.module';
import { UsersService } from '../users/users.service';
import { Logger } from '@nestjs/common';

async function bootstrap() {
  const logger = new Logger('PurgeScript');
  logger.log('Initializing Standalone Application Context...');

  // Initialize context without HTTP listener
  const app = await NestFactory.createApplicationContext(AppModule, {
    logger: ['error', 'warn', 'log'],
  });

  try {
    const usersService = app.get(UsersService);
    const purgedCount = await usersService.purgeInactive(365);
    logger.log(`Successfully purged ${purgedCount} inactive users.`);
  } catch (error) {
    logger.error('Purge execution failed', error);
    process.exitCode = 1;
  } finally {
    logger.log('Closing application context...');
    await app.close(); // Cleanly closes DB connections
  }
}

bootstrap();
```

---

### 4.2 Building CLI Tools with `nest-commander`

`nest-commander` allows you to write enterprise CLI tools using standard NestJS classes and decorators.

Install dependencies:
```bash
npm install --save nest-commander
```

#### Step 1: Define a Command

```typescript
// src/cli/seed.command.ts
import { Command, CommandRunner, Option } from 'nest-commander';
import { UsersService } from '../users/users.service';
import { Logger } from '@nestjs/common';

interface SeedOptions {
  count?: number;
  admin?: boolean;
}

@Command({
  name: 'seed:users',
  description: 'Seed fake user records into database',
})
export class SeedUsersCommand extends CommandRunner {
  private readonly logger = new Logger(SeedUsersCommand.name);

  constructor(private readonly usersService: UsersService) {
    super();
  }

  async run(inputs: string[], options?: SeedOptions): Promise<void> {
    const count = options?.count ?? 10;
    const makeAdmin = options?.admin ?? false;

    this.logger.log(`Seeding ${count} users (admin=${makeAdmin})...`);
    await this.usersService.seed(count, makeAdmin);
    this.logger.log('Seeding completed successfully.');
  }

  @Option({
    flags: '-c, --count [number]',
    description: 'Number of users to seed',
  })
  parseCount(val: string): number {
    return Number(val);
  }

  @Option({
    flags: '-a, --admin',
    description: 'Grant seeded users admin privileges',
  })
  parseAdmin(): boolean {
    return true;
  }
}
```

#### Step 2: Register in CLI Module and Entry Point

```typescript
// src/cli/cli.module.ts
import { Module } from '@nestjs/common';
import { SeedUsersCommand } from './seed.command';
import { UsersModule } from '../users/users.module';

@Module({
  imports: [UsersModule],
  providers: [SeedUsersCommand],
})
export class CliModule {}
```

```typescript
// src/cli-main.ts
import { CommandFactory } from 'nest-commander';
import { CliModule } from './cli/cli.module';

async function bootstrap() {
  await CommandFactory.run(CliModule, ['warn', 'error']);
}
bootstrap();
```

Add an execution script to `package.json`:
```json
{
  "scripts": {
    "cli": "nest start --entryFile cli-main --"
  }
}
```

Run from terminal:
```bash
npm run cli -- seed:users --count 50 --admin
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Dangling Process via Unclosed Connections
If `await app.close()` is not called inside a `finally` block, open database connection pools (e.g., TypeORM, Prisma, Mongoose) or Redis clients keep the Node.js event loop active indefinitely, preventing the script from terminating.
- **Handling**: Always invoke `await app.close()` in a `finally` block or register process signal listeners (`SIGINT`, `SIGTERM`).

### 5.2 Request-Scoped Dependencies
Injecting request-scoped services into a standalone task fails because there is no incoming HTTP request to establish the request context.
- **Handling**: Use `await app.resolve(RequestScopedService)` to explicitly instantiate an isolated scope per task run.

---

## 6. Performance & Memory Implications

- **Startup Speed**: Standalone application contexts boot significantly faster (~40–60% less time) than HTTP applications because no network sockets, Express middleware chains, or routing tables are registered.
- **Memory Footprint**: Heap memory remains low because no request-pipeline caches or body-parser buffers are allocated.

---

## 7. Anti-Patterns & Common Mistakes

- **Using `NestFactory.create()` for CLI Scripts**: Starting a full HTTP server listening on a random port just to run a database migration script.
- **Calling `process.exit(0)` Abruptly**: Terminating the process before `app.close()` runs. This truncates pending writes and leaves database connection pools open on the database server.
- **Hardcoding Configuration in Scripts**: Bypassing NestJS `ConfigModule` and reading raw `process.env` directly in standalone tasks.

---

## 8. Production Checklist & Operational Readiness

- [ ] All standalone scripts call `await app.close()` inside a `finally` block.
- [ ] Non-zero exit code (`process.exitCode = 1`) is set upon unhandled task exceptions.
- [ ] Database pools used by standalone jobs have appropriate short idle timeouts.
- [ ] CLI commands are executed within container tasks (e.g., Kubernetes Jobs or AWS ECS tasks) with proper memory and timeout boundaries.

---

## 9. Migration & Version History

- **NestJS 4.x**: Introduced `createApplicationContext()` for non-HTTP workloads.
- **NestJS 9.x**: Full ecosystem support for `nest-commander` 3.x.
- **NestJS 11.x**: Reverse-topological teardown during `app.close()` ensures zero dropped queries or broken sockets when standalone tasks terminate.

---

## 10. Verification & Testing

Unit testing a standalone command runner:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { SeedUsersCommand } from './seed.command';
import { UsersService } from '../users/users.service';

describe('SeedUsersCommand', () => {
  let command: SeedUsersCommand;
  let usersService: Partial<UsersService>;

  beforeEach(async () => {
    usersService = {
      seed: vi.fn().mockResolvedValue(undefined),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        SeedUsersCommand,
        { provide: UsersService, useValue: usersService },
      ],
    }).compile();

    command = module.get<SeedUsersCommand>(SeedUsersCommand);
  });

  it('executes user seeding with parsed flags', async () => {
    await command.run([], { count: 25, admin: true });
    expect(usersService.seed).toHaveBeenCalledWith(25, true);
  });
});
```

---

## 11. Comprehensive Examples

### Kubernetes Job Entrypoint Pattern

```typescript
// src/jobs/data-retention.job.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from '../app.module';
import { RetentionService } from '../retention/retention.service';

async function runJob() {
  const app = await NestFactory.createApplicationContext(AppModule);
  try {
    const service = app.get(RetentionService);
    await service.archiveOldAudits();
    console.log('Job finished successfully.');
  } catch (err) {
    console.error('Job error:', err);
    process.exitCode = 1;
  } finally {
    await app.close();
  }
}

runJob();
```

---

## 12. Cross-References & Further Reading

- [02-first-steps.md](./02-first-steps.md) - NestFactory bootstrapping
- [03-core-concepts/04-modules-and-di/01-dependency-injection.md](../03-core-concepts/04-modules-and-di/01-dependency-injection.md) - Understanding module contexts
- [05-advanced/02-background-processing/04-cron-and-schedules.md](../05-advanced/02-background-processing/04-cron-and-schedules.md) - Scheduled tasks
- [nest-commander Official GitHub](https://github.com/jmcdo29/nest-commander)
