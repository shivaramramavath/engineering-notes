# Soft Delete

A complete, production-ready soft-delete implementation, bringing together threads from across this guide: schema design, query middleware, partial indexes, and the trade-offs worth weighing before adopting it.

## The core mechanism

```js
const userSchema = new mongoose.Schema({
  name: String,
  email: String,
  deletedAt: { type: Date, default: null },
});
```

```js
// "deleting" — just set a timestamp
await User.updateOne({ _id: userId }, { $set: { deletedAt: new Date() } });

// "active" users — exclude soft-deleted ones
await User.find({ deletedAt: null });
```

The core idea, as covered in the MongoDB fundamentals section: nothing is actually removed. A document is "deleted" by marking it, and every query that should only see active data needs to filter it out.

---

## The real problem: remembering to filter every query

```js
// every single query anywhere in the app needs to remember this:
await User.find({ deletedAt: null, status: "active" });
await User.findOne({ deletedAt: null, email });
await User.countDocuments({ deletedAt: null });
```

Forgetting this filter in even one code path silently exposes "deleted" data — a real, easy-to-introduce bug. The fix: enforce it automatically with query middleware, so individual call sites don't have to remember.

---

## Automatic filtering with query middleware

```js
function excludeSoftDeleted(next) {
  // only apply if the query doesn't ALREADY explicitly ask about deletedAt
  if (this.getFilter().deletedAt === undefined) {
    this.where({ deletedAt: null });
  }
  next();
}

userSchema.pre("find", excludeSoftDeleted);
userSchema.pre("findOne", excludeSoftDeleted);
userSchema.pre("countDocuments", excludeSoftDeleted);
userSchema.pre("findOneAndUpdate", excludeSoftDeleted);
```

Now every `find`/`findOne`/`countDocuments`/`findOneAndUpdate` call on `User` automatically excludes soft-deleted documents, without any call site needing to remember. The `this.getFilter().deletedAt === undefined` check is important: it lets a query that _specifically_ wants to include deleted documents (an admin "show deleted users" view) opt out by explicitly including `deletedAt` in its own filter.

```js
await User.find(); // automatically filtered to { deletedAt: null }
await User.find({ deletedAt: { $ne: null } }); // explicitly overrides — sees ONLY deleted users
```

### Applying it to every relevant hook

Per `10-middleware-hooks/02-document-vs-query-vs-aggregate-middleware.md`, remember this needs to be applied to **every** relevant query-middleware hook name individually — there's no single "any read" hook that covers all of them automatically.

```js
[
  "find",
  "findOne",
  "countDocuments",
  "findOneAndUpdate",
  "findOneAndDelete",
  "updateMany",
].forEach((method) => {
  userSchema.pre(method, excludeSoftDeleted);
});
```

---

## The soft-delete method itself

```js
userSchema.methods.softDelete = function () {
  this.deletedAt = new Date();
  return this.save();
};

userSchema.statics.softDeleteById = function (id) {
  return this.updateOne({ _id: id }, { $set: { deletedAt: new Date() } });
};
```

Providing both an instance method and a static (`04-schemas/06-schema-methods-statics-virtuals.md`) covers the two common call shapes: one for when you already have a loaded document, one for a direct by-ID delete without loading it first.

### Restoring a soft-deleted document

```js
userSchema.methods.restore = function () {
  this.deletedAt = null;
  return this.save();
};
```

The direct recoverability benefit soft delete provides over a hard delete — undoing a mistaken or malicious delete is a one-line operation rather than a backup restoration.

---

## The partial unique index: solving the "can't re-register" problem

```js
const userSchema = new mongoose.Schema({
  email: { type: String }, // NOT unique: true directly — see below
  deletedAt: { type: Date, default: null },
});
```

```js
userSchema.index(
  { email: 1 },
  {
    unique: true,
    partialFilterExpression: { deletedAt: null },
  },
);
```

