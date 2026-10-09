# Integration Testing

An **integration test** checks that several real pieces work together: an HTTP handler, the service behind it, and an actual database. Unit tests with fakes prove your logic is right. Integration tests prove the **wiring** is right: that your SQL runs, your mapping matches the schema, your middleware is in the right order, and your validation rejects what it should. Most production bugs live at these seams, which fakes cannot see.

**Prerequisites:**
- [Unit testing](./00-unit-testing.md)
- [Dependency injection](../17-design-patterns/05-dependency-injection.md) (composition root)
- [Repository](../17-design-patterns/04-repository.md)
- [Express](../20-nodejs-backend/02-express.md) and [databases](../20-nodejs-backend/05-databases.md) (for the backend examples)

---

## Where integration tests fit

```text
          /\          few, slow, broad: real browser, real environment (end-to-end)
         /  \
        /----\        some: real database + real HTTP layer (integration)
       /      \
      /--------\      many, fast, narrow: pure logic with fakes (unit)
```

The usual advice is a mix: many unit tests for logic, a solid layer of integration tests for the boundaries your code owns, and a few end-to-end tests for critical user journeys. Some teams weight integration tests more heavily for backends, because they catch more real defects per test than heavily mocked unit tests do.

## Make the app testable: build it from a function

Tests should be able to create the app **without listening on a port** and without hard-wired dependencies. Export a factory that takes its dependencies ([dependency injection](../17-design-patterns/05-dependency-injection.md)):

```ts
// src/app.ts
import express from "express";

export interface AppDeps {
  userService: UserService;
}

export function createApp({ userService }: AppDeps) {
  const app = express();
  app.use(express.json());

  app.post("/users", async (req, res) => {
    const parsed = CreateUserSchema.safeParse(req.body);
    if (!parsed.success) return res.status(400).json(validationError(parsed.error));
    const user = await userService.register(parsed.data);
    res.status(201).json(toUserDto(user));
  });

  return app;
}
```

```ts
// src/server.ts  (only this file starts a server)
createApp(deps).listen(config.port);
```

## Testing the HTTP layer

`supertest` sends requests directly to an app object, with no port needed:

```ts
import request from "supertest";
import { describe, it, expect } from "vitest";

describe("POST /users", () => {
  it("creates a user", async () => {
    const app = createApp({ userService: realServiceWithTestDb });

    const res = await request(app)
      .post("/users")
      .send({ email: "a@b.com", password: "correct horse" })
      .expect(201);

    expect(res.body).toMatchObject({ email: "a@b.com" });
    expect(res.body).not.toHaveProperty("passwordHash");      // DTO does not leak
  });

  it("rejects an invalid email with 400", async () => {
    const res = await request(createApp(deps))
      .post("/users")
      .send({ email: "nope", password: "x" })
      .expect(400);

    expect(res.body.error.code).toBe("VALIDATION");
  });
});
```

Assert on **status, body shape, and headers**: the parts of the contract clients rely on ([API contracts](../16-type-safe-apis/00-api-contracts.md)). Also test the failure cases (400, 401, 403, 404, 409), because that is where handlers go wrong.

Other runtimes have equivalents. Many frameworks (Fastify, NestJS, Hono) provide a way to inject a request into the app without a network. Use whichever your framework recommends.

## A real database

Fakes cannot catch: a typo in SQL, a missing index, a NOT NULL constraint, a unique-violation, a migration that does not match the model, or a type conversion (dates, decimals, JSON). Run tests against the **same kind of database** you run in production.

Options:

| Approach | Notes |
|---|---|
| **Throwaway container per run** (for example Testcontainers, which starts a real database in Docker from test code) | closest to production, needs Docker, a few seconds to start |
| **A shared dev/test database** started by `docker compose` or CI services | fast to reuse, requires cleanup discipline |
| **Embedded or in-memory substitute** (SQLite in place of Postgres) | quick, but behavior differs (types, constraints, SQL dialect), so it can pass where production fails |

Prefer the real engine. If you must substitute, accept that some behavior is untested.

### Setting up and resetting state

```ts
import { beforeAll, afterAll, beforeEach } from "vitest";

let pool: Pool;

beforeAll(async () => {
  pool = new Pool({ connectionString: process.env.TEST_DATABASE_URL });
  await runMigrations(pool);                    // the real migrations, not a hand-written schema
});

afterAll(async () => {
  await pool.end();
});

beforeEach(async () => {
  await pool.query("truncate table orders, users restart identity cascade");   // clean slate
});
```

Isolation strategies, from simplest to fastest:

- **Truncate tables** before each test. Simple, reliable, slower on large schemas.
- **Run each test in a transaction and roll it back.** Fast, but cannot test code that itself manages transactions or uses multiple connections.
- **A separate schema or database per test worker,** so tests can run in parallel without colliding.

Whatever you choose, **each test must start from a known state and not depend on others**. Seed data with builders ([builder](../17-design-patterns/01-builder.md)) rather than shared fixtures that every test silently relies on.

### Testing the repository itself

The repository is where SQL lives, so test it directly against the real database:

