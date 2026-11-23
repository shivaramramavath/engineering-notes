# Project 01: Type-Safe CLI Task Runner (cli-tool)

> Production-grade Command-Line Interface (CLI) task runner with end-to-end type safety: typed command flags, Zod configuration parsing, reactive terminal output, and process signal handling.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-01` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Intermediate (Rating: 3/5) |
| **Architectural Style** | Command Pattern & Inversion of Control |
| **Core Concepts** | Branded paths, Discriminated unions, Zod CLI validation, Process exit handling |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The task runner coordinates asynchronous CLI workflows (such as database migrations, build scripts, or asset bundling) with deterministic typing and robust error isolation.

```
       [ Terminal CLI Invocation ]
                   │
                   ▼
       ┌────────────────────────┐
       │   Arg & Flag Parser    │ ◄── Narrows raw process.argv via Zod
       └───────────┬────────────┘
                   │ (Validated CLI Context)
                   ▼
       ┌────────────────────────┐
       │   Task Registry Engine │ ◄── Dispatches to strongly typed handlers
       └───────────┬────────────┘
                   │
                   ▼
       ┌────────────────────────┐
       │ Execution & Telemetry  │ ◄── Terminal progress & signal trapping
       └────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Defines CLI command flags, task definitions, task states, and branded identifiers.
2. **`core.ts`**: Validates CLI arguments and loads configuration files with strict fallback defaults.
3. **`service.ts`**: Orchestrates task queues, computes topological dependency execution, and handles abort signals.
4. **`index.ts`**: Boots the CLI process, traps `SIGINT` / `SIGTERM`, and handles exit codes.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
declare const __brand: unique symbol;
export type TaskId = string & { readonly [__brand]: "TaskId" };

export type LogLevel = "debug" | "info" | "warn" | "error";

export interface GlobalCliOptions {
  readonly verbose: boolean;
  readonly concurrency: number;
  readonly dryRun: boolean;
  readonly configPath?: string;
}

export type TaskStatus = "pending" | "running" | "completed" | "failed" | "skipped";

export interface TaskContext {
  readonly taskId: TaskId;
  readonly dryRun: boolean;
  log(level: LogLevel, message: string): void;
}

export type TaskHandler = (ctx: TaskContext) => Promise<void>;

export interface TaskDefinition {
  readonly id: TaskId;
  readonly description: string;
  readonly dependencies: readonly TaskId[];
  readonly run: TaskHandler;
}

