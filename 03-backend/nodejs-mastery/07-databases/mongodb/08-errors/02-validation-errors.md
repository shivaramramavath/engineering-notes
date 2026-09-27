# Validation Errors

`ValidationError`'s actual structure, in depth — how to read it, extract per-field messages, and use it to build a genuinely useful API response rather than a generic "invalid input" message.

## Triggering one

```js
const userSchema = new mongoose.Schema({
  name: { type: String, required: true },
  email: { type: String, required: true, match: /.+@.+\..+/ },
  age: { type: Number, min: 0, max: 120 },
});
```

```js
try {
  await User.create({ email: "not-an-email", age: 200 });
} catch (err) {
  console.log(err.name); // "ValidationError"
}
```

---

## The full structure

```js
console.log(err);
```

```js
ValidationError: User validation failed: name: Path `name` is required., email: Please enter a valid email, age: Path `age` (200) is more than maximum allowed value (120).
```

That's the flattened string form. The **useful** part is `err.errors` — an object keyed by field name, each value a `ValidatorError` with its own structured details:

```js
console.log(err.errors);
```

```js
{
  name: {
    message: "Path `name` is required.",
    kind: "required",
    path: "name",
    value: undefined,
  },
  email: {
    message: "Please enter a valid email",
    kind: "regexp",
    path: "email",
    value: "not-an-email",
  },
  age: {
    message: "Path `age` (200) is more than maximum allowed value (120).",
    kind: "max",
    path: "age",
    value: 200,
  },
}
```

Each entry tells you exactly which field failed (`path`), what kind of rule it violated (`kind` — `"required"`, `"min"`, `"max"`, `"enum"`, `"regexp"`, or a custom validator's name), the actual invalid value that was submitted (`value`), and a human-readable message (`message`).

---

## Extracting per-field messages for an API response

```js
function formatValidationError(err) {
  const fieldErrors = {};
  for (const [field, validatorError] of Object.entries(err.errors)) {
    fieldErrors[field] = validatorError.message;
  }
  return fieldErrors;
}
```

```js
app.post("/users", async (req, res, next) => {
  try {
    const user = await User.create(req.body);
    res.status(201).json(user);
  } catch (err) {
    if (err.name === "ValidationError") {
      return res.status(400).json({ errors: formatValidationError(err) });
    }
    next(err);
  }
});
```

```json
{
  "errors": {
    "name": "Path `name` is required.",
    "email": "Please enter a valid email",
    "age": "Path `age` (200) is more than maximum allowed value (120)."
  }
}
```

This shape — an object mapping each invalid field to its own message — is exactly what a frontend form needs to show field-level errors next to the relevant inputs, rather than one vague, unhelpful "something went wrong" message.

---

## Nested field validation errors

```js
const orderSchema = new mongoose.Schema({
  items: [{ product: String, quantity: { type: Number, min: 1 } }],
});
```

```js
await Order.create({ items: [{ product: "Widget", quantity: 0 }] });
```

```js
err.errors["items.0.quantity"].message;
// "Path `quantity` (0) is less than minimum allowed value (1)."
```

For a subdocument or array element, the key in `err.errors` uses dot notation including the array index — worth knowing when building a formatter that needs to translate this back into something meaningful for the client (e.g. "item 1: quantity must be at least 1").

---

## Checking for a validation failure on a specific field

```js
if (err.errors.email) {
  console.log("Specifically the email field failed:", err.errors.email.message);
}
```

Useful when your handling logic needs to react differently depending on _which_ field failed, rather than just displaying every message generically.

---

## `err.errors[field].kind` — branching on the type of rule violated

```js
for (const [field, validatorError] of Object.entries(err.errors)) {
  if (validatorError.kind === "required") {
    console.log(`${field} is missing`);
  } else if (validatorError.kind === "enum") {
    console.log(`${field} has an invalid value`);
  }
}
```

Rarely necessary for a simple "show the message" response, but useful if you want different handling/logging based on the _category_ of validation failure rather than just displaying the message Mongoose generated.

---

## `document.validate()` — validating without saving

```js
const user = new User({ email: "not-an-email" });

try {
  await user.validate(); // runs validation WITHOUT attempting to save
} catch (err) {
  console.log(err.errors);
}
```

Useful when you want to check validity as a distinct step — before some other side effect, or as part of a pre-check in a bulk-import script (as shown in `02-mongodb-vs-mongoose/02-when-to-drop-to-the-native-driver.md`'s "validate a sample, then bulk-insert" pattern) — without the `.save()` call actually attempting to write anything.

```js
user.validateSync(); // the synchronous version — no async validators (09-validation/03-async-validators.md) allowed
```

## Common mistakes

- **Only reading `err.message`** (the flattened string) instead of `err.errors` (the structured object) — the string is fine for logging, but far less useful for building a field-by-field API response.
- **Not handling nested/array field paths** (`"items.0.quantity"`) specially when formatting errors for a client that expects a flat field name — worth normalizing if your frontend needs simpler keys.
- **Assuming every field that failed validation appears in a fixed order** — `err.errors` is a plain object; iterate with `Object.entries()`/`for...in`, don't assume field order matches the schema's declared order.
- **Confusing `.validate()` (throws) with `.validateSync()` (also throws, but synchronous and can't run async validators)** — pick based on whether the schema has any async validators (`09-validation/03-async-validators.md`).

## Quick summary

- `err.errors` is a plain object keyed by field name, each holding `message`/`kind`/`path`/`value` — far more useful than the flattened `err.message` string for building a real response
- Nested/array field errors use dot-notation-with-index keys (`"items.0.quantity"`)
- `document.validate()`/`validateSync()` let you check validity as a standalone step, without attempting to save
- `err.errors[field].kind` identifies which _category_ of rule was violated, useful for conditional handling beyond just displaying the message

## Next

**`03-cast-errors.md`** covers the other everyday error type — `CastError` — and exactly how it differs from `ValidationError`.