```ts
it("round-trips an order", async () => {
  const repo = new PostgresOrderRepository(pool);
  const order = buildOrder({ status: "pending", totalCents: 500 });

  await repo.save(order);

  expect(await repo.findById(order.id)).toEqual(order);
});

it("returns null for a missing order", async () => {
  expect(await new PostgresOrderRepository(pool).findById("missing")).toBeNull();
});

it("rejects a stale update (optimistic locking)", async () => {
  // save, load twice, save one, expect the other to fail with a conflict
});
```

Run the **same behavioral suite** against your in-memory fake and the real repository (a contract test), so the fake used by unit tests cannot drift from real behavior ([repository](../17-design-patterns/04-repository.md)).

## External services

Your tests should not call real third-party APIs: they are slow, flaky, rate-limited, and may cost money. Intercept HTTP at the network level so the full client code (URL building, serialization, parsing) still runs:

```ts
import { http, HttpResponse } from "msw";
import { setupServer } from "msw/node";

const server = setupServer(
  http.post("https://api.payments.example/charges", () =>
    HttpResponse.json({ id: "ch_1", state: "SETTLED" }),
  ),
);

beforeAll(() => server.listen({ onUnhandledRequest: "error" }));
afterEach(() => server.resetHandlers());
afterAll(() => server.close());
```

`onUnhandledRequest: "error"` makes any unexpected outbound call fail the test, which finds accidental real network use. Mock the failure modes too: a `500`, a timeout, a malformed body, a slow response ([typed fetch and API client](../16-type-safe-apis/05-typed-fetch-and-api-client.md)).

For services you own, consider **contract tests** between consumer and provider so a mocked response cannot silently diverge from the real one ([contract-first APIs](../16-type-safe-apis/06-contract-first-apis.md)).

## Authentication and other cross-cutting concerns

- Create test users and tokens through the same code paths you use in production, or via a helper that signs valid tokens with a test key.
- Test **authorization**: that user A cannot read user B's data, not just that logged-in users can read their own.
- Test middleware ordering with a real request: rate limits, CORS headers, error handler output ([middleware](../20-nodejs-backend/03-middleware.md)).
- Test the **error handler** end to end: an unexpected exception should produce a generic `500` with no stack trace ([error response types](../16-type-safe-apis/04-error-response-types.md)).

## Configuration for tests

- Use a **separate test configuration** (`NODE_ENV=test`, a test database URL), validated like production config ([config and environment](../20-nodejs-backend/01-config-and-environment.md)).
- **Never** point integration tests at a shared or production database. Add a guard that refuses to run unless the database name contains `test`.
- Keep slow integration tests in their own folder or naming pattern (`*.int.test.ts`), so developers can run fast unit tests constantly and the full set before merging.

```json
{
  "scripts": {
    "test": "vitest run src",
    "test:integration": "vitest run --config vitest.integration.config.ts"
  }
}
```

See [test runners](./03-test-runners.md).

## Speed and reliability

- **Start expensive things once** (`beforeAll`), reset cheaply (`beforeEach`).
- **Run in parallel** only with proper isolation. Shared database state is the usual cause of "passes alone, fails together".
- **Avoid fixed sleeps.** Wait for conditions (polling with a timeout) instead of `setTimeout(1000)`.
- **Set explicit timeouts** so a hung test fails clearly instead of stalling CI.
- **Retry flaky tests only as a diagnostic,** not as a fix. Find the root cause: timing, shared state, or an uncontrolled external dependency.
- In CI, use service containers or Docker Compose to supply the database ([CI and deployment](../21-production-tooling/05-ci-and-deployment.md)).

## End-to-end tests (briefly)

End-to-end tests drive the real system through its real interface, usually a browser (Playwright and Cypress are common tools). They are the slowest and most brittle layer, so reserve them for a handful of critical journeys (sign up, pay, core workflow) and keep logic checks in lower layers.

## Common mistakes

- Testing against an in-memory substitute for a different database engine and trusting the result.
- Tests sharing database state and depending on order.
- Calling real third-party services.
- Integration tests that start a real server on a fixed port, colliding in parallel runs.
- Hand-written test schemas instead of the real migrations.
- Only testing happy paths.
- No cleanup, so data accumulates and tests start failing mysteriously.
- Pointing tests at a non-test database.
- Sleeping for fixed durations instead of waiting for conditions.
- Making everything an integration test, leaving the suite too slow to run often.

## Debugging

- **Run one failing test** and log the SQL and HTTP traffic. Most integration failures are visible in the actual request or query.
- **Inspect the database state** after a failure (keep the container, or dump tables) to see what was really written.
- If tests pass alone and fail together, check isolation and parallelism settings.
- If a test hangs, look for an unclosed connection, a pool that was not ended, or an unresolved promise. Node's `--test-timeout` or the runner's timeout shows where it stalls.
- Compare behavior with a manual `curl -i` against a locally running instance to separate a test problem from an app problem.
- Use the debugger inside integration tests ([debugging](./05-debugging.md)).

## Quick summary

- Integration tests verify the seams: handlers, services, real SQL, middleware, external clients. Unit tests with fakes cannot.
- Build the app from a factory with injected dependencies, so tests can create it without a port.
- Test against the same database engine as production, run the real migrations, and reset state before each test.
- Intercept outbound HTTP with a network-level mock, fail on unhandled requests, and test failure modes.
- Keep tests isolated and deterministic, never touch non-test databases, and keep the slow suite separate from the fast one.

**Next:** [Mocking](./02-mocking.md)
