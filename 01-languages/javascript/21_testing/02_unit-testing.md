# Unit Testing

A unit test checks one small piece of logic (a function, a class, a module) in isolation, with anything slow or external replaced by a fake. Unit tests are the cheapest tests to write and run, so they should cover most of your branching logic and edge cases.

**Prerequisites:** [Testing Fundamentals](./01_testing-fundamentals.md), [Error Handling](../10_error-handling/02_try-catch-finally.md)

---

## Testing a Pure Function

Pure functions are the easiest case: input in, output out, nothing to set up.

```js
// slugify.js
export function slugify(text) {
  return text
    .toLowerCase()
    .trim()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
}
```

```js
// slugify.test.js
import { describe, test, expect } from "vitest";
import { slugify } from "./slugify.js";

describe("slugify", () => {
  test("lowercases and joins words with hyphens", () => {
    expect(slugify("Hello World")).toBe("hello-world");
  });

  test("strips leading and trailing separators", () => {
    expect(slugify("  --Hello--  ")).toBe("hello");
  });

  test("returns an empty string for empty input", () => {
    expect(slugify("")).toBe("");
  });
});
```

`describe` only groups tests. It adds a label to the report and lets you share setup, nothing more.

---

## Many Cases, One Test: `test.each`

When only the data changes, use a table.

```js
test.each([
  ["Hello World", "hello-world"],
  ["  spaced  out  ", "spaced-out"],
  ["Rock & Roll!", "rock-roll"],
  ["", ""],
])("slugify(%j) → %j", (input, expected) => {
  expect(slugify(input)).toBe(expected);
});
```

Each row becomes its own test with its own pass/fail line. `%j` formats the value as JSON in the test name. Jest has the same `test.each` API.

---

## Choosing the Right Matcher

| Matcher | Compares with | Use for |
| --- | --- | --- |
| `toBe(x)` | `Object.is` (identity for objects) | Primitives, or "is this the same object" |
| `toEqual(x)` | Deep equality, ignores `undefined` properties | Objects and arrays |
| `toStrictEqual(x)` | Deep equality, also checks `undefined` properties, array holes and class types | When shape and type must match exactly |
| `toMatchObject(x)` | Subset match | Checking some fields of a larger object |
| `toBeCloseTo(n, digits)` | Numeric closeness | Floating-point results |
| `toContain(x)` | Array item or substring | Membership |
| `toThrow(...)` | A function that throws | Error cases |
| `toBeNull()`, `toBeUndefined()`, `toBeTruthy()` | Value checks | Prefer the specific one over `toBeTruthy` |

```js
expect({ a: 1 }).toBe({ a: 1 });        // fails: different objects
expect({ a: 1 }).toEqual({ a: 1 });     // passes

expect(0.1 + 0.2).toBe(0.3);            // fails: floating point
expect(0.1 + 0.2).toBeCloseTo(0.3);     // passes
```

---

## Testing Errors

Wrap the throwing call in a function. If you call it directly, the error escapes before `expect` can see it.

```js
function withdraw(balance, amount) {
  if (amount > balance) throw new RangeError("Insufficient funds");
  return balance - amount;
}

test("throws when the amount exceeds the balance", () => {
  expect(() => withdraw(10, 20)).toThrow(RangeError);
  expect(() => withdraw(10, 20)).toThrow("Insufficient funds");
});
```

For async functions, assert on the promise and await it:

```js
test("rejects when the user does not exist", async () => {
  await expect(loadUser(-1)).rejects.toThrow("not found");
});
```

Assert on the error **type or message** so the test fails if some other error is thrown. A bare `toThrow()` passes for any error, including a typo in your own code.

---

## Testing Classes and Stateful Code

Create a fresh instance for every test so nothing leaks between them.

```js
import { beforeEach, test, expect } from "vitest";

let cart;

beforeEach(() => {
  cart = new Cart();
});

test("starts empty", () => {
  expect(cart.count).toBe(0);
});

test("counts added items", () => {
  cart.add({ id: 1, price: 10 });
  expect(cart.count).toBe(1);
});
```

`beforeEach` runs before every test in its scope, `afterEach` after. Reach for them to build fresh state and clean up. Avoid using them to hide the setup a reader needs to understand a test. If it matters to the assertion, keep it visible in the test.

---

## Isolating the Unit

When a unit uses collaborators (database, mailer, clock), pass in fakes instead of the real ones.

```js
test("sends a welcome email on signup", async () => {
  const sent = [];
  const service = new SignupService({
    users: { findByEmail: async () => null, insert: async () => {} },
    mailer: { send: async (to) => void sent.push(to) },
  });

  await service.signup("a@example.com");

  expect(sent).toEqual(["a@example.com"]);
});
```

This works because dependencies are injected ([Dependency Injection](../20_design-patterns/09_dependency-injection.md)). For spies, stubs and module mocking, see [Mocking](./05_mocking.md).

---

## Edge Cases Worth Checking

- Empty string, empty array, empty object
- `null` and `undefined` if the function accepts them
- `0`, negative numbers, `NaN`, very large numbers
- Boundaries: the exact limit, one below, one above
- Duplicates, unsorted input, unicode
- Inputs that should be rejected, and the error that results

---

## Common Mistakes

- **`toBe` on objects or arrays.** It compares identity. Use `toEqual`.
- **Exact floating-point comparison.** Use `toBeCloseTo`.
- **Calling the throwing function directly** inside `expect(...)` instead of wrapping it.
- **Several behaviors in one test.** When it fails you don't know which one broke.
- **Test names that describe the code** ("calls helper") instead of the behavior ("rejects negative amounts").
- **Shared mutable variables** across tests without a reset in `beforeEach`.
- **Re-implementing the logic in the test** to compute the expected value. Hard-code a known-good expected result.
- **Asserting too much.** Checking every field of a large object makes tests brittle. Assert the fields that matter with `toMatchObject`.

---

## Quick Summary

- Unit tests check one piece of logic in isolation, with dependencies faked.
- Use `test.each` for data-driven cases and `describe` for grouping.
- Pick the matcher deliberately: `toBe` for primitives, `toEqual` for structures, `toBeCloseTo` for floats.
- Test errors with a wrapper function (sync) or `rejects` (async), and assert on type or message.
- Build fresh state per test with `beforeEach`.

**Next:** [Integration Testing](./03_integration-testing.md)