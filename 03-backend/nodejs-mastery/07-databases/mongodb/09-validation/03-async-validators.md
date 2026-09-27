# Async Validators

Custom validators that need to do something asynchronous — most commonly, querying the database — as part of deciding whether a value is valid.

## The basic shape: an `async` validator function

```js
const userSchema = new mongoose.Schema({
  referralCode: {
    type: String,
    validate: {
      validator: async function (value) {
        if (!value) return true; // optional field — nothing to check if not provided
        const referrer = await User.findOne({ referralCode: value });
        return !!referrer; // valid only if a user with this referral code actually exists
      },
      message: "Invalid referral code",
    },
  },
});
```

```js
await User.create({ referralCode: "DOES-NOT-EXIST" }); // ValidationError — the async check returned false
```

Mongoose detects that `validator` is an `async` function (or returns a Promise) and automatically `await`s its result before deciding whether validation passed — no special configuration needed beyond just writing the function as `async`.

---

## A genuinely common real example: application-level uniqueness pre-checks

```js
email: {
  type: String,
  validate: {
    validator: async function (value) {
      const existing = await User.findOne({ email: value });
      // if editing an existing document, exclude itself from the check
      return !existing || existing._id.equals(this._id);
    },
    message: "Email is already in use",
  },
}
```

Worth an important caveat immediately: **this does not replace a unique index.** A database-level unique index (`04-schemas/03-schema-type-options.md`) is the only genuinely race-condition-proof guarantee — two concurrent requests could both pass this async validator's check (since neither has committed yet at the moment the other checks) before one of them actually saves. An async uniqueness validator can provide a _friendlier, earlier_ error in the common case, but the unique index is still required as the real safety net, with the duplicate-key error (`08-errors/04-duplicate-key-errors.md`) as the backstop for the race-condition case this validator can't fully prevent.

---

## When an async validator is (and isn't) actually worth it

### Worth it: checking against external data the schema itself can't express

```js
categoryId: {
  type: mongoose.Schema.Types.ObjectId,
  validate: {
    validator: async function (value) {
      const category = await Category.findById(value);
      return category !== null && category.isActive;
    },
    message: "Category does not exist or is inactive",
  },
}
```

A rule like "this referenced category must actually exist and currently be active" genuinely can't be expressed any other way at the schema level — it depends on the live state of another collection.

### Usually not worth it: something achievable synchronously

```js
// ❌ unnecessary — this doesn't need to be async at all
age: {
  validate: {
    validator: async function (value) {
      return value >= 0;
    },
  },
}
```

Adding `async` (and the associated overhead of an extra Promise, and potentially a database round trip if misused) for a check that could be a plain synchronous function is unnecessary complexity — only reach for an async validator when the check genuinely requires awaiting something.

---

## Performance implications

```js
await Order.create({
  customerId: someId, // an async validator checking the customer exists
  productId: someOtherId, // an async validator checking the product exists
});
```

Each async validator on a document potentially triggers its own database query — for a document with several async-validated fields, saving one document could mean several additional round trips beyond the save itself. This is a real cost worth being deliberate about; if a reference genuinely needs to be confirmed to exist, consider whether that check belongs in application logic _once_, explicitly, rather than as several separate per-field async validators each adding their own query.

---

## Async validators and `validateBeforeSave: false` / `insertMany`

```js
await User.insertMany(documents, { validateBeforeSave: false }); // skips this validator entirely
```

As covered in `06-crud-methods/01-create-methods.md`, `insertMany` with validation disabled (or `bulkWrite`, which skips the validation pipeline largely by default) bypasses async validators along with everything else — worth remembering specifically for async validators that guard against something important (like the referral code or category-existence examples above), since a bulk import path might silently skip them.

---

## Timeout considerations

```js
validator: async function (value) {
  const result = await someSlowExternalApiCall(value);   // an external HTTP call, not just a DB query
  return result.isValid;
}
```

An async validator that calls out to something slow (an external API, not just a fast, indexed database query) can meaningfully slow down every save that triggers it, and introduces a new failure mode (the external call timing out or erroring) that needs its own handling. Generally, keep async validators to fast, reliable checks — a database lookup on an indexed field is usually fine; a slow third-party API call inside a validator is a design smell worth reconsidering (perhaps that check belongs in application logic with its own explicit error handling, rather than buried inside schema validation).

## Common mistakes

- **Relying on an async uniqueness validator instead of a real unique index** — leaves a genuine race-condition window; the validator is a nice-to-have early check, the index is the actual guarantee.
- **Making a validator `async` when it doesn't need to be** — unnecessary overhead and complexity for a check that could be synchronous.
- **Not accounting for the extra database round trips async validators add**, especially several on one document — can meaningfully slow down saves under load.
- **Forgetting async validators are skipped by `insertMany`/`bulkWrite` with validation disabled** — a bulk-import path might silently bypass an important check.
- **Putting a slow external API call inside a validator** — better handled as explicit application logic with its own error handling and timeout strategy, rather than hidden inside the schema validation pipeline.

## Quick summary

- An `async` (or Promise-returning) validator function is automatically awaited by Mongoose — no special config needed
- Async validators are genuinely necessary for checks depending on live external/database state that the schema alone can't express
- Never use an async uniqueness check as a substitute for a real unique index — it's a friendlier early warning, not a race-condition-proof guarantee
- Be mindful of the performance cost: each async validator can mean an extra database round trip per save
- `insertMany`/`bulkWrite` with validation disabled skip async validators along with everything else in the pipeline

## Next

**`04-validating-updates.md`** covers the single most commonly-missed validation gotcha in Mongoose — why update operations don't validate at all by default.
