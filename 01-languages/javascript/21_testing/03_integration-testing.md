# Integration Testing in JavaScript

> Multi-module collaboration, HTTP API testing, database transaction isolation, ephemeral test servers, and mock network services.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Unit Testing](file:///e:/Learning/engineering-notes/01-languages/javascript/21_testing/02_unit-testing.md), [HTTP Server](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/08_http-server.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | Node.js v18 LTS - v24+, REST & SQL/NoSQL |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Unit tests verify that individual functions or classes compute values correctly in isolation. However, in production applications, the vast majority of critical bugs do not occur within isolated business algorithms; they occur at the **seams and boundaries where different systems collaborate**:
- An SQL query contains a syntax error or references a missing table column.
- An HTTP route handler fails to parse a JSON body format or forgets to set appropriate response headers.
- A database transaction fails to roll back when a downstream service rejects a request.

**Integration Testing** verifies that two or more real software components—such as an HTTP controller, a business service, a real database, and a cache layer—integrate and execute correctly together. They bridge the gap between fast, isolated unit tests and slow, fragile end-to-end browser tests.

---

## 2. Core Concepts

### Scope of an Integration Test
Unlike unit tests that replace collaborators with fakes or stubs, integration tests execute real subsystems together:
- **HTTP Layer $\leftrightarrow$ Business Logic**: Verifies routing, authentication middleware, request validation, status codes, and JSON serialization.
- **Data Access Layer $\leftrightarrow$ Database**: Executes real queries against a test database instance (Postgres, SQLite, MongoDB), validating constraints, foreign keys, and indexes.

### Database State Isolation Strategies
Tests must never interfere with one another through shared database state:
1. **Transaction Rollback Pattern**: Each test begins a database transaction (`BEGIN`), executes operations, performs assertions, and immediately rolls back (`ROLLBACK`). The database is left 100% clean, and tests execute in milliseconds.
2. **Database Truncation**: After each test suite finishes, all table rows are truncated (`TRUNCATE TABLE users, orders CASCADE`). Slower than transaction rollbacks, but resets auto-increment IDs and handles multi-connection tests.
3. **Ephemeral Test Databases (Testcontainers)**: A dedicated, isolated Docker container or disposable SQLite in-memory database is spun up specifically for the test run and destroyed upon completion.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| TEST RUNNER (node:test / Vitest)                                         |
+--------------------------------------------------------------------------+
                                    |
  1. Spawns Ephemeral HTTP Server   | server.listen(0) -> OS assigns port 54321
  2. Begins DB Transaction          | client.query('BEGIN')
                                    v
+--------------------------------------------------------------------------+
| CLIENT REQUEST (Native fetch / Supertest)                                |
| POST http://localhost:54321/api/orders -> { item: 'Widget', qty: 2 }    |
+------------------------------------+-------------------------------------+
                                     | Network Socket
                                     v
+--------------------------------------------------------------------------+
| REAL HTTP SERVER & ROUTING PIPELINE                                      |
| - Express / Fastify / Node http                                          |
| - Auth Middleware -> Body Parser -> OrderController -> OrderService      |
+------------------------------------+-------------------------------------+
                                     | SQL Query
                                     v
+--------------------------------------------------------------------------+
| REAL TEST DATABASE (PostgreSQL / SQLite)                                 |
| INSERT INTO orders ... (Executed inside active transaction)             |
+------------------------------------+-------------------------------------+
                                     |
  3. Response Verified               | 201 Created { id: 101, status: 'PAID' }
  4. Cleanup Hook                    | client.query('ROLLBACK')
                                    v
+--------------------------------------------------------------------------+
| DATABASE IS PRISTINE (Zero residual test data)                           |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### Binding an Ephemeral Port with `server.listen(0)`
When running integration tests against an HTTP server, binding to port `0` instructs the operating system to dynamically allocate a free ephemeral port, preventing `EADDRINUSE` port collision errors during parallel test execution:
```javascript
const server = http.createServer(app);

await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const { port } = server.address();
const baseUrl = `http://127.0.0.1:${port}`;
```

---

## 5. Practical Examples

### Example 1: Full-Stack HTTP API Integration Test with Native `fetch`
```javascript
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';

// The Application Server
function createTestApp() {
  const users = new Map();

  return http.createServer(async (req, res) => {
    if (req.method === 'POST' && req.url === '/users') {
      let body = '';
      for await (const chunk of req) body += chunk;
      const data = JSON.parse(body);

      if (!data.username) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Username required' }));
      }

      const id = users.size + 1;
      const newUser = { id, username: data.username };
      users.set(id, newUser);

      res.writeHead(201, { 'Content-Type': 'application/json' });
      return res.end(JSON.stringify(newUser));
    }

    res.writeHead(404);
    res.end();
  });
}

