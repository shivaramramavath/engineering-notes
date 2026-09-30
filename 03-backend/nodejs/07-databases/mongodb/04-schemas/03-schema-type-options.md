# Schema Type Options

Every field can carry options beyond just its type — constraints, defaults, and metadata that shape how Mongoose validates, indexes, and populates that field. This file covers each one in depth.

## `required`

```js
name: { type: String, required: true }
```

```js
await User.create({}); // ValidationError — "name" is required
```

### With a custom message

```js
name: { type: String, required: [true, "Name is required"] }
```

### Conditional requirement

```js
email: {
  type: String,
  required: function () {
    return this.signupMethod === "email";   // only required for email-based signups, not OAuth
  },
}
```

A function lets `required` depend on other fields on the same document — common for fields that are only mandatory under certain conditions.

---

## `default`

```js
role: { type: String, default: "user" }
createdAt: { type: Date, default: Date.now }     // note: Date.now, not Date.now() — see below
tags: { type: [String], default: [] }
```

### `Date.now` vs `Date.now()` — a common mistake

```js
createdAt: { type: Date, default: Date.now }     // ✅ Mongoose calls this function at document creation time
createdAt: { type: Date, default: Date.now() }    // ❌ evaluated ONCE, when the schema is defined —
                                                      // every document gets the SAME frozen timestamp
```

Passing a function reference (`Date.now`) lets Mongoose call it fresh for every new document; calling it immediately (`Date.now()`) bakes in a single timestamp at schema-definition time, shared incorrectly by every document ever created from that schema.

### A default that depends on another field

```js
displayName: {
  type: String,
  default: function () {
    return this.email?.split("@")[0];
  },
}
```

---

## `enum` — restricting to a fixed set of values

```js
status: { type: String, enum: ["pending", "active", "suspended"] }
```

```js
await User.create({ status: "banned" }); // ValidationError — "banned" isn't in the enum
```

### With a custom message

```js
status: {
  type: String,
  enum: {
    values: ["pending", "active", "suspended"],
    message: "{VALUE} is not a valid status",
  },
}
```

`{VALUE}` is a Mongoose-specific placeholder, automatically replaced with the actual invalid value in the resulting error message.

---

## `min` / `max` — numeric and date range validation

```js
age: { type: Number, min: 0, max: 120 }
eventDate: { type: Date, min: "2020-01-01" }
```

```js
await User.create({ age: -5 }); // ValidationError
```

### With custom messages

```js
age: { type: Number, min: [0, "Age cannot be negative"], max: [120, "Age seems unrealistic"] }
```

---

## `minlength` / `maxlength` — string length validation

```js
username: { type: String, minlength: 3, maxlength: 20 }
```

---

## `match` — regex validation

```js
email: { type: String, match: [/.+@.+\..+/, "Please enter a valid email"] }
```

Validates the field's string value against a regular expression — a common, if fairly loose, first-pass email format check (real email validation is famously hard to do perfectly with regex alone; this catches obvious mistakes, not every invalid address).

---

## `unique` — an important clarification

```js
email: { type: String, unique: true }
```

**This is not a validator** — it doesn't run as part of Mongoose's validation pipeline (`09-validation/`) at all. `unique: true` is actually an instruction to build a **unique index** on this field at the MongoDB level; a violation produces a duplicate-key error (`08-errors/04-duplicate-key-errors.md`), not a `ValidationError`. This distinction matters a lot in practice:

```js
await User.create({ email: "taken@example.com" }); // first one succeeds
await User.create({ email: "taken@example.com" }); // ❌ MongoServerError, code 11000 — NOT a ValidationError
```

Handling this correctly (catching code `11000` specifically, and turning it into a clean "email already in use" response) is covered in full in `08-errors/04-duplicate-key-errors.md` and `08-errors/05-turning-errors-into-friendly-responses.md` — exactly the "email duplicate → user already exists" pattern.

### The index has to actually exist for `unique` to work

```js
email: { type: String, unique: true }
```

