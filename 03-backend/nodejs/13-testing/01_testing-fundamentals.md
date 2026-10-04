# Testing Fundamentals

Before learning tools, you need the ideas: what a test is, what kinds exist, what makes one valuable, and how much testing is enough.

## Why we test

| Benefit | How tests deliver it |
|---------|----------------------|
| **Catch regressions** | A change that breaks old behavior fails a test immediately |
| **Enable refactoring** | You can restructure code and know whether behavior survived |
| **Document behavior** | Test names and examples show how code is meant to be used |
| **Improve design** | Code that is hard to test is usually too coupled; tests push you toward smaller, clearer units |
| **Speed up debugging** | A failing test pinpoints where and what broke |
| **Give confidence to ship** | Automated checks replace manual re-testing |

Tests cost time to write and maintain. The goal is not "as many tests as possible" but **the most confidence per minute spent**.

## Anatomy of a test

Every test does the same three things (**Arrange, Act, Assert**):

```js
import { test, expect } from 'vitest';

test('applies a 10% discount to orders over 100', () => {
  // Arrange: set up inputs and the thing under test
  const order = { subtotal: 200 };

  // Act: do the one thing being tested
  const total = applyDiscount(order);

  // Assert: check the outcome
  expect(total).toBe(180);
});
```

A test **passes** if the assertions hold and nothing throws; it **fails** if an assertion fails or an unexpected error occurs. A good failure message tells you what was expected, what happened, and where.

### Vocabulary

| Term | Meaning |
|------|---------|
| **System under test (SUT)** | The code the test is checking |
| **Test case** | One scenario (`test` / `it`) |
| **Test suite** | A group of related tests (`describe` block or a file) |
| **Assertion** | A statement that must be true (`expect(x).toBe(y)`) |
| **Fixture** | Data or state prepared for tests |
| **Test double** | A stand-in for a real dependency (stub, spy, mock, fake) |
| **Test runner** | The tool that finds, runs, and reports tests (Vitest, Jest, `node:test`) |
| **Coverage** | How much code the tests execute |
| **Flaky test** | A test that sometimes passes and sometimes fails with no code change |
| **Regression** | A bug in behavior that used to work |

## Kinds of tests

| Type | Checks | Speed | Typical tools |
|------|--------|-------|---------------|
| **Unit** | One function or class in isolation | Milliseconds | Vitest, Jest, `node:test` |
| **Integration** | Several modules or a real dependency (database, HTTP layer) working together | 10 ms to seconds | Vitest/Jest + Supertest, Testcontainers |
| **End-to-end (E2E)** | The whole system the way a user uses it | Seconds to minutes | Playwright, Cypress |
| **Component** | UI components rendered in a DOM | Fast to moderate | Testing Library |
| **Contract** | Two services agree on an interface | Fast | Pact, schema tests |
| **Snapshot** | Output matches a stored reference | Fast | Vitest/Jest snapshots |
| **Performance / load** | Speed and capacity | Slow | autocannon, k6, benchmarks |
| **Static analysis** | Types, lint rules (not tests, but catches bugs early) | Fast | TypeScript, ESLint |
| **Manual / exploratory** | Human judgment, usability | Slow | A person |

## The test pyramid

```
            ╱╲
           ╱E2E╲            few: slow, expensive, realistic
          ╱──────╲
         ╱ Integr. ╲        some: moderate speed, real collaborators
        ╱────────────╲
       ╱     Unit     ╲     many: fast, cheap, precise
      ╱────────────────╲
```

- **Many unit tests**: quick feedback, point straight at the failing code
- **Some integration tests**: catch problems in wiring, queries, configuration, serialization
- **A few E2E tests**: cover the most critical user journeys (sign up, checkout)

Alternative shapes you may hear about:

| Shape | Idea |
|-------|------|
| **Testing trophy** (Kent C. Dodds) | Emphasize integration tests; static analysis at the base; fewer unit and E2E tests |
| **Honeycomb** (microservices) | Mostly integration tests, few unit and E2E |
| **Ice-cream cone** (anti-pattern) | Mostly manual and E2E tests: slow, brittle, expensive |

The right balance depends on your system. What matters: **fast feedback, realistic coverage of risk, and low maintenance cost**.

## Properties of a good test (F.I.R.S.T.)

| Letter | Meaning | In practice |
|--------|---------|-------------|
| **F**ast | Runs in milliseconds | No real network, disk, sleeps, or heavy setup in unit tests |
| **I**ndependent | No dependence on other tests or order | Each test sets up its own state; tests can run in parallel or random order |
| **R**epeatable | Same result every time, in any environment | Control time, randomness, network, and environment variables |
| **S**elf-validating | Pass or fail automatically | Assertions, not "look at the console" |
| **T**imely | Written near the code | Alongside or just before the implementation |

More qualities:

- **Readable**: someone new can tell what is being tested and why from the name and body
- **Focused**: one reason to fail
- **Resilient**: breaks when behavior changes, **not** when internals are refactored
- **Meaningful failures**: the error message points at the problem

## What to test

