# Transactions

Correctness under concurrency: ACID, isolation, MVCC and locking.

| Note | What it covers |
|---|---|
| [Transaction Fundamentals](./00-transaction-fundamentals.md) | ACID, BEGIN/COMMIT/ROLLBACK and savepoints. |
| [Isolation Levels](./01-isolation-levels.md) | Read anomalies and how each isolation level prevents them. |
| [MVCC](./02-mvcc.md) | Multi-version concurrency control in PostgreSQL and InnoDB. |
| [Locks](./03-locks.md) | Row, table and advisory locks, and diagnosing blocking. |
| [Deadlocks](./04-deadlocks.md) | How deadlocks happen, how to detect and prevent them, and retrying. |
| [Locking Strategies](./05-locking-strategies.md) | Optimistic vs pessimistic locking, SELECT FOR UPDATE and SKIP LOCKED. |
| [Concurrency Patterns](./06-concurrency-patterns.md) | Job queues, double-booking prevention, counters and idempotency. |


Next folder: [Views and Routines](../15-views-and-routines/README.md)
