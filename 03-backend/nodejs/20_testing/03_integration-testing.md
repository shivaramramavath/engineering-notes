# Integration Testing

**Integration tests** check that several pieces **work together**: your modules with each other, your code with a real database, an HTTP layer with its routes and middleware. Unit tests prove each part works alone; integration tests prove the **wiring** is right: queries, serialization, configuration, authentication, and error handling across boundaries.

See also: [Unit Testing](./02_unit-testing.md), [HTTP Server](../16_nodejs/08_http-server.md), [Dependency Injection](../20_design-patterns/09_dependency-injection.md), [Mocking](./05_mocking.md).

## Why unit tests are not enough

```js
// Every unit test passes...
userService.create(...)            // tested with a fake repository: works
userRepository.insert(...)         // tested with a fake database: works

// ...but in production:
// - the SQL has a typo
// - the column is named created_at, the code writes createdAt
// - the JSON body parser is not registered, so req.body is undefined
// - the auth middleware runs after the route
```

These bugs live **between** components. Only tests that exercise the real connections find them.

## Scope: what counts as integration

| Scope | Example | Real | Faked |
|-------|---------|------|-------|
| **Several modules, in-process** | Service + validation + mapping | All your code | External systems |
| **HTTP layer** | Router, middleware, handlers via requests | App code, serializers | Database (maybe), third-party APIs |
| **Data layer** | Repository against a real database | Database engine, SQL, migrations | Everything above the repository |
| **Full service** | App + real database + queue | Your service and its infrastructure | Third-party SaaS |
| **Cross-service** | Two services calling each other | Both | External vendors |

The more you keep real, the more confidence you gain, and the slower and harder to set up the tests become. Choose the scope by **risk**: where have bugs actually appeared?

## Testing several modules together

No special tooling: import the real pieces and wire them as in production, replacing only what leaves the process.

```js
import { describe, it, expect, beforeEach } from 'vitest';
import { createCart } from './cart.js';
import { createPricing } from './pricing.js';
import { createCheckout } from './checkout.js';

describe('checkout (cart + pricing + tax)', () => {
  let checkout;

  beforeEach(() => {
    const pricing = createPricing({ taxRate: 0.1 });
    const cart = createCart({ pricing });
    checkout = createCheckout({ cart, payments: fakePayments() });     // only payments is faked
  });

  it('charges the total including tax', async () => {
    checkout.cart.add({ sku: 'A', price: 100, qty: 2 });

    const receipt = await checkout.pay({ card: 'tok_test' });

    expect(receipt.total).toBe(220);
    expect(receipt.status).toBe('paid');
  });
});

function fakePayments() {
  const charges = [];
  return {
    charges,
    charge: async (amount) => { charges.push(amount); return { ok: true }; },
  };
}
```

## Testing an HTTP API

Send real requests through your app **without starting a network server on a fixed port**.

### With Supertest (Express, Fastify, `http.Server`)

```bash
npm install --save-dev supertest
```

```js
// app.js: export the app, do not call listen() here
import express from 'express';

export function createApp({ userRepo }) {
  const app = express();
  app.use(express.json());

  app.post('/users', async (req, res) => {
    const { name, email } = req.body ?? {};
    if (!name || !email) return res.status(400).json({ error: 'name and email are required' });

    const user = await userRepo.create({ name, email });
    res.status(201).json(user);
  });

  app.get('/users/:id', async (req, res) => {
    const user = await userRepo.find(req.params.id);
    if (!user) return res.status(404).json({ error: 'not found' });
    res.json(user);
  });

  return app;
}
```

```js
// server.js: the only file that listens
import { createApp } from './app.js';
createApp({ userRepo }).listen(3000);
```

