# Enterprise TypeScript Security Checklist: Production Readiness

> Comprehensive, actionable production security checklist for enterprise TypeScript systems: compiler hardening, AST lint rules, secret protection, runtime guardrails, and automated CI gates.

| Metadata | Details |
| :--- | :--- |
| **Category** | Security |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [23-security/00-unsafe-types-and-assertions.md](./00-unsafe-types-and-assertions.md), [23-security/01-input-validation.md](./01-input-validation.md) |
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

Securing an enterprise TypeScript codebase requires defense-in-depth across the entire software development lifecycle:
1. **Compile-Time**: Enforce strict compiler settings to eliminate implicit type holes.
2. **Static Analysis**: Block unsafe patterns (`any`, unhandled promises, dangerous regex) via AST lint rules.
3. **Supply Chain**: Restrict package installation permissions and audit dependencies.
4. **Boundary Ingress**: Validate and sanitize all external data via runtime schemas.
5. **Runtime**: Shield production processes against unhandled rejections, prototype pollution, and secret leaks.

This checklist serves as an authoritative audit guide for engineering leads and security architects prior to production deployment.

---

## 2. Core Concepts

### The 5 Pillars of TypeScript Security
- **Pillar 1: Compiler Hardening**: Maximum strictness in `tsconfig.json`.
- **Pillar 2: Linter Guardrails**: Strict type-checked rules in `eslint.config.mjs`.
- **Pillar 3: Boundary Defense**: Zero-trust runtime schema parsing (Zod, ArkType).
- **Pillar 4: Supply Chain Defense**: Pinned versions, `ignore-scripts`, lockfile integrity.
- **Pillar 5: Secret Management & Observability**: Redacted logging and environment validation.

```
+-------------------------------------------------------------------------+
|                  Enterprise 5-Pillar Security Architecture              |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ 1. Compiler ]         [ 2. Static Analysis ]     [ 3. Boundary ]     |
|  "strict": true          typescript-eslint v8       Zod / ArkType       |
|  noImplicitAny           no-floating-promises       Zero-trust schema   |
|  exactOptionalProperties consistent-type-assertions Input sanitization  |
|            |                        |                      |            |
|            +------------------------+----------------------+            |
|                                     |                                   |
|                                     v                                   |
|                 [ 4. Supply Chain & Package Hygiene ]                   |
|                 pnpm ignore-scripts, frozen lockfile                    |
|                                     |                                   |
|                                     v                                   |
|                 [ 5. Runtime & Secret Protection ]                      |
|                 Pino log redaction, non-root Docker                     |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

Security rules are enforced through layered validation filters:
- `tsc --noEmit` verifies type soundness.
- ESLint checks code semantics and AST safety.
- Runtime validators parse network payloads.
- Docker builds execute non-root unprivileged processes.

---

## 4. Syntax & API Reference

### Hardened Production `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noImplicitAny": true,
    "strictNullChecks": true,
    "noImplicitReturns": true,
    "noFallthroughCasesInSwitch": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "useUnknownInCatchVariables": true,
    "forceConsistentCasingInFileNames": true,
    "skipLibCheck": true,
    "noEmit": true
  }
}
```

### Production Security Checklist Table

| Pillar | Requirement | Enforcement Mechanism | Status |
| :--- | :--- | :--- | :--- |
| **Compiler** | `strict: true` | `tsconfig.json` | Mandatory |
| **Compiler** | `noUncheckedIndexedAccess: true` | `tsconfig.json` | Mandatory |
| **Compiler** | `useUnknownInCatchVariables: true`| `tsconfig.json` | Mandatory |
| **Linting** | Ban `any` types | `@typescript-eslint/no-explicit-any` | Mandatory |
| **Linting** | Ban floating promises | `@typescript-eslint/no-floating-promises` | Mandatory |
| **Linting** | Ban double assertions | `@typescript-eslint/consistent-type-assertions` | Mandatory |
| **Boundary** | Strict DTO validation | Zod `.strict()` | Mandatory |
| **Supply Chain** | Block install scripts | `ignore-scripts=true` in `.npmrc` | Mandatory |
| **Supply Chain** | Frozen lockfiles | `pnpm install --frozen-lockfile` | Mandatory |
| **Runtime** | Log Redaction | Pino `redact: ['headers.authorization']` | Mandatory |
| **Runtime** | Non-root container user | Dockerfile `USER node` | Mandatory |

---

## 5. Practical Examples

### Example 1: `noUncheckedIndexedAccess` Safety
```typescript
const userRoles: Record<string, string> = {
  alice: "admin",
  bob: "member",
};

