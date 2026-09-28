# Indexes in Mongoose

`04-schemas/07-indexes-in-schemas.md` covered the _syntax_ for declaring indexes. This file covers the performance side: what an index actually does, how to decide what to index, and how to verify an index is really being used.

## What an index actually does

```js
User.find({ email: "alice@example.com" });
```

Without an index on `email`, MongoDB performs a **collection scan** — reading every single document in the collection to check whether its `email` matches. With an index on `email`, MongoDB looks the value up in a sorted structure (much like a book's index) and jumps straight to the matching document(s).

```
1,000 documents      → a collection scan is fast enough not to notice
1,000,000 documents  → a collection scan can take seconds; an indexed lookup takes milliseconds
```

The difference is invisible on small development datasets, which is exactly why missing indexes so often go unnoticed until production data grows.

---

## The cost of an index

```js
userSchema.index({ email: 1 });
userSchema.index({ lastName: 1, firstName: 1 });
userSchema.index({ createdAt: -1 });
```

Indexes aren't free:

- **Every write gets slower** — inserting, updating, or deleting a document also has to update every index that covers its fields
- **Every index consumes memory and disk space**
- **Indexes are only worth it if queries actually use them** — an unused index is pure overhead

So the goal isn't "index everything" — it's "index what your real queries actually filter and sort on."

---

## Deciding what to index

Start from your actual query patterns, not from the schema:

```js
// these queries happen constantly → their filter/sort fields are index candidates
User.find({ email }); // → index on email (likely unique)
Post.find({ author: userId }).sort({ createdAt: -1 }); // → compound index on { author, createdAt }
Order.find({ status: "pending", createdAt: { $lt: cutoff } }); // → compound index on { status, createdAt }
```

Rough rules of thumb:

- **Fields you filter on frequently** (`find({ email })`) → index them
- **Fields you sort on** → index them (a sort on an unindexed field forces MongoDB to sort in memory)
- **Fields used together in filters** → consider a compound index
- **Low-cardinality fields alone** (a boolean `isActive`, a status with 3 possible values) are usually poor index candidates _on their own_ — an index that only narrows results down to "half the collection" doesn't help much (though they can be valuable as the _leading_ field of a compound index alongside a more selective one)

---

## Compound indexes and field order

```js
postSchema.index({ author: 1, createdAt: -1 });
```

Field order matters. This index efficiently supports:

```js
Post.find({ author: userId }); // ✅ uses the leading field
Post.find({ author: userId }).sort({ createdAt: -1 }); // ✅ uses both fields
```

But it does **not** efficiently support:

```js
Post.find({ createdAt: { $gte: someDate } }); // ❌ can't use this index efficiently — skips the leading field
```

This is the **leftmost prefix rule**: a compound index can serve queries that use its leading field(s) in order, but not queries that skip the leading field. Put the field you always filter on first, and the field you additionally sort/range-filter on after it.

### The equality-sort-range guideline

```js
orderSchema.index({ status: 1, customerId: 1, createdAt: -1 });
//                  ↑ equality   ↑ equality       ↑ sort / range
```

A widely-used rule of thumb for ordering compound index fields: **equality conditions first**, then **sort fields**, then **range conditions** — this order tends to let MongoDB use the index for the filter, the sort, and the range together.

---

## Verifying an index is actually used: `explain()`

Guessing whether an index helps is unreliable — `explain()` shows what MongoDB actually did:

```js
const plan = await User.find({ email: "alice@example.com" }).explain(
  "executionStats",
);
console.log(plan.executionStats);
```

```js
{
  executionSuccess: true,
  nReturned: 1,
  totalDocsExamined: 1,       // ← only looked at ONE document: an index was used
  totalKeysExamined: 1,
  executionTimeMillis: 0,
}
```

Compare with an unindexed query on a large collection:

```js
{
  nReturned: 1,
  totalDocsExamined: 250000,   // ← had to examine EVERY document: a collection scan
  totalKeysExamined: 0,
  executionTimeMillis: 340,
}
```

### What to look for

| Field                                  | What it tells you                                                                                      |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| `totalDocsExamined` vs `nReturned`     | A large gap means MongoDB read many documents to return few — a sign an index is missing or unsuitable |
| `stage: "COLLSCAN"` (in `winningPlan`) | A full collection scan — no index was used                                                             |
| `stage: "IXSCAN"`                      | An index scan — an index was used                                                                      |
| `executionTimeMillis`                  | Actual time taken                                                                                      |

The ideal query has `totalDocsExamined` close to `nReturned`.

---

## Index management in production

```js
mongoose.set("autoIndex", false); // recommended in production — see 04-schemas/07-indexes-in-schemas.md
```

```js
await User.syncIndexes(); // deliberately create missing indexes / drop ones no longer in the schema
```

```js
const indexes = await User.collection.getIndexes();
console.log(indexes);
```

Building an index on a large, already-populated collection can be slow and resource-intensive — do it deliberately, ideally during a low-traffic window, rather than implicitly at application startup.

---

## Finding unused indexes

```js
const stats = await User.collection.aggregate([{ $indexStats: {} }]).toArray();
```

`$indexStats` reports how many times each index has been used since the server last restarted. An index with zero (or near-zero) usage over a meaningful time window is a candidate for removal — it's costing write performance and storage for no benefit.

## Common mistakes

- **Never checking with `explain()`** — assuming an index is being used when it isn't (wrong field order, a query shape the index can't serve).
- **Indexing every field "just in case"** — every index slows writes and uses memory; index for real query patterns.
- **Getting compound index field order wrong** — an index on `{ a, b }` doesn't help a query filtering only on `b`.
- **Leaving `autoIndex: true` in production** on a large collection — index builds at startup can be slow and disruptive.
- **Testing performance only against a tiny development dataset** — missing indexes often don't show up until real data volumes arrive.

## Quick summary

- An index turns a full collection scan into a fast sorted lookup, at the cost of slower writes and extra storage
- Index based on actual filter/sort patterns; compound index field order follows the leftmost-prefix rule (equality, then sort, then range is a good guideline)
- `explain("executionStats")` is how you _verify_ an index helps — compare `totalDocsExamined` to `nReturned`
- Disable `autoIndex` in production and manage index creation deliberately; use `$indexStats` to find indexes that aren't earning their cost

## Next

**`02-query-optimization-and-lean.md`** covers optimizing the queries themselves — `.lean()`, `.select()`, and avoiding unnecessary work.