Test **behavior** (inputs and outputs, visible side effects), not **implementation** (private helpers, call order, internal variables).

```js
// Implementation-coupled: breaks if you rename or reorder internals
expect(cart._items.length).toBe(1);
expect(cart._recalculate).toHaveBeenCalledTimes(1);

// Behavior-focused: survives refactors
cart.add({ id: 'a', price: 10 });
expect(cart.total()).toBe(10);
expect(cart.items()).toHaveLength(1);
```

Rule of thumb: if you can rewrite the internals without changing behavior and the test fails, the test is too coupled to the implementation.

### Where to focus

| Test heavily | Test lightly or not at all |
|--------------|----------------------------|
| Business rules and calculations | Trivial getters and setters |
| Branching logic, edge cases, error paths | Framework or library internals (they have their own tests) |
| Parsing, validation, formatting, serialization | Configuration constants |
| Code with a history of bugs | Code that will be thrown away |
| Security-sensitive logic (permissions, input handling) | Pure delegation (`return this.repo.save(x)`) |
| Public APIs and contracts | Private implementation details |

### Cases to consider for any function

| Category | Examples |
|----------|----------|
| **Happy path** | Typical valid input |
| **Boundaries** | 0, 1, max, min, empty string, empty array, exactly at a limit, one over |
| **Invalid input** | `null`, `undefined`, wrong type, negative, `NaN`, malformed data |
| **Error handling** | Dependency fails, timeout, rejected promise |
| **Special values** | Unicode, very long strings, floating-point rounding, time zones, leap years |
| **State and ordering** | Called twice, called out of order, concurrent calls |
| **Idempotence** | Does a repeat call give the same result or double the effect? |

## Test-driven development (TDD)

TDD flips the order: write a failing test first, then the code.

```
1. RED     write a small test for behavior that does not exist yet; watch it fail
2. GREEN   write the simplest code that makes it pass
3. REFACTOR  clean up the code and tests while keeping everything green
   └──────▶ repeat
```

```js
// 1. RED
test('slugify lowercases and joins words with dashes', () => {
  expect(slugify('Hello World')).toBe('hello-world');
});
// fails: slugify is not defined

// 2. GREEN
export const slugify = (s) => s.toLowerCase().replace(/\s+/g, '-');

// 3. next RED: another behavior
test('slugify strips punctuation', () => {
  expect(slugify('Hello, World!')).toBe('hello-world');
});
```

Benefits: tests exist for everything, designs are driven by usage, and you only write code that is needed. Seeing the test fail first proves it can fail. TDD is a technique, not a religion: it fits well for logic-heavy code and bug fixes (**write a test that reproduces the bug first, then fix it**), and less well for exploratory UI work.

Related styles:

| Style | Idea |
|-------|------|
| **BDD** (behavior-driven) | Describe behavior in domain language (`describe('checkout')`, `it('rejects expired cards')`) |
| **ATDD** | Start from acceptance criteria agreed with stakeholders |
| **Test-after** | Write tests after the code: acceptable, but easy to skip or to test only what exists |

## Structuring tests

```js
import { describe, it, expect, beforeEach } from 'vitest';

describe('ShoppingCart', () => {
  let cart;

  beforeEach(() => {
    cart = new ShoppingCart();           // fresh state for each test
  });

  describe('add', () => {
    it('adds a new item', () => {
      cart.add({ id: 'a', price: 10 });
      expect(cart.count()).toBe(1);
    });

    it('increases the quantity when the same item is added again', () => {
      cart.add({ id: 'a', price: 10 });
      cart.add({ id: 'a', price: 10 });
      expect(cart.count()).toBe(1);
      expect(cart.quantityOf('a')).toBe(2);
    });
  });

  describe('total', () => {
    it('is zero for an empty cart', () => {
      expect(cart.total()).toBe(0);
    });
  });
});
```

`describe` groups tests; `it` and `test` are synonyms. Together the nested names should read like a sentence: *ShoppingCart add increases the quantity when the same item is added again*.

### Naming tests

Describe **behavior and condition**, not the method name:

| Weak | Better |
|------|--------|
| `test('login')` | `test('rejects login when the password is wrong')` |
| `test('works')` | `test('returns an empty list when there are no matches')` |
| `test('calculateTax test 3')` | `test('charges no tax for orders under the exemption threshold')` |

A failing test's name should tell you what broke without opening the file.

## Test isolation and state

Tests must not affect one another.

| Source of shared state | Fix |
|------------------------|-----|
| Module-level variables and singletons | Create fresh instances per test (`beforeEach`); inject dependencies |
| Database rows | Transaction rollback, truncate tables, or a fresh database per test file |
| Global mocks and spies | Restore after each test (`vi.restoreAllMocks()`, `jest.restoreAllMocks()`) |
| Environment variables | Set and restore in `beforeEach`/`afterEach` |
| Timers, `Date` | Use fake timers; restore real timers afterward |
| Files on disk | Use temporary directories (`fs.mkdtemp`), clean up afterward |
| Network | Mock it or use a local test server on a random port |

