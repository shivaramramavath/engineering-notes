# Unit Testing

A **unit test** checks one small piece of code in isolation: a function, a class, a module. It runs fast, needs no network or database, and tells you exactly which behavior broke. TypeScript catches a lot of mistakes at compile time, but it cannot tell you whether `discount(100, "gold")` returns the *right number*. Tests do that. This note covers writing clear, reliable unit tests in TypeScript, using Vitest in the examples. Jest's API is nearly identical, so the same code works with small changes.

**Prerequisites:**
- [Functions](../02-functions/README.md)
- [async/await](../12-async-and-iteration/02-async-await.md)
- [Custom errors](../11-error-handling/01-custom-errors.md)

---

## A first test

```ts
// src/slugify.ts
export function slugify(text: string): string {
  return text
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
}
```

```ts
// src/slugify.test.ts
import { describe, it, expect } from "vitest";
import { slugify } from "./slugify";

describe("slugify", () => {
  it("lowercases and replaces spaces with hyphens", () => {
    expect(slugify("Hello World")).toBe("hello-world");
  });

  it("collapses repeated separators", () => {
    expect(slugify("a  --  b")).toBe("a-b");
  });

  it("trims leading and trailing separators", () => {
    expect(slugify("  !!Hello!!  ")).toBe("hello");
  });

  it("returns an empty string for input with no valid characters", () => {
    expect(slugify("***")).toBe("");
  });
});
```

Run it:

```bash
npx vitest run          # once
npx vitest              # watch mode
```

Pieces: `describe` groups tests, `it` (or `test`) defines one, `expect(...)` makes an assertion with a **matcher** (`toBe`, `toEqual`, `toThrow`, ...). Test files commonly sit next to the code (`slugify.test.ts`) or in a `test/` folder. Runner choice and configuration are in [test runners](./03-test-runners.md).

## Structure: arrange, act, assert

A readable test has three parts, in order:

```ts
it("applies the gold discount", () => {
  // arrange: set up inputs
  const cart = new Cart([{ sku: "A", priceCents: 1000, qty: 2 }]);

  // act: do the thing under test
  const total = cart.total({ tier: "gold" });

  // assert: check the outcome
  expect(total).toBe(1800);
});
```

Keep each test focused on **one behavior**, and name it for that behavior ("applies the gold discount"), not for the method ("test total"). A failing test name should tell you what broke without opening the file.

## Matchers worth knowing

| Matcher | Use |
|---|---|
| `toBe(x)` | primitives and reference identity (`Object.is`) |
| `toEqual(x)` | deep equality of objects and arrays (ignores `undefined` properties) |
| `toStrictEqual(x)` | deep equality that also checks `undefined` properties and class types |
| `toBeNull()`, `toBeUndefined()`, `toBeDefined()` | absence checks |
| `toBeTruthy()` / `toBeFalsy()` | prefer a more precise matcher when you can |
| `toContain(x)` / `toHaveLength(n)` | arrays and strings |
| `toMatchObject(partial)` | an object contains at least these properties |
| `toBeCloseTo(n, digits)` | floating point comparisons |
| `toThrow(...)` | a function throws |
| `resolves` / `rejects` | promises |

Precise assertions give better failure messages. Prefer `expect(user.role).toBe("admin")` over `expect(user.role === "admin").toBe(true)`, which fails with "expected false to be true".

## Table-driven tests

When the same logic is checked with many inputs, use a table instead of copy-pasted tests:

```ts
it.each([
  ["Hello World", "hello-world"],
  ["  trim me  ", "trim-me"],
  ["Ünïcode", "n-code"],
  ["", ""],
])("slugify(%j) -> %j", (input, expected) => {
  expect(slugify(input)).toBe(expected);
});
```

Each row becomes a separate test with its own name and failure. Add a row to cover a new case, such as a bug you just fixed.

## Testing errors

```ts
function parseAge(input: string): number {
  const n = Number(input);
  if (!Number.isInteger(n) || n < 0) throw new RangeError(`Invalid age: ${input}`);
  return n;
}

it("throws for a negative age", () => {
  expect(() => parseAge("-1")).toThrow(RangeError);
  expect(() => parseAge("-1")).toThrow("Invalid age");
});
```

Wrap the call in a function, or the error is thrown before `expect` can catch it. For custom errors, assert on the class and on stable fields such as `code`, not on full message text ([custom errors](../11-error-handling/01-custom-errors.md)). For code that returns a `Result` instead of throwing, assert on the returned value ([the Result pattern](../11-error-handling/02-result-pattern.md)):

```ts
expect(parse("abc")).toEqual({ ok: false, error: "not-a-number" });
```

## Testing async code

Return the promise or `await` it, and use `resolves` / `rejects` for readable assertions:

```ts
it("loads a user", async () => {
  const user = await service.getUser("1");
  expect(user.name).toBe("Asha");
});

it("rejects for an unknown id", async () => {
  await expect(service.getUser("missing")).rejects.toThrow("not found");
});
```

**Always `await`** the `expect(...).rejects` line. A forgotten `await` makes the test finish before the assertion runs, so it can pass even when the code is wrong. This is the most common source of false-positive tests. The lint rule `@typescript-eslint/no-floating-promises` catches it ([async/await](../12-async-and-iteration/02-async-await.md)).

## Making tests deterministic

Tests must give the same result every time. Common sources of flakiness, and fixes:

