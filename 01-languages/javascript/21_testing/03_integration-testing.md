# Integration Testing

An integration test runs **several real parts together** and checks that they cooperate: an HTTP handler with its routing and validation, a service with a real database, a module with the real file system. Unit tests prove the pieces are right. Integration tests prove they are connected correctly.

Many production bugs live in the seams: a wrong column name, a missing header, a misconfigured middleware, a serialization mismatch. Unit tests with fakes can't see those.

**Prerequisites:** [Unit Testing](./02_unit-testing.md), [HTTP Server](../16_nodejs/08_http_server.md), [Fetch](../15_networking/02_fetch.md)

---

## What Changes From Unit Tests

| | Unit | Integration |
| --- | --- | --- |
| Dependencies | Faked | Real (or a faithful substitute) |
| Setup | Usually none | Start server, prepare data |
| Cleanup | Rarely | Required: close servers, reset data |
| Speed | Milliseconds | Hundreds of ms and up |
| Failure points to | A function | A connection between parts |

The key decision is the **boundary**: what's real and what's replaced. A good default is to keep everything you own real and replace only what you don't control or can't run locally (third-party APIs, payment providers, email).

---

## Example: Testing an HTTP API

Start the real server on a random port, call it with `fetch`, check the response.

```js
// users.int.test.js
import { createServer } from "node:http";
import { beforeAll, afterAll, beforeEach, test, expect } from "vitest";
import { createApp } from "./app.js";               // returns a (req, res) request listener
import { InMemoryUserRepo } from "./in-memory-user-repo.js";

let server;
let baseUrl;
let users;

beforeAll(async () => {
  users = new InMemoryUserRepo();
  server = createServer(createApp({ users }));
  await new Promise((resolve) => server.listen(0, resolve)); // port 0 = pick a free port
  baseUrl = `http://127.0.0.1:${server.address().port}`;
});

afterAll(async () => {
  await new Promise((resolve) => server.close(resolve));
});

beforeEach(async () => {
  await users.clear(); // each test starts from a known state
});

test("POST /users creates a user", async () => {
  const res = await fetch(`${baseUrl}/users`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ email: "a@example.com" }),
  });

  expect(res.status).toBe(201);
  expect(await res.json()).toMatchObject({ email: "a@example.com" });
});

test("POST /users rejects an invalid email", async () => {
  const res = await fetch(`${baseUrl}/users`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ email: "not-an-email" }),
  });

  expect(res.status).toBe(400);
});
```

Things to notice:

- **Port `0`** lets the OS choose a free port, so parallel test files don't collide.
- The test goes through routing, JSON parsing, validation, the handler and serialization, the pieces a unit test would skip.
- `afterAll` closes the server. If a test run hangs after finishing, an open server or connection is the first thing to check. `server.closeAllConnections()` (Node 18.2+) forces lingering keep-alive connections shut.
- Here the repository is an in-memory fake, which is enough to test the HTTP layer. To test the real persistence code, swap in the real repository against a real database.

Libraries like `supertest` wrap this pattern if you prefer less boilerplate. The plain `fetch` version has no extra dependency.

---

## Testing With a Real Database

Use the same database engine you run in production, because differences in SQL dialect, constraints and transactions are exactly what you want to catch. Common options are a local instance, a throwaway container, or an in-memory/embedded engine if that's what production uses.

The hard part is **isolation**: each test must not see data left by another.

| Strategy | How | Trade-off |
| --- | --- | --- |
| Truncate/delete tables before each test | `beforeEach` clears tables | Simple, slower with many tables |
| Roll back a transaction per test | Open a transaction in `beforeEach`, roll back in `afterEach` | Fast, but code that commits its own transactions can't be tested this way |
| Unique data per test | Random emails/IDs, never assert on "all rows" | No cleanup needed, but data accumulates |
| Fresh database per test file | Create/drop a schema per file | Strongest isolation, heaviest setup |

Whichever you pick, run schema migrations in a `beforeAll`/global setup, and make sure tests can run in parallel without touching the same rows.

---

## Handling External Services

Don't call real third-party APIs from tests. They are slow, rate-limited and flaky, and some cost money.

- Inject a fake of your own adapter ([Adapter Pattern](../20_design-patterns/07_adapter-pattern.md)). This is the simplest option.
- Or intercept at the network level with a tool such as MSW (Mock Service Worker), so your real HTTP client code still runs.
- Keep a **small number of separate contract or smoke tests** against the real service, run less often, to catch the case where the fake and the real API drift apart.

See [Mocking](./05_mocking.md) for the mechanics.

---

## Keeping Them Manageable

- Name them so they can be selected: `*.int.test.js`, or keep them in an `integration/` folder.
- Run them separately: `vitest run integration` filters by file path. Many teams run unit tests on every save and integration tests before commit or in CI.
- Share expensive setup (starting a container, running migrations) once per run, not once per test.
- Keep them few and meaningful. One test per important flow and one per tricky failure path is usually better than re-testing every unit-level branch.

---

## Common Mistakes

- **Leftover data between tests** causing order-dependent failures.
- **Hard-coded ports** that collide when files run in parallel.
- **Forgetting to close** servers, pools and connections, so the runner hangs or reports open handles.
- **Mocking everything.** If the database, router and validator are all faked, it isn't an integration test.
- **Asserting on whole responses** including timestamps and generated IDs. Use `toMatchObject` or check the fields that matter.
- **Depending on test order**, such as a second test using the user created by the first.
- **Calling real third-party services** in the default test run.

## Debugging

- If it passes alone and fails in the suite, suspect shared state. Run the file alone, then with its neighbors.
- Log the response body when a status code surprises you: `console.log(res.status, await res.text())`.
- If the run never exits, look for unclosed servers, timers or DB pools. See [Event Loop](../12_event-loop/01_event-loop.md) for why a pending handle keeps Node alive.
- Check that migrations or seed data actually ran before the first test.

---

## Quick Summary

- Integration tests run real components together to verify the seams unit tests can't see.
- Decide the boundary: keep what you own real, replace third-party services.
- Start servers on port `0`, and always close servers and connections afterward.
- Isolate data per test (truncate, rollback, or unique data).
- Keep them separate, selectable, and fewer than unit tests.

**Next:** [Vitest and Jest](./04_vitest-and-jest.md)