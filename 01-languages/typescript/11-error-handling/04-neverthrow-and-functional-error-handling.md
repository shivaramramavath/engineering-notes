# neverthrow and Functional Error Handling in TypeScript

> Practical guide to type-safe error handling using the `neverthrow` library, mastering `ok`, `err`, `ResultAsync`, `fromPromise`, `fromThrowable`, `safeTry`, and functional composition.

| Metadata | Details |
| :--- | :--- |
| **Category** | Error Handling |
| **Difficulty** | Advanced |
| **Prerequisites** | [11-error-handling/02-result-pattern.md](./02-result-pattern.md), [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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
While implementing custom Result types is straightforward, production applications require battle-tested primitives for asynchronous pipelines, wrapping third-party promises, and managing generator-based monadic syntax.

`neverthrow` is the premier lightweight functional error handling library for TypeScript. It encodes success and failure directly into the type signature without relying on heavy functional programming frameworks like `fp-ts`. By using `neverthrow`'s `Result<T, E>` and `ResultAsync<T, E>`, teams eliminate unhandled promise rejections, make every failure explicit, and compose complex business workflows linearly.

---

## 2. Core Concepts
- **`ok(value)` and `err(error)`**: Factory methods creating instances of `Ok<T, E>` and `Err<T, E>`.
- **`ResultAsync<T, E>`**: A specialized promise wrapper that models asynchronous operations returning a `Result`. Unlike a raw `Promise<Result<T, E>>`, `ResultAsync` exposes synchronous-like monadic operators (`map`, `andThen`, `mapErr`) directly without requiring nested `await` calls.
- **`fromThrowable`**: Wraps dangerous synchronous functions that might throw (like `JSON.parse`) into functions returning a `Result`.
- **`ResultAsync.fromPromise`**: Wraps promises from external libraries (like `fetch` or database drivers), requiring a compulsory error mapper function to turn unknown exceptions into typed errors.
- **`safeTry`**: A generator-based runner (similar to Rust's `?` operator or Haskell's `do` notation) that short-circuits on `Err` without nested callbacks.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|              Third-Party Async Function (fetch)             |
|                    May reject with unknown                  |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|              ResultAsync.fromPromise(promise, mapErr)       |
|            Catches rejection; passes to mapErr(e)           |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                   ResultAsync<Response, HttpError>          |
+------------------------------+------------------------------+
                               |
               .andThen(res => parseJson(res))
                               |
                               v
+-------------------------------------------------------------+
|                  ResultAsync<UserDto, AppError>             |
|         Short-circuits immediately if HttpError occurs      |
+-------------------------------------------------------------+
```

`ResultAsync` internally holds a native `Promise<Result<T, E>>`. When you invoke `.andThen(nextFn)`, it hooks into the internal promise chain with `.then()`, checking if the previous result was an `Err`. If it was an `Err`, it immediately returns another `Err` without invoking `nextFn`.

---

## 4. Syntax & API Reference

```typescript
import { ok, err, Result, ResultAsync, fromThrowable, fromPromise, safeTry } from 'neverthrow';

// 1. Basic synchronous creation
const success: Result<number, string> = ok(100);
const failure: Result<number, string> = err('Invalid input');

// 2. Converting throwing functions
const safeJsonParse = fromThrowable(
  JSON.parse,
  (caught: unknown) => new Error(`JSON parse error: ${String(caught)}`)
);

// 3. Wrapping asynchronous promises
function fetchUser(id: string): ResultAsync<UserData, FetchError> {
  return ResultAsync.fromPromise(
    fetch(`https://api.example.com/users/${id}`).then(r => r.json()),
    (err: unknown) => ({ code: 'NETWORK_ERROR', raw: err })
  );
}

// 4. Generator do-notation with safeTry
const workflow = safeTry(function* () {
  const a = yield* ok(10).safeUnwrap();
  const b = yield* ok(20).safeUnwrap();
  return ok(a + b);
});
```

---

## 5. Practical Examples

### Example 1: Type-Safe JSON File Reader
```typescript
import { ok, err, Result, fromThrowable } from 'neverthrow';
import * as fs from 'fs';

type FileError =
  | { type: 'FS_ERROR'; message: string }
  | { type: 'PARSE_ERROR'; message: string };

function readJsonConfig<T>(filePath: string): Result<T, FileError> {
  const readResult = Result.fromThrowable(
    () => fs.readFileSync(filePath, 'utf8'),
    (err): FileError => ({ type: 'FS_ERROR', message: String(err) })
  )();

  return readResult.andThen((content) => {
    return Result.fromThrowable(
      () => JSON.parse(content) as T,
      (err): FileError => ({ type: 'PARSE_ERROR', message: String(err) })
    )();
  });
}
```

### Example 2: Complete User Registration Pipeline with ResultAsync
```typescript
import { ok, err, Result, ResultAsync } from 'neverthrow';

interface User { id: string; email: string }
type AuthError =
  | { kind: 'INVALID_EMAIL'; email: string }
  | { kind: 'USER_EXISTS'; email: string }
  | { kind: 'DB_ERROR'; detail: string };

function validateEmail(email: string): Result<string, AuthError> {
  return email.includes('@') ? ok(email) : err({ kind: 'INVALID_EMAIL', email });
}

function checkUserUnique(email: string): ResultAsync<string, AuthError> {
  return ResultAsync.fromPromise(
    Promise.resolve(email === 'existing@test.com' ? false : true),
    (e): AuthError => ({ kind: 'DB_ERROR', detail: String(e) })
  ).andThen(isUnique =>
    isUnique ? ok(email) : err({ kind: 'USER_EXISTS', email })
  );
}

function insertUser(email: string): ResultAsync<User, AuthError> {
  return ResultAsync.fromPromise(
    Promise.resolve({ id: 'usr_123', email }),
    (e): AuthError => ({ kind: 'DB_ERROR', detail: String(e) })
  );
}

export function registerUser(rawEmail: string): ResultAsync<User, AuthError> {
  return validateEmail(rawEmail)
    .asyncAndThen(checkUserUnique)
    .andThen(insertUser);
}
```

### Example 3: Edge Case - Combining Multiple Parallel ResultAsyncs
```typescript
import { ResultAsync, ok } from 'neverthrow';

function fetchProfile(id: string): ResultAsync<{ name: string }, string> {
  return ResultAsync.fromPromise(Promise.resolve({ name: 'Alice' }), () => 'Failed profile');
}

function fetchPermissions(id: string): ResultAsync<string[], string> {
  return ResultAsync.fromPromise(Promise.resolve(['READ', 'WRITE']), () => 'Failed permissions');
}

// ResultAsync.combine waits for all in parallel and combines into a tuple
export function getUserDashboard(userId: string): ResultAsync<[ { name: string }, string[] ], string> {
  return ResultAsync.combine([
    fetchProfile(userId),
    fetchPermissions(userId),
  ]);
}
```

### Example 4: Compiler Failure Case - Handling Missing Error Branch
```typescript
import { ok, err, Result } from 'neverthrow';

const r: Result<number, string> = ok(42);

// @ts-expect-error TS2345: Argument of type '{ ok: (val: number) => number; }' is not assignable to parameter of type '{ ok: (val: number) => number; err: (err: string) => number; }'.
r.match({
  ok: val => val * 2,
});
```

### Example 5: Idiomatic safeTry Generator Syntax
```typescript
import { ok, err, safeTry, Result } from 'neverthrow';

function stepA(n: number): Result<number, string> {
  return n > 0 ? ok(n * 2) : err('Must be positive');
}

function stepB(n: number): Result<string, string> {
  return n < 100 ? ok(`Value: ${n}`) : err('Too large');
}

export function computeWorkflow(initial: number): Result<string, string> {
  return safeTry(function* () {
    const a = yield* stepA(initial).safeUnwrap();
    const b = yield* stepB(a).safeUnwrap();
    return ok(b);
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Unwrapped ResultAsync Execution**: A `ResultAsync` will not execute until it is awaited or `.then()` is invoked on it. Forgetting to `await resultAsync` means the background async task executes unmanaged.
2. **Missing `safeUnwrap()` in `safeTry`**: In `safeTry`, calling `yield* result` without `.safeUnwrap()` produces a generator type error.
3. **Synchronous Throws in Promise Callbacks**: If the function passed to `fromPromise` throws synchronously before producing a Promise, `fromPromise` catches it only if the engine reaches the promise constructor. Always verify that callers pass a promise or factory.
4. **ResultAsync.combine Fails Fast**: When combining multiple `ResultAsync`s, the first one to produce an `Err` causes the returned `ResultAsync` to reject with that single `Err`. It does not accumulate all errors unless `combineWithAllErrors` is used.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting `await` on `ResultAsync` | Unhandled dangling promise | Always `await` the `ResultAsync` |
| Calling `.match()` without `err` handler | **TS2345**: Property 'err' missing | Provide both `ok` and `err` matchers |
| Using `_unsafeUnwrap()` in production | Throws runtime exception if Result is Err | Use `unwrapOr` or pattern matching |
| Wrapping async functions with `fromThrowable` | Produces `Result<Promise<T>, E>` | Use `ResultAsync.fromPromise` |
| Ignoring typed error unions | Downgrades error types to generic `string` | Define explicit discriminated union for errors |

---

## 8. Performance & Type-Checker Complexity
- `neverthrow` has zero dependencies and a minified bundle size under 3KB.
- In benchmarks, `neverthrow` overhead compared to raw JavaScript promises is under 5%, while avoiding the severe de-optimization penalties of unhandled throws.
- Generics in `ResultAsync` use straightforward covariance, maintaining $\mathcal{O}(1)$ type checking in TSServer without deep recursive instantiation.

---

## 9. Compiler Internals
In `neverthrow`, `Ok<T, E>` and `Err<T, E>` are implemented as class hierarchies sharing a common interface with an `isOk(): this is Ok<T, E>` and `isErr(): this is Err<T, E>` custom type guard predicate. When calling `if (res.isOk())`, TypeScript's control-flow analyzer narrows `this` to `Ok<T, E>`, enabling safe access to `.value`.

---

## 10. Best Practices & Design Guidelines
- Replace raw `Promise<T>` return signatures in services with `ResultAsync<T, DomainError>`.
- Convert third-party libraries (SDKs, databases, HTTP clients) at the boundary using `ResultAsync.fromPromise`.
- Use `safeTry` to keep multi-step business logic flat and readable without callback nesting.
- Avoid `.match()` deep inside domain layers; bubble Results up to controller boundaries.
- Reserve `.unwrapOr(defaultVal)` for non-critical user-interface defaults.

---

## 11. Real-World Case Study
A fin-tech remittance pipeline processed international bank transfers across four disparate payment gateways. Network errors and validation exceptions were causing partial database updates, leading to ledger discrepancies.

**Refactored with neverthrow:**
```typescript
import { ResultAsync, ok, err } from 'neverthrow';

type RemittanceError =
  | { step: 'COMPLIANCE'; reason: string }
  | { step: 'FX_LOCK'; rateExpired: boolean }
  | { step: 'SETTLEMENT'; gatewayCode: string };

function verifyCompliance(txId: string): ResultAsync<void, RemittanceError> {
  return ResultAsync.fromPromise(
    checkSanctionsList(txId),
    (): RemittanceError => ({ step: 'COMPLIANCE', reason: 'Sanctions check failed' })
  );
}

function lockFxRate(txId: string): ResultAsync<number, RemittanceError> {
  return ResultAsync.fromPromise(
    requestFxQuote(txId),
    (): RemittanceError => ({ step: 'FX_LOCK', rateExpired: true })
  );
}

export function processRemittance(txId: string): ResultAsync<string, RemittanceError> {
  return verifyCompliance(txId)
    .andThen(() => lockFxRate(txId))
    .map(rate => `Transfer ${txId} cleared at rate ${rate}`);
}

async function checkSanctionsList(_id: string) { /* ... */ }
async function requestFxQuote(_id: string): Promise<number> { return 1.08; }
```
Transactions either succeed completely or short-circuit safely at the exact failing step with atomic rollback, reducing ledger reconciliation incidents to zero.

---

## 12. Interview Questions

<details>
<summary>1. What advantage does `neverthrow` provide over traditional `try/catch` in TypeScript?</summary>

It forces error handling to be explicit at the type level. With `try/catch`, functions do not declare what exceptions they throw, making it easy to forget error handling. With `neverthrow`, the return type `Result<T, E>` cannot be unpacked without handling both success and failure cases.
</details>

<details>
<summary>2. What is `ResultAsync<T, E>` and why is it preferred over `Promise<Result<T, E>>`?</summary>

`ResultAsync` provides chainable monadic methods (`map`, `andThen`, `mapErr`) directly on the asynchronous wrapper, eliminating the need to write `.then(res => res.andThen(...))` at every step.
</details>

<details>
<summary>3. What is the difference between `.map()` and `.andThen()` in `neverthrow`?</summary>

`.map()` transforms the value `T` into another value `U` using a synchronous function `(val: T) => U`. `.andThen()` takes a function that itself returns a new `Result<U, E>` and flattens the result, preventing nested `Result<Result<U, E>, E>`.
</details>

<details>
<summary>4. How does `neverthrow` convert an unsafe third-party function that throws?</summary>

Using `fromThrowable(fn, errorMapper)`. It returns a wrapped function that catches any synchronous exception thrown by `fn` and returns it as an `Err` formatted by `errorMapper`.
</details>

<details>
<summary>5. What does `safeTry` do in `neverthrow`?</summary>

It implements a generator-based syntax (coroutine) that allows developers to write sequential lines of code using `yield* res.safeUnwrap()`. If any step evaluates to an `Err`, the entire generator immediately returns that `Err`, mimicking Rust's `?` operator.
</details>

<details>
<summary>6. Why is `_unsafeUnwrap()` prefixed with an underscore in `neverthrow`?</summary>

To signal that it is an unsafe escape hatch. If called on an `Err` instance, it throws an unhandled exception at runtime. It should only be used in unit tests or when success is mathematically guaranteed.
</details>

<details>
<summary>7. How can you execute multiple `ResultAsync` tasks concurrently?</summary>

Using `ResultAsync.combine([task1, task2, ...])`. It returns a single `ResultAsync` with a tuple of results if all succeed, or short-circuits on the first failure.
</details>

<details>
<summary>8. How do you map an array of inputs through a `Result`-returning function?</summary>

Using `Result.combine(inputs.map(fn))`. If all items succeed, it returns `Ok<T[]>`. If any fail, it returns the first `Err`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Safe Number Parser**: Using `fromThrowable`, create a function `safeParseFloat(input: string): Result<number, Error>` that parses floating point numbers safely.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { ok, err, Result } from 'neverthrow';

   function safeParseFloat(input: string): Result<number, Error> {
     const val = Number.parseFloat(input);
     return Number.isNaN(val)
       ? err(new Error(`Failed to parse '${input}' to float`))
       : ok(val);
   }
   ```
   </details>

2. **Error Mapper**: Write a function that takes a `Result<number, string>` and converts the error string to uppercase using `.mapErr()`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Result } from 'neverthrow';

   function uppercaseError(r: Result<number, string>): Result<number, string> {
     return r.mapErr(e => e.toUpperCase());
   }
   ```
   </details>

### Medium
3. **Sequential Pipeline with `andThen`**: Build a 3-step validation pipeline for a password string (must be at least 8 chars, must contain a number, must contain an uppercase letter) using `neverthrow`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { ok, err, Result } from 'neverthrow';

   function checkLength(p: string): Result<string, string> {
     return p.length >= 8 ? ok(p) : err('Password too short (min 8)');
   }

   function checkNumber(p: string): Result<string, string> {
     return /\d/.test(p) ? ok(p) : err('Must contain at least one digit');
   }

   function checkUppercase(p: string): Result<string, string> {
     return /[A-Z]/.test(p) ? ok(p) : err('Must contain uppercase character');
   }

   export function validatePassword(p: string): Result<string, string> {
     return checkLength(p)
       .andThen(checkNumber)
       .andThen(checkUppercase);
   }
   ```
   </details>

4. **Async API Fetcher**: Wrap `fetch` to create `safeFetchJson<T>(url: string): ResultAsync<T, string>`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { ResultAsync } from 'neverthrow';

   function safeFetchJson<T>(url: string): ResultAsync<T, string> {
     return ResultAsync.fromPromise(
       fetch(url).then(async res => {
         if (!res.ok) throw new Error(`HTTP status ${res.status}`);
         return (await res.json()) as T;
       }),
       err => (err instanceof Error ? err.message : String(err))
     );
   }
   ```
   </details>

### Hard
5. **Parallel Validation Aggregator**: Write a function `validateAllFields(user: { name: string; age: number; email: string })` that runs independent validations on each field and returns `Result<ValidUser, string[]>` aggregating ALL errors instead of failing on the first error.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { ok, err, Result, ResultAsync } from 'neverthrow';

   interface RawUser { name: string; age: number; email: string }
   interface ValidUser { name: string; age: number; email: string }

   function validateAllFields(u: RawUser): Result<ValidUser, string[]> {
     const errors: string[] = [];
     if (!u.name.trim()) errors.push('Name cannot be empty');
     if (u.age < 18) errors.push('Age must be at least 18');
     if (!u.email.includes('@')) errors.push('Invalid email address');

     return errors.length === 0 ? ok(u) : err(errors);
   }
   ```
   </details>

6. **Full Flow with `safeTry` and Rollback**: Implement an order checkout flow using `neverthrow`'s `safeTry` that debits wallet, reduces inventory, and charges tax; if tax computation fails, it triggers a rollback action.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { ok, err, safeTry, Result } from 'neverthrow';

   function debitWallet(amount: number): Result<number, string> {
     return amount <= 500 ? ok(500 - amount) : err('Insufficient balance');
   }

   function calculateTax(amount: number): Result<number, string> {
     return amount > 0 ? ok(amount * 0.1) : err('Invalid tax calculation');
   }

   function refundWallet(amount: number): void {
     console.log(`Refunded ${amount}`);
   }

   export function checkoutFlow(total: number): Result<{ finalAmount: number }, string> {
     return safeTry(function* () {
       const balance = yield* debitWallet(total).safeUnwrap();
       const taxResult = calculateTax(total);

       if (taxResult.isErr()) {
         refundWallet(total);
         return err(taxResult.error);
       }

       return ok({ finalAmount: total + taxResult.value });
     });
   }
   ```
   </details>
