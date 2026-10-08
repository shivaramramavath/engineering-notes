# Testing Fundamentals

A test is code that checks that other code does what you think it does, **automatically and repeatably**. The value isn't in the first run. It's in the thousandth, after a refactor, a dependency upgrade, or a teammate's change, when the test tells you something broke before your users do.

## What tests are for

- **Confidence to change code.** Refactor, upgrade React, or restructure folders, and a green suite says behavior is intact.
- **Catching regressions.** Once a bug is fixed, a test keeps it fixed.
- **Documentation.** A good test shows how a component or function is supposed to be used.
- **Design feedback.** Code that's painful to test is usually painfully coupled.

What tests are **not**: proof of correctness, or a replacement for thinking. They check the cases you thought of.

## Types of tests

| Type | Checks | Example | Speed | Confidence per test |
|---|---|---|---|---|
| **Static** | Types and lint rules, no execution | TypeScript error, ESLint `exhaustive-deps` | Instant | Catches a whole class of bugs |
| **Unit** | One function or hook in isolation | `formatCurrency(1234.5, "EUR")`, a reducer | Very fast | Low (narrow) |
| **Integration** | Several units working together | A form + validation + API call + success message | Fast | **High** |
| **End-to-end (E2E)** | The real app in a real browser | Log in → create project → see it in the list | Slow | Highest (but costly, and flakier) |

### The testing trophy (not the pyramid)

The classic **pyramid** says: many unit tests, fewer integration, fewest E2E. For UI code, the **trophy** is a better guide:

```text
        ╔════════╗
        ║  E2E   ║      a few: critical journeys
       ╔╩════════╩╗
       ║Integration║    the bulk: features through the UI
      ╔╩══════════╩╗
      ║    Unit     ║   pure logic that deserves its own tests
     ╔╩════════════╩╗
     ║    Static      ║  TypeScript + ESLint: the base
     ╚════════════════╝
```

