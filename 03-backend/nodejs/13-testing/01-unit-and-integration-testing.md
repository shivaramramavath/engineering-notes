# Unit & Integration Testing

Writing solid unit tests, using test doubles well, handling async code, time and randomness, mocking ES modules — and understanding what integration tests add.

> Examples use the Jest-style API (`describe`, `test`, `expect`), which Vitest shares. In ESM Jest, add `import { jest } from "@jest/globals";`. In Vitest, replace `jest` with `vi` (`import { vi } from "vitest"`).

---

# Part 1 — Unit Tests

## What a unit test is

A **unit test** checks one small piece of behavior (a function, a class, a service method) **in isolation**: no database, no network, no filesystem, no real clock. Everything the unit depends on is replaced with a controlled stand-in.

The payoff is speed and precision: thousands run in seconds, and a failure points at one place.

```js
// src/modules/orders/pricing.js
export function calculateTotal(user, items) {
  const subtotal = items.reduce((sum, i) => sum + i.priceCents * i.quantity, 0);
  return user.isPremium ? Math.round(subtotal * 0.9) : subtotal;
}
```

```js
// src/modules/orders/pricing.test.js
import { calculateTotal } from "./pricing.js";

describe("calculateTotal", () => {
  test("sums price × quantity for regular customers", () => {
    const total = calculateTotal({ isPremium: false }, [
      { priceCents: 500, quantity: 2 },
      { priceCents: 250, quantity: 1 },
    ]);
    expect(total).toBe(1250);
  });

  test("gives premium customers 10% off", () => {
    expect(calculateTotal({ isPremium: true }, [{ priceCents: 1000, quantity: 2 }])).toBe(1800);
  });

  test("returns 0 for an empty cart", () => {
    expect(calculateTotal({ isPremium: true }, [])).toBe(0);
  });

  test("rounds to whole cents", () => {
    // 333 × 0.9 = 299.7 → 300
    expect(calculateTotal({ isPremium: true }, [{ priceCents: 333, quantity: 1 }])).toBe(300);
  });
});
```

Notice the pattern: normal case, the special rule, boundary (empty), and a rounding edge. Four tests, each with a name that reads as a specification.

---

## Structure: `describe`, `test`, and hooks

```js
describe("OrderService", () => {
  let service, repo, mailer;                      // shared per-suite variables

  beforeEach(() => {                              // runs before EVERY test: fresh state, so tests can't leak into each other
    repo = makeFakeOrderRepo();
    mailer = makeFakeMailer();
    service = makeOrderService({ repo, mailer });
  });

  afterEach(() => {
    jest.restoreAllMocks();                       // undo spies/mocks
  });

  describe("placeOrder", () => {
    test("stores the order", async () => { /* ... */ });
    test("emails the customer", async () => { /* ... */ });
  });

  describe("cancelOrder", () => {
    test("rejects shipped orders", async () => { /* ... */ });
  });
});
```

| Hook | Runs | Use for |
|---|---|---|
| `beforeEach` / `afterEach` | Around **every** test | Fresh fakes, reset mocks (the default choice) |
| `beforeAll` / `afterAll` | Once per `describe` block | Expensive one-time setup: start a container, connect to a DB |

Prefer `beforeEach` with fresh objects over `beforeAll` with shared mutable state. Shared state is the number one source of tests that pass alone and fail together.

### Useful modifiers

```js
test.skip("flaky thing, fix tomorrow", ...);     // skipped, shown in the report (don't leave these forever)
test.only("debugging this one", ...);            // run only this (never commit it)
test.todo("handles currency conversion");         // a reminder that appears in the output

test.each([
  // [input, expected]
  [0, 0],
  [999, 999],
  [1000, 900],
])("applies the bulk discount: %i cents → %i", (input, expected) => {
  expect(applyBulkDiscount(input)).toBe(expected);
});
```

`test.each` is ideal for **boundaries and tables of cases** (validation rules, tax brackets, state machines) without copy-pasting the test body.

---

## Assertions you'll use constantly

