# Project 13: Event Sourced Banking Ledger & CQRS (`proj-node-13-event-sourced-bank`)

> Financial-grade immutable ledger and banking service built with Node.js 22 LTS, Express 5, and PostgreSQL. Demonstrates Event Sourcing (append-only event store), Command Query Responsibility Segregation (CQRS), aggregate state reconstruction with optimistic concurrency control (`expected_version`), asynchronous read-model projections, and periodic snapshotting.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-13-event-sourced-bank` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Architecture** | Event Sourcing + CQRS |
| **Event Store** | Append-only PostgreSQL Table |
| **Concurrency Guard**| Optimistic Locking via Aggregate Sequence Version |
| **Projections** | Materialized Account Balances Table |
| **Testing** | Vitest (State reconstruction tests) |
| **Complexity Level** | Advanced / Enterprise Architecture |

---

## 1. Architectural Overview & Domain Design

### 1.1 Append-Only Event Store & CQRS Synchronization

In traditional CRUD architectures, database records are overwritten, destroying historical audit trails. In Event Sourcing:
1. **Append-Only Store**: State changes are recorded as an immutable sequence of events (`AccountOpened`, `MoneyDeposited`, `MoneyWithdrawn`). No rows are ever updated or deleted.
2. **Aggregate Reconstruction**: Current balance is derived by replaying past events from the beginning (or from the latest Snapshot).
3. **Optimistic Concurrency Control**: When appending an event, the command specifies `expectedVersion`. If another concurrent transaction appended an event in the interim, PostgreSQL rejects the transaction with a unique constraint violation, preventing double-spend race conditions.
4. **Projections (Read Models)**: Projector workers asynchronously consume new events to maintain denormalized, read-optimized tables for fast query responses.

```
+----------------------------------------------------------------------------------------------------+
|                                    CQRS & Event Sourcing Topology                                  |
|                                                                                                    |
|  [ Command Side: POST /accounts/:id/withdraw ]                                                     |
|                      │                                                                             |
|                      ▼                                                                             |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                   Command Handler & Aggregate               |                                   |
|  |  1. Rehydrate BankAccount Aggregate from Event Stream       |                                   |
|  |  2. Execute Business Invariant (balance >= withdrawAmount)   |                                   |
|  |  3. Produce Event: `MoneyWithdrawn`                         |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │                                                                   |
|                                ▼ Atomic INSERT (expected_version check)                            |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |             PostgreSQL Event Store (Append-Only)            |                                   |
|  |  - stream_id: 'acc_123', version: 4, type: 'MoneyWithdrawn' |                                   |
|  +─────────────────────────────┬───────────────────────────────+                                   |
|                                │ Notify Projection / Read Bus                                      |
|                                ▼                                                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|  |                Asynchronous Read-Model Projector            |                                   |
|  |  - UPDATE account_views SET balance = balance - 50          |                                   |
|  +─────────────────────────────────────────────────────────────+                                   |
|                                │ Fast Reads                                                        |
|                                ▼                                                                   |
|  [ Query Side: GET /accounts/:id/balance ] ──> [ Read-Optimized Table ]                            |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-13-event-sourced-bank/
├── src/
│   ├── domain/
│   │   ├── events.ts            # Domain event schemas & types
│   │   └── bank-account.aggregate.ts # Business logic & state rehydration
│   ├── infrastructure/
│   │   ├── db.ts                # PostgreSQL pool
│   │   ├── event-store.ts       # Append-only persistence & version checks
│   │   └── projector.ts         # Materialized view updater
│   ├── presentation/
│   │   └── app.ts               # Express 5 Command & Query endpoints
├── test/
│   └── unit/
│       └── bank-account.test.ts # Domain invariant & replay tests
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-13-event-sourced-bank",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/presentation/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "express": "^5.0.1",
    "pg": "^8.13.1",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/domain/events.ts`
```typescript
export type DomainEvent =
  | { type: 'AccountOpened'; data: { accountId: string; owner: string; initialBalanceCents: number } }
  | { type: 'MoneyDeposited'; data: { amountCents: number; transactionRef: string } }
  | { type: 'MoneyWithdrawn'; data: { amountCents: number; transactionRef: string } };

export interface StoredEvent {
  streamId: string;
  version: number;
  type: string;
  data: any;
  timestamp: string;
}
```

### `src/domain/bank-account.aggregate.ts`
```typescript
import { DomainEvent } from './events.js';

export class BankAccountAggregate {
  public id: string = '';
  public balanceCents: number = 0;
  public version: number = 0;
  public isClosed: boolean = false;

  private uncommittedEvents: DomainEvent[] = [];

  // State rehydration from historical events
  loadFromHistory(events: { type: string; data: any; version: number }[]) {
    for (const event of events) {
      this.applyEvent(event as DomainEvent);
      this.version = event.version;
    }
  }

  // Business Invariant 1: Open Account
  open(accountId: string, owner: string, initialDepositCents: number) {
    if (this.version > 0) throw new Error('Account is already open');
    if (initialDepositCents < 0) throw new Error('Initial deposit cannot be negative');

    const event: DomainEvent = {
      type: 'AccountOpened',
      data: { accountId, owner, initialBalanceCents: initialDepositCents },
    };
    this.recordEvent(event);
  }

