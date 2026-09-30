# Defining a Schema

The starting point of every Mongoose model — a `Schema` object describing the shape of a document.

## The basic syntax

```js
import mongoose from "mongoose";

const userSchema = new mongoose.Schema({
  name: String,
  age: Number,
  isActive: Boolean,
});
```

Each key becomes a field on documents created from this schema; each value declares that field's type. This shorthand (`name: String`) is equivalent to the fuller form used when you need options too:

```js
const userSchema = new mongoose.Schema({
  name: { type: String },
  age: { type: Number },
  isActive: { type: Boolean },
});
```

Both produce an identical schema — the shorthand is just more concise when no additional options (`03-schema-type-options.md`) are needed for a field.

---

## A schema is not a model — yet

```js
const userSchema = new mongoose.Schema({ name: String });

// userSchema alone can't query or create documents
// it has to be compiled into a model first:
const User = mongoose.model("User", userSchema);
```

The schema is a **description**; the model (`05-models/`) is the actual object with `.find()`, `.create()`, and every other method your application calls. This separation matters because the same schema can, in principle, be reused (e.g. as a subdocument schema, `04-nested-and-subdocument-schemas.md`) without necessarily becoming its own top-level model.

---

## Fields not in the schema are silently dropped

```js
const userSchema = new mongoose.Schema({ name: String, age: Number });
const User = mongoose.model("User", userSchema);

const user = await User.create({
  name: "Alice",
  age: 30,
  favoriteColor: "blue",
});
console.log(user.favoriteColor); // undefined — never stored, no error thrown
```

Any field passed to `create()`/`new Model()` that isn't declared in the schema is simply ignored — not saved, and (by default) no error or warning. This is one of the most common sources of "why isn't this field saving" confusion for people new to Mongoose; the fix is always to check the schema includes the field you expect.

---

## Arrays and nested structures, at a glance

```js
const userSchema = new mongoose.Schema({
  name: String,
  tags: [String], // an array of strings
  address: { city: String, zip: String }, // a nested plain object
  orders: [{ product: String, qty: Number }], // an array of subdocuments
});
```

Full depth on each of these shapes in `02-schema-types-reference.md` (for `[String]`-style arrays) and `04-nested-and-subdocument-schemas.md` (for the embedded-object shapes).

---

## Schemas can be defined separately and reused

```js
// schemas/address.js
export const addressSchema = new mongoose.Schema({
  city: String,
  zip: String,
});
```

```js
// schemas/user.js
import { addressSchema } from "./address.js";

const userSchema = new mongoose.Schema({
  name: String,
  address: addressSchema,
  shippingAddresses: [addressSchema],
});
```

Defining a schema once and reusing it (as a nested field, or in an array) avoids duplicating the same field definitions across multiple parent schemas — the same `addressSchema` here backs both a single embedded address and an array of them.

---

## A typical project structure

```
src/
├── models/
│   ├── User.js       (schema + model definition together, the common convention)
│   ├── Post.js
│   └── Address.js    (a reusable subdocument schema, no model of its own)
```

```js
// models/User.js — schema and model defined and exported from the same file
import mongoose from "mongoose";

const userSchema = new mongoose.Schema({
  name: String,
  email: String,
});

export default mongoose.model("User", userSchema);
```

```js
// elsewhere in the app
import User from "./models/User.js";

const user = await User.create({ name: "Alice", email: "alice@example.com" });
```

Defining the schema and immediately compiling/exporting the model from the same file is the near-universal convention — you'll rarely see a schema exported on its own unless it's specifically meant to be reused as a subdocument (like the `Address` example above).

## Common mistakes

- **Expecting an unsupplied field to throw an error** — an undeclared field is silently dropped, not rejected; this is different from a _declared_ field failing validation (`09-validation/`).
- **Forgetting the schema and the model are two different things** — you always need `mongoose.model("Name", schema)` before you can actually query or create documents.
- **Redefining the same nested shape (like an address) in multiple schemas** instead of extracting a reusable schema for it — leads to drift when the shape needs to change later.

## Quick summary

- `new mongoose.Schema({...})` describes a document's shape; `mongoose.model()` compiles it into something usable
- Fields not declared in the schema are silently ignored on write — the most common early confusion
- Schemas can be defined separately and reused as subdocuments (single or in an array)
- Convention: define and export the model directly from its own file, with the schema defined right alongside it

## Next

**`02-schema-types-reference.md`** covers every type a schema field can actually be declared as.
