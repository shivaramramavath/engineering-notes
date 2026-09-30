# Bulk & Batch Methods

Performing many writes efficiently in a single call — `Model.bulkWrite()` for mixed operation batches, and a closer look at `insertMany`'s batch-specific options.

## `Model.bulkWrite()`

```js
await User.bulkWrite([
  { insertOne: { document: { name: "Dave" } } },
  {
    updateOne: {
      filter: { name: "Alice" },
      update: { $set: { age: 31 } },
    },
  },
  { deleteOne: { filter: { name: "Eve" } } },
]);
```

```js
{
  insertedCount: 1,
  matchedCount: 1,
  modifiedCount: 1,
  deletedCount: 1,
  upsertedCount: 0,
}
```

Same shape as the native driver's `bulkWrite` (covered in the MongoDB fundamentals section) — Mongoose's version accepts the identical operation format, applied on top of your schema.

### Does `bulkWrite` validate against the schema?

```js
await User.bulkWrite([{ insertOne: { document: { age: "not a number" } } }]);
```

**`bulkWrite` skips Mongoose's normal validation and casting pipeline** by default, much like `insertMany` skips save middleware — it operates closer to the raw driver level. This matters: data that would be rejected by `create()`/`.save()` can be written successfully via `bulkWrite`, unless you're careful about what you pass in. Treat `bulkWrite` as a tool for already-trusted, pre-shaped data (an internal sync job, a migration script) rather than a direct path for arbitrary user input.

---

## Ordered vs unordered

```js
await User.bulkWrite(operations, { ordered: true }); // default — stops at first failure
await User.bulkWrite(operations, { ordered: false }); // keeps going, reports all failures together
```

Identical semantics to `insertMany`'s ordered option (`01-create-methods.md`) and the native driver's version — choose based on whether the operations are dependent on each other's success/order.

---

## A realistic use case: syncing external data

```js
const operations = externalRecords.map((record) => ({
  updateOne: {
    filter: { externalId: record.id },
    update: { $set: record },
    upsert: true,
  },
}));

await Product.bulkWrite(operations);
```

The same "sync a batch of external records, creating or updating each as needed" pattern from the MongoDB fundamentals section — expressed once as Mongoose's `bulkWrite`, using upsert on every operation rather than looping individual `updateOne({ upsert: true })` calls.

---

## `insertMany`'s batch-specific options, revisited

```js
await User.insertMany(documents, {
  ordered: false, // keep going past individual failures
  rawResult: true, // get the full driver result object, not just an array of Documents
  lean: true, // skip Document wrapping on the results, for speed
});
```

### `rawResult: true`

```js
const result = await User.insertMany(documents, { rawResult: true });
result.insertedCount;
result.insertedIds;
```

By default, `insertMany` returns an array of full Mongoose Documents; `rawResult: true` instead returns the underlying driver's raw result object (counts, inserted IDs) — useful when you specifically need those summary details rather than the created documents themselves, and want to skip the overhead of wrapping every one as a Document.

### `insertMany` and duplicate keys within an ordered batch

```js
await User.insertMany(
  [
    { email: "a@example.com" },
    { email: "a@example.com" }, // duplicate — violates a unique index
    { email: "b@example.com" },
  ],
  { ordered: true }, // default — stops here, "b@example.com" never gets inserted
);
```

```js
await User.insertMany(
  [
    { email: "a@example.com" },
    { email: "a@example.com" },
    { email: "b@example.com" },
  ],
  { ordered: false }, // "a@example.com" (first one) and "b@example.com" both succeed; the duplicate fails alone
);
```

Directly relevant to `08-errors/04-duplicate-key-errors.md` — a batch insert with `ordered: false` continuing past a duplicate-key failure is a common, deliberate choice when syncing a batch where a handful of records might already exist.

---

## `bulkWrite` vs looping individual Mongoose calls: when it actually matters

```js
// ❌ N round trips
for (const record of records) {
  await Product.updateOne(
    { externalId: record.id },
    { $set: record },
    { upsert: true },
  );
}
```

```js
// ✅ one round trip for the whole batch
await Product.bulkWrite(
  records.map((record) => ({
    updateOne: {
      filter: { externalId: record.id },
      update: { $set: record },
      upsert: true,
    },
  })),
);
```

For a handful of records, the difference is negligible. For hundreds or thousands, the accumulated network latency from looping individual calls becomes a real, measurable cost — `bulkWrite` is the right tool once volume actually matters.

## Common mistakes

- **Assuming `bulkWrite`/`insertMany` validate and cast exactly like `create()`/`.save()`** — they largely bypass that pipeline; treat them as tools for already-trusted data, not a direct path for raw user input.
- **Looping individual write calls for genuinely large batches** — works, just far less efficient than a single `bulkWrite`/`insertMany` call.
- **Not deciding deliberately between `ordered: true`/`false`** — the default (`ordered: true`) may stop a large, otherwise-successful batch at one early, unrelated failure.
- **Not using `rawResult: true` when only summary counts are actually needed** — wrapping thousands of results as full Documents is wasted work if you only care about `insertedCount`.

## Quick summary

- `Model.bulkWrite()` sends a mixed batch of inserts/updates/deletes in one round trip, but largely bypasses Mongoose's normal validation/casting — best suited to already-trusted data
- `insertMany`'s `ordered`/`rawResult`/`lean` options mirror `bulkWrite`'s efficiency trade-offs for the specific case of bulk creation
- A common, realistic use for both: syncing external records with `updateOne` + `upsert: true` per record, batched into one call
- Reach for these specifically once volume is large enough that per-operation round trips become a real cost — not as a default replacement for `create()`/`.save()` on ordinary application writes

## Section complete

That covers every core CRUD method Mongoose provides — create, read, update, delete, query chaining, and bulk operations, along with the trade-offs between similar-looking methods. **`07-filter-conditions`** goes deep on the filter object every one of these methods accepts.
