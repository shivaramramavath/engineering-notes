# Pagination

Returning large result sets a page at a time — and why the obvious `skip`/`limit` approach, which appeared throughout earlier files, stops scaling once a collection grows large.

## `skip`/`limit` — simple, but degrades on deep pages

```js
const page = 3;
const pageSize = 20;

const users = await User.find()
  .sort({ createdAt: -1 })
  .skip((page - 1) * pageSize)
  .limit(pageSize);
```

Easy to implement, and fine for small collections or shallow pages. The problem: `skip(n)` doesn't jump straight to document `n` — MongoDB still has to walk through and discard the first `n` matching documents before it can return the next batch.

```js
.skip(20)     // discards 20 documents — barely noticeable
.skip(20000)   // discards 20,000 documents — noticeably slower
.skip(2000000)  // discards 2,000,000 documents — very slow, on every request for that deep page
```

The cost of `skip` grows linearly with how deep into the result set you're paging — page 100,000 of a feed is dramatically more expensive than page 2, even though both return the same number of documents.

---

## Cursor-based (keyset) pagination — the scalable alternative

Instead of "skip N, take the next page," keep track of a value from the **last document seen**, and ask for documents after it.

```js
const pageSize = 20;

// first page — no cursor yet
let query = User.find().sort({ _id: -1 }).limit(pageSize);

// subsequent pages — filter to "everything after the last one we saw"
let query = User.find({ _id: { $lt: lastSeenId } })
  .sort({ _id: -1 })
  .limit(pageSize);
```

```js
const users = await query;
const nextCursor = users.length ? users[users.length - 1]._id : null;
```

Because this uses an indexed filter (`_id: { $lt: lastSeenId }`) rather than discarding N documents, the cost of fetching "the next page" stays roughly constant **no matter how deep** you are in the result set — page 2 and page 100,000 cost about the same.

### Using a field other than `_id`

```js
const users = await User.find({ createdAt: { $lt: lastSeenCreatedAt } })
  .sort({ createdAt: -1 })
  .limit(pageSize);
```

Works with any indexed, sortable, unique-enough field — `_id` is a convenient default since it's always indexed and always unique, and (per `00-mongodb-basics/03-architecture-and-bson.md`) sorts roughly by creation time already.

### Handling ties with a compound cursor

```js
// if createdAt isn't unique on its own, break ties with _id too
const users = await User.find({
  $or: [
    { createdAt: { $lt: lastSeen.createdAt } },
    { createdAt: lastSeen.createdAt, _id: { $lt: lastSeen._id } },
  ],
})
  .sort({ createdAt: -1, _id: -1 })
  .limit(pageSize);
```

If the sort field can have duplicate values across documents, a plain `$lt` on it alone can skip or repeat documents that share the same value — adding `_id` as a tiebreaker (matching the compound sort) makes the cursor unambiguous.

---

## `skip`/`limit` vs cursor-based: when to use which

|                                     | `skip`/`limit`                                      | Cursor-based                                                    |
| ----------------------------------- | --------------------------------------------------- | --------------------------------------------------------------- |
| Implementation complexity           | Simple                                              | Slightly more code                                              |
| Performance on deep pages           | Degrades with depth                                 | Stays roughly constant                                          |
| Supports "jump to page 47 directly" | Yes                                                 | No — only "next"/"previous" from a known point                  |
| Good fit for                        | Small collections, admin tools, bounded result sets | Public-facing feeds, infinite scroll, large/growing collections |

Cursor-based pagination gives up the ability to jump to an arbitrary page number (there's no way to compute "the cursor for page 47" without walking through the data) — if a UI genuinely needs numbered page links to arbitrary pages, `skip`/`limit` may be unavoidable despite its cost, though many products avoid that requirement specifically because of this trade-off (infinite scroll / "load more" instead of page numbers).

---

## Getting a total count alongside paginated results

```js
const [users, totalCount] = await Promise.all([
  User.find().sort({ createdAt: -1 }).limit(pageSize),
  User.countDocuments(),
]);
```

A separate `countDocuments()` call, run concurrently with the actual page fetch — needed if the UI shows "page 3 of 47" or "1,204 results," but worth noting `countDocuments()` itself has a real cost on a large, unfiltered collection; consider `estimatedDocumentCount()` (`06-crud-methods/02-read-methods.md`) if an exact count isn't essential.

---

## A complete cursor-based pagination endpoint

```js
app.get("/api/users", async (req, res) => {
  const pageSize = Math.min(Number(req.query.limit) || 20, 100); // cap the max page size
  const cursor = req.query.cursor;

  const filter = cursor ? { _id: { $lt: cursor } } : {};

  const users = await User.find(filter)
    .sort({ _id: -1 })
    .limit(pageSize)
    .lean();

  const nextCursor =
    users.length === pageSize ? users[users.length - 1]._id : null;

  res.json({ users, nextCursor });
});
```

```
GET /api/users?limit=20
GET /api/users?limit=20&cursor=64f1a2b3c4d5e6f7a8b9c0d1
```

Capping the maximum page size (`Math.min(..., 100)`) prevents a client from requesting an enormous, expensive page — worth doing regardless of which pagination style you use.

## Common mistakes

- **Using `skip`/`limit` for a public, potentially-deep feed** (social media, search results) without realizing performance will degrade as users page deeper or the collection grows.
- **Building a cursor from a non-unique field with no tiebreaker** — can skip or duplicate documents that share the same sort-field value.
- **Not capping the requested page size** — a client asking for `limit=1000000` should be clamped, not honored as-is.
- **Running `countDocuments()` on every paginated request on a huge, unfiltered collection** when an approximate count (`estimatedDocumentCount()`) or no count at all would be acceptable.

## Quick summary

- `skip`/`limit` is simple but gets slower the deeper you page, since MongoDB must walk through and discard every skipped document
- Cursor-based (keyset) pagination filters on "after the last item seen" using an indexed field, keeping cost roughly constant regardless of depth — the standard choice for large or growing, public-facing lists
- Use a unique or tie-broken field (often `_id`, or a compound sort with `_id` as a tiebreaker) as the cursor
- Cursor-based pagination trades away "jump to an arbitrary page number" for consistent performance — fine for infinite scroll, not for numbered pagination UIs
- Always cap the maximum requested page size

## Next

**`04-connection-pooling.md`** covers the connection layer underneath every one of these queries — how Mongoose's pool works and how to size it correctly.
