# Cast Errors

`CastError` — thrown when Mongoose tries to coerce a value into a field's declared schema type and genuinely can't. This file covers exactly when it fires, how it differs from `ValidationError`, and how to handle it.

## The core distinction: wrong shape vs. wrong rule

```js
const userSchema = new mongoose.Schema({
  age: { type: Number, min: 0, max: 120 },
});
```

```js
await User.create({ age: "thirty" }); // ❌ CastError — "thirty" cannot become a number AT ALL
await User.create({ age: -5 }); // ❌ ValidationError — -5 IS a valid number, it just violates min: 0
```

This is the single most important thing to internalize about `CastError`: it fires when a value **cannot be converted** to the declared type in the first place; `ValidationError` fires when a value **can** be converted, but then fails a rule (`required`, `min`/`max`, `enum`, etc.) checked afterward. They're fundamentally different failure stages — casting happens first, validation happens second, and only on values that survived casting.

---

## The most common trigger: an invalid `ObjectId`

```js
await User.findById("not-a-valid-id");
```

```
CastError: Cast to ObjectId failed for value "not-a-valid-id" (type string) at path "_id" for model "User"
```

Extremely common in practice, since IDs frequently arrive as raw strings from route parameters:

```js
app.get("/users/:id", async (req, res, next) => {
  try {
    const user = await User.findById(req.params.id); // req.params.id is always a string
    if (!user) return res.status(404).json({ error: "User not found" });
    res.json(user);
  } catch (err) {
    if (err.name === "CastError") {
      return res.status(400).json({ error: "Invalid user ID format" });
    }
    next(err);
  }
});
```

Note the distinction this handler makes: a **malformed** ID string (not even a valid `ObjectId` shape) is a `400 Bad Request` — the client sent something structurally wrong. A **well-formed but non-existent** ID (`findById` returns `null`) is a `404 Not Found` — the client's request was valid, the resource just doesn't exist. Conflating these two into the same response is a common, slightly misleading API design mistake.

---

## Other common cast failures

```js
await User.create({ age: "not-a-number" }); // String → Number fails
await Event.create({ startDate: "not-a-real-date" }); // String → Date fails (an ambiguous/invalid date string)
await User.create({ isActive: {} }); // an object where a Boolean/primitive was expected
```

```js
const orderSchema = new mongoose.Schema({
  customer: { type: mongoose.Schema.Types.ObjectId, ref: "Customer" },
});
```

```js
await Order.create({ customer: "also-not-a-valid-id" }); // CastError on a reference field, same as _id
```

Any typed field can throw a `CastError` — not just `_id`; any `ObjectId` reference field, `Number`, `Date`, or other typed field can fail to cast from a genuinely incompatible input.

---

## What Mongoose _can_ successfully cast (so you know the boundary)

```js
await User.create({ age: "30" }); // ✅ "30" → 30, succeeds
await User.create({ isActive: "true" }); // ✅ "true" → true, succeeds
await User.create({ createdAt: "2026-01-15" }); // ✅ a valid date string → a real Date, succeeds
```

Mongoose's casting is reasonably permissive for genuinely convertible values — a numeric string, a recognizable boolean-ish string, a parseable date string all succeed. `CastError` is reserved for values that are **fundamentally incompatible** with the target type, not just differently formatted.

---

## Reading a `CastError`'s details

```js
err.name; // "CastError"
err.path; // "age" — which field
err.value; // "thirty" — the actual value that failed
err.kind; // "Number" — the type it was trying to cast TO
err.reason; // the underlying JS error, if any, that caused the cast failure
```

Similar shape to an individual entry in a `ValidationError`'s `.errors` object (`02-validation-errors.md`), but `CastError` is its own top-level error, not nested inside a parent — it's thrown standalone, immediately, the moment casting fails, rather than being collected alongside other field errors.

---

## Does a `CastError` on one field abort the entire operation?

```js
await User.create({ name: "Alice", age: "not-a-number" });
// throws CastError immediately — "name" is never even checked, the whole create() call fails
```

Yes — unlike `ValidationError`, which collects failures from **every** field before throwing one combined error, a `CastError` on any single field throws immediately, without even attempting to cast or validate the remaining fields. This is a meaningful practical difference: a `ValidationError` can tell a client about multiple invalid fields at once; a `CastError` only ever reports the first one encountered.

---

## Preventing cast errors before they happen: validate the shape of IDs early

```js
import mongoose from "mongoose";

function isValidObjectId(id) {
  return mongoose.Types.ObjectId.isValid(id);
}
```

```js
app.get("/users/:id", async (req, res, next) => {
  if (!isValidObjectId(req.params.id)) {
    return res.status(400).json({ error: "Invalid user ID format" });
  }
  const user = await User.findById(req.params.id);
  if (!user) return res.status(404).json({ error: "User not found" });
  res.json(user);
});
```

A common, proactive alternative to catching `CastError` after the fact — checking `ObjectId.isValid()` upfront (often as reusable validation middleware, tying into `06-express/05-validation.md` and `09-validation/`) means a malformed ID is rejected before a query is even attempted, with a clear, deliberate error response rather than relying on catching an exception.

**Note:** `ObjectId.isValid()` checks the _format_ is valid (24 hex characters, or certain other acceptable forms) — it doesn't confirm the ID actually corresponds to an existing document; that's still a separate `null`-check after the query.

## Common mistakes

- **Confusing a `CastError` (malformed input) with a `404` (well-formed but nonexistent)** — these should generally produce different status codes, since they mean genuinely different things to a client.
- **Expecting `CastError` to collect every field's failures like `ValidationError` does** — it doesn't; it throws immediately on the first cast failure, before other fields are even checked.
- **Not proactively validating ID format** before querying, and relying entirely on catching the resulting `CastError` — both approaches work, but proactive validation (`ObjectId.isValid()`) is often clearer and avoids the exception-based control flow.
- **Treating every `CastError` as user error** — occasionally a cast failure indicates a bug in your _own_ code (e.g. accidentally passing an object where a string was expected), not necessarily bad client input; worth a moment's thought about the actual source before assuming it's always the client's fault.

## Quick summary

- `CastError` fires when a value cannot be converted to a field's type at all; `ValidationError` fires on correctly-typed values that violate a rule — different stages, different meanings
- Invalid `ObjectId` strings (often from route params) are the most common real-world trigger
- Unlike `ValidationError`, a `CastError` throws immediately on the first bad field, without checking the rest
- Proactively checking `mongoose.Types.ObjectId.isValid()` is a common alternative to catching the error after the fact
- Distinguish a malformed ID (`400`) from a well-formed-but-missing one (`404`) in your API responses — they mean different things to a client

## Next

**`04-duplicate-key-errors.md`** covers the error type this section builds toward most directly — code `11000`, and exactly how to identify which field caused it.
