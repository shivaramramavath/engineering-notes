# Debugging

Debugging a NestJS application means choosing the cheapest tool that answers your question: a log line, a breakpoint, a failing test, or a look at the module graph. This file covers all of them: Nest's built-in `Logger`, the Node.js inspector through `nest start --debug`, attaching VS Code and Chrome DevTools, debugging tests, the REPL and Devtools, and a decoder for the errors you will meet in your first weeks (port conflicts, unresolved dependencies, ESM import failures, wrong Node.js versions, routes that return 404).

---

## Overview

**What it is.** A set of techniques for finding why code does not behave as expected: observing (logs), pausing (breakpoints), isolating (tests, minimal repros), and inspecting (module graph, effective config).

**Why it matters.** Most Nest errors fall into a few families (dependency resolution, module wiring, configuration, module format). Recognizing the family cuts debugging time from hours to minutes.

**Where it is used.** Local development, test failures, CI failures, and production incidents (with safer tools: logs, metrics, traces).

**Why you should understand it.** Nest hides a lot of wiring behind decorators. When wiring breaks, the symptoms (`undefined` injected, 404 on a route that "exists") only make sense if you know what to inspect.

---

## Mental Model

Work from cheap to expensive, and from the outside in:

```text
 Something is wrong
        │
        ▼
 1. Read the error message fully (Nest errors name the class and parameter index)
        │
        ▼
 2. Reproduce with the smallest request:  curl -v ...
        │
        ▼
 3. Look at logs (raise the log level)
        │
        ▼
 4. Check wiring: is it registered? exported? imported? (module graph / Devtools)
        │
        ▼
 5. Pause execution: breakpoint via the inspector (nest start --debug)
        │
        ▼
 6. Isolate: write a failing test or a minimal repro project
        │
        ▼
 7. Compare environments: nest info · node version · tsconfig · builder
```

The inspector is powerful but slow to set up. Most Nest bugs are visible in steps 1 to 4.

---

## Core Concepts

### The Nest `Logger`

Nest ships with a built-in logger. Log level and output are configured when creating the app.

```typescript
import { Logger } from '@nestjs/common';

const app = await NestFactory.create(AppModule, {
  logger: ['error', 'warn', 'log', 'debug', 'verbose'],   // choose levels
});
```

| Level | Use |
|---|---|
| `fatal` | Unrecoverable failures |
| `error` | Errors |
| `warn` | Suspicious situations |
| `log` | General information (default level) |
| `debug` | Developer detail |
| `verbose` | Very detailed |

Use it in your own classes so log lines carry a context name:

```typescript
@Injectable()
export class TasksService {
  private readonly logger = new Logger(TasksService.name);

  findOne(id: string) {
    this.logger.debug(`findOne(${id})`);
    // ...
  }
}
```

NestJS 12 changed `ConsoleLogger` so that plain objects passed after the message are treated as **structured params** on the same entry (nested under `params` in JSON mode, or flattened with `flattenParams`). Set `structuredParams: false` to restore the earlier behavior.

```typescript
this.logger.log('Task created', { taskId: 42, userId: 7 });
```

Logging in depth: [Logging Fundamentals](../07-production/03-observability/01-logging-fundamentals.md).

### The Node.js Inspector

Node.js includes a debugger protocol (the **inspector**) that VS Code and Chrome DevTools use. It listens on a port, `9229` by default.

| Flag | Behavior |
|---|---|
| `node --inspect app.js` | Start and listen for a debugger. Do not pause |
| `node --inspect-brk app.js` | Start and **pause on the first line** until a debugger attaches |
| `--inspect=0.0.0.0:9229` | Listen on all interfaces (**dangerous**, see Security) |

With the Nest CLI:

```bash
nest start --debug --watch        # adds --inspect to the node process, restarts on change
nest start --debug 0.0.0.0:9229   # custom host:port (see Security before using)
```

The generated `start:debug` script runs `nest start --debug --watch`. Each file change restarts the child process, so the debugger session is dropped and must reattach (VS Code can do this automatically with `"restart": true`).

### Source Maps

Breakpoints in `.ts` files work because the build emits **source maps** (`sourceMap: true` in `tsconfig.json`) that map compiled JavaScript back to TypeScript. If breakpoints show as gray or "unbound", source maps or the `outFiles`/path mapping are the first suspects. For readable stack traces from plain `node dist/...` runs, use `--enable-source-maps`.

### Breakpoints, Stepping, and the Debug Console

| Action | What it does |
|---|---|
| Breakpoint | Pauses execution at a line |
| Conditional breakpoint | Pauses only when an expression is true (`id === '42'`) |
| Logpoint | Logs a message without editing code or pausing |
| Step over / into / out | Execute one line / enter a call / finish the current function |
| Call stack | Shows how execution got here (async frames appear for `await`) |
| Watch / Debug console | Evaluate expressions in the paused scope |

