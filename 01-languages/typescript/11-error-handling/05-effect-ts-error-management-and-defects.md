# Effect-TS Error Management and Defects in TypeScript

> Deep dive into Effect-TS error architecture: the three-channel `Effect<A, E, R>`, distinguishing expected failures from defects (unhandled crashes), tagged errors, cause inspections, and fiber error recovery.

| Metadata | Details |
| :--- | :--- |
| **Category** | Error Handling |
| **Difficulty** | Advanced |
| **Prerequisites** | [11-error-handling/02-result-pattern.md](./02-result-pattern.md), [06-generics/01-generic-types.md](../06-generics/01-generic-types.md) |
| **TypeScript Baseline** | TS 5.0+, Effect 3.x+ |
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
As TypeScript codebases grow to hundreds of thousands of lines, simple `Result` types often struggle with complex concerns: concurrency, interruption, resource cleanup (`finally`), context injection (dependency injection), and distinguishing between domain-expected business failures versus catastrophic runtime defects.

**Effect** (formerly Effect-TS) is the next-generation standard library for TypeScript. It models computations as a three-parameter type: `Effect<Success, Error, Requirements>`. Error handling in Effect is mathematically sound and production-hardened: errors are tracked in the static type system, defects (panics/bugs) are isolated without killing the runtime, and fibers guarantee zero resource leaks even during sudden cancellations.

---

## 2. Core Concepts
- **The Three Channels of `Effect<Success, Error, Requirements>`**:
  - `Success` (`A`): The type returned when the computation completes normally.
  - `Error` (`E`): The type of expected, recoverable business failures (tracked in the type system).
  - `Requirements` (`R`): The ambient dependencies/services required to execute the effect.
- **Failures vs Defects**:
  - *Failures (`fail`)*: Recoverable, expected domain errors tracked in the `E` type parameter.
  - *Defects (`die`)*: Unrecoverable defects (e.g. `ReferenceError`, out of bounds, invariant violation). They bypass `E` and are stored in the runtime `Cause` data structure.
- **`Data.TaggedError`**: Creates nominal error classes with a built-in `_tag` property for exhaustive pattern matching.
- **`Effect.catchAll` vs `Effect.catchTag`**: Catching all errors versus catching a specific tagged error discriminant.
- **`Cause<E>`**: A comprehensive algebraic data structure representing all ways an Effect can end abnormally: `Fail`, `Die`, `Interrupt`, `Sequential`, or `Parallel`.

---

## 3. How It Works Internally

```
                      Effect<Success, Error, Requirements>
                                       |
                   +-------------------+-------------------+
                   |                                       |
               Execution                               Exception
                   |                                       |
                   v                                       v
         Result / Exit<A, E>                      Was it expected?
          /               \                       /              \
         v                 v                     v                v
      Success            Failure               Fail(E)          Die(Defect)
     (Value A)          (Cause<E>)            Tracked in       Untracked in E
                                             Type Signature   (Cause structure)
```

At runtime, Effect programs execute on an asynchronous fiber runtime (green threads). Every fiber has an exit state `Exit<A, E> = Success<A> | Failure<Cause<E>>`. When an error occurs, the runtime inspects whether it was created via `Effect.fail` or `Effect.die`. The `Cause` structure preserves all concurrent errors, defect stack traces, and cancellation reasons without truncating information.

---

## 4. Syntax & API Reference

```typescript
import { Effect, Data } from 'effect';

// 1. Tagged Error Definition
class UserNotFoundError extends Data.TaggedError('UserNotFoundError')<{
  readonly userId: string;
}> {}

class DatabaseError extends Data.TaggedError('DatabaseError')<{
  readonly query: string;
  readonly cause: unknown;
}> {}

// 2. Creating Failing Effects
const failEffect: Effect.Effect<never, UserNotFoundError> = Effect.fail(
  new UserNotFoundError({ userId: 'u_123' })
);

// 3. Creating Defect Effects
const defectEffect: Effect.Effect<never, never> = Effect.dieMessage(
  'Fatal invariant broken: memory corrupted'
);

// 4. Catching Tagged Errors
const handled = failEffect.pipe(
  Effect.catchTag('UserNotFoundError', err =>
    Effect.succeed(`Handled missing user: ${err.userId}`)
  )
);
```

---

## 5. Practical Examples

