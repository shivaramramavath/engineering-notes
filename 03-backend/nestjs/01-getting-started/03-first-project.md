# First Project

This file walks you through creating, running, calling, changing, testing, and building your first NestJS application. By the end you will have traced a single HTTP request from `main.ts` through a controller and a service and back, added your own routes, generated a CRUD resource, and produced a production build. The goal is a working mental picture, not a tour of every concept. Each building block gets a proper treatment in [02-fundamentals](../02-fundamentals/README.md).

---

## Overview

**What you will build:** a small API with `GET /` (the generated "Hello World"), your own `GET /status` and `POST /echo` routes, and a generated CRUD resource, all tested with `curl`.

**Why start here.** A running application lets you see cause and effect: change a decorator, save, call the endpoint, observe. That feedback loop is how Nest's conventions stick.

**What you need:** the toolchain from [Installation](./01-installation.md) and the commands from [Nest CLI](./02-nest-cli.md).

---

## Mental Model

A Nest application is a **tree of modules** that contains **controllers** (which receive requests) and **providers** (which hold logic), started by a single call to `NestFactory.create()`.

```text
 main.ts
   │  NestFactory.create(AppModule)
   ▼
 ┌────────────────────── AppModule (root) ──────────────────────┐
 │                                                              │
 │   controllers: [AppController]       providers: [AppService] │
 │        │  ▲                                  ▲               │
 │        │  └────── injected by Nest ──────────┘               │
 │        ▼                                                     │
 │   @Get()  getHello()  ──calls──►  AppService.getHello()      │
 └──────────────────────────────────────────────────────────────┘
        ▲                                         │
  HTTP GET /                                'Hello World!'
```

Request path in one line: **HTTP → Express (or Fastify) → Nest router → controller method → service method → response.**

---

## Core Concepts

You only need four ideas to read the generated code.

### Module

A class decorated with `@Module()` that groups related controllers and providers. Every app has a root module (`AppModule`). Details: [Modules](../02-fundamentals/02-modules.md).

### Controller

A class decorated with `@Controller()` whose methods, decorated with `@Get()`, `@Post()`, and so on, handle requests. A controller should be thin: translate HTTP to a method call. Details: [Controllers](../02-fundamentals/03-controllers.md).

### Provider (Service)

A class decorated with `@Injectable()` that Nest can create and inject. Most business logic lives in services. Details: [Providers and Services](../02-fundamentals/04-providers-and-services.md).

### Dependency Injection (as you see it here)

The controller declares what it needs in its constructor. Nest creates the service and passes it in.

```typescript
constructor(private readonly appService: AppService) {}
```

You never write `new AppService()`. Details: [Dependency Injection](../02-fundamentals/05-dependency-injection.md) and [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md) (why this works).

---

## How It Works

What happens when you run `npm run start:dev` and call `GET /`:

```text
 Startup
 ───────
 1. nest start --watch compiles src/ → dist/ and runs the entry file with node
 2. main.ts runs bootstrap():
      NestFactory.create(AppModule)
         ├─ scans AppModule's metadata (imports, controllers, providers)
         ├─ builds the dependency graph
         ├─ instantiates providers (AppService), then controllers (AppController)
         ├─ registers routes with the HTTP adapter (Express by default)
         └─ returns the application object
      app.listen(PORT ?? 3000)  → server is accepting connections

 Request
 ───────
 3. curl http://localhost:3000/
 4. Express receives it → Nest's router matches  GET /  → AppController.getHello
 5. getHello() calls appService.getHello() → 'Hello World!'
 6. Nest sends the string as the response body with status 200
```

Step 2 is explained in depth in [How Nest Boots](../06-internals/01-how-nest-boots.md).

---

## Basic Example

### Step 1: Create the Project

```bash
nest new my-first-api
```

Answer the prompts:

| Prompt | Choose | Why |
|---|---|---|
| Package manager | `npm` (or your preference) | Keep one throughout |
| Module system | **ESM** (default) | Current default. Uses Vitest. Pick CommonJS if your team requires it |
| NestJS Observe | `No` for now | Optional observability platform. Not needed to learn Nest |

Or skip the prompts:

```bash
nest new my-first-api --package-manager npm --skip-git --no-observe
```

`--no-observe` skips the Observe prompt. In non-interactive environments (such as CI) the module system defaults to ESM. *If a flag is rejected by your CLI version, run `nest new --help`.*

### Step 2: Run It