Logpoints are the cleanest way to add "temporary console.log" without modifying files.

### Debugging Tests

Tests are often the best debugging environment: small, repeatable, and fast. The test runner depends on your module system.

| Project | Runner | Typical approach |
|---|---|---|
| ESM (default) | Vitest | Run a single test file, use `--inspect-brk`, or the VS Code Vitest extension |
| CommonJS | Jest | Run with `node --inspect-brk` against the Jest binary and `--runInBand` |

```bash
npx vitest run src/tasks/tasks.service.spec.ts          # one file
npx vitest run -t "creates a task"                       # one test by name
```

For stepping through a test, the VS Code **JavaScript Debug Terminal** (it auto-attaches to any `node` process started in it) is the lowest-friction option for either runner:

```text
Command Palette → "Debug: JavaScript Debug Terminal" → run  npx vitest run <file>
```

Deeper test debugging flags differ between runners and versions. *Verify the current `--inspect-brk` and single-thread options in the Vitest or Jest documentation for your version.*

### The REPL

Nest has a REPL that boots your application's module graph without an HTTP server so you can call providers interactively.

```bash
npm run start -- --entryFile repl
```

This requires a `src/repl.ts` that calls `repl(AppModule)` from `@nestjs/core`. Inside the REPL you can call methods such as `get(TasksService).findAll()`, `debug()` (print the module graph), and `methods(TasksService)`. See the NestJS REPL recipe in the official docs. *Verify the exact entry file setup for ESM projects.*

### Nest Devtools

`@nestjs/devtools-integration` exposes your application graph (modules, providers, routes) to the official Devtools UI so you can see what is registered and how things connect, which is exactly what you need for dependency-resolution problems. It is intended for **development only**. See the Devtools chapter in the official documentation for setup. *Verify the current registration API for your Nest version.*

### Route Conflict Diagnostics (NestJS 12)

Route registration order matters on Express: `@Get(':id')` declared before `@Get('me')` shadows it. NestJS 12 adds two **opt-in** options:

```typescript
const app = await NestFactory.create(AppModule, {
  routeConflictPolicy: { duplicate: 'error', shadow: 'warn' },
  routeResolutionStrategy: 'specificity',
});
```

Both default to the previous behavior. Enable `shadow: 'warn'` in development to be told about shadowed routes at startup.

### Error Families

Almost every beginner error belongs to one of these:

| Family | Typical message | Usual cause |
|---|---|---|
| Dependency resolution | `Nest can't resolve dependencies of the X (?)` | Provider not registered/exported, interface used as a type, `import type` on a class, circular import, missing metadata flags |
| Module wiring | `404 Cannot GET /x` | Controller not registered, wrong path or prefix, route shadowing |
| Port / process | `EADDRINUSE` | Another process holds the port |
| Module format (ESM/CJS) | `ERR_MODULE_NOT_FOUND`, `__dirname is not defined` | Missing `.js` extension, CommonJS globals in ESM |
| Environment | `ERR_REQUIRE_ASYNC_MODULE`, Node.js version errors | Wrong Node.js version, Jest on Node.js < 24.9 |
| Validation | `400` with message array | DTO validation failing (often correct behavior) |
| Configuration | `undefined` env values | `.env` not loaded, typo, process not restarted |

---

## How It Works

How an attached debugger works with `nest start --debug --watch`:

```text
 nest start --debug --watch
        │
        ├─ builder compiles src/ → dist/ (+ source maps)
        └─ spawns:  node --inspect dist/main
                          │
                          │  inspector listens on 127.0.0.1:9229
                          ▼
              ┌─────────────────────────┐
              │ VS Code / Chrome DevTools│  attach over WebSocket (DevTools protocol)
              └────────────┬────────────┘
                           │ sets breakpoints in main.ts, tasks.service.ts, ...
                           │ (translated to dist/*.js positions via source maps)
                           ▼
              request arrives → execution reaches breakpoint → paused → inspect/step

 File change → rebuild → child process killed → new process spawned → debugger reattaches
                                                   (with "restart": true in VS Code)
```

How Nest reports a dependency-resolution failure:

```text
 NestFactory.create(AppModule)
    ├─ scan modules → build provider registry
    ├─ for each provider/controller: read constructor param tokens
    ├─ token not found in the module's providers/imports/exports?
    │       └─► throw UnknownDependenciesException  →  message names:
    │              • the class being created
    │              • the index of the parameter that failed
    │              • the module context where it looked
    └─ process exits with code 1 (unless abortOnError: false)
```

---

## Basic Example

A complete debugging session in VS Code.

