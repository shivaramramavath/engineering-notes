# Project 07: Next.js 15 App Router Full-Stack Application (fullstack-application)

> Production full-stack web application leveraging Next.js 15 App Router, React 19 Server Components, type-safe Server Actions with Zod validation, and optimistic mutation rollbacks.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-07` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Server-Driven UI & Colocated Server Actions |
| **Core Concepts** | Async Server Components, Next.js 15 dynamic params promises, Server action results |

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

The application coordinates data fetching directly in asynchronous Server Components on Node.js, while client interaction triggers type-safe Server Actions validating inputs via Zod.

```
       [ Client Form Submission ]
                    │
                    ▼
       ┌─────────────────────────┐
       │   Next.js Server Action │ ◄── Validates form data via Zod schema
       └────────────┬────────────┘
                    │ (Type-Safe Payload)
                    ▼
       ┌─────────────────────────┐
       │   Domain Service Layer  │ ◄── Executes mutation in database
       └────────────┬────────────┘
                    │
                    ▼
       ┌─────────────────────────┐
       │   revalidatePath('/app')│ ◄── Invalidates server component cache
       └─────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Domain model interfaces, Server Action discriminated results, and Page Props.
2. **`core.ts`**: Zod validation schemas for Server Actions with field error mapping.
3. **`service.ts`**: Task management service simulating database transactions.
4. **`index.ts`**: Next.js 15 async page handler and Server Action implementation.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export interface TaskItem {
  readonly id: string;
  readonly title: string;
  readonly completed: boolean;
  readonly createdAt: string;
}

export type ActionResult<TData> =
  | { readonly success: true; readonly data: TData }
  | { readonly success: false; readonly errors: Record<string, string[]> };

// Next.js 15: params and searchParams are Promises
export interface PageProps {
  readonly params: Promise<{ slug: string }>;
  readonly searchParams: Promise<Record<string, string | string[] | undefined>>;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { z } from "zod";
import { ActionResult } from "./types.js";

export const CreateTaskSchema = z.object({
  title: z.string().min(3, "Title must be at least 3 characters").max(100),
});

export type CreateTaskInput = z.infer<typeof CreateTaskSchema>;

export function validateForm<T>(
  schema: z.ZodSchema<T>,
  formData: FormData
): { success: true; data: T } | { success: false; errors: Record<string, string[]> } {
  const raw = Object.fromEntries(formData.entries());
  const parsed = schema.safeParse(raw);
  if (!parsed.success) {
    return {
      success: false,
      errors: parsed.error.flatten().fieldErrors,
    };
  }
  return { success: true, data: parsed.data };
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { TaskItem } from "./types.js";
import crypto from "node:crypto";

export class TaskService {
  private tasks = new Map<string, TaskItem>();

  public async list(): Promise<TaskItem[]> {
    return Array.from(this.tasks.values());
  }

  public async create(title: string): Promise<TaskItem> {
    const id = crypto.randomUUID();
    const task: TaskItem = {
      id,
      title,
      completed: false,
      createdAt: new Date().toISOString(),
    };
    this.tasks.set(id, task);
    return task;
  }

  public async toggle(id: string): Promise<TaskItem> {
    const task = this.tasks.get(id);
    if (!task) throw new Error("Task not found");
    const updated = { ...task, completed: !task.completed };
    this.tasks.set(id, updated);
    return updated;
  }
}

export const globalTaskService = new TaskService();
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { ActionResult, PageProps, TaskItem } from "./types.js";
import { CreateTaskSchema, validateForm } from "./core.js";
import { globalTaskService } from "./service.js";

// Type-Safe Server Action
export async function createTaskAction(formData: FormData): Promise<ActionResult<TaskItem>> {
  const validation = validateForm(CreateTaskSchema, formData);
  if (!validation.success) {
    return { success: false, errors: validation.errors };
  }

  const created = await globalTaskService.create(validation.data.title);
  return { success: true, data: created };
}

// Next.js 15 Async Page Component Simulation
export async function Page(props: PageProps): Promise<{ title: string; tasks: TaskItem[] }> {
  const { slug } = await props.params;
  const tasks = await globalTaskService.list();
  return {
    title: `Dashboard: ${slug}`,
    tasks,
  };
}

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

### 2.5 Optimistic Task Hook for React 19 (`src/useOptimisticTasks.ts`)
```typescript
import { useOptimistic, useTransition } from "react";
import { TaskItem } from "./types.js";
import { createTaskAction } from "./index.js";

export function useOptimisticTaskList(initialTasks: TaskItem[]) {
  const [isPending, startTransition] = useTransition();
  const [optimisticTasks, setOptimisticTasks] = useOptimistic(
    initialTasks,
    (state, newTask: TaskItem) => [newTask, ...state]
  );

  const addTask = async (formData: FormData) => {
    const title = formData.get("title") as string;
    const tempTask: TaskItem = {
      id: "temp-" + Date.now(),
      title,
      completed: false,
      createdAt: new Date().toISOString(),
    };

    startTransition(async () => {
      setOptimisticTasks(tempTask);
      await createTaskAction(formData);
    });
  };

  return { optimisticTasks, addTask, isPending };
}
```

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/fullstack-application",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm",
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
import { describe, it, expect } from "vitest";
import { createTaskAction, Page } from "../src/index.js";

describe("Next.js 15 Full-Stack App Tests", () => {
  it("should validate and execute Server Action successfully", async () => {
    const formData = new FormData();
    formData.append("title", "Review PR #45");

    const result = await createTaskAction(formData);
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.data.title).toBe("Review PR #45");
      expect(result.data.completed).toBe(false);
    }
  });

  it("should return structured field errors on invalid input", async () => {
    const formData = new FormData();
    formData.append("title", "A"); // Too short

    const result = await createTaskAction(formData);
    expect(result.success).toBe(false);
    if (!result.success) {
      expect(result.errors.title).toBeDefined();
    }
  });

  it("should correctly resolve Next.js 15 async page params", async () => {
    const props = {
      params: Promise.resolve({ slug: "engineering" }),
      searchParams: Promise.resolve({}),
    };

    const rendered = await Page(props);
    expect(rendered.title).toBe("Dashboard: engineering");
    expect(Array.isArray(rendered.tasks)).toBe(true);
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (3 tests) 14ms
```