```bash
cd my-first-api
npm run start:dev
```

You should see Nest's startup logs ending with a line similar to:

```text
[Nest] 12345  - 10/04/2026, 10:00:00 AM     LOG [NestApplication] Nest application successfully started
```

(Timestamps and process id vary. Wording can differ by version.)

### Step 3: Call It

In another terminal:

```bash
curl -i http://localhost:3000/
```

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
...

Hello World!
```

You have a running Nest application.

### Step 4: Read the Generated Code

`src/main.ts` (ESM variant):

```typescript
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module.js';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);
  await app.listen(process.env.PORT ?? 3000);
}
await bootstrap();
```

- `NestFactory.create(AppModule)` builds the app from the root module.
- `app.listen(...)` starts the HTTP server on `PORT` or `3000`.
- Top-level `await` is possible because ESM supports it. In a CommonJS project the file ends with `bootstrap();`, and imports have no `.js` extension.
- If creation fails, the process exits with code `1`. To have the error thrown instead, pass `{ abortOnError: false }` to `create()`.

`src/app.module.ts`:

```typescript
import { Module } from '@nestjs/common';
import { AppController } from './app.controller.js';
import { AppService } from './app.service.js';

@Module({
  imports: [],
  controllers: [AppController],
  providers: [AppService],
})
export class AppModule {}
```

`src/app.controller.ts`:

```typescript
import { Controller, Get } from '@nestjs/common';
import { AppService } from './app.service.js';

@Controller()
export class AppController {
  constructor(private readonly appService: AppService) {}

  @Get()
  getHello(): string {
    return this.appService.getHello();
  }
}
```

`src/app.service.ts`:

```typescript
import { Injectable } from '@nestjs/common';

@Injectable()
export class AppService {
  getHello(): string {
    return 'Hello World!';
  }
}
```

> If you answered "yes" to Observe, `AppModule` and `main.ts` also contain the `@nestjs/observe` wiring. The generated files in your project are the source of truth.

---

## Practical Examples

### 1. Basic: Add Your Own GET Route

Edit `src/app.service.ts`:

```typescript
@Injectable()
export class AppService {
  getHello(): string {
    return 'Hello World!';
  }

  getStatus(): { status: string; uptimeSeconds: number } {
    return { status: 'ok', uptimeSeconds: Math.round(process.uptime()) };
  }
}
```

Edit `src/app.controller.ts`:

```typescript
@Get('status')
getStatus() {
  return this.appService.getStatus();
}
```

Save. Watch mode recompiles and restarts. Then:

```bash
curl -i http://localhost:3000/status
```

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

{"status":"ok","uptimeSeconds":12}
```

What to notice: returning an **object** makes Nest respond with JSON automatically. Returning a **string** produced `text/html`. You did not touch Express.

### 2. Common: Add a POST Route With a Body

```typescript
import { Body, Controller, Get, HttpCode, Post } from '@nestjs/common';

@Post('echo')
@HttpCode(200)                 // POST defaults to 201; override for a non-creating action
echo(@Body() body: Record<string, unknown>) {
  return { received: body };
}
```

```bash
curl -i -X POST http://localhost:3000/echo \
  -H 'Content-Type: application/json' \
  -d '{"name":"Ada"}'
```

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

{"received":{"name":"Ada"}}
```

Without `Content-Type: application/json`, the body will not be parsed as JSON. See [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md). This echo endpoint accepts anything. Real endpoints validate input with DTOs and pipes: [Validation Pipe](../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md).

### 3. Common: Route and Query Parameters

```typescript
import { Get, Param, Query } from '@nestjs/common';

@Get('hello/:name')
greet(@Param('name') name: string, @Query('shout') shout?: string) {
  const text = `Hello, ${name}!`;
  return shout === 'true' ? text.toUpperCase() : text;
}
```

```bash
curl http://localhost:3000/hello/Ada             # Hello, Ada!
curl 'http://localhost:3000/hello/Ada?shout=true'  # HELLO, ADA!
```

Route and query parameters arrive as **strings**. Converting and validating them is what [Pipes](../03-core-concepts/01-request-pipeline/03-pipes.md) are for.

### 4. Real-World: Generate a CRUD Resource

```bash
nest g resource tasks --dry-run     # preview
nest g resource tasks               # choose: REST API → yes to CRUD entry points
```

The generator creates `src/tasks/` (module, controller, service, DTOs, entity, specs) and registers `TasksModule` in `AppModule`.

```bash
curl -i http://localhost:3000/tasks             # list
curl -i http://localhost:3000/tasks/1           # one
curl -i -X POST http://localhost:3000/tasks \
  -H 'Content-Type: application/json' -d '{}'
