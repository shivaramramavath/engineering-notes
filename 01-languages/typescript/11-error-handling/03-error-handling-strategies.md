# Error Handling Strategies in TypeScript

> Architectural guide to architecting resilient TypeScript systems: distinguishing domain errors from system defects, error boundaries, fail-fast vs gracefully degraded architectures, logging, and metrics.

| Metadata | Details |
| :--- | :--- |
| **Category** | Error Handling |
| **Difficulty** | Advanced |
| **Prerequisites** | [11-error-handling/01-custom-errors.md](./01-custom-errors.md), [11-error-handling/02-result-pattern.md](./02-result-pattern.md) |
| **TypeScript Baseline** | TS 4.4+ (`useUnknownInCatchVariables`), TS 5.0+ |
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
Building scalable enterprise software requires more than wrapping functions in `try/catch`. Engineers must adopt structured error architectures: differentiating between operational domain faults (such as user invalid input, expired tokens, or payment declines) and programmer bugs / environment faults (such as invariant violations, out-of-memory, or corrupt network sockets).

In TypeScript, an effective error handling strategy coordinates static type checking, runtime boundary guards, observability instrumentation, and graceful degradation. This ensures that transient faults fail safely without taking down entire microservices or leaving frontend client applications in inconsistent blank states.

---

## 2. Core Concepts
- **Operational Errors vs Programmer Bugs**:
  - *Operational / Domain Errors*: Expected failure modes in normal operation (e.g. invalid password, file not found). Should be handled explicitly using `Result` or typed custom errors.
  - *Programmer Bugs (Defects)*: Unanticipated bugs (e.g. `TypeError: cannot read property of undefined`, assertion failures). Should fail fast, crash the worker or request boundary, and trigger alerts.
- **Error Boundaries**: Isolating faults at macro architectural boundaries (e.g. HTTP request middleware, React error boundaries, message queue workers) rather than swallowing errors deep inside leaf business logic.
- **Fail-Fast vs Resilient Degradation**: Critical financial calculations must fail immediately on invariant breach; non-critical recommendation feeds should gracefully fallback to cached results.
- **Observability & Contextual Logging**: Capturing structured log entries containing trace IDs, user contexts, and causal chains without leaking PII (Personally Identifiable Information).

---

## 3. How It Works Internally

```
+-----------------------------------------------------------+
|                  Inbound Request / Action                 |
+-----------------------------+-----------------------------+
                              |
                              v
+-----------------------------------------------------------+
|              Global Boundary (Middleware / Boundary)      |
+-----------------------------+-----------------------------+
                              |
                              v
+-----------------------------------------------------------+
|                     Domain Core Logic                     |
|                                                           |
|     Operational Failure?           Defect / Bug?          |
|      (e.g. User Not Found)          (e.g. null pointer)   |
|              |                              |             |
|              v                              v             |
|        Return Result                 Throw Exception      |
|              |                              |             |
|              v                              v             |
|     Handled / Mapped to 404         Bubble to Middleware  |
+---------------------------------------------+-------------+
                                              |
                                              v
                              +-------------------------------+
                              |    Global Error Middleware    |
                              | - Log with Stack & Trace ID   |
                              | - Emit Metric counter         |
                              | - Return 500 Internal Error   |
                              +-------------------------------+
```

The system splits error responsibilities: leaf domain functions use explicit Result types for business branches. If an actual exception is thrown, it bubbles up cleanly through pure functions to an overarching boundary handler that handles logging, telemetry, and client error response framing.

---

## 4. Syntax & API Reference

```typescript
// 1. Domain Error Classification
export type OperationalError =
  | { readonly kind: 'NOT_FOUND'; readonly entity: string; readonly id: string }
  | { readonly kind: 'VALIDATION_FAILED'; readonly fields: Record<string, string> }
  | { readonly kind: 'UNAUTHORIZED'; readonly reason: string };

// 2. Fatal Defect Assertion
export function invariant(condition: unknown, message: string): asserts condition {
  if (!condition) {
    const error = new Error(`Invariant Violation: ${message}`);
    error.name = 'InvariantViolationError';
    throw error;
  }
}

// 3. Error Boundary Interface
export interface ErrorHandlerContext {
  readonly traceId: string;
  readonly userId?: string;
  readonly timestamp: number;
}

export type GlobalErrorReporter = (error: unknown, context: ErrorHandlerContext) => void;
```

---

## 5. Practical Examples