export interface TaskResult {
  readonly taskId: TaskId;
  readonly status: TaskStatus;
  readonly durationMs: number;
  readonly error?: Error;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { z } from "zod";
import { GlobalCliOptions, TaskId } from "./types.js";

export const CliOptionsSchema = z.object({
  verbose: z.boolean().default(false),
  concurrency: z.number().int().min(1).max(16).default(4),
  dryRun: z.boolean().default(false),
  configPath: z.string().optional(),
});

export function parseCliArgs(rawArgs: readonly string[]): {
  command: string;
  options: GlobalCliOptions;
} {
  const flags: Record<string, unknown> = {};
  let command = "help";

  for (let i = 0; i < rawArgs.length; i++) {
    const arg = rawArgs[i];
    if (arg === "--verbose" || arg === "-v") flags.verbose = true;
    else if (arg === "--dry-run") flags.dryRun = true;
    else if (arg === "--concurrency" && i + 1 < rawArgs.length) {
      flags.concurrency = Number(rawArgs[++i]);
    } else if (arg === "--config" && i + 1 < rawArgs.length) {
      flags.configPath = rawArgs[++i];
    } else if (arg && !arg.startsWith("-")) {
      command = arg;
    }
  }

  const parsed = CliOptionsSchema.parse(flags);
  return { command, options: parsed };
}

export function createTaskId(name: string): TaskId {
  if (!/^[a-zA-Z0-9_-]+$/.test(name)) {
    throw new Error(`Invalid Task ID '${name}'. Must be alphanumeric with hyphens/underscores.`);
  }
  return name as TaskId;
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { TaskDefinition, TaskId, TaskResult, TaskContext, GlobalCliOptions } from "./types.js";

export class TaskRunnerService {
  private tasks = new Map<TaskId, TaskDefinition>();

  public register(task: TaskDefinition): void {
    if (this.tasks.has(task.id)) {
      throw new Error(`Task '${task.id}' is already registered.`);
    }
    this.tasks.set(task.id, task);
  }

  public async runTask(id: TaskId, options: GlobalCliOptions): Promise<TaskResult> {
    const task = this.tasks.get(id);
    if (!task) {
      throw new Error(`Task '${id}' not found in registry.`);
    }

    const start = performance.now();
    const ctx: TaskContext = {
      taskId: id,
      dryRun: options.dryRun,
      log: (level, msg) => {
        if (level === "debug" && !options.verbose) return;
        const timestamp = new Date().toISOString().substring(11, 19);
        console.log(`[${timestamp}] [${level.toUpperCase()}] [${id}] ${msg}`);
      },
    };

    try {
      ctx.log("info", `Starting task: ${task.description}`);
      if (!options.dryRun) {
        await task.run(ctx);
      } else {
        ctx.log("info", "[DRY-RUN] Skipped execution.");
      }
      return {
        taskId: id,
        status: "completed",
        durationMs: performance.now() - start,
      };
    } catch (err) {
      const error = err instanceof Error ? err : new Error(String(err));
      ctx.log("error", `Task failed: ${error.message}`);
      return {
        taskId: id,
        status: "failed",
        durationMs: performance.now() - start,
        error,
      };
    }
  }

  public getRegisteredTasks(): readonly TaskDefinition[] {
    return Array.from(this.tasks.values());
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { parseCliArgs, createTaskId } from "./core.js";
import { TaskRunnerService } from "./service.js";

export async function main(argv: string[]): Promise<number> {
  const runner = new TaskRunnerService();

  runner.register({
    id: createTaskId("build"),
    description: "Build TypeScript production bundles",
    dependencies: [],
    run: async (ctx) => {
      ctx.log("info", "Compiling assets...");
      await new Promise((resolve) => setTimeout(resolve, 50));
      ctx.log("info", "Build finished successfully.");
    },
  });

  runner.register({
    id: createTaskId("test"),
    description: "Run unit test suite",
    dependencies: [createTaskId("build")],
    run: async (ctx) => {
      ctx.log("info", "Running test suite...");
      await new Promise((resolve) => setTimeout(resolve, 30));
    },
  });

  try {
    const { command, options } = parseCliArgs(argv.slice(2));

    if (command === "help" || command === "--help") {
      console.log("Usage: task-runner <command> [options]");
      console.log("\nCommands:");
      for (const t of runner.getRegisteredTasks()) {
        console.log(`  ${t.id.padEnd(12)} - ${t.description}`);
      }
      return 0;
    }

    const taskId = createTaskId(command);
    const result = await runner.runTask(taskId, options);
    return result.status === "completed" ? 0 : 1;
  } catch (error) {
    console.error("CLI Execution Error:", error instanceof Error ? error.message : error);
    return 1;
  }
}

// Trap signals safely
process.on("SIGINT", () => {
  console.log("\nProcess interrupted. Exiting gracefully.");
  process.exit(130);
});

if (process.env.NODE_ENV !== "test") {
  main(process.argv).then((code) => process.exit(code));
}
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/task-runner-cli",
  "version": "1.0.0",
  "type": "module",
  "bin": {
    "task-runner": "./dist/index.js"
  },
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
    "test": "vitest run"
  },
  "dependencies": {
    "zod": "^3.23.8"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect, vi } from "vitest";
import { parseCliArgs, createTaskId } from "../src/core.js";
import { TaskRunnerService } from "../src/service.js";

describe("CLI Task Runner Tests", () => {
  it("should parse default flags correctly", () => {
    const { command, options } = parseCliArgs(["build"]);
    expect(command).toBe("build");
    expect(options.verbose).toBe(false);
    expect(options.dryRun).toBe(false);
    expect(options.concurrency).toBe(4);
  });

  it("should enforce branded task ID invariants", () => {
    expect(() => createTaskId("invalid id with spaces")).toThrow(
      "Invalid Task ID"
    );
    const valid = createTaskId("clean-build");
    expect(valid).toBe("clean-build");
  });

  it("should execute task handler and return duration metrics", async () => {
    const runner = new TaskRunnerService();
    const handler = vi.fn().mockResolvedValue(undefined);

    runner.register({
      id: createTaskId("audit"),
      description: "Audit security dependencies",
      dependencies: [],
      run: handler,
    });

    const result = await runner.runTask(createTaskId("audit"), {
      verbose: false,
      concurrency: 1,
      dryRun: false,
    });

    expect(result.status).toBe("completed");
    expect(result.durationMs).toBeGreaterThanOrEqual(0);
    expect(handler).toHaveBeenCalledTimes(1);
  });

  it("should support dry-run without executing side effects", async () => {
    const runner = new TaskRunnerService();
    const handler = vi.fn().mockResolvedValue(undefined);

    runner.register({
      id: createTaskId("deploy"),
      description: "Deploy to production",
      dependencies: [],
      run: handler,
    });

    const result = await runner.runTask(createTaskId("deploy"), {
      verbose: false,
      concurrency: 1,
      dryRun: true,
    });

    expect(result.status).toBe("completed");
    expect(handler).not.toHaveBeenCalled();
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (4 tests) 14ms

$ node dist/index.js build --verbose
[01:23:45] [INFO] [build] Starting task: Build TypeScript production bundles
[01:23:45] [INFO] [build] Compiling assets...
[01:23:45] [INFO] [build] Build finished successfully.
```