```

The generated service methods return placeholder strings such as `This action returns all tasks`. They are scaffolding. You will replace them with real logic and storage in later stages, starting with [Providers and Services](../02-fundamentals/04-providers-and-services.md).

### 5. Real-World: Change the Port With an Environment Variable

```bash
PORT=4000 npm run start:dev                 # macOS / Linux
$env:PORT=4000; npm run start:dev           # Windows PowerShell
```

`main.ts` reads `process.env.PORT ?? 3000`. You can also use `nest start --env-file .env`. See [Development Environment](./05-development-environment.md).

### 6. Real-World: Run the Tests

```bash
npm test               # unit tests (src/**/*.spec.ts)
npm run test:e2e       # end-to-end tests (test/)
npm run test:cov       # coverage
```

The generated unit test (`src/app.controller.spec.ts`) builds a testing module with the real controller and service and asserts that `getHello()` returns `'Hello World!'`. The generated e2e test boots the whole application and calls `GET /` through `supertest`. ESM projects run these with **Vitest**, CommonJS projects with **Jest**. Test syntax (`describe`, `it`, `expect`) is the same in both. Testing is covered in [04-intermediate/01-testing](../04-intermediate/01-testing/README.md).

### 7. Real-World: Build and Run for Production

```bash
npm run build          # nest build → compiles to dist/
npm run start:prod     # runs the compiled JavaScript with node
```

`start:prod` runs plain `node` on the compiled output. It does not start the CLI, does not watch files, and does not recompile. Check your generated `package.json` for the exact command and output path (see [Project Structure](./04-project-structure.md)).

### 8. Edge Case: Route Order Can Shadow Routes

```typescript
@Get(':id')       // declared first
findOne(@Param('id') id: string) { ... }

@Get('me')        // declared second: may never be reached on Express
me() { ... }
```

On order-sensitive adapters such as Express, `GET /me` matches `:id` first. Declare specific routes before parameterized ones. NestJS 12 adds **opt-in** diagnostics (`routeConflictPolicy`, `routeResolutionStrategy`) to catch this. See [Controllers](../02-fundamentals/03-controllers.md).

### 9. Edge Case: Application Fails at Startup

```typescript
@Controller()
export class AppController {
  constructor(private readonly missing: NotARegisteredService) {}   // not in any module's providers
}
```

Startup fails with an error similar to `Nest can't resolve dependencies of the AppController (?). Please make sure that the argument NotARegisteredService at index [0] is available in the AppModule context.` The cause is nearly always a provider that is not registered (or not exported) in the module that needs it. See [Debugging](./06-debugging.md).

---

## Syntax / API / Commands

| Command | Purpose |
|---|---|
| `nest new <name>` | Create the project |
| `npm run start` | Compile and run once |
| `npm run start:dev` | Watch mode |
| `npm run start:debug` | Watch mode with the inspector |
| `npm run start:prod` | Run the compiled build |
| `npm run build` | Compile to `dist/` |
| `npm test` / `test:watch` / `test:cov` / `test:e2e` | Tests |
| `npm run lint` | Lint with oxlint |
| `npm run format` | Format with Prettier |
| `nest g resource <name>` | Generate a CRUD feature |
| `curl -i URL` | Call the API and show headers |

| Decorator / API | Meaning |
|---|---|
| `@Module({...})` | Declare a module |
| `@Controller('path')` | Declare a controller with a route prefix |
| `@Get()`, `@Post()`, `@Put()`, `@Patch()`, `@Delete()` | HTTP method + path |
| `@Param()`, `@Query()`, `@Body()` | Read request data |
| `@HttpCode(n)` | Set the success status code |
| `@Injectable()` | Mark a class as injectable |
| `NestFactory.create(Module)` | Create the application |
| `app.listen(port)` | Start the HTTP server |

---

## Important Rules

