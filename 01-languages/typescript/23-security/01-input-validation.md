# Input Validation and Boundary Defense in TypeScript: Zod and ArkType

> Complete guide to zero-trust architecture and boundary defense in TypeScript: schema validation at trust boundaries, prototype pollution prevention, and automated schema-to-type inference.

| Metadata | Details |
| :--- | :--- |
| **Category** | Security |
| **Difficulty** | Advanced |
| **Prerequisites** | [15-runtime-validation/00-trust-boundaries.md](../15-runtime-validation/00-trust-boundaries.md), [15-runtime-validation/02-zod.md](../15-runtime-validation/02-zod.md) |
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

In TypeScript applications, the most dangerous security vulnerability is **assuming that static types exist at runtime**.
Every application is surrounded by external trust boundaries:
- Incoming HTTP request bodies, headers, and query parameters.
- Database query responses and Redis cache payloads.
- Environment variables (`process.env`).
- Third-party webhook payloads.

If a developer casts an incoming payload (`const body = req.body as CreateOrderDTO`), zero runtime validation occurs. If an attacker injects unexpected fields, malformed types, or prototype pollution vectors (`__proto__`), the application will either crash or execute unintended business logic.

Zero-Trust engineering dictates: **Validate at the boundary; trust within the core.**

---

## 2. Core Concepts

### Trust Boundaries
A trust boundary is any point where external, untrusted data enters the application memory space. Data must be parsed using runtime schema validators before crossing the boundary into typed business logic.

```
+-------------------------------------------------------------------------+
|                  Zero-Trust Boundary Validation Architecture            |
+-------------------------------------------------------------------------+
|                                                                         |
|  Untrusted Network Input (HTTP Request / Webhook / process.env)         |
|                                |                                        |
|                                v                                        |
|                 [ Trust Boundary: Zod / ArkType ]                       |
|                 Schema.parse(untrustedData)                             |
|                                |                                        |
|         +----------------------+----------------------+                 |
|         | Success                                     | Failure         |
|         v                                             v                 |
|  [ Validated Domain Type ]                   [ 400 Bad Request ]        |
|  Safe, typed, sanitized payload               Detailed error response   |
|  flows into business domain logic             terminated immediately    |
|                                                                         |
+-------------------------------------------------------------------------+
```

### Schema-First Single Source of Truth
Instead of maintaining separate TypeScript interfaces and runtime validation functions, schemas act as the single source of truth. TypeScript types are inferred dynamically via `z.infer<typeof Schema>` or ArkType `typeof type`.

---

## 3. How It Works Internally

1. **Parsing**: When `Schema.parse(input)` is called, the schema engine inspects the input against defined validation rules (string length, regex, integer bounds).
2. **Sanitization & Stripping**: By default, strict validators strip unexpected extraneous properties, defending against parameter injection.
3. **Prototype Defense**: Validators check own properties (`Object.hasOwn`), preventing poisoned prototype keys (`__proto__`, `constructor`) from affecting internal dictionaries.

---

## 4. Syntax & API Reference

### Hardened Zod Request Schema
```typescript
import { z } from "zod";

export const CreateUserSchema = z
  .object({
    username: z
      .string()
      .min(3)
      .max(30)
      .regex(/^[a-zA-Z0-9_]+$/, "Username must be alphanumeric"),
    email: z.string().email(),
    age: z.number().int().min(18).max(120),
    role: z.enum(["admin", "member"]).default("member"),
  })
  .strict(); // Disallows unexpected properties

export type CreateUserInput = z.infer<typeof CreateUserSchema>;
```

### ArkType High-Performance Schema
```typescript
import { type } from "arktype";

export const UserContract = type({
  id: "string.uuid",
  email: "email",
  "score?": "number>=0",
});

export type UserType = typeof UserContract.infer;
```

---

## 5. Practical Examples

### Example 1: Environment Variable Boundary Validation
```typescript
import { z } from "zod";

const EnvSchema = z.object({
  NODE_ENV: z.enum(["development", "production", "test"]),
  PORT: z.coerce.number().default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32, "JWT Secret must be at least 32 characters"),
});

export function loadValidatedEnv() {
  const result = EnvSchema.safeParse(process.env);
  if (!result.success) {
    console.error("Fatal: Invalid environment configuration:", result.error.format());
    process.exit(1);
  }
  return result.data;
}
```

### Example 2: Defending Express Middleware Against Prototype Poisoning
```typescript
import type { Request, Response, NextFunction } from "express";
import { z } from "zod";

export function validateBody<T extends z.ZodTypeAny>(schema: T) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const result = schema.safeParse(req.body);
    if (!result.success) {
      res.status(400).json({
        error: "Validation Failed",
        details: result.error.flatten(),
      });
      return;
    }
    req.body = result.data;
    next();
  };
}
```

