# Array Filter Conditions

Querying array fields specifically — membership, exact size, and the crucial `$elemMatch` operator for when a single array element must satisfy multiple conditions together.

## Matching a plain value against any array element

```js
User.find({ tags: "vip" });
```

Against an array field, a plain value match (no operator) checks whether **any element** of the array equals that value — this works identically for the shorthand equality check you'd use on a non-array field.

---

## `$all` — array must contain every listed value

```js
User.find({ tags: { $all: ["vip", "verified"] } });
```

Matches documents whose `tags` array contains **both** `"vip"` and `"verified"` (in any order, alongside any other tags) — different from `$in` (`02-comparison-operators.md`), which matches if the array contains **any** of the listed values.

```js
User.find({ tags: { $in: ["vip", "verified"] } }); // has vip OR verified (or both)
User.find({ tags: { $all: ["vip", "verified"] } }); // has vip AND verified (both required)
```

---

## `$size` — exact array length

```js
User.find({ tags: { $size: 3 } });
```

Matches only documents where the array has **exactly** this many elements — there's no built-in `$gt`/`$lt` equivalent for array size directly; for a range comparison on array length, you typically need an aggregation pipeline stage (`12-aggregation-with-mongoose/`) using `$size` as an aggregation expression instead.

---

## `$elemMatch` — one element satisfying multiple conditions together

This is the operator that resolves a genuinely common point of confusion, so it's worth building up to carefully.

### The problem, illustrated

```js
const user = {
  scores: [
    { subject: "math", value: 95 },
    { subject: "art", value: 40 },
  ],
};
```

```js
// ❌ this does NOT mean "an element with subject 'math' AND value < 50"
User.find({
  "scores.subject": "math",
  "scores.value": { $lt: 50 },
});
```

The query above matches the document, even though **no single score element** has both `subject: "math"` and `value < 50` — it matches because _some_ element has `subject: "math"` (the first one) and (separately) _some_ element has `value < 50` (the second one). Each condition is checked against the array independently, not against the same specific element.

### The fix: `$elemMatch`

```js
// ✅ requires a SINGLE element to satisfy both conditions together
User.find({
  scores: {
    $elemMatch: { subject: "math", value: { $lt: 50 } },
  },
});
```

Now the query correctly requires one element where `subject` is `"math"` **and** `value` is less than 50 — for the example document above, this correctly does **not** match, since the math score (95) isn't below 50.

### When you don't need `$elemMatch`

```js
User.find({ "scores.subject": "math" }); // fine — only one condition, no ambiguity possible
```

A single condition on an array's subfield doesn't need `$elemMatch` at all — the ambiguity only arises once you have **two or more conditions** that need to be satisfied by the _same_ element.

---

## `$elemMatch` in a projection

```js
User.find(
  { "orders.status": "shipped" },
  { "orders.$": 1 }, // positional projection — returns only the FIRST matching array element
);
```

```js
User.find(
  {},
  { orders: { $elemMatch: { status: "shipped" } } }, // similar idea, usable even without a matching filter condition
);
```

Both limit which array elements are actually returned (rather than the whole array) — the positional `$` requires the filter to have matched on that array; `$elemMatch` in a projection can be used more independently.

---

## Dot notation into arrays

```js
User.find({ "orders.status": "shipped" }); // any order with this status
User.find({ "orders.0.status": "shipped" }); // specifically the FIRST order in the array
```

Using a numeric index in the dot-notation path (`orders.0`) targets one specific array position directly — less common than an open "any element" query, but useful when array position is genuinely meaningful (e.g. a fixed-position array where index 0 always represents something specific).

---

## A realistic combined example

```js
Order.find({
  items: {
    $elemMatch: { sku: "ABC123", quantity: { $gte: 2 } },
  },
  status: { $ne: "cancelled" },
});
```

Orders (not cancelled) containing at least one line item that's both SKU `ABC123` **and** has a quantity of 2 or more — exactly the kind of query that would silently return wrong results without `$elemMatch`.

## Common mistakes

- **Using two separate dot-notation conditions on an array field**, intending them to apply to the same element — without `$elemMatch`, they're checked independently against the array as a whole, often producing false-positive matches.
- **Reaching for `$elemMatch` on a single-condition array query** — unnecessary; only needed once two or more conditions must be satisfied by the same element.
- **Expecting `$size` to support a range** (`$gt`/`$lt`) — it only matches an exact count; use an aggregation pipeline for a size range comparison.
- **Confusing `$all` (every listed value must be present) with `$in` (any listed value is sufficient)** — easy to mix up given how similar they look.

## Quick summary

- A plain value against an array field matches if **any** element equals it; `$all` requires **every** listed value to be present somewhere in the array
- `$size` matches an exact array length only — no built-in range comparison
- `$elemMatch` is required whenever **multiple conditions** must be satisfied by the **same** array element — without it, conditions are checked independently against the array as a whole, which is a common, subtle source of incorrect query results
- Numeric dot notation (`orders.0.field`) targets a specific array position directly

## Next

**`06-regex-and-text-filters.md`** covers pattern-matching and full-text search within filter conditions.
