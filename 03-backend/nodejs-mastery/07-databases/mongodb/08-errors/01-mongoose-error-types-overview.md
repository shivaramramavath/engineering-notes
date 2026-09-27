# Mongoose Error Types Overview

Every distinct error type Mongoose can throw, at a glance — what triggers each, and where in this section to find the full depth on the ones that matter most.

## The core error types

| Error type                        | Thrown when                                                                                                       | Depth                                                                       |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `ValidationError`                 | A schema validation rule fails (`required`, `min`/`max`, `enum`, a custom validator)                              | `02-validation-errors.md`                                                   |
| `CastError`                       | A value can't be coerced to the field's declared schema type                                                      | `03-cast-errors.md`                                                         |
| `MongoServerError` (code `11000`) | A unique index constraint is violated                                                                             | `04-duplicate-key-errors.md`                                                |
| `DocumentNotFoundError`           | A `findOneAndUpdate`/`findOneAndDelete`-family call with `{ strict: true }`-style options finds nothing to act on | Below                                                                       |
| `VersionError`                    | Optimistic concurrency conflict — the document changed since you loaded it                                        | Below                                                                       |
| `MongooseServerSelectionError`    | Mongoose can't reach any usable MongoDB server at all                                                             | Below                                                                       |
| `OverwriteModelError`             | Calling `mongoose.model()` twice with the same name                                                               | Covered in `05-models/03-compiling-models-and-avoiding-overwrite-errors.md` |
| `MongoParseError`                 | A malformed connection string                                                                                     | Below                                                                       |

---

## `ValidationError` — schema rules violated

```js
await User.create({ email: "not-an-email" });
```

```
ValidationError: User validation failed: email: Please enter a valid email
```

Thrown by `create()`/`.save()`/`insertMany()` (and update methods **only** with `runValidators: true`, per `06-crud-methods/03-update-methods.md`) whenever a schema-declared validator rejects the data. Full structure and handling in `02-validation-errors.md`.

---

## `CastError` — data of the wrong shape entirely

```js
await User.findById("not-a-valid-objectid");
```

```
CastError: Cast to ObjectId failed for value "not-a-valid-objectid" (type string) at path "_id"
```

Thrown when Mongoose tries to coerce a value to a field's declared type and genuinely can't — different from `ValidationError`, which fires on correctly-typed data that violates a business rule. Full depth, including exactly when this fires vs. `ValidationError`, in `03-cast-errors.md`.

---

## `MongoServerError` with code `11000` — duplicate key

```js
await User.create({ email: "alice@example.com" }); // first succeeds
await User.create({ email: "alice@example.com" }); // ❌ MongoServerError
```

```
E11000 duplicate key error collection: myapp.users index: email_1 dup key: { email: "alice@example.com" }
```

This is **not** a `ValidationError` — `unique: true` (`04-schemas/03-schema-type-options.md`) is an index constraint, not a schema validator, so a violation surfaces as a raw `MongoServerError` from the database layer instead. Full depth, including parsing out which field caused it, in `04-duplicate-key-errors.md`.

---

## `DocumentNotFoundError`

```js
await User.findOneAndUpdate({ _id: userId }, { $set: { age: 31 } }).orFail();
```

```
DocumentNotFoundError: No document found for query "{ _id: ... }" on model "User"
```

By default, a `findOneAndUpdate`/`findOneAndDelete` call that matches nothing simply returns `null` — it does **not** throw. Chaining `.orFail()` onto the query changes that, throwing `DocumentNotFoundError` instead of silently returning `null`, which is often exactly what you want for an operation where "nothing matched" should be treated as an error condition (e.g. updating a specific resource by ID that the caller expects to exist) rather than something the calling code has to remember to check for separately.

```js
const user = await User.findById(userId).orFail(
  () => new NotFoundError("User not found"),
);
```

`.orFail()` optionally accepts a function to produce a custom error instead of the default `DocumentNotFoundError` — a clean way to convert "nothing found" directly into your own application error type (`06-custom-application-errors.md`).

---

## `VersionError` — optimistic concurrency conflict

```js
const user = await User.findById(userId); // __v is 0
// ...meanwhile, someone else loads and saves the same document, incrementing __v to 1...
user.name = "New Name";
await user.save(); // ❌ VersionError — the document's __v no longer matches what this instance expects
```

Only occurs if `versionKey` (`__v`) is enabled (the default) and something else modified the document between when you loaded it and when you tried to save your own change — Mongoose's way of detecting a lost-update race condition rather than silently letting one overwrite happen unnoticed. Genuinely useful for documents with meaningful concurrent-edit risk; largely irrelevant for data that's realistically only ever modified by one process/request at a time.

---

## `MongooseServerSelectionError` — can't reach the database at all

```
MongooseServerSelectionError: connect ECONNREFUSED 127.0.0.1:27017
```

A connection-level failure, not a query-level one — the database is unreachable, misconfigured, or the connection string is wrong. This is what `serverSelectionTimeoutMS` (`03-setup/02-connecting-to-mongodb.md`) governs how long Mongoose waits before actually throwing it.

---

## `MongoParseError` — a malformed connection string

```
MongoParseError: Invalid connection string
```

Thrown immediately by `mongoose.connect()` if the connection string itself is malformed — typically a typo, a missing `mongodb://`/`mongodb+srv://` prefix, or unescaped special characters in a password.

---

## Checking an error's type in code

```js
try {
  await User.create(data);
} catch (err) {
  if (err.name === "ValidationError") {
    // ...
  } else if (err.name === "CastError") {
    // ...
  } else if (err.code === 11000) {
    // note: duplicate-key errors are checked by `.code`, not `.name` — see 04-duplicate-key-errors.md
  } else {
    throw err; // something unexpected — don't silently swallow it
  }
}
```

`err.name` distinguishes most Mongoose-specific error types; the duplicate-key case is the one important exception, checked via `err.code === 11000` instead, since it's actually a raw driver-level error rather than a Mongoose-specific class.

## Common mistakes

- **Treating every caught error identically** ("just return a generic 400") — throws away the specific, often user-facing-friendly information each error type actually carries.
- **Checking `err.name` for the duplicate-key case** — it's `err.code === 11000`, not a distinctly-named Mongoose error class; a common mix-up covered fully in `04-duplicate-key-errors.md`.
- **Not knowing `findOneAndUpdate`/`findOneAndDelete` return `null` rather than throwing when nothing matches** — assuming an error would be thrown, and being surprised by a silent `null` instead, unless `.orFail()` is used deliberately.
- **Re-throwing an unrecognized error type without logging it** — an error your code doesn't specifically handle is exactly the kind of thing worth logging clearly before it propagates further.

## Quick summary

- `ValidationError` (schema rules) and `CastError` (wrong data shape) are the two most common everyday errors, both distinguishable via `err.name`
- Duplicate-key violations surface as `MongoServerError` with `err.code === 11000` — not a distinctly-named error class
- `DocumentNotFoundError` only occurs when `.orFail()` is explicitly chained — otherwise "nothing matched" is a silent `null`, not an error
- `VersionError` guards against lost-update races when `versionKey` is enabled
- Connection-level failures (`MongooseServerSelectionError`, `MongoParseError`) are distinct from query-level errors entirely

## Next

**`02-validation-errors.md`** goes deep on reading and handling a `ValidationError`'s actual structure.
