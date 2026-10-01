# Testing

How to know your code works, keep knowing it as the code changes, and change it without fear.

## Why test?

```js
// Friday, 5:45 pm: "just a tiny change" to the discount rule
if (user.isPremium) total = Math.round(total * (1 - 0.1));
```

Did it break checkout for guests? Refunds? The nightly invoice job that reuses the same function? Without tests, the answer is "deploy and see." With tests, the answer arrives in seconds, before anything reaches production.

Tests give you:

| Benefit | What it means |
|---|---|
| **Confidence to change** | Refactor, upgrade dependencies, and fix bugs without fear of silent breakage |
| **Fast feedback** | Find problems in seconds on your machine instead of hours later in production |
| **Executable documentation** | A test shows exactly how a function or endpoint is meant to behave, and unlike a wiki page, it can't drift out of date without failing |
| **Better design** | Code that's hard to test is usually too coupled. Testing pushes you toward the structure in `10-architecture/` |
| **Regression protection** | Every bug you fix gets a test, so it can never silently return |
| **Safer deploys** | CI blocks a broken build before it ships (`16-production/05-ci-cd.md`) |

Tests also cost something: time to write, time to run, time to maintain. The goal is **not** "as many tests as possible" but **the right tests, giving the most confidence for the least cost.**

---

## The kinds of tests

```
                    ▲  fewer, slower, broader, more realistic
                   ╱ ╲
                  ╱E2E╲            Whole system, real browser/clients, real infrastructure
                 ╱─────╲
                ╱  API  ╲          HTTP requests through the real app, real DB (or fakes at the edges)
               ╱─────────╲
              ╱Integration╲        Several units + a real dependency (database, Redis, queue)
             ╱─────────────╲
            ╱     Unit      ╲      One function/class in isolation, dependencies faked
           ╱─────────────────╲
                  ▼  more, faster, narrower, more isolated
```

| Type | Tests | Dependencies | Speed | Failure tells you |
|---|---|---|---|---|
| **Unit** | One function or class, such as a pricing rule or a service method | Faked (fakes, stubs, mocks) | Milliseconds | Exactly which unit is wrong |
| **Integration** | A few units working together with something real, such as a repository against PostgreSQL | Real database/cache; external APIs faked | 10–500 ms | The seam between components is broken |
| **API / component** | A running app through HTTP (`supertest`) | Real app and DB; third parties faked | 50 ms–1 s | A user-visible behavior broke |
| **End-to-end (E2E)** | The deployed system from the outside, often via a browser (Playwright, Cypress) | Everything real | Seconds–minutes | A critical journey is broken |
| **Contract** | That two services agree on an API/event shape | Provider vs consumer | Fast | A change will break a consumer |
| **Load / performance** | Behavior under stress (`15-performance/04-load-balancing-and-testing.md`) | Production-like | Minutes | You'll fall over at N users |
| **Security** | Authorization, validation, injection (`08-authentication-security/`) | As needed | Varies | A vulnerability exists |

### The pyramid vs the trophy

The classic **test pyramid** says: many unit tests, fewer integration tests, very few E2E tests. It's driven by speed and cost: unit tests are cheap and precise.

A popular alternative for backend services is the **testing trophy** (or "honeycomb"): lean more on **integration/API tests**, because most real bugs in a Node.js API live in the *wiring* (SQL, middleware order, serialization, auth), not in isolated pure functions, and mocks can hide exactly those bugs.

A pragmatic split for a typical Express + database API:

| Layer | Share | Why |
|---|---|---|
| Unit tests for business rules, validators, utilities, state machines | Many | Fast, precise, no infrastructure |
| Integration tests for repositories and queue/cache usage | A solid set | Prove your SQL and queries actually work |
| API tests for each endpoint's happy path and key failures | Core of your confidence | Exercise routing, validation, auth, error format, and DB together |
| E2E tests for 3–10 critical journeys | A handful | Slow and brittle, so keep only what protects revenue and trust |

The mix matters less than the principle: **test each behavior at the lowest level that can meaningfully catch the bug, and don't test the same thing at every level.**

---

## What to test (and what not to)

### Test

- **Business rules:** pricing, eligibility, state transitions, permissions
- **Edge cases and boundaries:** empty input, zero, negative, maximum length, duplicates, off-by-one
- **Failure paths:** invalid input, missing records, upstream errors, timeouts, duplicate requests (`09-api-development/06-idempotency.md`)
- **Security-relevant behavior:** "user A cannot read user B's data," expired tokens, rate limits
- **Bugs you've fixed:** one regression test per bug
- **Contracts:** response shapes and error formats clients depend on
- **Anything complex or scary:** if you're nervous changing it, it needs tests

### Don't (or don't bother)

- Third-party libraries: don't test that `express` can route or that `bcrypt` hashes. Test *your* use of them.
- Trivial getters, plain data objects, and one-line pass-throughs
- Implementation details: private helpers, internal call order, exactly which queries ran. These tests break on every refactor and protect nothing.
- Generated code and config
- The same behavior at every level of the pyramid

