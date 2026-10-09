# Mocking

Real code talks to databases, networks, clocks, and other modules, and unit tests need to replace those with controlled stand-ins. These stand-ins are called **test doubles**, and "mocking" is the loose term for using them. The skill is knowing which kind of double to use, how to keep them type-safe in TypeScript, and when *not* to mock, because over-mocked tests pass while the real system is broken.

**Prerequisites:**
- [Unit testing](./00-unit-testing.md)
- [Dependency injection](../17-design-patterns/05-dependency-injection.md)
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)

---

## Kinds of test doubles

| Double | What it is | Typical use |
|---|---|---|
| **Dummy** | passed but never used | filling a required parameter |
| **Stub** | returns canned answers | "the API returns this user" |
| **Fake** | a working, simplified implementation | in-memory repository, in-memory file store |
| **Spy** | records how it was called, usually wrapping real behavior | "was this called, with what?" |
| **Mock** | a stub plus built-in expectations about calls | strict interaction checks |

In practice, most test frameworks call all of these "mocks" (`vi.fn()` in Vitest, `jest.fn()` in Jest). The distinction still matters for design: **fakes and stubs check outcomes** (what the code returned or stored), while **spies and mocks check interactions** (what it called). Outcome-based tests survive refactoring. Interaction-based tests break whenever you change how the code works, even when the behavior is the same.

## The best mock is a small interface and a hand-written fake

When code receives its dependencies as small interfaces ([dependency injection](../17-design-patterns/05-dependency-injection.md)), you often need no mocking library:

```ts
interface Mailer {
  send(to: string, subject: string): Promise<void>;
}

class FakeMailer implements Mailer {
  readonly sent: { to: string; subject: string }[] = [];
  async send(to: string, subject: string) {
    this.sent.push({ to, subject });
  }
}

it("sends a welcome email", async () => {
  const mailer = new FakeMailer();
  await new UserService(new InMemoryUsers(), mailer).register("a@b.com");

  expect(mailer.sent).toEqual([{ to: "a@b.com", subject: "Welcome!" }]);
});
```

`implements Mailer` means the compiler checks the fake against the real interface, so if `Mailer` changes, the fake stops compiling and you notice. A hand-written fake also reads clearly, can hold state, and is reusable across tests.

## Function mocks and spies

For callbacks and simple cases, use the runner's mock function:

```ts
import { vi, it, expect } from "vitest";

it("calls the callback with the result", () => {
  const onDone = vi.fn();

  process(items, onDone);

  expect(onDone).toHaveBeenCalledTimes(1);
  expect(onDone).toHaveBeenCalledWith({ count: 3 });
});

it("returns canned values", async () => {
  const fetchUser = vi.fn(async (id: string) => ({ id, name: "Asha" }));   // typed from the implementation

  expect(await fetchUser("1")).toEqual({ id: "1", name: "Asha" });
});
```

Giving `vi.fn` an implementation (rather than leaving it untyped) lets TypeScript infer its parameter and return types, so wrong calls and wrong assertions are caught. Generic signatures for `vi.fn`/`jest.fn` differ between versions, so check the documentation for yours.

Common helpers:

```ts
mock.mockReturnValue(1);                  // same value every call
mock.mockResolvedValue(user);             // async: returns a resolved promise
mock.mockRejectedValue(new Error("x"));   // async: returns a rejected promise
mock.mockReturnValueOnce(1);              // only the next call
mock.mockImplementation((x) => x * 2);
mock.mock.calls;                          // recorded argument lists
```

A **spy** wraps an existing method, keeping real behavior unless you override it:

```ts
const spy = vi.spyOn(logger, "warn");
doThing();
expect(spy).toHaveBeenCalledWith("deprecated option");
spy.mockRestore();                        // put the original back
```

## Mocking modules

Sometimes the dependency is an imported module, not an injected object. Module mocking replaces the import for the whole test file:

```ts
import { vi, it, expect } from "vitest";
import { getWeather } from "./weather-client";
import { describeWeather } from "./describe";

vi.mock("./weather-client");              // auto-mocks every export of that module

it("describes the weather", async () => {
  vi.mocked(getWeather).mockResolvedValue({ tempC: 21, summary: "sunny" });

  expect(await describeWeather("Paris")).toBe("sunny, 21C");
});
```

Points to know:

