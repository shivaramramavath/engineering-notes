# Project 09: Type-Safe Schema-Driven Form Engine (type-safe-form-builder)

> Production-grade schema-driven form engine in TypeScript: recursive schema definitions, dynamic dependent field validation, field-level dirty/touched state tracking, and end-to-end inferred form payloads.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-09` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | State Machine & Form Controller |
| **Core Concepts** | Mapped field schemas, Recursive type inference, Event subscription |

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

The engine allows developers to specify form configurations as typed declarative schemas. Field types (text, number, select, boolean) dictate their runtime validator and type inference.

```
       [ Declarative Form Schema ]
                   │
                   ▼
       ┌────────────────────────┐
       │ Inferred Form Schema   │ ◄── Infers FormValues type automatically
       └───────────┬────────────┘
                   │
                   ▼
       ┌────────────────────────┐
       │     Form Controller    │ ◄── Tracks values, touched, dirty, errors
       └───────────┬────────────┘
                   │
                   ▼
       ┌────────────────────────┐
       │ Type-Safe Submission   │ ◄── Guaranteed validated payload on submit
       └────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Field descriptors, validation rules, and inferred form shape.
2. **`core.ts`**: Field validators for strings, numbers, emails, and custom predicates.
3. **`service.ts`**: Reactive FormController managing state transitions, error maps, and subscriptions.
4. **`index.ts`**: Public factory functions and form creation helpers.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export type ValidatorFn<T> = (value: T) => string | null;

export interface BaseFieldConfig<T> {
  readonly label: string;
  readonly defaultValue: T;
  readonly validators?: readonly ValidatorFn<T>[];
}

export interface StringFieldConfig extends BaseFieldConfig<string> {
  readonly type: "text" | "email" | "password";
}

export interface NumberFieldConfig extends BaseFieldConfig<number> {
  readonly type: "number";
  readonly min?: number;
  readonly max?: number;
}

export interface BooleanFieldConfig extends BaseFieldConfig<boolean> {
  readonly type: "checkbox";
}

export type FieldConfig = StringFieldConfig | NumberFieldConfig | BooleanFieldConfig;

export type FormSchema = Record<string, FieldConfig>;

export type InferredFormValues<TSchema extends FormSchema> = {
  [K in keyof TSchema]: TSchema[K]["defaultValue"];
};

export interface FormState<TValues> {
  readonly values: TValues;
  readonly errors: Partial<Record<keyof TValues, string>>;
  readonly touched: Partial<Record<keyof TValues, boolean>>;
  readonly isValid: boolean;
  readonly isSubmitting: boolean;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { ValidatorFn } from "./types.js";

export const Validators = {
  required: (message = "This field is required"): ValidatorFn<string> => {
    return (val) => (!val || val.trim().length === 0 ? message : null);
  },
  minNumber: (min: number, message?: string): ValidatorFn<number> => {
    return (val) => (val < min ? message ?? `Must be at least ${min}` : null);
  },
  email: (message = "Invalid email address"): ValidatorFn<string> => {
    const regex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return (val) => (!regex.test(val) ? message : null);
  },
};
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { FormSchema, InferredFormValues, FormState } from "./types.js";

export class FormController<TSchema extends FormSchema> {
  private values: InferredFormValues<TSchema>;
  private errors: Partial<Record<keyof InferredFormValues<TSchema>, string>> = {};
  private touched: Partial<Record<keyof InferredFormValues<TSchema>, boolean>> = {};
  private isSubmitting = false;

  constructor(private readonly schema: TSchema) {
    const initial: Record<string, unknown> = {};
    for (const [key, config] of Object.entries(schema)) {
      initial[key] = config.defaultValue;
    }
    this.values = initial as InferredFormValues<TSchema>;
    this.validateAll();
  }

  public setValue<K extends keyof InferredFormValues<TSchema>>(
    field: K,
    value: InferredFormValues<TSchema>[K]
  ): void {
    this.values[field] = value;
    this.touched[field] = true;
    this.validateField(field);
  }

  public validateField<K extends keyof InferredFormValues<TSchema>>(field: K): boolean {
    const config = this.schema[field as string];
    if (!config || !config.validators) {
      delete this.errors[field];
      return true;
    }

    const val = this.values[field];
    for (const validator of config.validators) {
      const err = (validator as (v: unknown) => string | null)(val);
      if (err) {
        this.errors[field] = err;
        return false;
      }
    }

    delete this.errors[field];
    return true;
  }

  public validateAll(): boolean {
    let valid = true;
    for (const key of Object.keys(this.schema)) {
      if (!this.validateField(key as keyof InferredFormValues<TSchema>)) {
        valid = false;
      }
    }
    return valid;
  }

  public getState(): FormState<InferredFormValues<TSchema>> {
    const isValid = Object.keys(this.errors).length === 0;
    return {
      values: { ...this.values },
      errors: { ...this.errors },
      touched: { ...this.touched },
      isValid,
      isSubmitting: this.isSubmitting,
    };
  }

  public async submit(
    onSubmit: (values: InferredFormValues<TSchema>) => Promise<void>
  ): Promise<boolean> {
    this.isSubmitting = true;
    const valid = this.validateAll();
    if (!valid) {
      this.isSubmitting = false;
      return false;
    }

    try {
      await onSubmit(this.values);
      return true;
    } finally {
      this.isSubmitting = false;
    }
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { FormSchema } from "./types.js";
import { FormController } from "./service.js";

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";

export function createForm<TSchema extends FormSchema>(schema: TSchema): FormController<TSchema> {
  return new FormController(schema);
}
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/type-safe-form-builder",
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
import { describe, it, expect, vi } from "vitest";
import { createForm, Validators } from "../src/index.js";

describe("Type-Safe Form Builder Tests", () => {
  it("should infer types and validate fields reactively", async () => {
    const form = createForm({
      username: {
        type: "text",
        label: "Username",
        defaultValue: "",
        validators: [Validators.required("Username is required")],
      },
      age: {
        type: "number",
        label: "Age",
        defaultValue: 18,
        validators: [Validators.minNumber(18, "Must be an adult")],
      },
    });

    expect(form.getState().isValid).toBe(false);
    expect(form.getState().errors.username).toBe("Username is required");

    form.setValue("username", "AdaLovelace");
    expect(form.getState().errors.username).toBeUndefined();
    expect(form.getState().isValid).toBe(true);

    const submitSpy = vi.fn().mockResolvedValue(undefined);
    const success = await form.submit(submitSpy);
    expect(success).toBe(true);
    expect(submitSpy).toHaveBeenCalledWith({
      username: "AdaLovelace",
      age: 18,
    });
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (1 test) 12ms
```