```js
// primitives and identity
expect(total).toBe(1800);                       // strict equality (===), for numbers/strings/booleans/same reference
expect(result).toBeNull();
expect(value).toBeDefined();
expect(flag).toBeTruthy();

// objects and arrays: compare by VALUE with toEqual
expect(order).toEqual({ id: "o1", status: "pending", lines: [] });

// a SUBSET of properties (resilient to extra fields like timestamps)
expect(order).toMatchObject({ status: "pending", userId: "u1" });

// asymmetric matchers for values you can't predict
expect(order).toEqual({
  id: expect.any(String),
  createdAt: expect.any(Date),
  lines: expect.arrayContaining([expect.objectContaining({ productId: "p1" })]),
});

// arrays/collections
expect(list).toHaveLength(3);
expect(list).toContain("admin");
expect(list).toContainEqual({ id: 1 });

// numbers
expect(0.1 + 0.2).toBeCloseTo(0.3);             // floating point: never use toBe
expect(count).toBeGreaterThan(0);

// strings
expect(message).toMatch(/not found/i);

// negation
expect(list).not.toContain("banned");
```

`toBe` vs `toEqual` trips up everyone at first: `toBe` is `Object.is` (same reference for objects), while `toEqual` compares contents recursively.

---

## Testing async code

Every async test must **return or await its promise**, or the test finishes before anything is checked and passes falsely.

```js
// ✅ async/await
test("loads a user", async () => {
  const user = await service.getById("u1");
  expect(user.email).toBe("a@b.com");
});

// ❌ forgot await: the assertion runs after the test already "passed"
test("loads a user", () => {
  service.getById("u1").then((user) => expect(user.email).toBe("wrong@example.com"));   // never checked!
});
```

### Testing rejections and thrown errors

```js
// async: assert the promise rejects
await expect(service.getById("missing")).rejects.toThrow("User not found");
await expect(service.getById("missing")).rejects.toMatchObject({ code: "not_found", status: 404 });

// sync: wrap the call in a function
expect(() => Order.place({ items: [] })).toThrow(EmptyOrderError);
expect(() => parseConfig({})).toThrow(/DATABASE_URL/);
```

Assert on the error's **`code` or class**, not its message text, so rewording a message doesn't break tests (`09-api-development/05-error-responses.md`).

```js
// ❌ the try/catch that silently passes when nothing throws
test("throws on bad input", async () => {
  try {
    await service.register({ email: "x" });
  } catch (err) {
    expect(err.code).toBe("validation_error");     // if register() doesn't throw at all, this test STILL passes
  }
});

// ✅ guarantee that an assertion ran
test("throws on bad input", async () => {
  expect.assertions(1);
  try { await service.register({ email: "x" }); } catch (err) { expect(err.code).toBe("validation_error"); }
});
// ...or simply use .rejects (preferred)
```

### Concurrency inside a test

```js
test("handles 10 concurrent registrations of the same email", async () => {
  const results = await Promise.allSettled(
    Array.from({ length: 10 }, () => service.register({ email: "dup@example.com", name: "A", password: "x".repeat(12) }))
  );
  expect(results.filter((r) => r.status === "fulfilled")).toHaveLength(1);   // only one wins
});
```

(Race-condition tests are most meaningful against a real database, which is where unique constraints actually live: see Part 2 and `03`.)

---

## Test doubles in practice

### Stubs and fakes: prefer these

Because services receive their dependencies (`10-architecture/04-dependency-injection.md`), tests just pass what they need:

```js
// A fake: a tiny working implementation, reusable across tests
export function makeFakeUserRepo(initial = []) {
  const users = new Map(initial.map((u) => [u.id, u]));
  return {
    async findById(id) { return users.get(id) ?? null; },
    async findByEmail(email) { return [...users.values()].find((u) => u.email === email) ?? null; },
    async create(data) {
      const user = { id: `u${users.size + 1}`, ...data };
      users.set(user.id, user);
      return user;
    },
    _all: () => [...users.values()],              // test-only inspection helper
  };
}
```

