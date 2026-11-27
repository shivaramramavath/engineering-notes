# CQRS & Event Sourcing

| Field | Value |
| :--- | :--- |
| **Category** | Software Architecture (Data & State Modeling) |
| **Origin** | Greg Young / Bertrand Meyer (CQS) / Udi Dahan |
| **Difficulty** | Advanced |
| **Prerequisite(s)** | [Domain Events](file:///E:/Learning/engineering-notes/08-solid-design-patterns/09-enterprise-application-patterns/16-domain-events.md), [Event-Driven Architecture](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/05-event-driven.md) |
| **Status** | Complete |

> **TL;DR**: **CQRS** separates the **Write Model** (normalized, invariant-enforcing commands) from the **Read Model** (denormalized, fast query projections). **Event Sourcing** stores an **append-only log of domain events** as the primary source of truth instead of mutating current-state rows in place.

---

## 1. The Problem: Why Do CQRS & Event Sourcing Exist?

1. **Read/Write Mismatch (CQRS)**: Writes need strict normalization, invariants, and locking; Reads need fast, denormalized, multi-table dashboards or full-text search without bogging down transactional tables.
2. **Lost History (Event Sourcing)**: Traditional `UPDATE` overwrites state—you know a customer's balance is `$50`, not *how* it got there, or what it was last Tuesday. Financial ledgers, audit logs, and collaborative editors need the full history of facts.

---

## 2. How CQRS & Event Sourcing Work Together (or Separately!)

```mermaid
flowchart LR
    Cmd["Command: Deposit($50)"] --> Write["Write Model (Aggregate)"]
    Write --> ES[("Event Store (Append-Only):<br/>1. AccountOpened<br/>2. Deposited($50)")]
    ES -->|Projector (Async)| ReadDB[("Read Model (SQL/Elastic/Redis)")]
    Query["Query: GetDashboard"] --> ReadDB
```

**Important**: You can use **CQRS without Event Sourcing** (two views/models over Postgres, or Postgres write + Elasticsearch read)! You rarely need Event Sourcing everywhere.

---

## 3. Minimal Event Sourcing Replay Example (TypeScript / Python / Java)

### Java 25 LTS
```java
import java.util.*;

public class EventSourcingDemo {
    sealed interface BankEvent permits Deposited, Withdrawn {}
    record Deposited(int amount) implements BankEvent {}
    record Withdrawn(int amount) implements BankEvent {}

    static int replayBalance(List<BankEvent> history) {
        int balance = 0;
        for (var ev : history) {
            balance += switch (ev) {
                case Deposited(int a) -> a;
                case Withdrawn(int a) -> -a;
            };
        }
        return balance;
    }

    public static void main(String[] args) {
        var log = List.<BankEvent>of(new Deposited(100), new Withdrawn(35), new Deposited(20));
        System.out.println("Replayed balance: " + replayBalance(log)); // 85
    }
}
```

### TypeScript 5.8
```ts
type BankEvent = { kind: 'dep'; amt: number } | { kind: 'wdr'; amt: number };
const replay = (events: BankEvent[]) =>
  events.reduce((bal, e) => (e.kind === 'dep' ? bal + e.amt : bal - e.amt), 0);
console.log('Balance:', replay([{ kind: 'dep', amt: 100 }, { kind: 'wdr', amt: 35 }]));
```

### Python 3.14
```python
events = [("dep", 100), ("wdr", 35), ("dep", 20)]
balance = sum(amt if kind == "dep" else -amt for kind, amt in events)
print("Balance:", balance)
```

---

## 4. Real-World Examples

1. **Banking Ledgers & Accounting Systems**: Immutable append-only journal entries; balance is derived (with periodic snapshots).
2. **Git**: Commits are an append-only event/tree log; `HEAD` working directory is a projected read state.
3. **E-Commerce Search**: Postgres for transactional order writes (**Command**) + Elasticsearch for product search (**Query**).

---

## 5. Limitations & When NOT to Use Event Sourcing

- **Schema Evolution**: Old events from 3 years ago stay in the log forever—you must write **upcasters** to migrate old event schemas.
- **Eventual Consistency**: Read projections lag milliseconds behind writes (UI must handle read-your-writes).
- **Overkill for CRUD**: Never use Event Sourcing for a simple user profile or blog CMS.

---

## 6. Anti-Patterns & Common Mistakes

| Mistake | Why It Fails | Fix |
| :--- | :--- | :--- |
| Applying Event Sourcing to the whole system | Huge operational complexity for CRUD tables | Use CQRS/ES only on **high-value audit/core** aggregates |
| Mutating or deleting past events in the Event Store | Violates append-only immutability | Append compensating events (`PaymentReversed`) |
| Replaying 100,000 events on every single read | Destroys latency | Store periodic **Snapshots** (e.g., every 100 events) |
| Ignoring "Read-Your-Own-Writes" UX | User creates an order, redirects, and sees "Not Found" due to projection lag | Return created ID/DTO from command or show optimistic UI |

---

## 7. Interview Questions

<details>
<summary>1. Can you use CQRS without Event Sourcing?</summary>

Yes! Most teams use CQRS simply by having separate Command services (writing to normalized tables) and Query services (reading from SQL views, read replicas, or Elasticsearch) without an Event Store.

</details>

<details>
<summary>2. How do Snapshots solve slow Event Sourcing reads?</summary>

Periodically save `(aggregateId, version, serializedState)` every $N$ events so loading an aggregate only replays events *after* the latest snapshot.

</details>

<details>
<summary>3. How do you handle "deleting" or fixing a mistake in Event Sourcing?</summary>

Never `UPDATE`/`DELETE` historical events; append a new compensating event (`OrderCancelled`, `EntryCorrected`). For GDPR deletion, use **crypto-shredding** (encrypt PII with a per-user key and delete the key).

</details>

<details>
<summary>4. What is the main trade-off of asynchronous Read Projections in CQRS?</summary>

**Eventual consistency**: there is a brief delay between a Command succeeding and the Query Read Model reflecting the change.

</details>

---

## 8. Cheat Sheet

- **CQRS**: Separate Write (Commands + invariants) from Read (fast projections).
- **Event Sourcing**: Append-only event log = source of truth + periodic snapshots.

---

## 9. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Microservices (and When NOT to Use Them)](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/06-microservices-and-when-not-to.md) | [Module 10](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/README.md) | [Serverless & Function Patterns](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/08-serverless-and-function-patterns.md) |