1. **A controller must be listed in a module's `controllers`** or its routes do not exist.
2. **A provider must be listed in a module's `providers`** (and exported if another module needs it) before it can be injected.
3. **Never instantiate services with `new`.** Let Nest inject them.
4. **Return objects for JSON, strings for text.** Nest picks the content type for you (unless you take over the response with `@Res()`).
5. **Route and query parameters are strings** until you convert or validate them.
6. **Declare specific routes before parameterized ones** (on Express).
7. **Do not trust `@Body()` input.** Validate it (DTO + `ValidationPipe`) before using it.
8. **Watch mode restarts the process.** In-memory state is lost on every change.
9. **Production runs compiled output**, not `nest start --watch`.
10. **ESM projects need `.js` extensions on relative imports.** TypeScript does not add them for you.

---

## Under the Hood

### What `nest new` Gives You

A complete, buildable project: TypeScript config, build config, linter, formatter, test runner config, sample module/controller/service, unit and e2e tests, and scripts. Details in [Project Structure](./04-project-structure.md).

### Express by Default

Nest uses `@nestjs/platform-express` unless you choose Fastify. Your controller methods are not Express handlers. Nest wraps them, resolves parameters, applies pipes/guards/interceptors, serializes the return value, and calls the adapter. See [Platform Adapters](../06-internals/05-platform-adapters.md).

### Why `@Get()` Does Anything

`@Get()` is a method decorator that records metadata (HTTP method and path) on the method. At startup Nest reads that metadata and registers the route. Decorators do not run per request. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md).

### Watch Mode Restarts a Child Process

Each successful compile kills the old `node` process and starts a new one, which re-runs `bootstrap()` completely.

---

## Common Patterns

### Feature Folder per Resource

```text
src/
  app.module.ts
  tasks/
    tasks.module.ts
    tasks.controller.ts
    tasks.service.ts
    dto/
    entities/
```

Generated by `nest g resource tasks`. Each feature module is imported into `AppModule`.

### Thin Controller, Fat Service

The controller reads request data and calls one service method. The service holds the logic. This keeps logic testable without HTTP.

### Bootstrap Customization in `main.ts`

```typescript
const app = await NestFactory.create(AppModule);
app.enableCors();
app.setGlobalPrefix('api');
await app.listen(process.env.PORT ?? 3000);
```

Global concerns (prefix, CORS, global pipes, shutdown hooks) are configured here. See [Application Lifecycle](../03-core-concepts/04-modules-and-di/09-application-lifecycle.md).

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Controller not registered in a module | `404 Cannot GET /route` | Routes are only registered for controllers listed in a module | Add it to `controllers`, or use `nest g` which does this for you |
| Service not in `providers` | `Nest can't resolve dependencies of X (?)` | No provider for that class in the module context | Add to `providers`, and `exports`/`imports` across modules |
| Port already in use | `EADDRINUSE :::3000` | Another process (often an old dev server) holds the port | Stop it, or change `PORT` |
| Missing `.js` extension in an ESM project | `ERR_MODULE_NOT_FOUND` at runtime or a TypeScript error | ESM requires explicit extensions | `import ... from './x.js'` |
| Using `__dirname` in an ESM project | `ReferenceError: __dirname is not defined` | Not available in ESM | Use `import.meta.dirname` |
| Missing `Content-Type: application/json` on POST | Empty or unparsed `@Body()` | Body parser selects by header | Send the header |
| Expecting `201` from every POST | Surprise status codes | `POST` defaults to `201` | `@HttpCode(200)` where appropriate |
| Treating `@Param('id')` as a number | `'1' + 1 === '11'` | Params are strings | `ParseIntPipe` (see [Pipes](../03-core-concepts/01-request-pipeline/03-pipes.md)) |
| Editing `dist/` | Changes disappear | `dist/` is rebuilt | Edit `src/` |
| Keeping state in a service field and expecting persistence | Data lost on restart | Watch mode and deploys restart the process | Use a database |
| Running two dev servers | Confusing behavior, port errors | Old terminal still running | Check running processes |
| Forgetting to restart after changing `.env` | Old values used | Env is read at process start | Restart (watch mode only restarts on source changes) |

---

## Debugging

First-run problems and fixes:

| Symptom | Check |
|---|---|
| `nest: command not found` | [Installation](./01-installation.md) troubleshooting |
| `nest new` fails with a Node.js version error | CLI generators need Node.js 22.22.3+, 24.15+, or 26+ |
| App starts but `curl` fails to connect | Right port? Look at the startup log. Is something else bound to that port? |
| `404 Not Found` | Route spelled correctly? Global prefix set? Controller registered? |
| `400 Bad Request` on POST | Invalid JSON, or validation rejected the body. Read the response body |
| Changes not visible | Are you running `start:dev`? Did the build fail? Check the terminal for TypeScript errors |
| Type errors in the editor but app runs | The builder (SWC) may not type-check. Run `npx tsc --noEmit` |

