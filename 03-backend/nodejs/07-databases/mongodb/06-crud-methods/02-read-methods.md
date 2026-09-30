# Read Methods

Every way to read data back — full document retrieval, and the lighter-weight alternatives for when you don't need the whole document at all.

## `find()` — many documents

```js
const activeUsers = await User.find({ status: "active" });
```

```js
const allUsers = await User.find(); // an empty filter matches everything
```

Returns an array of Documents (technically, `find()` returns a `Query` object that resolves to an array when awaited — see `05-query-chaining-and-cursor-methods.md` for what that means for chaining). Full filter syntax in depth in `07-filter-conditions/`.

---

## `findOne()` — a single document

```js
const user = await User.findOne({ email: "alice@example.com" });
```

```js
user; // a single Document, or null if nothing matched
```

If multiple documents match, `findOne` returns only the first one encountered — with no guaranteed order unless you also chain `.sort()`.

---

## `findById()` — a convenience shortcut for `_id` lookups

```js
const user = await User.findById(userId);
```

```js
// exactly equivalent to:
const user = await User.findOne({ _id: userId });
```

`userId` can be a string or an actual `ObjectId` — Mongoose casts it automatically. If the string genuinely isn't a valid `ObjectId` format at all (not just "doesn't exist," but structurally invalid), this throws a `CastError` rather than returning `null` — an important distinction covered in `08-errors/03-cast-errors.md`.

```js
await User.findById("not-a-valid-id"); // ❌ CastError
await User.findById("64f1a2b3c4d5e6f7a8b9c0d1"); // ✅ valid format, returns the document OR null if not found
```

---

## `countDocuments()` — counting matches

```js
const activeCount = await User.countDocuments({ status: "active" });
```

Returns just a number — meaningfully cheaper than `(await User.find({ status: "active" })).length`, since it never actually loads the matching documents, just counts them.

### `estimatedDocumentCount()` — a faster, unfiltered alternative

```js
const totalUsers = await User.estimatedDocumentCount();
```

Ignores any filter entirely and returns a fast, approximate count of the **whole collection**, using cached metadata rather than scanning — appropriate only when you need the collection's total size, not a filtered count.

---

## `exists()` — checking presence without loading anything

```js
const emailTaken = await User.exists({ email: "alice@example.com" });
```

```js
emailTaken; // { _id: ObjectId("...") } if found, or null if not
```

Returns just the `_id` (or `null`) — meaningfully cheaper than `findOne()` when all you need to know is _whether_ a match exists, not the document's actual content. A very common, direct use: checking if an email is already taken before attempting a signup.

```js
if (await User.exists({ email })) {
  throw new ConflictError("Email already in use");
}
```

Worth knowing this is a slightly different check than relying on a unique index and catching the resulting duplicate-key error (`08-errors/04-duplicate-key-errors.md`) — `exists()` is a proactive check _before_ attempting the write; the unique index is the actual, race-condition-proof guarantee. Using both together (checking proactively for a fast, friendly error in the common case, while still relying on the index to catch a genuine race condition) is a reasonable, common pattern.

---

## `distinct()` — unique values across a field

```js
const uniqueRoles = await User.distinct("role");
```

```js
["user", "admin", "editor"];
```

```js
const uniqueCitiesForActiveUsers = await User.distinct("address.city", {
  status: "active",
});
```

Returns an array of the distinct values a field takes across matching documents (a second, optional filter argument scopes which documents are considered) — useful for populating a filter dropdown in a UI, or a quick check of what values actually exist in a field.

---

## Read methods compared, by cost

| Method                   | Loads full documents?        | Use when                                        |
| ------------------------ | ---------------------------- | ----------------------------------------------- |
| `find()`                 | Yes                          | You need the actual document data               |
| `findOne()`/`findById()` | Yes (one document)           | Same, for a single expected match               |
| `countDocuments()`       | No                           | You only need a count                           |
| `exists()`               | No (just `_id`)              | You only need to know if something matches      |
| `distinct()`             | No (just the field's values) | You need the unique set of values a field takes |

A common performance mistake is defaulting to `find()` and checking `.length` or truthiness, when a lighter-weight method would do the same job with meaningfully less data transferred and processed.

---

## Read methods and `select: false` fields

```js
const userSchema = new mongoose.Schema({
  password: { type: String, select: false }, // from 04-schemas/03-schema-type-options.md
});
```

```js
const user = await User.findOne({ email });
user.password; // undefined — excluded by default, per the schema option

const userWithPassword = await User.findOne({ email }).select("+password");
```

Every read method that returns full documents respects `select: false` automatically — worth remembering when a field seems to be "missing" from a query result unexpectedly.

## Common mistakes

- **Using `find().length` (or loading full documents just to count/check existence)** instead of `countDocuments()`/`exists()` — works, but transfers and processes far more data than necessary.
- **Assuming `findById()` returns `null` for a malformed ID string** — it throws a `CastError` instead, a different failure mode that needs its own handling (`08-errors/03-cast-errors.md`).
- **Relying solely on `exists()` to prevent a duplicate**, without a unique index backing it — leaves a real race-condition window between the check and the actual write; the index is the actual guarantee, the `exists()` check is just a faster, friendlier first pass.
- **Forgetting a `select: false` field needs `.select("+field")`** to explicitly opt back in when it's genuinely needed (e.g. the login flow needing the password hash).

## Quick summary

- `find`/`findOne`/`findById` return actual Documents; `findById` casts the ID and throws a `CastError` on a structurally invalid one
- `countDocuments`/`exists`/`distinct` are lighter-weight alternatives that avoid loading full documents when you don't actually need them
- `exists()` is a fast pre-check for the common "does this already exist" pattern, but a unique index is still the real, race-condition-proof guarantee
- `select: false` fields are automatically excluded across every read method, and need explicit `.select("+field")` to include

## Next

**`03-update-methods.md`** covers every way to modify documents you've found (or without finding them at all).
