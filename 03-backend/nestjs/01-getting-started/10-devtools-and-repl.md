# NestJS Devtools and REPL

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 01-getting-started
- **Prerequisites**: [02-first-steps](./02-first-steps.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Debugging large modular applications is challenging when the dependency injection tree spans dozens of modules, dynamic providers, and lifecycle hooks. Developers frequently need to inspect module relationships, visualize circular references, diagnose initialization bottlenecks, or interactively test service methods without launching a full HTTP client or creating throwaway scripts.

NestJS provides two dedicated tools for this:
1. **NestJS REPL**: An interactive read-eval-print loop running inside your compiled application context, allowing direct execution of service methods and container inspection from the terminal.
2. **NestJS Devtools**: A visual debugging and profiling platform that graphs module dependencies, tracks provider resolution times, and visualizes route hierarchies.

---

## 2. Theoretical Foundations

Both REPL and Devtools tap into the internal NestJS **Application Context** and metadata graph (`ModuleRef`, `InstanceLoader`, `DependenciesScanner`).
- **Interactive Inspection**: The REPL exposes the DI container to Node's native `readline` interface. Providers can be resolved on demand and evaluated with live inputs.
- **Graph Serialization**: Devtools instruments module scanning. During bootstrap, the framework serializes the complete Directed Acyclic Graph (DAG) of modules, controllers, providers, enhancers (guards, interceptors, pipes), and their injection relationships.

---

## 3. Architecture & Internal Mechanics

```
┌─────────────────────────────────────────────────────────────┐
│                   NestJS Application Graph                  │
│  [AppModule] ──> [UsersModule] ──> [DatabaseModule]         │
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
               ▼                              ▼
  ┌────────────────────────┐      ┌─────────────────────────┐
  │       REPL Mode        │      │    Devtools Publisher   │
  │   (repl.ts / CLI)      │      │   (@nestjs/devtools)    │
  ├────────────────────────┤      ├─────────────────────────┤
  │ - get(UsersService)    │      │ - HTTP / WS Bridge      │
  │ - select(UsersModule)  │      │ - Visual DAG inspection │
  │ - debug() / methods()  │      │ - Startup timing metrics│
  └────────────────────────┘      └─────────────────────────┘
```

### 3.1 REPL Initialization
The REPL bypasses HTTP server listening. Instead of `NestFactory.create()`, it initializes via `repl(AppModule)`. It starts the application lifecycle, instantiates the DI container, and connects to `process.stdin`.

---

## 4. Concrete Implementation

### 4.1 Setting Up the NestJS REPL

Create an entrypoint file `src/repl.ts`:

```typescript
// src/repl.ts
import { repl } from '@nestjs/core';
import { AppModule } from './app.module';

async function bootstrap() {
  const replServer = await repl(AppModule);
  replServer.setupHistory('.repl_history', (err) => {
    if (err) {
      console.error('Failed to setup REPL history', err);
    }
  });
}
bootstrap();
```

Add a run script to `package.json`:
```json
{
  "scripts": {
    "start:repl": "nest start --entryFile repl --watch"
  }
}
```

#### Common REPL Commands
Run `npm run start:repl`:

```text
$ npm run start:repl
LOG [NestFactory] Starting Nest application...
LOG [InstanceLoader] AppModule dependencies initialized
> methods(UsersService)
[ 'findAll', 'findById', 'create', 'delete' ]

> get(UsersService).findAll()
Promise { <pending> }
[ { id: 1, name: 'Alice', email: 'alice@example.com' } ]

> select(UsersModule).controllers
[ [class UsersController] ]

> debug()
Shows full module tree and registered providers.
```

---

### 4.2 Integrating NestJS Devtools

Install the devtools integration package:
```bash
npm install --save @nestjs/devtools-integration
```

Configure Devtools in `src/main.ts`:

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';

async function bootstrap() {
  const app = await NestFactory.create(AppModule, {
    snapshot: true, // Enables internal graph tracking
  });

  // Only enable Devtools in non-production environments
  if (process.env.NODE_ENV !== 'production') {
    const { Devtools } = await import('@nestjs/devtools-integration');
    // Initializes the local Devtools server (default port 8000)
    // Connect to https://devtools.nestjs.com
  }

  await app.listen(3000);
}
bootstrap();
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Request-Scoped Providers in REPL
Request-scoped (`Scope.REQUEST`) or transient providers cannot be retrieved via `get(Service)` because they have no singleton instance in the root container.
- **Handling**: Use `await resolve(RequestScopedService)` to dynamically instantiate a transient scope within the REPL session.

### 5.2 Leaking Devtools to Production
Enabling `snapshot: true` and the Devtools integration package in production creates a memory overhead (retaining the AST/metadata graph) and exposes internal routing and environment architecture over local ports.
- **Prevention**: Gate initialization behind strict `process.env.NODE_ENV !== 'production'` checks.

---

## 6. Performance & Memory Implications

- **REPL Overhead**: Zero production overhead; REPL is run as a dedicated developer process.
- **Devtools Snapshot**: When `snapshot: true` is active, NestJS retains metadata nodes for every class and provider. This adds roughly 5–15 MB of heap memory during development.

---

## 7. Anti-Patterns & Common Mistakes

- **Writing Throwaway Express/Curl Scripts for Testing Services**: Spawning separate HTTP callers instead of loading REPL to test isolated database queries or service methods directly.
- **Invoking Destructive Methods in Production via REPL**: Connecting REPL to a live production database and running `get(UsersService).deleteAll()`.
- **Leaving Unclosed Database Connections in REPL**: Terminating the REPL with `Ctrl+C` without executing `close()` can leave dangling connections on local databases.

---

## 8. Production Checklist & Operational Readiness

- [ ] `snapshot: false` (or unset) in production `main.ts`.
- [ ] `@nestjs/devtools-integration` is excluded or strictly bypassed in production builds.
- [ ] REPL entry point (`repl.ts`) is omitted from production deployment containers (`tsconfig.build.json` excludes `**/*repl.ts`).

---

## 9. Migration & Version History

- **NestJS 9.x**: First introduction of the interactive `repl()` function in `@nestjs/core`.
- **NestJS 9.3**: Release of the official NestJS Devtools platform.
- **NestJS 11.x**: Support for Express 5 routing graph introspection and reversed-lifecycle teardown visualization in Devtools.

---

## 10. Verification & Testing

Programmatic verification of REPL bootstrapping:

```typescript
import { Test } from '@nestjs/testing';
import { AppModule } from './app.module';

describe('REPL Integration Check', () => {
  it('creates application context successfully without HTTP listener', async () => {
    const context = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    expect(context).toBeDefined();
    await context.close();
  });
});
```

---

## 11. Comprehensive Examples

### Custom REPL Helper Script

```typescript
// src/repl-helpers.ts
import { INestApplicationContext } from '@nestjs/common';
import { UsersService } from './users/users.service';

export async function seedDemoUser(app: INestApplicationContext) {
  const usersService = app.get(UsersService);
  const user = await usersService.create({
    name: 'REPL Tester',
    email: `repl_${Date.now()}@example.com`,
  });
  console.log('Seeded User:', user);
  return user;
}
```

In the REPL session:
```text
> const helpers = await import('./repl-helpers');
> await helpers.seedDemoUser(app);
```

---

## 12. Cross-References & Further Reading

- [02-first-steps.md](./02-first-steps.md) - Application bootstrapping basics
- [03-core-concepts/04-modules-and-di/01-dependency-injection.md](../03-core-concepts/04-modules-and-di/01-dependency-injection.md) - Understanding the DI container
- [Official NestJS REPL Guide](https://docs.nestjs.com/recipes/repl)
- [Official NestJS Devtools](https://devtools.nestjs.com)
