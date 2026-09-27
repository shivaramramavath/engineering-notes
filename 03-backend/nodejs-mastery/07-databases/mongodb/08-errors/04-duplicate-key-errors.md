# Duplicate Key Errors

Code `11000` — MongoDB's duplicate-key error, thrown when a write would violate a unique index. This is the error behind the "email already exists" pattern, and this file covers exactly how to read it and identify which field actually caused it.

## Triggering one

```js
const userSchema = new mongoose.Schema({
  email: { type: String, unique: true },
});
```

```js
await User.create({ email: "alice@example.com" }); // succeeds
await User.create({ email: "alice@example.com" }); // ❌ throws
```

```
MongoServerError: E11000 duplicate key error collection: myapp.users index: email_1 dup key: { email: "alice@example.com" }
```

---

## Why this is not a `ValidationError`

As established in `04-schemas/03-schema-type-options.md` and `01-mongoose-error-types-overview.md`: `unique: true` builds an **index**, it isn't a validator. The uniqueness check happens at the database level, at write time — not during Mongoose's own validation pipeline. This means:

```js
try {
  await User.create({ email: "alice@example.com" });
} catch (err) {
  err.name; // "MongoServerError" — NOT "ValidationError"
  err.code; // 11000
}
```

You must check `err.code === 11000`, not `err.name === "ValidationError"` — a very easy, very common mistake, since intuitively a "uniqueness" rule feels like it should be a validation concern.

---

## The full error object's useful properties

```js
console.log(err.code); // 11000
console.log(err.keyPattern); // { email: 1 } — WHICH field(s) the violated index covers
console.log(err.keyValue); // { email: "alice@example.com" } — the actual duplicate value
console.log(err.message); // the full raw MongoDB error string
```

`err.keyPattern` and `err.keyValue` are the genuinely useful, structured properties — no need to parse the raw message string at all in modern Mongoose/driver versions, since these are provided directly.

---

## Identifying which field caused it (for a compound unique index)

```js
orderSchema.index({ customerId: 1, orderNumber: 1 }, { unique: true });
```

```js
try {
  await Order.create({ customerId, orderNumber });
} catch (err) {
  if (err.code === 11000) {
    console.log(err.keyPattern); // { customerId: 1, orderNumber: 1 }
    console.log(err.keyValue); // { customerId: ObjectId("..."), orderNumber: "ORD-001" }
  }
}
```

For a compound unique index, `keyValue` includes **every** field in that index — telling you the exact combination that violated uniqueness, not just a single field name.

---

## Extracting the field name generically

```js
function getDuplicateField(err) {
  return Object.keys(err.keyValue)[0]; // the first (or only) field in a simple unique index
}
```

```js
try {
  await User.create({ email: "alice@example.com" });
} catch (err) {
  if (err.code === 11000) {
    const field = getDuplicateField(err); // "email"
    console.log(`${field} is already in use`);
  }
}
```

For a single-field unique index, `Object.keys(err.keyValue)[0]` reliably gives you the field name — the core mechanic behind mapping a duplicate-key error to a specific, friendly message.

---

## Multiple unique fields on the same schema

```js
const userSchema = new mongoose.Schema({
  email: { type: String, unique: true },
  username: { type: String, unique: true },
});
```

```js
try {
  await User.create({ email: "taken@example.com", username: "newuser" });
} catch (err) {
  if (err.code === 11000) {
    const field = Object.keys(err.keyValue)[0]; // "email" — specifically identifies WHICH one collided
    // NOT "username", even though both are unique on this schema
  }
}
```

This is exactly why reading `err.keyValue`/`err.keyPattern` matters rather than assuming — a schema can have several independently-unique fields, and only `err.keyValue` tells you which specific one actually caused a given failure.

---

## The index name, if you need it

```js
err.message;
// "E11000 duplicate key error collection: myapp.users index: email_1 dup key: { email: \"alice@example.com\" }"
```

The index name (`email_1`, following MongoDB's default naming convention of `field_direction`) appears in the raw message if you ever need it for logging or debugging — but for actually identifying the offending field programmatically, `err.keyValue`/`err.keyPattern` are far more reliable than parsing this string, since a custom-named index (`schema.index({...}, { name: "custom_name" })`) wouldn't follow the default naming pattern at all.

---

## Duplicate-key errors during `insertMany`/`bulkWrite`

```js
try {
  await User.insertMany(
    [{ email: "a@example.com" }, { email: "a@example.com" }],
    { ordered: false },
  );
} catch (err) {
  err.writeErrors; // an ARRAY of individual write errors, one per failed document
}
```

For a batch operation with `ordered: false` (`06-crud-methods/01-create-methods.md`), a duplicate-key failure on one document doesn't throw a single top-level error the same way — instead, the thrown error's `writeErrors` array contains the individual failures, each with its own `code`/`keyValue`, since other documents in the batch may have succeeded.

## Common mistakes

- **Checking `err.name === "ValidationError"` for a duplicate-key error** — it's `err.code === 11000` on a `MongoServerError`; a genuinely common and easy mix-up.
- **Assuming there's only one unique field, and hardcoding which field name a duplicate-key error refers to** — always read `err.keyValue`/`err.keyPattern` to identify the actual field(s), especially on a schema with multiple unique fields.
- **Parsing the raw `err.message` string** to extract the field name — unreliable, and unnecessary given `err.keyValue` is provided directly and structured.
- **Not handling `insertMany`/`bulkWrite`'s different error shape** (`err.writeErrors`, an array) — the single-operation `err.code === 11000` check doesn't directly apply the same way to a batch failure.

## Quick summary

- Duplicate-key violations are `MongoServerError`s with `err.code === 11000` — not a `ValidationError`, since `unique` is an index constraint, not a schema validator
- `err.keyValue` and `err.keyPattern` tell you exactly which field(s) and value(s) caused the violation — no need to parse the raw message string
- A schema can have multiple independently-unique fields; always check `keyValue` to know which one actually failed for a given error
- Batch operations (`insertMany`/`bulkWrite` with `ordered: false`) report duplicate-key failures differently, via a `writeErrors` array

## Next

**`05-turning-errors-into-friendly-responses.md`** puts this together into the complete pattern: catching this error and responding with a clean "email already exists" message.
