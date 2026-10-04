# Testing Patterns

Once you know the tools, the next challenge is keeping a test suite **readable, reliable, and cheap to maintain** as it grows. This file collects the patterns that experienced teams use: builders, table-driven tests, contract tests, snapshots, property-based testing, test organization, and ways to hunt down flaky tests.

See also: [Testing Fundamentals](./01_testing-fundamentals.md), [Unit Testing](./02_unit-testing.md), [Mocking](./05_mocking.md), [Builder Pattern](../20_design-patterns/04_builder-pattern.md).

## Pattern 1: Arrange-Act-Assert with one behavior per test

```js
it('rejects an expired coupon', () => {
  // Arrange
  const cart = buildCart({ subtotal: 100 });
  const coupon = buildCoupon({ expiresAt: '2025-01-01' });

  // Act
  const result = applyCoupon(cart, coupon, { now: new Date('2026-01-01') });

  // Assert
  expect(result.ok).toBe(false);
  expect(result.reason).toBe('expired');
});
```

Each test should have **one reason to fail**. If you need `and` in the test name, consider splitting it.

## Pattern 2: Test data builders

Tests should mention only the data that **matters to that test**. A builder supplies valid defaults for everything else.

```js
let seq = 0;

export function buildUser(overrides = {}) {
  seq += 1;
  return {
    id: seq,
    name: `User ${seq}`,
    email: `user${seq}@example.com`,          // unique by default
    role: 'member',
    active: true,
    createdAt: new Date('2026-01-01T00:00:00Z'),
    ...overrides,
  };
}

export function buildOrder(overrides = {}) {
  return {
    id: `ord_${++seq}`,
    items: [buildOrderItem()],
    status: 'pending',
    ...overrides,
  };
}

export const buildOrderItem = (overrides = {}) => ({ sku: 'SKU-1', price: 10, qty: 1, ...overrides });
```

```js
it('does not email inactive users', async () => {
  const user = buildUser({ active: false });            // only the relevant field is stated
  await notifyUser(user);
  expect(mailer.sent).toHaveLength(0);
});

it('grants admins access', () => {
  expect(canEdit(buildUser({ role: 'admin' }))).toBe(true);
});
```

Benefits:

- **Readability**: the override is the point of the test
- **Maintenance**: adding a required field means editing one builder, not 200 tests
- **Isolation**: unique defaults avoid collisions (emails, IDs)

For deeply configurable objects, a fluent builder works ([Builder Pattern](../20_design-patterns/04_builder-pattern.md)):

```js
const order = anOrder().withItem({ sku: 'A', qty: 3 }).shipped().build();
```

Libraries: **Fishery**, **@faker-js/faker** (random realistic values), **rosie**. If you use random data, make it **seeded** (`faker.seed(123)`) so failures reproduce.

### Avoid: one giant shared fixture

```js
// Tests silently depend on whatever is inside this file
import data from './fixtures/everything.json';
```

Large shared fixtures hide what each test relies on, and any edit risks breaking unrelated tests. Prefer builders or small inline data.

## Pattern 3: Table-driven (parameterized) tests

When many cases follow one shape, list inputs and expected outputs:

```js
describe('shippingCost', () => {
  it.each([
    // weightKg, zone,    expected
    [0.5,      'local',   40],
    [0.5,      'national', 70],
    [5,        'local',   90],
    [5,        'national', 150],
    [30,       'national', 600],
  ])('weight %fkg to %s costs %i', (weightKg, zone, expected) => {
    expect(shippingCost({ weightKg, zone })).toBe(expected);
  });
});

// Named rows read better when there are many columns
it.each([
  { name: 'empty string', input: '', valid: false },
  { name: 'missing @', input: 'ada.example.com', valid: false },
  { name: 'plain address', input: 'ada@example.com', valid: true },
  { name: 'plus addressing', input: 'ada+news@example.com', valid: true },
])('email validation: $name', ({ input, valid }) => {
  expect(isValidEmail(input)).toBe(valid);
});
```

Guidelines:

- Keep tables **literal**: no computation of expected values inside the table
- Make each row's name explain the case
- Do not hide meaningfully different scenarios in one table: separate tests read better when setup differs

## Pattern 4: Contract tests (keep fakes honest)

When you have an interface with several implementations (real database repo, in-memory fake, a cached wrapper), write the behavioral expectations **once** and run them against each.