### Example 3: Edge Case - Prototype Pollution Sanitization
```typescript
export function sanitizeInputObject<T extends object>(payload: T): T {
  // Guard against prototype pollution
  const clean = Object.create(null);
  for (const [key, value] of Object.entries(payload)) {
    if (key === "__proto__" || key === "constructor" || key === "prototype") {
      continue; // Blocked!
    }
    clean[key] = value;
  }
  return clean as T;
}
```

### Example 4: Compiler Error & Inferred Type Safety
```typescript
const DataSchema = z.object({
  active: z.boolean(),
  tags: z.array(z.string()),
});

type DataModel = z.infer<typeof DataSchema>;

const model: DataModel = {
  active: true,
  tags: ["security", "validation"],
};

// @ts-expect-error - TS2322: Type 'number' is not assignable to type 'boolean'.
model.active = 1;
```

### Example 5: High-Speed ArkType In-Memory Validation
```typescript
import { type } from "arktype";

const TransactionSchema = type({
  txId: "string",
  amount: "number>0",
  "memo?": "string",
});

export function processTx(raw: unknown) {
  const out = TransactionSchema(raw);
  if (out instanceof type.errors) {
    throw new Error(`Invalid transaction: ${out.summary}`);
  }
  // out is strictly typed as { txId: string; amount: number; memo?: string }
  return out.txId;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`z.coerce` Risks**:
   `z.coerce.boolean()` evaluates `"false"` to `true` (because in JS `Boolean("false") === true`). Use explicit transforms or custom schemas for boolean query parameters.
2. **Prototype Pollution via Recursive Merge**:
   Libraries that perform deep cloning without checking `Object.prototype` can be corrupted by malformed JSON. Always use `.strict()` or hardened clone utilities.
3. **Memory Exhaustion via Regex DOS (ReDoS)**:
   Poorly written regex validations in schemas can lock up the V8 event loop when fed malicious strings. Avoid catastrophic backtracking patterns.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Type Casting at Boundary** | `const u = req.body as User` | `UserSchema.parse(req.body)` | Zero runtime validation |
| **Loose Schema (.passthrough)** | Accepting arbitrary extra keys | Use `.strict()` to reject unknowns | Mass-assignment vulnerability |
| **Unvalidated Environment** | Direct access to `process.env.PORT` | Parse env via Zod on startup | Runtime crash on missing env |
| **Flawed Boolean Coercion** | `z.coerce.boolean()` on strings | `z.enum(['true', 'false'])` | `"false"` evaluates to `true` |
| **Unbounded Array Sizes** | `z.array(z.string())` | `z.array(z.string()).max(100)` | Denial of Service (OOM) |
| **Ignoring Prototype Keys** | Unsanitized deep object assign | Filter out `__proto__` keys | Prototype pollution |

---

## 8. Performance & Type-Checker Complexity

- **Zod**: Feature-rich, highly flexible, ~50,000 ops/sec.
- **ArkType**: JIT-optimized parser in TypeScript, ~500,000+ ops/sec.
- **TypeBox**: Uses JSON Schema compiled via `Ajv`, achieving over **1,000,000 ops/sec**.

---

## 9. Compiler Internals

When using `z.infer<T>`:
1. TypeScript evaluates mapped conditional types on the Zod schema class hierarchy.
2. It recursively extracts inner types: `T extends ZodType<infer Output> ? Output : never`.
3. In TS 5.0+, complex mapped inference is memoized by the compiler to prevent evaluation slowdowns.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `.strict()` on Input DTOs**: Prevents parameter injection attacks.
2. **Validate Environment Variables at Boot**: Fail immediately if secrets or ports are invalid.
3. **Sanitize or Strip Prototype Keys**: Ensure user input cannot pollute the global object prototype.
4. **Cap All Array and String Lengths**: Prevent resource exhaustion and buffer memory bloat.

---

## 11. Real-World Case Study

A cloud-based SaaS platform experienced a mass-assignment privilege escalation vulnerability. The user update API accepted JSON payloads without strict schema validation:
```typescript
// Vulnerable:
app.patch("/api/user", (req, res) => {
  const update = req.body;
  db.users.update(req.userId, update); // Injected { role: 'admin' }!
});
```
An attacker updated their own profile passing `{ role: "admin" }`, immediately gaining root access.

**Fix**:
Implemented `UpdateProfileSchema = z.object({ name: z.string(), bio: z.string() }).strict()`. The API rejected any unauthorized role properties with a 400 error.

---

## 12. Interview Questions

<details>
<summary>1. Why is `req.body as UserDTO` dangerous in a production backend?</summary>

Type assertions (`as`) are completely erased during compilation and do not perform any runtime checks. If an attacker submits missing, malformed, or malicious data, the application will proceed as if the data were valid, resulting in crashes, prototype pollution, or authorization bypasses.
</details>

<details>
<summary>2. How does `.strict()` in Zod enhance API security?</summary>

By default, Zod strips unrecognized properties without throwing an error. `.strict()` causes validation to fail if any unexpected properties are present, preventing mass-assignment attacks where attackers inject privileged fields like `isAdmin: true`.
</details>

<details>
<summary>3. What is prototype pollution and how can TypeScript applications defend against it?</summary>

Prototype pollution occurs when an attacker injects properties into `Object.prototype` (often via `__proto__` or `constructor.prototype`), affecting all objects across the application. Defense involves creating null-prototype dictionaries (`Object.create(null)`), freezing prototypes, and sanitizing incoming keys.
</details>

<details>
<summary>4. Why can `z.coerce.boolean()` cause subtle security bugs?</summary>

In JavaScript, `Boolean("false")` evaluates to `true` because any non-empty string is truthy. Consequently, passing `"false"` to `z.coerce.boolean()` yields `true`.
</details>

<details>
<summary>5. How does schema-first development eliminate type drift?</summary>

By deriving TypeScript types directly from schemas (`type User = z.infer<typeof UserSchema>`), developers ensure that static types and runtime validation rules can never diverge or fall out of sync.
</details>

<details>
<summary>6. How do ArkType and TypeBox achieve higher performance than traditional validators?</summary>

They pre-compile validation routines into optimized JavaScript code (or evaluate JIT validation closures) rather than dynamically interpreting recursive object rules on every request.
</details>

<details>
<summary>7. What is ReDoS (Regular Expression Denial of Service) in input validation?</summary>

ReDoS occurs when an unanchored or catastrophically backtracking regular expression (e.g. `(a+)+$`) is matched against a crafted input string, causing CPU utilization to spike to 100% and locking the Node.js event loop.
</details>

<details>
<summary>8. Where should boundary validation occur in a layered application?</summary>

At the outermost ingress points: controller middleware, API gateways, database repository output adapters, and configuration loaders, ensuring the internal domain model receives only verified data.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Create a strict Zod login schema</summary>

**Task**: Write a Zod schema for login requiring valid email and password (min 8 chars), disallowing unexpected keys.

**Solution**:
```typescript
import { z } from "zod";

