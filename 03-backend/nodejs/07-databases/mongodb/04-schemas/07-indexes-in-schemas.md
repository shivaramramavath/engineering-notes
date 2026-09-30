# Indexes in Schemas

Declaring indexes as part of your schema definition, so index creation is tied directly to your model rather than managed as a separate, easy-to-forget step. Full performance-oriented depth lives in `14-performance/01-indexes-in-mongoose.md` — this file covers the schema-level syntax.

## Single-field indexes

```js
const userSchema = new mongoose.Schema({
  email: { type: String, index: true },
});
```

or, equivalently, declared separately from the field definition:

```js
userSchema.index({ email: 1 }); // 1 = ascending, -1 = descending
```

Both produce the same result — `index: true` inline is convenient for a single field; `schema.index()` is necessary for compound indexes (below) and gives you a place to add index-specific options.

---

## Unique indexes

```js
userSchema.index({ email: 1 }, { unique: true });
```

Equivalent to `{ type: String, unique: true }` inline (`03-schema-type-options.md`) — both create a unique index; `schema.index()` is just an alternative, more explicit place to declare it, especially useful when combining `unique` with other index options.

---

## Compound indexes — spanning multiple fields

```js
userSchema.index({ lastName: 1, firstName: 1 });
```

A compound index supports queries filtering/sorting on the leading field(s) efficiently — this index helps a query on `lastName` alone, or `lastName` + `firstName` together, but does **not** help a query filtering on `firstName` alone (the "leftmost prefix" rule — full depth in `14-performance/01-indexes-in-mongoose.md`).

### A compound unique index

```js
orderSchema.index({ customerId: 1, orderNumber: 1 }, { unique: true });
```

Enforces uniqueness across the **combination** of fields, not each individually — a given `orderNumber` can repeat across different customers, but not twice for the same customer. A common, genuinely useful pattern for composite natural keys.

---

## Text indexes — for search

```js
articleSchema.index({ title: "text", body: "text" });
```

```js
await Article.find({ $text: { $search: "mongoose tutorial" } });
```

Enables MongoDB's built-in text search across the indexed fields — a collection can have at most one text index, though it can span multiple fields as shown above.

---

## TTL indexes — automatically expiring documents

```js
sessionSchema.index({ createdAt: 1 }, { expireAfterSeconds: 3600 });
```

MongoDB automatically deletes a document once `expireAfterSeconds` has elapsed since the value in the indexed field — extremely useful for session data, temporary tokens, or any data with a natural expiration, without needing a manual cleanup job.

---

## Partial indexes — indexing only a subset of documents

```js
userSchema.index(
  { email: 1 },
  { unique: true, partialFilterExpression: { deletedAt: { $exists: false } } },
);
```

Enforces email uniqueness only among **non-soft-deleted** users — directly solving the common problem from `15-patterns-and-architecture/02-soft-delete.md`, where a soft-deleted user's email shouldn't block a new signup using the same address, but active users' emails still need to be unique among each other.

---

## 2dsphere indexes — for geospatial queries

```js
placeSchema.index({ location: "2dsphere" });
```

```js
const schema = new mongoose.Schema({
  location: {
    type: { type: String, enum: ["Point"], default: "Point" },
    coordinates: { type: [Number] }, // [longitude, latitude]
  },
});
schema.index({ location: "2dsphere" });
```

Needed for `$near`, `$geoWithin`, and similar geospatial query operators — mentioned here for completeness; genuinely deep geospatial coverage is outside this guide's Mongoose-focused scope.

---

## Ensuring indexes exist (development vs production)

```js
mongoose.set("autoIndex", true); // default in development — builds indexes automatically on connect
mongoose.set("autoIndex", false); // recommended in production
```

By default, Mongoose builds any indexes declared in your schemas automatically the first time a model connects — convenient in development, but building an index on a large, already-populated production collection can be slow and resource-intensive, potentially impacting live traffic. In production, the standard practice is disabling `autoIndex` and running `Model.syncIndexes()` (or a dedicated migration script) deliberately, during a maintenance window or as part of a controlled deploy step, rather than implicitly at app startup.

```js
await Model.syncIndexes(); // creates missing indexes AND drops indexes no longer in the schema
```

---

## Checking what indexes actually exist

```js
const indexes = await User.collection.getIndexes();
console.log(indexes);
```

Worth verifying directly (or via Compass, `00-mongodb-basics/02-shell-and-compass.md`) after a schema change involving indexes, especially in production where `autoIndex` is off and index creation is a deliberate, separate step.

## Common mistakes

- **Relying on `autoIndex: true` in production** — index builds on a large collection can be slow and impact live queries; disable it and manage index creation deliberately.
- **Adding a compound index in the wrong field order** for the queries it's meant to support — remember the leftmost-prefix rule; an index on `{ a: 1, b: 1 }` doesn't efficiently support a query filtering on `b` alone.
- **Forgetting a partial filter expression on a unique index interacting with soft deletes** — without it, a soft-deleted document's unique field value can block a legitimate new document from using that same value.
- **Not verifying an index actually exists after a schema change**, especially in production with `autoIndex` disabled — the schema _declaring_ an index and the database actually _having_ it are two different things once autoIndex is off.

## Section complete

That's schemas covered in full depth — definition, every type, per-field options, nesting, schema-level options, methods/statics/virtuals, and indexes. **`05-models`** covers compiling a schema into the actual model object your application code uses.