### Test behavior, not implementation

```js
// ❌ coupled to HOW it works: breaks if you rename a helper or reorder calls
test("placeOrder calls repo.save once, then mailer.send once", async () => {
  await placeOrder(input);
  expect(repo.save).toHaveBeenCalledTimes(1);
  expect(mailer.send).toHaveBeenCalledTimes(1);
  expect(callOrder).toEqual(["save", "send"]);
});

// ✅ coupled to WHAT it promises: survives refactors
test("placing an order stores it and emails the customer", async () => {
  const order = await placeOrder(input);
  expect(await orders.findById(order.id)).toMatchObject({ status: "pending" });
  expect(sentEmails).toContainEqual(expect.objectContaining({ to: user.email }));
});
```

A good test fails **only when behavior a user or caller cares about changes.**

---

## Anatomy of a good test

### Arrange – Act – Assert

```js
test("premium customers get 10% off", () => {
  // Arrange: set up the situation
  const user = { id: "u1", isPremium: true };
  const items = [{ priceCents: 1000, quantity: 2 }];

  // Act: do the ONE thing under test
  const total = calculateTotal(user, items);

  // Assert: check the outcome
  expect(total).toBe(1800);
});
```

### The FIRST properties

| | Meaning |
|---|---|
| **F**ast | Run in milliseconds, so people run them constantly |
| **I**solated | No dependence on other tests, ordering, or shared state |
| **R**epeatable | Same result every time, on every machine (no real clock, randomness, network) |
| **S**elf-validating | Pass or fail automatically, with no human reading of output |
| **T**imely | Written alongside (or before) the code, not months later |

### Good names say what and when

```js
// ❌ vague
test("works", ...);
test("test order", ...);

// ✅ scenario + expectation
test("rejects an order when stock is insufficient", ...);
test("returns 404 when the order belongs to another user", ...);
test("retries a failed email up to 5 times, then dead-letters it", ...);
```

When a test fails in CI, its name alone should tell you what broke.

### One reason to fail

Each test should check **one behavior**. Several assertions about that single behavior are fine; several unrelated behaviors in one test make failures confusing.

---

## Test doubles: a vocabulary

When a unit depends on something slow, unreliable, or non-deterministic (a database, an email provider, the clock), you replace it with a **test double**. People use "mock" for all of these, but the distinctions are useful:

| Double | What it does | Example |
|---|---|---|
| **Dummy** | Fills a parameter that's never actually used | `{}` passed as `logger` |
| **Stub** | Returns canned answers | `findById: async () => ({ id: "u1" })` |
| **Fake** | A simplified **working** implementation | An in-memory repository backed by a `Map` |
| **Spy** | Wraps the real thing and records calls | `jest.spyOn(service, "send")` |
| **Mock** | Pre-programmed with expectations about calls; fails if they aren't met | `expect(mailer.send).toHaveBeenCalledWith(...)` |

Prefer **fakes and stubs** (assert on *outcomes*) over heavy **mocks** (assert on *interactions*), and prefer **real dependencies** (a real test database) over any double when it's cheap enough. Each double is a lie you tell the test, and every lie can drift from reality. See `01` and `02`.

---

## Choosing a test runner

| Runner | Notes |
|---|---|
| **Vitest** | Fast, ESM-native, TypeScript out of the box, Jest-compatible API, built-in coverage and watch mode. A great default for new projects |
| **Jest** | The long-time standard; huge ecosystem and docs. ESM support is behind `--experimental-vm-modules` and mocking ES modules needs `jest.unstable_mockModule`, which is the main friction |
| **`node:test`** (built into Node 20+) | Zero dependencies: `node --test`, `describe/it`, `assert`, mocking (`mock.fn`, `mock.method`, `mock.timers`), coverage via `--experimental-test-coverage`. Smaller ecosystem, fewer matchers |
| **Mocha + Chai** | Older, flexible; bring your own assertions and mocks |

This section writes examples in the **Jest-style API** (`describe`, `test`, `expect`), which Vitest shares almost exactly, so the code works in either. Where they differ, a note says so. Swap `jest.fn` → `vi.fn`, `jest.spyOn` → `vi.spyOn`, `jest.useFakeTimers` → `vi.useFakeTimers`, and `jest.unstable_mockModule` → `vi.mock`.

### Setup: Vitest (recommended for new ESM projects)

```bash
npm install -D vitest @vitest/coverage-v8 supertest
```

```js
// vitest.config.js
import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    environment: "node",
    globals: true,                         // describe/test/expect without imports (optional)
    include: ["src/**/*.test.js", "test/**/*.test.js"],
    setupFiles: ["./test/setup.js"],
    coverage: { provider: "v8", reporter: ["text", "lcov"] },
  },
});
```

### Setup: Jest with ES modules

```bash
npm install -D jest supertest
```

```json
{
  "type": "module",
  "scripts": {
    "test": "NODE_OPTIONS=--experimental-vm-modules jest",
    "test:watch": "NODE_OPTIONS=--experimental-vm-modules jest --watch",
    "test:coverage": "NODE_OPTIONS=--experimental-vm-modules jest --coverage"
  },
  "jest": {
    "testEnvironment": "node",
    "transform": {}
  }
}
```

