# Transactions

Prisma gives you several levels of transactional behavior, from automatic (nested writes) to explicit (`$transaction` in two forms). The general theory (ACID, isolation levels, keeping transactions short, outbox) is in [Database foundations: transactions](../02-database-foundations/04-transactions.md); this note covers the Prisma mechanics and its limits.

Prerequisites: [Prisma Client](./03-prisma-client.md), [relations](./04-relations.md), [repositories](./05-repositories.md).

## Level 0: operations that are already atomic

You often don't need an explicit transaction:

- **A single query** (`create`, `update`, `delete`, `updateMany`, `createMany`) is atomic.
- **Nested writes** are atomic: creating a parent with children in one `create` either writes everything or nothing ([relations](./04-relations.md)).
- **Atomic number operations** (`{ increment: n }`) run in the database ([Prisma Client](./03-prisma-client.md)).

```ts
// one atomic operation: order + items (no $transaction needed)
await prisma.order.create({
  data: {
    userId,
    total,
    items: { create: lines.map((l) => ({ productId: l.productId, quantity: l.quantity })) },
  },
});
```

Reach for `$transaction` only when you need **several separate operations** to succeed together.

## Level 1: batch transactions (`$transaction([...])`)

Pass an **array of Prisma operations**; they run sequentially in one transaction and all roll back if any fails.

```ts
const [page, total] = await prisma.$transaction([
  prisma.post.findMany({ where, take: 20, skip: 0 }),
  prisma.post.count({ where }),
]);

await prisma.$transaction([
  prisma.account.update({ where: { id: fromId }, data: { balance: { decrement: amount } } }),
  prisma.account.update({ where: { id: toId }, data: { balance: { increment: amount } } }),
]);
```

Characteristics:

- Operations are **not awaited individually**: you pass the un-awaited promises (the operations), Prisma runs them.
- You **can't use the result of one step in the next**, and you can't put application logic between them.
- Good for independent writes and for "page + count" style reads.
- Returns an array of results in the same order.

## Level 2: interactive transactions (`$transaction(async (tx) => ...)`)

When logic depends on earlier results, use the callback form.

```ts
await this.prisma.$transaction(async (tx) => {
  const from = await tx.account.findUniqueOrThrow({ where: { id: fromId } });
  if (from.balance < amount) {
    throw new BadRequestException('Insufficient funds');        // rolls back everything
  }

  await tx.account.update({ where: { id: fromId }, data: { balance: { decrement: amount } } });
  await tx.account.update({ where: { id: toId }, data: { balance: { increment: amount } } });
});
```

Rules:

- **Use `tx`, not `prisma`, for every query inside.** Calls on `prisma` run on a different connection, **outside** the transaction.
- The transaction **commits** when the callback resolves and **rolls back** if it throws (or rejects).
- The callback's return value is returned from `$transaction`.
- `tx` is a client without `$transaction`, `$connect`, and similar methods: you can't nest `$transaction` on it.

### Options: timeout, wait, isolation

```ts
await prisma.$transaction(async (tx) => { /* ... */ }, {
  maxWait: 2000,                                            // ms to wait for a connection from the pool
  timeout: 5000,                                            // ms the whole transaction may run
  isolationLevel: Prisma.TransactionIsolationLevel.Serializable,
});
```

- Interactive transactions have a **time limit** (the documented defaults are a couple of seconds to acquire a connection and a few seconds to run; check your version). After the timeout the transaction is rolled back and further queries on `tx` fail with an error like "Transaction already closed". Keep them short rather than raising the timeout as a first resort.
- `maxWait` covers pool contention: if the pool is exhausted, the transaction can't even start ([connection pool sizing](../02-database-foundations/02-database-connection.md)).
- Isolation levels: `ReadUncommitted`, `ReadCommitted`, `RepeatableRead`, `Snapshot`, `Serializable` (support varies by database). Stricter levels can cause conflicts you must retry ([foundations](../02-database-foundations/04-transactions.md)).

## Joining repositories to a transaction

Pass `tx` (typed `Prisma.TransactionClient`) into repository methods, as shown in [repositories](./05-repositories.md):

```ts
await this.prisma.$transaction(async (tx) => {
  await this.accounts.debit(fromId, amount, tx);
  await this.accounts.credit(toId, amount, tx);
});
```