```js
test("register stores a hashed password and returns the user", async () => {
  const repo = makeFakeUserRepo();
  const service = makeAuthService({
    userRepository: repo,
    passwordHasher: { hash: async (p) => `hashed:${p}` },       // a stub: instant and deterministic
    logger: { info() {}, error() {} },                           // a dummy
  });

  const user = await service.register({ email: "a@b.com", name: "A", password: "secret-password" });

  expect(repo._all()).toHaveLength(1);
  expect(repo._all()[0].passwordHash).toBe("hashed:secret-password");   // asserts on OUTCOME
  expect(user).not.toHaveProperty("passwordHash");                       // never leak the hash
});
```

An **in-memory fake** shines when a flow touches the repository several times: it behaves like the real thing, so the test reads like a story instead of a pile of `mockResolvedValueOnce` calls.

**Keep fakes honest.** A fake that behaves differently from the real repository hides bugs. Run the *same contract tests* against both the fake and the real implementation (see "Contract tests for fakes" in Part 2).

### Spies and mocks: for verifying side effects

Use `jest.fn()` when the *call itself* is the behavior you care about ("an email was sent"):

```js
test("emails the customer after placing an order", async () => {
  const sendConfirmation = jest.fn().mockResolvedValue(undefined);
  const service = makeOrderService({ ...deps, mailer: { sendConfirmation } });

  await service.placeOrder({ userId: "u1", items: [{ productId: "p1", quantity: 1 }] });

  expect(sendConfirmation).toHaveBeenCalledTimes(1);
  expect(sendConfirmation).toHaveBeenCalledWith("a@b.com", expect.objectContaining({ id: expect.any(String) }));
});
```

Common mock controls:

```js
const fn = jest.fn();
fn.mockReturnValue(42);                       // sync return
fn.mockResolvedValue({ ok: true });           // async success
fn.mockRejectedValue(new Error("boom"));      // async failure
fn.mockResolvedValueOnce(a).mockResolvedValueOnce(b);   // different results per call
fn.mockImplementation((x) => x * 2);

fn.mock.calls;                                // [[arg1, arg2], ...]: inspect every call
fn.mockClear();                               // reset call history
fn.mockReset();                               // + reset implementations
```

`jest.spyOn(object, "method")` wraps a real method so you can observe it (or override it), and `mockRestore()` puts it back:

```js
const spy = jest.spyOn(console, "error").mockImplementation(() => {});     // silence expected noise
// ...
expect(spy).toHaveBeenCalledWith(expect.stringContaining("Email failed"));
spy.mockRestore();
```

### Testing that something did *not* happen

```js
expect(sendConfirmation).not.toHaveBeenCalled();     // e.g. an order that failed validation must not send an email
expect(repo._all()).toHaveLength(0);                  // and nothing was stored
```

### Don't over-mock

```js
// ❌ a test that's mostly mocks, asserting a script of calls: proves the code matches itself, not that it's correct
repo.findByEmail.mockResolvedValueOnce(null);
hasher.hash.mockResolvedValueOnce("h");
repo.create.mockResolvedValueOnce({ id: "1" });
await service.register(input);
expect(repo.findByEmail).toHaveBeenCalledWith("a@b.com");
expect(hasher.hash).toHaveBeenCalledWith("pw");
expect(repo.create).toHaveBeenCalledWith({ email: "a@b.com", passwordHash: "h" });
```

If every line of the production function has a matching `expect(...).toHaveBeenCalledWith`, the test restates the implementation, and any refactor breaks it for no benefit. Assert on **results and meaningful side effects**.

---

## Time, randomness, and other non-determinism

Tests must produce the same result every run. Anything that varies is a flakiness source.

### Time

**Best: inject a clock** (`10-architecture/04-dependency-injection.md`):

```js
function makeTokenService({ clock = () => Date.now() } = {}) {
  return { issue: (userId) => ({ userId, expiresAt: clock() + 15 * 60_000 }) };
}

test("tokens expire after 15 minutes", () => {
  const tokens = makeTokenService({ clock: () => 1_000_000 });
  expect(tokens.issue("u1").expiresAt).toBe(1_900_000);
});
```

**Otherwise: fake timers**, which control `Date`, `setTimeout`, and `setInterval`:

