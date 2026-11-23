# Trust Boundaries and Runtime Validation in TypeScript

> Fundamental architectural guide to trust boundaries: why compile-time types cannot protect runtime inputs, parsing vs validating, deserialization boundaries, and defense-in-depth in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Runtime Validation |
| **Difficulty** | Intermediate |
| **Prerequisites** | [01-fundamentals/06-any-and-unknown.md](../01-fundamentals/06-any-and-unknown.md), [14-type-system-internals/00-type-erasure-and-runtime.md](../14-type-system-internals/00-type-erasure-and-runtime.md) |
| **TypeScript Baseline** | TS 4.0+, TS 5.0+ |
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
One of the most dangerous misconceptions among developers learning TypeScript is assuming that because an input has a TypeScript type annotation (e.g. `user: User`), the object at runtime is guaranteed to conform to that structure. Due to **Type Erasure**, TypeScript types vanish at runtime. When data crosses a boundary—from an HTTP request body, a database query, environment variables, `localStorage`, or an external webhook—it is completely untyped and untrusted.

A **Trust Boundary** is any interface where external data enters your application. At these boundaries, static types provide zero security. Establishing strong runtime validation gates using the "Parse, Don't Validate" principle ensures that invalid payloads are rejected immediately, protecting internal business logic from runtime corruption.

---

## 2. Core Concepts
- **Trust Boundaries**: The perimeter separating your verified internal domain from unverified external inputs:
  - Inbound HTTP request JSON bodies, headers, and query strings.
  - Environment variables (`process.env`).
  - Database and cache results.
  - Third-party REST / GraphQL API responses.
  - User file uploads and web storage (`localStorage`).
- **Parse, Don't Validate**: A functional programming principle:
  - *Validating*: Checking data and returning a boolean without changing the type.
  - *Parsing*: Converting untrusted, loosely structured input into a verified, statically typed domain model, or failing explicitly.
- **Type Assertions (`as T`) vs Parsers**: `data as User` tells the compiler to lie to itself; it performs no runtime checks. A parser inspects runtime memory structures before returning a typed object.
- **Defense-in-Depth**: Validating early at the network controller, maintaining strict domain invariants, and sanitizing outputs.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|               Untrusted World (External Inputs)             |
|       - HTTP POST JSON body: "{ \"amount\": -50 }"          |
|       - process.env.PORT: "invalid_string"                  |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                      TRUST BOUNDARY                         |
|                 Type: unknown (NOT any!)                    |
+------------------------------+------------------------------+
                               |
                 Runtime Schema Validation / Parser
                               |
            +------------------+------------------+
            |                                     |
       Invalid Data                          Valid Data
            |                                     |
            v                                     v
    Reject Immediately                  Convert to Typed Entity
    Return 400 Bad Request              Type: ValidatedOrder
            |                                     |
            x                                     v
                                    +---------------------------+
                                    |   Trusted Internal Domain |
                                    |   (100% Type-Safe Logic)  |
                                    +---------------------------+
```

---

## 4. Syntax & API Reference

```typescript
// 1. Insecure Boundary (Anti-pattern)
app.post('/api/user', (req, res) => {
  const user = req.body as User; // DANGEROUS: Zero runtime checks!
  db.save(user);
});

// 2. Secure Boundary with Type Guard
app.post('/api/user', (req, res) => {
  const rawBody: unknown = req.body;
  if (!isUserPayload(rawBody)) {
    return res.status(400).json({ error: 'Invalid user payload' });
  }
  // rawBody is narrowed to UserPayload
  db.save(rawBody);
});

// 3. User-defined Type Guard definition
function isUserPayload(data: unknown): data is { name: string; age: number } {
  return (
    typeof data === 'object' &&
    data !== null &&
    'name' in data &&
    typeof (data as Record<string, unknown>).name === 'string' &&
    'age' in data &&
    typeof (data as Record<string, unknown>).age === 'number'
  );
}
```

---

## 5. Practical Examples

### Example 1: Environment Variable Trust Boundary
```typescript
interface AppEnvironment {
  PORT: number;
  DATABASE_URL: string;
  NODE_ENV: 'development' | 'production' | 'test';
}

