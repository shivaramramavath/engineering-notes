# Installing Mongoose

Getting Mongoose installed and seeing a complete, minimal working example before diving into any one piece of it in depth.

## Install

```bash
npm install mongoose
```

No separate native driver install needed — Mongoose depends on the `mongodb` package internally and manages that dependency itself.

---

## A complete minimal example

```js
import mongoose from "mongoose";

// 1. Connect
await mongoose.connect("mongodb://localhost:27017/myapp");

// 2. Define a schema
const userSchema = new mongoose.Schema({
  name: String,
  email: String,
  age: Number,
});

// 3. Compile it into a model
const User = mongoose.model("User", userSchema);

// 4. Use it
const alice = await User.create({
  name: "Alice",
  email: "alice@example.com",
  age: 30,
});
console.log(alice);

const users = await User.find({ age: { $gte: 18 } });
console.log(users);
```

Four steps, every time: **connect**, **define a schema**, **compile a model**, **use the model**. Every later section of this guide is really just going deeper into steps 2 through 4 — `04-schemas/` on step 2, `05-models/` on step 3, and `06-crud-methods/` onward on step 4.

---

## Checking the version

```bash
npm list mongoose
```

```js
console.log(mongoose.version);
```

Worth checking specifically because Mongoose's API has changed meaningfully across major versions (most notably around Node/driver compatibility and some default option changes) — if you're following a tutorial or Stack Overflow answer that looks slightly different from what's shown in this guide, a version mismatch is the most likely reason.

---

## What comes with the package

```js
import mongoose from "mongoose";

mongoose.connect(...);        // establishing a connection
mongoose.Schema;                 // the Schema constructor
mongoose.model(...);               // compiling/retrieving a model
mongoose.Types.ObjectId;             // Mongoose's ObjectId type
mongoose.connection;                   // the current connection object
```

Everything you need lives on the single `mongoose` default export — no separate packages required for the core functionality covered in this guide (plugins, covered in `07-model-behavior/03-plugins.md`, are the main place you'd add something extra).

## Quick summary

- `npm install mongoose` — no separate native driver install needed
- Every Mongoose app follows the same four steps: connect, define a schema, compile a model, use it
- Check `mongoose.version` if something behaves differently than expected — API details have shifted across major versions

## Next

**`02-connecting-to-mongodb.md`** covers step one — `mongoose.connect()` — in real depth.
