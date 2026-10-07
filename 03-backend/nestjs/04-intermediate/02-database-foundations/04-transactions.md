# Transactions

A transaction groups several database operations into **one unit of work**: either all of them take effect, or none do. Without it, a crash or error halfway through a multi-step operation leaves your data in a state that never should have existed (money debited but not credited, an order without items).

Prerequisites: [Repository pattern](./03-repository-pattern.md), [database connection](./02-database-connection.md).

## ACID in practice

| Property | Meaning | What it protects you from |
|----------|---------|---------------------------|
| **Atomicity** | All or nothing | Half-finished operations |
| **Consistency** | Constraints hold before and after | Violating foreign keys, uniques, checks |
| **Isolation** | Concurrent transactions don't see each other's partial work (to a configurable degree) | Race conditions between requests |
| **Durability** | Committed data survives crashes | Lost writes |

## When you need one

Use a transaction when **two or more writes must succeed together**:

```text
transfer(from, to, amount):   debit(from)  +  credit(to)     → atomic
placeOrder(cart):             insert order + insert items + decrement stock → atomic
```

You do **not** need one for a single `INSERT`/`UPDATE` (a single statement is already atomic), or for independent writes where partial success is acceptable.

## The shape of every transaction

```text
BEGIN
  step 1
  step 2
  step 3        ← any throw jumps to ROLLBACK
COMMIT          ← only reached if every step succeeded
```

The code pattern is the same in every ORM: run your steps inside a callback; **if the callback throws, the transaction rolls back**.

### TypeORM

```ts
await this.dataSource.transaction(async (manager) => {
  await manager.decrement(Account, { id: fromId }, 'balance', amount);
  await manager.increment(Account, { id: toId }, 'balance', amount);
});
```

Use the provided `manager`, **not** injected repositories: repositories use the default connection and wouldn't be part of the transaction. For finer control use a `QueryRunner` (`connect`, `startTransaction`, `commitTransaction`, `rollbackTransaction`) and **always** `release()` in `finally`. See [TypeORM transactions](../03-typeorm/07-transactions.md).

### Prisma

```ts
// batch: array of operations, run in one transaction
await prisma.$transaction([
  prisma.account.update({ where: { id: fromId }, data: { balance: { decrement: amount } } }),
  prisma.account.update({ where: { id: toId }, data: { balance: { increment: amount } } }),
]);

// interactive: logic between steps
await prisma.$transaction(async (tx) => {
  const from = await tx.account.findUniqueOrThrow({ where: { id: fromId } });
  if (from.balance < amount) throw new Error('Insufficient funds');   // rolls back
  await tx.account.update({ where: { id: fromId }, data: { balance: { decrement: amount } } });
  await tx.account.update({ where: { id: toId }, data: { balance: { increment: amount } } });
}, { timeout: 5000 });
```