```js
// test/contracts/user-repo.contract.js
import { describe, it, expect, beforeEach } from 'vitest';

export function userRepoContract(name, createRepo) {
  describe(`UserRepo contract: ${name}`, () => {
    let repo;
    beforeEach(async () => { repo = await createRepo(); });

    it('finds a created user by id', async () => {
      const created = await repo.create({ name: 'Ada', email: 'ada@example.com' });
      expect(await repo.find(created.id)).toMatchObject({ name: 'Ada' });
    });

    it('returns null for an unknown id', async () => {
      expect(await repo.find('missing')).toBeNull();
    });

    it('rejects duplicate emails', async () => {
      await repo.create({ name: 'Ada', email: 'ada@example.com' });
      await expect(repo.create({ name: 'Other', email: 'ada@example.com' })).rejects.toThrow();
    });
  });
}
```

```js
// in-memory.test.js
import { userRepoContract } from './contracts/user-repo.contract.js';
import { createInMemoryUserRepo } from '../src/in-memory-user-repo.js';
userRepoContract('in-memory', () => createInMemoryUserRepo());

// postgres.test.js (slower; runs in the integration suite)
userRepoContract('postgres', async () => { await db.reset(); return createUserRepo(db.pool); });
```

If the fake and the real implementation both pass the contract, unit tests that use the fake can be trusted. A related idea for services is **consumer-driven contract testing** (Pact): the consumer records what it expects from a provider, and the provider verifies it.

## Pattern 5: Snapshot testing (use deliberately)

A snapshot stores serialized output and fails on any change.

```js
it('serializes the invoice', () => {
  expect(renderInvoice(buildOrder({ id: 'ord_1' }))).toMatchInlineSnapshot(`
    "Invoice ord_1
     1 x SKU-1 ........ 10.00
     Total ............ 10.00"
  `);
});
```

| Good for | Poor for |
|----------|----------|
| Stable text output: CLI help, generated code, rendered templates, error messages | Huge objects or entire component trees |
| Serialized data formats | Anything with timestamps, IDs, or randomness (unless normalized) |
| Catching unintended output changes in a small, well-understood surface | Logic you can express with direct assertions |

Rules to keep snapshots useful:

- Keep them **small and focused**, and prefer **inline** snapshots so reviewers see them in the test
- **Review snapshot diffs** like code; never run `-u` blindly
- **Normalize** volatile values (dates, ids) before snapshotting
- If a snapshot is hard to read, replace it with explicit assertions

```js
expect(JSON.parse(JSON.stringify(result, replacer))).toMatchSnapshot();
```

## Pattern 6: Property-based testing

Instead of hand-picking examples, state a **property** that must hold for **all** inputs, and let a library generate hundreds of random cases (and **shrink** failures to a minimal example).

```bash
npm install --save-dev fast-check
```

```js
import fc from 'fast-check';
import { it, expect } from 'vitest';

it('reversing twice gives the original array', () => {
  fc.assert(
    fc.property(fc.array(fc.integer()), (arr) => {
      expect(reverse(reverse(arr))).toEqual(arr);
    }),
  );
});

it('sorted output is ordered and a permutation of the input', () => {
  fc.assert(
    fc.property(fc.array(fc.integer()), (arr) => {
      const out = mySort(arr);
      for (let i = 1; i < out.length; i++) expect(out[i - 1]).toBeLessThanOrEqual(out[i]);
      expect(out).toHaveLength(arr.length);
      expect([...out].sort((a, b) => a - b)).toEqual([...arr].sort((a, b) => a - b));
    }),
  );
});

it('encode then decode returns the original', () => {
  fc.assert(
    fc.property(fc.string(), (s) => {
      expect(decode(encode(s))).toBe(s);
    }),
  );
});
```

Common properties to look for:

| Property | Example |
|----------|---------|
| **Round trip** | `decode(encode(x)) === x`, `parse(format(x)) === x` |
| **Idempotence** | `normalize(normalize(x)) === normalize(x)` |
| **Invariants** | Sorted output stays sorted; length is preserved; totals never go negative |
| **Oracle** | Compare against a slower, obviously correct implementation |
| **Commutativity / associativity** | `merge(a, b)` equals `merge(b, a)` when it should |

Property tests excel at finding edge cases you did not think of (empty strings, huge numbers, Unicode). When one fails, **save the failing example as a regular example test** (fast-check prints the seed, so `fc.assert(prop, { seed, path })` can replay it).

## Pattern 7: Testing error and edge behavior systematically

