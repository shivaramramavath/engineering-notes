# Testing with Real Databases & Testcontainers in Node.js

> Eliminating mock drift by executing unit and integration tests against real ephemeral PostgreSQL Docker containers using `@testcontainers/postgresql`.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Testing Tools** | `@testcontainers/postgresql`, `node:test` / Vitest |
| **Level** | Senior Testing Strategy |

---

## 1. Why Mocks Fail for Databases

Mocking SQL database queries (`pg-mock`, manual stubs) creates dangerous blind spots:
1. **Mock Drift**: A mock succeeds even if a migration changed a column name, dropped a constraint, or introduced a syntax error.
2. **False Confidence**: Complex features (foreign key constraints, unique indexes, transactions, trigger cascades) can only be verified against a real database engine.

**Testcontainers** solves this by spinning up a real, throwaway Docker container in code before tests run, and destroying it automatically when tests finish.

---

## 2. Implementing Testcontainers with Vitest / `node:test`

Install:
```bash
npm install -D @testcontainers/postgresql
```

Test Implementation:
```typescript
import { describe, it, beforeAll, afterAll } from 'vitest';
import { PostgreSqlContainer, StartedPostgreSqlContainer } from '@testcontainers/postgresql';
import pg from 'pg';

describe('User Repository Integration Tests', () => {
  let container: StartedPostgreSqlContainer;
  let pool: pg.Pool;

  beforeAll(async () => {
    // 1. Spin up a real, isolated PostgreSQL 16 container
    container = await new PostgreSqlContainer('postgres:16-alpine')
      .withDatabase('test_db')
      .withUsername('test_user')
      .withPassword('test_pass')
      .start();

    // 2. Connect via pg Pool
    pool = new pg.Pool({ connectionString: container.getConnectionUri() });

    // 3. Run schema migrations
    await pool.query(`
      CREATE TABLE users (
        id SERIAL PRIMARY KEY,
        email VARCHAR(255) UNIQUE NOT NULL,
        created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
      );
    `);
  }, 30000); // 30s timeout for container pull/boot

  afterAll(async () => {
    await pool?.end();
    await container?.stop();
  });

  it('should enforce unique email constraint in real PostgreSQL', async () => {
    await pool.query('INSERT INTO users (email) VALUES ($1)', ['alice@example.com']);

    // Attempt duplicate insert
    await expect(
      pool.query('INSERT INTO users (email) VALUES ($1)', ['alice@example.com'])
    ).rejects.toThrow(/duplicate key value violates unique constraint/);
  });
});
```
This guarantees 100% fidelity with production database behavior.