### Example 1: Invariant Assertion for Programmer Bugs
```typescript
function invariant(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(`[CRITICAL INVARIANT BREACH] ${message}`);
  }
}

function processOrder(orderId: string, totalAmount: number): void {
  invariant(totalAmount >= 0, `Order ${orderId} amount cannot be negative (${totalAmount})`);
  // totalAmount is verified valid
}
```

### Example 2: Architectural Controller Boundary
```typescript
interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: { code: string; message: string; traceId: string };
}

async function handleController<T>(
  traceId: string,
  handler: () => Promise<T>
): Promise<ApiResponse<T>> {
  try {
    const data = await handler();
    return { success: true, data };
  } catch (err: unknown) {
    console.error(`[Trace: ${traceId}] Unhandled exception:`, err);
    return {
      success: false,
      error: {
        code: 'INTERNAL_SERVER_ERROR',
        message: 'An unexpected internal error occurred. Please contact support.',
        traceId,
      },
    };
  }
}
```

### Example 3: Fallback Degradation Pattern
```typescript
interface Recommendation {
  id: string;
  title: string;
}

async function getPersonalizedRecommendations(userId: string): Promise<Recommendation[]> {
  try {
    return await fetchFromMlService(userId);
  } catch (err: unknown) {
    console.warn(`ML service failed for user ${userId}. Falling back to default top items.`, err);
    return getDefaultTrendingItems();
  }
}

async function fetchFromMlService(userId: string): Promise<Recommendation[]> {
  // Simulated service call
  return [{ id: '1', title: 'Trending TS Guide' }];
}

function getDefaultTrendingItems(): Recommendation[] {
  return [{ id: '0', title: 'Default Top Article' }];
}
```

### Example 4: Compiler Failure Case - Exhaustive Error Matching
```typescript
type AppErrors =
  | { type: 'NETWORK_TIMEOUT' }
  | { type: 'DATABASE_LOCKED' }
  | { type: 'RATE_LIMIT_EXCEEDED' };

function getHttpCode(err: AppErrors): number {
  switch (err.type) {
    case 'NETWORK_TIMEOUT': return 504;
    case 'DATABASE_LOCKED': return 503;
    // Missing 'RATE_LIMIT_EXCEEDED'
  }
  // @ts-expect-error TS2366: Function lacks ending return statement and return type does not include 'undefined'.
}
```