```js
describe('transfer', () => {
  it.each([
    { name: 'negative amount', args: [-5], error: /positive/ },
    { name: 'zero amount', args: [0], error: /positive/ },
    { name: 'NaN', args: [NaN], error: /number/ },
    { name: 'more than balance', args: [1_000], error: /insufficient/i },
  ])('rejects $name', async ({ args, error }) => {
    const account = buildAccount({ balance: 100 });
    await expect(account.withdraw(...args)).rejects.toThrow(error);
    expect(account.balance).toBe(100);                   // state unchanged on failure
  });
});
```

Always check that a **failed operation leaves state unchanged** (no partial writes), and that errors are the **right type** with a useful message.

## Pattern 8: Setup helpers and the "test harness"

When many tests need the same object graph, wrap setup in a function that returns what tests need:

```js
function setup(overrides = {}) {
  const mailer = createSpyMailer();
  const userRepo = createInMemoryUserRepo();
  const clock = { now: new Date('2026-01-01T00:00:00Z') };

  const service = createSignupService({ mailer, userRepo, now: () => clock.now, ...overrides });
  return { service, mailer, userRepo, clock };
}

it('sends a welcome email', async () => {
  const { service, mailer } = setup();
  await service.signup({ email: 'ada@example.com' });
  expect(mailer.sent).toHaveLength(1);
});

it('blocks signups that already exist', async () => {
  const { service, userRepo } = setup();
  await userRepo.create({ email: 'ada@example.com' });
  await expect(service.signup({ email: 'ada@example.com' })).rejects.toThrow(/exists/);
});
```

Prefer a **function called in each test** over a shared `let` assigned in `beforeEach`: the data flow is explicit, and unused parts are not created. Use `beforeEach` for simple shared setup.

## Pattern 9: Page objects and user-centric helpers (UI and E2E)

Hide selectors and low-level steps behind domain actions:

```js
class LoginPage {
  constructor(page) { this.page = page; }

  async open() { await this.page.goto('/login'); }

  async loginAs({ email, password }) {
    await this.page.getByLabel('Email').fill(email);
    await this.page.getByLabel('Password').fill(password);
    await this.page.getByRole('button', { name: 'Sign in' }).click();
  }

  async errorMessage() { return this.page.getByRole('alert').textContent(); }
}

test('shows an error for a wrong password', async ({ page }) => {
  const login = new LoginPage(page);
  await login.open();
  await login.loginAs({ email: 'ada@example.com', password: 'wrong' });
  await expect(page.getByRole('alert')).toContainText('Invalid credentials');
});
```

When the UI changes, you update one class, not every test. Query by **role, label, and text** (what users perceive) rather than CSS classes or XPath.

## Pattern 10: Organizing and naming

```
src/
  cart/
    cart.js
    cart.test.js            ← unit tests next to the code
test/
  integration/
    users-api.int.test.js   ← slower tests grouped separately
  e2e/
    checkout.spec.js
  helpers/
    builders.js
    in-memory-repos.js
  contracts/
    user-repo.contract.js
```

| Convention | Why |
|------------|-----|
| Co-locate unit tests | Easy to find; moved with the code |
| Distinct names/folders for integration and E2E | Run fast tests constantly and slow ones less often |
| `describe` by feature or unit, `it` by behavior | Names read as sentences |
| Shared builders and fakes in `test/helpers` | Reuse without copy-paste |
| Name tests by **behavior and condition** | `it('returns 404 when the user does not exist')` |

## Pattern 11: Test the contract, not the implementation (refactoring safety)

A good test survives refactors:

```js
// Before: array-backed
class Playlist { #songs = []; add(s) { this.#songs.push(s); } get length() { return this.#songs.length; } }

// After: Set-backed (deduplicates)
class Playlist { #songs = new Set(); add(s) { this.#songs.add(s); } get length() { return this.#songs.size; } }

// This test works for both because it checks behavior through the public API
it('counts added songs', () => {
  const p = new Playlist();
  p.add('a'); p.add('b');
  expect(p.length).toBe(2);
});
```

When you refactor and tests break while behavior is intact, the tests were coupled to implementation. Fix the **tests**, not just the symptoms.

## Flaky tests

A **flaky** test passes and fails without code changes. It erodes trust: people re-run CI and ignore failures, and real bugs slip through.

### Common causes and fixes

