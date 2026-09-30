# Logical Operators

`$and`, `$or`, `$nor`, `$not` — combining multiple conditions explicitly, for cases the plain multi-field shorthand from `01-basic-filter-syntax.md` can't express.

## `$and` — explicit, for when it's actually needed

```js
User.find({
  $and: [{ age: { $gte: 18 } }, { age: { $lte: 65 } }],
});
```

Wait — this specific example doesn't actually need `$and` at all:

```js
User.find({ age: { $gte: 18, $lte: 65 } }); // identical result, simpler
```

**When `$and` is genuinely required:** combining multiple _separate_ conditions on the same field where they can't be merged into one operator object, or combining explicit `$or`/`$nor` blocks with other conditions:

```js
User.find({
  $and: [
    { $or: [{ status: "active" }, { status: "pending" }] },
    { age: { $gte: 18 } },
  ],
});
```

This genuinely needs `$and`, since without it, mixing a top-level `$or` key with other top-level fields can be ambiguous or simply isn't how you'd naturally express "this OR that, AND also this other condition."

---

## `$or` — any of several conditions

```js
User.find({
  $or: [{ status: "active" }, { role: "admin" }],
});
```

Matches documents satisfying **at least one** of the listed conditions — active users, admin users, or both (a user could be an inactive admin and still match).

### `$or` vs `$in`

```js
// ❌ works, but $in is the more idiomatic choice for this specific case
User.find({ $or: [{ status: "active" }, { status: "pending" }] });

// ✅ same result, clearer intent
User.find({ status: { $in: ["active", "pending"] } });
```

Reach for `$or` when the conditions are on **different fields**, or are otherwise not expressible as a simple value-membership check; use `$in` (`02-comparison-operators.md`) for same-field, fixed-set membership.

---

## `$nor` — none of several conditions

```js
User.find({
  $nor: [{ status: "banned" }, { status: "deleted" }],
});
```

The inverse of `$or` — matches documents satisfying **none** of the listed conditions. Less commonly used than `$or`/`$and`, but occasionally clearer than an equivalent `$and` of negations:

```js
// equivalent, using $and + $ne instead
User.find({
  $and: [{ status: { $ne: "banned" } }, { status: { $ne: "deleted" } }],
});
```

For this particular same-field case, `$nin` (`02-comparison-operators.md`) is actually the more idiomatic choice over either — `$nor` earns its keep mainly when the excluded conditions span different fields.

---

## `$not` — negating a single condition

```js
User.find({ age: { $not: { $lt: 18 } } });
```

Negates a single operator expression on one field — different from `$ne`, which only negates a plain equality check. `$not` can wrap any operator expression, including a regex (`06-regex-and-text-filters.md`):

```js
User.find({ email: { $not: /^test/ } }); // exclude emails starting with "test"
```

---

## Nesting logical operators

```js
User.find({
  $or: [
    { $and: [{ status: "active" }, { role: "admin" }] },
    { $and: [{ status: "pending" }, { role: "editor" }] },
  ],
});
```

Reads as: (active AND admin) OR (pending AND editor). Logical operators can nest arbitrarily — useful for genuinely complex business rules, though a filter this nested is often a sign the underlying business logic might be clearer expressed as named, composable filter-building functions in your application code, rather than one large literal object.

---

## A realistic combined example

```js
User.find({
  $or: [
    { role: "admin" },
    { $and: [{ role: "editor" }, { department: "content" }] },
  ],
  status: { $ne: "suspended" },
});
```

Reads as: (any admin) OR (an editor specifically in the content department) — but never a suspended user, regardless of role. The top-level `status` condition combines with the `$or` block via the implicit `$and` from listing them together.

## Common mistakes

- **Using `$and` when the plain multi-field shorthand would do** — adds visual noise without changing the result; only reach for explicit `$and` when genuinely needed (same-field multiple conditions that can't merge, or combining with `$or`/`$nor`).
- **Using `$or`/`$nor` for same-field, fixed-set membership** — `$in`/`$nin` are the more idiomatic, readable choice for that specific case.
- **Confusing `$not` with `$ne`** — `$ne` only negates equality; `$not` can wrap any operator expression, including regexes.
- **Deeply nesting logical operators for genuinely complex business rules** without extracting readable, named filter-building functions — makes the resulting query hard to review or modify later.

## Quick summary

- `$and` is usually implicit (just list multiple fields); use it explicitly only for same-field multi-conditions or combining with `$or`/`$nor`
- `$or` matches any of several conditions, best for cross-field alternatives — `$in` is preferred for same-field, fixed-set membership
- `$nor` is the inverse of `$or`, less commonly needed than `$nin` for the same-field case
- `$not` negates a single operator expression (including regexes), unlike `$ne` which only negates plain equality
- Nested logical operators can express arbitrarily complex conditions, but consider extracting readable filter-building functions once nesting gets deep

## Next

**`04-element-and-type-operators.md`** covers checking whether a field exists at all, or matches a specific BSON type.
