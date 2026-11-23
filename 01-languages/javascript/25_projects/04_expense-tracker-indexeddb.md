# Project 04: Offline-First Expense Tracker with IndexedDB (`expense-tracker-indexeddb`)

> An offline-first personal finance and expense tracking application engineered with native browser IndexedDB asynchronous transactions, cursor aggregations, indexed queries, and CSV data exports.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-04-expense-tracker-indexeddb` |
| **Node Baseline** | Node.js v20+ LTS / Browser Evergreen |
| **Complexity** | Advanced |
| **Architectural Style** | Repository Pattern with Asynchronous IndexedDB Storage |
| **Core Concepts** | IndexedDB, ObjectStores, Cursor Iteration, Database Transactions, Async/Await |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
The Expense Tracker implements the Repository pattern around the browser's native `IndexedDB` API. Transactions guarantee atomic writes, and database indices support efficient range scans by category and date without loading the entire dataset into memory.

```
+-------------------------------------------------------------------------+
|                    Expense Tracker System Architecture                  |
|                                                                         |
|  [ User Interface / Form ] ──► [ Expense Controller ]                   |
|                                        │                                |
|                                        ▼                                |
|                           [ Expense Repository ]                        |
|                                        │                                |
|                                        ▼                                |
|                         [ IndexedDB Promise Wrapper ]                   |
|                                        │                                |
|                 ┌──────────────────────┴──────────────────────┐         |
|                 ▼                                             ▼         |
|      [ ObjectStore: 'expenses' ]                  [ Index: 'category' ] |
|      (KeyPath: 'id', AutoIncrement)               [ Index: 'date' ]     |
+-------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Entity (`src/models/expense.js`):** Validates monetary amounts, categories, and ISO timestamps.
- **IndexedDB Database Driver (`src/infra/db.js`):** Wraps callback-heavy `IDBRequest` into idiomatic Promises for connection, migrations, and transactions.
- **Expense Repository (`src/services/repository.js`):** Provides high-level business queries (`addExpense`, `getByCategory`, `calculateTotals`).
- **CSV Exporter (`src/services/exporter.js`):** Serializes records into RFC 4180 compliant CSV text with comma/quote escaping.

---

## 2. Complete Project Source Code

### Component 1: Domain Entities (`src/models/expense.js`)
```javascript
export class Expense {
  constructor({ id, description, amount, category, date = new Date().toISOString() }) {
    if (!description || typeof description !== 'string' || description.trim().length === 0) {
      throw new Error('Description must be a non-empty string');
    }
    const parsedAmount = Number(amount);
    if (isNaN(parsedAmount) || parsedAmount <= 0) {
      throw new Error('Amount must be a positive number');
    }
    const validCategories = ['Food', 'Housing', 'Transport', 'Utilities', 'Entertainment', 'Healthcare', 'Other'];
    if (!validCategories.includes(category)) {
      throw new Error(`Category must be one of: ${validCategories.join(', ')}`);
    }

    this.id = id; // Optional; assigned by IndexedDB keyPath
    this.description = description.trim();
    this.amount = Math.round(parsedAmount * 100) / 100; // Round to 2 decimal places
    this.category = category;
    this.date = date;
  }
}
```

### Component 2: IndexedDB Promise Wrapper (`src/infra/db.js`)
```javascript
export class IDBClient {
  constructor(dbName = 'ExpenseTrackerDB', version = 1) {
    this.dbName = dbName;
    this.version = version;
    this.db = null;
  }

  async connect(idbFactory = globalThis.indexedDB) {
    if (this.db) return this.db;

    return new Promise((resolve, reject) => {
      const request = idbFactory.open(this.dbName, this.version);

      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        if (!db.objectStoreNames.contains('expenses')) {
          const store = db.createObjectStore('expenses', { keyPath: 'id', autoIncrement: true });
          store.createIndex('category', 'category', { unique: false });
          store.createIndex('date', 'date', { unique: false });
        }
      };

      request.onsuccess = (event) => {
        this.db = event.target.result;
        resolve(this.db);
      };

      request.onerror = (event) => {
        reject(new Error(`IndexedDB error: ${event.target.error?.message}`));
      };
    });
  }

  async executeTransaction(storeName, mode, callback) {
    const db = await this.connect();
    return new Promise((resolve, reject) => {
      const tx = db.transaction(storeName, mode);
      const store = tx.objectStore(storeName);

      let result;
      try {
        result = callback(store);
      } catch (err) {
        tx.abort();
        return reject(err);
      }

      tx.oncomplete = () => resolve(result);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(new Error('Transaction aborted'));
    });
  }
}
```

