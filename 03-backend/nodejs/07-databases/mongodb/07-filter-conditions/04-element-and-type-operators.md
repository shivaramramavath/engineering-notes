# Element & Type Operators

`$exists` and `$type` — checking whether a field is present at all, or what BSON type it actually holds, rather than checking its value.

## `$exists` — is the field present?

```js
User.find({ deletedAt: { $exists: true } }); // has a deletedAt field (any value, including null)
User.find({ deletedAt: { $exists: false } }); // has NO deletedAt field at all
```

Because MongoDB doesn't enforce a fixed schema at the database level (`00-mongodb-basics/01-mongodb-architecture.md`... covered in the fundamentals section), documents in the same collection can genuinely have different fields — `$exists` is how you query based on that presence/absence directly.

### The single most common real use: filtering out soft-deleted documents

```js
User.find({ deletedAt: { $exists: false } });
```

This is exactly the query at the heart of the soft-delete pattern (`15-patterns-and-architecture/02-soft-delete.md`) — "active" documents are simply the ones where a `deletedAt` field was never set.

### `$exists: true` includes fields explicitly set to `null`

```js
{
  email: null;
} // this document DOES match { email: { $exists: true } }
```

`$exists` only checks whether the key is present in the document at all — a field explicitly set to `null` still counts as "existing." If you specifically want to exclude both missing _and_ `null` values, combine it:

```js
User.find({ email: { $exists: true, $ne: null } });
```

---

## `$type` — matching by BSON type

```js
User.find({ age: { $type: "number" } });
User.find({ age: { $type: "string" } }); // finds documents where age was stored as a STRING, not a number
```

Useful specifically for finding **data inconsistencies** — a field that should always be a number but, due to a bug or a change in application logic over time, ended up stored as a string in some older documents. `$type` lets you find exactly those inconsistent documents directly, which is genuinely useful when planning a data migration/cleanup.

### Common type strings

```
"double", "string", "object", "array", "objectId", "bool", "date", "null", "int", "long", "decimal"
```

(The full list is longer — these are the ones you'll encounter most often in application data.)

### Matching multiple types

```js
User.find({ value: { $type: ["string", "number"] } });
```

An array of type names matches if the field is **any** of the listed types.

---

## Combining `$exists` and `$type`

```js
User.find({ phone: { $exists: true, $type: "string" } });
```

Fields that both exist and are the expected type — useful as a defensive filter before running logic that assumes a field is present and correctly typed, on a collection where historical data might not fully conform to the current schema (e.g. after a schema change that added a new required field, older documents predating the change won't have it).

---

## A realistic combined example: finding data to migrate

```js
// find users who predate the "preferences" field being added, to backfill a default
User.find({ preferences: { $exists: false } });
```

```js
// find any document where "age" wasn't stored as expected, for cleanup
User.find({ age: { $exists: true, $type: "string" } });
```

Both are genuinely common real-world queries when a schema evolves over time — new fields get added after a collection already has data, and `$exists`/`$type` are exactly the tools for finding and fixing (or working around) the resulting inconsistency.

## Common mistakes

- **Assuming `$exists: true` excludes `null` values** — it doesn't; a field explicitly set to `null` still "exists." Add `$ne: null` if that distinction matters.
- **Using `$type` as a substitute for actual schema validation** — it's a query-time tool for finding existing inconsistent data, not a way to prevent bad data from being written in the first place (that's what schema validation, `09-validation/`, is for).
- **Forgetting older documents may lack a newly-added schema field** — a query or piece of logic assuming a field is always present can behave unexpectedly on data written before that field existed; `$exists` is the tool for finding (and handling) exactly this gap.

## Quick summary

- `$exists: true`/`false` checks whether a key is present in the document at all, regardless of MongoDB's flexible per-document schema
- A field explicitly set to `null` still counts as existing — combine with `$ne: null` to exclude that case too
- `$type` checks a field's actual BSON type, most useful for finding data inconsistencies from schema evolution over time, rather than as a routine filter
- Both are especially relevant to schemas that have evolved, and to the soft-delete pattern's core query

## Next

**`05-array-filter-conditions.md`** covers querying array fields specifically — membership, size, and matching a single element against multiple conditions.