**1. Add a launch configuration** (`.vscode/launch.json`):

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Attach to Nest (9229)",
      "type": "node",
      "request": "attach",
      "port": 9229,
      "restart": true,
      "sourceMaps": true,
      "skipFiles": ["<node_internals>/**"]
    }
  ]
}
```

**2. Start Nest in debug mode:**

```bash
npm run start:debug
```

Look for the line `Debugger listening on ws://127.0.0.1:9229/...` in the output.

**3. Attach:** run the "Attach to Nest (9229)" configuration (F5).

**4. Set a breakpoint** in a controller method, for example in `src/app.controller.ts` on the line inside `getHello()`.

**5. Trigger it:**

```bash
curl -i http://localhost:3000/
```

**6. Inspect:** execution pauses at your breakpoint. Look at variables, the call stack, and use the Debug Console. Step into `appService.getHello()` to see the call continue into the service. Resume with F5.

What happens:

1. `--debug` makes the Nest CLI start `node` with `--inspect`, which opens port 9229.
2. VS Code attaches over the DevTools protocol.
3. Source maps translate your `.ts` breakpoint to the matching compiled `.js` line.
4. `"restart": true` reattaches after each watch-mode restart.

**Even simpler:** open the **JavaScript Debug Terminal** (Command Palette → "Debug: JavaScript Debug Terminal") and run `npm run start:dev` there. VS Code auto-attaches to the spawned `node` process, and you need no `launch.json`.

---

## Practical Examples

### 1. Basic: Turn Up Logging

```typescript
// src/main.ts
const app = await NestFactory.create(AppModule, {
  logger: process.env.NODE_ENV === 'production' ? ['error', 'warn', 'log'] : ['error', 'warn', 'log', 'debug', 'verbose'],
});
```

Example debug output:

```text
[Nest] 12345  - 10/04/2026, 10:00:00 AM   DEBUG [TasksService] findOne(42)
```

(Format and timestamps vary by version.)

### 2. Common: Fix `Nest can't resolve dependencies`

```text
Nest can't resolve dependencies of the TasksController (?). Please make sure that the argument TasksService at index [0] is available in the TasksModule context.
```

Read it literally: **class** `TasksController`, **parameter index** `0`, **type** `TasksService`, **module** `TasksModule`. Checklist:

```text
□ Is TasksService listed in `providers` of TasksModule?
□ If it lives in another module: is that module imported, and does it export TasksService?
□ Does TasksService have @Injectable()?
□ Is the constructor type a CLASS (not an interface or a union)?
□ Is the import a normal import, not `import type`?
□ Are experimentalDecorators + emitDecoratorMetadata true?
□ Circular import between the two files? (the `?` may be undefined)
□ Did you switch builders (SWC/Rspack) recently? Compare with `-b tsc`.
```

Fix example (cross-module):

```typescript
// users.module.ts
@Module({ providers: [UsersService], exports: [UsersService] })
export class UsersModule {}

// tasks.module.ts
@Module({ imports: [UsersModule], providers: [TasksService], controllers: [TasksController] })
export class TasksModule {}
```

### 3. Common: `EADDRINUSE`

```text
Error: listen EADDRINUSE: address already in use :::3000
```

```bash
lsof -i :3000                       # macOS/Linux: find the process
kill <PID>                          # or kill -9 <PID> if it ignores SIGTERM
netstat -ano | findstr :3000        # Windows
taskkill /PID <PID> /F              # Windows
PORT=3001 npm run start:dev         # or just use another port
```

Typical cause: an earlier dev server still running in another terminal or a crashed watch process.

### 4. Common: A Route Returns 404

```bash
curl -i http://localhost:3000/tasks
```

Checklist:

```text
□ Controller listed in a module's `controllers` array, and that module imported (directly or transitively) by AppModule?
□ Path correct? @Controller('tasks') + @Get() → /tasks
□ Global prefix set? app.setGlobalPrefix('api') → /api/tasks
□ Versioning enabled? → /v1/tasks
□ HTTP method matches (GET vs POST)?
□ Shadowed by an earlier parameterized route (@Get(':id') before @Get('me'))?
□ Restarted after changes? Build errors in the terminal?
```

Enable shadow warnings in development:

```typescript
NestFactory.create(AppModule, { routeConflictPolicy: { shadow: 'warn', duplicate: 'error' } });
```

### 5. Common: ESM Import Errors

```text
Error [ERR_MODULE_NOT_FOUND]: Cannot find module '/app/dist/app.module' imported from /app/dist/main.js
```

Cause: a relative import without a `.js` extension in an ESM project.

```typescript
// Wrong in ESM
import { AppModule } from './app.module';
// Right in ESM
import { AppModule } from './app.module.js';
```

```text
ReferenceError: __dirname is not defined in ES module scope
```

