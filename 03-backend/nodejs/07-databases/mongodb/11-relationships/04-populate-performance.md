# Populate Performance

`.populate()` is convenient, but it's not free — this file covers exactly what it costs, when that cost matters, and when an aggregation `$lookup` is the better-performing alternative.

## What `.populate()` actually costs: extra queries

```js
const posts = await Post.find({ published: true }).populate("author");
```

As established in `01-references-and-populate.md`, this is genuinely **two separate queries** under the hood: one for the posts, one for the referenced authors (batched into a single `User.find({ _id: { $in: [...] } })` covering every unique author across the result set — not one query per post, which is an important detail: Mongoose is smart enough to batch the referenced-document lookup into one additional query, not N additional queries).

```js
// roughly equivalent to what Mongoose does internally:
const posts = await Post.find({ published: true });
const authorIds = [...new Set(posts.map((p) => p.author.toString()))];
const authors = await User.find({ _id: { $in: authorIds } });
// then Mongoose stitches the authors back onto the corresponding posts
```

So `.populate()` is **not** N+1 queries in the classic sense (one extra query per row) — it's a fixed, small number of additional queries (typically one per populated field, regardless of how many documents are in the result), which is much better than naive per-document lookups but still real, additional round trips beyond the base query.

---

## Where it does get expensive: nested/chained populates

```js
const post = await Post.find()
  .populate({
    path: "comments",
    populate: { path: "author" },
  })
  .populate("category");
```

Each level of nested populate, and each separately-chained `.populate()` call, is another query — a deeply nested populate chain (posts → comments → comment authors → ...) can add up to a meaningful number of sequential round trips for a single logical request, even though each individual query is fast.

---

## Fetching more data than needed

```js
// ❌ fetches the ENTIRE User document for every author, including fields never used
const posts = await Post.find().populate("author");
```

```js
// ✅ only the fields actually needed
const posts = await Post.find().populate("author", "name avatarUrl");
```

A very easy, very common performance win — always field-select on `.populate()` (`01-references-and-populate.md`), the same way you'd `.select()` on a normal query, rather than pulling back a full referenced document (which might include a `select: false`-protected field anyway, but still often includes plenty of genuinely unnecessary data).

---

## `.lean()` and `.populate()` together

```js
const posts = await Post.find().populate("author").lean();
```

`.lean()` (`06-crud-methods/05-query-chaining-and-cursor-methods.md`) works together with `.populate()` — the populated result is plain objects instead of full Mongoose Documents, meaningfully lighter for a read-only listing that doesn't need Document features on either the parent or the populated data.

---

## When to reach for aggregation's `$lookup` instead

```js
// .populate() — two queries, simple and readable
const posts = await Post.find({ published: true }).populate("author", "name");
```

```js
// aggregation $lookup — ONE query, but more verbose to write
const posts = await Post.aggregate([
  { $match: { published: true } },
  {
    $lookup: {
      from: "users",
      localField: "author",
      foreignField: "_id",
      as: "author",
    },
  },
  { $unwind: "$author" },
  { $project: { title: 1, "author.name": 1 } },
]);
```

`$lookup` performs the join **inside MongoDB itself**, in a single round trip, rather than as two separate queries stitched together by Mongoose in application code. For most everyday use, `.populate()`'s extra round trip is genuinely negligible — but for high-throughput endpoints, very large result sets, or pipelines that need to filter/sort/aggregate based on fields from the _related_ collection (which `.populate()` can't do at all, since it happens after the main query already ran), `$lookup` is both faster and more capable. Full aggregation coverage in `12-aggregation-with-mongoose/`.

### The key capability gap: filtering on a populated field

```js
// ❌ you CANNOT filter posts by their author's name using .populate() —
// populate happens AFTER the main query has already selected which posts to return
const posts = await Post.find({ "author.name": "Alice" }).populate("author"); // does NOT work as expected
```

```js
// ✅ $lookup can filter on the joined data, since the join happens as part of the query itself
const posts = await Post.aggregate([
  {
    $lookup: {
      from: "users",
      localField: "author",
      foreignField: "_id",
      as: "author",
    },
  },
  { $unwind: "$author" },
  { $match: { "author.name": "Alice" } },
]);
```

This is the single clearest signal that `.populate()` is the wrong tool for the job: if you need to filter or sort the **primary** query results based on a field that lives in the **referenced** collection, `.populate()` fundamentally can't do it (since the referenced data isn't available until after the main query already ran) — that's exactly the situation `$lookup` exists for.

---

## A rough decision guide

| Situation                                                                                               | Prefer                                                                                            |
| ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------- |
| Simple "get this document plus its related data" for display                                            | `.populate()` — simpler code, the extra query is usually negligible                               |
| Need to filter/sort the main result set based on a field in the related collection                      | `$lookup` — `.populate()` can't do this at all                                                    |
| Very high-throughput endpoint, every millisecond matters                                                | `$lookup` — one round trip instead of two                                                         |
| Deeply nested relationships, several levels                                                             | Consider `$lookup`, since each populate level adds another sequential query                       |
| Need computed/aggregated values combining data across the relationship (a sum, a count with conditions) | `$lookup` + further aggregation stages — plain `.populate()` has no aggregation capability at all |

## Common mistakes

- **Assuming `.populate()` is N+1 queries** — it's not; Mongoose batches the referenced-document lookup into one query per populated field, not one per document.
- **Not field-selecting on `.populate()`** — fetches entire referenced documents when only a couple of fields are actually used.
- **Trying to filter the main query based on a populated field's value** — doesn't work as expected, since populate happens after the primary query already selected its results; this specifically requires `$lookup`.
- **Reaching for `$lookup` prematurely for a simple, low-traffic "show this with its related data" case** — adds real code complexity for a performance difference that's often genuinely negligible at that scale.

## Quick summary

- `.populate()` adds one additional (batched) query per populated field — not one per document, but still real, additional round trips
- Always field-select on populate (`"field", "onlyThese"`) to avoid fetching unnecessary data from the referenced collection
- `.lean()` combines with `.populate()` for lighter-weight, read-only results
- `.populate()` **cannot** filter or sort the primary query based on a field in the related collection — that specific capability requires an aggregation `$lookup`
- Default to `.populate()` for simplicity; reach for `$lookup` when you need to query across the relationship itself, not just display related data alongside it

## Section complete

That covers relationships in full — references and `.populate()`, the embed-vs-reference decision, virtual populate, and performance trade-offs including when `$lookup` is required rather than just faster. **`12-aggregation-with-mongoose`** covers the aggregation pipeline itself, including `$lookup`, in depth.