`"transform": {}` turns off Babel so native ESM works. On Windows, set the environment variable with `cross-env`. In ESM Jest, import `jest` explicitly: `import { jest } from "@jest/globals";`.

### Setup: Node's built-in runner

```js
import { test, describe, mock } from "node:test";
import assert from "node:assert/strict";

describe("calculateTotal", () => {
  test("applies the premium discount", () => {
    assert.equal(calculateTotal({ isPremium: true }, [{ priceCents: 1000, quantity: 2 }]), 1800);
  });
});
```

```bash
node --test                          # discovers *.test.js and test/ files
node --test --watch
node --test --experimental-test-coverage
```

---

## Project layout

Two common conventions. Pick one and stay consistent.

```
# A) Co-located: tests live next to the code (easy to find, move with the module)
src/modules/orders/
├── orders.service.js
├── orders.service.test.js           ← unit
├── orders.repository.js
├── orders.repository.int.test.js    ← integration (needs DB)
└── orders.routes.js

# B) Separate tree: tests mirror the source structure
test/
├── unit/
├── integration/
├── api/
├── helpers/            ← builders, auth helpers, fake factories
└── setup.js
```

Separate **fast** tests (no infrastructure) from **slow** ones (DB, containers) so developers can run the fast set on every save:

```json
{
  "scripts": {
    "test": "vitest run",
    "test:unit": "vitest run --exclude '**/*.int.test.js' --exclude '**/*.api.test.js'",
    "test:int": "vitest run int.test",
    "test:api": "vitest run api.test",
    "test:watch": "vitest",
    "test:coverage": "vitest run --coverage"
  }
}
```

---

## Testing and architecture reinforce each other

The structure from `10-architecture/` is exactly what makes testing cheap:

| Architectural choice | Testing payoff |
|---|---|
| Thin controllers, logic in services | Business rules tested without HTTP |
| Repositories behind a clear interface | Services tested with in-memory fakes; repositories tested against a real DB |
| Dependency injection (`buildApp(overrides)`) | Swap in fakes for email, payments, queues, the clock |
| `app.js` separated from `server.js` | `supertest` drives the app without opening a port |
| Injected clock, ID generator, config | Deterministic tests, no flakiness |
| Domain errors with codes | Assert on `code`, not on message text |

If a test is painful to write, listen to it: the code is probably too coupled.

---

## What's in this section

| File | What you learn |
|---|---|
| `01-unit-and-integration-testing.md` | Writing solid unit tests, using test doubles, testing async code and time, mocking modules, factories, and what integration tests add |
| `02-api-testing-and-mocking.md` | Testing HTTP endpoints with `supertest`, auth in tests, mocking external HTTP with `nock`/MSW, middleware, uploads, webhooks, contracts |
| `03-test-database-and-coverage.md` | Real databases in tests (Docker, Testcontainers), isolation strategies, migrations and seeding, parallelism, and code coverage |

Read in order: `01` is the foundation, `02` applies it to HTTP, `03` handles the hard part: data.

---

## Prerequisites

- `03-javascript-for-node/01-callbacks-promises-async-await.md`: nearly every test is `async`
- `10-architecture/03-repository-and-service-pattern.md` and `04-dependency-injection.md`: the structure that makes code testable
- `09-api-development/05-error-responses.md`: the error format your API tests assert on
- `07-databases/`: what you'll be setting up and cleaning in `03`

---

## Habits of a healthy test suite

1. **Tests must be trustworthy.** A flaky test is worse than no test, because it teaches the team to ignore red builds. Fix or delete flaky tests immediately.
2. **Keep the suite fast.** A suite that takes 20 minutes stops being run. Parallelize, avoid unnecessary I/O, and separate slow tests.
3. **Make failures informative.** Clear names and precise assertions tell you what broke without a debugger.
4. **Treat test code like production code:** refactor duplication into helpers and builders, but keep each test readable on its own (some repetition beats a clever abstraction nobody understands).
5. **Run tests in CI on every push,** and block merges on failure (`16-production/05-ci-cd.md`).
6. **Write the regression test first** when fixing a bug: reproduce it, watch it fail, fix it, watch it pass.
7. **Don't chase a coverage number.** Coverage tells you what is *untested*, not what is *well tested* (`03`).
8. **Delete tests that no longer protect anything.** Tests are a cost as well as an asset.

### A note on TDD

**Test-Driven Development** is a rhythm: write a failing test (red) → write the minimum code to pass (green) → clean up (refactor) → repeat. It isn't mandatory, but the habit of *thinking about behavior first* is valuable, and it's especially natural for bug fixes and well-defined business rules. For exploratory work, many developers write code first and add tests right after, which is fine so long as the tests do get written.

## Next

**`01-unit-and-integration-testing.md`** starts with the fundamentals: structuring unit tests, using test doubles well, handling async code, time and randomness, mocking ES modules, and where integration tests fit.
