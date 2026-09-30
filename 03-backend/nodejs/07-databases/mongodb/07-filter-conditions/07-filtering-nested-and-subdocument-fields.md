# Filtering Nested & Subdocument Fields

Dot notation appeared throughout earlier files in this section already — this file goes deeper into the subtleties: matching a whole embedded document vs. one of its fields, and how this interacts with arrays of subdocuments.

## Matching a single nested field

```js
User.find({ "address.city": "Boston" });
```

The most common case, seen throughout this section already — reaches into an embedded document (`04-schemas/04-nested-and-subdocument-schemas.md`) without needing any special operator, just a dot-notation string key.

---

## Matching an entire embedded document, exactly

```js
User.find({
  address: { city: "Boston", zip: "02101" },
});
```

This is meaningfully different from matching individual fields — it requires the embedded document to match **exactly**, field for field, in the **same key order**, with **no extra fields**. If the stored document has any additional field in `address` (even one not shown here), or the same fields in a different order, this exact-match query won't match it.

```js
User.find({ "address.city": "Boston", "address.zip": "02101" });
```

This dot-notation version, by contrast, matches regardless of field order or any additional fields present in `address` — almost always the more robust, intended query. Exact whole-document matching is a sharp edge worth knowing about specifically so you don't reach for it by accident when you meant field-by-field matching.

---

## Nested fields inside an array of subdocuments

```js
Order.find({ "items.sku": "ABC123" });
```

Same dot notation, now reaching into each element of an array of subdocuments — matches if **any** item in the array has this SKU. As covered in `05-array-filter-conditions.md`, once more than one condition needs to apply to the **same** array element, `$elemMatch` becomes necessary instead of multiple separate dot-notation conditions.

---

## Filtering by the presence of a nested field

```js
User.find({ "address.apartment": { $exists: true } });
```

Combines the `$exists` operator (`04-element-and-type-operators.md`) with dot notation — useful when a nested field is genuinely optional (not every address has an apartment number) and you specifically need documents where it was actually provided.

---

## Deeply nested paths

```js
Company.find({ "departments.teams.name": "Platform" });
```

Dot notation composes for arbitrarily deep nesting — though as covered in `04-schemas/04-nested-and-subdocument-schemas.md`, very deep nesting tends to make both queries and updates increasingly awkward, and is often a sign a deeply-nested level should be its own referenced collection instead (`11-relationships/`).

---

## Querying a subdocument by its own `_id`

```js
User.find({ "orders._id": orderId });
```

Since array subdocuments get an auto-generated `_id` by default (`04-schemas/04-nested-and-subdocument-schemas.md`), you can filter directly by that ID — useful for "find the parent document containing this specific nested item" queries.

```js
const user = await User.findOne({ "orders._id": orderId });
const order = user.orders.id(orderId); // then pull out the specific subdocument in application code
```

---

## Filtering on a computed/aggregated condition across nested data

```js
// this DOESN'T directly express "find users whose total order value exceeds 100" —
// a plain filter can't sum across an array; that needs an aggregation pipeline
```

Worth flagging as a boundary: plain filter objects (everything in this section) can check individual fields, array membership, and per-element conditions via `$elemMatch` — but they **cannot** express a computed aggregate across an array (a sum, an average, a count meeting a threshold). That's exactly the boundary where `12-aggregation-with-mongoose/` picks up, using `$unwind`/`$group`/`$match` in a pipeline instead of a plain filter.

## Common mistakes

- **Using an exact whole-document match on a nested object** when field-by-field dot notation was actually intended — the exact-match form is far stricter (field order and completeness both matter) than most people expect.
- **Writing multiple separate dot-notation conditions on the same array's subfields**, expecting them to apply to the same element — needs `$elemMatch` instead, per `05-array-filter-conditions.md`.
- **Trying to express a computed aggregate (a sum, an average) as a plain filter condition** — not possible; that requires the aggregation pipeline.
- **Over-nesting data to the point where queries need very long, fragile dot-notation paths** — often a sign a level of nesting should be a separate referenced collection instead.

## Quick summary

- Field-by-field dot notation (`"address.city"`) is the normal, robust way to query nested data — order-independent and doesn't require an exact match on the whole subdocument
- Matching an entire embedded object directly (`{ address: {...} }`) requires an exact match, including field order and completeness — a sharp edge, not the usual approach
- Dot notation composes for deep nesting and works into arrays of subdocuments (matching "any element"), but needs `$elemMatch` once multiple conditions must apply to the same element
- A plain filter can't express a computed aggregate across an array — that's the aggregation pipeline's job

## Next

**`08-combining-filters-with-mongoose-query-builders.md`** covers `.where()` — Mongoose's chainable, alternative way of building the exact same filters covered throughout this section.