This is the detail that makes soft delete work correctly with uniqueness constraints, covered originally in `04-schemas/07-indexes-in-schemas.md`: a plain `unique: true` on `email` would permanently block a new signup using an email that belongs to a _soft-deleted_ (not actually gone) user — the value still technically exists in the collection. The `partialFilterExpression` scopes the uniqueness constraint to **only** active (non-soft-deleted) documents, so:

```js
await User.create({ email: "alice@example.com" }); // succeeds
await User.updateOne(
  { email: "alice@example.com" },
  { $set: { deletedAt: new Date() } },
); // soft-deleted
await User.create({ email: "alice@example.com" }); // ✅ succeeds — the old one is excluded from the unique constraint
await User.create({ email: "alice@example.com" }); // ❌ now fails — TWO active users can't share this email
```

Without this, soft delete and unique fields conflict in a genuinely confusing way that's easy to miss until it happens in production.

---

## Cascading soft deletes

```js
userSchema.pre("save", async function (next) {
  if (this.isModified("deletedAt") && this.deletedAt) {
    await Post.updateMany(
      { author: this._id },
      { $set: { deletedAt: new Date() } },
    );
  }
  next();
});
```

Whether a soft delete should cascade to related documents is a real design decision, not automatic — unlike a hard delete's referential-integrity concerns, a dangling reference to a soft-deleted (not actually gone) document is often perfectly fine to leave as-is (you can still `.populate()` it if needed, or explicitly check `deletedAt`), so cascading is a choice based on your actual requirements, not a forced necessity the way it can feel with hard deletes.

---

## Trade-offs, revisited with the full implementation in view

|                                           | Benefit                                     | Cost                                                                                                                                            |
| ----------------------------------------- | ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| Recoverability                            | Undo a mistake in one line                  | Every query needs the exclusion filter (mitigated by middleware)                                                                                |
| Audit trail                               | Data isn't silently gone                    | Collection grows unboundedly unless periodically archived                                                                                       |
| Preserved references                      | Other documents' references stay resolvable | Partial unique indexes needed wherever uniqueness matters                                                                                       |
| Compliance (e.g. "right to be forgotten") | —                                           | Soft delete alone may not satisfy a genuine legal erasure requirement — sometimes a real hard delete is still required after a retention period |

### When soft delete might genuinely be the wrong choice

Some data (genuinely sensitive personal information under certain privacy regulations, for instance) may need to be **actually** erased, not just hidden — soft delete conflicts with that requirement. A common resolution: soft-delete immediately for the recoverability/UX benefit, then run a scheduled job that permanently hard-deletes documents that have been soft-deleted past some retention window — combining both approaches rather than picking one exclusively.

## Common mistakes

- **Applying `unique: true` directly on a field without a `partialFilterExpression`** on a soft-deletable schema — blocks legitimate reuse of a value that belongs to a soft-deleted document.
- **Forgetting to apply the exclusion middleware to every relevant hook name** — `find`/`findOne` covered, but `countDocuments`/`findOneAndUpdate` forgotten, leaves gaps.
- **Assuming soft delete alone satisfies a genuine data-erasure legal requirement** — it doesn't; combine with a scheduled hard-delete job past a retention window if that's a real requirement.
- **Cascading soft deletes without deciding deliberately whether it's actually needed** — sometimes leaving related documents untouched (just referencing a now-soft-deleted parent) is perfectly fine.

## Quick summary

- Soft delete marks a document (typically with a `deletedAt` timestamp) instead of removing it, and needs every query to exclude marked documents
- Query middleware automates that exclusion, with an escape hatch (checking `this.getFilter()`) for queries that deliberately want to see deleted data
- A `partialFilterExpression` on any unique index is required to let a value be reused after its original holder is soft-deleted
- Cascading a soft delete to related documents is a deliberate choice, not an automatic necessity the way it often is for hard deletes
- Soft delete alone may not satisfy genuine legal data-erasure requirements — pair it with a scheduled hard-delete job past a retention window if that applies

## Next

**`03-multi-tenancy.md`** covers supporting multiple tenants in one MongoDB deployment — the main strategies and their trade-offs.
