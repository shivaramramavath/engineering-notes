# 13 — Transactions

Grouping multiple database operations into a single all-or-nothing unit. Several earlier files (`06-crud-methods/04-delete-methods.md`, `10-middleware-hooks/03-common-hook-patterns.md`, `11-relationships/`) pointed here whenever partial failure across multiple writes would leave data in a genuinely bad state — this section covers how that's actually done.

## In this section

| File                              | Covers                                                                                                       |
| --------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `01-sessions-and-transactions.md` | Sessions, `startTransaction`/`commitTransaction`/`abortTransaction`, and Mongoose's `withTransaction` helper |
| `02-transaction-patterns.md`      | Practical patterns: money transfers, creating related documents together, and retrying on transient errors   |

## What a transaction actually guarantees

```
Without a transaction:  write A ✅ → write B ❌ → data is now inconsistent (A happened, B didn't)
With a transaction:      write A ✅ → write B ❌ → BOTH are rolled back — as if neither ever happened
```

Classic examples: transferring money between two accounts (debit one, credit the other — both must succeed or neither should), creating an order along with its line items, deleting a parent along with all its dependents.

## An important prerequisite

Transactions require MongoDB to be running as a **replica set** (or a sharded cluster) — they don't work on a standalone `mongod` instance. This catches many people off guard when they first try transactions against a plain local install:

- **MongoDB Atlas** — every cluster (including the free tier) is already a replica set; transactions work out of the box
- **A local install** — needs to be started as a single-node replica set (`mongod --replSet rs0`, then `rs.initiate()`)
- **A local Docker container** — same requirement; a plain `docker run mongo` is a standalone instance and will reject transaction attempts

## What you should be able to do after this section

- Start a session and wrap multiple operations in a transaction, committing or aborting correctly
- Use Mongoose's `withTransaction` helper to avoid manually managing commit/abort/end
- Apply the transaction pattern to realistic scenarios like money transfers and multi-document creation
- Understand transient transaction errors and how to retry them

## Next

**`14-performance`** covers making everything in this guide fast — indexes, query optimization, `.lean()`, pagination, and connection pooling.
