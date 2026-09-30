# Transaction Patterns

Realistic, practical patterns built on the mechanics from `01-sessions-and-transactions.md` — money transfers, creating related documents together, deleting a parent with its dependents, and retrying transient errors.

## Pattern 1: Transferring between two accounts

The canonical transaction example — both the debit and the credit must succeed, or neither should.

```js
async function transferFunds(fromId, toId, amount) {
  await mongoose.connection.transaction(async (session) => {
    const from = await Account.findById(fromId).session(session);
    const to = await Account.findById(toId).session(session);

    if (!from || !to) {
      throw new Error("Account not found");
    }
    if (from.balance < amount) {
      throw new Error("Insufficient funds");
    }

    from.balance -= amount;
    to.balance += amount;

    await from.save({ session });
    await to.save({ session });
  });
}
```

Notice the reads (`findById`) also take `.session(session)` — reading within the transaction ensures you see a consistent view of the data, and matters specifically because another concurrent transaction could otherwise change a balance between your read and your write. Throwing anywhere inside the callback (the "Insufficient funds" check, for example) automatically aborts the whole transaction — neither account is modified.

### A more concurrency-safe version using atomic operators

```js
async function transferFunds(fromId, toId, amount) {
  await mongoose.connection.transaction(async (session) => {
    const debited = await Account.findOneAndUpdate(
      { _id: fromId, balance: { $gte: amount } }, // the balance check is part of the filter itself
      { $inc: { balance: -amount } },
      { session, new: true },
    );

    if (!debited) {
      throw new Error("Insufficient funds or account not found");
    }

    await Account.updateOne(
      { _id: toId },
      { $inc: { balance: amount } },
      { session },
    );
  });
}
```

This version folds the balance check into the update's filter (`balance: { $gte: amount }`), so the "check then debit" happens as **one atomic operation** rather than a separate read followed by a write — safer under high concurrency, since there's no window between checking the balance and deducting from it for another transaction to sneak in.

---

## Pattern 2: Creating related documents together

```js
async function createOrder(customerId, items) {
  return mongoose.connection.transaction(async (session) => {
    const [order] = await Order.create(
      [{ customer: customerId, status: "pending" }],
      { session },
    );

    await OrderItem.create(
      items.map((item) => ({ order: order._id, ...item })),
      { session },
    );

    for (const item of items) {
      const updated = await Product.findOneAndUpdate(
        { _id: item.productId, stock: { $gte: item.quantity } },
        { $inc: { stock: -item.quantity } },
        { session },
      );
      if (!updated) {
        throw new Error(`Insufficient stock for product ${item.productId}`);
      }
    }

    return order;
  });
}
```

Creating the order, creating each line item, and decrementing inventory — all three must happen together or not at all. If the stock check fails partway through the loop, the order and its items (already created earlier in the same transaction) are automatically rolled back too — exactly the guarantee that would be impossible to provide correctly with separate, independent writes.

---

## Pattern 3: Deleting a parent with all its dependents

```js
async function deleteUserAndData(userId) {
  await mongoose.connection.transaction(async (session) => {
    await Comment.deleteMany({ author: userId }, { session });
    await Post.deleteMany({ author: userId }, { session });
    await User.deleteOne({ _id: userId }, { session });
  });
}
```

This is the explicit-transaction alternative to the hook-based cascading delete from `10-middleware-hooks/03-common-hook-patterns.md` — and, as that file noted, generally the more reliable option, since a hook-based cascade can leave data partially cleaned up if one step fails, whereas a transaction guarantees all-or-nothing.

---

## Handling transient transaction errors: retrying

```js
try {
  await mongoose.connection.transaction(async (session) => {
    // ...
  });
} catch (err) {
  if (err.hasErrorLabel?.("TransientTransactionError")) {
    // MongoDB is saying "this failed for a temporary reason (e.g. a write conflict with a concurrent
    // transaction) — retrying the WHOLE transaction has a good chance of succeeding"
  }
  throw err;
}
```

Transactions can fail for **transient** reasons that have nothing to do with your code being wrong — most commonly a **write conflict**, where two concurrent transactions try to modify the same document at once, and MongoDB aborts one of them. `withTransaction()`/`connection.transaction()` (`01-sessions-and-transactions.md`) automatically retry the callback when they see a `TransientTransactionError` label — one of the main reasons they're preferable to a hand-rolled manual `startTransaction`/`commitTransaction`/`abortTransaction`, which would need to implement that retry logic explicitly.

### An important consequence: the callback must be safe to run more than once

```js
await mongoose.connection.transaction(async (session) => {
  await Order.create([{ ... }], { session });
  await sendConfirmationEmail();   // ❌ could be sent multiple times if the callback is retried
});
```

Because the callback may run several times (once per retry attempt), it must be **idempotent with respect to anything outside the transaction** — this is one more reason to keep non-database side effects (emails, external API calls) **out** of the transaction callback, and run them only after the transaction has definitively committed.

```js
const order = await mongoose.connection.transaction(async (session) => {
  const [order] = await Order.create([{ ... }], { session });
  return order;
});
await sendConfirmationEmail(order);   // ✅ runs exactly once, after a confirmed successful commit
```

---

## When you probably DON'T need a transaction

```js
// a single write is already atomic on its own — no transaction needed
await User.updateOne({ _id: userId }, { $set: { name: "New Name", age: 31 } });
```

MongoDB guarantees a **single document** write is atomic on its own — even updating multiple fields within one document happens all-or-nothing, without any transaction. This is one of the design benefits of MongoDB's embedded document model (`11-relationships/02-embedding-vs-referencing-in-mongoose.md`): data that lives together in one document can be changed atomically without a transaction at all. Transactions become necessary specifically when a change must span **multiple documents** (or multiple collections) atomically.

## Common mistakes

- **Reading outside the session, then writing inside it** — the read may see stale data relative to the transaction's view; pass `.session(session)` on reads that inform writes within the same transaction.
- **Doing external side effects (emails, API calls) inside the transaction callback** — they can be repeated on a retry and won't be rolled back on failure; do them after a confirmed commit.
- **Using a transaction where a single-document atomic update would suffice** — adds real overhead for no benefit, since single-document writes are already atomic by default.
- **Splitting the balance check and the debit into a separate read and write** in a money-transfer scenario, when folding the check into the update's filter (`balance: { $gte: amount }`) makes it a single atomic operation with no race window.

## Quick summary

- Transactions guarantee all-or-nothing across multiple documents/collections — the classic uses are money transfers, creating related documents together, and deleting a parent with its dependents
- Folding a condition into an update's filter (`{ balance: { $gte: amount } }`) is often safer than a separate check-then-write inside a transaction
- `withTransaction()`/`connection.transaction()` automatically retry on transient errors like write conflicts — which means the callback **must** be safe to run more than once, so no external side effects inside it
- A single document's write is already atomic without any transaction — transactions are for changes that must span multiple documents

## Section complete

That covers transactions in full — the session/commit/abort mechanics and the practical patterns built on them. **`14-performance`** covers making everything in this guide fast — indexes, query optimization, `.lean()`, pagination, and connection pooling.