| Cause | Fix |
|-------|-----|
| **Fixed sleeps / timing assumptions** | Wait for a condition (`waitFor`, events, promises); fake timers |
| **Shared state between tests** | Isolate: fresh instances, reset database and globals, restore mocks |
| **Test order dependence** | Run with `--shuffle`; remove hidden coupling |
| **Real time, time zones, daylight saving** | Fake or inject the clock; set `TZ=UTC` |
| **Randomness** | Seed or inject it; log the seed on failure |
| **Network and external services** | Mock at the boundary; never hit real services |
| **Concurrency and race conditions** | Await everything; avoid unordered assertions on async arrivals |
| **Port, file, and resource conflicts in parallel runs** | Port `0`, temp directories, per-worker databases |
| **Unawaited promises** | `await` or `return`; lint for floating promises |
| **Resource leaks (open handles)** | Close servers, pools, timers in `afterAll` |
| **Slow machines in CI** | Avoid tight timeouts; make tests independent of speed |
| **Animations and async rendering in UI tests** | Use auto-waiting locators and assertions |
| **Order of unordered data** (object keys, `Set`, DB rows without `ORDER BY`) | Sort before asserting, or use order-insensitive matchers |

### A process for flaky tests

1. **Reproduce**: run the test many times (a shell loop such as `for i in $(seq 100); do npx vitest run file.test.js || break; done`, or your runner's repeat option), under CPU load, in random order, and in isolation
2. **Find the nondeterminism**: time, order, shared state, async, or environment
3. **Fix the root cause**, not the symptom (adding retries or longer timeouts hides it)
4. **Quarantine** if you cannot fix quickly: mark it skipped with a ticket and an owner, so it does not block the team, and does not vanish
5. **Track** flaky tests (CI dashboards) and treat a rising rate as a quality incident

Retries (`retry: 2`) are acceptable as a **stopgap or for E2E infrastructure noise**, never as a first response to flakiness in unit tests.

## Mutation testing (checking the tests themselves)

Coverage shows what **ran**; **mutation testing** shows what is actually **checked**. A tool (Stryker) makes small changes to your code (`>` to `>=`, `+` to `-`, deleting a statement) and reruns the tests. If tests still pass, the mutant **survived**, which means the tests do not really verify that code.

```bash
npm install --save-dev @stryker-mutator/core @stryker-mutator/vitest-runner
npx stryker init
npx stryker run
```

Slow, so run it periodically or on critical modules, not on every commit.

## Anti-patterns to avoid

| Anti-pattern | Description | Better |
|--------------|-------------|--------|
| **The Liar** | Passes, but asserts nothing meaningful | Assert behavior; check by temporarily breaking the code |
| **The Giant** | One test with dozens of steps and assertions | Focused tests |
| **The Mockery** | So many mocks that the test only checks the mocks | Fakes and real collaborators |
| **The Inspector** | Reaches into private state | Public interface only |
| **The Slow Poke** | Real network, sleeps, big setups in unit tests | Fakes, fake timers, builders |
| **The Free Ride** | Extra assertions piled onto an existing test | New test per behavior |
| **The Local Hero** | Passes only on one machine (paths, time zone, env) | Hermetic tests, same CI environment |
| **Conditional logic in tests** | `if`/loops make the test itself buggy | Table-driven tests with literals |
| **Copy-paste setup** | Same 30 lines in every test | Builders and setup helpers |
| **Commented-out or skipped tests** | Dead weight, false comfort | Fix or delete (git remembers) |
| **Testing the framework** | Verifying that `Array.map` works | Test your logic |
| **Asserting on exact error text from libraries** | Breaks on dependency upgrades | Match a stable fragment or error type |

## A checklist for a test you are about to commit

- [ ] Its name says **what behavior** and **under what condition**
- [ ] It fails if the behavior breaks (you saw it fail once)
- [ ] It tests **one thing** and has one reason to fail
- [ ] It does not depend on **other tests, order, time, randomness, or the network**
- [ ] It uses **public behavior**, not private details
- [ ] Setup shows only the data that matters (builders)
- [ ] Mocks are limited to **boundaries** and restored afterwards
- [ ] It runs **fast** and cleans up everything it opens

## Key takeaways

- One behavior per test; keep the Arrange, Act, Assert phases clear
- Use **builders** so each test states only the data that matters; avoid giant shared fixtures
- Use **table-driven** tests for many similar cases, and keep tables literal
- Run **contract tests** against fakes and real implementations to keep doubles honest
- Use **snapshots** sparingly: small, inline, reviewed, with volatile values normalized
- Use **property-based tests** for invariants, round trips, and edge cases you would not think of
- Organize tests by feature, separate slow suites from fast ones, and name tests by behavior
- Treat **flaky tests** as bugs: find the nondeterminism (time, order, shared state, async) and fix the root cause
- Tests should survive refactors: if they do not, they were coupled to implementation
- Use mutation testing occasionally to check whether tests truly verify behavior

**Next:** [Security](../22_security/00_README.md)
