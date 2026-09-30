# Create Methods

Every way to create documents in Mongoose — `Model.create()`, `new Model().save()`, and `insertMany()` — and when each one is the right choice.

## `Model.create()` — the most common way

```js
const user = await User.create({ name: "Alice", email: "alice@example.com" });
```

```js
user._id; // auto-generated ObjectId
user.name; // "Alice"
user instanceof mongoose.Document; // true — a full Document, not a plain object
```

Under the hood, `create()` is a convenience shortcut equivalent to:

```js
const user = new User({ name: "Alice", email: "alice@example.com" });
await user.save();
```

It runs full schema validation and casting, then saves in one call — the right default choice whenever you don't need to inspect or modify the document between construction and saving.

### Creating multiple documents at once

```js
const users = await User.create([{ name: "Alice" }, { name: "Bob" }]);
```

```js
users; // an array of Documents, in the same order as the input
```

Passing an array runs `create()` for each document, but **not** as a single atomic batch by default (unlike `insertMany`, below) — each document is validated and saved individually.

---

## `new Model() + .save()` — when you need the document before persisting it

```js
const user = new User({ name: "Alice", email: "alice@example.com" });

user.name = user.name.trim(); // modify before saving
if (someCondition) {
  user.role = "admin";
}

await user.save();
```

Use this whenever your code needs to inspect, validate, or conditionally modify the document **before** it's actually written — `Model.create()` doesn't give you that window, since construction and saving happen in one call.

### `.save()` also handles updates to an existing document

```js
const user = await User.findById(userId);
user.age = 31;
await user.save(); // Mongoose knows this is an update, not a new insert, because the document already has an _id and isn't new
```

Covered fully in `03-update-methods.md` — `.save()` is a dual-purpose method: create for a new document, update for an existing one, based on the document's `isNew` state (`05-models/02-model-vs-document.md`).

### Handling a validation failure

```js
try {
  await user.save();
} catch (err) {
  if (err.name === "ValidationError") {
    // full coverage in 08-errors/02-validation-errors.md
  }
  throw err;
}
```

---

## `insertMany()` — efficient bulk creation

```js
await User.insertMany([{ name: "Alice" }, { name: "Bob" }, { name: "Carol" }]);
```

Unlike looping `create()` calls (or passing an array to `create()`), `insertMany` sends all the documents to MongoDB in a **single, more efficient batch operation** — the right choice specifically for bulk creation where you don't need per-document, one-at-a-time processing.

### Validation still runs — but per document, and reported together

```js
await User.insertMany([
  { name: "Alice", age: 30 },
  { age: "not a number" }, // fails casting/validation
]);
```

By default, `insertMany` is **ordered**: it validates and inserts documents in order, stopping at the first failure — documents after the failed one are never attempted, same as the native driver behavior (`01-mongodb-native-driver/02-native-crud-methods.md`).

### Unordered inserts — keep going despite failures

```js
await User.insertMany(documents, { ordered: false });
```

Attempts every document regardless of individual failures, reporting all errors together at the end — the right choice when documents are independent and you want to maximize successful inserts.

### Skipping validation entirely (rare, deliberate use only)

```js
await User.insertMany(documents, { validateBeforeSave: false });
```

Bypasses Mongoose's validation pipeline for the batch — occasionally used for large volumes of already-trusted, pre-validated data where the validation overhead genuinely matters (the same trade-off discussed in `02-mongodb-vs-mongoose/02-when-to-drop-to-the-native-driver.md`). Not a default to reach for casually, since it removes exactly the safety net Mongoose exists to provide.

### `insertMany` and middleware

```js
userSchema.pre("save", function (next) {
  /* ... */ next();
});
```

**`insertMany` does not trigger `pre("save")`/`post("save")` document middleware** — this is a real, commonly-missed gap. If a schema relies on `pre("save")` for something essential (like hashing a password, `10-middleware-hooks/03-common-hook-patterns.md`), documents created via `insertMany` will skip that logic entirely. Use `create()` (which does trigger save middleware) instead, if that behavior matters for the data being inserted.

---

## Comparing the three

|                      | Runs `pre`/`post("save")`? | Efficient for bulk?             | Returns Documents?                 |
| -------------------- | -------------------------- | ------------------------------- | ---------------------------------- |
| `Model.create()`     | Yes                        | No — one operation per document | Yes                                |
| `new Model().save()` | Yes                        | No — one at a time              | Yes                                |
| `insertMany()`       | **No**                     | Yes — single batch operation    | Yes (unless `lean` option is used) |

## Common mistakes

- **Using `insertMany` for data that depends on `pre("save")` middleware** — the middleware silently doesn't run, which is especially dangerous for something like password hashing.
- **Looping individual `create()` calls for a large batch** instead of using `insertMany` — works, but far less efficient due to per-document round trips.
- **Not handling the ordered-vs-unordered distinction deliberately** — the default (`ordered: true`) stops the whole batch at the first failure, which may not be the desired behavior for independent documents.
- **Reaching for `validateBeforeSave: false` on user-supplied data** — appropriate only for already-trusted data; using it to sidestep a validation error on real input defeats the schema's purpose.

## Quick summary

- `Model.create()` is the standard, most common way to create one or more documents, running full validation and save middleware
- `new Model() + .save()` is the same underlying mechanism, but gives you a window to inspect/modify the document before persisting it
- `insertMany()` is the efficient choice for bulk creation, but **skips `pre`/`post("save")` middleware** — a real, easy-to-miss gap
- `{ ordered: false }` on `insertMany` keeps going past individual failures instead of stopping at the first one

## Next

**`02-read-methods.md`** covers every way to read documents back out.