```js
beforeEach(() => jest.useFakeTimers());
afterEach(() => jest.useRealTimers());

test("expires a pending order after 30 minutes", async () => {
  jest.setSystemTime(new Date("2026-09-30T10:00:00Z"));
  const order = await service.placeOrder(input);

  jest.advanceTimersByTime(31 * 60_000);          // jump 31 minutes ahead instantly
  await jest.runOnlyPendingTimersAsync();          // let timer callbacks (and their promises) finish

  expect((await repo.findById(order.id)).status).toBe("expired");
});
```

Fake timers pair poorly with real I/O (a real database connection can hang waiting for a timer that never advances). Use them for pure logic and in-process scheduling, not in integration tests.

### Randomness and IDs

```js
function makeOrderFactory({ generateId = () => crypto.randomUUID() } = {}) { /* ... */ }

const factory = makeOrderFactory({ generateId: () => "fixed-id-1" });     // deterministic in tests
```

Or assert on *shape* instead of value: `expect(order.id).toMatch(/^[0-9a-f-]{36}$/)`.

### Environment and configuration

```js
const original = process.env;
beforeEach(() => { process.env = { ...original, STRIPE_KEY: "sk_test_x" }; });
afterEach(() => { process.env = original; });
```

Better: have code accept config as a parameter (`makeMailer({ apiKey })`) so tests don't mutate globals.

### Other usual suspects

- **Test order dependence:** tests that only pass when run in a certain order (shared state)
- **Real network calls:** a dependency on a live service (use `nock`/MSW, see `02`)
- **Timeouts that are too tight** on slow CI machines
- **Unawaited promises** that finish after the test does
- **Port collisions:** use port `0`
- **Locale/timezone differences:** set `TZ=UTC` in the test script

---

## Mocking modules

DI avoids most module mocking. When you must replace an import (a legacy module with a hard-wired dependency, a third-party SDK created at import time), here's how.

### Vitest

```js
import { vi } from "vitest";

vi.mock("../lib/sendgrid.js", () => ({
  sendgrid: { send: vi.fn().mockResolvedValue({ id: "msg_1" }) },
}));

import { sendgrid } from "../lib/sendgrid.js";          // gets the mocked version
import { notifyUser } from "./notify.js";

test("notifyUser sends one email", async () => {
  await notifyUser("u1");
  expect(sendgrid.send).toHaveBeenCalledTimes(1);
});
```

`vi.mock` is hoisted to the top of the file automatically.

### Jest with native ES modules

`jest.mock` doesn't work on native ESM imports. Use `jest.unstable_mockModule` and **dynamically import** the code under test *after* mocking:

```js
import { jest } from "@jest/globals";

const sendMock = jest.fn().mockResolvedValue({ id: "msg_1" });

jest.unstable_mockModule("../lib/sendgrid.js", () => ({
  sendgrid: { send: sendMock },
}));

const { notifyUser } = await import("./notify.js");      // import AFTER the mock is registered

test("notifyUser sends one email", async () => {
  await notifyUser("u1");
  expect(sendMock).toHaveBeenCalledTimes(1);
});
```

### Node's built-in runner

```js
import { mock, test } from "node:test";
import assert from "node:assert/strict";

test("spies on a method", () => {
  const m = mock.method(service, "send", async () => "ok");
  // ...
  assert.equal(m.mock.callCount(), 1);
});
```

Mocking modules works, but it couples tests to **file paths and import structure**. If you find yourself doing it constantly, that's a design smell: pass the dependency in instead.

---

## Test data: builders and factories

Constructing full objects in every test is noisy and brittle: add one required field and 60 tests break. Use **builders** that provide sensible defaults and let each test override only what it cares about.

```js
// test/helpers/builders.js
let seq = 0;

export const buildUser = (overrides = {}) => ({
  id: `user-${++seq}`,
  email: `user${seq}@example.com`,
  name: "Test User",
  isPremium: false,
  role: "user",
  createdAt: new Date("2026-01-01T00:00:00Z"),
  ...overrides,
});

export const buildProduct = (overrides = {}) => ({
  id: `prod-${++seq}`,
  name: "Pen",
  priceCents: 500,
  stock: 100,
  ...overrides,
});
```

