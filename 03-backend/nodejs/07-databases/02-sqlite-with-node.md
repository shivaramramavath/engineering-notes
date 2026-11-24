# SQLite with Node.js: Built-in `node:sqlite` vs. `better-sqlite3`

> Embedded database engineering: Synchronous V8 execution, in-memory databases, WAL mode, and high-performance embedded workloads.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Modules** | `node:sqlite` (Built-in), `better-sqlite3` (npm) |
| **Level** | Embedded Relational Databases |

---

## 1. When to Use SQLite in Production

SQLite is not a toy; it is an in-process ACID-compliant relational database:
- **Best Used For**:
  - Embedded microservices, edge devices, local development and testing.
  - Read-heavy applications serving up to millions of reads per day with WAL mode.
  - Applications where zero external network hops ($< 0.1$ms query latency) are required.
- **When NOT to Use**:
  - High concurrent write workloads from distributed multi-container pods (SQLite allows single-writer locking).

---

## 2. Enabling WAL Mode (Write-Ahead Logging)

By default, SQLite uses a rollback journal that locks readers during writes.
Enabling **WAL Mode** allows concurrent readers while a write is occurring:

```javascript
import { Database } from 'node:sqlite';

const db = new Database('app.db');

// Enable WAL mode for high read concurrency:
db.exec('PRAGMA journal_mode = WAL;');
// Synchronous normal for faster disk writes while remaining ACID safe:
db.exec('PRAGMA synchronous = NORMAL;');
```

---

## 3. Comparison: `node:sqlite` vs `better-sqlite3`

| Feature | `node:sqlite` (Native) | `better-sqlite3` (npm) |
| :--- | :--- | :--- |
| **Installation** | **Zero dependencies** (built into Node) | Requires native C++ compilation (`node-gyp`) |
| **API Style** | Synchronous | Synchronous |
| **Stability** | Release Candidate (Node 25) / Stable in 22+ | Battle-tested for 8+ years |
| **Ecosystem Support** | Direct Node standard library | Widely supported by Knex, Drizzle |
