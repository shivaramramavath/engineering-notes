# Common Pipeline Recipes

Practical, ready-to-adapt aggregation pipelines for the situations you'll hit constantly — joins, grouping/counting, and realistic multi-stage combinations.

## `$match` — filtering (the aggregation equivalent of a `find()` filter)

```js
Order.aggregate([{ $match: { status: "completed" } }]);
```

Uses the exact same filter syntax as everything in `07-filter-conditions/` — `$match` is where all those operators (`$gte`, `$in`, `$exists`, etc.) apply within a pipeline. Put `$match` as early as possible in a pipeline, so later, more expensive stages only process the documents that actually matter.

---

## `$group` — aggregating values

```js
const spendingByCustomer = await Order.aggregate([
  { $match: { status: "completed" } },
  {
    $group: {
      _id: "$customerId",
      totalSpent: { $sum: "$amount" },
      orderCount: { $sum: 1 },
      averageOrderValue: { $avg: "$amount" },
    },
  },
  { $sort: { totalSpent: -1 } },
]);
```

```js
[
  {
    _id: ObjectId("..."),
    totalSpent: 1250.5,
    orderCount: 8,
    averageOrderValue: 156.31,
  },
  {
    _id: ObjectId("..."),
    totalSpent: 890.0,
    orderCount: 3,
    averageOrderValue: 296.67,
  },
];
```

`_id` in a `$group` stage is the field(s) to group **by** — every document sharing the same `customerId` gets collapsed into one result row, with `$sum`/`$avg`/`$min`/`$max`/`$push` (and others) computing an aggregate value across that group.

### Grouping by multiple fields

```js
Order.aggregate([
  {
    $group: {
      _id: { customer: "$customerId", month: { $month: "$createdAt" } },
      total: { $sum: "$amount" },
    },
  },
]);
```

A compound `_id` groups by the combination of fields — here, spending per customer, per calendar month.

---

## `$lookup` — joining another collection

```js
const posts = await Post.aggregate([
  { $match: { published: true } },
  {
    $lookup: {
      from: "users", // the actual COLLECTION name — see 01-the-aggregate-method.md
      localField: "author",
      foreignField: "_id",
      as: "author",
    },
  },
  { $unwind: "$author" }, // $lookup produces an ARRAY — unwind it to a single object if you expect exactly one match
]);
```

This is the `.populate()` alternative from `11-relationships/04-populate-performance.md`, run entirely inside MongoDB in one round trip. `$unwind` deconstructs the array `$lookup` always produces (even for a one-to-one relationship, it's still an array with one element) into a plain object — skip `$unwind` if you're joining a genuinely one-to-many relationship and want to keep the full array.

### Filtering on the joined data (the thing `.populate()` can't do)

```js
Post.aggregate([
  {
    $lookup: {
      from: "users",
      localField: "author",
      foreignField: "_id",
      as: "author",
    },
  },
  { $unwind: "$author" },
  { $match: { "author.role": "admin" } }, // filtering based on the JOINED collection's field
]);
```

Exactly the capability gap flagged in `11-relationships/04-populate-performance.md` — filtering the primary result set based on a field from the related collection, something `.populate()` fundamentally cannot do.

---

## `$project` — reshaping the output

```js
Order.aggregate([
  { $project: { customerId: 1, amount: 1, year: { $year: "$createdAt" } } },
]);
```

Like a query projection (`06-crud-methods/02-read-methods.md`), but far more powerful — `$project` can compute entirely new fields (like extracting the year from a date) rather than just including/excluding existing ones.

---

## `$facet` — multiple aggregations in one query

```js
const results = await Product.aggregate([
  { $match: { category: "electronics" } },
  {
    $facet: {
      totalCount: [{ $count: "count" }],
      priceRanges: [
        {
          $bucket: {
            groupBy: "$price",
            boundaries: [0, 50, 100, 500, 1000],
            default: "1000+",
          },
        },
      ],
      topRated: [{ $sort: { rating: -1 } }, { $limit: 5 }],
    },
  },
]);
```

```js
{
  totalCount: [{ count: 142 }],
  priceRanges: [ { _id: 0, count: 30 }, { _id: 50, count: 55 }, ... ],
  topRated: [ /* top 5 products */ ],
}
```

`$facet` runs multiple independent sub-pipelines against the **same** filtered input in a single query — useful for a page needing several different views of the same underlying data set (a total count, a price-range breakdown, and a top-rated list) without running three completely separate queries.

---

## A realistic, fully combined pipeline

```js
const report = await Order.aggregate([
  {
    $match: {
      status: "completed",
      createdAt: { $gte: startOfMonth, $lt: endOfMonth },
    },
  },
  {
    $lookup: {
      from: "users",
      localField: "customerId",
      foreignField: "_id",
      as: "customer",
    },
  },
  { $unwind: "$customer" },
  {
    $group: {
      _id: "$customer.country",
      totalRevenue: { $sum: "$amount" },
      orderCount: { $sum: 1 },
    },
  },
  { $sort: { totalRevenue: -1 } },
  { $limit: 10 },
]);
```

Reads as: this month's completed orders, joined with customer data, grouped by the customer's country, sorted by total revenue, top 10 — a realistic "revenue by country this month" report combining `$match`, `$lookup`, `$unwind`, `$group`, `$sort`, and `$limit` in one pipeline, one round trip.

---

## Pipeline stage ordering matters for performance

```js
// ❌ joins EVERY order to a user before filtering — wasteful
Order.aggregate([
  {
    $lookup: {
      from: "users",
      localField: "customerId",
      foreignField: "_id",
      as: "customer",
    },
  },
  { $match: { status: "completed" } },
]);
```

```js
// ✅ filters first, so $lookup only processes the orders that actually matter
Order.aggregate([
  { $match: { status: "completed" } },
  {
    $lookup: {
      from: "users",
      localField: "customerId",
      foreignField: "_id",
      as: "customer",
    },
  },
]);
```

Unlike a SQL query planner, which can sometimes reorder operations for you, an aggregation pipeline runs stages in **exactly** the order you wrote them — putting `$match` (and any other filtering) as early as possible is a genuinely important, easy performance habit, since every stage after a filter only processes however many documents survived it.

## Common mistakes

- **Putting `$lookup`/`$group` before `$match`** — processes far more data than necessary; always filter as early in the pipeline as possible.
- **Forgetting `$unwind` after a `$lookup`** when you expect exactly one matching document — leaves the field as a single-element array instead of the plain object you probably wanted.
- **Using the Mongoose model name instead of the actual collection name** in `$lookup`'s `from` field (covered in the previous file, worth repeating since it's such a common trip-up).
- **Running several separate aggregation queries against the same filtered data** when `$facet` could combine them into one round trip.

## Quick summary

- `$match` filters using the same operators as `07-filter-conditions/`; put it as early as possible for performance
- `$group` aggregates by a chosen `_id` (single or compound), using `$sum`/`$avg`/`$min`/`$max`/etc.
- `$lookup` joins another collection by its real collection name, producing an array that `$unwind` can flatten — and, unlike `.populate()`, can be filtered on directly with a subsequent `$match`
- `$project` reshapes output, including computing entirely new derived fields
- `$facet` runs multiple independent sub-aggregations against the same input in a single query
- Pipeline stages run in the exact order written — ordering is a real, direct performance lever

## Section complete

That covers aggregation with Mongoose — the `aggregate()` method's Mongoose-specific behavior, and the join/group/reshape/multi-view recipes you'll reach for constantly. **`13-transactions`** covers wrapping multiple operations — writes, and sometimes reads — into a single atomic unit.
