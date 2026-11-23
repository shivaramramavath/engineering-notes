# Integration Testing in TypeScript: Databases, APIs, and Containers

> Designing robust integration tests in TypeScript: testing across subsystem boundaries, supertest HTTP assertions, Testcontainers for real databases, and transaction rollback isolation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [16-type-safe-apis/00-api-contracts.md](../16-type-safe-apis/00-api-contracts.md), [18-testing-and-debugging/00-unit-testing.md](./00-unit-testing.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

While unit tests verify algorithms in isolation, **Integration Testing** verifies that multiple distinct modules, services, databases, and network adapters work together correctly.

A unit test verifying a SQL repository against a fake in-memory Map cannot catch:
- SQL syntax errors (`SELECT * FORM users`).
- Database migration mismatches and column name drift.
- Foreign key constraint violations and unique index collisions.
- HTTP routing, JSON deserialization, and CORS header misconfigurations.

Integration tests execute against real or containerized infrastructure (such as real PostgreSQL instances spun up via **Testcontainers**), verifying end-to-end functionality from HTTP request to database commit.

```
+-----------------------------------------------------------+
|                    Integration Test Suite                 |
|                                                           |
|   HTTP Client (Supertest)                                 |
|          |                                                |
|          v                                                |
|   Express / Fastify App (Controllers + Middleware)        |
|          |                                                |
|          v                                                |
|   Database Adapter (Prisma / Drizzle)                     |
|          |                                                |
|          v                                                |
|   Real PostgreSQL (Docker Container via Testcontainers)   |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. HTTP API Testing with Supertest
`supertest` binds to an Express or Fastify HTTP server instance without listening on an actual public port, allowing fast local HTTP assertion pipelines:
```typescript
import request from 'supertest';
import { app } from '../src/app';

const res = await request(app)
  .post('/api/users')
  .send({ name: 'Alice', email: 'alice@example.com' })
  .expect(201);
```

### 2. Ephemeral Real Databases via Testcontainers
Rather than connecting to a shared, dirty staging database, **Testcontainers** uses Docker to launch a clean, disposable database container per test run:
```typescript
import { PostgreSqlContainer } from '@testcontainers/postgresql';

const container = await new PostgreSqlContainer('postgres:16-alpine').start();
const connectionUri = container.getConnectionUri();
```

### 3. Test Isolation via Database Transactions
Running migrations and seeding data before every single test is slow. The fastest isolation strategy is wrapping each test inside a database transaction and issuing a `ROLLBACK` at the end of the test.

---

## 3. How It Works Internally

```
1. beforeAll:
   - Start PostgreSQL Docker container
   - Run database migrations (schema setup)

2. beforeEach:
   - Begin SQL Transaction: BEGIN TRANSACTION;

3. test executes:
   - Service inserts row into database
   - Asserts query returns inserted row

4. afterEach:
   - Rollback SQL Transaction: ROLLBACK;
   - (Database returns to pristine state instantaneously!)

5. afterAll:
   - Stop Docker container
```

---

## 4. Syntax & API Reference

### Database Transaction Isolation Fixture
```typescript
export interface TestDatabaseContext {
  connectionUri: string;
  beginTransaction(): Promise<void>;
  rollbackTransaction(): Promise<void>;
  close(): Promise<void>;
}
```

---

## 5. Practical Examples

### Example 1: Basic Supertest HTTP Integration Test
```typescript
import express from 'express';
import request from 'supertest';
import { describe, it, expect } from 'vitest';

const app = express();
app.use(express.json());
app.post('/api/echo', (req, res) => {
  res.status(200).json({ received: req.body });
});

describe('POST /api/echo', () => {
  it('returns sent payload with 200 OK', async () => {
    const payload = { message: 'hello world' };
    const res = await request(app)
      .post('/api/echo')
      .send(payload)
      .expect(200);

    expect(res.body.received).toEqual(payload);
  });
});
```

### Example 2: Real-World Testcontainers PostgreSQL Integration Test
```typescript
import { describe, beforeAll, afterAll, it, expect } from 'vitest';
import { PostgreSqlContainer, StartedPostgreSqlContainer } from '@testcontainers/postgresql';
import { Client } from 'pg';

describe('Postgres Repository Integration', () => {
  let container: StartedPostgreSqlContainer;
  let client: Client;

  beforeAll(async () => {
    container = await new PostgreSqlContainer('postgres:16-alpine').start();
    client = new Client({ connectionString: container.getConnectionUri() });
    await client.connect();
    await client.query(`
      CREATE TABLE users (
        id SERIAL PRIMARY KEY,
        email VARCHAR(255) UNIQUE NOT NULL
      );
    `);
  }, 60000); // 60s timeout for docker pull

  afterAll(async () => {
    await client?.end();
    await container?.stop();
  });

  it('enforces unique email constraint', async () => {
    await client.query("INSERT INTO users (email) VALUES ('test@example.com')");

    await expect(
      client.query("INSERT INTO users (email) VALUES ('test@example.com')")
    ).rejects.toThrow();
  });
});
```

### Example 3: Edge Case - Concurrent Request Race Condition Testing
```typescript
it('handles concurrent debit requests without double-spending', async () => {
  // Simulate 10 simultaneous debit requests of $20 against an account with $50
  const promises = Array.from({ length: 10 }, () =>
    request(app).post('/api/account/debit').send({ amount: 20 })
  );

  const responses = await Promise.all(promises);
  const successCount = responses.filter((r) => r.status === 200).length;
  const failureCount = responses.filter((r) => r.status === 400 || r.status === 409).length;

  // Exactly 2 debits must succeed ($40 total); the rest must fail!
  expect(successCount).toBe(2);
  expect(failureCount).toBe(8);
});
```

### Example 4: Failure Case - Missing Environment Variables in Test Harness
```typescript
function getDatabaseUrl(): string {
  const url = process.env.DATABASE_URL;
  if (!url) throw new Error('DATABASE_URL environment variable is not defined');
  return url;
}

// @ts-expect-error TS2345: Property 'DATABASE_URL' does not exist if strict env types are configured!
process.env.NON_EXISTENT_VAR;
```

### Example 5: Idiomatic Transaction Rollback Helper
```typescript
export async function withTransactionRollback(
  client: Client,
  testFn: () => Promise<void>
): Promise<void> {
  await client.query('BEGIN');
  try {
    await testFn();
  } finally {
    await client.query('ROLLBACK');
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Docker Availability in CI**: Testcontainers requires a running Docker daemon. In CI environments like GitHub Actions, Docker is pre-installed on Linux runners, but running tests in nested Docker containers requires mounting the Docker socket (`/var/run/docker.sock`).
2. **Port Collisions**: Never hardcode database ports (`5432`) in tests. Allow Testcontainers to map to dynamic, randomized host ports.
3. **Database Migration Latency**: Running Prisma or Knex migrations inside `beforeAll` adds 2-5 seconds per suite. Reuse container instances across files using global setup scripts when possible.
4. **Unclosed Connections**: Failing to close database connections or HTTP servers causes test runner processes to hang indefinitely after test completion.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Shared Dirty Database** | Connecting tests to shared staging DB | Ephemeral Testcontainers or SQLite | Tests fail randomly due to leftover data |
| **Hardcoded Ports** | Binding test server to `localhost:3000` | Dynamic port `server.listen(0)` | Port collision in parallel test runners |
| **Testing with SQLite When Prod is Postgres** | SQLite in tests, PG in production | Testcontainers PostgreSQL in tests | Misses Postgres-specific syntax, triggers, JSONB |
| **Missing Connection Cleanup** | Omitting `afterAll(async () => db.close())` | Always close handles in `afterAll` | Test runner hangs forever after completion |
| **Skipping HTTP Validation Layers** | Invoking controller methods directly | Supertest `request(app)` | Bypasses auth middleware and JSON body parser |
| **Unbounded Test Timeouts** | Default 5s timeout on Docker container pull | Set 60s timeout on container startup | Test fails with `Timeout exceeded` on cold pulls |

---

## 8. Performance & Type-Checker Complexity

- **Execution Speed**: Testcontainers adds ~3-5 seconds of initial container startup overhead. Running multiple integration tests in parallel within the same container using transaction rollbacks yields ~10ms per test execution time.
- **Type Safety**: Supertest responses return `res.body: any` by default. Strongly type responses using assertion wrappers:
  `const body = res.body as ExpectedDTO;` or parse with Zod schemas.

---

## 9. Compiler Internals

TypeScript performs control flow analysis over `beforeAll` and `afterAll` hooks. However, variables initialized inside `beforeAll` must be declared as `let container: StartedContainer | undefined;` or with a non-null assertion `let container!: StartedContainer;` to satisfy `"strictNullChecks": true`.

---

## 10. Best Practices & Design Guidelines

1. **Test Real Technologies**: If production runs PostgreSQL, integration test against PostgreSQL, not an in-memory substitute.
2. **Use Transaction Rollbacks for Speed**: Wrap each test in `BEGIN ... ROLLBACK` to reset database state instantaneously.
3. **Test HTTP Through the Entire Middleware Pipeline**: Use Supertest to verify authentication, parsing, validation, and error serialization.
4. **Always Close Resources in `afterAll`**: Prevent hung processes in CI.

---

## 11. Real-World Case Study

### E-Commerce Checkout Integration Test Pipeline
Verifying complete order creation, inventory deduction, and transaction commit against live PostgreSQL:

```typescript
describe('POST /api/orders', () => {
  it('creates order and decrements stock in a single transaction', async () => {
    // 1. Arrange: Seed test product with 5 units
    const product = await db.product.create({ data: { sku: 'SHIRT-1', stock: 5 } });

    // 2. Act: Place order for 2 units via HTTP
    const res = await request(app)
      .post('/api/orders')
      .send({ sku: 'SHIRT-1', quantity: 2 })
      .expect(201);

    expect(res.body.orderId).toBeDefined();

    // 3. Assert: Verify database reflects 3 remaining units
    const updated = await db.product.findUnique({ where: { id: product.id } });
    expect(updated?.stock).toBe(3);
  });
});
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between unit testing and integration testing?</summary>

Unit testing evaluates individual components (functions, classes) in isolation with all external dependencies mocked. Integration testing verifies that multiple integrated components, subsystems, or external systems (databases, caches, third-party APIs) interact correctly together.
</details>

<details>
<summary>2. Why is testing against in-memory SQLite problematic when production uses PostgreSQL?</summary>

SQLite and PostgreSQL differ significantly in SQL dialect, type systems (Postgres supports JSONB, Arrays, UUIDs, ENUMs), concurrency models (SQLite uses file-level locking), and constraint handling. Code that passes on SQLite can fail in production PostgreSQL.
</details>

<details>
<summary>3. How does the transaction rollback strategy speed up database integration tests?</summary>

Instead of dropping and re-migrating the database between tests (which takes hundreds of milliseconds), each test begins a transaction (`BEGIN`), performs its operations, and rolls back (`ROLLBACK`) in `afterEach()`. This resets the database state in sub-millisecond time.
</details>

<details>
<summary>4. How does `supertest` test an Express app without listening on a network port?</summary>

`supertest` passes the Express app to Node's `http.createServer(app)`. It can invoke HTTP request pipelines internally using Node's in-memory socket streams without requiring an open external network port.
</details>

<details>
<summary>5. What are Testcontainers and why are they used in CI/CD?</summary>

Testcontainers is a library that allows starting ephemeral Docker containers programmatically from test code. It ensures tests run against real, versioned databases and message brokers (Postgres, Redis, Kafka) without requiring pre-configured infrastructure on the CI machine.
</details>

<details>
<summary>6. How do you prevent test runner hanging after integration test suites complete?</summary>

Ensure all database connection pools, Redis clients, HTTP servers, and timers are explicitly closed inside `afterAll()` hooks (`await pool.end()`, `server.close()`).
</details>

<details>
<summary>7. How do you strongly type `supertest` response bodies in TypeScript?</summary>

Since `res.body` is typed as `any`, parse it using a Zod schema (`const data = UserSchema.parse(res.body)`) or pass it to a generic helper function `typedBody<T>(res): T`.
</details>

<details>
<summary>8. How do you test for race conditions in concurrent database operations?</summary>

By dispatching multiple concurrent asynchronous requests (`Promise.all([req1, req2, req3])`) and asserting that atomic database locks (like `SELECT ... FOR UPDATE` or serializable transactions) prevent inconsistent states like double-spending.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Supertest Health Check</summary>

**Problem:** Write an integration test using Supertest verifying that `GET /health` returns `{ status: 'ok' }` with HTTP 200.

**Solution:**
```typescript
import request from 'supertest';
import express from 'express';
import { it, expect } from 'vitest';

const app = express();
app.get('/health', (_, res) => res.status(200).json({ status: 'ok' }));

it('GET /health returns ok', async () => {
  const res = await request(app).get('/health').expect(200);
  expect(res.body).toEqual({ status: 'ok' });
});
```
</details>

<details>
<summary>Exercise 2 (Easy): Header Assertion</summary>

**Problem:** Assert that a response contains the `Content-Type: application/json` header and a custom `X-Response-Time` header.

**Solution:**
```typescript
it('asserts headers', async () => {
  await request(app)
    .get('/health')
    .expect('Content-Type', /json/)
    .expect((res) => {
      if (!res.headers['x-response-time']) throw new Error('Missing timing header');
    });
});
```
</details>

<details>
<summary>Exercise 3 (Medium): Database Seed and Clean Fixture</summary>

**Problem:** Implement a database test fixture that seeds 3 test users in `beforeEach` and truncates the table in `afterEach`.

**Solution:**
```typescript
export class TestUserFixture {
  constructor(private client: { query: (sql: string) => Promise<any> }) {}

  async seed() {
    await this.client.query("INSERT INTO users (name) VALUES ('Alice'), ('Bob'), ('Charlie')");
  }

  async clean() {
    await this.client.query('TRUNCATE TABLE users CASCADE');
  }
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Type-Safe Supertest Response Parser</summary>

**Problem:** Write a wrapper function `requestJson<T>(app: any, method: string, path: string): Promise<{ status: number; body: T }>` that types response bodies.

**Solution:**
```typescript
export async function requestJson<T>(
  app: any,
  path: string
): Promise<{ status: number; body: T }> {
  const res = await request(app).get(path);
  return { status: res.status, body: res.body as T };
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Transaction Isolation Wrapper for Vitest</summary>

**Problem:** Create a Vitest test decorator that automatically wraps any test function in a database transaction that rolls back upon completion.

**Solution:**
```typescript
export function itInTx(
  name: string,
  client: { query: (s: string) => Promise<any> },
  fn: () => Promise<void>
) {
  it(name, async () => {
    await client.query('BEGIN');
    try {
      await fn();
    } finally {
      await client.query('ROLLBACK');
    }
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Race Condition Double-Spend Verifier</summary>

**Problem:** Write an integration test asserting that when two concurrent balance transfer requests are submitted simultaneously with a sum exceeding account balance, exactly one succeeds and one fails.

**Solution:**
```typescript
it('prevents double-spending under concurrent HTTP load', async () => {
  const [res1, res2] = await Promise.all([
    request(app).post('/transfer').send({ amount: 100 }),
    request(app).post('/transfer').send({ amount: 100 })
  ]);

  const statuses = [res1.status, res2.status].sort();
  expect(statuses).toEqual([200, 400]); // One ok, one rejected
});
```
</details>