| Source | Fix |
|---|---|
| Current time (`new Date()`, `Date.now()`) | inject a clock, or use fake timers and a fixed system time |
| Randomness, generated ids | inject the generator, or seed it |
| Timers and delays | fake timers |
| Shared mutable state between tests | create fresh objects per test, reset in `beforeEach` |
| Test order dependence | make each test independent |
| Network, filesystem, database | use fakes in unit tests ([mocking](./02-mocking.md)) |

```ts
import { vi, beforeEach, afterEach } from "vitest";

beforeEach(() => {
  vi.useFakeTimers();
  vi.setSystemTime(new Date("2024-01-01T00:00:00Z"));
});

afterEach(() => {
  vi.useRealTimers();
});

it("expires a token after one hour", () => {
  const token = issueToken();
  vi.advanceTimersByTime(60 * 60 * 1000);
  expect(isExpired(token)).toBe(true);
});
```

(Jest uses `jest.useFakeTimers()` and similar.) Even better than faking global time is **injecting** it, so the code under test takes a `now: () => Date` parameter ([dependency injection](../17-design-patterns/05-dependency-injection.md)).

## Testing classes with dependencies

Pass fakes through the constructor:

```ts
class InMemoryUsers implements UserRepository {
  private users = new Map<string, User>();
  async findById(id: string) { return this.users.get(id) ?? null; }
  async save(user: User) { this.users.set(user.id, user); }
}

it("registers a new user", async () => {
  const users = new InMemoryUsers();
  const service = new UserService(users, { send: async () => {} });

  await service.register("a@b.com");

  expect(await users.findById("a@b.com")).not.toBeNull();
});
```

Dependencies expressed as small interfaces make fakes trivial ([repository](../17-design-patterns/04-repository.md), [mocking](./02-mocking.md)).

## What to test

Test **behavior** (inputs and observable outputs), not implementation details:

```ts
// brittle: tied to how it works
expect(service["cache"].size).toBe(1);

// robust: tied to what it does
expect(await service.getUser("1")).toEqual(user);
expect(fetchSpy).toHaveBeenCalledTimes(1);     // only if "calls once" is the requirement
```

Good targets:

- **Pure logic:** parsing, formatting, calculations, validation, state transitions ([state machines](../17-design-patterns/06-state-machines.md)).
- **Edge cases:** empty input, zero, negative numbers, very large values, `null`/`undefined` where your types allow them, boundaries (limits, off-by-one).
- **Error paths:** the failures your code is designed to produce.
- **Bugs:** write a failing test that reproduces the bug, then fix it. It stays as a regression test.

Lower value: trivial getters, framework glue, code that is only a pass-through. TypeScript already checks the types, so do not write tests that merely re-check them. Type-level code has its own approach: [type testing](./04-type-testing.md).

## Test data builders

Avoid repeating large object literals. A small builder creates valid defaults and lets each test state only what matters ([builder](../17-design-patterns/01-builder.md)):

```ts
function buildUser(overrides: Partial<User> = {}): User {
  return { id: "u1", name: "Test", email: "t@example.com", role: "member", ...overrides };
}

const admin = buildUser({ role: "admin" });
```

## Organizing tests

- **Co-locate** unit tests with source files (`thing.ts` and `thing.test.ts`), or mirror the structure in `test/`.
- **One `describe` per unit,** nested `describe` per method or scenario if it helps.
- **Use `beforeEach`** for per-test setup, and keep setup visible in the test when it matters to the outcome. Hidden setup in distant hooks makes tests hard to read.
- **Keep tests independent and runnable alone** (`it.only` or `-t "name"`).
- **Snapshots** (`toMatchSnapshot`) are useful for large, stable output, but they make it easy to accept changes without reading them. Prefer explicit assertions for logic.

## Coverage

Coverage tools report which lines and branches ran during tests:

```bash
npx vitest run --coverage
```

Use coverage to **find untested code**, not as a goal. 100% coverage does not mean correct (assertions may be weak), and chasing it encourages low-value tests. Look at uncovered branches in important code and ask whether a test is warranted.

## Common mistakes

- Forgetting to `await` async assertions, producing false passes.
- Testing implementation details (private fields, call order) instead of behavior.
- Tests that depend on each other or on execution order.
- Shared mutable fixtures that leak state between tests.
- Real time, randomness, or network in unit tests.
- Vague names (`it("works")`) and several unrelated assertions per test.
- Asserting on whole error messages that change with wording.
- Over-mocking so tests pass while the real integration is broken ([mocking](./02-mocking.md)).
- Snapshots updated blindly.
- Using `as any` to build test inputs, hiding that the code no longer accepts them.

## Debugging

- Run one test: `npx vitest run -t "applies the gold discount"`, or `it.only(...)` temporarily.
- Read the **diff** in the failure output. For objects, it shows exactly which property differs.
- Add `console.log` or use the debugger inside the test ([debugging](./05-debugging.md)).
- If a test passes alone and fails in the suite, look for shared state or order dependence.
- If a test passes when it should fail, check for a missing `await`, an assertion in an uncalled callback, or a matcher that is too loose (`toBeTruthy`).
- Temporarily break the code under test to confirm the test actually fails. A test you have never seen fail is unproven.

## Quick summary

- A unit test checks one behavior of a small unit, quickly and deterministically. Name tests for behavior and structure them arrange, act, assert.
- Use precise matchers, table-driven tests for many inputs, and `resolves`/`rejects` with `await` for promises.
- Control time, randomness, and external effects by injecting them or faking them.
- Test behavior and edge cases, not implementation details or what the type checker already guarantees.
- Use builders for test data, keep tests independent, and treat coverage as a map, not a score.

**Next:** [Integration testing](./01-integration-testing.md)