export function loadValidatedEnv(env: Record<string, string | undefined>): AppEnvironment {
  const portStr = env.PORT ?? '3000';
  const port = Number(portStr);
  if (Number.isNaN(port) || port <= 0) {
    throw new Error(`Invalid PORT environment variable: "${portStr}"`);
  }

  const dbUrl = env.DATABASE_URL;
  if (!dbUrl || !dbUrl.startsWith('postgresql://')) {
    throw new Error('DATABASE_URL must be a valid postgres connection string');
  }

  const nodeEnv = env.NODE_ENV ?? 'development';
  if (nodeEnv !== 'development' && nodeEnv !== 'production' && nodeEnv !== 'test') {
    throw new Error(`Invalid NODE_ENV: "${nodeEnv}"`);
  }

  return { PORT: port, DATABASE_URL: dbUrl, NODE_ENV: nodeEnv };
}
```

### Example 2: Safe API Ingestion from Third-Party Service
```typescript
interface ExternalWeatherDto {
  temperatureC: number;
  humidity: number;
}

async function fetchWeather(city: string): Promise<ExternalWeatherDto> {
  const res = await fetch(`https://api.weather.com/v1/${encodeURIComponent(city)}`);
  const raw: unknown = await res.json(); // Treat as unknown!

  if (
    typeof raw === 'object' &&
    raw !== null &&
    'temperatureC' in raw &&
    typeof (raw as Record<string, unknown>).temperatureC === 'number' &&
    'humidity' in raw &&
    typeof (raw as Record<string, unknown>).humidity === 'number'
  ) {
    return raw as ExternalWeatherDto;
  }

  throw new Error(`Invalid response contract from Weather API for city ${city}`);
}
```

### Example 3: Edge Case - Prototype Pollution at Deserialization Boundary
```typescript
// Malicious JSON payload: '{"__proto__": {"isAdmin": true}}'
function sanitizeObject(input: unknown): Record<string, unknown> {
  if (typeof input !== 'object' || input === null) {
    throw new Error('Expected plain object');
  }

  const clean = Object.create(null);
  for (const [key, value] of Object.entries(input)) {
    if (key === '__proto__' || key === 'constructor' || key === 'prototype') {
      continue; // Block prototype pollution attack!
    }
    clean[key] = value;
  }
  return clean;
}
```

### Example 4: Compiler Failure Case - Treating Unknown as Usable
```typescript
function processIncomingWebhook(payload: unknown) {
  // @ts-expect-error TS18046: 'payload' is of type 'unknown'.
  console.log(payload.eventId);
}
```

### Example 5: Idiomatic "Parse, Don't Validate" Pattern
```typescript
interface NonEmptyStringBrand {
  readonly NonEmptyString: unique symbol;
}
export type NonEmptyString = string & NonEmptyStringBrand;

export function parseNonEmptyString(input: string): NonEmptyString {
  const trimmed = input.trim();
  if (trimmed.length === 0) {
    throw new Error('String cannot be empty');
  }
  return trimmed as NonEmptyString;
}

