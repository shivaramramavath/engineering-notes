# Delete Methods

Every way to remove documents — direct collection-level deletes, atomic find-and-delete variants, and the soft-delete alternative worth considering before reaching for any of them.

## `deleteOne()` — removes the first match

```js
await User.deleteOne({ email: "alice@example.com" });
```

```js
{ acknowledged: true, deletedCount: 1 }
```

Only counts are returned — the document's content isn't available from this call. If multiple documents match, only the first encountered is removed.

---

## `deleteMany()` — removes every match

```js
await User.deleteMany({ status: "inactive" });
```

```js
{ acknowledged: true, deletedCount: 47 }
```

### ⚠️ An empty filter deletes the entire collection

```js
await User.deleteMany({});
```

Exactly as dangerous as the native driver's equivalent — always double-check a `deleteMany` filter, especially in scripts or migrations run against real data.

---

## `findByIdAndDelete()` / `findOneAndDelete()` — atomic delete, returns the document

```js
const deletedUser = await User.findByIdAndDelete(userId);
```

```js
deletedUser; // the actual removed Document, or null if nothing matched that id
```

```js
const deletedUser = await User.findOneAndDelete({ email: "alice@example.com" });
```

Use these instead of `deleteOne`/`deleteById` whenever your code needs to know **what** was deleted — logging the removed data, showing a confirmation with the deleted item's details, or triggering follow-up logic based on its content (e.g. cleaning up related records, though for that a transaction, `13-transactions/`, is usually the more correct tool).

---

## Comparing the delete approaches

|                                        | Returns the deleted document? | Efficient?                                                            |
| -------------------------------------- | ----------------------------- | --------------------------------------------------------------------- |
| `deleteOne`/`deleteMany`               | No, just counts               | Yes — single, direct operation                                        |
| `findOneAndDelete`/`findByIdAndDelete` | Yes                           | Yes — still a single atomic operation, just also returns the document |

Unlike the update-methods comparison, there's no `runValidators`-style gotcha here — deletion doesn't validate anything, since there's no new data being written. The main decision is simply whether you need the deleted document's content back.

---

## Does delete middleware run?

```js
userSchema.pre("deleteOne", { document: false, query: true }, function (next) {
  console.log("About to delete:", this.getFilter());
  next();
});
```

Delete middleware exists (`10-middleware-hooks/`), but the exact hook name and whether it fires depends on **which** delete method you call and whether you're hooking the document or query middleware form — `deleteOne`/`deleteMany` called directly on the Model trigger **query** middleware, not document middleware, since no document is ever loaded. This distinction is covered fully in `10-middleware-hooks/02-document-vs-query-vs-aggregate-middleware.md` — worth being aware of here since it directly affects whether cleanup logic (like cascading a delete to related documents) actually fires for a given delete call.

---

## Soft delete: worth considering before any real delete

```js
await User.updateOne({ _id: userId }, { $set: { deletedAt: new Date() } });
```

```js
await User.find({ deletedAt: { $exists: false } }); // "active" users, excluding soft-deleted ones
```

As covered generally in the MongoDB fundamentals section, a soft delete trades some query complexity (every query needs to exclude soft-deleted documents) for recoverability, audit trails, and avoiding dangling references from other documents. In Mongoose specifically, this is often implemented with a **query middleware** hook (`10-middleware-hooks/`) that automatically injects `{ deletedAt: { $exists: false } }` into every `find`-family query on the schema, so individual call sites don't have to remember to add the filter manually. Full implementation depth in `15-patterns-and-architecture/02-soft-delete.md`.

---

## Deleting related documents together (a transaction consideration)

```js
const session = await mongoose.startSession();
session.startTransaction();
try {
  await Order.deleteOne({ _id: orderId }, { session });
  await OrderItem.deleteMany({ orderId }, { session });
  await session.commitTransaction();
} catch (err) {
  await session.abortTransaction();
  throw err;
} finally {
  session.endSession();
}
```

Deleting a parent and its related documents together, atomically, is a common transaction use case — full coverage in `13-transactions/`. Without a transaction, a failure partway through (e.g. the order deletes successfully but the order items delete fails) can leave the data in an inconsistent, partially-deleted state.

## Common mistakes

- **Running `deleteMany({})` without realizing it targets the entire collection** — always sanity-check the filter, especially in migration/cleanup scripts.
- **Using `deleteOne`/`deleteMany` when the deleted document's content is actually needed afterward** — use `findOneAndDelete`/`findByIdAndDelete` instead, rather than a separate `findOne` immediately before deleting (which isn't atomic and has a small race-condition window).
- **Assuming document middleware fires for a Model-level `deleteOne`/`deleteMany` call** — it's query middleware in that case; document middleware only fires for `document.deleteOne()` called on an already-loaded instance.
- **Choosing a hard delete for data with audit/legal retention needs, or that other documents reference** — a soft delete is usually the safer default in those cases.

## Quick summary

- `deleteOne`/`deleteMany` are efficient but only return counts; `findOneAndDelete`/`findByIdAndDelete` are equally efficient but also return the deleted document
- An empty filter on `deleteMany` removes the entire collection — always double-check
- Whether document or query middleware fires for a delete depends on which method you call — Model-level deletes trigger query middleware, not document middleware
- Consider a soft delete before a hard one, especially for anything with audit requirements or cross-document references
- Deleting related documents across collections atomically is a solid use case for a transaction

## Next

**`05-query-chaining-and-cursor-methods.md`** covers refining any of these queries further — sorting, limiting, selecting fields, and populating relationships.