Declaring `unique: true` in a schema tells Mongoose to _build_ the index — but if documents violating uniqueness already existed in the collection before the index was created, or if the index build hasn't completed yet (relevant right after deploying a schema change to an existing, populated collection), uniqueness won't actually be enforced until the index exists and is valid. Worth confirming in `mongosh`/Compass (`00-mongodb-basics/02-shell-and-compass.md`) that the index actually exists as expected, especially after a schema change.

---

## `index` — a plain (non-unique) index

```js
email: { type: String, index: true }
```

Speeds up queries filtering/sorting on this field, without enforcing uniqueness. Full depth, including compound indexes spanning multiple fields, in `07-indexes-in-schemas.md`.

---

## `select` — excluding a field from query results by default

```js
password: { type: String, required: true, select: false }
```

```js
const user = await User.findOne({ email });
console.log(user.password); // undefined — excluded by default

const userWithPassword = await User.findOne({ email }).select("+password"); // explicitly opt back in
```

An extremely common, important pattern for sensitive fields — `select: false` means a plain `find()`/`findOne()` never returns the password hash at all, unless a specific query explicitly asks for it (e.g. the login flow, which needs the hash to compare against). Far safer than remembering to manually strip the field from every response.

---

## `immutable` — preventing a field from being changed after creation

```js
createdBy: { type: mongoose.Schema.Types.ObjectId, ref: "User", immutable: true }
```

```js
user.createdBy = someoneElse;
await user.save(); // the change is silently ignored — createdBy keeps its original value
```

Useful for fields that should be set once and never modified again (an audit "created by" field, a signup date).

---

## `get` / `set` — transforming a value on read/write

```js
price: {
  type: Number,
  get: (v) => (v / 100).toFixed(2),   // stored in cents, displayed as dollars
  set: (v) => Math.round(v * 100),     // convert dollars to cents on the way in
}
```

```js
const product = new Product({ price: 19.99 });
product.price; // stored internally as 1999 (cents)... but getters require { getters: true } to apply on read
// by default when accessing the property directly — see the schema-level toJSON option
// in 05-schema-options.md for making this apply automatically on serialization
```

A specialized option, most commonly used for exactly this kind of unit conversion (storing money as integer cents to avoid floating-point issues, while working in a more human-friendly unit at the application boundary).

---

## Combining several options on one field

```js
email: {
  type: String,
  required: [true, "Email is required"],
  unique: true,
  lowercase: true,     // normalizes the value automatically before saving
  trim: true,            // strips leading/trailing whitespace automatically
  match: [/.+@.+\..+/, "Please enter a valid email"],
  select: false,
}
```

`lowercase`/`uppercase`/`trim` are simple string-normalization options worth knowing about too — applied automatically before validation runs, which is exactly why normalizing an email's case _before_ checking uniqueness matters (`"Alice@Example.com"` and `"alice@example.com"` should almost certainly be treated as the same address).

## Common mistakes

- **Writing `default: Date.now()` instead of `default: Date.now`** — bakes in one frozen timestamp shared by every document, instead of a fresh one per document.
- **Treating `unique: true` as a validator** — it's an index constraint enforced by MongoDB, producing a duplicate-key error, not a `ValidationError`; they need to be caught and handled differently.
- **Forgetting `select: false` on sensitive fields** like password hashes — they'll be included in every query result by default otherwise.
- **Not normalizing case/whitespace (`lowercase`/`trim`) before enforcing uniqueness** — allows near-duplicate values that differ only in case or stray whitespace to slip past a unique index.

## Quick summary

- `required`/`enum`/`min`/`max`/`minlength`/`maxlength`/`match` are true validators, checked during Mongoose's validation pipeline
- `unique` is **not** a validator — it's an index-level constraint, producing a different kind of error entirely (`08-errors/04-duplicate-key-errors.md`)
- `select: false` keeps a field out of query results by default — the standard way to protect password hashes and similar sensitive fields
- `immutable` locks a field after creation; `lowercase`/`trim` normalize a string field automatically before it's saved
- Always pass `Date.now` (the function reference), never `Date.now()` (its immediately-evaluated result), as a default

## Next

**`04-nested-and-subdocument-schemas.md`** covers embedding one schema's shape inside another.