```typescript
// Wrong in ESM
join(__dirname, 'hero/hero.proto')
// Right in ESM
join(import.meta.dirname, 'hero/hero.proto')
```

### 6. Common: Wrong Node.js Version

```bash
node --version
nest info
```

| Symptom | Fix |
|---|---|
| `nest new` / `nest generate` / `nest upgrade` refuses to run | Use Node.js 22.22.3+, 24.15+, or 26+ |
| App fails to start with `ERR_REQUIRE_ESM` | Node.js lacks `require(esm)`: use 20.19+ or 22.12+ |
| Jest tests fail with `ERR_REQUIRE_ASYNC_MODULE` | Run Jest on Node.js 24.9+, or move to Vitest |

### 7. Real-World: Conditional Breakpoint and Logpoint

In VS Code, right-click the breakpoint gutter:

- **Conditional breakpoint:** `request.userId === 42`. It pauses only for that user.
- **Logpoint:** `Handling {id} for user {request.userId}`. It logs without pausing and without modifying code.

Use these on hot paths where an unconditional breakpoint would pause hundreds of times.

### 8. Real-World: Debug a Failing Test

```bash
npx vitest run src/tasks/tasks.service.spec.ts -t "creates a task"
```

In a JavaScript Debug Terminal, run the same command with a breakpoint set in the test or the service. For stack traces in plain `node` runs:

```bash
node --enable-source-maps dist/main
```

If a test fails only when the whole suite runs, suspect shared state (module-level variables, singletons not reset between tests) and run tests in isolation to confirm.

### 9. Real-World: Inspect the Module Graph With the REPL

```typescript
// src/repl.ts  (ESM style imports)
import { repl } from '@nestjs/core';
import { AppModule } from './app.module.js';

async function bootstrap() {
  await repl(AppModule);
}
await bootstrap();
```

```bash
npm run start -- --entryFile repl
```

```text
> debug()                 // prints modules with their controllers and providers
> methods(TasksService)   // lists methods
> await get(TasksService).findAll()
```

*Use `help()` inside the REPL to see the available commands for your version.*

### 10. Edge Case: Debugging Inside Docker

```bash
# container: expose the inspector, but bind it so only the host can reach it
docker run --rm -p 127.0.0.1:9229:9229 -p 3000:3000 my-app \
  node --inspect=0.0.0.0:9229 dist/main
```

Inside the container the inspector must bind `0.0.0.0` to be reachable through the port mapping, but publish the port only to `127.0.0.1` on the host (as above) and never in production. In VS Code, attach to `localhost:9229` and set `localRoot`/`remoteRoot` if source paths differ. Mismatched paths are a common cause of unbound breakpoints in containers.

### 11. Edge Case: Breakpoints Show as Unbound

Check, in order:

```text
□ Is sourceMap: true in the tsconfig the build uses?
□ Did the build succeed, and is the debugger attached to the CURRENT process (restart: true)?
□ Does `dist/` contain .js.map files?
□ Does the debugger's source path map to your files (rootDir/outDir, Docker localRoot/remoteRoot)?
□ Is the file actually executed (an old copy in a different folder)?
□ Using SWC? Ensure source maps are emitted for it.
```

### 12. Edge Case: Swallowed Errors

An `async` handler that is not awaited or a promise created and not returned loses its error (the symptom is "nothing happens"). Add temporary logging around the call and check for floating promises with a typed lint rule or a code review. See [Node.js Async and Event Loop](../00-prerequisites/03-nodejs-async-and-event-loop.md).

---

## Syntax / API / Commands

| Command / API | Purpose |
|---|---|
| `nest start --debug --watch` (`npm run start:debug`) | Run with the inspector, restart on change |
| `nest start --debug host:port` | Custom inspector address |
| `node --inspect-brk dist/main` | Pause at start until a debugger attaches |
| `node --enable-source-maps dist/main` | TypeScript line numbers in stack traces |
| `node --cpu-prof dist/main` | Write a CPU profile on exit |
| `node --heap-prof dist/main` | Write a heap profile |
| `chrome://inspect` | Attach Chrome DevTools to a Node.js process |
| `nest info` | Versions and environment |
| `curl -v URL` | Raw request and response |
| `npm run start -- --entryFile repl` | Start the REPL entry |
| `new Logger(Name.name)` | Context-aware logger |
| `NestFactory.create(AppModule, { logger: [...] })` | Choose log levels |
| `NestFactory.create(AppModule, { abortOnError: false })` | Throw startup errors instead of exiting |
| `lsof -i :PORT` / `netstat -ano \| findstr :PORT` | Find a process holding a port |

---

## Important Rules

