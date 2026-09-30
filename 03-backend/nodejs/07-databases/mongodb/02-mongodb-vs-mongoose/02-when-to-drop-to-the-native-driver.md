# When to Drop to the Native Driver

Mongoose exposes the underlying native driver collection directly, for the situations where its own abstraction gets in the way rather than helping. This file covers the escape hatches and when reaching for them is the right call.

## `Model.collection` — the raw native driver collection

```js
const User = mongoose.model("User", userSchema);

User.collection; // the actual native driver Collection object, no Mongoose layer at all
```

```js
// bypasses schema casting, validation, and middleware entirely
await User.collection.insertOne({
  anything: "goes here",
  age: "not even a number",
});
```

This is the direct connection to everything covered in `01-mongodb-native-driver/` — every method from that section (`insertOne`, `find`, `updateOne`, `bulkWrite`, etc.) is available here, completely unmediated by Mongoose.

---

## When this is the right call

### 1. Raw aggregation pipelines needing a driver-only stage or option

```js
const results = await Model.collection
  .aggregate([
    { $match: { status: "active" } },
    { $group: { _id: "$category", total: { $sum: "$amount" } } },
  ])
  .toArray();
```

Mongoose's own `Model.aggregate()` (`12-aggregation-with-mongoose/`) is usually sufficient and is the normal path — but for certain advanced pipeline options or stages, dropping to `.collection.aggregate()` avoids any Mongoose-specific behavior getting in the way.

### 2. Bulk operations where validation overhead genuinely matters

```js
// thousands of pre-validated, trusted records from an internal data pipeline
await Model.collection.insertMany(millionsOfAlreadyValidatedRecords);
```

If data is already known-good (e.g. produced by your own trusted internal process, not user input), skipping Mongoose's per-document casting/validation for a very large batch can be a meaningful performance win — though this trade-off should be deliberate, not a default.

### 3. Index management outside what Mongoose's schema-level `index()` covers

```js
await Model.collection.createIndex({ location: "2dsphere" });
await Model.collection.listIndexes().toArray();
```

Mongoose can declare indexes in a schema (`04-schemas/07-indexes-in-schemas.md`), but for one-off administrative tasks — inspecting existing indexes, dropping one manually, a specialized index type Mongoose's schema API doesn't cleanly express — the native methods are often more direct.

### 4. Driver-level options Mongoose doesn't surface

```js
await Model.collection.find({}, { hint: { age: 1 } }).toArray(); // forcing a specific index
```

Some fine-grained query options are easier to reach directly on the native collection than through Mongoose's query builder.

---

## What you lose when you drop down

```js
// ❌ no casting: "30" stays a string, not coerced to a Number
// ❌ no validation: a missing "required" field doesn't throw
// ❌ no middleware: a pre("save") password-hashing hook does NOT run
// ❌ returns plain driver result objects/cursors, not Mongoose Documents
await User.collection.insertOne({ email: "test@test.com" });
```

None of Mongoose's value from `01-what-mongoose-adds.md` applies once you're calling `.collection` methods directly — you're fully back to native driver behavior, with all the responsibility that implies. This is exactly why it should be a deliberate, occasional choice, not a habit — reaching for it routinely usually signals either a genuine gap in Mongoose's API for your use case, or a sign the abstraction isn't fitting the problem well.

---

## A middle ground: use Mongoose for structure, drop down for one operation

```js
async function bulkImportTrustedData(records) {
  // validate a SAMPLE with Mongoose first, to catch a bad data source early
  const sample = new Model(records[0]);
  await sample.validate(); // throws if the shape is clearly wrong

  // then bulk-insert the rest via the native driver for speed
  await Model.collection.insertMany(records);
}
```

A reasonable pattern: use Mongoose's validation where it adds real safety, and drop to the native driver specifically for the performance-sensitive bulk operation — rather than an all-or-nothing choice between the two for an entire codebase.

---

## Getting the native `Db`/`MongoClient` objects directly

```js
mongoose.connection.db; // the native driver Db object
mongoose.connection.getClient(); // the native driver MongoClient
```

Occasionally useful for truly driver-level operations (running an admin command, listing every collection in the database) that aren't scoped to any single Mongoose model at all.

## Common mistakes

- **Reaching for `.collection` as a default habit** instead of only when Mongoose genuinely lacks a clean way to do something — defeats the purpose of using an ODM in the first place.
- **Forgetting that `.collection` results are plain objects, not Mongoose Documents** — no `.save()`, no virtuals, no instance methods available on what comes back.
- **Bypassing validation for user-supplied data** to save a small amount of overhead — the performance case for skipping validation applies to trusted, pre-validated data, not arbitrary external input.
- **Not documenting _why_ a particular call drops to the native driver** — future readers (including yourself) will wonder why this one spot skips Mongoose's usual behavior; a short comment explaining the reason saves real confusion later.

## Quick summary

- `Model.collection` is the raw native driver collection, available on every Mongoose model, bypassing schemas/casting/validation/middleware entirely
- Reach for it for advanced aggregation options, bulk operations on already-trusted data, direct index management, or driver-level query options Mongoose doesn't expose
- Everything Mongoose adds (`01-what-mongoose-adds.md`) is gone the moment you're calling `.collection` methods directly — a deliberate trade-off, not a routine habit
- `mongoose.connection.db`/`.getClient()` expose the native `Db`/`MongoClient` for operations not scoped to a single model

## Section complete

That closes out the MongoDB-focused portion of this guide — basics, the native driver, and exactly how Mongoose relates to it. **`03-setup`** begins the Mongoose-focused track: installing it and connecting for real.
