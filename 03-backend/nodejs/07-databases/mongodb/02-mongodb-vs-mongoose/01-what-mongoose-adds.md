# What Mongoose Adds

Mongoose is an **ODM** (Object Document Mapper) — a layer over the native driver that adds structure, validation, and convenience MongoDB itself doesn't enforce. This file makes concrete exactly what that buys you, and what it costs.

## 1. Schemas — structure MongoDB doesn't enforce

```js
// native driver — MongoDB accepts this without complaint
await collection.insertOne({ nam: "Alice", agee: "thirty" }); // typos, wrong types, nothing stops this
```

```js
// Mongoose — a schema defines the expected shape
const userSchema = new mongoose.Schema({
  name: String,
  age: Number,
});
const User = mongoose.model("User", userSchema);

await User.create({ nam: "Alice", agee: "thirty" });
// "nam"/"agee" are silently dropped (not in the schema); "age" is never set
```

MongoDB itself has no concept of "this collection's documents should look like X" unless you separately configure server-side JSON Schema validation. Mongoose gives every application-level interaction with a collection a defined shape, checked in your own code before anything reaches the database at all. Full depth in `04-schemas/`.

---

## 2. Casting — automatic type coercion

```js
const userSchema = new mongoose.Schema({ age: Number });
const User = mongoose.model("User", userSchema);

await User.create({ age: "30" }); // a string...
```

```js
const user = await User.findOne();
typeof user.age; // "number" — Mongoose cast "30" to 30 automatically
```

The native driver would store `"30"` as a literal string, forever. Mongoose attempts to **cast** incoming values to match the schema's declared type — genuinely convenient for things like route params and form data, which arrive as strings even when they represent numbers, but also a behavior worth understanding precisely (a `CastError` is thrown when a value truly can't be coerced — full coverage in `08-errors/03-cast-errors.md`).

---

## 3. Validation — rejecting bad data before it's written

```js
const userSchema = new mongoose.Schema({
  name: { type: String, required: true },
  email: { type: String, required: true, match: /.+@.+\..+/ },
  age: { type: Number, min: 0, max: 120 },
});
```

```js
await User.create({ name: "Alice", email: "not-an-email" });
// ValidationError — rejected before ever reaching MongoDB
```

The native driver has no concept of "required," "min/max," or a regex pattern constraint at all — it stores whatever you give it. Mongoose validates against the schema and refuses the write if it fails, throwing a structured `ValidationError` your application can catch and respond to meaningfully (`08-errors/02-validation-errors.md`, `09-validation/`).

---

## 4. Middleware (hooks) — behavior attached to the model itself

```js
userSchema.pre("save", async function (next) {
  if (this.isModified("password")) {
    this.password = await hashPassword(this.password);
  }
  next();
});
```

The native driver has no equivalent — every `insertOne` call site would need to remember to hash a password itself. Mongoose lets you attach behavior _to the schema_, guaranteeing it runs every time, regardless of which part of your codebase triggers the save. Full depth in `10-middleware-hooks/`.

---

## 5. A richer query API and relationship support

```js
const posts = await Post.find({ published: true })
  .populate("author") // fetch related documents automatically
  .sort({ createdAt: -1 })
  .limit(10);
```

`.populate()` in particular has no direct native-driver equivalent — it's Mongoose performing a `$lookup`-like operation (or a separate query) for you, based on a reference you defined in the schema. Full depth in `11-relationships/`.

---

## What this costs

None of this is free — Mongoose's layer has real trade-offs worth knowing:

### Performance overhead

```js
// native driver — sends the document essentially as-is
await collection.insertOne(doc);

// Mongoose — validates, casts, runs middleware, THEN sends to the driver
await Model.create(doc);
```

Every Mongoose operation does meaningfully more work than the equivalent native call — for most applications this is completely negligible next to actual database I/O latency, but it's not zero, and matters more at very high throughput. `.lean()` (`14-performance/02-query-optimization-and-lean.md`) exists specifically to skip most of this overhead for read-only queries that don't need Mongoose's document features.

### An abstraction that can hide what's actually happening

```js
const user = await User.findOne({ email });
```

It's easy to forget this is a real network call with real latency, because it reads so much like synchronous, in-memory JavaScript. Understanding the native driver underneath (`01-mongodb-native-driver/`) is what keeps this abstraction from becoming a black box when something behaves unexpectedly.

### Not every MongoDB feature has a clean Mongoose wrapper

Some things — certain aggregation pipeline stages, specific driver-level options, raw index management — are more naturally expressed against the native driver directly. Mongoose exposes `Model.collection` as an escape hatch precisely for this (`02-when-to-drop-to-the-native-driver.md`).

## Quick summary

- Mongoose adds four main things over the native driver: schemas (structure), casting (type coercion), validation (rejecting bad data), and middleware (attached behavior) — plus a richer query API including `.populate()`
- None of these exist in the native driver at all; MongoDB itself stores whatever shape of data it's given
- The cost is a real (if usually small) performance overhead, and a layer of abstraction that can obscure what's actually a network call underneath
- `.lean()` and dropping to `Model.collection` are the two main ways to reduce or bypass that overhead when needed

## Next

**`02-when-to-drop-to-the-native-driver.md`** covers exactly how and when to bypass Mongoose's layer without leaving Mongoose entirely.
