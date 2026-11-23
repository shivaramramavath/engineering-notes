# Middleware Typing and Request Augmentation in Node.js and TypeScript

> Typing middleware in Express, Fastify, and Hono: Declaration Merging for `Express.Request`, `res.locals` typing, AsyncLocalStorage for request context, and error-handling middleware.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [09-declaration-files/01-ambient-declarations.md](../09-declaration-files/01-ambient-declarations.md), [20-nodejs-backend/02-express.md](./02-express.md) |
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

In backend web frameworks, **Middleware** is a composable pipeline of functions that intercept incoming HTTP requests before they reach the terminal route handler. Middleware handles cross-cutting concerns:
- Authentication & JWT token extraction.
- Rate limiting and throttling.
- Request correlation tracing (`x-trace-id`).
- Logging and error handling.

A frequent TypeScript architectural requirement is **Request Context Augmentation**: after authentication middleware runs, downstream handlers expect `req.user` to exist with strong typing:
```typescript
// Downstream controller:
app.get('/me', (req, res) => {
  // How does TypeScript know req.user exists and what shape it has?
  res.json({ id: req.user.id });
});
```

Without proper typing, developers resort to `(req as any).user`, destroying type safety. This note covers the three primary solutions:
1. **TypeScript Declaration Merging** on `Express.Request`.
2. **`res.locals` Typing**.
3. **Node.js `AsyncLocalStorage`** (Context without modifying request objects).

```
+-----------------------------------------------------------+
|               Express Request Augmentation                |
|                                                           |
|   Incoming HTTP Request                                   |
|             |                                             |
|             v                                             |
|   authMiddleware: verifies JWT, sets req.user = payload   |
|             |                                             |
|             v                                             |
|   Declaration Merging:                                    |
|   declare global {                                        |
|     namespace Express {                                   |
|       interface Request { user: AuthenticatedUser; }      |
|     }                                                     |
|   }                                                       |
|             v                                             |
|   Route Handler: req.user.id is 100% type-checked!        |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Declaration Merging for `Express.Request`
By creating an ambient `.d.ts` file in your project:
```typescript
// src/types/express.d.ts
export interface UserSession {
  userId: string;
  email: string;
  roles: string[];
}

declare global {
  namespace Express {
    interface Request {
      user?: UserSession;
      traceId: string;
    }
  }
}
```
*Note*: `user` must be marked as **optional** (`user?: UserSession`) because public routes (like `/login` or `/health`) execute without the auth middleware, meaning `req.user` will be `undefined`!

### 2. Typing `res.locals`
Express provides `res.locals` as an object scoped to the current request lifecycle. You can type it globally:
```typescript
declare global {
  namespace Express {
    interface Locals {
      startTime: number;
      tenantId: string;
    }
  }
}
```

### 3. Node.js `AsyncLocalStorage` (Context Anywhere)
`AsyncLocalStorage` stores request context in an asynchronous execution store, accessible anywhere in the call stack (including deep inside database repositories) without passing `req` down through function arguments!

---

## 3. How It Works Internally

During compilation, TypeScript gathers all interface declarations with the identical namespace and identifier (`Express.Request`).
The compiler merges their member lists into a single unified interface. Any file that imports Express sees the combined property definitions.

---

## 4. Syntax & API Reference

### Express Error Middleware Signature
An Express error handler MUST accept **exactly 4 parameters**, or Express will treat it as a standard request handler:
```typescript
import type { ErrorRequestHandler } from 'express';

export const errorHandler: ErrorRequestHandler = (err, req, res, next) => {
  res.status(500).json({ error: err.message });
};
```

---

## 5. Practical Examples

### Example 1: Basic Authentication Middleware with Declaration Merging
```typescript
import type { Request, Response, NextFunction } from 'express';

export interface AuthUser {
  id: string;
  role: 'admin' | 'member';
}

declare global {
  namespace Express {
    interface Request {
      user?: AuthUser;
    }
  }
}