describe('User Registration HTTP API Integration', () => {
  let server;
  let baseUrl;

  before(async () => {
    server = createTestApp();
    // Bind to ephemeral port
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const port = server.address().port;
    baseUrl = `http://127.0.0.1:${port}`;
  });

  after(async () => {
    await new Promise(resolve => server.close(resolve));
  });

  it('creates user and returns 201 Created', async () => {
    const res = await fetch(`${baseUrl}/users`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: 'ada_lovelace' }),
    });

    assert.strictEqual(res.status, 201);
    const body = await res.json();
    assert.strictEqual(body.username, 'ada_lovelace');
    assert.strictEqual(typeof body.id, 'number');
  });

  it('rejects missing username with 400 Bad Request', async () => {
    const res = await fetch(`${baseUrl}/users`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({}),
    });

    assert.strictEqual(res.status, 400);
    const body = await res.json();
    assert.strictEqual(body.error, 'Username required');
  });
});
```

### Example 2: Database Integration with Transaction Rollbacks
```javascript
import { describe, it, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';

// Simulated SQL Database Client
class DatabaseClient {
  constructor() { this.rows = []; this.inTransaction = false; this.snapshot = []; }
  async query(sql, params) {
    if (sql === 'BEGIN') { this.inTransaction = true; this.snapshot = [...this.rows]; return; }
    if (sql === 'ROLLBACK') { this.rows = [...this.snapshot]; this.inTransaction = false; return; }
    if (sql.startsWith('INSERT')) {
      const record = { id: this.rows.length + 1, email: params[0] };
      this.rows.push(record);
      return record;
    }
    if (sql.startsWith('SELECT')) return this.rows.filter(r => r.email === params[0]);
  }
}

describe('Database Repository Integration', () => {
  const db = new DatabaseClient();

  beforeEach(async () => {
    // Isolate test inside a database transaction
    await db.query('BEGIN');
  });

  afterEach(async () => {
    // Rollback changes to leave database pristine
    await db.query('ROLLBACK');
  });

  it('persists a new user record within transaction', async () => {
    await db.query('INSERT INTO users (email) VALUES ($1)', ['alice@example.com']);
    const records = await db.query('SELECT * FROM users WHERE email = $1', ['alice@example.com']);

    assert.strictEqual(records.length, 1);
    assert.strictEqual(records[0].email, 'alice@example.com');
  });

  it('proves database was rolled back and contains zero records from previous test', async () => {
    const records = await db.query('SELECT * FROM users WHERE email = $1', ['alice@example.com']);
    assert.strictEqual(records.length, 0); // Completely clean!
  });
});
```

### Example 3: Mocking External Third-Party APIs with In-Memory HTTP Servers
```javascript
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';

// In an integration test, we do NOT want to hit real external payment APIs (like Stripe).
// We spin up a local mock server representing the external service.
describe('Payment Service with External Gateway', () => {
  let mockStripeServer;
  let mockStripeUrl;

  before(async () => {
    mockStripeServer = http.createServer((req, res) => {
      if (req.method === 'POST' && req.url === '/v1/charges') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ id: 'ch_12345', status: 'succeeded' }));
      }
      res.writeHead(404);
      res.end();
    });

    await new Promise(r => mockStripeServer.listen(0, '127.0.0.1', r));
    mockStripeUrl = `http://127.0.0.1:${mockStripeServer.address().port}`;
  });

  after(async () => {
    await new Promise(r => mockStripeServer.close(r));
  });

  it('successfully executes checkout against external gateway', async () => {
    // Service configured with mockStripeUrl
    const res = await fetch(`${mockStripeUrl}/v1/charges`, { method: 'POST' });
    const data = await res.json();

    assert.strictEqual(data.status, 'succeeded');
    assert.strictEqual(data.id, 'ch_12345');
  });
});
```

### Example 4: Testing Middleware Chain Execution
```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