1. **Read the full error.** Nest names the class, the parameter index, and the module context.
2. **Reproduce with `curl -v` before debugging code.** Know exactly what request fails.
3. **Dependency errors are wiring errors.** Check registration, exports, imports, class-vs-interface, and `import type` before anything else.
4. **Never expose the inspector port publicly.** A reachable inspector allows arbitrary code execution.
5. **Keep source maps on** in development.
6. **Debug the compiled app in the same mode you run it.** A bug that appears only in `dist/` (production build) needs a production-build debug session.
7. **Raise log levels temporarily, then lower them.** Do not leave `verbose` in production.
8. **Prefer tests to manual repro steps.** A failing test stays fixed.
9. **ESM projects need `.js` extensions and `import.meta.dirname`.**
10. **Don't log secrets, tokens, or full request bodies.**
11. **Restart after changing environment variables.** Watch mode reacts to source changes only.
12. **Compare environments** (`nest info`, Node.js version, builder) when something works on one machine only.

---

## Under the Hood

### The Inspector Protocol

`--inspect` starts a WebSocket server implementing the Chrome DevTools Protocol. Debuggers send commands (set breakpoint, step, evaluate) and receive events (paused, console output). Because the protocol can evaluate arbitrary JavaScript in your process, **anyone who can connect can run code as your application.**

### Why Breakpoints Pause the Whole Process

Node.js runs your JavaScript on one thread. Pausing at a breakpoint stops the event loop: other requests wait and timers do not fire. Health checks and WebSocket heartbeats may time out while you are stopped. This is another reason not to debug against shared or production environments.

### Async Stack Traces

Because `await` splits a function into continuations, plain stack traces would lose the callers. Modern V8 reconstructs async call stacks for `async`/`await` code. Raw callbacks and some event emitters do not get this treatment, so prefer `async`/`await` for debuggable code.

### How `UnknownDependenciesException` Gets Its Message

The injector walks constructor parameter tokens (from `design:paramtypes` or `@Inject()`), looks each up in the module's provider registry and its imported modules' exports, and, when it cannot find one, records the class, the index, and the module. The `(?)` placeholder marks the unresolvable parameter. If the token itself is `undefined` (usually a circular import) or `Object` (an interface or union), the message cannot name a useful type, which is itself a clue.

### Structured Logging

`ConsoleLogger` can emit JSON. Extra objects passed after the message become structured params on the same entry (a NestJS 12 behavior, disable with `structuredParams: false`), which makes log search and aggregation far easier than free-form strings.

---

## Common Patterns

### Log at Boundaries

Log request entry and key decisions at `debug` level in services. Avoid logging inside tight loops.

### Bisect by Registration

Comment out half of a module's `imports`/`providers`, start the app, and see which half contains the failing dependency. Fast for large modules.

### Failing Test First

Reproduce the bug as a unit or e2e test, fix the code, keep the test.

### Minimal Repro Project

`nest new repro --skip-git`, add only the failing pieces. If the repro works, the problem is in your project's configuration, not Nest.

### Environment Diff

Run `nest info` and compare Node.js and Nest versions between the working and failing machines or between local and CI.

### Dev-Only Diagnostics

Enable `routeConflictPolicy` warnings and Devtools in development only.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Provider not in `providers` | `Nest can't resolve dependencies` | Not registered in that module | Add it, or import the module that exports it |
| Provider not exported | Same error across modules | Providers are private to their module by default | `exports: [Service]` and `imports: [Module]` |
| Interface as constructor type | Error naming `Object` | Interfaces are erased | Use a class or an `@Inject()` token |
| `import type` on an injected class | Dependency unresolved | Reference erased, metadata lost | Normal import |
| Circular file imports | `?` / `undefined` dependency | Evaluation order | Break the cycle, `forwardRef` as a last resort |
| Debugging with `console.log` only | Slow, noisy iterations | No stepping or inspection | Use breakpoints, logpoints, or tests |
| Debugger disconnects on every change | Constant reattaching | Watch mode restarts the process | `"restart": true` or the JS Debug Terminal |
| Breakpoint "unbound" | Never hits | Source maps/path mapping broken, or stale process | Check `sourceMap`, `dist/*.map`, path mapping |
| Exposing 9229 publicly | Remote code execution risk | Inspector bound to `0.0.0.0` and published | Bind to localhost, publish to `127.0.0.1`, never in production |
| Ignoring `404` causes | Wasted time | Assuming the route exists | Check controller registration, prefix, versioning, shadowing |
| Leaving `verbose` logs on in production | Log flood, performance cost, leaked data | Dev config shipped | Environment-based log levels |
| Debugging a test in watch mode with shared state | Heisenbugs | State persists between runs | Run a single file or test in isolation |
| Forgetting `await` | "Nothing happened", unhandled rejections | Floating promise | Add `await`, enable lint/type rules for floating promises |
| Using `__dirname` or missing `.js` in ESM | Startup crash | ESM semantics | `import.meta.dirname`, explicit extensions |