export function requireAuth(req: Request, res: Response, next: NextFunction): void {
  const token = req.headers['authorization'];
  if (!token) {
    res.status(401).json({ error: 'Unauthorized' });
    return;
  }

  // Simulated verification
  req.user = { id: 'usr_123', role: 'member' };
  next();
}
```

### Example 2: Real-World Type-Safe Context via `AsyncLocalStorage`
```typescript
import { AsyncLocalStorage } from 'node:async_hooks';
import type { Request, Response, NextFunction } from 'express';

export interface RequestContext {
  traceId: string;
  userId?: string;
}

export const requestContextStorage = new AsyncLocalStorage<RequestContext>();

export function traceMiddleware(req: Request, res: Response, next: NextFunction): void {
  const traceId = (req.headers['x-trace-id'] as string) || crypto.randomUUID();
  res.setHeader('x-trace-id', traceId);

  requestContextStorage.run({ traceId }, () => {
    next();
  });
}

// Can be called anywhere deep in services without passing req!
export function getTraceId(): string {
  return requestContextStorage.getStore()?.traceId ?? 'unknown';
}
```

### Example 3: Edge Case - Strict Guard for Required User
```typescript
// Route handler helper that asserts user is non-null
export function assertAuthenticated(req: Request): asserts req is Request & { user: AuthUser } {
  if (!req.user) {
    throw new Error('User is not authenticated');
  }
}

export function profileHandler(req: Request, res: Response) {
  assertAuthenticated(req);
  // req.user is narrowed to AuthUser (non-optional!)
  res.json({ id: req.user.id });
}
```

### Example 4: Failure Case - Missing 4th Argument in Error Middleware
```typescript
import type { Request, Response, NextFunction } from 'express';

// @ts-expect-error TS2345: Express requires 4 arguments (err, req, res, next) for error handlers!
app.use((err: Error, req: Request, res: Response) => {
  // If next is omitted, Express treats this as a regular 3-param handler, ignoring errors!
});
```

### Example 5: Idiomatic Rate Limiting Middleware
```typescript
import type { RequestHandler } from 'express';