```js
test("rejects when stock is insufficient", async () => {
  const product = buildProduct({ stock: 1 });                  // only what matters is spelled out
  const user = buildUser({ isPremium: true });
  // ...
});
```

Libraries: **`@faker-js/faker`** (realistic random data; seed it for repeatability: `faker.seed(123)`), and **`fishery`** (a small factory library). Hand-rolled builders like the above are often enough.

Rules for test data:

- **Make the important fields visible in the test.** If a test depends on `isPremium: true`, set it explicitly instead of hiding it in a default.
- **Don't use production data** (privacy, PII).
- **Use unique values** where uniqueness constraints exist (emails), so tests don't collide.

---

## Testing other Node.js things

### Event emitters

```js
import { once } from "node:events";

test("emits 'order.placed' after a successful order", async () => {
  const bus = new EventEmitter();
  const service = makeOrderService({ ...deps, bus });

  const eventPromise = once(bus, "order.placed");          // start listening BEFORE triggering
  await service.placeOrder(input);
  const [payload] = await eventPromise;

  expect(payload).toMatchObject({ orderId: expect.any(String) });
});
```

### Streams

```js
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";

test("transforms CSV rows to JSON lines", async () => {
  const input = Readable.from(["a,b\n", "1,2\n"]);
  const chunks = [];
  await pipeline(input, csvToJsonLines(), async function* (source) {
    for await (const chunk of source) chunks.push(chunk.toString());
  });
  expect(chunks.join("")).toBe('{"a":"1","b":"2"}\n');
});
```

### Filesystem

Use a **temporary directory** per test and clean it up, or an in-memory FS (`memfs`) for pure logic:

```js
import { mkdtemp, rm, readFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import path from "node:path";

let dir;
beforeEach(async () => { dir = await mkdtemp(path.join(tmpdir(), "test-")); });
afterEach(async () => { await rm(dir, { recursive: true, force: true }); });

test("writes the report", async () => {
  await saveReport(path.join(dir, "out.csv"), rows);
  expect(await readFile(path.join(dir, "out.csv"), "utf8")).toContain("id,name");
});
```

(See `02-core-modules/01-fs.md`.)

### Snapshot tests: use sparingly

```js
expect(renderInvoice(order)).toMatchSnapshot();       // saved to a file; later runs must match
expect(error).toMatchInlineSnapshot(`{ "code": "not_found" }`);
```

Snapshots are convenient for **large, stable text output** (rendered emails, generated SQL, CLI help). But they invite "update snapshot and move on" without review, and they assert *everything*, so unrelated changes break them. Prefer targeted assertions for logic, and review snapshot diffs in code review like any other change.

---

## Going further

- **Property-based testing** (`fast-check`): instead of hand-picking inputs, state a rule ("for any cart, total ≥ 0 and total ≤ subtotal") and let the library generate hundreds of inputs, shrinking failures to the smallest case. Excellent for parsers, pricing, and serialization.
- **Mutation testing** (Stryker): automatically changes your code (flips `>` to `>=`, removes a line) and checks that tests *fail*. Survivors show where tests exist but don't actually assert anything. See `03-test-database-and-coverage.md`.
- **Test-driven bug fixes:** reproduce the bug as a failing test first.

---

# Part 2 — Integration Tests

## What changes with integration tests

A unit test proves a **piece** is right when its neighbors behave as you imagined. An integration test proves the **pieces fit together** with real neighbors: your SQL against a real PostgreSQL, your cache logic against a real Redis, your queue code against real BullMQ.

Most bugs that survive unit testing live at these seams:

| Bug a unit test with fakes can't catch | Why |
|---|---|
| SQL typo, wrong column name, missing `WHERE` | The fake repository doesn't run SQL |
| Missing unique index → duplicates possible | The fake enforces uniqueness in JavaScript, the database enforces it with an index you forgot |
| Transaction doesn't roll back on failure | The fake has no transactions |
| JSON/JSONB, date, or numeric type mismatches (`pg` returns `bigint` as a string) | The fake returns whatever you stored |
| Cursor pagination returns duplicates under ties | No real `ORDER BY` semantics in memory |
| Redis key TTL or expiry behavior | A `Map` has no TTL |
| Connection pool exhaustion, deadlocks | Only real I/O reveals them |