// Internal functions only accept NonEmptyString, guaranteeing valid data
function publishPost(title: NonEmptyString) {
  console.log(`Publishing: ${title}`);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The `as T` Illusion**: Writing `const user = req.body as User` compiles without warnings, creating an illusion of type safety while performing zero actual checks. If `req.body` is empty, code crashes on `user.name.toUpperCase()`.
2. **`JSON.parse` Returns `any`**: In TypeScript standard definitions, `JSON.parse` returns `any`. Always immediately type-cast the result to `unknown`: `const data: unknown = JSON.parse(str)`.
3. **Database Drivers Returning `any`**: Many ORMs or SQL clients return rows as `any`. Always validate rows crossing into the domain.
4. **Dates in JSON Deserialization**: Dates serialized over JSON become ISO strings (`"2026-11-24T00:00:00Z"`). Expecting `payload.createdAt instanceof Date` fails at runtime without a transformation step.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `req.body as MyDto` | Zero runtime validation; crashes deep in domain | Validate with Zod / type guard at controller |
| `const data = JSON.parse(str)` (implicitly `any`) | Inadvertently spreads `any` to downstream callers | Use `const data: unknown = JSON.parse(str)` |
| Validating without narrowing (`isUser()` returning boolean) | Forces redundant casting | Use type predicate: `isUser(x): x is User` |
| Assuming `process.env.VAR` is always defined | Runtime `undefined` bugs in production | Validate all environment variables at startup |
| Trusting internal microservice HTTP responses | Service drift causes uncaught production crashes | Validate microservice payloads at boundary |

---

## 8. Performance & Type-Checker Complexity
- Validating data at trust boundaries introduces negligible runtime overhead (typically $<0.05\text{ms}$ per request) while saving hours of debugging production data corruption.
- Writing type guards using control-flow analysis requires $\mathcal{O}(1)$ type-checking work in TSServer.

---

## 9. Compiler Internals
In TypeScript's binder and checker:
- A variable annotated as `unknown` has `TypeFlags.Unknown`.
- In `checkPropertyAccessExpression`, if the target has `TypeFlags.Unknown`, diagnostic `TS18046` is emitted immediately.
- Only AST control-flow nodes (like `typeof x === 'string'` or custom predicates `x is T`) narrow `TypeFlags.Unknown`.

---

## 10. Best Practices & Design Guidelines
- Treat all data crossing external boundaries as `unknown`.
- Never use type assertions (`as Type`) to ingest network, file, or environment inputs.
- Adopt "Parse, Don't Validate": transform untrusted input into verified domain entities.
- Validate environment variables at process boot; exit immediately (`process.exit(1)`) on invalid config.
- Centralize validation gates in framework middleware (Express, Fastify, NestJS) before reaching business controllers.

---

## 11. Real-World Case Study
A crypto payment gateway accepted merchant webhook confirmations. The handler relied on type assertions: `const payload = req.body as WebhookConfirmation;`. A merchant changed their API version, sending `amountInSatoshis` instead of `amountInBtc`. The type assertion compiled, but `payload.amountInBtc` was undefined. Math operations on `undefined` evaluated to `NaN`, leading to zero-dollar balance credits and thousands of dollars in uncollected revenue.

**Remediation:**
The gateway refactored the webhook controller to use a strict runtime parser. Payloads with missing or renamed fields were rejected with HTTP 400 immediately, alerting both engineering teams to the API contract mismatch before any balances were credited.

---

## 12. Interview Questions

<details>
<summary>1. Why does TypeScript not protect your code from invalid HTTP request bodies?</summary>

Because TypeScript's type system is completely erased during compilation. At runtime, JavaScript receives raw JSON strings from the network. TypeScript type annotations do not exist in the V8 runtime and cannot perform data validation.
</details>

<details>
<summary>2. What is the difference between "Validating" and "Parsing"?</summary>

- **Validating**: Checking whether data meets certain constraints and returning a boolean (`true`/`false`), leaving the data in its loose, unverified type.
- **Parsing**: Inspecting the raw data, verifying its invariants, and transforming it into a strongly-typed domain model, guaranteeing that downstream code cannot access unverified states.
</details>

<details>
<summary>3. Why is `const data = JSON.parse(str) as User` considered an anti-pattern?</summary>

Because `as User` is a static type assertion that overrides compiler checking without performing any runtime inspection. If `str` is `"{}"` or contains corrupt keys, `data` is not a `User`, leading to runtime crashes when properties are accessed.
</details>

<details>
<summary>4. What is a "Trust Boundary"?</summary>

Any point where data originating outside the control of your application (user input, HTTP requests, third-party APIs, database rows, environment variables) enters your program.
</details>

<details>
<summary>5. How should you type the return value of an external API call before validation?</summary>

`Promise<unknown>`. Never `Promise<any>`.
</details>

<details>
<summary>6. What is prototype pollution and how can it occur at trust boundaries?</summary>

An attack where an untrusted JSON object contains properties like `"__proto__"` or `"constructor"`. When merged or cloned without sanitization, it mutates `Object.prototype`, affecting all objects across the JavaScript runtime.
</details>

<details>
<summary>7. How do user-defined type guards (`val is T`) help enforce trust boundaries?</summary>

They encapsulate runtime verification checks while allowing TypeScript's control-flow analyzer to safely narrow an `unknown` value to type `T` within downstream conditional branches.
</details>

<details>
<summary>8. What is the best strategy for handling environment variables in production TypeScript apps?</summary>

Validate all environment variables at process startup using a schema library (like Zod or `t3-env`), throwing an immediate fatal error and terminating the process if any required variable is missing or malformed.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Safe Unknown Extractor**: Write a function `getStringProperty(obj: unknown, key: string): string | null` that safely extracts a string property without crashing.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function getStringProperty(obj: unknown, key: string): string | null {
     if (typeof obj === 'object' && obj !== null && key in obj) {
       const val = (obj as Record<string, unknown>)[key];
       return typeof val === 'string' ? val : null;
     }
     return null;
   }
   ```
   </details>

2. **Boolean Boundary Guard**: Write a type guard `isBoolean(val: unknown): val is boolean`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function isBoolean(val: unknown): val is boolean {
     return typeof val === 'boolean';
   }
   ```
   </details>

### Medium
3. **Environment Validator**: Write a complete validator function `validateDbConfig(env: Record<string, string | undefined>)` that verifies `DB_HOST`, `DB_PORT`, and `DB_USER` exist and returns a typed configuration object.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface DbConfig {
     host: string;
     port: number;
     user: string;
   }

   export function validateDbConfig(env: Record<string, string | undefined>): DbConfig {
     const host = env.DB_HOST;
     if (!host) throw new Error('Missing DB_HOST');

     const port = Number(env.DB_PORT);
     if (Number.isNaN(port) || port <= 0) throw new Error('Invalid DB_PORT');

     const user = env.DB_USER;
     if (!user) throw new Error('Missing DB_USER');

     return { host, port, user };
   }
   ```
   </details>

4. **Array Contract Guard**: Write a type predicate `isStringRecordArray(val: unknown): val is Array<Record<string, string>>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function isStringRecordArray(val: unknown): val is Array<Record<string, string>> {
     if (!Array.isArray(val)) return false;
     return val.every(item => {
       if (typeof item !== 'object' || item === null) return false;
       return Object.values(item).every(v => typeof v === 'string');
     });
   }
   ```
   </details>

### Hard
5. **Recursive JSON Safe Parser**: Implement a function `parseJsonSafely<T>(raw: string, guard: (data: unknown) => data is T): { ok: true; data: T } | { ok: false; error: string }` that catches syntax errors and schema validation mismatches cleanly.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function parseJsonSafely<T>(
     raw: string,
     guard: (data: unknown) => data is T
   ): { ok: true; data: T } | { ok: false; error: string } {
     let parsed: unknown;
     try {
       parsed = JSON.parse(raw);
     } catch (err) {
       return { ok: false, error: `Invalid JSON syntax: ${String(err)}` };
     }

     if (guard(parsed)) {
       return { ok: true, data: parsed };
     }

     return { ok: false, error: 'Data failed schema validation guard' };
   }
   ```
   </details>

6. **Branded Currency Value Object Parser**: Create a branded type `UsdAmount` and a parser `parseUsd(input: unknown): UsdAmount` that ensures the input is a valid positive number with at most 2 decimal places, rejecting NaN, negative values, and floating point overprecision.
   <details>
   <summary>Solution</summary>

   ```typescript
   declare const UsdAmountBrand: unique symbol;
   export type UsdAmount = number & { readonly [UsdAmountBrand]: true };

   export function parseUsd(input: unknown): UsdAmount {
     if (typeof input !== 'number' || Number.isNaN(input) || !Number.isFinite(input)) {
       throw new Error('Amount must be a finite number');
     }
     if (input <= 0) {
       throw new Error('Amount must be positive');
     }
     // Check decimal precision (at most 2 decimals)
     const decimals = (input.toString().split('.')[1] || '').length;
     if (decimals > 2) {
       throw new Error('USD amount cannot have more than 2 decimal places');
     }

     return input as UsdAmount;
   }
   ```
   </details>