export function createRateLimiter(limit: number): RequestHandler {
  const counts = new Map<string, number>();

  return (req, res, next) => {
    const ip = req.ip || 'unknown';
    const current = (counts.get(ip) || 0) + 1;
    counts.set(ip, current);

    if (current > limit) {
      res.status(429).json({ error: 'Too Many Requests' });
      return;
    }
    next();
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Making `req.user` Non-Optional in Declaration Merging**:
   If you declare `interface Request { user: AuthUser; }` without `?`, TypeScript assumes `req.user` exists on **every** route. In unauthenticated routes (`/login`, `/health`), accessing `req.user.id` causes a runtime `TypeError`! Always type injected properties as optional (`user?: AuthUser`).
2. **Error Handler Parameter Count**:
   Express checks `fn.length === 4` via JavaScript reflection. If an error handler has 3 parameters, Express silently skips it during error bubbling!
3. **AsyncLocalStorage Context Loss in Detached Callbacks**:
   In older Node versions, bridging between native promises and callback-based libraries could drop `AsyncLocalStorage` context. Ensure you are on Node 18+ or wrap legacy callbacks with `AsyncLocalStorage.bind()`.
4. **Mutating Objects Across Requests**:
   Storing state directly on global objects rather than `req` or `res.locals` causes cross-request data leaks between concurrent users.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **`req.user` Non-Optional** | `interface Request { user: User; }` | `interface Request { user?: User; }` | Runtime crashes on public endpoints |
| **Error Handler with 3 Params** | `app.use((err, req, res) => ...)` | `app.use((err, req, res, next) => ...)` | Express ignores handler completely |
| **Using `(req as any).user`** | Casting `any` to read user | Declaration merging | Zero autocomplete; fragile refactoring |
| **Forgetting `return` on Early Exit** | `res.status(401).send(); next();` | `res.status(401).send(); return;` | "Cannot set headers after they are sent" |
| **Global State Mutation** | Storing request user in module variable | `req.user` or `AsyncLocalStorage` | Catastrophic session hijacking across users |

---

## 8. Performance & Type-Checker Complexity

- **AsyncLocalStorage Performance**: In Node.js 20/22, `AsyncLocalStorage` overhead is negligible (<1-2% throughput impact) due to V8 C++ optimizations.
- **Type Checking**: Declaration merging compiles instantly with zero noticeable compiler overhead.

---

## 9. Compiler Internals

When a declaration file contains:
```typescript
declare global { namespace Express { interface Request { ... } } }
```
TypeScript's symbol table merges the definitions. The file must contain at least one `export {}` statement to be treated as an ES module; otherwise, it is treated as an ambient script, which can pollute global scope unintentionally.

---

## 10. Best Practices & Design Guidelines

1. **Mark Injected Request Properties as Optional**: `user?: AuthUser`.
2. **Use Type Assertion Functions for Guards**: `assertAuthenticated(req)` narrows `req.user` cleanly.
3. **Always Include `next` in Error Handlers**: Keep the 4-parameter signature intact.
4. **Use `AsyncLocalStorage` for Logging Context**: Inject trace IDs without threading them through 10 function layers.

---

## 11. Real-World Case Study

### Distributed Tracing and Tenant Context Pipeline
A multi-tenant SaaS architecture attaching tenant DB connections and OpenTelemetry trace IDs to each request:

```typescript
import { AsyncLocalStorage } from 'node:async_hooks';
import type { Request, Response, NextFunction } from 'express';

export interface TenantContext {
  tenantId: string;
  traceId: string;
}

export const tenantStorage = new AsyncLocalStorage<TenantContext>();

export function tenantMiddleware(req: Request, res: Response, next: NextFunction): void {
  const tenantId = (req.headers['x-tenant-id'] as string) || 'default';
  const traceId = (req.headers['x-trace-id'] as string) || crypto.randomUUID();

  tenantStorage.run({ tenantId, traceId }, () => {
    next();
  });
}

// In deep database query layer:
export function getCurrentTenantId(): string {
  const store = tenantStorage.getStore();
  if (!store) throw new Error('Attempted DB query outside of tenant context');
  return store.tenantId;
}
```

---

## 12. Interview Questions

<details>
<summary>1. How do you add custom properties to the Express `Request` object in TypeScript?</summary>

Using **TypeScript Declaration Merging**. In a declaration file (`express.d.ts`), use `declare global { namespace Express { interface Request { myProp?: string; } } }`. TypeScript merges this definition with the official `@types/express` interface.
</details>

<details>
<summary>2. Why should properties added to `Express.Request` via declaration merging almost always be typed as optional (`?`)?</summary>

Because middleware runs sequentially and conditionally. On routes that do not execute that specific middleware (or before that middleware runs in the pipeline), the property will be `undefined` at runtime. Marking it as required would trick the compiler into believing it is always present, leading to runtime null-pointer exceptions.
</details>

<details>
<summary>3. Why must an Express error-handling middleware function have exactly 4 parameters?</summary>

Express inspects the function's `length` property at runtime via `fn.length`. If `fn.length === 4`, Express identifies it as an error handler `(err, req, res, next)`. If it has 2 or 3 parameters, Express treats it as a standard route handler and skips it when errors are thrown.
</details>

<details>
<summary>4. What is `AsyncLocalStorage` and why is it useful in Node.js backend development?</summary>

`AsyncLocalStorage` is a built-in Node.js module that stores data across asynchronous execution chains. It allows storing request-scoped data (such as trace IDs, current user, or database transactions) that can be retrieved anywhere down the call stack without having to pass the request object through every service and repository function.
</details>

<details>
<summary>5. What is the difference between `req.user` and `res.locals` in Express?</summary>

`req` represents the incoming HTTP request. `res.locals` is an object scoped to the response lifecycle, explicitly intended by Express for sharing data across middleware and template renderers. Both can be used for request context, but `res.locals` is specifically isolated to the active response.
</details>

<details>
<summary>6. How do you type an assertion function in TypeScript to guarantee `req.user` is defined?</summary>

Using the `asserts` return type:
```typescript
function assertAuth(req: Request): asserts req is Request & { user: User } {
  if (!req.user) throw new Error('Unauthorized');
}
```
</details>

<details>
<summary>7. What causes the "Cannot set headers after they are sent to the client" error in middleware?</summary>

Calling `res.send()` or `res.json()` and forgetting to `return`, allowing execution to continue and invoke `next()`, which causes a second handler to attempt sending another response.
</details>

<details>
<summary>8. How do you ensure an ambient `.d.ts` declaration file is treated as a module rather than a global script?</summary>

Add an `export {}` statement at the top or bottom of the declaration file.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Request Timing Middleware</summary>

**Problem:** Write middleware that measures request duration and attaches `res.setHeader('X-Response-Time', ms)`.

**Solution:**
```typescript
import type { Request, Response, NextFunction } from 'express';

export function timingMiddleware(req: Request, res: Response, next: NextFunction): void {
  const start = Date.now();
  res.on('finish', () => {
    const duration = Date.now() - start;
    console.log(`${req.method} ${req.url} - ${duration}ms`);
  });
  next();
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Correlation ID Header Middleware</summary>

**Problem:** Ensure every request has an `x-correlation-id` header, generating a UUID if missing.

**Solution:**
```typescript
import type { Request, Response, NextFunction } from 'express';

export function correlationIdMiddleware(req: Request, res: Response, next: NextFunction): void {
  const id = (req.headers['x-correlation-id'] as string) || crypto.randomUUID();
  req.headers['x-correlation-id'] = id;
  res.setHeader('x-correlation-id', id);
  next();
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Type Assertion Guard for Authenticated Request</summary>

**Problem:** Write an assertion guard function `requireAdmin(req: Request)` that verifies `req.user.role === 'admin'`.

**Solution:**
```typescript
import type { Request } from 'express';

export interface AdminUser { id: string; role: 'admin'; }

export function requireAdmin(req: Request): asserts req is Request & { user: AdminUser } {
  if (!req.user || req.user.role !== 'admin') {
    throw new Error('Forbidden: Admin access required');
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Global Express Locals Augmentation</summary>

**Problem:** Use declaration merging to add `startTime: number` and `requestId: string` to `Express.Locals`.

**Solution:**
```typescript
declare global {
  namespace Express {
    interface Locals {
      startTime: number;
      requestId: string;
    }
  }
}
export {};
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Composable Middleware Pipeline</summary>

**Problem:** Implement a function `composeMiddleware(...fns)` that executes multiple async middleware functions sequentially.

**Solution:**
```typescript
import type { Request, Response, NextFunction, RequestHandler } from 'express';

export function composeMiddleware(...middlewares: RequestHandler[]): RequestHandler {
  return (req, res, next) => {
    let index = 0;
    function dispatch(err?: any) {
      if (err) return next(err);
      if (index >= middlewares.length) return next();
      const fn = middlewares[index++];
      fn(req, res, dispatch);
    }
    dispatch();
  };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): AsyncLocalStorage Database Transaction Injector</summary>

**Problem:** Build an `AsyncLocalStorage` wrapper that makes a database transaction client accessible to any repository method during a request.

**Solution:**
```typescript
import { AsyncLocalStorage } from 'node:async_hooks';

export interface DbClient { query(sql: string): Promise<any>; }

export const txStorage = new AsyncLocalStorage<DbClient>();

export async function runInTransaction<T>(client: DbClient, fn: () => Promise<T>): Promise<T> {
  return txStorage.run(client, fn);
}

export function getActiveTransaction(defaultClient: DbClient): DbClient {
  return txStorage.getStore() || defaultClient;
}
```
</details>