- **`vi.mock` is hoisted** to the top of the file by the runner, before imports, so it applies to modules imported in that file. Variables you reference inside a mock factory must be created with `vi.hoisted` or be defined inside the factory.
- **`vi.mocked(fn)`** returns the function typed as a mock, so `.mockResolvedValue(...)` is type-checked against the real function's signature.
- **ES modules vs CommonJS** differ in how module mocking works. ESM mocking depends on the runner (Vitest handles it; Jest's ESM support needs extra configuration). Check your runner's documentation ([test runners](./03-test-runners.md)).
- Prefer **injecting** dependencies over module mocking. It is less magic, more type-safe, and works in every runner. Module mocks are best for things you cannot inject, such as a third-party import you do not control.

## Keeping mocks type-safe

Mocks are a common place for `as any`, which silently disables checking. Better options:

```ts
// 1. Fake only the part you use, via a narrow interface
interface Clock { now(): Date }
const clock: Clock = { now: () => new Date("2024-01-01") };

// 2. Implement the real interface fully with a class or object
const repo: UserRepository = {
  findById: async () => null,
  save: async () => {},
};

// 3. When you must build a partial object, make the assumption visible and local
const partialUser = { id: "1", name: "Asha" } as User;     // a deliberate, contained cast

// 4. Check shape without widening, when you can
const response = { status: 200, body: { id: "1" } } satisfies ApiResponse;
```

If a dependency is a huge class with dozens of methods, that is a sign to depend on a **smaller interface** instead, and fake that. Helper libraries such as `jest-mock-extended` or `vitest-mock-extended` create fully typed deep mocks from an interface when you do need them.

## Mocking time and timers

```ts
vi.useFakeTimers();
vi.setSystemTime(new Date("2024-01-01"));
vi.advanceTimersByTime(5000);              // run timers scheduled within 5s
await vi.runAllTimersAsync();              // flush timers and awaited promises
vi.useRealTimers();                        // always restore
```

Faking time avoids slow tests and flakiness. Always restore real timers in `afterEach`, or later tests inherit fake time ([unit testing](./00-unit-testing.md)).

## Mocking HTTP

Prefer intercepting at the **network level** (for example with `msw`) over mocking `fetch` or an HTTP client directly. The whole client code runs for real, including URL construction, headers, serialization, and parsing, and the mock only supplies the response ([integration testing](./01-integration-testing.md)).

## Verifying interactions

Use interaction assertions when the call **is** the behavior you care about:

- "Sends exactly one email."
- "Does not call the payment gateway when the cart is empty."
- "Retries three times, then gives up."

```ts
expect(gateway.charge).not.toHaveBeenCalled();
expect(gateway.charge).toHaveBeenCalledWith(expect.objectContaining({ amountCents: 500 }));
```

Matchers like `expect.objectContaining` and `expect.any(Number)` check only the parts that matter, so tests are not coupled to incidental fields. Avoid asserting exact call counts and call order when they are not part of the requirement.

## Cleaning up mocks

Mocks keep state (call history, implementations) across tests unless reset. Leaked state causes order-dependent failures. Configure the runner to reset automatically, or do it in hooks:

```ts
afterEach(() => {
  vi.restoreAllMocks();        // restore spied originals
  vi.clearAllMocks();          // clear call history (keep implementations)
});
```

Runners also have config options (for example `restoreMocks`, `clearMocks`, `mockReset`) to do this globally. Know which your project uses.

## When not to mock

- **Pure functions.** Call them. There is nothing to isolate.
- **Value objects and simple data.** Build real ones.
- **Code you own that is cheap and deterministic.** Use the real thing, so the test also exercises it.
- **Things you do not own.** Do not mock a third-party library's internals. Wrap it in an [adapter](../17-design-patterns/03-adapter.md) you own, and fake **that**. A mock of a vendor SDK encodes your *guess* about how it behaves.
- **The database, when the behavior depends on it.** Use a real one in an integration test.

The more a test mocks, the more it verifies your mocks instead of your code. If a unit test needs five mocks, either the unit does too much, or the test belongs at a higher level.

## Important rules and misconceptions

- **A passing test with mocks proves the code works against the mock.** Not against reality. Keep some tests that use real collaborators.
- **A mock is only as correct as your understanding.** If the real service behaves differently, tests stay green while production breaks. Contract tests and integration tests guard against this.
- **`vi.fn()` with no implementation returns `undefined`.** That may not match the real return type, and TypeScript will not warn you.
- **Call-count assertions are brittle** when they restate implementation.
- **Mocks do not enforce types at runtime.** `as any` mocks can drift silently from the real interface.

## Common mistakes

- Mocking everything, so the test just restates the implementation.
- Asserting on implementation details (call order, internal helper calls).
- Mocking third-party libraries directly instead of wrapping them.
- Leaking mock state between tests.
- Forgetting to restore spies, fake timers, or module mocks.
- Using `as any` for mocks, so the code and the mock diverge unnoticed.
- Referencing variables inside a hoisted `vi.mock` factory before they exist.
- Fakes that are more forgiving than the real implementation.
- Mocking `fetch` instead of using network-level interception.

## Debugging

- Print what the mock received: `console.log(mock.mock.calls)` shows every call's arguments.
- If a mock "was not called", check that the code under test uses the mocked instance or module (a different import path, or the dependency was created before the mock was installed).
- If a module mock does not apply, check hoisting, the module path (it must match the import specifier), and ESM versus CommonJS configuration.
- If a test passes alone but fails with others, a mock or timer leaked: add resets or run with isolation.
- If assertions on arguments fail unexpectedly, look at the diff: `toHaveBeenCalledWith` uses deep equality, so an extra or missing property fails.

## Quick summary

- Test doubles: dummies, stubs, fakes, spies, and mocks. Prefer fakes and stubs and assert on **outcomes**. Use spies and mocks only when the interaction is the requirement.
- Depend on small interfaces and write hand-made fakes that `implements` them, so the compiler keeps them honest.
- Use `vi.fn`, `vi.spyOn`, and `vi.mock` for callbacks, methods, and unavoidable module dependencies. Prefer injection over module mocking.
- Keep mocks typed. Avoid `as any`, and wrap third-party code in adapters you own.
- Fake time, intercept HTTP at the network level, reset mocks between tests, and do not mock what you can use for real.

**Next:** [Test runners](./03-test-runners.md)