```js
// app.test.js
import request from 'supertest';
import { describe, it, expect, beforeEach } from 'vitest';
import { createApp } from './app.js';
import { createInMemoryUserRepo } from './test/in-memory-user-repo.js';

describe('Users API', () => {
  let app;

  beforeEach(() => {
    app = createApp({ userRepo: createInMemoryUserRepo() });     // fresh state per test
  });

  it('creates a user', async () => {
    const res = await request(app)
      .post('/users')
      .send({ name: 'Ada', email: 'ada@example.com' })
      .expect(201)
      .expect('Content-Type', /json/);

    expect(res.body).toMatchObject({ name: 'Ada', email: 'ada@example.com' });
    expect(res.body.id).toBeDefined();
  });

  it('rejects a request with missing fields', async () => {
    const res = await request(app).post('/users').send({ name: 'Ada' }).expect(400);
    expect(res.body.error).toMatch(/required/);
  });

  it('returns 404 for an unknown user', async () => {
    await request(app).get('/users/999').expect(404);
  });

  it('can read back a created user', async () => {
    const { body } = await request(app).post('/users').send({ name: 'Ada', email: 'a@x.com' });
    const res = await request(app).get(`/users/${body.id}`).expect(200);
    expect(res.body.name).toBe('Ada');
  });
});
```

Key design choice: **separate creating the app from listening**. Tests import `createApp` and let Supertest bind an ephemeral port.

### With Node's built-in tools (no dependencies)

```js
import http from 'node:http';
import { once } from 'node:events';
import test from 'node:test';
import assert from 'node:assert/strict';

test('GET /health returns ok', async (t) => {
  const server = http.createServer(handler).listen(0);        // port 0: the OS picks a free port
  t.after(() => server.close());
  await once(server, 'listening');

  const { port } = server.address();
  const res = await fetch(`http://127.0.0.1:${port}/health`);

  assert.equal(res.status, 200);
  assert.deepEqual(await res.json(), { ok: true });
});
```

Always use **port `0`** in tests to avoid collisions when files run in parallel. Fastify offers `app.inject()` for in-process requests with no sockets at all.

### What to assert in API tests

| Aspect | Example |
|--------|---------|
| Status codes | `201` on create, `400` on bad input, `401`/`403` without access, `404` unknown, `409` conflict |
| Response body shape | Fields, types, no leaked secrets (`password` absent) |
| Headers | `Content-Type`, `Location`, cache headers, CORS, security headers |
| Validation | Missing, extra, wrongly typed fields |
| Authentication and authorization | No token, bad token, wrong role, right role |
| Side effects | A row was inserted, an event was published, an email was queued |
| Error format | Consistent JSON errors; no stack traces in production mode |
| Idempotency and ordering | Repeating a request, pagination boundaries |

## Testing against a real database

Repository and query code is full of things only a real database can verify: SQL syntax, constraints, indexes, transactions, ordering, `NULL` handling, and migrations.

### Options

| Approach | Pros | Cons |
|----------|------|------|
| **In-memory fake** (a `Map`) | Instant, no setup | Does not check real SQL or constraints |
| **SQLite in memory** as a stand-in | Fast, easy | SQL dialect differences from production (PostgreSQL, MySQL) |
| **Real engine in a container** (Testcontainers, Docker Compose) | Same engine as production | Slower startup, needs Docker |
| **Shared dev/test database** | No setup | Flaky, tests interfere with each other |

Prefer the **same engine as production**. Testcontainers starts a throwaway container from test code:

```bash
npm install --save-dev testcontainers @testcontainers/postgresql pg
```

```js
// test/setup-db.js
import { PostgreSqlContainer } from '@testcontainers/postgresql';
import pg from 'pg';

export async function startTestDb() {
  const container = await new PostgreSqlContainer('postgres:16').start();
  const pool = new pg.Pool({ connectionString: container.getConnectionUri() });
  await runMigrations(pool);

  return {
    pool,
    async reset() {
      await pool.query('TRUNCATE users, orders RESTART IDENTITY CASCADE');
    },
    async stop() {
      await pool.end();
      await container.stop();
    },
  };
}
```

```js
// user-repo.test.js
import { describe, it, expect, beforeAll, afterAll, beforeEach } from 'vitest';
import { startTestDb } from './test/setup-db.js';
import { createUserRepo } from './user-repo.js';