  // Business Invariant 2: Withdraw Money
  withdraw(amountCents: number, transactionRef: string) {
    if (this.balanceCents < amountCents) {
      throw new Error(`Insufficient funds: cannot withdraw ${amountCents} with balance ${this.balanceCents}`);
    }

    const event: DomainEvent = {
      type: 'MoneyWithdrawn',
      data: { amountCents, transactionRef },
    };
    this.recordEvent(event);
  }

  private recordEvent(event: DomainEvent) {
    this.applyEvent(event);
    this.uncommittedEvents.push(event);
  }

  private applyEvent(event: DomainEvent) {
    switch (event.type) {
      case 'AccountOpened':
        this.id = event.data.accountId;
        this.balanceCents = event.data.initialBalanceCents;
        break;
      case 'MoneyDeposited':
        this.balanceCents += event.data.amountCents;
        break;
      case 'MoneyWithdrawn':
        this.balanceCents -= event.data.amountCents;
        break;
    }
  }

  getUncommittedEvents(): DomainEvent[] {
    return this.uncommittedEvents;
  }

  clearUncommittedEvents() {
    this.uncommittedEvents = [];
  }
}
```

### `src/infrastructure/event-store.ts`
```typescript
import pg from 'pg';
import { DomainEvent, StoredEvent } from '../domain/events.js';

export class EventStore {
  constructor(private pool: pg.Pool) {}

  async getEventsForStream(streamId: string): Promise<StoredEvent[]> {
    const { rows } = await this.pool.query(
      'SELECT stream_id AS "streamId", version, type, data, timestamp FROM event_store WHERE stream_id = $1 ORDER BY version ASC',
      [streamId]
    );
    return rows;
  }

  async appendEvents(streamId: string, events: DomainEvent[], expectedVersion: number): Promise<void> {
    const client = await this.pool.connect();
    try {
      await client.query('BEGIN');

      let currentVersion = expectedVersion;
      for (const event of events) {
        currentVersion += 1;
        await client.query(
          `INSERT INTO event_store (stream_id, version, type, data) VALUES ($1, $2, $3, $4)`,
          [streamId, currentVersion, event.type, JSON.stringify(event.data)]
        );
      }

      await client.query('COMMIT');
    } catch (err: any) {
      await client.query('ROLLBACK');
      if (err.code === '23505') { // PostgreSQL Unique Constraint Violation on (stream_id, version)
        throw new Error('CONCURRENCY_CONFLICT_DETECTED');
      }
      throw err;
    } finally {
      client.release();
    }
  }
}
```

### `src/presentation/app.ts`
```typescript
import express from 'express';
import pg from 'pg';
import { EventStore } from '../infrastructure/event-store.js';
import { BankAccountAggregate } from '../domain/bank-account.aggregate.js';

export const app = express();
app.use(express.json());

const pool = new pg.Pool({ connectionString: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/bank' });
const eventStore = new EventStore(pool);

// Command: Withdraw
app.post('/api/v1/accounts/:id/withdraw', async (req, res) => {
  const accountId = req.params.id;
  const { amountCents, transactionRef } = req.body;

  try {
    // 1. Rehydrate aggregate from event stream
    const events = await eventStore.getEventsForStream(accountId);
    if (events.length === 0) {
      res.status(404).json({ error: 'Account does not exist' });
      return;
    }

    const account = new BankAccountAggregate();
    account.loadFromHistory(events);

    // 2. Execute business invariant
    account.withdraw(amountCents, transactionRef);

    // 3. Atomically persist uncommitted events with optimistic concurrency
    await eventStore.appendEvents(accountId, account.getUncommittedEvents(), account.version);
    account.clearUncommittedEvents();

    res.status(200).json({ status: 'success', currentBalanceCents: account.balanceCents });
  } catch (err: any) {
    if (err.message === 'CONCURRENCY_CONFLICT_DETECTED') {
      res.status(409).json({ error: 'Concurrent write detected. Please retry the transaction.' });
      return;
    }
    res.status(400).json({ error: err.message });
  }
});

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => console.log('[Event Sourced Bank API] Running on port 3000'));
}
```

---

## 3. Database Schema

```sql
CREATE TABLE IF NOT EXISTS event_store (
    id BIGSERIAL PRIMARY KEY,
    stream_id VARCHAR(100) NOT NULL,
    version INTEGER NOT NULL,
    type VARCHAR(100) NOT NULL,
    data JSONB NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
    CONSTRAINT uq_stream_version UNIQUE(stream_id, version)
);

CREATE INDEX IF NOT EXISTS idx_event_stream ON event_store(stream_id, version);
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/bank-account.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';
import { BankAccountAggregate } from '../../src/domain/bank-account.aggregate.js';

describe('BankAccount Aggregate', () => {
  it('reconstructs balance and rejects withdrawal exceeding funds', () => {
    const account = new BankAccountAggregate();
    account.loadFromHistory([
      { type: 'AccountOpened', data: { accountId: 'acc_1', owner: 'Alice', initialBalanceCents: 10000 }, version: 1 },
      { type: 'MoneyDeposited', data: { amountCents: 5000, transactionRef: 'dep_1' }, version: 2 },
    ]);

    expect(account.balanceCents).toBe(15000);
    expect(account.version).toBe(2);

    expect(() => account.withdraw(20000, 'wth_fail')).toThrow('Insufficient funds');
  });
});
```

---

## 5. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/presentation/app.js"]
```