Put the **transaction boundary in the service method** that represents the business operation. Alternatives like ambient (`AsyncLocalStorage`-based) transaction context exist as community libraries; they hide the `tx` parameter but add implicit behavior and edge cases (nesting, propagation), so evaluate them carefully. See also [scopes and request context](../../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

## Preventing lost updates

A transaction doesn't by itself stop two requests from reading the same row and overwriting each other. In order of preference:

### 1. Atomic update with a guard

```ts
const res = await prisma.account.updateMany({
  where: { id, balance: { gte: amount } },
  data: { balance: { decrement: amount } },
});
if (res.count === 0) throw new BadRequestException('Insufficient funds');
```

One statement, no race, no locks. Use `updateMany` here so a non-match returns `count: 0` instead of throwing.

### 2. Optimistic concurrency with a version field

```prisma
model Document {
  id      String @id
  content String
  version Int    @default(0)
}
```

```ts
const res = await prisma.document.updateMany({
  where: { id, version: expectedVersion },
  data: { content, version: { increment: 1 } },
});
if (res.count === 0) throw new ConflictException('Document was modified by someone else');
```

The client sends the version it read (for example, via `If-Match`); a mismatch means someone else wrote first. Prisma doesn't maintain version columns for you, so you manage the field. Suits low-contention updates.

### 3. Pessimistic locking (`SELECT ... FOR UPDATE`)

Prisma's query API has no first-class row-lock option. Inside an interactive transaction you can use raw SQL:

```ts
await prisma.$transaction(async (tx) => {
  const [acc] = await tx.$queryRaw<{ id: string; balance: number }[]>`
    SELECT id, balance FROM accounts WHERE id = ${id} FOR UPDATE
  `;
  // ...decide, then update via tx
});
```

Use the tagged-template form (parameterized), keep the lock short, and lock rows in a consistent order across code paths to avoid deadlocks. Check the Prisma docs to see whether newer versions add native locking support.

## Failures and retries

Under contention the database aborts some transactions (deadlocks, serialization failures). Prisma surfaces write conflicts and deadlocks as `PrismaClientKnownRequestError` with code **`P2034`**. Retry the **whole** `$transaction` a few times with backoff, only if it's safe to repeat:

```ts
async function withRetry<T>(fn: () => Promise<T>, attempts = 3): Promise<T> {
  for (let i = 1; ; i++) {
    try {
      return await fn();
    } catch (err) {
      const retryable = err instanceof Prisma.PrismaClientKnownRequestError && err.code === 'P2034';
      if (!retryable || i >= attempts) throw err;
      await new Promise((r) => setTimeout(r, 50 * 2 ** i + Math.random() * 50));
    }
  }
}
```

Never retry constraint violations (`P2002`, `P2003`); they fail identically each time ([database errors](../02-database-foundations/07-database-errors.md)).

## Side effects and boundaries

- **Database work only inside the transaction.** No HTTP calls, emails, or queue publishes: they hold a connection and locks, and can't be rolled back.
- Do side effects **after** `$transaction` resolves. For a reliable "write + publish", store an event row inside the transaction and publish it afterwards ([transactional outbox](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md)).
- Don't hold a transaction open across requests or while waiting on users.

## Interaction with connection pools

An interactive transaction pins **one connection** until it finishes. Many concurrent long transactions exhaust the pool, and then every other query (and new transactions, via `maxWait`) waits. Size the pool for your concurrency, keep transactions short, and use a pooler for many instances ([setup](./01-setup.md)).

## MongoDB

Transactions need a replica set (or sharded cluster), even locally. Batch and interactive forms work the same way from the API side; check Prisma's MongoDB documentation for limitations.

## Testing

Verify rollback in [integration tests](../01-testing/05-integration-testing.md): make a middle step throw and assert that earlier writes are absent.

```ts
await expect(service.transfer(a, b, tooMuch)).rejects.toThrow();
const acc = await prisma.account.findUniqueOrThrow({ where: { id: a } });
expect(acc.balance).toBe(initialBalance);
```

Mocking `PrismaService` can't prove atomicity.

## Common mistakes

- **Using `prisma` instead of `tx`** inside an interactive transaction.
- **Awaiting operations before passing them** to the batch form (`$transaction([await ..., await ...])`), which runs them outside the transaction.
- **Using batch `$transaction`** when later steps depend on earlier results.
- **Long interactive transactions** that hit the timeout ("Transaction already closed").
- **Read-modify-write without a guard**, so concurrent requests lose updates.
- **External calls inside the transaction**, or publishing events before commit.
- **Retrying non-transient errors**, or retrying non-idempotent work.
- **Forgetting that `tx` can't open nested `$transaction`s.**
- **Wrapping single operations** or nested writes in transactions unnecessarily.
- **Pool exhaustion** from many concurrent interactive transactions.

## Debugging

- Partial data after a failure: some query used `prisma` rather than `tx`, or ran in the batch form un-awaited incorrectly.
- "Transaction already closed" / timeout: the callback ran too long, or used `tx` after the transaction ended (a forgotten `await` that let the callback return early).
- "Unable to start a transaction in the given time": `maxWait` exceeded; the pool is saturated.
- `P2034`: write conflict or deadlock; retry, shorten the transaction, standardize lock order.
- Enable query logging to see `BEGIN`, statements, and `COMMIT`/`ROLLBACK` ([setup](./01-setup.md)).

## Quick Summary

- Single operations, nested writes, and atomic increments are already atomic; add `$transaction` for multiple separate operations.
- Batch form (`$transaction([...])`) for independent operations; interactive form (`async (tx) => ...`) when logic depends on results; **always use `tx` inside**.
- Interactive transactions have timeouts and pin a connection: keep them short and database-only; configure `maxWait`/`timeout`/`isolationLevel` deliberately.
- Prevent lost updates with guarded atomic updates, optimistic version fields, or raw `FOR UPDATE` in an interactive transaction.
- Retry `P2034` for the whole transaction with backoff; do side effects after commit; verify rollback in integration tests.

## Next

Section complete. Continue with [Mongoose](../05-mongoose/README.md), or revisit the [ORM comparison](../02-database-foundations/08-orm-comparison.md).

← Back to [Prisma overview](./README.md)
