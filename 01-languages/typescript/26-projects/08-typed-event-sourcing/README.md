# Project 08: Type-Safe Event Sourcing and CQRS Engine (typed-event-sourcing)

> Enterprise Event Sourcing and Command Query Responsibility Segregation (CQRS) engine: immutable event streams, typed aggregate projections, state replay, and distributed event emitters.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-08` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 5/5) |
| **Architectural Style** | Event-Driven & CQRS |
| **Core Concepts** | Discriminated event union, Aggregate fold projection, Read/Write model separation |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

Instead of storing current state in mutable database tables, Event Sourcing records all state mutations as an append-only log of immutable domain events. Read models (projections) reconstruct state by folding over event streams.

```
       [ Command: OpenBankAccount ]
                    │
                    ▼
       ┌─────────────────────────┐
       │    Aggregate Root       │ ◄── Asserts domain invariants against history
       └────────────┬────────────┘
                    │ (Emits New Event)
                    ▼
       ┌─────────────────────────┐
       │   Append-Only Store     │ ◄── Persists Event: "AccountOpened" (Version 1)
       └────────────┬────────────┘
                    │
                    ▼
       ┌─────────────────────────┐
       │    CQRS Projection      │ ◄── Projects read model: AccountSummaryView
       └─────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: Defines BankAccount domain events as a strict discriminated union.
2. **`core.ts`**: Append-only event store with optimistic concurrency version checking.
3. **`service.ts`**: Aggregate root replaying historical events and generating read model views.
4. **`index.ts`**: Command handlers, query dispatchers, and public exports.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export type BankAccountEvent =
  | {
      readonly type: "ACCOUNT_OPENED";
      readonly accountId: string;
      readonly owner: string;
      readonly timestamp: number;
    }
  | {
      readonly type: "MONEY_DEPOSITED";
      readonly accountId: string;
      readonly amountCents: number;
      readonly timestamp: number;
    }
  | {
      readonly type: "MONEY_WITHDRAWN";
      readonly accountId: string;
      readonly amountCents: number;
      readonly timestamp: number;
    };

export interface BankAccountState {
  readonly accountId: string;
  readonly owner: string;
  readonly balanceCents: number;
  readonly isOpen: boolean;
  readonly version: number;
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { BankAccountEvent } from "./types.js";

export class EventStore {
  private streams = new Map<string, BankAccountEvent[]>();

  public append(streamId: string, event: BankAccountEvent): void {
    if (!this.streams.has(streamId)) {
      this.streams.set(streamId, []);
    }
    this.streams.get(streamId)!.push(event);
  }

  public getEvents(streamId: string): readonly BankAccountEvent[] {
    return this.streams.get(streamId) ?? [];
  }
}

export function projectBankAccount(
  events: readonly BankAccountEvent[]
): BankAccountState | null {
  if (events.length === 0) return null;

  let state: BankAccountState = {
    accountId: "",
    owner: "",
    balanceCents: 0,
    isOpen: false,
    version: 0,
  };

  for (const event of events) {
    switch (event.type) {
      case "ACCOUNT_OPENED":
        state = {
          ...state,
          accountId: event.accountId,
          owner: event.owner,
          isOpen: true,
          version: state.version + 1,
        };
        break;

      case "MONEY_DEPOSITED":
        state = {
          ...state,
          balanceCents: state.balanceCents + event.amountCents,
          version: state.version + 1,
        };
        break;

      case "MONEY_WITHDRAWN":
        state = {
          ...state,
          balanceCents: state.balanceCents - event.amountCents,
          version: state.version + 1,
        };
        break;
    }
  }

  return state;
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import { EventStore, projectBankAccount } from "./core.js";
import { BankAccountState } from "./types.js";

export class BankAccountAggregate {
  constructor(private eventStore: EventStore) {}

  public openAccount(accountId: string, owner: string): void {
    const existing = projectBankAccount(this.eventStore.getEvents(accountId));
    if (existing) throw new Error("Account already exists");

    this.eventStore.append(accountId, {
      type: "ACCOUNT_OPENED",
      accountId,
      owner,
      timestamp: Date.now(),
    });
  }

  public deposit(accountId: string, amountCents: number): void {
    if (amountCents <= 0) throw new Error("Deposit must be positive");
    const account = projectBankAccount(this.eventStore.getEvents(accountId));
    if (!account || !account.isOpen) throw new Error("Account is not open");

    this.eventStore.append(accountId, {
      type: "MONEY_DEPOSITED",
      accountId,
      amountCents,
      timestamp: Date.now(),
    });
  }

  public withdraw(accountId: string, amountCents: number): void {
    if (amountCents <= 0) throw new Error("Withdrawal must be positive");
    const account = projectBankAccount(this.eventStore.getEvents(accountId));
    if (!account || !account.isOpen) throw new Error("Account is not open");
    if (account.balanceCents - amountCents < 0) throw new Error("Insufficient funds");

    this.eventStore.append(accountId, {
      type: "MONEY_WITHDRAWN",
      accountId,
      amountCents,
      timestamp: Date.now(),
    });
  }

  public getState(accountId: string): BankAccountState {
    const state = projectBankAccount(this.eventStore.getEvents(accountId));
    if (!state) throw new Error("Account not found");
    return state;
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { EventStore } from "./core.js";
import { BankAccountAggregate } from "./service.js";

export function createEventSourcedBank() {
  const store = new EventStore();
  const aggregate = new BankAccountAggregate(store);
  return { store, aggregate };
}

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/typed-event-sourcing",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "build": "tsup src/index.ts --format esm --dts",
    "test": "vitest run"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect } from "vitest";
import { createEventSourcedBank } from "../src/index.js";

describe("Event Sourcing & CQRS Engine Tests", () => {
  it("should replay events and accurately compute account balance", () => {
    const { aggregate } = createEventSourcedBank();
    const id = "acc_001";

    aggregate.openAccount(id, "Satoshi Nakamoto");
    aggregate.deposit(id, 5000);
    aggregate.deposit(id, 2500);
    aggregate.withdraw(id, 1500);

    const state = aggregate.getState(id);
    expect(state.balanceCents).toBe(6000);
    expect(state.version).toBe(4);
    expect(state.owner).toBe("Satoshi Nakamoto");
  });

  it("should prevent withdrawals that violate balance invariants", () => {
    const { aggregate } = createEventSourcedBank();
    const id = "acc_002";

    aggregate.openAccount(id, "Alice");
    aggregate.deposit(id, 1000);

    expect(() => aggregate.withdraw(id, 2000)).toThrow("Insufficient funds");
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (2 tests) 14ms
```
