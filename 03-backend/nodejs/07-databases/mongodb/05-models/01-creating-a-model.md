# Creating a Model

`mongoose.model()` — the step that turns a schema (a description) into a model (something you can actually call methods on).

## The basic call

```js
const userSchema = new mongoose.Schema({ name: String, email: String });

const User = mongoose.model("User", userSchema);
```

Two required arguments: a name (used for collection naming and for `ref` lookups in relationships, `11-relationships/`) and the schema itself.

```js
const user = await User.create({ name: "Alice", email: "alice@example.com" });
const users = await User.find();
```

---

## Collection naming: automatic pluralization and lowercasing

```js
mongoose.model("User", userSchema); // → collection "users"
mongoose.model("Category", categorySchema); // → collection "categories"
mongoose.model("Person", personSchema); // → collection "people" (Mongoose handles some irregular plurals)
```

Mongoose derives the collection name automatically: lowercase, pluralized. This is convenient, but worth knowing about explicitly — it's why inspecting the raw database in `mongosh`/Compass shows `users`, not `User`, and can occasionally surprise you with an irregular plural.

### Overriding the collection name explicitly

```js
mongoose.model("User", userSchema, "app_users"); // third argument — explicit collection name
```

or, equivalently, via a schema option (`04-schemas/05-schema-options.md`):

```js
const userSchema = new mongoose.Schema(
  { name: String },
  { collection: "app_users" },
);
```

Useful when connecting Mongoose to an existing database whose collection names don't follow Mongoose's automatic convention, or when you specifically want a name that differs from the pluralized default.

---

## Retrieving an already-compiled model

```js
// somewhere else in the codebase, without importing the original file
const User = mongoose.model("User");
```

Calling `mongoose.model("Name")` with **only** a name (no schema argument) retrieves a model that was already compiled elsewhere, rather than creating a new one. This works, but relying on it can make dependencies between files less obvious — importing the model directly from wherever it's defined (the convention shown in `04-schemas/01-defining-a-schema.md`) is generally clearer and is what causes the `OverwriteModelError` covered in `03-compiling-models-and-avoiding-overwrite-errors.md` if done carelessly.

---

## A model is tied to a specific connection

```js
const User = mongoose.model("User", userSchema); // uses mongoose's default connection

const secondaryConnection = mongoose.createConnection(uri);
const AuditLog = secondaryConnection.model("AuditLog", auditLogSchema); // a DIFFERENT connection
```

`mongoose.model()` compiles against Mongoose's single default connection (the one established by `mongoose.connect()`, `03-setup/02-connecting-to-mongodb.md`); a connection created via `createConnection()` has its **own** `.model()` method for models specific to that connection — relevant only if your app talks to more than one database.

---

## Typical file organization

```
src/
├── models/
│   ├── User.js
│   ├── Post.js
│   └── index.js    (optional — re-exports everything from one place)
```

```js
// models/User.js
import mongoose from "mongoose";

const userSchema = new mongoose.Schema({
  name: String,
  email: String,
});

export default mongoose.model("User", userSchema);
```

```js
// models/index.js (optional convenience)
export { default as User } from "./User.js";
export { default as Post } from "./Post.js";
```

```js
// elsewhere
import { User, Post } from "./models/index.js";
```

One model per file, compiled and exported directly, is the standard convention — it makes each model's schema easy to find and keeps `mongoose.model("Name", schema)` from ever being called more than once for the same name (the root cause covered in the next file).

## Common mistakes

- **Calling `mongoose.model("Name", schema)` more than once for the same name** — typically from a file being re-imported/re-executed (common with hot-reloading or certain test setups) — throws `OverwriteModelError`; full coverage and fixes in `03-compiling-models-and-avoiding-overwrite-errors.md`.
- **Not knowing the actual collection name a model maps to**, especially with irregular plurals — worth checking directly in `mongosh`/Compass if unsure (`00-mongodb-basics/02-shell-and-compass.md`).
- **Retrieving a model by name-only (`mongoose.model("User")`)** from a file that doesn't obviously depend on wherever the model was originally defined — prefer a direct import for clearer dependencies.

## Quick summary

- `mongoose.model(name, schema)` compiles a schema into a model, tied to Mongoose's default connection unless created via a separate `createConnection()`
- Collection names are auto-derived: lowercased and pluralized, overridable via a third argument or the schema's `collection` option
- `mongoose.model("Name")` (no schema) retrieves an already-compiled model — works, but a direct import is usually clearer
- One model per file, defined and exported directly, is the standard, safest convention

## Next

**`02-model-vs-document.md`** covers the fundamental distinction between the model you just created and the documents it produces.
