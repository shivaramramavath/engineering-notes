# Validating Updates

The single most commonly-missed validation gotcha in Mongoose: `updateOne`, `updateMany`, `findOneAndUpdate`, and `findByIdAndUpdate` **do not run your schema's validators by default.** This file covers exactly why, and how to fix it correctly and consistently.

## The gotcha, demonstrated

```js
const userSchema = new mongoose.Schema({
  email: { type: String, required: true, match: /.+@.+\..+/ },
  age: { type: Number, min: 0, max: 120 },
});
```

```js
await User.updateOne({ _id: userId }, { $set: { age: -50 } });
```

```js
{ acknowledged: true, matchedCount: 1, modifiedCount: 1 }
```

**No error.** The document now has `age: -50` in the database, despite the schema explicitly declaring `min: 0` — a rule that `create()`/`.save()` would have enforced without question.

---

## Why this happens

Mongoose's update methods (`updateOne`, `updateMany`, `findOneAndUpdate`, `findByIdAndUpdate`) translate directly into a native MongoDB update command (`06-crud-methods/03-update-methods.md`) — by default, they operate closer to the raw driver level and skip the full document-construction-and-validation pipeline that `create()`/`new Model().save()` go through. This is a deliberate (if commonly surprising) design choice, largely for performance and flexibility reasons — validating a partial `$set` update against a full document schema is a genuinely more complex operation than validating a complete new document.

---

## The fix: `runValidators: true`

```js
await User.updateOne(
  { _id: userId },
  { $set: { age: -50 } },
  { runValidators: true },
);
```

```js
// ❌ now correctly throws
ValidationError: Validation failed: age: Path `age` (-50) is less than minimum allowed value (0).
```

This single option is the fix — always include it on any update operation where the update could plausibly introduce invalid data (which, in practice, is most of them).

---

## `runValidators` with `findOneAndUpdate`/`findByIdAndUpdate`

```js
const user = await User.findByIdAndUpdate(
  userId,
  { $set: { age: -50 } },
  { new: true, runValidators: true },
);
```

The near-universal, recommended combination for these two methods: `{ new: true, runValidators: true }` together — `new: true` so you get the updated document back (per `06-crud-methods/03-update-methods.md`'s other gotcha), `runValidators: true` so the update is actually checked against the schema.

---

## A subtlety: `required` validators and partial updates

```js
const userSchema = new mongoose.Schema({
  name: { type: String, required: true },
  email: { type: String, required: true },
});
```

```js
await User.updateOne(
  { _id: userId },
  { $set: { email: "new@example.com" } },
  { runValidators: true },
);
// ✅ succeeds — even though "name" isn't included in this $set at all
```

With `runValidators: true`, Mongoose only validates the fields **actually being modified** by this specific update — it does **not** re-validate the entire existing document against every `required` rule. So updating just `email` doesn't fail even though `name` isn't part of the update, since `name` presumably already has a valid value from when the document was originally created. This is different from `.save()`'s full-document validation and is worth understanding clearly, since it means `runValidators: true` protects the fields you're changing, not the document as a whole.

---

## Context for cross-field/custom validators: `context: "query"`

```js
const eventSchema = new mongoose.Schema({
  startDate: Date,
  endDate: {
    type: Date,
    validate: {
      validator: function (value) {
        return value > this.startDate; // `this` — but what is `this` during an UPDATE?
      },
    },
  },
});
```

```js
await Event.updateOne(
  { _id: eventId },
  { $set: { endDate: new Date("2026-01-01") } },
  { runValidators: true, context: "query" },
);
```

During a `create()`/`.save()`, `this` inside a custom validator refers to the document being saved. During an **update** with `runValidators: true`, there's no full document being constructed the same way — `this` by default refers to the underlying `Query`, not a document, which breaks cross-field validators expecting `this.otherField` to work the same way it does on create. `context: "query"` is required to make `this` behave as expected inside a validator during an update — and even then, accessing a field _not_ included in the current update's `$set` (like `this.startDate`, if only `endDate` is being updated) may not reflect the document's actual current value, since the update might not have loaded the full document at all. This is a genuinely subtle area — cross-field custom validators on update-heavy schemas are one of the more finicky corners of Mongoose, and worth testing carefully rather than assuming they behave identically to the create-time case.

---

## `upsert` and `setDefaultsOnInsert`

```js
await User.updateOne(
  { email: "new@example.com" },
  { $set: { name: "Alice" } },
  { upsert: true, runValidators: true, setDefaultsOnInsert: true },
);
```

Covered in `06-crud-methods/03-update-methods.md` — worth repeating here since it's part of the same "make sure an update behaves like a create would" family of options: `setDefaultsOnInsert: true` ensures schema defaults apply if the upsert actually creates a new document, the same way `create()` would apply them.

---

## A checklist for update calls

```js
await Model.updateOne(filter, update, {
  runValidators: true, // ✅ actually validate the update
  new: true, // ✅ (findOneAndUpdate/findByIdAndUpdate only) return the updated document
  context: "query", // ✅ needed if any custom validators reference `this`
  setDefaultsOnInsert: true, // ✅ if also using upsert: true
});
```

Not every option is needed on every call — but `runValidators: true` specifically is worth defaulting to including, as a matter of habit, on essentially every update that touches user-influenced data.

## Common mistakes

- **The core mistake this entire file exists to prevent: forgetting `runValidators: true`** — silently allows invalid data through any update path, even on a schema with carefully-defined validation rules that work perfectly on `create()`.
- **Assuming `runValidators: true` re-validates the whole document against every `required` rule** — it only validates the fields actually being changed by that update.
- **Expecting a cross-field custom validator to work identically on update as on create**, without `context: "query"` — and even then, being surprised when `this.otherField` doesn't reflect what you expect, since the full document may not be loaded during an update.
- **Relying on `.save()` everywhere specifically to avoid this gotcha**, at the cost of an unnecessary extra round trip (load, then save) for changes that a direct, correctly-configured `updateOne` could handle just as safely and more efficiently.

## Quick summary

- `updateOne`/`updateMany`/`findOneAndUpdate`/`findByIdAndUpdate` skip schema validation entirely **unless** `{ runValidators: true }` is explicitly passed — the single most important thing to remember about Mongoose updates
- `runValidators: true` only validates the fields being changed by that specific update, not the entire document against every rule
- Cross-field custom validators referencing `this` need `context: "query"` during an update, and even then behave subtly differently than during a create
- `{ new: true, runValidators: true }` is the standard, recommended pairing for `findOneAndUpdate`/`findByIdAndUpdate`; add `setDefaultsOnInsert: true` too when combined with `upsert: true`

## Section complete

That covers validation in full depth — built-in validators, custom validators (including cross-field), async validators, and the critical update-validation gotcha. **`10-middleware-hooks`** covers the other major way Mongoose attaches behavior to the document lifecycle: `pre`/`post` hooks.
