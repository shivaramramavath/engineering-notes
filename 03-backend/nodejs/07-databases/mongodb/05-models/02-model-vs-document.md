# Model vs Document

The single most useful mental model for understanding which method goes where in Mongoose: a **Model** is like a class, and a **Document** is like an instance of that class.

## The analogy

```js
class User {
  static findByEmail(email) {
    /* ... */
  } // a static — called on the class itself
  save() {
    /* ... */
  } // an instance method — called on an object
}

User.findByEmail("alice@example.com"); // class-level operation
const alice = new User();
alice.save(); // instance-level operation
```

```js
const User = mongoose.model("User", userSchema); // roughly, a "class"

User.find({ status: "active" }); // Model-level — operates on the COLLECTION
const alice = await User.findOne(); // returns a Document — an INSTANCE
alice.save(); // Document-level — operates on ONE record
```

This maps almost exactly onto the static-method-vs-instance-method distinction covered in `04-schemas/06-schema-methods-statics-virtuals.md` — because that's genuinely what's happening; `schema.statics` become Model methods, `schema.methods` become Document methods.

---

## Model: operates on the whole collection

```js
User.find({ age: { $gte: 18 } });        // find MANY documents
User.findOne({ email });                    // find ONE document
User.create({ name: "Alice" });               // create a NEW document
User.updateMany({ status: "pending" }, {...}); // update MANY documents at once, without loading them
User.deleteOne({ _id: userId });                 // delete a document, without loading it first
User.countDocuments({ status: "active" });          // count matching documents
```

Every Model method either **produces** document(s) (`find`, `findOne`, `create`) or acts on the collection **without necessarily loading** a full document into memory first (`updateMany`, `deleteOne`, `countDocuments`). Full coverage of every one of these in `06-crud-methods/`.

---

## Document: represents and operates on one specific record

```js
const alice = await User.findOne({ email: "alice@example.com" });

alice.name; // reading a field
alice.name = "Alice Smith"; // changing a field, in memory only, so far
alice.isModified("name"); // true — Mongoose is tracking this change
await alice.save(); // persist the change back to MongoDB
await alice.deleteOne(); // delete THIS specific document
alice.getFullName(); // a custom instance method, if one was defined (04-schemas/06-)
```

A Document is a genuine JavaScript object with real state and behavior — it tracks which fields have been modified since it was loaded (`isModified()`), can validate itself (`document.validate()`), and knows how to save or delete itself.

---

## The key practical difference: does it load a document into memory first?

```js
// Model method — one network round trip, MongoDB does the update directly, no document loaded
await User.updateOne({ _id: userId }, { $set: { age: 31 } });

// Document approach — load, modify in memory, then save (a SECOND round trip)
const user = await User.findById(userId);
user.age = 31;
await user.save();
```

Both achieve the same end result, but the Model-level `updateOne` is more efficient (one round trip instead of two) — the Document-level approach is worth it specifically when you need to **read** the document's current state, run instance methods, or trigger `pre("save")` middleware (`10-middleware-hooks/`) as part of the change, none of which a direct `updateOne` call does. Choosing between them is a real, recurring decision covered further in `06-crud-methods/03-update-methods.md`.

---

## New, unsaved documents

```js
const newUser = new User({ name: "Alice", email: "alice@example.com" });

newUser instanceof mongoose.Document; // true — it's a real Document already...
newUser.isNew; // true — ...but hasn't been saved to MongoDB yet

await newUser.save(); // now it's persisted
newUser.isNew; // false
```

`new Model(data)` creates a Document **before** anything touches the database — useful when you want to validate, modify, or otherwise work with the data as a real Mongoose Document prior to actually saving it. `Model.create(data)` is a convenience shortcut that does `new Model(data)` + `.save()` in one call.

---

## `Model.hydrate()` — the rare, explicit conversion

```js
const plainObject = { _id: someId, name: "Alice" }; // e.g. from a cache, not a fresh query
const doc = User.hydrate(plainObject); // wraps plain data as a real Document, with no validation/casting
```

An uncommon method worth knowing exists — turning a plain object (e.g. retrieved from a cache like Redis, or from `.lean()`, `14-performance/02-query-optimization-and-lean.md`) back into a genuine Mongoose Document with all its instance methods available, without re-fetching from the database.

## Common mistakes

- **Calling a Document instance method (`schema.methods`) on the Model, or a static (`schema.statics`) on a Document** — they're only available on the side they were actually attached to; check which one a given method belongs to.
- **Loading a full document just to change one field with `.save()`**, when a direct Model-level `updateOne` would be more efficient and equally correct, if middleware/instance-method behavior isn't needed for that particular change.
- **Forgetting `.save()` after mutating a loaded document's field** — the change only exists in memory until explicitly saved.
- **Assuming `.lean()` query results (plain objects, not Documents) have instance methods or `.save()` available** — they don't; `.lean()` trades away exactly this Document behavior for performance (`14-performance/02-`).

## Quick summary

- A Model is like a class — it represents the whole collection and provides methods like `find`/`create`/`updateMany` that don't necessarily load a document into memory
- A Document is like an instance — it represents one specific record, with real state, change tracking, and its own methods (`save`, custom instance methods)
- `schema.statics` become Model methods; `schema.methods` become Document methods — the exact same class/instance split
- Choose direct Model-level updates for efficiency when you don't need to read/modify/run middleware on the current state; load a Document first when you do

## Next

**`03-compiling-models-and-avoiding-overwrite-errors.md`** covers a very common practical error that comes directly from misunderstanding how model compilation works.
