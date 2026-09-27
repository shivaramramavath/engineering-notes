# Document vs Query vs Aggregate Middleware

Mongoose has three distinct middleware categories, and which one applies to a given hook depends entirely on _how_ the operation is triggered — this is the source of a very common, confusing "why didn't my hook run" problem, and this file exists specifically to resolve it clearly.

## The three categories

| Middleware type          | `this` refers to       | Triggered by                                                                                                              |
| ------------------------ | ---------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Document** middleware  | The document instance  | `document.save()`, `document.deleteOne()`, `document.validate()` — called on an already-loaded instance                   |
| **Query** middleware     | The `Query` object     | `Model.find()`, `Model.updateOne()`, `Model.findOneAndUpdate()`, `Model.deleteOne()`, etc. — called on the Model directly |
| **Aggregate** middleware | The `Aggregate` object | `Model.aggregate()`                                                                                                       |

The critical distinction: **whether a full document ever gets loaded into memory before the operation runs.** `document.save()` obviously has a document (you're calling a method on one). `Model.updateOne()` does not — it sends the update directly to MongoDB without ever constructing a Mongoose Document first, which is exactly why it's query middleware, not document middleware, and why `this` inside it is a `Query`, not the data being changed.

---

## The gotcha, demonstrated directly

```js
userSchema.pre("save", function (next) {
  console.log("Document pre-save hook running");
  this.updatedAt = new Date(); // `this` is the document — this works
  next();
});
```

```js
const user = await User.findById(userId);
user.name = "New Name";
await user.save(); // ✅ "Document pre-save hook running" — this hook fires
```

```js
await User.updateOne({ _id: userId }, { $set: { name: "New Name" } }); // ❌ the hook above does NOT fire at all
```

`updateOne` called directly on the Model never triggers a `pre("save")` document hook — there's no document being saved in that code path at all. This is exactly the same underlying gap as `insertMany` skipping save middleware (`06-crud-methods/01-create-methods.md`) — any hook attached to `"save"` specifically only fires through the document-loading path (`new Model()` + `.save()`, or `Model.create()`, which does the same thing internally).

---

## Hooking the query-level equivalent instead

```js
userSchema.pre("updateOne", function (next) {
  console.log("Query pre-updateOne hook running");
  this.set({ updatedAt: new Date() }); // `this` is the QUERY — use .set() to modify the update
  next();
});
```

```js
await User.updateOne({ _id: userId }, { $set: { name: "New Name" } });
// ✅ "Query pre-updateOne hook running" fires this time
```

To reliably run logic for **both** `.save()`-based updates and direct `Model.updateOne()` calls, you generally need **two separate hooks** — one for `"save"` (document middleware) and one for `"updateOne"` (query middleware) — since they're genuinely different middleware categories that don't share behavior automatically.

---

## `this` behaves differently — `.set()` vs direct property assignment

```js
// document middleware — direct property assignment works
userSchema.pre("save", function (next) {
  this.slug = slugify(this.title);
  next();
});
```

```js
// query middleware — there's no document to assign a property to; use .set() on the update itself
userSchema.pre("findOneAndUpdate", function (next) {
  this.set({ updatedAt: new Date() });
  next();
});
```

Inside query middleware, `this.getUpdate()` retrieves the current update object, and `this.set()` modifies it — you can't just assign `this.someField = value` the way you would on an actual document, since `this` is the query, not a document instance.

```js
userSchema.pre("findOneAndUpdate", function (next) {
  const update = this.getUpdate();
  console.log(update.$set); // inspect what's being changed
  next();
});
```

---

## Deletes: the same distinction applies

```js
userSchema.pre("deleteOne", { document: true, query: false }, function (next) {
  console.log("Document-level delete:", this._id); // fires for document.deleteOne()
  next();
});

userSchema.pre("deleteOne", { document: false, query: true }, function (next) {
  console.log("Query-level delete:", this.getFilter()); // fires for Model.deleteOne()
  next();
});
```

Because `deleteOne` (and `deleteMany`, `updateOne`, `updateMany`) can be called **either** on a Model directly **or** on a loaded document instance, Mongoose requires you to explicitly specify `{ document: true/false, query: true/false }` to disambiguate which form of `deleteOne` a given hook should apply to — without this option, Mongoose's default assumption may not match what you actually intended, especially for these dual-purpose method names.

```js
const user = await User.findById(userId);
await user.deleteOne(); // triggers the { document: true } hook

await User.deleteOne({ _id: userId }); // triggers the { document: false, query: true } hook
```

This exact distinction is precisely why a cascading-delete hook (`03-common-hook-patterns.md`) needs careful thought about which form of delete it should actually respond to.

---

## Aggregate middleware

```js
userSchema.pre("aggregate", function (next) {
  this.pipeline().unshift({ $match: { deletedAt: { $exists: false } } }); // inject a stage at the START
  next();
});
```

```js
await User.aggregate([{ $group: { _id: "$role", count: { $sum: 1 } } }]);
// the pre hook automatically prepends a $match stage excluding soft-deleted documents
```

`this.pipeline()` gives you the actual array of pipeline stages (`12-aggregation-with-mongoose/`), which you can inspect or modify before the aggregation runs — a genuinely useful pattern for automatically applying something like a soft-delete filter to every aggregation on a schema, without every call site needing to remember to add it manually.

## Common mistakes

- **Expecting a `pre("save")` hook to fire for a `Model.updateOne()` call** — it never does; these are entirely different middleware categories, and logic needed for both requires two separate hooks.
- **Trying to assign `this.field = value` inside query middleware** — `this` is a `Query`, not a document; use `this.set({ field: value })` instead.
- **Not specifying `{ document, query }` options on a `deleteOne`/`updateOne`/`updateMany` hook** — since these method names apply to both document and query forms, an ambiguous hook may not fire for the form you actually intended.
- **Assuming one hook automatically covers "any way this data might be updated"** — Mongoose doesn't unify update paths this way; a soft-delete filter, an auto-updated timestamp, or similar cross-cutting concerns typically need to be hooked at both the document and query level to cover every real code path.

## Quick summary

- Document middleware (`this` = the document) fires for methods called on an already-loaded instance (`.save()`, `document.deleteOne()`)
- Query middleware (`this` = the `Query`) fires for methods called directly on the Model (`Model.updateOne()`, `Model.find()`, `Model.deleteOne()`)
- A `pre("save")` hook never fires for `Model.updateOne()`, and vice versa — covering both paths needs two separate hooks
- `deleteOne`/`updateOne`/`updateMany` can be either document or query middleware depending on how they're called — use `{ document, query }` options to disambiguate
- Aggregate middleware's `this.pipeline()` lets you inspect/modify an aggregation's stages before it runs

## Next

**`03-common-hook-patterns.md`** puts all of this together into three genuinely common, practical patterns.