---

## Debugging

This file is the debugging reference for the repository. Quick triage tables:

### Startup Errors

| Error | First check |
|---|---|
| `Nest can't resolve dependencies of X (?)` | Registered, exported, imported? Class not interface? Normal import? |
| `EADDRINUSE` | Another process on the port: `lsof -i :PORT` |
| `ERR_MODULE_NOT_FOUND` | `.js` extension, path typos, did the build run? |
| `ReferenceError: __dirname is not defined` | ESM: use `import.meta.dirname` |
| `Cannot find module '.../dist/main'` | Build failed or output path mismatch ([Project Structure](./04-project-structure.md)) |
| `ERR_REQUIRE_ASYNC_MODULE` | Jest on Node.js < 24.9 |
| Node.js version error from CLI | CLI generators need 22.22.3+/24.15+/26+ |
| Process exits with code 1 and no stack | Startup error swallowed by `abortOnError`: pass `{ abortOnError: false }` to see it |

### Runtime Errors

| Symptom | First check |
|---|---|
| `404` | Controller registered, path/prefix/version, route shadowing |
| `400` with a message array | DTO validation failing: read the messages |
| `500` | The server log has the stack: read it, then reproduce |
| `undefined` config values | `.env` loaded? Variable name? Process restarted? |
| Works in dev, fails in production build | Compare `start:dev` and `start:prod`: builder, `dist` layout, env |
| Changes not applied | Build error in the watch terminal, or a second old server still running |
| Intermittent failures | Shared state between requests or tests, race conditions, unhandled promises |

### Commands Cheat Sheet

```bash
nest info
curl -v http://localhost:3000/path
npm run start:debug
node --inspect-brk dist/main
node --enable-source-maps dist/main
lsof -i :3000
npx tsc --noEmit -p tsconfig.build.json
npx vitest run <file> -t "<test name>"
```

### Isolating a Problem

1. Reproduce with `curl`. Remove headers and body until it is minimal.
2. Check logs at `debug`.
3. Comment out modules or providers (bisect).
4. Write a failing test.
5. Build a minimal repro with `nest new`.
6. Compare `nest info` and configuration between environments.

---

## Performance

Debugging tools can distort performance:

- **Breakpoints freeze the event loop.** Timeouts, health checks, and heartbeats will fire while you are paused.
- **`--inspect` adds overhead** and `verbose`/`debug` logging adds I/O. Do not benchmark with them enabled.
- **Source maps** add startup and memory cost when `--enable-source-maps` is used. Acceptable in production for readable traces, but measure.
- For performance problems use profilers, not breakpoints:

```bash
node --cpu-prof dist/main       # then open the .cpuprofile in Chrome DevTools (Performance/Profiler)
node --heap-prof dist/main
```

See [Performance Fundamentals](../07-production/02-performance/01-performance-fundamentals.md) and [Memory and CPU Optimization](../07-production/02-performance/04-memory-and-cpu-optimization.md).

---

## Security

- **The inspector is remote code execution.** Bind it to `127.0.0.1`. Publish container ports only to `127.0.0.1` on the host. Never enable it on production services reachable from a network. If you must debug remotely, use an SSH tunnel.
- **Do not log secrets.** Redact `Authorization` headers, cookies, tokens, passwords, and full request bodies that may contain personal data.
- **Debug and verbose logs may contain sensitive data.** Treat them as sensitive, restrict access, and do not ship them to third parties casually.
- **Devtools and REPL are development tools.** Do not enable them in production builds. Guard them with environment checks.
- **Stack traces should not reach clients.** Production error responses should be generic. See [Exception Filters](../03-core-concepts/01-request-pipeline/08-exception-filters.md).
- **Heap and CPU profiles can contain data.** Heap snapshots include in-memory values (tokens, personal data). Handle them like secrets.
- **Never attach a debugger to a shared environment** where pausing the process would affect others.

---

## Production Considerations

Production debugging uses **observation**, not interactive debugging:

| Tool | Use |
|---|---|
| Structured logs with a correlation ID | Follow one request across services. See [Request Logging and Correlation ID](../07-production/03-observability/03-request-logging-and-correlation-id.md) |
| Metrics (error rate, latency percentiles, event loop delay) | Detect and size problems. See [Metrics](../07-production/03-observability/05-metrics.md) |
| Tracing | See where time goes across services. See [Tracing](../07-production/03-observability/06-tracing.md) |
| Error tracking | Group and alert on exceptions. See [Error Tracking and Alerting](../07-production/03-observability/07-error-tracking-and-alerting.md) |
| Health checks | Detect unhealthy instances. See [Health Checks](../07-production/03-observability/04-health-checks.md) |
| `@nestjs/observe` (new in v12) | Official SDK reporting requests, jobs, errors, and traces in terms of controllers and providers. Optional |
| Source maps in stack traces | `node --enable-source-maps` |
| Reproduce locally | Copy the failing request, use a production-like environment and data (sanitized) |