describe('UserRepo (PostgreSQL)', () => {
  let db, repo;

  beforeAll(async () => {
    db = await startTestDb();                     // slow: once per file
    repo = createUserRepo(db.pool);
  }, 60_000);                                     // allow time for the container to start

  afterAll(async () => { await db.stop(); });

  beforeEach(async () => { await db.reset(); });  // clean data per test

  it('inserts and finds a user', async () => {
    const created = await repo.create({ name: 'Ada', email: 'ada@example.com' });
    const found = await repo.findByEmail('ada@example.com');
    expect(found).toMatchObject({ id: created.id, name: 'Ada' });
  });

  it('enforces unique emails', async () => {
    await repo.create({ name: 'Ada', email: 'ada@example.com' });
    await expect(repo.create({ name: 'Other', email: 'ada@example.com' }))
      .rejects.toThrow(/unique|duplicate/i);
  });

  it('returns users ordered by creation time', async () => {
    await repo.create({ name: 'First', email: 'a@x.com' });
    await repo.create({ name: 'Second', email: 'b@x.com' });
    const users = await repo.list();
    expect(users.map((u) => u.name)).toEqual(['First', 'Second']);
  });
});
```

### Keeping database tests isolated and fast

| Technique | Notes |
|-----------|-------|
| **Start the database once per test file or run** | Container startup dominates the cost; reuse it |
| **Reset between tests** | `TRUNCATE ... CASCADE`, or delete in dependency order |
| **Wrap each test in a transaction and roll back** | Very fast; works when the code under test uses the same connection |
| **One schema or database per worker** | Safe parallelism: name it with the worker ID |
| **Run migrations once** | Test the migrations themselves in one dedicated test |
| **Seed minimal data per test** | Builders and factories; avoid giant shared fixtures |
| **Use random unique values** | Avoid collisions between tests (emails, usernames) |

```js
// Transaction rollback pattern
let client;
beforeEach(async () => { client = await pool.connect(); await client.query('BEGIN'); });
afterEach(async () => { await client.query('ROLLBACK'); client.release(); });
```

## Third-party services

Do not call real external APIs (payments, email, maps) in automated tests: they are slow, flaky, rate-limited, and can cost money.

| Technique | Use |
|-----------|-----|
| **Dependency injection with a fake client** | Replace the adapter you own |
| **Network mocking (MSW, nock, undici MockAgent)** | Intercept HTTP calls and return canned responses |
| **Local fake server** | A tiny `http` server on port 0 that mimics the service |
| **Vendor sandbox/test mode** | Occasionally, in a separate slower suite |
| **Contract tests (Pact, schema checks)** | Verify your assumptions match the provider's actual contract |

```js
// MSW in a Node test
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';

const server = setupServer(
  http.get('https://api.weather.test/forecast', () => HttpResponse.json({ tempC: 21 })),
);

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));   // fail loudly on unexpected calls
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

it('shows the temperature', async () => {
  expect(await getTemperatureLabel('Pune')).toBe('21°C');
});
```

More in [Mocking](./05_mocking.md).

## Test environments and configuration

| Concern | Practice |
|---------|----------|
| **Configuration** | Load a dedicated test configuration (`NODE_ENV=test`, `.env.test`); never point tests at production |
| **Secrets** | Use dummy values; never real credentials |
| **Ports** | Port `0` for servers |
| **Time zone and locale** | Fix them (`TZ=UTC`) for repeatable output |
| **Parallelism** | Isolate databases, files, and ports per worker, or run integration files serially |
| **Cleanup** | Close servers, pools, and timers in `afterAll`; otherwise the runner hangs |
| **Separate commands** | `npm run test:unit` (fast) and `npm run test:integration` (slower) |

```json
{
  "scripts": {
    "test": "vitest run",
    "test:unit": "vitest run --project unit",
    "test:integration": "vitest run --project integration",
    "test:watch": "vitest"
  }
}
```

Name integration files differently (`*.int.test.js`) or place them in a separate directory so you can run them on their own.

## Testing asynchronous workflows

For queues, events, and background jobs, **wait for a condition**, never for a fixed time:

```js
// Brittle: guess how long the job takes
await new Promise((r) => setTimeout(r, 2000));

