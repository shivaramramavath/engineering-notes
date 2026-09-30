# Update Methods

Every way to modify existing documents — direct collection-level updates, atomic find-and-update variants, and updating via a loaded Document's `.save()`.

## `updateOne()` — update the first match, without loading it

```js
await User.updateOne({ email: "alice@example.com" }, { $set: { age: 31 } });
```

```js
{ acknowledged: true, matchedCount: 1, modifiedCount: 1 }
```

The most efficient way to change a field — one round trip, no document ever loaded into memory. Returns only counts, not the actual document. Uses the same update operators (`$set`, `$inc`, `$push`, etc.) covered in the MongoDB fundamentals section.

---

## `updateMany()` — update every match

```js
await User.updateMany({ status: "pending" }, { $set: { status: "active" } });
```

Same shape as `updateOne`, applied to every matching document rather than just the first.

---

## `findOneAndUpdate()` — atomic update, returns the document

```js
const user = await User.findOneAndUpdate(
  { email: "alice@example.com" },
  { $set: { age: 31 } },
  { new: true },
);
```

```js
user; // the actual updated Document — not just a count
```

### The `{ new: true }` option — easy to forget

```js
const user = await User.findOneAndUpdate(filter, update); // returns the document BEFORE the update, by default
const user = await User.findOneAndUpdate(filter, update, { new: true }); // returns the document AFTER the update
```

Mongoose's default (`new: false`) returns the **pre-update** version of the document — a common point of confusion for anyone expecting to see their change reflected immediately in the return value. Always add `{ new: true }` when you actually want the updated state back.

---

## `findByIdAndUpdate()` — the `_id`-specific shortcut

```js
const user = await User.findByIdAndUpdate(
  userId,
  { $set: { age: 31 } },
  { new: true },
);
```

Exactly `findOneAndUpdate({ _id: userId }, ...)`, same `{ new: true }` caveat applies.

---

## Running validators on update — the other easy-to-forget option

```js
const userSchema = new mongoose.Schema({
  age: { type: Number, min: 0, max: 120 },
});
```

```js
await User.updateOne({ _id: userId }, { $set: { age: -5 } });
// ⚠️ succeeds by default — update operations do NOT run schema validators unless told to
```

```js
await User.updateOne(
  { _id: userId },
  { $set: { age: -5 } },
  { runValidators: true },
);
// ❌ now correctly throws a ValidationError
```

This is one of the single most surprising Mongoose defaults: **`updateOne`/`updateMany`/`findOneAndUpdate`/`findByIdAndUpdate` do not run your schema's validators unless you explicitly pass `{ runValidators: true }`.** Without it, an update can silently write data that would have been rejected outright if it had gone through `create()`/`.save()` instead. Full depth on this specific gotcha in `09-validation/04-validating-updates.md` — it's important enough to be worth internalizing here too.

```js
await User.findByIdAndUpdate(
  userId,
  { $set: { age: -5 } },
  { new: true, runValidators: true },
);
```

Combining `new: true` and `runValidators: true` together is an extremely common, near-default pairing for any `findOneAndUpdate`/`findByIdAndUpdate` call.

---

## Updating via a loaded Document's `.save()`

```js
const user = await User.findById(userId);
user.age = 31;
await user.save(); // full validation runs automatically, and pre/post("save") middleware fires
```

Unlike the direct update methods above, `.save()` **always** runs validators and **always** triggers `pre("save")`/`post("save")` middleware — no option needed to opt in. The trade-off is an extra round trip (load, then save) compared to a direct `updateOne`.

---

## Comparing the update approaches

|                                        | Round trips | Returns the document?     | Runs validators by default?          | Runs save middleware? |
| -------------------------------------- | ----------- | ------------------------- | ------------------------------------ | --------------------- |
| `updateOne`/`updateMany`               | 1           | No, just counts           | **No** — needs `runValidators: true` | No                    |
| `findOneAndUpdate`/`findByIdAndUpdate` | 1           | Only with `{ new: true }` | **No** — needs `runValidators: true` | No                    |
| `findById()` + mutate + `.save()`      | 2           | Yes, always               | Yes, always                          | Yes, always           |

This table is worth memorizing — it's the source of two of the most common Mongoose surprises (silently unvalidated updates, and stale-looking `findOneAndUpdate` results) covered above.

---

## Upserts

```js
await User.updateOne(
  { email: "alice@example.com" },
  { $set: { name: "Alice", lastLoginAt: new Date() } },
  { upsert: true },
);
```

Inserts a new document if nothing matches the filter, updates if something does — same mechanism as the native driver, works identically across `updateOne`/`findOneAndUpdate`.

```js
await User.findOneAndUpdate(
  { email },
  { $set: { lastLoginAt: new Date() } },
  { upsert: true, new: true, setDefaultsOnInsert: true }, // apply schema defaults on the INSERT branch too
);
```

`setDefaultsOnInsert: true` ensures that if the upsert actually creates a new document, any schema-level `default` values (`04-schemas/03-schema-type-options.md`) are applied — without it, an upsert-triggered insert can skip your schema's defaults entirely.

---

## Array update operators, via Mongoose

```js
await User.updateOne({ _id: userId }, { $push: { tags: "vip" } });

await User.updateOne(
  { _id: userId, "orders.sku": "ABC123" },
  { $set: { "orders.$.quantity": 5 } }, // positional operator — first matching array element
);

await User.updateMany(
  {},
  { $set: { "items.$[elem].discounted": true } },
  { arrayFilters: [{ "elem.price": { $gt: 100 } }] },
);
```

Same operators and mechanics covered in the MongoDB fundamentals section — Mongoose doesn't change the syntax, just adds the casting/validation layer around it (when `runValidators` is enabled).

## Common mistakes

- **Assuming update methods validate by default** — they don't; always add `{ runValidators: true }` when the update could plausibly introduce invalid data.
- **Forgetting `{ new: true }`** and being confused why `findOneAndUpdate`'s return value doesn't reflect the change just made.
- **Using `.save()` for a simple, single-field change when a direct `updateOne` would do** — an unnecessary extra round trip if middleware/full validation of the whole document isn't actually needed for that particular change.
- **Upserting without `setDefaultsOnInsert: true`** — a new document created via upsert can silently skip schema defaults that a normal `create()` would have applied.

## Quick summary

- `updateOne`/`updateMany` are fast and don't load a document, but **skip validators by default** — always add `runValidators: true` when needed
- `findOneAndUpdate`/`findByIdAndUpdate` return the document, but return the **pre-update** version unless `{ new: true }` is passed
- `.save()` on a loaded Document always validates and always runs save middleware, at the cost of an extra round trip
- `{ upsert: true, setDefaultsOnInsert: true }` is the combination needed for an upsert's insert branch to correctly apply schema defaults

## Next

**`04-delete-methods.md`** covers removing documents — the last of the four core operations.
