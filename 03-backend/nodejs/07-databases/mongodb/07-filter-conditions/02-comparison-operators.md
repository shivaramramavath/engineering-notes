# Comparison Operators

`$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte`, `$in`, `$nin` — the operators for anything beyond a plain exact-match filter.

## `$eq` — explicit equality

```js
User.find({ status: { $eq: "active" } });
// identical to:
User.find({ status: "active" });
```

Rarely written explicitly, since the plain shorthand does the same thing — you'll mostly see `$eq` when it needs to combine with other operators on the same field, or in generated/dynamic query code.

---

## `$ne` — not equal

```js
User.find({ status: { $ne: "banned" } });
```

Matches every document where `status` is anything other than `"banned"` — including documents where the field is missing entirely, which is worth remembering (`$ne` doesn't require the field to exist, just to not equal the given value).

---

## `$gt` / `$gte` / `$lt` / `$lte` — ranges

```js
User.find({ age: { $gt: 18 } }); // strictly greater than
User.find({ age: { $gte: 18 } }); // greater than or equal
User.find({ age: { $lt: 65 } }); // strictly less than
User.find({ age: { $lte: 65 } }); // less than or equal
```

### Combining into a range

```js
User.find({ age: { $gte: 18, $lte: 65 } }); // 18 to 65 inclusive
```

Both conditions on the same field, combined in one object — this is one of the specific cases where you're applying multiple conditions to a single field without needing an explicit `$and`.

### Date ranges

```js
Order.find({
  createdAt: {
    $gte: new Date("2026-01-01"),
    $lt: new Date("2026-02-01"),
  },
});
```

This only works correctly because `createdAt` is a real BSON `Date` type (`00-mongodb-basics/03-architecture-and-bson.md`), not a string — comparison operators on a string field compare lexicographically, which rarely gives the result you actually want for dates.

---

## `$in` — matches any value in a set

```js
User.find({ status: { $in: ["active", "pending"] } });
```

Equivalent to (but far more concise than) an `$or` across each value:

```js
User.find({ $or: [{ status: "active" }, { status: "pending" }] });
```

Use `$in` whenever you're checking a field against a **fixed set of acceptable values** — it's the idiomatic choice over `$or` for this specific, very common case.

### `$in` with an array field

```js
User.find({ tags: { $in: ["vip", "verified"] } });
```

Against an array field, `$in` matches if the document's array contains **any** of the given values — different from `$all` (`05-array-filter-conditions.md`), which requires **every** given value to be present.

---

## `$nin` — matches none of a set

```js
User.find({ status: { $nin: ["banned", "deleted"] } });
```

The inverse of `$in` — matches documents whose field value isn't any of the listed values (and, like `$ne`, also matches documents where the field is missing entirely).

---

## Realistic combined example

```js
User.find({
  status: { $in: ["active", "pending"] },
  age: { $gte: 18, $lte: 65 },
  lastLoginAt: { $gte: new Date("2026-01-01") },
});
```

Active or pending users, aged 18-65, who've logged in since the start of 2026 — three fields, each using a different comparison operator, combined via the implicit `$and` from listing them together (`01-basic-filter-syntax.md`).

## Common mistakes

- **Using `$in`/`$nin` with a single value** — works, but `{ status: "active" }` is simpler and clearer than `{ status: { $in: ["active"] } }` when there's genuinely only one acceptable value.
- **Range-querying a string-typed date field** — lexicographic string comparison doesn't reliably sort/compare the way a real `Date` type does; make sure the schema field is actually `Date`.
- **Forgetting `$ne`/`$nin` also match documents where the field is entirely absent** — can produce more results than expected if you assumed it only excludes documents where the field explicitly holds the excluded value.
- **Using `$or` for a fixed-set membership check** where `$in` would be simpler and clearer — `$in` is the idiomatic choice for "any of these specific values."

## Quick summary

- `$eq`/`$ne` for equality/inequality; `$gt`/`$gte`/`$lt`/`$lte` for ranges — combine two on the same field for a full range
- `$in`/`$nin` check membership against a set of acceptable/unacceptable values — the idiomatic alternative to an `$or` chain for this case
- Range operators only behave correctly on genuinely comparable BSON types (numbers, real `Date` values) — not on strings that merely look like dates/numbers
- `$ne`/`$nin` also match documents missing the field entirely, not just ones with a different value

## Next

**`03-logical-operators.md`** covers combining multiple conditions explicitly — `$and`, `$or`, `$nor`, and `$not`.