```bash
curl -v http://localhost:3000/          # see exactly what is sent and received
lsof -i :3000                           # who holds the port? (macOS/Linux)
netstat -ano | findstr :3000            # Windows
nest info                               # environment and versions
```

Deeper techniques (inspector, breakpoints, test debugging): [Debugging](./06-debugging.md).

---

## Performance

For a first project, performance is not a concern. Two habits are worth forming:

- Use **watch mode** (`start:dev`) for development and **compiled output** (`start:prod`) for anything resembling production or benchmarking. Never benchmark `start:dev`.
- Keep controllers thin and non-blocking. A synchronous loop inside a handler blocks every request (see [Node.js Async and Event Loop](../00-prerequisites/03-nodejs-async-and-event-loop.md)).

If dev rebuilds feel slow, try `npm run start:dev -- -b swc` ([Nest CLI](./02-nest-cli.md)).

---

## Security

- The `echo` example reflects whatever it receives. Real endpoints must validate input and encode output. Never reflect untrusted HTML.
- A freshly generated app has **no authentication, no authorization, no rate limiting, no CORS policy, and no security headers.** It is a learning scaffold, not a secure baseline. See [Security Fundamentals](../07-production/01-security/01-security-fundamentals.md).
- Do not commit `.env` files with secrets (see [Development Environment](./05-development-environment.md)).
- Do not expose the dev server or the debugger port (`9229`) to the public internet.

---

## Production Considerations

What differs between this tutorial app and a production service:

| Concern | Tutorial | Production |
|---|---|---|
| Process | `nest start --watch` | `node` on compiled output, under a supervisor or orchestrator |
| Config | Hard-coded default port | Validated environment configuration ([Configuration](../03-core-concepts/03-configuration/README.md)) |
| Errors | Default messages | Consistent error format, no stack traces to clients |
| Logging | Console | Structured logs ([Observability](../07-production/03-observability/README.md)) |
| Shutdown | Ctrl+C | Graceful shutdown on `SIGTERM` ([Graceful Shutdown](../07-production/04-deployment/02-graceful-shutdown-and-process-management.md)) |
| Health | None | Liveness and readiness endpoints |
| Data | In memory | Database |

---

## Best Practices

### Recommended

```typescript
@Controller('tasks')
export class TasksController {
  constructor(private readonly tasksService: TasksService) {}

  @Get(':id')
  findOne(@Param('id') id: string) {
    return this.tasksService.findOne(id);       // controller delegates
  }
}
```

### Avoid

```typescript
@Controller('tasks')
export class TasksController {
  private tasks = [];                            // state and logic in the controller

  @Get(':id')
  findOne(@Param('id') id: string) {
    const service = new TasksService();          // bypasses dependency injection
    return service.findOne(id);
  }
}
```

Why: services created with `new` miss their own injected dependencies and cannot be replaced in tests. Controllers that hold logic cannot be reused or tested without HTTP.

Additional guidance:

- Commit right after `nest new` so later diffs show only your changes.
- Run lint, test, and build once on the pristine project. You will know what "green" looks like.
- Read every generated file before editing it.
- Prefer generating files with the CLI so modules are updated consistently.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `nest new` module system | Prompt: ESM (default) or CommonJS | CommonJS only |
| Generated tests | Vitest (ESM) or Jest (CommonJS) | Jest |
| Linter | oxlint | ESLint |
| `main.ts` | `await bootstrap();` (ESM) | `bootstrap();` |
| Relative imports | `./x.js` (ESM) | `./x` |
| Node.js to run | 20.19+ or 22.12+ | 20+ (v11) |
| Node.js to generate | 22.22.3+, 24.15+, or 26+ | Lower |
| Route conflict diagnostics | Opt-in `routeConflictPolicy`, `routeResolutionStrategy` | Not available |

*Compare against your own generated project. Anything that differs there takes precedence over this table.*

---

## Real-World Use Cases

- **Prototyping an API** in minutes before deciding on structure.
- **Teaching and onboarding:** the generated app is the common starting point for tutorials and exercises.
- **Template for microservices:** many teams start every service from `nest new` plus an internal template.
- **Spike for a library or feature:** isolate an idea (a queue, a gateway) in a fresh project.
- **Interview take-home tasks:** a fresh Nest project with a CRUD resource is a common starting exercise.

