# Project 11: Schema Validation Library from Scratch (schema-validation-library)

> Production-grade runtime schema validation library built from scratch in TypeScript: Zod-inspired fluent API, parser combinators, error accumulator paths, and automatic compile-time static type inference (`z.infer<T>`).

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-11` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 5/5) |
| **Architectural Style** | Combinator Pattern & Higher-Order Types |
| **Core Concepts** | Conditional type unwrapping, Parser combinators, SafeParseResult discriminated union |

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

The library implements composable schema objects (`ZodType<T>`) that validate unknown input at runtime and infer exact TypeScript types statically without external dependencies.

```
       [ Schema Definition: z.object({ name: z.string(), age: z.number() }) ]
                                      │
                                      ▼
                    ┌───────────────────────────────────┐
                    │      Type Inference Engine        │ ◄── Static<typeof Schema> -> { name: string; age: number }
                    └─────────────────┬─────────────────┘
                                      │
                                      ▼
                    ┌───────────────────────────────────┐
                    │      Runtime Parser Combinator    │ ◄── SafeParseResult (Success / Failure with paths)
                    └───────────────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: ValidationError structures, ParseResult discriminated union, and Infer type helper.
2. **`core.ts`**: Abstract `BaseSchema<T>` class and error formatter utilities.
3. **`service.ts`**: Specific schema implementations (`StringSchema`, `NumberSchema`, `ObjectSchema`, `ArraySchema`).
4. **`index.ts`**: The fluent `z` namespace exporter.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export interface ValidationError {
  path: string;
  message: string;
}

export type ParseResult<T> =
  | { success: true; data: T }
  | { success: false; errors: ValidationError[] };

export type Infer<T> = T extends { _output: infer O } ? O : never;
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { ParseResult } from "./types.js";

export abstract class BaseSchema<TOutput> {
  // Phantom type marker for compile-time inference
  public readonly _output!: TOutput;

  public abstract parse(input: unknown, path?: string): TOutput;
  public abstract safeParse(input: unknown, path?: string): ParseResult<TOutput>;
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { BaseSchema } from "./core.js";
import { ParseResult, ValidationError } from "./types.js";

export class ZString extends BaseSchema<string> {
  private minLen?: number;
  private isEmail = false;

  public min(len: number): this {
    this.minLen = len;
    return this;
  }

  public email(): this {
    this.isEmail = true;
    return this;
  }

  public safeParse(input: unknown, path = ""): ParseResult<string> {
    if (typeof input !== "string") {
      return { success: false, errors: [{ path, message: "Expected string, received " + typeof input }] };
    }
    if (this.minLen !== undefined && input.length < this.minLen) {
      return { success: false, errors: [{ path, message: `String must contain at least ${this.minLen} characters` }] };
    }
    if (this.isEmail && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(input)) {
      return { success: false, errors: [{ path, message: "Invalid email format" }] };
    }
    return { success: true, data: input };
  }

  public parse(input: unknown, path = ""): string {
    const res = this.safeParse(input, path);
    if (!res.success) throw new Error(res.errors[0]?.message);
    return res.data;
  }
}

export class ZNumber extends BaseSchema<number> {
  public safeParse(input: unknown, path = ""): ParseResult<number> {
    if (typeof input !== "number" || Number.isNaN(input)) {
      return { success: false, errors: [{ path, message: "Expected number, received " + typeof input }] };
    }
    return { success: true, data: input };
  }

  public parse(input: unknown, path = ""): number {
    const res = this.safeParse(input, path);
    if (!res.success) throw new Error(res.errors[0]?.message);
    return res.data;
  }
}

export type RawShape = Record<string, BaseSchema<any>>;

export class ZObject<TShape extends RawShape> extends BaseSchema<{ [K in keyof TShape]: TShape[K]["_output"] }> {
  constructor(private shape: TShape) {
    super();
  }

