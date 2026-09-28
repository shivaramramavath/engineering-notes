# The `aggregate()` Method

`Model.aggregate()` — how it works, how it differs from `find()`, and the Mongoose-specific details worth knowing before writing a pipeline.

## Basic usage

```js
const results = await Order.aggregate([
  { $match: { status: "completed" } },
  { $group: { _id: "$customerId", totalSpent: { $sum: "$amount" } } },
  { $sort: { totalSpent: -1 } },
]);
```

An array of **stages**, each one transforming the data as it flows through the pipeline — `$match` filters, `$group` aggregates, `$sort` orders, and the result of each stage feeds into the next.

---

## Key differences from `find()`

### 1. Results are plain objects, not Mongoose Documents

```js
const results = await Order.aggregate([{ $match: { status: "completed" } }]);
results[0] instanceof mongoose.Document; // false — always a plain object
results[0].save; // undefined
```

Unlike `find()` (which returns Documents unless you explicitly call `.lean()`), `aggregate()` **always** returns plain JavaScript objects — there's no equivalent "un-lean" option, since an aggregation result often doesn't even correspond one-to-one with a real document shape anymore (it might be a grouped summary, a reshaped projection, or a joined combination of two collections).

### 2. No automatic casting or validation

```js
await Order.aggregate([
  { $match: { amount: "not-a-number" } }, // no CastError — this just won't match anything meaningfully
]);
```

`find()` casts filter values against the schema (`07-filter-conditions/01-basic-filter-syntax.md`); `aggregate()` does **not** — you're responsible for passing correctly-typed values into pipeline stages yourself, since Mongoose doesn't apply its schema layer to aggregation pipeline construction the way it does for query filters.

### 3. Middleware still applies, but only aggregate middleware

```js
schema.pre("aggregate", function (next) {
  this.pipeline().unshift({ $match: { deletedAt: { $exists: false } } });
  next();
});
```

As covered in `10-middleware-hooks/02-document-vs-query-vs-aggregate-middleware.md`, `aggregate()` triggers its own distinct middleware category — not document or query middleware — useful for automatically injecting something like a soft-delete filter into every aggregation on a schema.

---

## Building a pipeline incrementally

```js
const pipeline = [];

pipeline.push({ $match: { status: "completed" } });

if (startDate) {
  pipeline.push({ $match: { createdAt: { $gte: startDate } } });
}

pipeline.push({ $group: { _id: "$customerId", total: { $sum: "$amount" } } });

const results = await Order.aggregate(pipeline);
```

Since a pipeline is just a plain array, building it up conditionally (similar to the dynamic filter-building pattern from `07-filter-conditions/01-basic-filter-syntax.md`) is a completely normal, common pattern for a pipeline whose exact stages depend on optional inputs.

---

## Using Mongoose's aggregation-helper methods

```js
Order.aggregate()
  .match({ status: "completed" })
  .group({ _id: "$customerId", total: { $sum: "$amount" } })
  .sort({ total: -1 });
```

Mongoose's `Aggregate` object offers chainable helper methods mirroring common stages — functionally equivalent to passing the same stages as a plain array, purely a matter of syntax preference (the same choice as `.where()` vs. a plain filter object in `07-filter-conditions/08-combining-filters-with-mongoose-query-builders.md`). The plain-array form is what you'll see in the overwhelming majority of real code and documentation, since it maps directly onto raw MongoDB aggregation syntax.

---

## Referencing schema fields vs. raw collection field names

```js
const userSchema = new mongoose.Schema(
  { name: String },
  { collection: "app_users" },
);
```

```js
Order.aggregate([
  {
    $lookup: {
      from: "app_users",
      localField: "customerId",
      foreignField: "_id",
      as: "customer",
    },
  },
]);
```

An important detail: `$lookup`'s `from` field needs the actual **collection name** (`"app_users"` here, per `04-schemas/05-schema-options.md`'s collection-naming discussion), not the Mongoose model name (`"User"`) — a common early mistake, since everywhere else in Mongoose you refer to models by their registered name, but `$lookup` operates at the raw MongoDB collection level.

---

## `.exec()` and `await`, same as queries

```js
const results = await Order.aggregate(pipeline);
// or
const results = await Order.aggregate(pipeline).exec();
```

Same optional `.exec()` behavior as regular queries (`06-crud-methods/05-query-chaining-and-cursor-methods.md`) — `await`ing directly works fine in modern Mongoose.

---

## Getting results back as real Mongoose Documents (rare, deliberate)

```js
const results = await Order.aggregate(pipeline);
const hydrated = results.map((r) => Order.hydrate(r));
```

If you specifically need aggregation results to behave like real Documents (accessing virtuals, instance methods), `Model.hydrate()` (mentioned in `05-models/02-model-vs-document.md`) can wrap a plain result back into one — but only makes sense when the aggregation's output shape still actually resembles the original schema; a heavily reshaped/grouped result usually won't correspond to a valid document shape at all.

## Common mistakes

- **Expecting aggregation results to have `.save()` or other Document methods** — they're always plain objects, with no "un-lean" equivalent.
- **Passing an unconverted string/wrong-typed value into a pipeline stage**, expecting Mongoose to cast it the way `find()` would — aggregation doesn't apply schema-based casting at all.
- **Using the Mongoose model name instead of the actual collection name in `$lookup`'s `from` field** — a very common, confusing mistake, since `$lookup` operates at the raw collection level, not the Mongoose model level.
- **Forgetting that `pre("aggregate")` is its own middleware category** — a `pre("find")` hook does not automatically also apply to `aggregate()` calls.

## Quick summary

- `Model.aggregate(pipeline)` runs a multi-stage pipeline; results are always plain objects, never Mongoose Documents
- No automatic casting/validation happens on aggregation inputs — that's exclusively a `find()`/query-level behavior
- `$lookup`'s `from` needs the actual collection name, not the Mongoose model name — a very common early mistake
- Aggregate middleware is its own distinct category, separate from document/query middleware, and useful for injecting cross-cutting filters (like excluding soft-deleted documents) into every aggregation automatically

## Next

**`02-common-pipeline-recipes.md`** covers practical, ready-to-adapt pipelines for the situations you'll hit constantly — joins, grouping, and multi-stage combinations.
