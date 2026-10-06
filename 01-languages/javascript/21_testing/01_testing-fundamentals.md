# Testing Fundamentals

A test is code that runs your code and fails loudly when the behavior is wrong. Automated tests let you change code without fear: refactor, upgrade a dependency, or fix a bug and know within seconds whether something else broke.

Testing is not about proving the code is bug-free. It's about **cheaply detecting regressions** and **documenting intended behavior**.

## Prerequisites

- [Functions](../02_functions/README.md) and [Error handling](../10_error-handling/README.md)
- [Async JavaScript](../11_asynchronous-javascript/README.md) (most real tests touch promises)

---

## Why Tests Pay Off

| Without tests | With tests |
|---|---|
| Every change needs manual re-checking | Change, run, know |
| Refactoring is risky, so code rots | Refactoring is routine |
| Bugs come back | A regression test pins each fixed bug |
| Behavior lives in people's heads | Tests are executable documentation |

Tests also **shape design**. Code that is hard to test (hidden globals, `new Date()` everywhere, I/O mixed with logic) is usually hard to reason about too.

---

## Kinds of Tests

```text
        /\
       /  \        E2E        few, slow, high confidence, brittle
      /----\
     /      \      Integration  some, moderate speed, real collaboration
    /--------\
   /          \    Unit         many, fast, isolated, cheap
  /------------\
```

| Type | Scope | Typical tool | Speed |
|---|---|---|---|
| Unit | One function/class, collaborators faked | Vitest, Jest | ms |
| Integration | Several real modules (e.g. route + service + DB) | Vitest/Jest + supertest | 10s–100s ms |
| End-to-end | Whole app through the UI or public API | Playwright, Cypress | seconds |

The classic "pyramid" says: many unit tests, fewer integration tests, very few E2E. A popular variant (the "testing trophy") puts more weight on integration tests because they catch the bugs that actually reach users. Neither is a law. Pick the cheapest test that gives you confidence in the behavior.

> Related: [Unit testing](./02_unit-testing.md), [Integration testing](./03_integration-testing.md).

---

## Anatomy of a Test

```js
import { describe, it, expect } from 'vitest';
import { add } from './math.js';

describe('add', () => {
  it('adds two numbers', () => {
    // Arrange
    const a = 2, b = 3;

    // Act
    const result = add(a, b);

    // Assert
    expect(result).toBe(5);
  });
});
```

The **Arrange / Act / Assert** shape is the thing to internalize:

1. **Arrange**: build inputs and the system under test.
2. **Act**: do *one* thing.
3. **Assert**: check the outcome.

If you need two Acts, you probably need two tests.

A test **passes** if it finishes without throwing. An `expect` is just a function that throws when the condition fails.

---

## What Makes a Good Test

- **Fast.** Slow suites don't get run.
- **Isolated.** No shared mutable state, no required order. Each test sets up its own world.
- **Deterministic.** Same input, same result. No real clock, randomness, or network unless controlled.
- **Readable.** The name states the behavior; the body shows cause and effect.
- **Tests behavior, not implementation.** Assert *what* happens, not *how*.

```js
// Brittle: breaks if you rename or restructure internals
expect(cart._items.length).toBe(1);

// Resilient: tests the public contract
expect(cart.count()).toBe(1);
```

If a refactor that keeps behavior identical breaks your tests, those tests are coupled to implementation.

### Naming

Describe behavior, not methods:

```js
it('rejects an expired coupon')          // good
it('test applyCoupon')                   // useless on failure
it('returns 0 when the cart is empty')   // good: input + outcome
```

When a test fails in CI, the name alone should tell you what broke.

---

## Core Vocabulary

| Term | Meaning |
|---|---|
| **System under test (SUT)** | The code the test exercises |
| **Assertion** | A check that throws on failure |
| **Fixture** | Prepared data/state a test needs |
| **Test double** | A stand-in for a dependency (stub, spy, mock, fake). See [Mocking](./05_mocking.md) |
| **Regression test** | A test added to pin a bug that was fixed |
| **Flaky test** | Passes and fails without code changes |
| **Coverage** | % of code executed during tests |

---

## Misconceptions

**"100% coverage means it's well tested."** Coverage tells you which lines *ran*, not whether anything was *verified*. This has 100% line coverage and tests nothing:

```js
it('runs', () => { calculateTotal([1, 2, 3]); }); // no assertion
```

Use coverage to find **untested areas**, not as a quality score. A target like 80% as a floor is fine; chasing 100% produces junk tests.

**"Mock everything for isolation."** Over-mocking makes tests pass while the real system is broken. Mock at *boundaries* (network, clock, filesystem), not between your own modules without a reason.

**"Tests slow you down."** Writing them has a cost; *not* having them costs more once the code lives for more than a few weeks.

**"Test every function."** Test **behavior that matters**. Trivial getters and glue code rarely need their own tests.

---

## TDD in One Paragraph

Test-driven development is a loop: write a failing test (**red**), write the minimum code to pass (**green**), clean up (**refactor**). It's a design tool, not a religion. It works especially well for pure logic and for reproducing bugs: write the failing test first, then fix.

---

## Flaky Tests

A flaky test is worse than no test because it trains the team to ignore failures. Common causes:

| Cause | Fix |
|---|---|
| Real timers / `setTimeout` waits | Fake timers; await real conditions |
| `Date.now()` / `Math.random()` | Inject or mock them |
| Shared state between tests | Reset in `beforeEach`; avoid module-level mutables |
| Test order dependency | Make each test self-contained |
| Real network/DB | Use a local fake or controlled test instance |
| Unawaited promises | Always `await`/`return` async work |

Never "fix" flakiness by adding `sleep(500)`.

---

## Quick Summary

- Tests exist to **catch regressions cheaply** and document behavior.
- Prefer many fast unit tests, supported by integration tests for real wiring and a few E2E tests.
- Structure tests as **Arrange → Act → Assert**; one behavior per test.
- Test **public behavior**, not internals.
- Coverage finds gaps; it doesn't prove quality.
- Eliminate flakiness at the source (time, randomness, shared state, I/O).

**Next:** [Unit Testing](./02_unit-testing.md)
