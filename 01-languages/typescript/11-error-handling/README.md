# Module 11: Error Handling in TypeScript

Welcome to **Module 11: Error Handling in TypeScript**. This module provides a complete, production-grade guide to modeling, narrowing, and recovering from errors across all tiers of TypeScript application architecture.

---

## Module Overview

| Note | Title | Level | Core Topics |
| :--- | :--- | :--- | :--- |
| **00** | [Catching and Narrowing Errors](./00-catching-and-narrowing-errors.md) | Intermediate | `catch (e: unknown)`, `useUnknownInCatchVariables`, `instanceof`, type guards, `Error.cause` |
| **01** | [Custom Error Classes](./01-custom-errors.md) | Intermediate | Subclassing `Error`, `Object.setPrototypeOf`, prototype downleveling, nominal tags, JSON serialization |
| **02** | [Result Pattern](./02-result-pattern.md) | Advanced | Discriminated unions, `Ok` vs `Err`, functor `map`, monad `flatMap`, eliminating runtime exceptions |
| **03** | [Error Handling Strategies](./03-error-handling-strategies.md) | Advanced | Operational errors vs defects, boundaries, fail-fast vs degradation, structured logging, telemetry |
| **04** | [neverthrow and Functional Error Handling](./04-neverthrow-and-functional-error-handling.md) | Advanced | `neverthrow`, `ResultAsync`, `fromPromise`, `fromThrowable`, `safeTry`, parallel combinators |
| **05** | [Effect-TS Error Management and Defects](./05-effect-ts-error-management-and-defects.md) | Advanced | `Effect<A, E, R>`, failures vs defects, `Data.TaggedError`, `catchTag`, `Cause<E>`, fiber recovery |

---

## Learning Path

```
                    +------------------------------------+
                    | 00-catching-and-narrowing-errors   |
                    | (useUnknownInCatchVariables, cause)|
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 01-custom-errors                   |
                    | (Subclasses, prototypes, brands)   |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    | 02-result-pattern                  |
                    | (Discriminated unions, monads)     |
                    +-----------------+------------------+
                                      |
                 +--------------------+--------------------+
                 |                                         |
                 v                                         v
+------------------------------------+   +------------------------------------+
| 03-error-handling-strategies       |   | 04-neverthrow-and-functional       |
| (Boundaries, defects, telemetry)   |   | (ResultAsync, safeTry, promises)   |
+------------------------------------+   +-----------------+------------------+
                                                           |
                                                           v
                                         +------------------------------------+
                                         | 05-effect-ts-error-management      |
                                         | (Effect<A, E, R>, defects, fibers) |
                                         +------------------------------------+
```

---

## Key Principles & Best Practices
1. **Never use `catch (e: any)`**: Always enforce `"useUnknownInCatchVariables": true` (or `"strict": true`) and narrow using type guards.
2. **Domain Errors as Data**: Model anticipated business errors (validation, authentication, not found) as explicit discriminated union types rather than thrown exceptions.
3. **Defects as Crashes**: Throw exceptions or use `Effect.die` for programmer bugs and invariant breaches that require immediate boundary recovery.
4. **Preserve Causality**: Always wrap low-level transport errors using `new Error(msg, { cause: err })` to retain original stack traces.
5. **Boundary Logging**: Log errors exactly once at architectural boundaries (middleware, queue workers) with correlation IDs and PII masking.