// Reliable: poll until the condition holds (with a timeout)
async function waitFor(check, { timeout = 3000, interval = 25 } = {}) {
  const deadline = Date.now() + timeout;
  while (true) {
    const result = await check();
    if (result) return result;
    if (Date.now() > deadline) throw new Error('waitFor timed out');
    await new Promise((r) => setTimeout(r, interval));
  }
}

await queue.add({ type: 'send-email', to: 'ada@example.com' });
const sent = await waitFor(() => mailbox.find('ada@example.com'));
expect(sent.subject).toBe('Welcome');
```

Better still, expose a hook the test can await (a returned promise, an event, or a `drain()` method on the queue).

## End-to-end tests (briefly)

E2E tests drive the whole system from the outside, often through a browser:

```js
// Playwright
import { test, expect } from '@playwright/test';

test('a user can sign up and see the dashboard', async ({ page }) => {
  await page.goto('/signup');
  await page.getByLabel('Email').fill('ada@example.com');
  await page.getByLabel('Password').fill('correct horse battery staple');
  await page.getByRole('button', { name: 'Create account' }).click();

  await expect(page).toHaveURL(/dashboard/);
  await expect(page.getByRole('heading', { name: 'Welcome' })).toBeVisible();
});
```

Keep E2E tests to **critical journeys** (sign up, log in, checkout). They are valuable but slow and prone to flakiness; use role-based, user-visible selectors, wait for conditions (Playwright auto-waits), and keep test data self-contained.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Tests that share a database without cleanup | Order-dependent failures | Reset per test (truncate, rollback, fresh schema) |
| Calling `app.listen(3000)` in tests | Port conflicts, hanging runs | Export the app; Supertest or port `0` |
| Forgetting to close servers, pools, or containers | The runner never exits | `afterAll` cleanup; `t.after()` |
| Fixed `sleep()` waits | Slow and flaky | Poll for a condition, or await a signal |
| Mocking the database in "integration" tests | You are not testing the SQL or schema | Use the real engine (or a faithful stand-in) |
| SQLite standing in for PostgreSQL | Dialect and behavior differences hide bugs | Same engine as production |
| Real third-party calls in CI | Flaky, slow, costly | Fakes, network mocks, contract tests |
| Huge shared seed data | Fragile; unclear what tests rely on | Small per-test builders |
| Hard-coded IDs or unique values | Collisions when running in parallel | Generate unique values |
| Running everything in one slow command | People stop running tests locally | Separate fast and slow suites |
| Testing implementation-level calls through the stack | Brittle | Assert observable outcomes: responses, stored data |
| Skipping error paths | Production incidents live there | Test 4xx and 5xx behavior, timeouts, constraint violations |
| E2E tests for everything | Slow, flaky suite | Cover business-critical paths; push the rest down the pyramid |

## Key takeaways

- Integration tests verify the **connections** between components: queries, serialization, middleware, configuration
- Separate **creating** the app from **listening**; test through Supertest, `app.inject`, or an ephemeral port (`0`)
- Use the **same database engine** as production (Testcontainers is a good fit); isolate data per test by truncating or rolling back
- Fake or intercept **third-party services**; verify assumptions with contract tests
- Wait for **conditions**, not fixed delays
- Keep fast unit tests and slower integration tests in separate commands
- Reserve E2E tests for a few critical user journeys
- Clean up every resource a test opens

**Next:** [Vitest and Jest](./04_vitest-and-jest.md)