---

## Interview Questions

### Beginner

1. What does `nest new` create?
   - A project with config files, a `src/` folder (module, controller, service, `main.ts`), tests, and scripts, plus installed dependencies.
2. What is the entry point of a Nest application?
   - `src/main.ts`, which calls `NestFactory.create(AppModule)` and `app.listen()`.
3. What are the roles of a module, a controller, and a service?
   - Module: groups related pieces. Controller: handles HTTP routes. Service: holds business logic.
4. How do you run in watch mode?
   - `npm run start:dev` (which runs `nest start --watch`).

### Intermediate

1. How does a controller get its service instance?
   - Through constructor injection. Nest creates the provider and passes it in because it is registered in the module.
2. What do you return from a handler to get JSON?
   - An object or array. A string produces a text response.
3. What is the default status code of `@Post()` and how do you change it?
   - `201`. Use `@HttpCode()`.
4. Why must a controller be listed in a module?
   - Nest builds routes from the module graph. Unlisted controllers are never instantiated or routed.
5. What is the difference between `start:dev` and `start:prod`?
   - `start:dev` compiles, watches, and restarts. `start:prod` runs the already compiled output with plain `node`.

### Advanced

1. Describe what happens between `node dist/main` and the first request being served.
   - `NestFactory.create` scans module metadata, builds the dependency graph, instantiates providers and controllers, registers routes on the adapter, and `listen` binds the port.
2. Why can route declaration order matter, and how does NestJS 12 help?
   - Express matches in registration order, so `:id` can shadow `me`. Version 12 adds opt-in route conflict diagnostics and a specificity-based resolution strategy.
3. What changes in a project when you choose ESM instead of CommonJS?
   - `"type": "module"`, `nodenext` module settings, `.js` extensions in relative imports, no `__dirname`, top-level `await`, and Vitest instead of Jest.
4. Why shouldn't you benchmark `start:dev`?
   - It includes watch tooling and non-optimized startup. Benchmark the compiled output in production-like conditions.
5. How would you turn this scaffold into something production-ready?
   - Add validation, configuration, error handling, logging, health checks, security middleware, persistence, tests, and a container-based deployment with graceful shutdown.

---

## Quick Reference

```text
Create         nest new <name>
Run            npm run start:dev          (watch)  ·  npm run start:prod  (compiled)
Call           curl -i http://localhost:3000/
Generate       nest g resource <name> [--dry-run]
Test           npm test  ·  npm run test:e2e  ·  npm run test:cov
Build          npm run build   → dist/

Wiring         @Module({ controllers: [...], providers: [...] })
Route          @Controller('x') + @Get('y')  → GET /x/y
DI             constructor(private readonly svc: Service) {}
Data           @Param('id') string · @Query('q') string · @Body() object
Response       object → JSON · string → text · @HttpCode(n) → status
ESM            import './x.js' · await bootstrap() · import.meta.dirname
Port           process.env.PORT ?? 3000
```

---

## Key Takeaways

- `nest new` gives you a runnable, testable, buildable app. Run the tests and the build once before changing anything.
- Everything starts in `main.ts`: `NestFactory.create(AppModule)` builds the module graph, and `listen` serves it.
- Controllers receive requests, services hold logic, and modules wire them together. Nest injects services through constructors.
- Return objects for JSON. Route and query parameters are strings. Body data must be validated.
- Watch mode restarts the process, so state in memory is temporary. Production runs compiled output.
- A fresh project has no authentication, validation, security headers, or rate limiting. It is a scaffold.
- ESM is the default for new v12 projects: `.js` import extensions, top-level `await`, Vitest.

---

## Related Topics

```text
02 Nest CLI
      ↓
[03 First Project]
      ↓
04 Project Structure  →  02-fundamentals (Modules, Controllers, Providers)
```

- [Getting Started Overview](./README.md)
- [Nest CLI](./02-nest-cli.md)
- [Project Structure](./04-project-structure.md)
- [Debugging](./06-debugging.md)
- [Modules](../02-fundamentals/02-modules.md)
- [Controllers](../02-fundamentals/03-controllers.md)
- [Providers and Services](../02-fundamentals/04-providers-and-services.md)
- [Request Lifecycle](../02-fundamentals/09-request-lifecycle.md)
- [How Nest Boots](../06-internals/01-how-nest-boots.md)
