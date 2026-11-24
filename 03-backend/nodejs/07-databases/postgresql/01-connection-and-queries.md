# PostgreSQL Connection Pooling & Parameterized Queries in Node.js

> Production guide to connecting Node.js to PostgreSQL using `pg` (`node-postgres`), configuring pool sizing, lifecycle management, and strict SQL injection defenses.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Database Driver** | `pg` (`node-postgres` v8.13+) |
| **Database** | PostgreSQL 15 / 16 / 17 |
| **Security** | Zero raw SQL concatenation; parameterized queries only |

---

## 1. Why Connection Pooling is Mandatory

In PostgreSQL, each incoming TCP client connection spawns a **dedicated operating system backend process** on the database server.
- Spawning a new Postgres process for each HTTP request consumes ~5–10MB of server RAM and adds 30–100ms of TCP/TLS handshake latency.
- In Node.js, an application must use a persistent **Connection Pool** (`new pg.Pool()`). Sockets remain connected in the pool and are borrowed for query execution, then returned immediately.

```
Node.js Application Threads
     │
     ▼
[ pg.Pool Singleton (max: 20) ] ──(Borrow socket on query)──┐
     │                                                      │
     ▼                                                      ▼
[ PostgreSQL Server (5-20 persistent backend worker processes) ]
```

---

## 2. Idiomatic Connection Pool Setup (`db.js`)

```javascript
import pg from 'pg';

const { Pool } = pg;

export const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  max: 20,                       // Maximum concurrent sockets in pool
  idleTimeoutMillis: 30000,      // Close idle clients after 30s
  connectionTimeoutMillis: 2000, // Return an error after 2s if pool is fully busy
  maxUses: 7500,                 // Terminate socket after 7,500 queries to release memory
});

// Event listeners for diagnostics
pool.on('error', (err, client) => {
  console.error('Unexpected error on idle PostgreSQL client:', err);
});

// Helper for single query execution (borrows, executes, and releases automatically)
export async function query(text, params) {
  const start = Date.now();
  const res = await pool.query(text, params);
  const duration = Date.now() - start;
  if (duration > 100) {
    console.warn(`[Slow Query] (${duration}ms): ${text}`);
  }
  return res;
}
```

---

## 3. Parameterized Queries & SQL Injection Defense

> **Golden Rule**: NEVER concatenate variables, template literals, or untrusted user input into SQL query strings. Always pass arguments via the `values` array using `$1`, `$2`, etc.

```javascript
// ❌ CRITICAL VULNERABILITY (SQL Injection):
// const sql = `SELECT * FROM users WHERE email = '${req.body.email}'`;

// ✅ IDIOMATIC & SECURE (Parameterized Query):
export async function getUserByEmail(email) {
  const sql = `
    SELECT id, email, full_name, created_at
    FROM users
    WHERE email = $1
    LIMIT 1;
  `;
  const result = await query(sql, [email]);
  return result.rows[0] || null;
}
```

---

## 4. Borrowing Clients for Transactions

When executing transactions or setting session variables, check out a single client explicitly and return it via `client.release()`:

```javascript
export async function withTransaction(callback) {
  const client = await pool.connect();
  try {
    await client.query('BEGIN');
    const result = await callback(client);
    await client.query('COMMIT');
    return result;
  } catch (err) {
    await client.query('ROLLBACK');
    throw err;
  } finally {
    // CRITICAL: Always release client back to pool in finally block!
    client.release();
  }
}
```