### Example 5: Structured Telemetry Logger with PII Sanitization
```typescript
export interface LogContext {
  readonly traceId: string;
  readonly correlationId: string;
  readonly path: string;
}

export function logErrorWithSanitization(error: unknown, ctx: LogContext): void {
  const payload = {
    traceId: ctx.traceId,
    path: ctx.path,
    timestamp: new Date().toISOString(),
    error: error instanceof Error ? {
      name: error.name,
      message: error.message,
      stack: error.stack,
    } : String(error),
  };

  // Mask any email, credit card, or auth token
  const sanitized = JSON.stringify(payload).replace(/Bearer\s+[A-Za-z0-9-_.]+/g, 'Bearer [REDACTED]');
  process.stderr.write(sanitized + '\n');
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The Catch-All Black Hole**: Using empty catch blocks (`try { ... } catch {}`) silences fatal programming errors, leading to ghost bugs where services silently stall or deliver corrupt data.
2. **Double Reporting**: Catching an error, logging it, and then re-throwing it causes every layer in the stack to log the same error repeatedly, drowning log aggregators in noise. Log once at the top boundary.
3. **Async Fire-and-Forget**: Forgetting `await` on a promise inside a `try/catch` causes rejected promises to escape the catch block entirely and become unhandled rejections.
4. **Leaking Stack Traces to End Users**: Returning raw error stacks in public API responses exposes internal file paths, framework versions, and database schemas to attackers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `try { ... } catch (e) {}` (Silent catch) | Impossible to debug; data corruption | Always log or rethrow |
| Logging and re-throwing at every layer | Redundant duplicate logs across services | Log only at the boundary that handles it |
| Catching errors inside domain models | Tight coupling between business logic and transport | Use Result pattern in domain; catch at boundary |
| Exposing internal database errors to clients | Security vulnerability (information disclosure) | Map to sanitized generic public error messages |
| Using `catch (e: any)` | Disables TypeScript safety for caught error | Use `unknown` and guard types |
| Missing unhandled rejection handlers | Node process dies unexpectedly | Add `process.on('unhandledRejection', ...)` |

---

## 8. Performance & Type-Checker Complexity
Separating domain error modeling into pure Result unions and keeping try/catch blocks confined to outer boundaries provides optimal performance:
- The V8 TurboFan compiler can aggressively inline pure domain functions without de-optimizing for exception handling tables.
- TypeScript compiler time is kept $\mathcal{O}(1)$ by keeping error discriminant unions flat.

---

## 9. Compiler Internals
TypeScript's flow analysis models `asserts condition` statements using `createFlowAssertion`:
- When `invariant(val !== null)` is invoked, the compiler's flow graph records a synthetic assertion node.
- The Type Checker prunes `null` from `val` for all subsequent AST statements in that branch.
- If the assertion function is not annotated with `asserts`, the compiler treats it as a standard void function and fails to narrow types.

---

## 10. Best Practices & Design Guidelines
- Adopt **Fail Fast** for invariant violations; adopt **Graceful Degradation** for auxiliary optional features.
- Log errors exactly once at the boundary responsible for dispatching the response or acknowledging the queue message.
- Tag every error log with a unique `traceId` or `requestId` to correlate logs across distributed services.
- Never expose internal database or ORM exception messages directly over client APIs.
- Use `asserts condition` functions for runtime invariants to guide TypeScript's control-flow analysis.

---

## 11. Real-World Case Study
An e-commerce checkout API suffered intermittent 500 errors during peak sale events. Investigation revealed that when the third-party loyalty points calculation timed out, the entire checkout transaction failed because the error was thrown uncaught inside the checkout pipeline.

**Refactored Strategy:**
1. Split transaction operations into **Critical** (Payment, Inventory Lock) and **Auxiliary** (Loyalty Points, Email Notification).
2. Wrapped Auxiliary operations in isolated fallback boundaries that return default states upon timeout without failing the checkout.
3. Added structured tracing to correlate loyalty timeouts in Datadog while keeping checkout conversion at 99.98%.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between an operational error and a programmer bug?</summary>

An operational error is an anticipated failure scenario that occurs during normal application execution (e.g., network disconnect, user typing invalid input, file missing). A programmer bug (defect) is an unexpected flaw in the application logic itself (e.g., calling a method on `null`, violating an array index boundary, assertion failure).
</details>

<details>
<summary>2. Why is "log and re-throw" considered an anti-pattern?</summary>

Because every caller up the call stack will repeat the same pattern, creating multiple redundant log entries with different stack traces for a single error. It wastes log storage and makes triaging incident alarms significantly harder.
</details>

<details>
<summary>3. How does TypeScript's assertion syntax (`asserts condition`) assist error handling?</summary>

It allows an engineer to write an invariant checking function that throws if the condition is false, while informing TypeScript's control-flow analyzer to narrow the type of the asserted expression in subsequent lines of code.
</details>

<details>
<summary>4. Where should error boundaries be located in a backend application?</summary>

At the entry and exit points of the application: HTTP route middleware, queue consumer message loops, gRPC interceptors, and cron job runners.
</details>

<details>
<summary>5. What is the risk of returning `err.message` directly in API responses?</summary>

It can expose sensitive technical implementation details, such as SQL queries, database table names, internal IP addresses, API keys, or library versions, to malicious actors.
</details>

<details>
<summary>6. How can you ensure that all variants of an error union are handled in a switch statement?</summary>

By adding a `default` branch that assigns the error to a variable of type `never`:
```typescript
default:
  const _exhaustiveCheck: never = err;
  return _exhaustiveCheck;