Guidelines:

- Make production logs searchable and structured. Include request IDs.
- Set log levels by environment variable so you can raise verbosity on one instance temporarily.
- Capture enough context in error logs to reproduce (inputs minus secrets, versions, correlation IDs).
- Prefer reproducing in staging over attaching to production.
- Keep crash policy explicit: log, flush, exit, and let the supervisor restart.

---

## Best Practices

### Recommended

```typescript
@Injectable()
export class TasksService {
  private readonly logger = new Logger(TasksService.name);

  async findOne(id: string) {
    this.logger.debug(`findOne id=${id}`);
    const task = await this.repo.findById(id);
    if (!task) {
      this.logger.warn(`task not found id=${id}`);
      throw new NotFoundException(`Task ${id} not found`);
    }
    return task;
  }
}
```

```jsonc
// .vscode/launch.json
{ "type": "node", "request": "attach", "port": 9229, "restart": true, "sourceMaps": true }
```

### Avoid

```typescript
console.log('here');                       // no context, no level, left in code
console.log(req.headers);                  // logs Authorization and cookies
catch (e) { }                              // swallowing errors hides root causes
```

Why: context-aware leveled logs can be filtered and turned off, logging headers leaks credentials, and empty `catch` blocks erase the evidence you need.

Additional guidance:

- Write a failing test before you start stepping through code.
- Use logpoints instead of temporary `console.log` edits.
- Remove debugging code before committing.
- Throw specific exceptions (`NotFoundException`) rather than generic errors, so logs and responses are meaningful.
- Keep a short personal checklist for the top error families (registration, exports, class-vs-interface, extensions, port, Node.js version).

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `nest start --debug [hostport]` | Available | Available |
| Test runner for debugging | Vitest (ESM) or Jest (CommonJS) | Jest |
| `ConsoleLogger` structured params | On by default (`structuredParams: false` to disable; `flattenParams` option) | Extra objects emitted as separate records |
| Route conflict diagnostics | Opt-in `routeConflictPolicy` / `routeResolutionStrategy` | Not available |
| `HttpException` `errorCode` option | Available, serialized into the response body | Not available |
| `@Optional()` inheritance | **Not inherited** by subclasses (explicit constructor needed), failing with `UnknownDependenciesException` | Inherited |
| Lifecycle hook order | By component hierarchy level, can change relative order | Previous ordering |
| `@nestjs/observe` | New official observability SDK, opt-in | Not available |
| Express graceful shutdown | Adapter drains in-flight requests on shutdown | Not drained |
| ESM errors (`ERR_MODULE_NOT_FOUND`, no `__dirname`) | Apply to ESM projects | Not applicable to CommonJS |
| Jest on ESM-only Nest packages | Needs Node.js 24.9+ | n/a |

Two of those changes are classic debugging surprises after upgrading from v11: a subclass that used to resolve a missing `@Optional()` dependency as `undefined` now throws, and lifecycle hooks may run in a different order. `nest upgrade` prints notes about both.

*Verify tool-specific flags (Vitest/Jest inspector options, Devtools registration, REPL entry setup, VS Code configuration keys) against their current documentation. They change faster than the framework.*

---

## Real-World Use Cases

- **Day-to-day development:** logs plus a debugger attached to `start:debug` to step through a handler.
- **Onboarding:** learning the error families so new developers fix wiring errors without asking.
- **CI failures:** reproduce locally with the CI Node.js version and a clean install, then isolate with a single test file.
- **Post-upgrade regressions:** `nest upgrade` report plus checks for `@Optional()` and lifecycle ordering.
- **Production incidents:** correlation IDs and structured logs to trace one failing request, metrics to see scope, a failing test to lock the fix.
- **Container debugging:** a localhost-bound inspector mapping for hard-to-reproduce environment issues.

---

## Interview Questions

### Beginner

1. How do you start a Nest app in debug mode?
   - `nest start --debug --watch` (the `start:debug` script), then attach a debugger to port 9229.
2. What does `Nest can't resolve dependencies of X (?)` mean?
   - Nest could not provide a constructor parameter. The message names the class, the parameter index, and the module context.
3. How do you change Nest's log level?
   - Pass a `logger` array of levels to `NestFactory.create()`.
4. What does `EADDRINUSE` mean?
   - The port is already in use by another process.

### Intermediate

1. List the usual causes of an unresolved dependency.
   - Not in `providers`, not exported or imported across modules, missing `@Injectable()`, interface used as the type, `import type` on a class, circular import, or missing decorator metadata flags.
