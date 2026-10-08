# Transactions

MongoDB guarantees that a write to a **single document** (including all its embedded data) is atomic. Many operations never need a transaction if your data model keeps related data together. When a business operation must change **several documents (or collections) atomically**, MongoDB supports **multi-document transactions**, but only on a replica set or sharded cluster, and with real costs.

General concepts (ACID, keeping transactions short, side effects after commit) are in [Database foundations: transactions](../02-database-foundations/04-transactions.md). This note covers the Mongoose mechanics.

Prerequisites: [Setup](./01-setup.md) (replica set), [repositories](./03-repositories.md), [populate](./04-populate.md) (embed vs reference).

## First: can you avoid a transaction?

| Situation | Alternative |
|-----------|-------------|
| Update several fields of one entity | One `updateOne` with `$set`: already atomic |
| Counter / balance changes | `$inc` (atomic) with a guard in the filter |
| Order and its line items | **Embed** the items in the order document: one atomic write |
| "Create if not exists" | A unique index + `upsert`, handling duplicate-key errors |
| Update one document based on its current state | Conditional update: `updateOne({ _id, status: 'pending' }, { $set: { status: 'paid' } })`, then check `matchedCount` |

```ts
// atomic, guarded debit: no transaction needed for a single account
const res = await this.accountModel.updateOne(
  { _id: id, balance: { $gte: amount } },
  { $inc: { balance: -amount } },
);
if (res.matchedCount === 0) throw new BadRequestException('Insufficient funds');
```

Transactions are for cases like **transferring between two account documents** or **creating documents in two collections that must succeed together**. If most of your operations need them, reconsider whether MongoDB's document model (or a relational database) fits your data ([architecture](../02-database-foundations/01-database-architecture.md)).

## Requirements

- A **replica set** or sharded cluster. A standalone `mongod` rejects transactions with *"Transaction numbers are only allowed on a replica set member or mongos"*. For local development run a single-node replica set ([setup](./01-setup.md)).
- A recent MongoDB version and driver (check the MongoDB docs for transaction feature support in your deployment; sharded transactions have extra constraints).
- Collections should already exist before you write to them in a transaction in some situations (collection creation inside transactions has version-specific rules), so create collections and indexes up front ([indexes](./06-indexes.md)).

## The Mongoose API

Use the **connection** (inject it with `@InjectConnection()`).

### `connection.transaction()` (recommended helper)

```ts
@Injectable()
export class TransfersService {
  constructor(
    @InjectConnection() private readonly connection: Connection,
    @InjectModel(Account.name) private readonly accounts: Model<Account>,
    @InjectModel(Transfer.name) private readonly transfers: Model<Transfer>,
  ) {}

  async transfer(fromId: string, toId: string, amount: number) {
    return this.connection.transaction(async (session) => {
      const debit = await this.accounts.updateOne(
        { _id: fromId, balance: { $gte: amount } },
        { $inc: { balance: -amount } },
        { session },
      );
      if (debit.matchedCount === 0) throw new BadRequestException('Insufficient funds');

      await this.accounts.updateOne({ _id: toId }, { $inc: { balance: amount } }, { session });
      const [record] = await this.transfers.create([{ fromId, toId, amount }], { session });
      return record;
    });
  }
}
```

`connection.transaction(fn)` starts a session, runs your callback in a transaction, **commits** on success, **aborts** (rolls back) if the callback throws, ends the session, and returns the callback's result. It's built on the driver's `withTransaction`, which also **retries** the callback on transient errors.

### Manual session control

```ts
const session = await this.connection.startSession();
try {
  await session.withTransaction(async () => {
    // operations with { session }
  });
} finally {
  await session.endSession();            // always end the session
}
```

You can also call `session.startTransaction()`, `commitTransaction()`, and `abortTransaction()` yourself for full control, but then **you** handle retries and cleanup. Prefer the helper unless you need that control.

## Passing the session (the part everyone forgets)

A transaction only includes operations that carry its **session**. An operation without it runs **outside** the transaction, and won't roll back.

```ts
Model.create([doc], { session })                          // array form is REQUIRED when passing options with create
Model.insertMany(docs, { session })
Model.updateOne(filter, update, { session })
Model.findOneAndUpdate(filter, update, { session, returnDocument: 'after' })
Model.deleteOne(filter, { session })
Model.find(filter).session(session)                       // queries: chain .session()
Model.aggregate(pipeline).session(session)
doc.save({ session })                                     // documents loaded with .session(session) remember it
Model.countDocuments(filter).session(session)
```

Reads must use the session too, to see the transaction's own uncommitted writes and a consistent snapshot. Mongoose documents loaded within a session are bound to it; saving them uses that session.

## Passing the session through your repositories

Just as with other ORMs, repositories need to accept the transaction context ([repository pattern](../02-database-foundations/03-repository-pattern.md)):

```ts
@Injectable()
export class AccountsRepository {
  constructor(@InjectModel(Account.name) private readonly model: Model<Account>) {}

  debit(id: string, amount: number, session?: ClientSession) {
    return this.model.updateOne(
      { _id: id, balance: { $gte: amount } },
      { $inc: { balance: -amount } },
      { session },
    );
  }
}

await this.connection.transaction(async (session) => {
  await this.accounts.debit(fromId, amount, session);
  await this.accounts.credit(toId, amount, session);
});
```

Passing `session` as an explicit optional parameter is simple and visible. Ambient (`AsyncLocalStorage`-style) context libraries exist but add implicit behavior; evaluate them carefully ([scopes and request context](../../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md)).

