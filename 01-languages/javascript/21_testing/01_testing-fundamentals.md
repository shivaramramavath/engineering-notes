# Testing Fundamentals

A test is code that runs your code and fails loudly when the result isn't what you expected. Automated tests let you change code, upgrade dependencies, and refactor without re-checking everything by hand.

This note covers what tests are for, how a test runner works, the main kinds of tests, and what separates a useful test from a noisy one. Examples use [Vitest](./04_vitest-and-jest.md), but the ideas apply to any runner.

**Prerequisites:** [Functions](../02_functions/README.md), [Error Handling](../10_error-handling/README.md), [Promises](../11_asynchronous-javascript/03_promises.md)

---

## Your First Test

```js
// sum.js
export const sum = (a, b) => a + b;
```

```js
// sum.test.js
import { test, expect } from "vitest";
import { sum } from "./sum.js";

test("adds two numbers", () => {
  expect(sum(2, 3)).toBe(5);
});
```

```bash
npx vitest run
```

---

## How a Test Runner Works

1. It finds test files (by default names like `*.test.js` and `*.spec.js`).
2. It runs each `test()` callback.
3. A test **passes** if the callback finishes without throwing. It **fails** if it throws or, for async tests, if the returned promise rejects.
4. `expect(actual).toBe(expected)` throws an assertion error when the check doesn't hold. That thrown error is the failure.

Two consequences worth remembering:

- **A test with no assertions passes.** Running code is not the same as checking it.
- **An async test must return or await its promise.** If the runner can't see the promise, it may finish before the assertion runs.

```js
// Wrong: the assertion isn't awaited, so the test may finish first
test("rejects for unknown user", () => {
  expect(loadUser(-1)).rejects.toThrow();
});

// Right
test("rejects for unknown user", async () => {
  await expect(loadUser(-1)).rejects.toThrow();
});
```

---

## Structure: Arrange, Act, Assert

Most readable tests have three visible steps.

```js
test("applies a 10% coupon", () => {
  // Arrange
  const cart = createCart([{ price: 200, qty: 1 }]);

  // Act
  const total = cart.total({ coupon: "SAVE10" });

  // Assert
  expect(total).toBe(180);
});
```

If you can't tell which line is the "act", the test is doing too much.

---

## Kinds of Tests

```text
        /\          End-to-end: a few, slow, closest to real usage
       /  \
      /----\        Integration: some, real components working together
     /      \
    /--------\      Unit: many, fast, one piece in isolation
```

| Kind | Scope | Speed | Good at catching | Cost |
| --- | --- | --- | --- | --- |
| Unit | One function/class, dependencies faked | Milliseconds | Logic bugs, edge cases | Cheap to write and run |
| Integration | Several real parts (e.g. HTTP handler + database) | Slower | Wiring, config, contract mismatches | Needs setup and cleanup |
| End-to-end | The whole app, often through a browser | Slowest | Broken user flows | Expensive, can be flaky |

The "pyramid" is a guide to proportions, not a rule. Many teams of API-heavy services put more weight on integration tests because most of their bugs live in the wiring. Follow where your bugs actually come from.

See [Unit Testing](./02_unit-testing.md) and [Integration Testing](./03_integration-testing.md).

---

## What Makes a Test Good

- **Tests behavior, not implementation.** Assert on what the code returns or does through its public interface. If a refactor that keeps behavior the same breaks the test, the test was coupled to internals.
- **Deterministic.** Same code, same result, every run. Time, randomness, network and shared state are the usual sources of trouble.
- **Independent.** Each test sets up what it needs and can run alone or in any order.
- **Fast.** Slow suites don't get run.
- **Fails with a clear message.** A failure should point at the problem without a debugger.
- **Has one reason to fail.** A test named "works" that checks twelve things is hard to diagnose.

---

## What to Test

Worth testing:

- Business logic and branches (every `if` that changes a result)
- Edge cases: empty input, `null`/`undefined`, zero, negative, very large, duplicates
- Error paths: what happens when something throws or a dependency fails
- Every bug you fix: write the failing test first, then fix. It stays as a regression guard.

Usually not worth testing:

- The framework or language itself (`Array.prototype.map` works)
- Trivial getters and pass-through functions
- Exact internal calls that don't affect observable behavior

### Coverage is not quality

Coverage tells you which lines ran, not whether anything was checked.

```js
test("discount", () => {
  applyDiscount(100, 0.1); // runs every line, asserts nothing
});
```

Use coverage to find **untested** code, not to prove tested code is correct. A high number as a target tends to produce tests like the one above.

---

## Writing Testable Code

Code is easy to test when it separates decisions from side effects.

- Put logic in **pure functions** (same input, same output, no I/O). See [Pure Functions](../07_functional-programming/01_pure-functions-and-side-effects.md).
- Pass in anything that touches the outside world (database, clock, HTTP client). See [Dependency Injection](../20_design-patterns/09_dependency-injection.md).
- Keep the I/O at the edges thin, so most of your code can be tested without any fakes.

---

## Common Mistakes

- Tests that depend on **execution order** or leftover state from another test.
- **Real time, real network, real randomness** in unit tests. They fail one run in fifty.
- **Missing `await`** on async assertions (see above).
- **Copying the implementation** into the test (computing the expected value with the same formula), so a bug passes both.
- **One giant test** covering a whole flow, where the first failure hides the rest.
- **Skipping or `.only` left in** committed code. `test.only` silently disables every other test in the file.

## Debugging a Failing Test

1. Read the diff the runner prints (expected vs received) before changing anything.
2. Run just that test: `npx vitest run -t "applies a 10% coupon"`, or temporarily use `test.only`.
3. Check for a missing `await` or a shared variable modified by another test.
4. Add a `console.log` or run under the debugger ([DevTools and Debugging](../00_setup/04_devtools-and-debugging.md)).
5. If it passes alone but fails with others, you have shared state or order dependence.

---

## Quick Summary

- A test passes unless it throws or rejects, so assertions are what give it meaning.
- Use Arrange / Act / Assert. Keep tests independent and deterministic.
- Unit tests are many and fast, integration tests check the wiring, end-to-end tests are few.
- Test behavior and edge cases, not implementation details. Coverage finds gaps, not correctness.
- Testable code isolates pure logic and injects its dependencies.

**Next:** [Unit Testing](./02_unit-testing.md)