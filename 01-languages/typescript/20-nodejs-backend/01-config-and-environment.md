# Configuration and Environment Variable Typing in TypeScript

> Type-safe application configuration in Node.js: process.env typing via Declaration Merging, Zod schema validation (t3-env), and fail-fast startup invariants.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Intermediate |
| **Prerequisites** | [09-declaration-files/01-ambient-declarations.md](../09-declaration-files/01-ambient-declarations.md), [15-runtime-validation/02-zod.md](../15-runtime-validation/02-zod.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In native Node.js, `process.env` is typed by `@types/node` as `Dict<string>`:
```typescript
interface ProcessEnv extends Dict<string> {}
```
This loose typing introduces critical production vulnerabilities:
1. **Silent Undefined Values**: `process.env.DATABASE_URL` is typed as `string | undefined`. Calling `db.connect(process.env.DATABASE_URL)` compiles cleanly if strict null checks are loose, but crashes in production when the variable is missing.
2. **Type Confusion**: `process.env.PORT` is a `string` (`"3000"`), not a `number`. Passing it to `server.listen(port)` can produce subtle bugs.
3. **No Fail-Fast Guarantee**: If a developer forgets to configure `STRIPE_SECRET_KEY`, the server boots up normally, only crashing hours later when a customer attempts to purchase an item!

A type-safe configuration architecture enforces **Fail-Fast Initialization**: validating and parsing all environment variables on server startup using Zod or Envalid. If any variable is missing or malformed, the process logs an informative error and exits immediately before accepting traffic.

```
+-----------------------------------------------------------+
|               Type-Safe Environment Validation            |
|                                                           |
|   process.env (Raw Strings)                               |
|          |                                                |
|          v                                                |
|   Zod Schema Validation (Coercion, Min, URL checks)       |
|          |                                                |
|     +----+----+                                           |
|     |         |                                           |
|  (Invalid)  (Valid)                                      |
|     v         v                                           |
|  process.   export const env = { PORT: 3000, ... }        |
|  exit(1)    (Strongly typed singleton across the app)     |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Declaration Merging for `process.env`
You can augment Node's global `ProcessEnv` interface via TypeScript declaration merging:
```typescript
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      NODE_ENV: 'development' | 'production' | 'test';
      PORT: string;
      DATABASE_URL: string;
    }
  }
}
```
*Note*: While declaration merging provides autocompletion, it does **not** validate that those variables actually exist at runtime! Schema validation is mandatory for real runtime safety.

### 2. Runtime Schema Validation with Zod
```typescript
import { z } from 'zod';

const EnvSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']).default('development'),
  PORT: z.coerce.number().default(3000),
  DATABASE_URL: z.string().url(),
  API_KEY: z.string().min(1)
});

export const env = EnvSchema.parse(process.env);
export type Env = z.infer<typeof EnvSchema>;
```

---

## 3. How It Works Internally

1. `EnvSchema.parse(process.env)` iterates over defined keys.
2. `z.coerce.number()` transforms `"3000"` to number `3000`.
3. If validation succeeds, `env` is frozen and exported as a strongly typed singleton.
4. If validation fails, `ZodError` captures all missing or invalid variables in a single comprehensive error report, halting execution via `process.exit(1)`.

---

## 4. Syntax & API Reference

### Validated Environment Singleton Pattern
```typescript
import { z } from 'zod';

export function parseEnv<T extends z.ZodRawShape>(schema: z.ZodObject<T>) {
  const result = schema.safeParse(process.env);
  if (!result.success) {
    console.error('❌ Invalid environment variables:', result.error.flatten().fieldErrors);
    process.exit(1);
  }
  return Object.freeze(result.data);
}
```

---

## 5. Practical Examples

### Example 1: Basic Zod Environment Parser
```typescript
import { z } from 'zod';

const envSchema = z.object({
  PORT: z.coerce.number().default(8080),
  DATABASE_URL: z.string().min(1)
});

export const env = envSchema.parse(process.env);
// env.PORT is guaranteed number!
```

### Example 2: Real-World Multi-Environment Server Configuration
```typescript
import { z } from 'zod';

const serverConfigSchema = z.object({
  NODE_ENV: z.enum(['development', 'production', 'test']),
  HOST: z.string().default('0.0.0.0'),
  PORT: z.coerce.number().int().min(1024).max(65535).default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
  REDIS_URL: z.string().url().optional()
});

export type ServerConfig = z.infer<typeof serverConfigSchema>;