// With noUncheckedIndexedAccess: true, accessing an arbitrary key produces:
// string | undefined (NOT string!)
const role = userRoles["charlie"];

// TypeScript forces safe narrowing:
if (role !== undefined) {
  console.log(`Role: ${role.toUpperCase()}`);
} else {
  console.log("User has no assigned role");
}
```

### Example 2: Hardened Process Signal & Exception Trapping
```typescript
import { logger } from "./logger.js";

// Trap unhandled promise rejections before they crash the Node.js process
process.on("unhandledRejection", (reason, promise) => {
  logger.fatal({ reason, promise }, "Unhandled Promise Rejection encountered");
  process.exit(1);
});

process.on("uncaughtException", (error) => {
  logger.fatal({ err: error }, "Uncaught Exception fatal crash");
  process.exit(1);
});
```

### Example 3: Edge Case - Secret Masking in Error Logs
```typescript
export function maskApiKey(key: string): string {
  if (key.length <= 8) return "********";
  const start = key.slice(0, 4);
  const end = key.slice(-4);
  return `${start}****${end}`;
}
```

### Example 4: Compiler Error & Unchecked Index Violation
```typescript
const numbers: number[] = [10, 20, 30];

// Under noUncheckedIndexedAccess: true, element is number | undefined:
const val: number | undefined = numbers[5];

// @ts-expect-error - TS2322: Type 'number | undefined' is not assignable to type 'number'.
const unsafeVal: number = numbers[5];
```

### Example 5: Automated Pre-Deployment Security Audit Runner
```typescript
import { execSync } from "node:child_process";

