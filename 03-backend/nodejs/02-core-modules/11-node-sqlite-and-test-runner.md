# Built-in SQLite (`node:sqlite`) & Test Runner (`node:test`)

> Comprehensive guide to Node's built-in relational database module (`node:sqlite`) and native zero-dependency test runner (`node:test`).

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Built-in Modules** | `node:sqlite`, `node:test`, `node:assert/strict` |
| **Status** | `node:test` (Stable), `node:sqlite` (Release Candidate) |

---

## 1. Native SQLite Module (`node:sqlite`)

Starting with Node.js 22.5.0, Node.js embeds SQLite directly into the binary. No external C++ compilation or npm modules (`sqlite3`, `better-sqlite3`) are required.

### Key Architecture Points
- **Primary Class**: `Database` (`import { Database } from 'node:sqlite'`).
- **Synchronous Execution**: Like `better-sqlite3`, queries execute synchronously on the main thread.
- **In-Memory or File**: Pass `':memory:'` for transient testing or a file path (e.g. `'app.db'`).

### Example: CRUD with `node:sqlite`
```javascript
import { Database } from 'node:sqlite';

// Initialize in-memory database
const db = new Database(':memory:');

// Execute DDL schema
db.exec(`
  CREATE TABLE users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
  ) STRICT;
`);

// Prepared statement for insertion
const insert = db.prepare('INSERT INTO users (name, email) VALUES (?, ?)');
const result = insert.run('Alice Chen', 'alice@example.com');
console.log('Inserted row ID:', result.lastInsertRowid);

// Querying records
const query = db.prepare('SELECT * FROM users WHERE email = ?');
const user = query.get('alice@example.com');
console.log('User record:', user);

// Fetching all records
const allUsers = db.prepare('SELECT * FROM users').all();
console.log('Total users:', allUsers.length);

db.close();
```

---

## 2. Built-in Test Runner (`node:test`)

Node.js ships with a zero-dependency test runner supporting suites, lifecycle hooks, assertions, mocking, and coverage.

### Example: Test Suite with `node:test`
```javascript
import { describe, it, before, after, mock } from 'node:test';
import assert from 'node:assert/strict';

describe('User Authentication Service', () => {
  let dbConnection;

  before(() => {
    // Setup test resources
    dbConnection = { isConnected: true };
  });

  after(() => {
    // Teardown
    dbConnection = null;
  });

  it('should hash passwords correctly and verify match', async () => {
    const plain = 'secret123';
    const fakeHash = `hash_${plain}`;
    
    assert.equal(fakeHash, 'hash_secret123');
    assert.notEqual(plain, fakeHash);
  });

  it('should mock a payment gateway call', (t) => {
    const paymentGateway = {
      charge: (amount) => ({ success: true, txId: 'tx_123' }),
    };

    // Built-in mock tracker
    t.mock.method(paymentGateway, 'charge');

    const res = paymentGateway.charge(100);
    assert.equal(res.success, true);
    assert.equal(paymentGateway.charge.mock.callCount(), 1);
  });
});
```

### Running Tests via CLI
```bash
# Run all test files matching *.test.js or test/*.js
node --test

# Run in watch mode
node --test --watch

# Run with built-in code coverage report
node --test --experimental-test-coverage
```
