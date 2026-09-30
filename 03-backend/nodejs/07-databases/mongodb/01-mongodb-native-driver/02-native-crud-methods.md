# Native CRUD Methods

The raw methods available on a `Collection` object from the native driver — the exact methods Mongoose's `Model.find()`, `Model.create()`, and friends are built directly on top of.

## Create

```js
await collection.insertOne({ name: "Alice", age: 30 });

await collection.insertMany([
  { name: "Bob", age: 25 },
  { name: "Carol", age: 35 },
]);
```

```js
{
  acknowledged: true,
  insertedId: ObjectId("64f1a2b3c4d5e6f7a8b9c0d1")
}
```

---

## Read

```js
await collection.findOne({ name: "Alice" }); // one document, or null

const cursor = collection.find({ age: { $gte: 18 } }); // a cursor, not an array
const users = await cursor.toArray();
```

```js
collection.find().sort({ age: 1 }).limit(10).skip(20);
collection.find({}, { projection: { name: 1, email: 1 } }); // note: "projection" is a named option here,
// unlike Mongoose's shorthand second argument
```

```js
await collection.countDocuments({ status: "active" });
```

---

## Update

```js
await collection.updateOne({ name: "Alice" }, { $set: { age: 31 } });

await collection.updateMany(
  { status: "pending" },
  { $set: { status: "active" } },
);

await collection.replaceOne(
  { name: "Alice" },
  { name: "Alice", age: 31, email: "alice@example.com" }, // entire document, replaced
);
```

```js
{ acknowledged: true, matchedCount: 1, modifiedCount: 1 }
```

Same operators (`$set`, `$inc`, `$push`, etc.) you'll use inside Mongoose later — nothing about the operator vocabulary changes; Mongoose adds validation/casting around the same underlying calls.

---

## Delete

```js
await collection.deleteOne({ name: "Alice" });
await collection.deleteMany({ status: "inactive" });
```

```js
{ acknowledged: true, deletedCount: 1 }
```

---

## `findOneAnd...` — atomic find + modify in one call

```js
const updated = await collection.findOneAndUpdate(
  { _id: userId },
  { $inc: { credits: -10 } },
  { returnDocument: "after" },
);

const deleted = await collection.findOneAndDelete({ _id: userId });
```

Returns the actual document, not just an acknowledgment — useful when your code needs the resulting value immediately.

---

## Bulk operations

```js
await collection.bulkWrite([
  { insertOne: { document: { name: "Dave" } } },
  { updateOne: { filter: { name: "Alice" }, update: { $set: { age: 31 } } } },
  { deleteOne: { filter: { name: "Eve" } } },
]);
```

---

## What Mongoose adds around every one of these

```js
// native driver
await collection.insertOne({ name: "Alice", age: "thirty" }); // ❌ "thirty" is silently stored as-is — no validation

// Mongoose
await User.create({ name: "Alice", age: "thirty" }); // ❌ throws a CastError — Mongoose tries to cast
// "thirty" to a Number and fails, rejecting the write
// before it ever reaches the driver
```

This is the core value proposition, made concrete: the native driver methods above accept **any shape of data** and send it to MongoDB as-is; Mongoose intercepts the call first, validates and casts the input against your schema, and only then calls the equivalent native method shown in this file. Every Mongoose method in `06-crud-methods/` maps onto one of the methods here, with that validation/casting layer added in front.

| Native driver            | Mongoose equivalent                              |
| ------------------------ | ------------------------------------------------ |
| `insertOne`/`insertMany` | `Model.create()`, `new Model().save()`           |
| `find`/`findOne`         | `Model.find()`/`Model.findOne()`                 |
| `updateOne`/`updateMany` | `Model.updateOne()`/`updateMany()`               |
| `findOneAndUpdate`       | `Model.findOneAndUpdate()`/`findByIdAndUpdate()` |
| `deleteOne`/`deleteMany` | `Model.deleteOne()`/`deleteMany()`               |
| `bulkWrite`              | `Model.bulkWrite()`                              |

## Common mistakes

- **Expecting the native driver to validate types** — it doesn't; whatever shape you give it is what gets stored, which is exactly the gap Mongoose schemas fill.
- **Forgetting `find()` returns a cursor** — same as in Mongoose; `.toArray()` (or Mongoose's implicit thenable behavior) is what actually gets you an array.
- **Using `updateOne`/`deleteOne` when every match should be affected** — the same `One` vs `Many` distinction Mongoose inherits directly.

## Quick summary

- The native driver's methods (`insertOne`, `find`, `updateOne`, `deleteOne`, `findOneAndUpdate`, `bulkWrite`) are the actual operations sent to MongoDB
- Mongoose's `Model` methods are a thin(ish) layer over exactly these calls, adding schema validation and type casting first
- Learning this mapping means Mongoose's behavior — what it validates, casts, and rejects before a write even happens — stops being a black box

## Next

**`03-query-operators-reference.md`** covers the full filter/operator vocabulary (`$gte`, `$in`, `$exists`, and more) used identically in both the native driver and Mongoose.