Inside the interactive callback use **`tx`**, not `prisma`. Interactive transactions have a timeout (default is short; check your version's docs), so keep them quick. See [Prisma transactions](../04-prisma/07-transactions.md).

### Mongoose / MongoDB

```ts
const session = await connection.startSession();
try {
  await session.withTransaction(async () => {
    await Account.updateOne({ _id: fromId }, { $inc: { balance: -amount } }, { session });
    await Account.updateOne({ _id: toId }, { $inc: { balance: amount } }, { session });
  });
} finally {
  await session.endSession();
}
```

Transactions require a **replica set** (or sharded cluster), even for local development. Pass `{ session }` to every operation. See [Mongoose transactions](../05-mongoose/07-transactions.md). If a design needs transactions everywhere, reconsider whether a relational database fits better; a single MongoDB document update is already atomic.

## Passing the transaction to your repositories

The hard part isn't `BEGIN`/`COMMIT`; it's making **several repository calls share the same transaction**. Options:

**1. Explicit parameter** (clear, a bit noisy):

```ts
await this.dataSource.transaction(async (manager) => {
  await this.accounts.debit(fromId, amount, manager);
  await this.accounts.credit(toId, amount, manager);
});

// repository
debit(id: string, amount: number, manager = this.defaultManager) { /* use manager */ }
```

**2. Ambient context** via `AsyncLocalStorage`: repositories look up the current transaction automatically. Community libraries exist for this (for example for TypeORM and Prisma); they remove the parameter threading but add implicit behavior, so evaluate their maintenance and how they handle nested transactions before adopting. See also [scopes and request context](../../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

**3. Unit-of-work/service-level repository**: one repository method performs the whole multi-step write internally. Simple when the operation is self-contained.

Decide where the **transaction boundary** lives: usually in the **service method** that represents the business operation, not in controllers or deep in repositories.

## Isolation levels

Isolation controls what concurrent transactions can see. Stronger isolation means fewer anomalies and more conflicts/retries.

| Level | Prevents | Notes |
|-------|----------|-------|
| Read uncommitted | Almost nothing | Rarely useful; PostgreSQL treats it as Read committed |
| **Read committed** | Dirty reads | **Default in PostgreSQL**; each statement sees the latest committed data |
| Repeatable read | + non-repeatable reads | MySQL/InnoDB's default; PostgreSQL's version also avoids phantoms |
| Serializable | + all anomalies (as if run one at a time) | Safest; may abort transactions with serialization failures you must retry |

Anomalies you'll meet:

- **Lost update**: two requests read a balance of 100, both write 100 − 10. The second overwrites the first.
- **Non-repeatable read**: reading the same row twice in a transaction returns different values.
- **Phantom**: re-running a query returns new rows.
- **Write skew**: two transactions each read overlapping data and write disjoint rows, jointly breaking an invariant (for example two doctors both going off-call).

Defaults are fine for many apps, as long as you handle the **read-modify-write** cases below. Set a stricter level per transaction only where an invariant demands it, and be ready to retry.

## Read-modify-write: the classic bug

```ts
// ❌ race: two concurrent requests lose an update
const acc = await repo.findOneBy({ id });
acc.balance -= amount;
await repo.save(acc);
```

Fixes, from simplest:

1. **Atomic update in the database**: `UPDATE account SET balance = balance - $1 WHERE id = $2 AND balance >= $1` (check affected rows). In ORMs: `decrement`, Prisma `{ decrement: n }`, Mongo `$inc`.
2. **Pessimistic lock**: lock the row for the transaction (`SELECT ... FOR UPDATE`; TypeORM `lock: { mode: 'pessimistic_write' }` inside a transaction). Others wait.
3. **Optimistic locking**: a `version` column; update `WHERE id = ? AND version = ?` and retry if no row was updated (TypeORM `@VersionColumn`; Prisma needs a manual version field).

Prefer atomic updates; use locks when you must read, decide, then write.

## Keep transactions short and local

A transaction holds locks and a pooled connection until it ends.

- **No external calls inside** (HTTP APIs, email, message publishing). A slow call keeps locks open, and if the transaction rolls back after the email was sent, the side effect can't be undone.
- Do only database work inside; do side effects **after commit**.
- For reliable "write data and publish an event", use the [transactional outbox](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md): write the event row in the same transaction, publish it afterwards.
- Don't hold a transaction open while waiting for user input or across requests.

## Failure handling

- **Throwing rolls back.** Let the error propagate (or rethrow) so the transaction aborts; swallowing errors inside the callback can leave you committing partial work.
- **Deadlocks and serialization failures** are normal under contention: the database aborts one transaction. Retry the whole transaction a few times with backoff, only if it's safe to repeat (idempotent). See [database errors](./07-database-errors.md).
- **Nested transactions:** most ORMs don't truly nest. A "nested" call usually joins the outer transaction or uses savepoints depending on the tool. Check behavior before relying on it.
- **Idempotency:** retried transactions should not double-apply effects ([idempotency](../08-api-design/06-idempotency.md)).

## Testing transactions

Verify rollback explicitly in [integration tests](../01-testing/05-integration-testing.md): make a middle step throw, then assert earlier writes were **not** persisted. A mocked repository can't prove this.

## Common mistakes

- **Using injected repositories/`prisma` inside the transaction callback** instead of `manager`/`tx`, so work happens outside the transaction.
- **Read-modify-write without locking or atomic updates.**
- **Calling external services inside the transaction**, or publishing events before commit.
- **Forgetting to release** a manually created `QueryRunner` or end a Mongo session.
- **Swallowing exceptions** in the callback so a failed step still commits.
- **Long transactions** (and Prisma interactive timeouts).
- **Assuming a single process-wide transaction**: transactions are per connection; parallel requests are separate.
- **MongoDB transactions on a standalone server** (they need a replica set).
- **Wrapping everything in transactions "to be safe"**, hurting throughput for single-statement operations.

## Debugging

- Partial data after an error: some writes ran outside the transaction (wrong `manager`/`tx`/`session`).
- Requests hang: long-lived transaction or leaked connection holding locks; check for missing `release()` and look at the database's active/blocked queries (`pg_stat_activity` in PostgreSQL).
- Deadlock errors: ensure operations lock rows in a consistent order, shorten transactions, and add retries.
- "Transaction already closed" / timeout (Prisma): the callback ran too long or used a client after the transaction ended.
- Mongo "Transaction numbers are only allowed on a replica set member": run a single-node replica set locally.

## Quick Summary

- A transaction makes multiple writes atomic; throwing inside the callback rolls everything back.
- Use the transaction handle (`manager`/`tx`/`session`) for every operation inside; decide how repositories join it.
- Put the boundary in the service method; keep it short; no external calls inside; side effects after commit (outbox for reliability).
- Avoid read-modify-write races with atomic updates, row locks, or optimistic versioning.
- Isolation defaults are often enough; stricter levels need retries on serialization failures.

## Next

[Migrations →](./05-migrations.md)
