# Sessions & Transactions

The mechanics of running multiple operations as one atomic unit in Mongoose — sessions, starting/committing/aborting a transaction, and the `withTransaction` helper that handles the boilerplate for you.

## Sessions: the container for a transaction

```js
const session = await mongoose.startSession();
```

A **session** is a context MongoDB uses to group related operations together. On its own, a session doesn't do anything transactional — it's the thing a transaction is _attached to_, and the thing every operation in that transaction must be explicitly associated with.

---

## The manual approach: start, commit, abort

```js
const session = await mongoose.startSession();
session.startTransaction();

try {
  await Account.updateOne(
    { _id: fromId },
    { $inc: { balance: -100 } },
    { session },
  );
  await Account.updateOne(
    { _id: toId },
    { $inc: { balance: 100 } },
    { session },
  );

  await session.commitTransaction();
} catch (err) {
  await session.abortTransaction();
  throw err;
} finally {
  session.endSession();
}
```

Step by step:

1. **`startSession()`** — creates the session
2. **`startTransaction()`** — begins the transaction within it
3. **Pass `{ session }` to every operation** that should be part of the transaction
4. **`commitTransaction()`** — if everything succeeded, makes all the changes permanent, atomically
5. **`abortTransaction()`** — if anything failed, undoes every change made within the transaction
6. **`endSession()`** — always, in `finally`, to release the session's resources

### The critical detail: `{ session }` must be passed to every operation

```js
await Account.updateOne(
  { _id: fromId },
  { $inc: { balance: -100 } },
  { session },
); // ✅ part of the transaction
await Account.updateOne({ _id: toId }, { $inc: { balance: 100 } }); // ❌ NOT part of it — no session passed
```

An operation that doesn't have `{ session }` passed to it runs **outside** the transaction entirely — it takes effect immediately and permanently, regardless of whether the transaction later commits or aborts. Forgetting to pass `{ session }` to even one operation is the single most common, most dangerous transaction bug, since everything still appears to work correctly until a failure occurs and the transaction's rollback silently fails to include the forgotten operation.

---

## Passing `session` to different kinds of operations

```js
// queries and updates — as an option
await User.updateOne(filter, update, { session });
await User.findOne(filter).session(session); // .session() on a query chain
await User.find(filter).session(session);

// creating documents
await User.create([{ name: "Alice" }], { session }); // create() with a session requires an ARRAY, even for one document
const user = new User({ name: "Alice" });
await user.save({ session }); // .save() takes it as an option too

// deleting
await User.deleteOne(filter, { session });

// aggregation
await User.aggregate(pipeline).session(session);
```

### A specific gotcha: `create()` needs an array when using a session

```js
await User.create({ name: "Alice" }, { session }); // ❌ doesn't work as expected — the second argument isn't treated as options
await User.create([{ name: "Alice" }], { session }); // ✅ passing an array makes the second argument the options object
```

`Model.create()` with a session specifically requires the documents to be passed as an array, even for a single document — otherwise the `{ session }` object gets misinterpreted as additional document data rather than as an options object.

---

## `withTransaction()` — the recommended helper

```js
const session = await mongoose.startSession();

try {
  await session.withTransaction(async () => {
    await Account.updateOne(
      { _id: fromId },
      { $inc: { balance: -100 } },
      { session },
    );
    await Account.updateOne(
      { _id: toId },
      { $inc: { balance: 100 } },
      { session },
    );
  });
} finally {
  session.endSession();
}
```

`withTransaction()` handles starting the transaction, committing on success, aborting on any thrown error, **and automatically retrying** the entire callback if MongoDB reports a transient transaction error (`02-transaction-patterns.md`) — all of which you'd otherwise have to implement manually. For most real use, this is the better default over the manual start/commit/abort approach shown above, since the automatic retry handling alone covers a real class of intermittent failures that a hand-rolled version would need to explicitly account for.

### Returning a value from `withTransaction`

```js
const result = await session.withTransaction(async () => {
  const user = await User.create([{ name: "Alice" }], { session });
  return user[0];
});
```

---

## Mongoose's own convenience wrapper: `connection.transaction()`

```js
await mongoose.connection.transaction(async (session) => {
  await Account.updateOne(
    { _id: fromId },
    { $inc: { balance: -100 } },
    { session },
  );
  await Account.updateOne(
    { _id: toId },
    { $inc: { balance: 100 } },
    { session },
  );
});
```

An even more concise Mongoose-specific helper — it creates the session, runs `withTransaction` on it, and ends the session automatically, passing the `session` into your callback. Functionally the cleanest option for straightforward cases, since it removes the manual session lifecycle management (`startSession()` / `endSession()`) entirely.

---

## What a transaction does NOT guarantee

### It doesn't make external side effects atomic

```js
await mongoose.connection.transaction(async (session) => {
  await Order.create([{ ... }], { session });
  await sendConfirmationEmail(customer);   // ❌ NOT rolled back if the transaction later aborts
});
```

A transaction only covers **database operations in that MongoDB deployment** — an email, an HTTP call to an external API, a write to a different system entirely won't be undone if the transaction aborts afterward. Side effects like these generally belong **after** a transaction successfully commits, not inside it.

### Long-running transactions have limits

MongoDB imposes a default time limit on transactions (60 seconds by default) — a transaction that runs longer than that is aborted automatically. Transactions are meant for short, targeted groups of related operations, not for long-running batch processing.

## Common mistakes

- **Forgetting to pass `{ session }` to one of the operations** — that operation silently runs outside the transaction and won't be rolled back on failure; the single most common and most dangerous transaction bug.
- **Calling `Model.create(doc, { session })` without wrapping `doc` in an array** — the session option gets misinterpreted as document data.
- **Forgetting `session.endSession()`** in a `finally` block — leaks session resources (`connection.transaction()` handles this for you automatically).
- **Doing non-database side effects (emails, external API calls) inside a transaction**, expecting them to roll back — they won't; do them after a successful commit instead.
- **Trying transactions against a standalone `mongod`** — they require a replica set; a plain standalone instance rejects them outright.

## Quick summary

- A session groups operations; a transaction runs within one, and **every** operation meant to be part of it must have `{ session }` passed explicitly
- `withTransaction()` handles commit/abort/retry automatically; `mongoose.connection.transaction()` additionally manages the session lifecycle for you — both are preferable to hand-rolled `startTransaction`/`commitTransaction`/`abortTransaction` for most cases
- `Model.create()` with a session needs an array of documents, even for one
- Transactions cover database operations only — external side effects (emails, API calls) aren't rolled back and belong after a successful commit
- Transactions require a replica set — Atlas provides this automatically; a plain local `mongod` does not

## Next

**`02-transaction-patterns.md`** covers realistic patterns: money transfers, creating related documents together, and handling transient errors with retries.