export function loadConfig(): ServerConfig {
  const parsed = serverConfigSchema.safeParse(process.env);
  if (!parsed.success) {
    console.error('Environment configuration failed:');
    console.error(JSON.stringify(parsed.error.flatten().fieldErrors, null, 2));
    throw new Error('Fatal: Invalid configuration');
  }
  return parsed.data;
}
```

### Example 3: Edge Case - Boolean Flags from Environment Strings
```typescript
// In process.env, boolean flags are passed as "true", "false", "1", "0"
const boolSchema = z.string().transform((v) => v === 'true' || v === '1');

export const featureFlagsSchema = z.object({
  ENABLE_BETA_FEATURES: boolSchema.default('false'),
  MAINTENANCE_MODE: boolSchema.default('false')
});
```

### Example 4: Failure Case - Type Discrepancy on Parsed Number
```typescript
const badConfig = {
  // @ts-expect-error TS2322: Type 'string | undefined' is not assignable to type 'number'.
  port: process.env.PORT
};
```

### Example 5: Idiomatic Config Provider Module
```typescript
export class ConfigService {
  private static instance: ServerConfig;

  public static get(): ServerConfig {
    if (!this.instance) {
      this.instance = loadConfig();
    }
    return this.instance;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`z.coerce.boolean()` Trap**:
   In Zod, `z.coerce.boolean()` uses `Boolean(value)`. In JavaScript, `Boolean("false")` evaluates to `true`! Never use `z.coerce.boolean()` for environment variables. Use `z.enum(['true', 'false']).transform(v => v === 'true')`.
2. **Declaration Merging Lies**:
   Adding types to `NodeJS.ProcessEnv` makes TypeScript assume variables are always strings, bypassing compiler errors even when the variable was never set in `.env`. Always combine declaration merging with runtime validation.
3. **Secret Leakage in Error Logs**:
   When logging validation errors, be careful not to print values of sensitive variables (`API_KEYS`, `SECRETS`) to console logs. Log the field names that failed, not their values.
4. **Testing Environments**:
   In unit tests, mutating `process.env.PORT = '4000'` can affect concurrent tests. Reset `process.env` in test setup or pass explicit config objects to services.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **`z.coerce.boolean()`** | `z.coerce.boolean()` on `"false"` | Check `val === 'true'` | `"false"` evaluates to boolean `true`! |
| **Scattered `process.env` Calls** | Reading `process.env` inside 20 services | Centralized `env` singleton | Missing vars crash deep inside business logic |
| **Declaration Merging Without Validation** | `declare global { namespace NodeJS ... }` | Zod schema parse at startup | Compiles fine, crashes with `undefined` at runtime |
| **Un-coerced Port Number** | `const port = process.env.PORT` | `z.coerce.number()` | Port is string `"8080"` instead of `8080` |
| **Late Failure on First Request** | Validating DB URL only inside first handler | Validate at server startup | Server boots but fails on first user traffic |

---

## 8. Performance & Type-Checker Complexity

- **Startup Execution**: Validating 50 environment variables with Zod takes ~1-2 milliseconds during server startup, which is negligible compared to database connection times (~200ms).
- **Type Checking**: Zod's `z.infer<typeof schema>` evaluates instantaneously in TypeScript.

---

## 9. Compiler Internals

When declaration merging:
```typescript
declare namespace NodeJS { interface ProcessEnv { PORT: string; } }
```
TypeScript merges the new property declarations into the existing symbol table entry for `ProcessEnv` created by `@types/node`.

---

## 10. Best Practices & Design Guidelines

1. **Fail Fast at Startup**: Halt server initialization if any environment variable is missing.
2. **Never Read `process.env` in Application Code**: Import from `src/config/env.ts`.
3. **Use Explicit Boolean Transformers**: Avoid `Boolean("false") === true`.
4. **Freeze the Config Object**: Prevent accidental runtime mutations.

---

## 11. Real-World Case Study

### High-Security FinTech Core Configuration Engine
A banking microservice strictly validates TLS certificates, API keys, and database pool limits:

```typescript
import { z } from 'zod';

export const FinTechConfigSchema = z.object({
  NODE_ENV: z.enum(['production', 'staging', 'development']),
  APP_PORT: z.coerce.number().default(4000),
  DB_POOL_MIN: z.coerce.number().default(5),
  DB_POOL_MAX: z.coerce.number().default(20),
  DATABASE_URL: z.string().url().refine((url) => url.startsWith('postgresql://'), {
    message: 'Must be a valid PostgreSQL connection URI'
  }),
  ENCRYPTION_KEY_HEX: z.string().length(64, 'Must be a 256-bit hex key')
});

export const config = FinTechConfigSchema.parse(process.env);
```

---

## 12. Interview Questions

<details>
<summary>1. Why is relying exclusively on `process.env` in Node.js TypeScript dangerous?</summary>

`process.env` returns `string | undefined` for all properties. It provides no runtime guarantee that required variables are defined, does not coerce strings to numbers or booleans, and can fail deep inside business logic long after the server has booted.
</details>

<details>
<summary>2. Why does `z.coerce.boolean()` fail on environment variables?</summary>

`z.coerce.boolean()` calls `Boolean(value)`. In JavaScript, any non-empty string is truthy (`Boolean("false") === true`). Consequently, setting `FEATURE_FLAG="false"` in an `.env` file evaluates to `true`.
</details>

<details>
<summary>3. What is the Fail-Fast principle in configuration management?</summary>

The Fail-Fast principle states that systems should immediately halt if an unrecoverable configuration error is detected during startup. Validating all environment variables before binding network listeners prevents servers from accepting traffic in a broken or insecure state.
</details>

<details>
<summary>4. How does TypeScript declaration merging apply to `process.env`?</summary>

By declaring `interface ProcessEnv` inside `namespace NodeJS`, TypeScript merges your custom field definitions with the existing global `ProcessEnv` interface from `@types/node`.
</details>

<details>
<summary>5. How should numbers and ports be parsed from environment variables?</summary>

Using `z.coerce.number().int().min(1).max(65535)` or `Number.parseInt(process.env.PORT, 10)`.
</details>

<details>
<summary>6. How can you prevent secret environment variables from leaking into client-side code in full-stack frameworks?</summary>

Next.js and Vite only bundle environment variables prefixed with `NEXT_PUBLIC_` or `VITE_` into client bundles. Any variable without this prefix remains server-only.
</details>

<details>
<summary>7. What is `@t3-oss/env-core`?</summary>

It is a popular TypeScript utility built on top of Zod that enforces strict separation between server and client environment variables, automatically halting builds if required variables are missing.
</details>

<details>
<summary>8. How do you mock environment variables in unit tests without cross-test pollution?</summary>

Save `const originalEnv = { ...process.env }` in `beforeEach()`, mutate `process.env` for the specific test, and restore `process.env = originalEnv` in `afterEach()`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Port Getter</summary>

**Problem:** Write a function `getPort(fallback: number): number` that parses `process.env.PORT` safely.

**Solution:**
```typescript
export function getPort(fallback = 3000): number {
  const raw = process.env.PORT;
  if (!raw) return fallback;
  const num = Number(raw);
  return Number.isInteger(num) && num > 0 ? num : fallback;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Require Env Helper</summary>

**Problem:** Create a function `requireEnv(name: string): string` that returns the variable or throws an error.

**Solution:**
```typescript
export function requireEnv(name: string): string {
  const val = process.env[name];
  if (!val) throw new Error(`Missing required environment variable: ${name}`);
  return val;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Boolean Environment Transformer</summary>

**Problem:** Write a Zod schema validating a boolean environment flag that correctly interprets `"true"`, `"false"`, `"1"`, and `"0"`.

**Solution:**
```typescript
import { z } from 'zod';

export const EnvBoolean = z.string().transform((val, ctx) => {
  if (val === 'true' || val === '1') return true;
  if (val === 'false' || val === '0') return false;
  ctx.addIssue({ code: z.ZodIssueCode.custom, message: 'Must be true/false or 1/0' });
  return z.NEVER;
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Declaration Merging for Custom App Env</summary>

**Problem:** Augment `NodeJS.ProcessEnv` with `REDIS_HOST: string` and `REDIS_PORT: string`.

**Solution:**
```typescript
declare global {
  namespace NodeJS {
    interface ProcessEnv {
      REDIS_HOST: string;
      REDIS_PORT: string;
    }
  }
}
export {};
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Composable Env Loader</summary>

**Problem:** Create a generic function that takes a Zod schema, validates `process.env`, and returns an immutable configuration singleton.

**Solution:**
```typescript
import { z } from 'zod';

export function makeConfig<T extends z.ZodTypeAny>(schema: T): Readonly<z.infer<T>> {
  const result = schema.safeParse(process.env);
  if (!result.success) {
    throw new Error(`Config validation failed:\n${JSON.stringify(result.error.flatten(), null, 2)}`);
  }
  return Object.freeze(result.data);
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Environment Validator with Prefix Stripping</summary>

**Problem:** Write a parser that extracts all environment variables starting with `APP_` and strips the prefix into a typed object.

**Solution:**
```typescript
export function extractPrefixedEnv<T extends Record<string, string>>(prefix = 'APP_'): T {
  const out: Record<string, string> = {};
  for (const [k, v] of Object.entries(process.env)) {
    if (k.startsWith(prefix) && v !== undefined) {
      out[k.slice(prefix.length).toLowerCase()] = v;
    }
  }
  return out as T;
}
```
</details>
