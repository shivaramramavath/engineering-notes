# 11 - Error Handling

How to deal with failure in TypeScript: catching what is thrown, modeling your own errors, making failure visible in return types, and designing where and how errors are handled across an application.

TypeScript adds little to JavaScript's `try/catch` and has no checked exceptions. Its contribution is in the details: `unknown` in catch clauses, narrowing, class hierarchies, and discriminated unions that turn failures into ordinary, exhaustively checkable values.

## Prerequisites

- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): narrowing and discriminated unions are used throughout
- [05 Classes](../05-classes/README.md): custom errors are classes
- [01 Fundamentals: `any` and `unknown`](../01-fundamentals/06-any-and-unknown.md)

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Catching and narrowing errors](./00-catching-and-narrowing-errors.md) | `unknown` catch variables, `instanceof`, `toError`, `cause`, async pitfalls |
| 01 | [Custom errors](./01-custom-errors.md) | Extending `Error`, ES5 prototype issue, `AppError` with codes, serialization |
| 02 | [The Result pattern](./02-result-pattern.md) | `Result<T, E>`, typed error unions, `tryCatch`, when to return vs throw |
| 03 | [Error handling strategies](./03-error-handling-strategies.md) | Expected vs unexpected, where to handle, HTTP mapping, retries, global handlers, logging |

Read them in order. 00 and 01 are the mechanics, 02 is a modeling choice, and 03 ties everything into an approach for a real application.

## Quick decisions

| Question | Answer |
|---|---|
| What type is `e` in `catch (e)`? | `unknown` under `strict`. Narrow before use. |
| How do I check for my error class? | `e instanceof MyError`, or a `code` property across package boundaries |
| How do I keep the original error when wrapping? | `new Error("context", { cause: e })` |
| Throw or return a `Result`? | Return for expected, caller-handled failures. Throw for bugs and boundary-handled failures. |
| Where do I catch errors? | At the lowest level that can handle them, otherwise at the boundary. Log once. |
| What do I send to clients on an unknown error? | A generic `500` with a reference id. Never the stack or internals. |
| Should I retry? | Only transient errors on idempotent operations, with backoff and a cap |
| `catch (e: any)`? | Avoid it. Use `unknown` and narrow. |

## Ideas that recur across the section

- **Anything can be thrown.** Treat the caught value as untrusted until narrowed.
- **Errors are data.** Give them types, codes, and causes so callers can branch on them without parsing messages.
- **Make failure visible.** Return types (`Result`, error unions) turn forgotten error handling into compile errors, which exceptions cannot do.
- **Expected failures vs bugs.** Handle the first, let the second propagate and fail loudly.
- **Handle once, at the right place.** Translate low in the stack, decide in the middle, finish at the boundary.
- **Types do not validate runtime data.** Failure from outside input needs real validation ([15 Runtime Validation](../15-runtime-validation/README.md)).

## Related sections

- [12 Async and Iteration](../12-async-and-iteration/README.md): promise rejection, `async`/`await`, `Promise.allSettled`
- [15 Runtime Validation](../15-runtime-validation/README.md): producing typed errors from untrusted input
- [16 Type-Safe APIs: error response types](../16-type-safe-apis/04-error-response-types.md)
- [17 Design Patterns](../17-design-patterns/README.md)
- [18 Testing and Debugging](../18-testing-and-debugging/README.md): asserting on thrown errors, reading stack traces
- [20 Node.js Backend](../20-nodejs-backend/README.md): Express error middleware
- [21 Production Tooling: logging and observability](../21-production-tooling/06-logging-and-observability.md)
- [23 Security](../23-security/README.md): not leaking internals through errors

## Next

[12 Async and Iteration](../12-async-and-iteration/README.md)