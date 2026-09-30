# Built-in Validators

Every validator Mongoose ships with out of the box — a consolidated, example-driven reference. Most of these were introduced individually in `04-schemas/03-schema-type-options.md`; this file gathers them together with a focus on getting messages and edge cases right.

## `required`

```js
name: { type: String, required: true }
name: { type: String, required: [true, "Name is required"] }
name: {
  type: String,
  required: function () {
    return this.accountType === "business";   // conditionally required
  },
}
```

`required` fails on `undefined`, `null`, and (for strings specifically) an empty string `""` — but **not** on `0` or `false` for numbers/booleans, which are legitimate values, not "missing" ones. This is worth double-checking explicitly if a boolean or numeric field is ever incorrectly rejected as "required" when it's actually just falsy.

---

## `min` / `max` — numbers and dates

```js
age: { type: Number, min: 0, max: 120 }
age: { type: Number, min: [0, "Age cannot be negative"], max: [120, "Age seems unrealistic"] }
eventDate: { type: Date, min: "2020-01-01" }
```

Inclusive on both ends — `min: 0` allows exactly `0`, not just values greater than it.

---

## `minlength` / `maxlength` — strings

```js
username: { type: String, minlength: 3, maxlength: 20 }
username: { type: String, minlength: [3, "Username must be at least 3 characters"] }
```

Checked against the string's length **after** any `trim`/`lowercase` transforms have already been applied — worth remembering if a value is right at the boundary and a trim changes its effective length.

---

## `enum` — a fixed set of allowed values

```js
status: { type: String, enum: ["pending", "active", "suspended"] }
```

```js
status: {
  type: String,
  enum: {
    values: ["pending", "active", "suspended"],
    message: "{VALUE} is not a valid status",
  },
}
```

`{VALUE}` is substituted automatically with the actual rejected value in the error message. `enum` also works on `Number` fields, restricting to a fixed set of numeric values, though it's most commonly seen on strings.

---

## `match` — regex pattern validation

```js
email: { type: String, match: /.+@.+\..+/ }
email: { type: String, match: [/.+@.+\..+/, "Please enter a valid email"] }
```

Validates the field's string value against the pattern — `undefined`/`null` values are **not** checked against `match` (that's what `required` is for separately); `match` only fires when a value is actually present but doesn't fit the pattern.

---

## Validators only run on values, not on missing fields (except `required`)

```js
age: { type: Number, min: 0, max: 120 }
```

```js
await User.create({ name: "Alice" }); // age is undefined — min/max are simply skipped, no error
```

This is an important, sometimes-surprising default: `min`/`max`/`match`/`enum`/`minlength`/`maxlength` only validate a field **if a value was actually provided** — an absent field isn't automatically invalid unless you also mark it `required`. If a field genuinely must be present _and_ meet a constraint, both need to be declared together.

---

## Combining several validators on one field

```js
const userSchema = new mongoose.Schema({
  email: {
    type: String,
    required: [true, "Email is required"],
    match: [/.+@.+\..+/, "Please enter a valid email"],
    lowercase: true,
    trim: true,
  },
  age: {
    type: Number,
    min: [0, "Age cannot be negative"],
    max: [120, "Age seems unrealistic"],
  },
  role: {
    type: String,
    enum: {
      values: ["user", "admin", "editor"],
      message: "{VALUE} is not a valid role",
    },
    default: "user",
  },
});
```

When multiple validators are declared on one field, Mongoose runs all of them and includes **every** failure for that field's message if more than one fails — though in practice, most single-field validator combinations only have one meaningfully failing at a time (a field can't simultaneously be both too short and not match a pattern for most realistic rule sets).

---

## Validators run in a defined order relative to casting

```js
age: { type: Number, min: 0 }
```

```js
await User.create({ age: "not-a-number" }); // CastError — thrown BEFORE min/max is ever checked
await User.create({ age: "-5" }); // casts successfully to -5, THEN fails min: 0 → ValidationError
```

Casting (`08-errors/03-cast-errors.md`) always happens first; validators only run against values that survived casting successfully. A value that can't be cast at all never reaches the validator stage — it throws a `CastError` immediately instead.

## Common mistakes

- **Assuming `required` catches `0`/`false`** — it doesn't; these are valid values for numbers/booleans, not "missing" ones. Use a custom validator (`02-custom-validators.md`) if you specifically need to reject a particular falsy-but-present value.
- **Adding `min`/`max`/`match` without also adding `required`**, when the field is actually meant to be mandatory — an absent field silently skips those other validators entirely.
- **Not providing custom messages** — Mongoose's default messages are functional but generic; a custom message (the `[value, "message"]` array form) is usually worth the small extra effort for anything user-facing.
- **Expecting a validator to run on a value that failed casting** — casting happens first; a `CastError` prevents the validator stage from ever being reached for that field.

## Quick summary

- `required`, `min`/`max`, `minlength`/`maxlength`, `enum`, and `match` are the built-in validators, each accepting an optional `[value, "custom message"]` form
- `required` treats `0`/`false` as valid, present values — not "missing"
- Every non-`required` validator only checks a field if a value is actually present; declare both together when a field must exist AND meet a rule
- Casting happens before validation — a value that can't be cast throws `CastError` before ever reaching a validator

## Next

**`02-custom-validators.md`** covers writing your own validation logic beyond what the built-ins provide.