### Example 1: Basic Effect Pipeline with Tagged Errors
```typescript
import { Effect, Data } from 'effect';

class InvalidInputError extends Data.TaggedError('InvalidInput')<{ readonly reason: string }> {}
class NegativeNumberError extends Data.TaggedError('NegativeNumber')<{ readonly val: number }> {}

function parseNumber(str: string): Effect.Effect<number, InvalidInputError> {
  const n = Number(str);
  return Number.isNaN(n)
    ? Effect.fail(new InvalidInputError({ reason: `"${str}" is NaN` }))
    : Effect.succeed(n);
}

function ensurePositive(n: number): Effect.Effect<number, NegativeNumberError> {
  return n < 0
    ? Effect.fail(new NegativeNumberError({ val: n }))
    : Effect.succeed(n);
}

export const pipeline = (raw: string) =>
  parseNumber(raw).pipe(
    Effect.flatMap(ensurePositive),
    Effect.map(val => val * 10)
  );
```

### Example 2: Real-World Multi-Error Recovery
```typescript
import { Effect, Data } from 'effect';

class NetworkError extends Data.TaggedError('NetworkError')<{ readonly status: number }> {}
class TimeoutError extends Data.TaggedError('TimeoutError')<{ readonly ms: number }> {}

declare function fetchData(): Effect.Effect<string, NetworkError | TimeoutError>;

export const resilientFetch = fetchData().pipe(
  Effect.catchTag('TimeoutError', _err => {
    // Retry once on timeout
    return fetchData();
  }),
  Effect.catchTag('NetworkError', err => {
    if (err.status >= 500) {
      return Effect.succeed('Cached fallback data');
    }
    // Re-fail client 4xx errors
    return Effect.fail(err);
  })
);
```

### Example 3: Edge Case - Intercepting Defects (die) with `catchAllDefects`
```typescript
import { Effect } from 'effect';

const faultyLogic = Effect.sync(() => {
  throw new TypeError('Cannot read properties of null');
});

// Convert unhandled defects into controlled domain errors
export const safeProgram = faultyLogic.pipe(
  Effect.catchAllDefects(defect => {
    console.error('Recovered from defect:', defect);
    return Effect.succeed('Default Safe State');
  })
);
```

### Example 4: Compiler Failure Case - Unhandled Error Union
```typescript
import { Effect, Data } from 'effect';

class ErrA extends Data.TaggedError('A') {}
class ErrB extends Data.TaggedError('B') {}

declare const step: Effect.Effect<string, ErrA | ErrB>;

// Attempting to run a program that still has unhandled errors:
// Effect.runSync requires Effect<A, never, never>
// @ts-expect-error TS2345: Argument of type 'Effect<string, ErrA | ErrB, never>' is not assignable to parameter of type 'Effect<string, never, never>'.
Effect.runSync(step);
```