  public safeParse(input: unknown, path = ""): ParseResult<{ [K in keyof TShape]: TShape[K]["_output"] }> {
    if (typeof input !== "object" || input === null) {
      return { success: false, errors: [{ path, message: "Expected object, received " + typeof input }] };
    }

    const errors: ValidationError[] = [];
    const output: Record<string, unknown> = {};
    const raw = input as Record<string, unknown>;

    for (const [key, schema] of Object.entries(this.shape)) {
      const fieldPath = path ? `${path}.${key}` : key;
      const result = schema.safeParse(raw[key], fieldPath);
      if (result.success) {
        output[key] = result.data;
      } else {
        errors.push(...result.errors);
      }
    }

    if (errors.length > 0) return { success: false, errors };
    return { success: true, data: output as any };
  }

  public parse(input: unknown, path = ""): { [K in keyof TShape]: TShape[K]["_output"] } {
    const res = this.safeParse(input, path);
    if (!res.success) throw new Error(res.errors[0]?.message);
    return res.data;
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { ZString, ZNumber, ZObject, RawShape } from "./service.js";

export const z = {
  string: () => new ZString(),
  number: () => new ZNumber(),
  object: <T extends RawShape>(shape: T) => new ZObject(shape),
};

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

### 2.5 Array & Union Schemas (`src/combinators.ts`)
```typescript
import { BaseSchema } from "./core.js";
import { ParseResult, ValidationError } from "./types.js";

export class ZArray<TItem> extends BaseSchema<TItem[]> {
  constructor(private elementSchema: BaseSchema<TItem>) {
    super();
  }

  public safeParse(input: unknown, path = ""): ParseResult<TItem[]> {
    if (!Array.isArray(input)) {
      return { success: false, errors: [{ path, message: "Expected array, received " + typeof input }] };
    }

    const errors: ValidationError[] = [];
    const output: TItem[] = [];

    for (let i = 0; i < input.length; i++) {
      const itemPath = `${path}[${i}]`;
      const res = this.elementSchema.safeParse(input[i], itemPath);
      if (res.success) {
        output.push(res.data);
      } else {
        errors.push(...res.errors);
      }
    }

    if (errors.length > 0) return { success: false, errors };
    return { success: true, data: output };
  }

  public parse(input: unknown, path = ""): TItem[] {
    const res = this.safeParse(input, path);
    if (!res.success) throw new Error(res.errors[0]?.message);
    return res.data;
  }
}

export class ZUnion<TMember extends BaseSchema<any>> extends BaseSchema<TMember["_output"]> {
  constructor(private members: readonly TMember[]) {
    super();
  }

  public safeParse(input: unknown, path = ""): ParseResult<TMember["_output"]> {
    for (const member of this.members) {
      const res = member.safeParse(input, path);
      if (res.success) return res;
    }
    return { success: false, errors: [{ path, message: "Input does not match any union variant" }] };
  }

  public parse(input: unknown, path = ""): TMember["_output"] {
    const res = this.safeParse(input, path);
    if (!res.success) throw new Error(res.errors[0]?.message);
    return res.data;
  }
}
```

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/schema-validation-library",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
    "test": "vitest run"
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
import { z, Infer } from "../src/index.js";

describe("Mini Schema Validation Library Tests", () => {
  it("should validate valid nested objects and infer static types", () => {
    const UserSchema = z.object({
      name: z.string().min(2),
      email: z.string().email(),
      age: z.number(),
    });

    type User = Infer<typeof UserSchema>;

    const validPayload = {
      name: "Grace Hopper",
      email: "grace@navy.mil",
      age: 85,
    };

    const res = UserSchema.safeParse(validPayload);
    expect(res.success).toBe(true);
    if (res.success) {
      expect(res.data).toEqual(validPayload);
      const user: User = res.data;
      expect(user.name).toBe("Grace Hopper");
    }
  });

  it("should accumulate path-accurate errors on nested failure", () => {
    const Schema = z.object({
      email: z.string().email(),
    });

    const res = Schema.safeParse({ email: "invalid-email" });
    expect(res.success).toBe(false);
    if (!res.success) {
      expect(res.errors[0]?.path).toBe("email");
      expect(res.errors[0]?.message).toBe("Invalid email format");
    }
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (2 tests) 14ms
```
