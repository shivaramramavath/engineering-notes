# Custom Validators

Writing your own validation logic with `validate`, for rules the built-ins (`01-built-in-validators.md`) can't express — including validation that depends on another field on the same document.

## The basic shape

```js
const userSchema = new mongoose.Schema({
  age: {
    type: Number,
    validate: {
      validator: function (value) {
        return value % 1 === 0; // must be a whole number
      },
      message: (props) => `${props.value} is not a whole number`,
    },
  },
});
```

```js
await User.create({ age: 25.5 }); // ValidationError — fails the custom validator
```

`validator` is a function returning `true` (valid) or `false` (invalid); `message` can be a static string or a function receiving `props` (which includes `props.value`, the actual invalid value) to build a dynamic message.

### Shorthand: just the function

```js
age: {
  type: Number,
  validate: (value) => value % 1 === 0,
}
```

Fine for simple cases, but you lose the ability to customize the error message — the fuller `{ validator, message }` form is generally preferable for anything user-facing.

---

## Cross-field validation

```js
const eventSchema = new mongoose.Schema({
  startDate: Date,
  endDate: {
    type: Date,
    validate: {
      validator: function (value) {
        return value > this.startDate; // `this` refers to the document being validated
      },
      message: "End date must be after start date",
    },
  },
});
```

```js
await Event.create({
  startDate: new Date("2026-06-01"),
  endDate: new Date("2026-05-01"), // before startDate
});
// ValidationError: End date must be after start date
```

This is exactly the kind of rule the built-in validators can't express at all — a field's validity depends on _another field's_ value on the same document. As with instance methods and virtuals (`04-schemas/06-schema-methods-statics-virtuals.md`), this requires a regular `function`, not an arrow function, since arrow functions don't bind their own `this`.

---

## Multiple custom validators on one field

```js
password: {
  type: String,
  validate: [
    {
      validator: (value) => value.length >= 8,
      message: "Password must be at least 8 characters",
    },
    {
      validator: (value) => /[A-Z]/.test(value),
      message: "Password must contain an uppercase letter",
    },
    {
      validator: (value) => /[0-9]/.test(value),
      message: "Password must contain a number",
    },
  ],
}
```

An array of `{ validator, message }` objects — each runs independently, and **every** failing one contributes its own message to the resulting `ValidationError`, giving a genuinely helpful, specific list of exactly what's wrong (e.g. "needs an uppercase letter AND a number," not just a single generic "invalid password").

---

## A validator that checks array contents

```js
tags: {
  type: [String],
  validate: {
    validator: function (value) {
      return value.length <= 10;
    },
    message: "Cannot have more than 10 tags",
  },
}
```

```js
orderItems: {
  type: [orderItemSchema],
  validate: {
    validator: function (items) {
      return items.length > 0;
    },
    message: "An order must have at least one item",
  },
}
```

Custom validators work on array fields too — `value` inside the validator function is the full array, letting you check its length, contents, or any other aggregate property individually validators on the array's elements couldn't express on their own.

---

## Reusable custom validators

```js
// validators/isValidPhoneNumber.js
export function isValidPhoneNumber(value) {
  return /^\+?[1-9]\d{7,14}$/.test(value);
}
```

```js
import { isValidPhoneNumber } from "../validators/isValidPhoneNumber.js";

const userSchema = new mongoose.Schema({
  phone: {
    type: String,
    validate: {
      validator: isValidPhoneNumber,
      message: "Please enter a valid phone number",
    },
  },
});
```

Extracting a validator function to its own module, the same way you'd extract any reusable logic — useful once the same validation rule needs to apply across multiple schemas.

---

## Distinguishing "this is invalid" from "I couldn't check"

```js
someField: {
  validate: {
    validator: function (value) {
      try {
        return someComplexCheck(value);
      } catch {
        return false;   // treat a check failure as "invalid," a deliberate choice
      }
    },
  },
}
```

A synchronous custom validator should always return a boolean, never throw — if the validation logic itself might throw (e.g. parsing something that could be malformed), catch it internally and decide deliberately whether that failure means "invalid" or something else. An uncaught throw inside a validator function produces a confusing, non-standard error rather than a clean `ValidationError`.

## Common mistakes

- **Using an arrow function for a custom validator that needs `this`** — arrow functions don't bind their own `this`, so cross-field validation referencing `this.otherField` silently breaks.
- **Letting a validator function throw instead of returning `false`** — produces an unexpected error shape rather than a normal `ValidationError`.
- **Not using the array form of `validate`** when multiple independent rules apply to one field — a single validator trying to check several unrelated things in one function produces a less specific, less helpful combined error than separate validators each with their own message.
- **Duplicating the same custom validation logic across multiple schemas** instead of extracting a reusable function — leads to drift when the rule needs to change.

## Quick summary

- `validate: { validator: fn, message: "..." }` is the core custom validator shape — `fn` returns `true`/`false`
- Cross-field validation works via `this` inside a regular `function`, referencing other fields on the same document being validated
- An array of `{ validator, message }` objects lets multiple independent rules on one field each report their own specific message
- Custom validators work on array fields too, checking the array as a whole (length, contents) rather than individual elements
- Never let a validator function throw — catch internal errors and return `false` deliberately if that's the intended behavior

## Next

**`03-async-validators.md`** covers validators that need to check something asynchronously, like querying the database as part of validation.