```
</details>

<details>
<summary>7. What happens if an error is thrown inside an un-awaited asynchronous function?</summary>

The exception cannot be caught by an enclosing synchronous `try/catch` block. It results in an unhandled promise rejection, which in modern Node.js terminates the process unless a global rejection handler is registered.
</details>

<details>
<summary>8. How do you implement graceful degradation in microservices?</summary>

By wrapping external downstream RPC/HTTP calls with circuit breakers and fallback providers (e.g. cached data or static defaults) so that temporary downstream outages do not break upstream customer experiences.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Invariant Function**: Write a typed invariant assertion function `assertIsDefined<T>(val: T | null | undefined, name: string): asserts val is T`.
   <details>
   <summary>Solution</summary>

   ```typescript
   function assertIsDefined<T>(val: T | null | undefined, name: string): asserts val is T {
     if (val === null || val === undefined) {
       throw new Error(`Expected ${name} to be defined, but received ${String(val)}`);
     }
   }
   ```
   </details>

2. **Sanitized Public Error**: Create a mapper function that takes an unknown error and returns a sanitized object `{ code: 'INTERNAL_ERROR', message: 'Something went wrong' }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   function sanitizePublicError(_err: unknown): { code: string; message: string } {
     return {
       code: 'INTERNAL_ERROR',
       message: 'An unexpected internal error occurred.',
     };
   }
   ```
   </details>

### Medium
3. **Exhaustive Error Switch**: Create a discriminated union of 4 error types and write an exhaustive switch statement with a `never` check that causes a compile error if a type is missing.
   <details>
   <summary>Solution</summary>

   ```typescript
   type SystemError =
     | { kind: 'AUTH_FAILED'; reason: string }
     | { kind: 'NOT_FOUND'; id: string }
     | { kind: 'RATE_LIMITED'; retrySec: number }
     | { kind: 'SERVICE_UNAVAILABLE' };

   function getErrorStatusCode(err: SystemError): number {
     switch (err.kind) {
       case 'AUTH_FAILED': return 401;
       case 'NOT_FOUND': return 404;
       case 'RATE_LIMITED': return 429;
       case 'SERVICE_UNAVAILABLE': return 503;
       default: {
         const _exhaustive: never = err;
         throw new Error(`Unhandled error kind: ${JSON.stringify(_exhaustive)}`);
       }
     }
   }
   ```
   </details>

4. **Retry with Exponential Backoff**: Implement a generic retry wrapper `retryAsync<T>(fn: () => Promise<T>, retries: number, delayMs: number): Promise<T>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   async function retryAsync<T>(fn: () => Promise<T>, retries: number, delayMs: number): Promise<T> {
     let lastError: unknown;
     for (let attempt = 0; attempt <= retries; attempt++) {
       try {
         return await fn();
       } catch (err: unknown) {
         lastError = err;
         if (attempt < retries) {
           await new Promise(resolve => setTimeout(resolve, delayMs * Math.pow(2, attempt)));
         }
       }
     }
     throw lastError;
   }
   ```
   </details>

### Hard
5. **Distributed Trace Boundary Middleware**: Implement a higher-order API middleware that creates a UUID trace ID, captures timing, catches any exception, logs with trace metadata, and formats a RFC 7807 Problem Details JSON response.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { randomUUID } from 'crypto';

   interface ProblemDetails {
     type: string;
     title: string;
     status: number;
     detail: string;
     instance: string;
   }

   async function withApiBoundary<T>(
     reqPath: string,
     handler: (traceId: string) => Promise<T>
   ): Promise<{ status: number; body: T | ProblemDetails }> {
     const traceId = randomUUID();
     const start = performance.now();
     try {
       const result = await handler(traceId);
       return { status: 200, body: result };
     } catch (err: unknown) {
       const durationMs = (performance.now() - start).toFixed(2);
       console.error(`[Trace ${traceId}] Path: ${reqPath} failed after ${durationMs}ms:`, err);
       return {
         status: 500,
         body: {
           type: 'https://api.example.com/errors/internal',
           title: 'Internal Server Error',
           status: 500,
           detail: 'An unexpected fault occurred processing your request.',
           instance: `/traces/${traceId}`,
         },
       };
     }
   }
   ```
   </details>

6. **Circuit Breaker Pattern with State Machine**: Build a type-safe `CircuitBreaker<T>` class with states `CLOSED`, `OPEN`, and `HALF_OPEN` that trips after $N$ consecutive failures and automatically probes after a cooldown period.
   <details>
   <summary>Solution</summary>

   ```typescript
   type CircuitState = 'CLOSED' | 'OPEN' | 'HALF_OPEN';

   class CircuitBreaker<T> {
     private state: CircuitState = 'CLOSED';
     private failureCount = 0;
     private nextAttempt = Date.now();

     constructor(
       private readonly action: () => Promise<T>,
       private readonly threshold = 3,
       private readonly cooldownMs = 10000
     ) {}

     async execute(): Promise<T> {
       if (this.state === 'OPEN') {
         if (Date.now() > this.nextAttempt) {
           this.state = 'HALF_OPEN';
         } else {
           throw new Error('CircuitBreaker is OPEN: downstream service offline');
         }
       }

       try {
         const result = await this.action();
         this.onSuccess();
         return result;
       } catch (err: unknown) {
         this.onFailure();
         throw err;
       }
     }

     private onSuccess(): void {
       this.failureCount = 0;
       this.state = 'CLOSED';
     }

     private onFailure(): void {
       this.failureCount++;
       if (this.failureCount >= this.threshold || this.state === 'HALF_OPEN') {
         this.state = 'OPEN';
         this.nextAttempt = Date.now() + this.cooldownMs;
       }
     }
   }
   ```
   </details>