2. Why do you need source maps to debug TypeScript?
   - The running code is compiled JavaScript. Source maps map positions back to your `.ts` files.
3. How would you debug a failing unit test?
   - Run just that test, set a breakpoint, and run it in a JavaScript Debug Terminal (or with the runner's inspector flags).
4. Why does the debugger disconnect in watch mode, and how do you handle it?
   - Each rebuild restarts the process. Use `"restart": true` in VS Code or the JavaScript Debug Terminal.
5. What are the ESM-specific startup errors in a NestJS 12 project?
   - `ERR_MODULE_NOT_FOUND` from missing `.js` extensions and `__dirname is not defined` (use `import.meta.dirname`).

### Advanced

1. Why is exposing the inspector port dangerous, and how do you debug in Docker safely?
   - It allows arbitrary code execution. Bind to localhost, publish ports only to `127.0.0.1`, never in production, and use an SSH tunnel for remote cases.
2. How does a breakpoint affect a running server?
   - It pauses the single JavaScript thread, so the event loop stalls. Other requests, timers, and health checks wait or time out.
3. After upgrading to NestJS 12 a subclass fails to resolve a dependency that used to be `undefined`. Why?
   - `@Optional()` markers are no longer inherited, so the subclass needs its own constructor redeclaring them.
4. How would you find route shadowing problems?
   - Enable `routeConflictPolicy: { shadow: 'warn' }` (and optionally `routeResolutionStrategy: 'specificity'`) in development, and review route declaration order.
5. Outline how you would debug a production incident without a debugger.
   - Use correlation IDs and structured logs to isolate the request, metrics and traces for scope and timing, error tracking for grouping, reproduce in staging with sanitized data, then lock the fix with a test.
6. How can swallowed promise errors be detected?
   - Typed lint rules for floating promises, a process-level `unhandledRejection` handler that logs, and code review of `async` usage in event handlers.

---

## Quick Reference

```text
Debug run           npm run start:debug   (nest start --debug --watch)  → port 9229
Attach              VS Code: attach, port 9229, restart: true, sourceMaps: true
                    or: JavaScript Debug Terminal → npm run start:dev
Pause at start      node --inspect-brk dist/main
Logs                NestFactory.create(AppModule, { logger: [...] }) · new Logger(Name.name)
Stack traces        node --enable-source-maps dist/main
Test one            npx vitest run <file> -t "<name>"
Module graph        REPL: debug() · Nest Devtools (dev only)
Versions            nest info

Errors
  can't resolve dependencies → registered? exported/imported? class not interface? normal import? flags?
  EADDRINUSE                 → lsof -i :PORT  (or change PORT)
  ERR_MODULE_NOT_FOUND       → add .js extension (ESM)
  __dirname not defined      → import.meta.dirname (ESM)
  404                        → controller registered? prefix/version? route shadowing?
  ERR_REQUIRE_ASYNC_MODULE   → Jest needs Node.js 24.9+ (or use Vitest)

Never               expose 9229 publicly · log secrets · leave verbose logs in production
```

---

## Key Takeaways

- Start with the full error message. Nest tells you the class, the parameter index, and the module.
- Most beginner errors fall into a few families: dependency resolution, module wiring, port conflicts, ESM imports, and Node.js versions.
- `nest start --debug --watch` plus a VS Code attach (or the JavaScript Debug Terminal) gives you breakpoints in TypeScript, as long as source maps are on.
- Logs, tests, and the module graph (REPL, Devtools) solve most problems faster than stepping through code.
- A breakpoint freezes the event loop. The inspector port is a remote-code-execution risk. Keep both away from shared and production systems.
- In production, debug by observation: structured logs with correlation IDs, metrics, traces, and error tracking.
- NestJS 12 behavior changes (no inherited `@Optional()`, lifecycle order, structured log params, opt-in route conflict diagnostics) are worth checking first after an upgrade.

---

## Related Topics

```text
05 Development Environment
      ↓
[06 Debugging]
      ↓
02-fundamentals (Modules, Providers, Dependency Injection)
      ↓
07-production/03-observability (production debugging)
```

- [Getting Started Overview](./README.md)
- [First Project](./03-first-project.md)
- [Project Structure](./04-project-structure.md)
- [Dependency Injection](../02-fundamentals/05-dependency-injection.md)
- [Circular Dependencies](../03-core-concepts/04-modules-and-di/04-circular-dependencies.md)
- [Logging Fundamentals](../07-production/03-observability/01-logging-fundamentals.md)
- [Performance Fundamentals](../07-production/02-performance/01-performance-fundamentals.md)
- [Troubleshooting Common Errors](../09-best-practices/03-troubleshooting-common-errors.md)
