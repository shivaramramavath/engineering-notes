# Basic Filter Syntax

How a plain JavaScript object passed to `find`/`updateOne`/`deleteOne`/etc. actually gets interpreted as a query — the foundation every other file in this section builds on.

## The simplest form: exact match

```js
User.find({ status: "active" });
```

Reads as: "find documents where the `status` field equals exactly `'active'`." No operator needed for a plain equality check — this is shorthand for `{ status: { $eq: "active" } }`.

```js
User.findOne({ email: "alice@example.com" });
User.deleteOne({ _id: userId });
```

Every one of these — filtering, updating, deleting — takes exactly this same kind of object as its first argument.

---

## Multiple fields: an implicit `$and`

```js
User.find({ status: "active", age: { $gte: 18 } });
```

Listing more than one field in the same object means **all** of them must match — this is implicitly:

```js
User.find({
  $and: [{ status: "active" }, { age: { $gte: 18 } }],
});
```

The plain multi-field form is what you'll write the vast majority of the time; explicit `$and` (`03-logical-operators.md`) is only needed for two specific cases: multiple conditions on the **same** field, or combining with an explicit `$or`/`$nor` alongside other conditions.

---

## An empty filter matches everything

```js
User.find({}); // every document in the collection
User.deleteMany({}); // ⚠️ deletes every document in the collection — always double-check this
```

Worth internalizing early, since an accidentally-empty filter (e.g. a variable that resolved to `{}` unexpectedly) silently matches everything rather than throwing an error.

---

## Nested field access: dot notation

```js
User.find({ "address.city": "Boston" });
```

Reaches into an embedded document's field directly, as a string key with a dot — covered in full depth in `07-filtering-nested-and-subdocument-fields.md`. Note this is a **string key**, not actual JavaScript property access — `{ address.city: "Boston" }` (without quotes) is invalid JavaScript syntax entirely.

---

## Values are cast against the schema

```js
const userSchema = new mongoose.Schema({ age: Number });
```

```js
User.find({ age: "30" }); // "30" is cast to the number 30 before the query runs, matching correctly
User.find({ age: "not-a-number" }); // ❌ throws a CastError — see 08-errors/03-cast-errors.md
```

This is a genuinely useful Mongoose-specific behavior: because the schema declares `age` as a `Number`, Mongoose casts a string value in the filter to match — convenient when a value comes from a route param or query string (always a string, `06-express/01-setup-and-routing.md`'s discussion of `req.query`/`req.params`), since you often don't need to manually convert it yourself before filtering.

---

## Filtering by `_id`

```js
User.find({ _id: userId }); // userId can be a string OR an actual ObjectId — both are cast/accepted
User.find({ _id: { $in: [id1, id2, id3] } }); // matching several specific documents by id
```

```js
User.find({ _id: "not-a-valid-objectid" }); // ❌ CastError — the string isn't a structurally valid ObjectId
```

---

## Building a filter dynamically from optional inputs

```js
function buildUserFilter({ status, minAge, city }) {
  const filter = {};
  if (status) filter.status = status;
  if (minAge) filter.age = { $gte: minAge };
  if (city) filter["address.city"] = city;
  return filter;
}

const users = await User.find(
  buildUserFilter({ status: "active", city: "Boston" }),
);
```

An extremely common real pattern — building a filter object incrementally based on which optional query parameters a client actually provided, rather than requiring every possible filter field to always be present.

## Common mistakes

- **Writing `{ address.city: "Boston" }` without quotes** — invalid JavaScript; nested field paths must be a quoted string key: `{ "address.city": "Boston" }`.
- **Not realizing an empty filter `{}` matches everything** — dangerous specifically with `deleteMany`, but also relevant to an accidental unfiltered `find` returning far more than expected.
- **Passing a value of the wrong type and being surprised by a `CastError`** rather than a `ValidationError` — filter-value casting failures are a distinct error type, covered in `08-errors/03-cast-errors.md`.
- **Manually converting `req.query` string values to numbers before filtering**, when Mongoose's automatic casting against the schema often handles it already — verify with a quick test rather than assuming manual conversion is always necessary.

## Quick summary

- `{ field: value }` is an exact-match filter; listing multiple fields is an implicit `$and`
- An empty filter `{}` matches every document — a real danger with `deleteMany`
- Dot notation (as a quoted string key) reaches into nested/embedded document fields
- Mongoose casts filter values against the schema's declared types automatically, throwing a `CastError` if a value truly can't be coerced

## Next

**`02-comparison-operators.md`** covers the operators for anything beyond exact equality — ranges, "not equal," and matching against a set of values.