### Example 5: Idiomatic Generator-Style Flow with `Effect.gen`
```typescript
import { Effect, Data } from 'effect';

class AuthError extends Data.TaggedError('AuthError')<{ readonly reason: string }> {}
class BalanceError extends Data.TaggedError('BalanceError')<{ readonly needed: number }> {}

function authenticate(token: string): Effect.Effect<string, AuthError> {
  return token === 'secret' ? Effect.succeed('usr_42') : Effect.fail(new AuthError({ reason: 'Bad token' }));
}

function checkBalance(userId: string): Effect.Effect<number, BalanceError> {
  return userId === 'usr_42' ? Effect.succeed(150) : Effect.fail(new BalanceError({ needed: 200 }));
}

export const processPayment = (token: string) =>
  Effect.gen(function* () {
    const userId = yield* authenticate(token);
    const balance = yield* checkBalance(userId);
    return { status: 'SUCCESS', userId, balanceRemaining: balance - 50 };
  });
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Running Effects with Unhandled Errors**: `Effect.runSync` and `Effect.runPromise` will reject or throw if the `Error` channel is not `never`. You must catch all errors or use `Effect.runPromiseExit`.
2. **Defects vs Failures**: If you write `Effect.sync(() => { throw new Error(); })`, Effect treats this as a **defect** (unexpected crash), NOT a typed failure. To capture thrown exceptions as typed errors, use `Effect.try`.
3. **Fiber Interruption**: If a fiber is cancelled, Effect executes finalizers registered with `Effect.ensuring`. Code must not leave sockets open expecting synchronous unwinding.
4. **Tag Collisions**: When defining `Data.TaggedError('SameTag')`, if two errors share the identical tag string, `Effect.catchTag` cannot distinguish between them at runtime.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| `Effect.sync(() => JSON.parse(str))` | Thrown error becomes untracked Defect | Use `Effect.try({ try: () => JSON.parse(str), catch: ... })` |
| Calling `Effect.runPromise` inside business logic | Bypasses fiber runtime and dependency injection | Return `Effect` and run only at application entrypoint |
| Using plain strings for errors | Loses `catchTag` discrimination | Use `Data.TaggedError` classes |
| Swallowing defects with `catchAll` | `catchAll` only catches expected failures (`E`), not defects | Use `catchAllDefects` if defect recovery is needed |
| Missing `yield*` inside `Effect.gen` | Yields an Effect object instead of unwrapping its value | Always use `yield* effect` |

---

## 8. Performance & Type-Checker Complexity
- Effect uses a custom Virtual Machine loop (trampolined execution) inside fibers. It avoids JavaScript call stack limits and handles millions of operations with zero stack overflows.
- Type checking `Effect<A, E, R>` involves 3 type arguments. Because Effect 3.x uses flat types and avoids excessive conditional type recursion, compilation speeds are exceptionally fast.

---

## 9. Compiler Internals
`Data.TaggedError` generates a class whose prototype contains `_tag: string`. The Effect type system uses this literal string tag to distribute over union types:
```typescript
type FilterByTag<E, Tag extends string> = E extends { _tag: Tag } ? E : never;
```
When `Effect.catchTag('MyError', handler)` is called, the compiler subtracts `MyError` from the `E` error union of the returned Effect.

---

## 10. Best Practices & Design Guidelines
- Define all domain errors using `Data.TaggedError`.
- Keep the `E` error channel narrow and explicit; do not lump all errors into `unknown` or `any`.
- Use `Effect.gen` for complex multi-step procedural logic to maintain clean readability.
- Handle defects exclusively at top-level process/worker supervisors.
- Run effects only at the "end of the world" (e.g. main script runner or Express/Fastify route handler).

---

## 11. Real-World Case Study
A real-time financial order router deployed on Node.js experienced memory leaks and unhandled rejection process terminations whenever third-party WebSocket feeds disconnected mid-order.

**Effect-TS Architecture:**
```typescript
import { Effect, Data } from 'effect';

class WebSocketDisconnected extends Data.TaggedError('WebSocketDisconnected') {}
class OrderTimeout extends Data.TaggedError('OrderTimeout') {}

function routeOrder(orderId: string): Effect.Effect<string, WebSocketDisconnected | OrderTimeout> {
  return Effect.gen(function* () {
    // Acquire resource safely
    yield* Effect.acquireRelease(
      Effect.log(`Acquired lock for ${orderId}`),
      () => Effect.log(`Released lock for ${orderId}`)
    );
    return `Order ${orderId} filled`;
  });
}
```
Resource release was mathematically guaranteed via `acquireRelease`, and fiber cancellation handled sudden socket disconnects without dropping orders or leaking memory.

---

## 12. Interview Questions

<details>
<summary>1. What are the three type parameters of `Effect<A, E, R>`?</summary>

- `A` is the Success type (returned on normal completion).
- `E` is the Expected Error type (recoverable business failure).
- `R` is the Requirements type (context/dependencies needed to run).
</details>

<details>
<summary>2. What is the difference between a Failure and a Defect in Effect?</summary>

A **Failure** (`Effect.fail`) is an expected domain error tracked in the `E` channel at compile time. A **Defect** (`Effect.die`) represents an unexpected crash or programmer bug (e.g. `TypeError`, assertion failure) that bypasses `E` and is stored in `Cause`.
</details>

<details>
<summary>3. Why does `catchAll` not catch exceptions thrown inside `Effect.sync`?</summary>

Because exceptions thrown inside `Effect.sync` are classified as defects, whereas `catchAll` specifically handles the typed `E` channel. To catch defects, use `catchAllDefects` or wrap the code using `Effect.try`.
</details>

<details>
<summary>4. What does `Data.TaggedError` provide?</summary>

It provides a class constructor with an immutable `_tag` property, support for structural equality checks, and integration with `catchTag` for type-safe pattern matching.
</details>

<details>
<summary>5. What is `Cause<E>` in Effect?</summary>

`Cause` is an algebraic data structure that captures all reasons for abnormal fiber termination, including failures, defects, interruptions, and composite parallel errors.
</details>

<details>
<summary>6. How does `Effect.gen` work?</summary>

It uses JavaScript generator functions to provide sequential, async/await-like syntax for Effects. Calling `yield* effect` pauses execution until the effect resolves, short-circuiting immediately if a failure occurs.
</details>

<details>
<summary>7. What does "End of the World" mean in Effect architecture?</summary>

It refers to the boundary where an Effect program is finally executed via `Effect.runPromise` or `Effect.runFork` (typically in `main.ts` or an HTTP controller), while all internal functions remain pure Effect descriptions.
</details>

<details>
<summary>8. How does Effect handle cancellation?</summary>

Through fibers. When a fiber is interrupted, the runtime walks backwards through the fiber's execution history, executing all finalizers registered with `ensuring` or `acquireRelease` in order.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Tagged Error Creation**: Define a `UserNotFound` error using `Data.TaggedError` with an `id: string` field.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Data } from 'effect';

   class UserNotFound extends Data.TaggedError('UserNotFound')<{ readonly id: string }> {}
   ```
   </details>