export function runSecurityAudit() {
  console.log("Executing Enterprise Security Audit...");

  const checks = [
    { name: "Compiler Strictness", cmd: "pnpm tsc --noEmit" },
    { name: "Lint Guardrails", cmd: "pnpm eslint . --max-warnings 0" },
    { name: "Dependency Audit", cmd: "pnpm audit --audit-level=high" },
  ];

  for (const check of checks) {
    process.stdout.write(`Running ${check.name}... `);
    try {
      execSync(check.cmd, { stdio: "ignore" });
      console.log("PASSED");
    } catch {
      console.error("FAILED! Deployment aborted.");
      process.exit(1);
    }
  }
  console.log("All security gates passed successfully!");
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`noUncheckedIndexedAccess` with Array Methods**:
   While `arr[0]` is typed as `T | undefined`, methods like `arr.forEach` or `for (const x of arr)` correctly infer `x` as `T`, because the loop iteration is guaranteed to exist.
2. **Third-Party Ambient Side Effects**:
   Certain utility libraries still augment `String.prototype`. Audit your lockfile to prevent unintended global prototype pollution.
3. **Environment Leakage into Client Bundles**:
   Never prefix server-side database passwords or API keys with public bundler prefixes like `NEXT_PUBLIC_` or `VITE_`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Loose Compiler Flags** | `"strict": false` | `"strict": true` | Type holes pass unnoticed |
| **Unchecked Arrays** | `noUncheckedIndexedAccess: false` | Set to `true` | `Cannot read property of undefined` |
| **Root Docker User** | Running container as root | `USER node` | Container breakout vulnerability |
| **Leaking Secrets in Logs** | Logging raw headers/passwords | Pino `redact` filter | PII and token exposure |
| **Missing Unhandled Rejection Trap** | Ignoring process events | Log fatal and restart | Silent thread hang / crash |
| **Broad Type Suppressions** | Using `@ts-ignore` everywhere | Use `@ts-expect-error` with issue tag | Unmaintained dead suppressions |

---

## 8. Performance & Type-Checker Complexity

- Enabling all strict flags increases compiler type-checking duration by only **~5%–10%**, but eliminates up to **80% of common production runtime exceptions**.

---

## 9. Compiler Internals

When `noUncheckedIndexedAccess` is active:
1. `ts.TypeChecker` modifies the return type of element access expressions (`node[index]`).
2. It automatically synthesizes a union type: `T | undefined`.
3. Control flow analysis requires an explicit truthiness or `!== undefined` branch before permitting property accesses on the element.

---

## 10. Best Practices & Design Guidelines

1. **Enable the Complete Strict Suite in `tsconfig.json`**.
2. **Treat All Inputs as Untrusted**: Parse with Zod schemas using `.strict()`.
3. **Block Script Execution during Package Installs**: Use `ignore-scripts=true`.
4. **Deploy Containers as Unprivileged Users**: Protect the host infrastructure.

---

## 11. Real-World Case Study

A tier-1 fintech application underwent an external SOC2 Type II compliance and penetration test audit.

**Audit Findings**:
- 14 instances of `any` casts in transaction handling.
- Lack of array bounds checking (`noUncheckedIndexedAccess` disabled) causing potential null pointer exceptions in stock trade processing.
- Container running as root.

**Remediation**:
- Configured hardened `tsconfig.json` (`strict: true`, `noUncheckedIndexedAccess: true`).
- Enforced zero `any` policy via typescript-eslint.
- Added non-root `USER node` in Dockerfile.

**Result**: Passed SOC2 audit with zero critical or high findings.

---

## 12. Interview Questions

<details>
<summary>1. What does `noUncheckedIndexedAccess` do in TypeScript?</summary>

It changes the type of property access on indexed types (arrays, tuples, and `Record<string, T>`) from `T` to `T | undefined`. This forces developers to handle the possibility that a given key or array index does not exist before accessing properties on it.
</details>

<details>
<summary>2. Why should production Docker containers run as user `node` rather than `root`?</summary>

Running as root inside a container increases the risk of container escape attacks. If an attacker exploits a vulnerability in the application or Node.js runtime, they gain root access to the host machine. Running as an unprivileged user limits the blast radius.
</details>

<details>
<summary>3. Why is `exactOptionalPropertyTypes` important for security?</summary>

Without this setting, TypeScript treats `{ key?: string }` as allowing `{ key: undefined }`. With `exactOptionalPropertyTypes: true`, the property must either be omitted completely or assigned a valid string, preventing ambiguous data states.
</details>

<details>
<summary>4. What is defense-in-depth in a TypeScript backend?</summary>

Defense-in-depth means applying multiple redundant layers of security: strict compiler flags, AST static analysis, runtime schema validation at boundaries, secure package installation policies, and unprivileged runtime environments.
</details>

<details>
<summary>5. How can environment variables be safely handled in enterprise applications?</summary>

By validating all required variables at application startup using a runtime schema validator (such as Zod) and failing fast if any required secret or port is missing or malformed.
</details>

<details>
<summary>6. How does typescript-eslint help enforce security?</summary>

It detects unsafe patterns that the compiler allows, such as floating promises (`@typescript-eslint/no-floating-promises`), unsafe casts (`@typescript-eslint/consistent-type-assertions`), and explicit `any` usage.
</details>

<details>
<summary>7. Why should you prefer `@ts-expect-error` over `@ts-ignore`?</summary>

`@ts-expect-error` will fail compilation if no error is present on the following line. This prevents stale suppression comments from lingering after the underlying code has been refactored or fixed.
</details>

<details>
<summary>8. How do you prevent secret keys from leaking in application logs?</summary>

By configuring log redaction paths in structured loggers like Pino (`redact: ['password', 'authorization', 'secret']`) and masking API keys before outputting error payloads.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Enable noUncheckedIndexedAccess in tsconfig</summary>

**Task**: Add the option to `tsconfig.json` and fix an array access.

**Solution**:
```json
{
  "compilerOptions": {
    "noUncheckedIndexedAccess": true
  }
}
```
Fix:
```typescript
const items = ["a", "b"];
const first = items[0];
if (first) {
  console.log(first.toUpperCase());
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Hardened Dockerfile user</summary>

**Task**: Ensure a Node.js Dockerfile switches to an unprivileged user.

**Solution**:
```dockerfile
FROM node:22-alpine
WORKDIR /app
COPY --chown=node:node dist/ ./dist
USER node
CMD ["node", "dist/index.js"]
```
</details>

<details>
<summary>Challenge 3 (Medium): Safe environment configuration loader</summary>

**Task**: Author an environment config module that validates `PORT`, `NODE_ENV`, and `API_KEY` using Zod and exits on error.

**Solution**:
```typescript
import { z } from "zod";

const EnvSchema = z.object({
  PORT: z.coerce.number().default(3000),
  NODE_ENV: z.enum(["development", "production", "test"]),
  API_KEY: z.string().min(16),
});

export const env = EnvSchema.parse(process.env);
```
</details>

<details>
<summary>Challenge 4 (Medium): Safe Record Lookup Function</summary>

**Task**: Implement a helper `getSafe<T>(record: Record<string, T>, key: string): T` that throws an explicit error if the key is missing.

**Solution**:
```typescript
export function getSafe<T>(record: Record<string, T>, key: string): T {
  const value = record[key];
  if (value === undefined) {
    throw new Error(`Required configuration key '${key}' not found`);
  }
  return value;
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Zero-Trust Webhook Ingress Verifier</summary>

**Task**: Implement a typed webhook handler that verifies cryptographic HMAC SHA-256 signatures before parsing the payload with Zod.

**Solution**:
```typescript
import crypto from "node:crypto";
import { z } from "zod";

export const WebhookEventSchema = z.object({
  id: z.string(),
  type: z.enum(["order.created", "order.paid"]),
  timestamp: z.number(),
}).strict();

export function verifyAndParseWebhook(
  rawBody: string,
  signature: string,
  secret: string
) {
  const computed = crypto.createHmac("sha256", secret).update(rawBody).digest("hex");
  if (!crypto.timingSafeEqual(Buffer.from(computed), Buffer.from(signature))) {
    throw new Error("Invalid HMAC signature");
  }

  const parsedJson: unknown = JSON.parse(rawBody);
  return WebhookEventSchema.parse(parsedJson);
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Automated Security Health Gate Script</summary>

**Task**: Author a Node.js pre-flight script that verifies tsconfig strictness, checks lockfile existence, and checks for banned packages (`ts-node`, `babel-cli` in production dependencies).

**Solution**:
```typescript
import fs from "node:fs";

export function preFlightSecurityCheck() {
  const tsconfig = JSON.parse(fs.readFileSync("tsconfig.json", "utf-8"));
  if (!tsconfig.compilerOptions?.strict) {
    throw new Error("Security Violation: 'strict: true' is missing in tsconfig.json");
  }

  const pkg = JSON.parse(fs.readFileSync("package.json", "utf-8"));
  const banned = ["ts-node", "babel-cli", "nodemon"];
  for (const dep of Object.keys(pkg.dependencies ?? {})) {
    if (banned.includes(dep)) {
      throw new Error(`Security Violation: Banned dependency '${dep}' found in production dependencies!`);
    }
  }

  console.log("Pre-flight security check passed successfully!");
}
```
</details>
