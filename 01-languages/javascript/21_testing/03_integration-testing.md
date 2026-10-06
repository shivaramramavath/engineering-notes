# Integration Testing

Unit tests prove each piece works alone. Integration tests prove the pieces work **together**: routes call services, services talk to a database, JSON serializes the way the client expects. Many real bugs live in the seams between modules, not inside them.

## Prerequisites

- [Unit testing](./02_unit-testing.md)
- [HTTP server basics](../16_nodejs/08_http-server.md) and [HTTP fundamentals](../15_networking/01_http-fundamentals.md)

---

## What Counts as Integration?

Anything that exercises **real collaboration** between parts of your system, with only the *outermost* boundaries (third-party APIs, email, payment providers) faked.

```text
Unit:         [ service ]                     ← collaborators faked

Integration:  [ route ] → [ service ] → [ repository ] → [ test DB ]
                  ▲                                          
              test enters here, asserts on HTTP response + stored data
```

| | Unit | Integration |
|---|---|---|
| Real collaborators | No | Yes |
| Speed | ms | tens–hundreds of ms |
| Failure tells you | Which function | Which interaction is broken |
| Setup cost | Low | Higher (DB, server, fixtures) |

---

## Example: Testing an HTTP API

The key design move: **export the app without starting a server**, so tests can drive it in-process.

```js
// app.js
import express from 'express';

export function createApp({ userRepo }) {
  const app = express();
  app.use(express.json());

  app.post('/users', async (req, res) => {
    const { name, email } = req.body;
    if (!name || !email) return res.status(400).json({ error: 'name and email required' });
    const user = await userRepo.create({ name, email });
    res.status(201).json(user);
  });

  app.get('/users/:id', async (req, res) => {
    const user = await userRepo.findById(req.params.id);
    if (!user) return res.status(404).json({ error: 'not found' });
    res.json(user);
  });

  return app;
}
```

```js
// server.js: only this file calls listen()
import { createApp } from './app.js';
import { userRepo } from './db.js';
createApp({ userRepo }).listen(3000);
```

Using [supertest](https://github.com/ladjs/supertest) to call the app:

```js
// app.test.js
import { describe, it, expect, beforeEach } from 'vitest';
import request from 'supertest';
import { createApp } from './app.js';

// A small in-memory repo with the same interface as the real one
function createMemoryRepo() {
  const users = new Map();
  let id = 1;
  return {
    async create(data) { const u = { id: String(id++), ...data }; users.set(u.id, u); return u; },
    async findById(i) { return users.get(i) ?? null; },
  };
}

describe('users API', () => {
  let app;
  beforeEach(() => { app = createApp({ userRepo: createMemoryRepo() }); });

  it('creates and then fetches a user', async () => {
    const created = await request(app)
      .post('/users')
      .send({ name: 'Asha', email: 'asha@example.com' })
      .expect(201);

    const fetched = await request(app).get(`/users/${created.body.id}`).expect(200);
    expect(fetched.body).toMatchObject({ name: 'Asha', email: 'asha@example.com' });
  });

  it('rejects an invalid payload', async () => {
    const res = await request(app).post('/users').send({ name: 'No Email' });
    expect(res.status).toBe(400);
  });

  it('returns 404 for an unknown user', async () => {
    await request(app).get('/users/999').expect(404);
  });
});
```

This test covers routing, JSON parsing, validation, status codes, and serialization in one go, which is something no single unit test can claim.

---

## Using a Real Database

An in-memory fake is fast but can hide bugs (constraints, transactions, query syntax). When the database *is* the risk, test against the real engine:

- A **disposable instance**: a Docker container (e.g. via Testcontainers) or a local test database.
- Run **migrations** before the suite so the schema matches production.
- Keep the connection details in env vars (`DATABASE_URL`), never hard-coded to a shared dev DB.

```js
import { beforeAll, afterAll, beforeEach } from 'vitest';

beforeAll(async () => { await db.connect(process.env.TEST_DATABASE_URL); await db.migrate(); });
afterAll(async () => { await db.close(); });
beforeEach(async () => { await db.truncateAll(); });  // clean slate per test
```

(`db` is your own wrapper; the shape is the point, not the API.)

### Keeping tests isolated

| Strategy | Trade-off |
|---|---|
| Truncate tables in `beforeEach` | Simple, reliable, slower with many tables |
| Wrap each test in a transaction and roll back | Fast, but doesn't work if code under test manages its own transactions |
| Fresh database per test file | Strongest isolation, more setup |

Whichever you choose: tests must not depend on data left by other tests, and they should be able to run in any order.

---

## Faking the Outside World

Real third-party calls make tests slow, flaky, and sometimes costly. Intercept at the network layer with [MSW](https://mswjs.io/) so your real `fetch` code is still exercised:

```js
import { setupServer } from 'msw/node';
import { http, HttpResponse } from 'msw';
import { beforeAll, afterEach, afterAll, it, expect } from 'vitest';

const server = setupServer(
  http.get('https://api.example.com/rates/USD', () =>
    HttpResponse.json({ INR: 83.2 })
  ),
);

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());

it('converts using the remote rate', async () => {
  expect(await convertUsdToInr(10)).toBeCloseTo(832);
});

it('handles an upstream failure', async () => {
  server.use(http.get('https://api.example.com/rates/USD', () => new HttpResponse(null, { status: 503 })));
  await expect(convertUsdToInr(10)).rejects.toThrow();
});
```

`onUnhandledRequest: 'error'` makes any accidental real request fail loudly. Always test the **failure paths** (timeouts, 5xx, malformed bodies); that's where integration tests earn their keep. See [Mocking](./05_mocking.md) for the trade-offs of each fake.

---

## Test Data

Avoid giant shared fixtures. Prefer small **factories** that build valid objects with overrides:

```js
const makeUser = (overrides = {}) => ({
  name: 'Test User',
  email: `user-${crypto.randomUUID()}@example.com`,
  role: 'member',
  ...overrides,
});

await repo.create(makeUser({ role: 'admin' }));
```

Unique values (like emails) avoid collisions with unique constraints. More in [Testing patterns](./06_testing-patterns.md).

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Calling `app.listen()` inside the module under test → port conflicts | Export the app; listen in a separate entry file |
| Tests share DB state | Reset per test; never rely on order |
| Hitting real third-party APIs | Intercept with MSW or inject a fake client |
| Asserting only the status code | Also assert body and persisted state |
| Not closing connections → runner hangs | Close DB/server in `afterAll` |
| Hard-coded ports or `sleep()` waits | Let the OS pick ports; await real conditions |
| Making integration tests exhaustive | Cover key flows and edge cases; leave combinatorics to unit tests |

---

## Debugging

- **Hanging test run** usually means an open handle (DB pool, server, timer). Ensure teardown runs; check with `--detectOpenHandles` (Jest) or by closing resources explicitly.
- **Passes alone, fails in suite** means shared state or order dependence. Run with the single file, then bisect.
- **Works locally, fails in CI** is usually a missing env var, a different DB version, timing assumptions, or a port in use.
- Log the response body on failure: `console.log(res.body)`.

---

## Quick Summary

- Integration tests verify **real collaboration** between modules with only external boundaries faked.
- Export your app/factory without `listen()`, then drive it in-process with supertest.
- Use a real (disposable) DB when queries and constraints matter; reset state per test.
- Fake third-party HTTP at the network layer (MSW) and test failure cases.
- Keep them focused: a handful of key flows, not every permutation.

**Next:** [Vitest and Jest](./04_vitest-and-jest.md)