2. **Basic Success Effect**: Create an effect that succeeds with the number `42` and run it synchronously using `Effect.runSync`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Effect } from 'effect';

   const program = Effect.succeed(42);
   const result = Effect.runSync(program); // 42
   ```
   </details>

### Medium
3. **Catch Specific Tag**: Write a pipeline that takes an Effect failing with `ErrA | ErrB`, catches `ErrA`, and produces an Effect failing only with `ErrB`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Effect, Data } from 'effect';

   class ErrA extends Data.TaggedError('ErrA') {}
   class ErrB extends Data.TaggedError('ErrB') {}

   declare const action: Effect.Effect<string, ErrA | ErrB>;

   const handled: Effect.Effect<string, ErrB> = action.pipe(
     Effect.catchTag('ErrA', () => Effect.succeed('Recovered from A'))
   );
   ```
   </details>

4. **Wrapping Unsafe JSON with Effect.try**: Implement `safeParseJson<T>(raw: string)` returning an Effect that fails with a tagged `JsonParseError` on failure.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Effect, Data } from 'effect';

   class JsonParseError extends Data.TaggedError('JsonParseError')<{ readonly raw: string }> {}

   function safeParseJson<T>(raw: string): Effect.Effect<T, JsonParseError> {
     return Effect.try({
       try: () => JSON.parse(raw) as T,
       catch: () => new JsonParseError({ raw }),
     });
   }
   ```
   </details>

### Hard
5. **Robust Retry with Jitter Policy**: Build an Effect program that wraps an unreliable HTTP call, catches `NetworkError`, and retries up to 3 times with exponential backoff before falling back to a default value.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Effect, Data, Schedule } from 'effect';

   class NetworkError extends Data.TaggedError('NetworkError') {}

   declare function callService(): Effect.Effect<string, NetworkError>;

   const retryPolicy = Schedule.exponential('100 millis').pipe(
     Schedule.compose(Schedule.recurs(3))
   );

   export const robustProgram = callService().pipe(
     Effect.retry(retryPolicy),
     Effect.catchTag('NetworkError', () => Effect.succeed('Default cached value'))
   );
   ```
   </details>

6. **Full Transaction with Resource Management (`acquireRelease`)**: Author a database transaction pipeline using `Effect.acquireRelease` that opens a connection, executes two steps inside `Effect.gen`, and guarantees the connection is committed or rolled back and closed.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Effect } from 'effect';

   interface DbConnection {
     query(sql: string): Effect.Effect<void>;
     rollback(): Effect.Effect<void>;
     close(): Effect.Effect<void>;
   }

   declare function openConnection(): Effect.Effect<DbConnection>;

   export const transaction = Effect.acquireUseRelease(
     openConnection(),
     conn =>
       Effect.gen(function* () {
         yield* conn.query('UPDATE accounts SET balance = balance - 100 WHERE id = 1');
         yield* conn.query('UPDATE accounts SET balance = balance + 100 WHERE id = 2');
         return 'TRANSFER_COMPLETE';
       }),
     conn => conn.close()
   );
   ```
   </details>