### The simplest integration test: a repository against a real DB

```js
// src/modules/users/users.repository.int.test.js
import { pool } from "../../../test/helpers/db.js";           // connects to the TEST database (see 03)
import { makeUserRepository } from "./users.repository.js";

const repo = makeUserRepository({ db: pool });

beforeEach(async () => { await pool.query("TRUNCATE users RESTART IDENTITY CASCADE"); });
afterAll(async () => { await pool.end(); });

test("create then findByEmail round-trips", async () => {
  const created = await repo.create({ email: "a@b.com", name: "A", passwordHash: "h" });

  const found = await repo.findByEmail("a@b.com");

  expect(found).toMatchObject({ id: created.id, email: "a@b.com", name: "A" });
});

test("a duplicate email violates the unique constraint", async () => {
  await repo.create({ email: "dup@b.com", name: "A", passwordHash: "h" });

  await expect(repo.create({ email: "dup@b.com", name: "B", passwordHash: "h" }))
    .rejects.toMatchObject({ code: "23505" });                   // PostgreSQL unique_violation: a unit test can't prove this
});

test("findByEmail returns null for unknown addresses", async () => {
  expect(await repo.findByEmail("nobody@example.com")).toBeNull();
});
```

How to provide that test database, keep tests isolated, and run them in parallel is the subject of `03-test-database-and-coverage.md`.

### Contract tests for fakes

If you use an in-memory fake in unit tests, **prove it behaves like the real repository** by running one shared suite against both:

```js
// test/contracts/userRepository.contract.js
export function userRepositoryContract(name, makeRepo) {
  describe(`${name} (userRepository contract)`, () => {
    let repo;
    beforeEach(async () => { repo = await makeRepo(); });

    test("create then findById returns the user", async () => {
      const u = await repo.create({ email: "a@b.com", name: "A", passwordHash: "h" });
      expect(await repo.findById(u.id)).toMatchObject({ email: "a@b.com" });
    });

    test("findByEmail is case-insensitive", async () => {
      await repo.create({ email: "Sam@Example.com", name: "S", passwordHash: "h" });
      expect(await repo.findByEmail("sam@example.com")).not.toBeNull();
    });

    test("findById returns null when missing", async () => {
      expect(await repo.findById("00000000-0000-0000-0000-000000000000")).toBeNull();
    });
  });
}
```

```js
// run it against BOTH implementations
userRepositoryContract("in-memory", async () => makeFakeUserRepo());
userRepositoryContract("postgres", async () => { await resetDb(); return makeUserRepository({ db: pool }); });
```

When a rule differs (case-insensitive email in Postgres but not in the fake), the contract test fails on one implementation and tells you the fake is lying.

### Integration tests for caches, queues, and other infrastructure

```js
// Redis-backed cache: real TTL behavior
test("cached values expire", async () => {
  await cache.set("k", "v", { ttlSeconds: 1 });
  expect(await cache.get("k")).toBe("v");
  await new Promise((r) => setTimeout(r, 1200));
  expect(await cache.get("k")).toBeNull();
});

// BullMQ: job flows through a real queue and worker (11-async-processing/01-queues-and-bullmq.md)
test("a welcome-email job is processed", async () => {
  const queue = new Queue(`test-${randomUUID()}`, { connection });       // unique queue name per test
  const processed = new Promise((resolve) => {
    new Worker(queue.name, async (job) => job.data.userId, { connection }).on("completed", (_j, result) => resolve(result));
  });

  await queue.add("welcome", { userId: "u1" });

  expect(await processed).toBe("u1");
  await queue.obliterate({ force: true });
});
```

Real-time waits (`setTimeout`) make tests slow and flaky. Where possible, use short TTLs, a controllable clock, or **poll until a condition is true** with a timeout:

```js
async function waitFor(fn, { timeout = 2000, interval = 25 } = {}) {
  const start = Date.now();
  for (;;) {
    try { return await fn(); } catch (err) {
      if (Date.now() - start > timeout) throw err;
      await new Promise((r) => setTimeout(r, interval));
    }
  }
}

await waitFor(async () => expect(await repo.findById(id)).toMatchObject({ status: "processed" }));
```

