# 18 - Testing and Debugging

How to know your code works, and how to find out why it does not. TypeScript's compiler catches a large class of mistakes before anything runs, but it cannot tell you whether a discount calculation is right, whether your SQL matches the schema, or why a request returns the wrong data. This section covers the practices and tools for the rest: writing tests at the right level, replacing dependencies safely, testing the *types* themselves, stepping through running code, and reading compiler errors fluently.

## Prerequisites

- [11 Error Handling](../11-error-handling/README.md): errors are something tests assert on and debuggers chase
- [12 Async and Iteration](../12-async-and-iteration/README.md): async tests and async bugs
- [17 Design Patterns](../17-design-patterns/README.md): dependency injection, repositories, and fakes make code testable
- [13 Compiler and tsconfig](../13-compiler-and-tsconfig/README.md): source maps, strictness, and per-purpose configs

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [Unit testing](./00-unit-testing.md) | Structure, matchers, table-driven tests, async and error tests, determinism, what to test |
| 01 | [Integration testing](./01-integration-testing.md) | App factories, HTTP tests, real databases, isolation, mocking external HTTP, speed |
| 02 | [Mocking](./02-mocking.md) | Test doubles, hand-written fakes, `vi.fn`/`vi.mock`, typed mocks, timers, when not to mock |
| 03 | [Test runners](./03-test-runners.md) | Vitest, Jest, `node:test`, running vs type-checking, config, aliases, environments, coverage |
| 04 | [Type testing](./04-type-testing.md) | Testing types at compile time: `Equal`/`Expect`, `@ts-expect-error`, `expectTypeOf`, `tsd` |
| 05 | [Debugging](./05-debugging.md) | Source maps, VS Code and DevTools, debugging tests, logging, a method for finding bugs |
| 06 | [Reading type errors](./06-reading-type-errors.md) | Anatomy of an error, common codes, overload errors, isolating and inspecting types |

Read 00 to 02 in order for the testing story. 03 explains the tooling around it. 04 applies to anyone writing library or utility types. 05 and 06 are the debugging pair: one for runtime, one for compile time.

## What do I need?

| Situation | Go to |
|---|---|
| I want to start testing a pure function or class | [00](./00-unit-testing.md) |
| My async test passes even when the code is wrong | [00](./00-unit-testing.md) (missing `await`) |
| My test is flaky because of time or randomness | [00](./00-unit-testing.md), [02](./02-mocking.md) |
| I need to test an Express route with a real database | [01](./01-integration-testing.md) |
| I do not want tests calling third-party APIs | [01](./01-integration-testing.md), [02](./02-mocking.md) |
| How do I replace a dependency in a test? | [02](./02-mocking.md) |
| My mocks make tests pass but production breaks | [02](./02-mocking.md) (when not to mock) |
| Vitest or Jest? How do I configure TypeScript? | [03](./03-test-runners.md) |
| Tests are green but `tsc` reports errors | [03](./03-test-runners.md) (running vs type-checking) |
| How do I test that my utility type is correct? | [04](./04-type-testing.md) |
| How do I assert that code should not compile? | [04](./04-type-testing.md) |
| My breakpoints never hit in `.ts` files | [05](./05-debugging.md) (source maps) |
| Error stack traces point at compiled JavaScript | [05](./05-debugging.md) |
| I cannot reproduce a bug | [05](./05-debugging.md) (method) |
| A type error message is a wall of text | [06](./06-reading-type-errors.md) |
| "No overload matches this call" | [06](./06-reading-type-errors.md) |

## Ideas that recur across the section

- **Types and tests check different things.** The compiler verifies consistency. Tests verify behavior. You need both, plus runtime validation at boundaries.
- **Green tests do not mean type-correct code.** Most runners strip types without checking them, so run `tsc --noEmit` separately.
- **Test behavior, not implementation.** Assert on outcomes. Interaction assertions are for when the call itself is the requirement.
- **Prefer small interfaces and hand-written fakes** over deep mocking. The compiler keeps fakes honest, and tests survive refactoring.
- **Use real things at the seams.** Mocks verify your guesses, while a real database and a real HTTP layer verify the wiring.
- **Determinism is a requirement.** Inject or fake time, randomness, and external effects.
- **A test you have never seen fail is unproven.** Break the code once to confirm.
- **Observe, do not assume.** Debuggers and logs show runtime truth, and compiler errors read best from the bottom up.

## Related sections

- [03 Unions and Narrowing](../03-unions-and-narrowing/README.md): narrowing resolves most "possibly undefined" errors
- [10 Advanced Types: type-level programming](../10-advanced-types/08-type-level-programming.md): `Equal`/`Expect` and the types worth testing
- [14 Type System Internals](../14-type-system-internals/README.md): assignability and variance explain many error messages
- [15 Runtime Validation](../15-runtime-validation/README.md): checking real data that tests and types cannot
- [16 Type-Safe APIs: typed client](../16-type-safe-apis/05-typed-fetch-and-api-client.md): testing with network-level mocks
- [17 Design Patterns](../17-design-patterns/README.md): the patterns that make testing straightforward
- [21 Production Tooling: CI and deployment](../21-production-tooling/05-ci-and-deployment.md) and [logging and observability](../21-production-tooling/06-logging-and-observability.md)
- [22 Performance](../22-performance/README.md): profiling when the bug is slowness
- [26 Projects: type challenges](../26-projects/exercises/00-type-challenges.md): practice for type testing
- [27 Interview](../27-interview/README.md)

## Next

[19 React and Frontend](../19-react-and-frontend/README.md)