function executeMiddlewareChain(req, res, middlewares, targetHandler) {
  let index = 0;

  function next(err) {
    if (err) throw err;
    if (index < middlewares.length) {
      const mw = middlewares[index++];
      mw(req, res, next);
    } else {
      targetHandler(req, res);
    }
  }

  next();
}

describe('Middleware Integration Pipeline', () => {
  it('correctly executes auth and audit middleware in sequence', () => {
    const executed = [];

    const authMiddleware = (req, res, next) => {
      req.user = { id: 101, role: 'admin' };
      executed.push('auth');
      next();
    };

    const auditMiddleware = (req, res, next) => {
      executed.push('audit');
      next();
    };

    const handler = (req, res) => {
      executed.push('handler');
    };

    const req = {};
    const res = {};
    executeMiddlewareChain(req, res, [authMiddleware, auditMiddleware], handler);

    assert.deepStrictEqual(executed, ['auth', 'audit', 'handler']);
    assert.strictEqual(req.user.id, 101);
  });
});
```

### Example 5: Testing File Upload Pipelines to Disk
```javascript
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';

describe('File Storage Integration Pipeline', () => {
  const tempDir = path.resolve('./temp-test-uploads');

  before(async () => {
    await fs.mkdir(tempDir, { recursive: true });
  });

  after(async () => {
    await fs.rm(tempDir, { recursive: true, force: true });
  });

  it('streams uploaded file to disk and calculates sha256 checksum', async () => {
    const filePath = path.join(tempDir, 'sample.txt');
    const content = 'Test file payload data';

    await fs.writeFile(filePath, content, 'utf8');

    const stat = await fs.stat(filePath);
    assert.strictEqual(stat.size, Buffer.byteLength(content));

    const readBack = await fs.readFile(filePath, 'utf8');
    assert.strictEqual(readBack, content);
  });
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Port Collisions with Fixed Ports**: Hardcoding `server.listen(3000)` causes `EADDRINUSE` crashes when multiple test files run in parallel or when a previous test run failed to exit cleanly. Always use ephemeral port `0` (`server.listen(0)`).
2. **Database Deadlocks in Concurrent Tests**: If tests execute against a shared test database in parallel without schema or row partitioning, concurrent transactions can deadlock when updating foreign keys or shared sequence tables.
3. **Unclosed Open Handles**: If an integration test creates a database connection pool, starts an HTTP server, or schedules a recurring heartbeat timer, and fails to close them in an `after` hook, the Node.js process hangs indefinitely and never exits.
4. **Environment Variable Bleed**: Overwriting `process.env.DATABASE_URL` inside one test alters environment variables globally for all concurrent test workers. Use dependency injection to pass configuration explicitly.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Hardcoding port numbers (`server.listen(3000)`) | Causes `EADDRINUSE` when tests run in parallel | Bind to ephemeral port `0` (`server.listen(0)`) |
| Hitting live third-party production APIs | Flaky, slow, consumes API rate limits, costs money | Mock third-party APIs using local HTTP servers |
| Forgetting to close database pools in `after()` | Process hangs indefinitely and CI runner times out | Always disconnect databases and close servers in `after()` |
| Sharing mutable database rows across tests | Tests become order-dependent and fail unpredictably | Use transaction rollbacks or database truncation |
| Pointing test suites to production database | Risk of catastrophic accidental data destruction | Enforce strict environment guards on test DB URLs |

---

## 8. Performance Characteristics

- **Ephemeral HTTP Round-Trip**: Making a local `fetch` request over an ephemeral loopback socket (`127.0.0.1`) takes roughly 1 to 3 milliseconds.
- **Transaction Rollback Speed**: Resetting database state via `ROLLBACK` executes in $< 5$ms, compared to $50\text{ms}$–$500\text{ms}$ for database migrations and truncations.

---

## 9. Engine Internals

1. **OS Ephemeral Port Allocation**:
   - Calling `bind()` with port `0` invokes the operating system kernel's dynamic port allocator.
   - The kernel assigns an available port from its ephemeral port range (typically 49152 to 65535 on modern Linux and Windows) and returns the bound port descriptor.

---

## 10. Best Practices & Design Guidelines

- **Enforce Database Safety Guards**: Write an assertion in your test database setup script verifying that the database name ends with `_test` or contains `test`, immediately terminating if connected to a production database.
- **Clean Teardown Hooks**: Always pair server starts with `after(async () => server.close())`.
- **Prefer Transaction Rollbacks**: Design your repositories to accept an optional transaction client so tests can wrap operations in `BEGIN` and `ROLLBACK`.

---

## 11. Real-World Case Study

### High-Speed CI Integration Test Suite with SQLite In-Memory
```javascript
// PROBLEM: An enterprise API test suite of 400 integration tests took 4 minutes to run
// against a physical remote Postgres instance, bottle-necking CI pipelines.

// SOLUTION: Implemented dual-driver architecture where integration tests run against
// an in-memory SQLite database (`:memory:`) via transaction rollbacks.
import { describe, it, before, after, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';

class InMemorySqlRepository {
  constructor() {
    this.table = new Map();
  }

  async insert(user) {
    this.table.set(user.id, user);
    return user;
  }

  async find(id) {
    return this.table.get(id) || null;
  }

  clear() {
    this.table.clear();
  }
}

describe('Enterprise User Service Integration', () => {
  const repo = new InMemorySqlRepository();

  afterEach(() => {
    repo.clear(); // Reset state instantaneously in memory
  });

  it('creates and reads user in sub-millisecond execution', async () => {
    const user = { id: 'usr-101', name: 'Grace Hopper', role: 'engineer' };
    await repo.insert(user);

    const fetched = await repo.find('usr-101');
    assert.deepStrictEqual(fetched, user);
  });
});

// Result: Total CI test duration dropped from 4 minutes to 3.8 seconds.
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between a Unit Test and an Integration Test?</summary>
A <b>Unit Test</b> verifies a single unit of work in complete isolation, replacing external collaborators with fakes, stubs, or mocks. An <b>Integration Test</b> verifies that two or more real software components (such as an HTTP controller, database adapter, and filesystem) collaborate and execute correctly together.
</details>

<details>
<summary>2. Why should HTTP integration tests listen on port 0?</summary>
Passing port <code>0</code> (e.g. <code>server.listen(0)</code>) instructs the operating system to dynamically assign an unused ephemeral port. This prevents <code>EADDRINUSE</code> port conflicts when running multiple test suites in parallel or in CI environments.
</details>

<details>
<summary>3. What is the Transaction Rollback strategy for database testing?</summary>
In the transaction rollback pattern, a database transaction is opened (<code>BEGIN</code>) before each test starts. The test performs database insertions and queries within that transaction. In the <code>afterEach</code> hook, the transaction is rolled back (<code>ROLLBACK</code>), instantaneously leaving the database in its original clean state without writing permanent rows.
</details>

<details>
<summary>4. What causes a test suite to hang after all tests have completed?</summary>
A test runner hangs when active handles or timers remain open in the libuv event loop. Common culprits include: active database connection pools, unclosed HTTP servers, lingering WebSockets, or uncleared <code>setInterval</code> timers.
</details>

<details>
<summary>5. How should external third-party HTTP services (like Stripe or GitHub) be tested in integration tests?</summary>
Rather than making live HTTP calls over the internet (which is slow, flaky, and violates rate limits), spin up a local in-memory HTTP mock server (or use tools like Mock Service Worker / MSW) that runs locally on an ephemeral port and returns canned mock responses.
</details>

<details>
<summary>6. What are Testcontainers and why are they useful?</summary>
Testcontainers is a library that programmatically spins up real Docker containers (e.g., PostgreSQL, Redis, Kafka) for the duration of a test suite and destroys them upon completion. It ensures that integration tests run against identical database engines and versions as production.
</details>

<details>
<summary>7. How do you test file download endpoints in an integration test?</summary>
By issuing an HTTP request using <code>fetch</code>, verifying headers (<code>Content-Type: application/pdf</code> and <code>Content-Disposition</code>), and consuming the response body as an <code>ArrayBuffer</code> to assert on byte length or checksum.
</details>

<details>
<summary>8. Why is database truncation slower than transaction rollbacks?</summary>
Truncating tables requires disk I/O, locks tables across connections, and invalidates database index caches. Transaction rollbacks simply discard uncommitted in-memory transaction logs in the database engine, executing in a fraction of the time.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build an Ephemeral HTTP Test Harness Helper
Create a reusable helper class `TestServerHarness` that wraps any standard `http.Server`, automatically starts it on an ephemeral port, provides a `fetch(endpoint, options)` method that prepends the base URL, and cleans up on `stop()`.

<details>
<summary>View Solution</summary>

```javascript
import http from 'node:http';

export class TestServerHarness {
  constructor(appHandler) {
    this.server = http.createServer(appHandler);
    this.baseUrl = '';
  }

  async start() {
    await new Promise((resolve) => {
      this.server.listen(0, '127.0.0.1', resolve);
    });
    const port = this.server.address().port;
    this.baseUrl = `http://127.0.0.1:${port}`;
  }

  fetch(path, options = {}) {
    const fullUrl = `${this.baseUrl}${path.startsWith('/') ? path : `/${path}`}`;
    return fetch(fullUrl, options);
  }

  async stop() {
    await new Promise((resolve) => {
      this.server.close(resolve);
    });
  }
}
```
</details>

### Exercise 2: Testing an Authenticated Protected Route
Implement an integration test that creates a protected HTTP route requiring `Authorization: Bearer valid-secret`, asserting that requests without the header return 401, while requests with the header return 200 with the payload.

<details>
<summary>View Solution</summary>

```javascript
import { describe, it, before, after } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';

describe('Protected Route Integration', () => {
  let server;
  let baseUrl;

  before(async () => {
    server = http.createServer((req, res) => {
      const auth = req.headers['authorization'];
      if (auth !== 'Bearer secret-token-123') {
        res.writeHead(401, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ error: 'Unauthorized' }));
      }
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ data: 'Confidential Records' }));
    });

    await new Promise(r => server.listen(0, '127.0.0.1', r));
    baseUrl = `http://127.0.0.1:${server.address().port}`;
  });

  after(async () => {
    await new Promise(r => server.close(r));
  });

  it('rejects unauthorized requests with 401', async () => {
    const res = await fetch(`${baseUrl}/data`);
    assert.strictEqual(res.status, 401);
  });

  it('allows authorized requests with 200', async () => {
    const res = await fetch(`${baseUrl}/data`, {
      headers: { 'Authorization': 'Bearer secret-token-123' },
    });
    assert.strictEqual(res.status, 200);
    const body = await res.json();
    assert.strictEqual(body.data, 'Confidential Records');
  });
});
```
</details>