export const LoginSchema = z
  .object({
    email: z.string().email(),
    password: z.string().min(8),
  })
  .strict();

export type LoginInput = z.infer<typeof LoginSchema>;
```
</details>

<details>
<summary>Challenge 2 (Easy): Safe boolean query parameter parser</summary>

**Task**: Write a Zod transformer that safely parses `"true"` and `"false"` query strings into actual booleans.

**Solution**:
```typescript
import { z } from "zod";

export const BooleanQuerySchema = z
  .enum(["true", "false"])
  .transform((val) => val === "true");
```
</details>

<details>
<summary>Challenge 3 (Medium): Validate environment variables on startup</summary>

**Task**: Implement a typed configuration reader that parses `DATABASE_URL` and `PORT` from `process.env`.

**Solution**:
```typescript
import { z } from "zod";

const ConfigSchema = z.object({
  DATABASE_URL: z.string().url(),
  PORT: z.string().transform(Number),
});

export const config = ConfigSchema.parse(process.env);
```
</details>

<details>
<summary>Challenge 4 (Medium): Bounded pagination schema</summary>

**Task**: Write a pagination query schema that caps `limit` to maximum 100 and enforces positive integers.

**Solution**:
```typescript
import { z } from "zod";

export const PaginationSchema = z.object({
  page: z.coerce.number().int().min(1).default(1),
  limit: z.coerce.number().int().min(1).max(100).default(20),
});
```
</details>

<details>
<summary>Challenge 5 (Hard): Prototype-Clean Object Cloner</summary>

**Task**: Implement a deep clone utility that drops any property named `__proto__`, `constructor`, or `prototype`.

**Solution**:
```typescript
export function safeDeepClone<T>(input: T): T {
  if (input === null || typeof input !== "object") return input;

  if (Array.isArray(input)) {
    return input.map((item) => safeDeepClone(item)) as unknown as T;
  }

  const output = Object.create(null);
  for (const [key, value] of Object.entries(input)) {
    if (key === "__proto__" || key === "constructor" || key === "prototype") {
      continue;
    }
    output[key] = safeDeepClone(value);
  }
  return output as T;
}
```
</details>

<details>
<summary>Challenge 6 (Hard): High-Performance TypeBox Validator</summary>

**Task**: Author a TypeBox schema for a high-frequency financial quote and compile it into an Ajv validator.

**Solution**:
```typescript
import { Type, Static } from "@sinclair/typebox";
import Ajv from "ajv";

const ajv = new Ajv();

export const QuoteSchema = Type.Object({
  symbol: Type.String({ minLength: 1, maxLength: 10 }),
  bid: Type.Number({ minimum: 0 }),
  ask: Type.Number({ minimum: 0 }),
  timestamp: Type.Integer(),
});

export type Quote = Static<typeof QuoteSchema>;

export const validateQuote = ajv.compile(QuoteSchema);
```
</details>