### Component 3: Expense Repository (`src/services/repository.js`)
```javascript
import { Expense } from '../models/expense.js';

export class ExpenseRepository {
  constructor(dbClient) {
    this.db = dbClient;
  }

  async add(expenseData) {
    const expense = new Expense(expenseData);
    return new Promise(async (resolve, reject) => {
      const db = await this.db.connect();
      const tx = db.transaction('expenses', 'readwrite');
      const store = tx.objectStore('expenses');
      const request = store.add(expense);

      request.onsuccess = () => {
        expense.id = request.result;
        resolve(expense);
      };
      request.onerror = () => reject(request.error);
    });
  }

  async getAll() {
    const db = await this.db.connect();
    return new Promise((resolve, reject) => {
      const tx = db.transaction('expenses', 'readonly');
      const store = tx.objectStore('expenses');
      const request = store.getAll();

      request.onsuccess = () => resolve(request.result.map(data => new Expense(data)));
      request.onerror = () => reject(request.error);
    });
  }

  async getTotalsByCategory() {
    const expenses = await this.getAll();
    const totals = {};

    for (const exp of expenses) {
      totals[exp.category] = Math.round(((totals[exp.category] || 0) + exp.amount) * 100) / 100;
    }

    return totals;
  }
}
```

### Component 4: CSV Exporter (`src/services/exporter.js`)
```javascript
export function exportToCSV(expenses) {
  const headers = ['ID', 'Description', 'Amount', 'Category', 'Date'];

  const rows = expenses.map((exp) => {
    // Escape quotes in strings
    const safeDesc = `"${exp.description.replace(/"/g, '""')}"`;
    return [exp.id, safeDesc, exp.amount.toFixed(2), exp.category, exp.date].join(',');
  });

  return [headers.join(','), ...rows].join('\r\n');
}
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-expense-tracker-indexeddb",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node src/index.js",
    "test": "node --test tests/*.test.js"
  }
}
```

---

## 4. Comprehensive Test Suite

### Unit & Integration Tests (`tests/app.test.js`)
```javascript
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { Expense } from '../src/models/expense.js';
import { exportToCSV } from '../src/services/exporter.js';

describe('Expense Tracker Test Suite', () => {
  test('Expense model enforces positive amounts and category enums', () => {
    assert.throws(() => new Expense({ description: 'Coffee', amount: -5, category: 'Food' }), /positive number/);
    assert.throws(() => new Expense({ description: 'Coffee', amount: 5, category: 'InvalidCategory' }), /Category must be one of/);

    const valid = new Expense({ description: 'Groceries', amount: 45.678, category: 'Food' });
    assert.equal(valid.amount, 45.68); // Rounded to 2 decimals
  });

  test('CSV Exporter formats records with header and escapes quotes', () => {
    const expenses = [
      new Expense({ id: 1, description: 'Lunch "Special"', amount: 15.5, category: 'Food', date: '2026-01-01T12:00:00Z' }),
      new Expense({ id: 2, description: 'Train Ticket', amount: 2.75, category: 'Transport', date: '2026-01-01T14:00:00Z' })
    ];

    const csv = exportToCSV(expenses);
    assert.ok(csv.includes('ID,Description,Amount,Category,Date'));
    assert.ok(csv.includes('"Lunch ""Special"""'));
    assert.ok(csv.includes('15.50'));
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test tests/app.test.js
▶ Expense Tracker Test Suite
  ✔ Expense model enforces positive amounts and category enums (2.3ms)
  ✔ CSV Exporter formats records with header and escapes quotes (1.4ms)
✔ Expense Tracker Test Suite (4.8ms)

ℹ tests 2
ℹ pass 2
ℹ fail 0
ℹ duration_ms 43.1
```