(Vitest provides `vi.waitFor`; Testing Library's `waitFor` works similarly.)

---

## Unit vs integration: how to choose

| Question | Lean toward |
|---|---|
| Is this a pure business rule or calculation? | **Unit** (fast, exhaustive edge cases) |
| Does correctness depend on SQL, an index, a transaction, or Redis semantics? | **Integration** against the real thing |
| Am I mostly checking that A calls B correctly? | Probably an **API/integration test** instead of a mock-heavy unit test |
| Is the dependency external and uncontrollable (Stripe, SendGrid)? | **Fake it** at the boundary (`nock`/MSW, `02`) |
| Is it slow to set up but critical? | Few, targeted integration tests |

A reliable recipe for each module:

1. **Unit tests** for the service's business rules, with in-memory fakes for repositories.
2. **Integration tests** for the repository, with a real database.
3. **A couple of API tests** for the endpoints (`02`), which exercise the layers together.

### Keep integration tests healthy

- **Isolate every test:** clean or roll back data so tests don't depend on each other (`03`).
- **Use unique data** (random emails/IDs) when you can't reset between tests.
- **Don't mock what you're integrating:** the point is to touch the real component.
- **Close what you open:** pools, clients, servers, workers. Otherwise Jest hangs with open handles (`--detectOpenHandles` helps find them).
- **Keep them separate from unit tests** (naming convention `*.int.test.js` or a folder) so the fast suite stays fast.
- **Make them runnable by anyone:** one command (`docker compose up -d` or Testcontainers) should provide the needed infrastructure, locally and in CI.

---

## Common mistakes

```js
// ❌ tests that share mutable state and depend on order
let user;                                       // created in test 1, used in test 2
test("creates a user", async () => { user = await repo.create(...); });
test("finds the user", async () => { expect(await repo.findById(user.id)).toBeDefined(); });   // fails if run alone

// ❌ forgetting to await (assertions never run, test passes falsely)
// ❌ asserting on error messages instead of codes/types
// ❌ asserting call sequences on mocks instead of outcomes
// ❌ real clock/randomness/network in tests → flaky
// ❌ giant tests that arrange 40 lines and assert 15 unrelated things
// ❌ `if` statements, loops, or try/catch logic inside tests (the test itself needs testing)
// ❌ copy-pasted production logic inside the test to compute the "expected" value (tests the code against itself)
expect(total).toBe(subtotal * (user.isPremium ? 0.9 : 1));      // ❌ re-implements the rule: use a literal: toBe(1800)

// ❌ unit-testing with a hand-written fake that has drifted from the real repository
// ❌ leaving test.only / test.skip committed
// ❌ leaking open handles (DB pools, servers, timers) so the test process never exits
// ❌ snapshot-testing everything and blindly updating snapshots
```

## Checklist

- [ ] Each test checks one behavior, named as a scenario and expectation (`rejects an order when stock is insufficient`)
- [ ] Arrange–Act–Assert, with the important inputs visible in the test
- [ ] Fresh state per test (`beforeEach`), no order dependence
- [ ] Async tests `await` everything; errors asserted with `.rejects` / `toThrow` on **codes or classes**
- [ ] Clock, IDs, and randomness injected or faked; no real network in unit tests
- [ ] Prefer fakes/stubs and outcome assertions; mocks only for meaningful side effects
- [ ] Builders/factories for test data; no production data
- [ ] Business rules unit-tested (including boundaries); repositories integration-tested against a real DB
- [ ] Fakes verified by shared contract tests against the real implementation
- [ ] Integration tests isolated, cleaned up, and kept in a separate (slower) suite
- [ ] Every opened resource (pool, client, server, worker) is closed in `afterAll`

## Next

**`02-api-testing-and-mocking.md`** moves up to the HTTP layer: driving the whole app with `supertest`, handling authentication in tests, faking third-party HTTP calls with `nock` and MSW, and testing middleware, uploads, and webhooks.
