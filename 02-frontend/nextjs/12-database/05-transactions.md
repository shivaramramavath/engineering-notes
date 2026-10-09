# Transactions

A transaction groups several database operations into one unit: all of them take effect, or none do. They protect data from the half-finished states that happen when a request fails midway or two users act at the same time.

> Verified against the Prisma and Drizzle transaction docs and the Mongoose transactions guide. SQL isolation behavior and the MongoDB replica-set requirement are general database knowledge; confirm for your database version.

## What it is

ACID in short:

| Property | Meaning |
|---|---|
| **Atomicity** | All statements apply, or none |
| **Consistency** | Constraints hold before and after |
| **Isolation** | Concurrent transactions do not see each other's half-done work (to a degree set by the isolation level) |
| **Durability** | Once committed, it survives a crash |

## When to use one

| Situation | Transaction? |
|---|---|
| Move money: debit one account, credit another | **Yes** |
| Create an order and its line items and decrement stock | **Yes** |
| Create a user and their default settings row | **Yes** (or a single nested write) |
| Read-modify-write on a counter or balance | **Yes**, or one atomic statement |
| One `INSERT` | No, a single statement is already atomic |
| Insert, then send an email | The insert is a transaction; the email is **not** in it ([below](#what-not-to-put-in-a-transaction)) |

Often you can avoid a transaction with **one atomic statement**:

```sql
UPDATE accounts SET balance = balance - 100 WHERE id = $1 AND balance >= 100;
-- check rows affected: 0 means insufficient funds, no race possible
```

## Plain SQL with `pg`

```ts
import "server-only";
import { pool } from "@/db";

export async function transfer(from: number, to: number, amount: number) {
  const client = await pool.connect();          // one connection for the whole transaction
  try {
    await client.query("BEGIN");

    const debit = await client.query(
      "UPDATE accounts SET balance = balance - $1 WHERE id = $2 AND balance >= $1",
      [amount, from],
    );
    if (debit.rowCount === 0) throw new Error("Insufficient funds");

    await client.query("UPDATE accounts SET balance = balance + $1 WHERE id = $2", [amount, to]);

    await client.query("COMMIT");
  } catch (e) {
    await client.query("ROLLBACK");
    throw e;
  } finally {
    client.release();                           // always return it to the pool
  }
}
```

Never run `BEGIN` through `pool.query`: each call may use a different connection, so the statements would not be in the same transaction.

## Prisma

Three forms:

```ts
// 1. Nested writes: automatically transactional
await prisma.user.create({
  data: { email: "a@x.com", posts: { create: [{ title: "One" }, { title: "Two" }] } },
});

// 2. Sequential batch: independent operations, run in order, succeed or fail together
const [posts, total] = await prisma.$transaction([
  prisma.post.findMany({ where: { published: true } }),
  prisma.post.count(),
]);

// 3. Interactive: read, decide, write
const result = await prisma.$transaction(
  async (tx) => {
    const sender = await tx.account.update({
      where: { id: fromId },
      data: { balance: { decrement: amount } },
    });
    if (sender.balance < 0) throw new Error("Insufficient funds");   // throw => rollback
    return tx.account.update({
      where: { id: toId },
      data: { balance: { increment: amount } },
    });
  },
  {
    maxWait: 5000,                                   // time to get a connection (default 2000 ms)
    timeout: 10000,                                  // max transaction time (default 5000 ms)
    isolationLevel: Prisma.TransactionIsolationLevel.Serializable,
  },
);
```

Facts from the Prisma docs:

- Throwing inside the callback rolls back; returning commits.
- Use `tx`, not the outer `prisma`, for every query inside, or it runs outside the transaction.
- **Always `await` queries inside** (the docs' own example has a missing `await` on the recipient update, which would let the transaction commit before it finishes).
- `Promise.all` inside an interactive transaction runs queries **serially**, because they share one connection.
- Defaults come from the database: PostgreSQL and SQL Server `ReadCommitted`, MySQL `RepeatableRead`, CockroachDB and SQLite `Serializable`.
- Under `Serializable`, write conflicts or deadlocks raise **`P2034`**; retry in application code.
- `updateMany` and `deleteMany` do not support nested writes.
- Isolation levels are not supported on MongoDB.

## Drizzle

```ts
import { and, eq, gte, sql } from "drizzle-orm";

const newBalance = await db.transaction(async (tx) => {
  const debited = await tx
    .update(accounts)
    .set({ balance: sql`${accounts.balance} - ${amount}` })
    .where(and(eq(accounts.id, fromId), gte(accounts.balance, amount)))
    .returning({ id: accounts.id });

  if (debited.length === 0) tx.rollback();             // throws, rolls back

  await tx.update(accounts)
    .set({ balance: sql`${accounts.balance} + ${amount}` })
    .where(eq(accounts.id, toId));

  const [row] = await tx.select({ balance: accounts.balance }).from(accounts).where(eq(accounts.id, fromId));
  return row.balance;                                    // the callback's return value is the result
});
```

From the Drizzle docs:

- `tx.rollback()` **throws**, which rolls the transaction back; an ordinary thrown error also rolls back.
- **Nested transactions** (`tx.transaction(async (tx2) => ...)`) create **savepoints**.
- Relational queries work as `tx.query.users.findMany(...)`.
- PostgreSQL options as the second argument:

```ts
await db.transaction(async (tx) => { /* ... */ }, {
  isolationLevel: "serializable",   // "read uncommitted" | "read committed" | "repeatable read" | "serializable"
  accessMode: "read write",         // or "read only"
  deferrable: true,
});
```

## Mongoose

MongoDB multi-document transactions require a **replica set or sharded cluster** (Atlas clusters qualify; a plain standalone local `mongod` does not). The Mongoose page does not state this; it comes from MongoDB's documentation. For local development run a single-node replica set (for example `mongod --replSet rs0` and `rs.initiate()`, or a Docker image configured that way).

```ts
import mongoose from "mongoose";
import dbConnect from "@/lib/mongodb";

await dbConnect();

await mongoose.connection.transaction(async (session) => {
  const from = await Account.findOneAndUpdate(
    { _id: fromId, balance: { $gte: amount } },
    { $inc: { balance: -amount } },
    { session, new: true },
  );
  if (!from) throw new Error("Insufficient funds");

  await Account.updateOne({ _id: toId }, { $inc: { balance: amount } }, { session });
  await Ledger.create([{ fromId, toId, amount }], { session });   // create() takes an array with a session
});
```

From the Mongoose docs:

- `connection.transaction()` wraps `session.withTransaction()`: it commits on success, aborts on a thrown error, and **retries on transient transaction errors**. It also resets document change-tracking state after an abort so a later `save()` sends the changes again.
- **Every operation must receive the session** (`{ session }` or `.session(session)`), or it runs outside the transaction. Documents loaded with a session keep it for later `save()` calls.
- `Model.create([...], { session })` uses the **array form** with a session.
- **No parallel operations inside a transaction** (`Promise.all` is not supported).
- **No nested transactions** on the same session: `withTransaction()` inside a transaction throws `Transaction already in progress`.
- Mongoose 8.4+: `mongoose.set("transactionAsyncLocalStorage", true)` applies the session to every operation inside a `connection.transaction()` callback automatically.
- For manual control: `session.startTransaction()`, `commitTransaction()` or `abortTransaction()`, then `session.endSession()`. The docs do not describe retries for this manual path.
- A single-document write is already atomic in MongoDB; design documents so that data that must change together lives in one document, and reach for transactions only when you cannot.

## Isolation levels (PostgreSQL view)

| Level | Prevents | Notes |
|---|---|---|
| Read committed (default) | Dirty reads | Each statement sees data committed before it started; lost updates and non-repeatable reads still possible |
| Repeatable read | Plus non-repeatable reads | Snapshot for the transaction; may fail with serialization errors |
| Serializable | Plus phantoms and write skew | Behaves as if transactions ran one at a time; **retry on `40001`** |

Higher isolation costs throughput and produces retryable failures. Pick the lowest level that makes your invariant safe, and prefer explicit locking or atomic updates for hot rows.

### Locking a row

```sql
BEGIN;
SELECT balance FROM accounts WHERE id = $1 FOR UPDATE;   -- others wait for this row
-- compute, then:
UPDATE accounts SET balance = $2 WHERE id = $1;
COMMIT;
```

Take locks in a **consistent order** (always lower account ID first) to avoid deadlocks.

## Retries

Some failures are expected and safe to retry **if the whole transaction is re-run from the start**:

| Database | Retryable signal |
|---|---|
| PostgreSQL | SQLSTATE `40001` (serialization failure), `40P01` (deadlock) |
| Prisma | `P2034` |
| MongoDB | `TransientTransactionError` label (handled by `withTransaction`) |

```ts
async function withRetry<T>(fn: () => Promise<T>, attempts = 3): Promise<T> {
  for (let i = 0; ; i++) {
    try {
      return await fn();
    } catch (e: any) {
      const retryable = e?.code === "40001" || e?.code === "40P01" || e?.code === "P2034";
      if (!retryable || i >= attempts - 1) throw e;
      await new Promise((r) => setTimeout(r, 50 * 2 ** i + Math.random() * 25));   // backoff + jitter
    }
  }
}
```

Retries require **idempotent** transaction bodies: no side effects outside the database inside the callback.

## Optimistic concurrency

Instead of locking, detect conflicting writers with a version number:

```ts
const updated = await prisma.seat.updateMany({
  where: { id: seat.id, version: seat.version },          // only if nobody changed it since I read it
  data: { claimedBy: userEmail, version: { increment: 1 } },
});
if (updated.count === 0) return { message: "That seat was just taken. Try again." };
```

Works in any database (`UPDATE ... WHERE id = $1 AND version = $2`), needs no long transaction, and fits user-facing conflicts. Prisma's docs show this pattern.

## Idempotency

Design write operations so running them twice gives the same result: unique constraints (for example on a payment's external ID), `INSERT ... ON CONFLICT DO NOTHING`, and checking existing state before creating. This makes retries and double-submitted forms safe.

## What not to put in a transaction

| Do not | Why | Instead |
|---|---|---|
| HTTP calls (payment API, email) | Holds locks and a connection while waiting on the network; cannot be rolled back | Commit first, then call; or record intent in an "outbox" table and process it afterwards |
| Slow queries or user input | Long transactions block others and risk timeouts and deadlocks | Prepare data first; keep the transaction to the writes |
| `revalidatePath` / `updateTag` | Revalidating before commit can re-read old data | Revalidate **after** the transaction returns |
| Reads that do not need the transaction | Longer lock time | Read before |

Keep transactions **short**: Prisma documents that long ones hurt performance and can deadlock, and gives interactive transactions a 5 second default timeout.

## In a Server Action

```ts
"use server";
import { revalidatePath } from "next/cache";
import { verifySession } from "@/app/lib/dal";
import { prisma } from "@/db/prisma";

export async function placeOrder(cartId: number) {
  const { userId } = await verifySession();             // authenticate first

  const order = await prisma.$transaction(async (tx) => {
    const items = await tx.cartItem.findMany({ where: { cartId, cart: { userId: Number(userId) } } });
    if (items.length === 0) throw new Error("Cart is empty");

    for (const item of items) {
      const { count } = await tx.product.updateMany({
        where: { id: item.productId, stock: { gte: item.qty } },
        data: { stock: { decrement: item.qty } },
      });
      if (count === 0) throw new Error("Out of stock");  // rolls everything back
    }

    const created = await tx.order.create({
      data: { userId: Number(userId), lines: { create: items.map((i) => ({ productId: i.productId, qty: i.qty })) } },
    });
    await tx.cartItem.deleteMany({ where: { cartId } });
    return created;
  });

  revalidatePath("/orders");                              // after commit
  return { orderId: order.id };                           // charge the card / send email after this point
}
```

Error handling: a thrown error inside the transaction becomes a rejection from the action. Catch it in the action and return `{ message }` for expected failures (out of stock), and rethrow unexpected ones. Keep `redirect()` outside any `try/catch` ([Action Errors](../07-server-actions/04-action-errors.md)).

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Statements not rolled back | Used the pool or outer client instead of `tx` / the transaction's connection | Use `tx` or the checked-out client for every statement |
| Pool exhausted, requests hang | `pool.connect()` without `release()` | `try/finally` |
| `Transaction already closed` / timed out (Prisma) | Slow work inside; exceeded `timeout` | Shorten, move slow work outside, raise `timeout` carefully |
| `P2034` / `40001` / `40P01` | Serialization conflict or deadlock | Retry the whole transaction; consistent lock order |
| Mongoose: "Transaction numbers are only allowed on a replica set member or mongos" | Standalone MongoDB | Run a replica set or use Atlas |
| Mongoose changes not rolled back | Operation missed the `session` | Pass `{ session }` everywhere or enable AsyncLocalStorage |
| Mongoose `Transaction already in progress` | Nested `withTransaction` | Flatten |
| Stock goes negative under load | Read-then-write race | Atomic conditional update, `FOR UPDATE`, or Serializable with retries |
| Cached page shows old data after commit | Revalidated too early or not at all | Revalidate after the transaction returns |
| Emails sent twice after a retry | Side effect inside the retried callback | Move it after the commit; make it idempotent |

## Common mistakes

| Mistake | Fix |
|---|---|
| Transaction around one statement | Not needed |
| Queries on the outer client inside a transaction callback | Use `tx` |
| Missing `await` inside the callback | `await` everything |
| Network calls inside | Commit, then call |
| Retrying non-idempotent bodies | Keep bodies DB-only and idempotent |
| `Promise.all` in Prisma or Mongoose transactions | Run sequentially |
| High isolation everywhere | Lowest level that is safe |
| Locking rows in random order | Consistent order |
| Revalidating before commit | After |
| Relying on a transaction instead of constraints | Keep `UNIQUE` / `CHECK` / foreign keys too |

## Quick Summary

- A transaction makes several writes all-or-nothing; a single atomic statement is often simpler.
- Prisma: nested writes, `$transaction([])`, and interactive `$transaction(async tx => ...)` with `maxWait`, `timeout` and `isolationLevel`.
- Drizzle: `db.transaction(async tx => ...)`, `tx.rollback()`, savepoints via nested transactions.
- Mongoose: `connection.transaction()`, pass the session to every operation, a replica set is required, no parallel operations.
- Keep transactions short and database-only; retry serialization failures and deadlocks; revalidate after commit.

## Next

- Chapter 13 in the repo root [README](../README.md)
- [Server Action Errors](../07-server-actions/04-action-errors.md)
- [Mutations and Optimistic UI](../07-server-actions/03-mutations-and-optimistic-ui.md)

Sources: [Prisma transactions](https://www.prisma.io/docs/orm/prisma-client/queries/transactions), [Drizzle transactions](https://orm.drizzle.team/docs/transactions), [Mongoose transactions](https://mongoosejs.com/docs/transactions.html)