## The callback may run more than once

`withTransaction`/`connection.transaction` **re-run the callback** when MongoDB reports a transient transaction error (for example a write conflict or a brief election), and may retry the commit as well. So:

- The callback must be **idempotent with respect to anything outside the transaction**. No emails, HTTP calls, or message publishing inside it: they could happen several times, and can't be rolled back.
- Don't mutate outer variables in a way that breaks on re-execution; compute from the data you read inside.
- Do side effects **after** the transaction resolves, or use the [outbox pattern](../../08-architecture-and-patterns/04-real-world-patterns/03-transactional-outbox.md): write an event document inside the transaction and publish it afterwards.

## Concerns, isolation, and limits

```ts
await this.connection.transaction(
  async (session) => { /* ... */ },
  {
    readConcern: { level: 'snapshot' },
    writeConcern: { w: 'majority' },
    // readPreference: 'primary' is required inside transactions
  },
);
```

- Transactions provide **snapshot isolation** semantics when using `snapshot` read concern; `writeConcern: majority` makes commits durable across the replica set. Check the MongoDB docs for defaults and what combinations are valid.
- **Write conflicts** abort immediately: if two transactions modify the same document, one fails with a transient error and is retried by the helper. High contention on hot documents means retries and wasted work.
- **Time limit:** transactions that run too long are aborted (the server default is about 60 seconds). Keep them to short, bounded work.
- **Size/ops limits and oplog pressure:** huge transactions are expensive. Don't wrap bulk imports in one transaction; batch them.
- **Performance:** transactions are slower than single-document operations and add load. Use them where needed, not by default.
- Inside transactions, reads must go to the **primary**, so read-preference tuning doesn't apply.

## Optimistic concurrency without a transaction

For "someone else changed this while I was editing", Mongoose can enforce versioning on `save()`:

```ts
@Schema({ optimisticConcurrency: true })
export class Document { /* ... */ }

const doc = await model.findById(id);
doc.content = 'new';
await doc.save();      // throws VersionError if another save changed the document since you loaded it
```

(It uses the `__v` version key; check the docs for which updates it guards.) Alternatively use conditional updates (`updateOne({ _id, version }, { $inc: { version: 1 }, ... })`) and check `matchedCount`. Map conflicts to `409` ([database errors](../02-database-foundations/07-database-errors.md)).

## Errors and retries

Transient transaction errors carry labels such as **`TransientTransactionError`** (retry the whole transaction) and **`UnknownTransactionCommitResult`** (retry the commit). The `withTransaction`/`connection.transaction` helpers handle these for you. If you manage sessions manually, you must check `err.hasErrorLabel(...)` and retry. Non-transient errors (validation errors, duplicate keys) abort the transaction and propagate; don't retry them.

## Testing

- Use a real MongoDB **replica set** in tests: Docker with `--replSet`, or an in-memory replica-set helper such as `mongodb-memory-server`'s replica set mode. A standalone in-memory instance won't support transactions.
- Verify **rollback**: make a middle step throw and assert the earlier writes aren't visible ([integration testing](../01-testing/05-integration-testing.md)).

```ts
await expect(service.transfer(a, b, 1_000_000)).rejects.toThrow();
expect((await accounts.findById(a).lean())!.balance).toBe(initialBalance);
```

Mocked models can't prove atomicity.

## Common mistakes

- **Running transactions against a standalone MongoDB** (no replica set).
- **Forgetting `{ session }`** on some operations, so they run outside the transaction.
- **`Model.create(doc, { session })`** (object form): with options, `create` needs the **array form** `create([doc], { session })`.
- **Side effects inside the callback** that may execute multiple times.
- **Using transactions to paper over a data model** that should embed related data.
- **Long or huge transactions**, hitting time limits and slowing the cluster.
- **Reading outside the session**, missing uncommitted writes or seeing inconsistent data.
- **Not ending sessions** when using manual control (leaks resources).
- **Retrying non-transient errors**, or hand-rolling retries when the helper already retries.
- **Hot-document contention** causing repeated write-conflict retries.

## Debugging

- `Transaction numbers are only allowed on a replica set member or mongos`: you're connected to a standalone server; use a replica set and `?replicaSet=rs0` in the URI.
- Partial data after a failure: some operation lacked the session; audit every call in the callback and in repositories it calls.
- `WriteConflict` / `TransientTransactionError` appearing often: contention on the same documents; shorten the transaction, redesign hot spots, or use atomic single-document operators.
- "Transaction ... has been aborted" / timeout: the transaction ran too long; reduce the work.
- Enable `mongoose.set('debug', true)` in development to see operations and sessions.

## Quick Summary

- Single-document writes are atomic; embed related data and use atomic operators (`$inc`, conditional `updateOne`) so you often don't need transactions.
- Multi-document transactions need a **replica set** (even locally) and carry performance and contention costs.
- Use `connection.transaction(async (session) => ...)` (or `session.withTransaction`) and pass `{ session }` to **every** operation; `create` needs the array form with options.
- The callback can re-run: no external side effects inside; do them after commit (outbox for reliability).
- Keep transactions short, handle transient errors via the helper, and verify rollback in integration tests against a real replica set.

## Next

Section complete. Continue with [API design](../08-api-design/README.md), or revisit [Database foundations](../02-database-foundations/README.md) and the [ORM comparison](../02-database-foundations/08-orm-comparison.md).

← Back to [Mongoose overview](./README.md)