A reliable check: run tests in random order or one at a time (`--shuffle`, `.only`) and confirm they still pass.

## Coverage

Coverage measures which code was **executed** by tests.

| Metric | Counts |
|--------|--------|
| **Line / statement** | Lines or statements run |
| **Branch** | Each `if`/`else`, `?:`, `&&`, `??`, `switch` path taken |
| **Function** | Functions called at least once |

```bash
npx vitest run --coverage
```

What coverage tells you:

- **Low coverage** (say, 20%) shows large untested areas
- **High coverage** does **not** prove the tests are good: code can run without being checked

```js
// 100% line coverage, zero value: no assertions
test('runs', () => {
  calculateTotal([1, 2, 3]);
});
```

Practical guidance:

- Use coverage to **find gaps**, not as a score to maximize
- Branch coverage is more informative than line coverage
- A target like 70 to 90% on important code is reasonable; chasing 100% often produces low-value tests
- Enforce a **threshold** in CI to prevent decline, and review uncovered lines in pull requests
- **Mutation testing** (Stryker) measures test quality by changing code and checking that tests fail

## Deterministic tests: control the uncontrollable

Tests must give the same result every run. Common sources of nondeterminism and fixes:

| Source | Fix |
|--------|-----|
| Current time (`Date.now()`, `new Date()`) | Inject a clock, or use fake timers and set the system time |
| Randomness (`Math.random`, UUIDs) | Inject a generator, seed it, or mock it |
| Network and external services | Mock, stub, or use a local fake server |
| File system and environment | Temp directories; explicit env setup |
| Timers and delays (`setTimeout`, sleep) | Fake timers; wait for conditions, never fixed sleeps |
| Concurrency and ordering | Await everything; avoid relying on arrival order |
| Locale, time zone | Set explicitly (`TZ=UTC`, pass locale arguments) |
| Test order | Make tests independent |

## Testing errors and failure paths

```js
// Synchronous throw
expect(() => parseAge('abc')).toThrow('Invalid age');

// Async rejection
await expect(fetchUser(-1)).rejects.toThrow(/not found/i);

// A function that should NOT throw
expect(() => parseAge('42')).not.toThrow();
```

Failure paths (timeouts, bad input, dependency failures) are where bugs hide and where users get hurt. Give them as much attention as the happy path.

## Reading a failing test

1. Read the **test name** and **assertion message**: what behavior and what difference?
2. Look at the **diff** between expected and actual
3. Read the **stack trace** to find the line in your code
4. Reproduce in isolation (`.only`, `-t "name"`)
5. Decide: is the **code** wrong, or did the **requirements** change so the test must change?

Never "fix" a failing test by weakening its assertion unless the expected behavior truly changed.

## Tests in your workflow

| Practice | Why |
|----------|-----|
| Run tests in **watch mode** while developing | Instant feedback |
| Run the full suite in **CI** on every push and pull request | Catch breakage before merging |
| Run fast tests before slow tests; fail fast | Shorter feedback loops |
| Keep the suite **fast** (unit tests in seconds) | Slow suites get skipped |
| Fix or delete **flaky** tests immediately | They erode trust in everything |
| **Write a test for every bug** you fix | Prevents regressions |
| Review tests in code review like production code | Tests are code too |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Testing implementation details | Tests break on harmless refactors | Test inputs, outputs, and visible effects |
| Tests with no assertions | They pass but verify nothing | Always assert behavior |
| Chasing 100% coverage | Low-value tests, false confidence | Cover risk; read branch coverage |
| Dependent or order-sensitive tests | Mysterious failures | Isolate state; randomize order |
| Real network, clock, or randomness in unit tests | Slow and flaky | Inject or fake them |
| One giant test checking many things | Unclear failures | One behavior per test |
| Unclear test names | Hard to know what broke | Describe behavior and condition |
| Copy-pasted setup everywhere | Painful to maintain | Helpers, builders, `beforeEach` |
| Ignoring or skipping failing tests | The suite stops meaning anything | Fix or delete |
| Only the happy path | Bugs live at the edges and in errors | Add boundaries and failure cases |
| Slow suite | People stop running it | Faster tests, parallelism, a smaller scope per run |
| Logic (loops, conditionals) inside tests | The test itself can be wrong | Keep tests simple and literal |
| Over-mocking | Tests pass while the real system fails | Prefer real collaborators or fakes; see [Mocking](./05_mocking.md) |

## Key takeaways

- A test follows **Arrange, Act, Assert** and should check behavior, not implementation
- Balance the pyramid: many fast unit tests, some integration tests, a few E2E tests
- Good tests are fast, independent, repeatable, self-validating, readable, and focused
- Test boundaries, errors, and edge cases, not just the happy path
- TDD (red, green, refactor) is a powerful technique; at minimum, write a test for every bug before fixing it
- Coverage finds gaps; it does not prove quality
- Control time, randomness, network, and shared state so results are deterministic
- Keep the suite fast and trustworthy: eliminate flaky tests immediately

**Next:** [Unit Testing](./02_unit-testing.md)