Why integration gets the most weight in React apps: a bug usually lives in the **seams** (a component passes the wrong prop, a hook and the cache disagree, a route isn't wired up). A test that renders the feature and clicks through it covers those seams. A pile of tiny unit tests for components in isolation often passes while the app is broken.

## What to test

Ask: **"If this broke, would a user notice or care?"**

**Test:**

- **User-visible behavior**: "typing a query filters the list", "submitting an invalid email shows an error", "a deleted item disappears".
- **Business logic**: price calculation, permission rules, validation, reducers ([reducer tests](../13-state-management/02-reducer-and-context-pattern.md#testing-is-the-payoff)).
- **Edge cases**: empty, loading, error, very long input, boundary values, permissions.
- **Bugs you fixed**: write the failing test first, then the fix.
- **Critical flows end to end**: sign up, checkout, anything that makes or loses money.

**Don't test:**

- **Implementation details**: internal state values, which hook was called, a component's private function names, CSS class names.
- **Third-party libraries**: React Router works; test *your* use of it.
- **Trivial code**: a component that only renders a prop in a `<div>`.
- **Everything at 100% coverage for its own sake.**
- **Styling** (use visual regression tooling only if it genuinely matters).

### Implementation details vs behavior

```tsx
// ✗ Brittle: tied to how it's built
expect(wrapper.state("count")).toBe(1)
expect(component.find(".counter-value").text()).toBe("1")

// ✓ Resilient: tied to what the user sees and does
await user.click(screen.getByRole("button", { name: /increment/i }))
expect(screen.getByText("Count: 1")).toBeInTheDocument()
```

The second test survives rewriting the component from `useState` to `useReducer`, renaming classes, or splitting it into three components. The first breaks on any of them even though nothing a user cares about changed. **A good test fails only when behavior breaks.**

## Anatomy of a test: Arrange, Act, Assert

```ts
test("applies a percentage discount", () => {
  // Arrange: set up the inputs
  const cart = { items: [{ price: 100, qty: 2 }], coupon: { type: "percent", value: 10 } }

  // Act: do the thing
  const total = calculateTotal(cart)

  // Assert: check the result
  expect(total).toBe(180)
})
```

Keep each test focused on **one behavior**. When it fails, the name should tell you what broke.

### Naming

Describe behavior, not functions:

```ts
describe("LoginForm", () => {
  it("shows an error when the password is too short", …)
  it("disables the submit button while the request is pending", …)
  it("redirects to the dashboard after a successful login", …)
})
```

Reading the test names top to bottom should read like a spec.

## Properties of good tests

- **Deterministic**: same result every run. No dependence on the current time, random values, network, test order, or machine speed.
- **Independent**: each test sets up its own state and doesn't rely on another test having run.
- **Fast**: slow suites don't get run.
- **Readable**: someone else can tell what's being verified without reading the implementation.
- **Meaningful failure**: the message points to what's wrong.
- **Resilient to refactors**: they test behavior (above).

### Flaky tests

A test that sometimes passes and sometimes fails is **worse than no test**, because people stop trusting the suite. Common causes:

- **Timing**: asserting before async work finishes, or using fixed `sleep`s instead of waiting for a condition.
- **Shared state**: tests leaking data (a cache, a store, `localStorage`, a mock) into each other.
- **Real clocks, randomness, and network.**
- **Order dependence.**
- **Animations and transitions** in E2E tests.

Fix the root cause (wait for the right thing, isolate state, control time). Don't just add retries and hope.

## Test doubles: mock, stub, spy, fake

Terms get used loosely, but the distinctions help:

| Term | What it is | Example |
|---|---|---|
| **Stub** | Returns canned answers | A function that always returns `{ id: 1 }` |
| **Spy** | Wraps the real thing and records calls | `vi.spyOn(console, "error")` |
| **Mock** | Replaces something *and* has expectations on how it was called | `vi.fn()` asserted with `toHaveBeenCalledWith(...)` |
| **Fake** | A simplified working implementation | An in-memory database, a fake server (MSW) |

Rules of thumb ([details](./04-mocking-and-msw.md)):

- **Mock at the boundaries** (network, time, randomness, browser APIs), not your own internal modules.
- The more you mock, the less your test proves. Over-mocked tests pass while the app is broken.
- Prefer fakes that behave like the real thing (a mock server) over hand-written call assertions.

## Coverage

Coverage reports which lines or branches your tests executed.

- **It tells you what's *not* tested.** Uncovered code is untested for sure.
- **It does not tell you what's *well* tested.** A test that runs a line without asserting anything still counts as covered.
- **Chasing 100%** produces low-value tests of trivial code and brittle tests of implementation. A healthy range depends on the project, and the trend and the *critical-path* coverage matter more than the number.

Use coverage as a **flashlight for gaps**, not a target or a quality metric.

## Snapshot tests: use sparingly

A snapshot test serializes output (a component's HTML, an object) and compares it to a stored copy.

- Good for small, stable outputs (a serialized config, an error message shape).
- Bad for large component trees: they fail on every harmless change, people press "update snapshot" without reading the diff, and they assert nothing specific. If you can't say what a snapshot *means*, prefer an explicit assertion.

## Test-driven development (TDD)

Write a failing test, make it pass, refactor. It isn't mandatory, but two parts are widely useful:

- **For bug fixes**: reproduce the bug in a failing test first. You prove you understood it, and you get a regression test for free.
- **For pure logic** with clear rules (parsers, calculations, reducers), tests-first clarifies the requirements.

For exploratory UI work, writing tests after the shape settles is perfectly reasonable.

## Testing pure logic

The easiest, highest-value tests: functions with inputs and outputs, no DOM, no mocks.

```ts
// utils/format.ts
export function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  const units = ["KB", "MB", "GB"]
  let value = bytes / 1024, i = 0
  while (value >= 1024 && i < units.length - 1) { value /= 1024; i++ }
  return `${value.toFixed(1)} ${units[i]}`
}

// utils/format.test.ts
test.each([
  [0, "0 B"],
  [1023, "1023 B"],
  [1024, "1.0 KB"],
  [1536, "1.5 KB"],
  [5 * 1024 * 1024, "5.0 MB"],
])("formatBytes(%i) → %s", (input, expected) => {
  expect(formatBytes(input)).toBe(expected)
})
```

Table-driven tests (`test.each`) make the edge cases visible at a glance. A tip for architecture: **move logic out of components into pure functions and reducers**, and it becomes trivially testable ([component design](../05-component-design/README.md)).

## Where to put tests

Two common conventions, both fine:

```text
src/features/cart/
  CartPage.tsx
  CartPage.test.tsx          ← colocated: easy to find, moves with the code
  cart-reducer.ts
  cart-reducer.test.ts
```

or a parallel `__tests__/` folder. **Colocate** by default; keep shared test utilities (render helpers, MSW handlers, factories) in `src/test/`.

## Tests in your workflow

- **Run them locally in watch mode** while developing.
- **Run them in CI on every pull request** and block merges on failure ([CI/CD](../19-production/04-ci-cd.md)).
- **Static checks first** (TypeScript, ESLint): fastest feedback.
- Keep the fast tests fast, and run slow E2E tests on a smaller critical set.
- Treat a failing test as a signal to investigate, not to delete.

## Common mistakes

- **Testing implementation details**: state values, internal functions, class names.
- **Over-mocking**, so tests pass while the real integration is broken.
- **Chasing a coverage number** instead of valuable behavior.
- **Large snapshot tests** nobody reads.
- **Flaky tests** tolerated with retries instead of fixed.
- **Tests that depend on each other** or on execution order.
- **Real time, randomness, or network** in unit and integration tests.
- **Vague test names** (`it("works")`) that explain nothing when they fail.
- **Testing everything at the same level**: only E2E (slow and fragile) or only unit (misses the seams).
- **Skipping the "fail first" check**, so you never see the test fail and can't be sure it tests anything.
- **Deleting or `.skip`ping a failing test** to get CI green.

## Quick summary

- Tests exist to make change safe: regression protection, documentation, design feedback.
- Four layers: **static**, **unit**, **integration**, **E2E**. For UI, favor **integration** tests (the trophy).
- Test **behavior users observe**, not implementation details; a good test fails only when behavior breaks.
- Structure tests as **Arrange / Act / Assert**, one behavior each, with descriptive names.
- Keep tests **deterministic, independent, and fast**; fix flakiness at the root.
- **Mock at the boundaries**; prefer fakes (like MSW) over call assertions.
- Coverage shows gaps, not quality; snapshots sparingly; pure logic is the easiest win.

## Next

[01 — Vitest](./01-vitest.md)
