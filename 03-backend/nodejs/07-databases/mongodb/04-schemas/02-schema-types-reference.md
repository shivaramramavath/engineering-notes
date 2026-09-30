# Schema Types Reference

Every built-in type a schema field can be declared as, with what each one actually does and casts.

## `String`

```js
name: String;
```

```js
await User.create({ name: 42 });
// name is cast to "42"
```

Mongoose casts most primitive values to a string automatically. Combine with string-specific validators (`03-schema-type-options.md`) like `minlength`/`maxlength`/`match`/`enum`.

## `Number`

```js
age: Number;
```

```js
await User.create({ age: "30" }); // string...
user.age === 30; // ...cast to a real number
await User.create({ age: "thirty" }); // ❌ CastError — can't be coerced to a number
```

Combine with `min`/`max` for range validation.

## `Date`

```js
createdAt: Date;
```

```js
await User.create({ createdAt: "2026-01-15" }); // a string...
user.createdAt instanceof Date; // ...cast to a real Date

await Model.find({ createdAt: { $gte: new Date("2026-01-01") } }); // real date range queries
```

Maps directly onto BSON's `Date` type (`00-mongodb-basics/03-architecture-and-bson.md`) — this is what makes proper date range queries possible.

## `Boolean`

```js
isActive: Boolean;
```

```js
await User.create({ isActive: "true" }); // "true"/"false"/0/1/"yes"/"no" are all cast sensibly
```

## `Buffer`

```js
avatar: Buffer;
```

For storing raw binary data directly in a document — small images, encrypted blobs. For anything large, prefer GridFS or external object storage over embedding binary data directly.

## `ObjectId` (`mongoose.Schema.Types.ObjectId`)

```js
author: { type: mongoose.Schema.Types.ObjectId, ref: "User" }
```

The type used for references to other documents — `ref` tells `.populate()` (`11-relationships/`) which model to look the referenced document up in. Also the type used implicitly for every document's own `_id`.

## `Array`

```js
tags: [String]; // an array of strings
scores: [Number]; // an array of numbers
orders: [orderSchema]; // an array of subdocuments (04-nested-and-subdocument-schemas.md)
```

```js
await User.create({ tags: ["a", "b", 42] });
user.tags; // ["a", "b", "42"] — each element cast individually according to the array's declared type
```

## `Mixed` (`mongoose.Schema.Types.Mixed`)

```js
metadata: mongoose.Schema.Types.Mixed;
// or shorthand:
metadata: {
}
```

```js
await User.create({ metadata: { anything: "goes", nested: { too: true } } });
```

`Mixed` opts a field **out** of schema enforcement entirely — any shape of data is accepted, with no casting or validation. Useful for genuinely unpredictable data (e.g. an arbitrary third-party webhook payload you're just storing for reference), but it forfeits everything Mongoose normally provides — use it deliberately and sparingly, not as a shortcut to avoid designing a proper schema.

### A `Mixed` field's change-tracking gotcha

```js
user.metadata.newField = "value";
await user.save(); // may NOT persist — Mongoose can't automatically detect a mutation inside a Mixed field
```

```js
user.markModified("metadata"); // tell Mongoose explicitly that this field changed
await user.save(); // now it persists correctly
```

Because `Mixed` has no defined shape, Mongoose can't automatically detect when a nested property inside it changes — `markModified()` is the required workaround, and forgetting it is a common, confusing "why didn't my update save" bug specific to `Mixed` fields.

## `Map`

```js
scoresByGame: { type: Map, of: Number }
```

```js
await User.create({ scoresByGame: { chess: 1200, checkers: 900 } });
user.scoresByGame.get("chess"); // 1200
```

Useful for a genuinely dynamic set of keys where the key names themselves aren't known ahead of time (unlike a fixed set of fields, where a plain nested object/subdocument schema is more appropriate). `of` declares the type of the map's _values_ — the keys are always strings.

## `Decimal128`

```js
price: mongoose.Schema.Types.Decimal128;
```

```js
await Order.create({ price: mongoose.Types.Decimal128.fromString("19.99") });
```

For exact decimal precision — currency, anything where ordinary floating-point rounding errors matter (`00-mongodb-basics/03-architecture-and-bson.md` covers why `Decimal128` exists at the BSON level). Note that arithmetic on a `Decimal128` in JavaScript isn't as convenient as plain numbers — you typically convert for calculations and convert back for storage, or use a decimal-math library.

## `Schema.Types.UUID`

```js
sessionId: { type: mongoose.Schema.Types.UUID, default: () => crypto.randomUUID() }
```

A more recent addition for storing UUIDs as their own proper BSON binary subtype rather than as a plain string.

---

## A schema combining several types

```js
const productSchema = new mongoose.Schema({
  name: String,
  price: mongoose.Schema.Types.Decimal128,
  tags: [String],
  category: { type: mongoose.Schema.Types.ObjectId, ref: "Category" },
  metadata: mongoose.Schema.Types.Mixed,
  attributesByLocale: { type: Map, of: String },
  createdAt: Date,
  isPublished: Boolean,
});
```

## Common mistakes

- **Using `Mixed` as a default shortcut** instead of properly typing fields — forfeits casting, validation, and reliable change detection for that field.
- **Forgetting `markModified()` after mutating a nested property inside a `Mixed` field** — the mutation silently fails to persist.
- **Using a plain object/subdocument schema when a `Map` is actually appropriate** (or vice versa) — a `Map` fits genuinely dynamic key names; a subdocument/nested object fits a known, fixed set of fields.
- **Doing floating-point arithmetic on `Decimal128` values directly** without converting properly — loses the exact-precision benefit the type exists for.

## Quick summary

- `String`/`Number`/`Date`/`Boolean`/`Buffer` are the core primitives, each with their own casting behavior
- `ObjectId` (with `ref`) is how references to other documents/models are declared
- `Mixed` opts a field out of schema enforcement entirely — use sparingly, and remember `markModified()` for nested mutations
- `Map` fits dynamic, unpredictable key names; a subdocument schema fits a known, fixed shape
- `Decimal128` gives exact decimal precision for currency and similar values

## Next

**`03-schema-type-options.md`** covers the per-field options (`required`, `default`, `enum`, and more) that apply on top of these types.
