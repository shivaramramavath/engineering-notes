# Schema Options

Options passed as the **second** argument to `new mongoose.Schema(fields, options)` — configuring the schema as a whole, rather than any individual field.

```js
const userSchema = new mongoose.Schema(
  { name: String, email: String },
  { timestamps: true }, // ← schema-level options go here
);
```

## `timestamps` — automatic `createdAt`/`updatedAt`

```js
const userSchema = new mongoose.Schema({ name: String }, { timestamps: true });
```

```js
const user = await User.create({ name: "Alice" });
user.createdAt; // set automatically
user.updatedAt; // set automatically, and updated on every subsequent save
```

Almost every real schema uses this — without it, tracking when a document was created/last modified means manually adding and maintaining those fields yourself. Custom field names are supported too:

```js
{ timestamps: { createdAt: "created_at", updatedAt: "updated_at" } }
```

---

## `toJSON` / `toObject` — shaping serialized output

By default, a Mongoose document's JSON representation includes Mongoose-specific internals (`__v`, and `_id` as an `ObjectId` rather than a plain string) that often aren't what you want to actually send to an API client.

```js
const userSchema = new mongoose.Schema(
  { name: String, password: String },
  {
    toJSON: {
      transform: (doc, ret) => {
        delete ret.password; // never expose this, even if somehow queried
        delete ret.__v;
        ret.id = ret._id; // add a cleaner "id" alias
        delete ret._id;
        return ret;
      },
    },
  },
);
```

```js
res.json(user); // Express calls JSON.stringify → triggers toJSON → the transformed shape is what's actually sent
```

This runs automatically whenever a document is serialized to JSON (including via `res.json()` in Express, since that calls `JSON.stringify` under the hood, which Mongoose documents hook into via their own `toJSON` method) — a reliable, centralized place to strip sensitive fields from every API response involving this model, rather than remembering to do it manually at every route.

### `virtuals: true` — including virtual properties in output

```js
{
  toJSON: {
    virtuals: true;
  }
}
```

By default, virtuals (`06-schema-methods-statics-virtuals.md`) are computed and accessible on a document instance but **not** included when converting to JSON/a plain object — this option includes them.

---

## `versionKey` — the `__v` field

```js
const userSchema = new mongoose.Schema({ name: String }, { versionKey: false });
```

Mongoose adds a `__v` field to every document by default, used internally for optimistic concurrency control (detecting when a document was modified by someone else between when you loaded it and when you tried to save your own changes). Disabling it (`versionKey: false`) is common when you don't need that protection and just want cleaner documents — but understand what you're giving up: without it, two concurrent updates to the same document can silently overwrite each other's changes instead of one being detected and rejected.

---

## Collection naming

```js
mongoose.model("User", userSchema); // → collection "users" (pluralized, lowercased)
mongoose.model("Category", schema, "categories"); // explicit collection name, third argument
```

```js
const userSchema = new mongoose.Schema(
  { name: String },
  { collection: "app_users" }, // or set it in schema options instead
);
```

Useful when the automatic pluralization doesn't produce the name you want (irregular plurals, or matching an existing collection's name from a pre-existing database).

---

## `strict` mode

```js
const userSchema = new mongoose.Schema(
  { name: String },
  { strict: true }, // the default
);

await User.create({ name: "Alice", extraField: "ignored" });
// extraField is silently dropped, per strict mode's default behavior
```

`strict: true` (the default) is exactly the "undeclared fields are dropped" behavior from `01-defining-a-schema.md`. Setting it to `false` allows arbitrary extra fields to actually be saved, even though they're not in the schema — rarely the right choice for application code, since it defeats much of the point of defining a schema at all, but occasionally useful when working with a genuinely dynamic, evolving external data source.

```js
{
  strict: "throw";
} // instead of silently dropping, THROW an error on an undeclared field
```

`"throw"` is a useful middle ground during development — surfacing a typo'd field name immediately as an error, rather than silently discarding it and leaving you wondering why the data didn't save.

---

## `id` — the virtual `id` getter

```js
{
  id: true;
} // the default
```

By default, Mongoose adds a virtual `id` getter that returns `_id.toString()` — convenient for code that prefers working with a plain string ID rather than an `ObjectId` object directly.

```js
user.id; // "64f1a2b3c4d5e6f7a8b9c0d1" (string)
user._id; // ObjectId("64f1a2b3c4d5e6f7a8b9c0d1")
```

---

## Combining schema options in practice

```js
const userSchema = new mongoose.Schema(
  {
    name: String,
    email: { type: String, select: false === false },
    password: { type: String, select: false },
  },
  {
    timestamps: true,
    versionKey: false,
    toJSON: {
      virtuals: true,
      transform: (doc, ret) => {
        delete ret.password;
        delete ret._id;
        return ret;
      },
    },
  },
);
```

A realistic, common combination: automatic timestamps, no `__v` clutter, virtuals included in JSON output, and a transform that guarantees sensitive fields never leak into a serialized response regardless of how the document was queried.

## Common mistakes

- **Not setting `timestamps: true`** and manually maintaining `createdAt`/`updatedAt` fields instead — more code, more chances to forget updating `updatedAt` on a change.
- **Relying only on `select: false` (a per-field option) without also stripping the field in `toJSON`** — `select: false` only affects _queries_; if a password hash is ever loaded explicitly (e.g. during login), it needs the `toJSON` transform as a second layer of defense before that document could ever accidentally be sent in a response.
- **Disabling `versionKey` without understanding what optimistic concurrency control was protecting against** — fine for many apps, but a real trade-off for anything with meaningful concurrent-write risk.
- **Using `strict: false`** as a workaround for forgetting to declare a field, rather than actually adding it to the schema — reintroduces exactly the lack of structure Mongoose exists to prevent.

## Quick summary

- `timestamps: true` is close to a default-on convention — automatic `createdAt`/`updatedAt`
- `toJSON`'s `transform` is the reliable, centralized place to strip sensitive fields from every serialized response for a model
- `versionKey: false` removes `__v` but gives up Mongoose's built-in optimistic concurrency protection
- `strict` (default `true`) drops undeclared fields silently; `"throw"` is a useful stricter alternative during development
- The virtual `id` getter (`_id.toString()`) is on by default, for convenience when a plain string ID is preferred

## Next

**`06-schema-methods-statics-virtuals.md`** covers attaching custom behavior directly to a schema — including the virtuals mentioned in this file's `toJSON` discussion.
