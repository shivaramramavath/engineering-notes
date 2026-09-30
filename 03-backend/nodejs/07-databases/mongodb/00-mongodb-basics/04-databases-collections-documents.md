# Databases, Collections & Documents

The core hierarchy every MongoDB deployment — and every Mongoose model — is built on.

```
Server (mongod)
  └── Database
        └── Collection   ←── a Mongoose Model maps to exactly one of these
              └── Document   ←── a Mongoose Document is one of these
```

## Databases

A container for collections, created implicitly on first write — there's no separate "create database" step.

```js
// this single write creates both the "myapp" database and the "users" collection,
// the moment Mongoose actually performs it
await User.create({ name: "Alice" });
```

Mongoose's connection string includes the database name directly:

```
mongodb://localhost:27017/myapp
                            ^^^^^ the database
```

## Collections — what a Mongoose Model actually is

A collection groups documents together, without enforcing a fixed schema at the database level — MongoDB itself doesn't care if two documents in the same collection have completely different shapes.

```js
const User = mongoose.model("User", userSchema);
```

This single line is the bridge between the two worlds: `User` is a **Mongoose Model**, and it corresponds to one **MongoDB collection** (by default, `"users"` — lowercased and pluralized from `"User"`). Mongoose's entire value proposition — schemas, validation, casting — is a layer your application enforces _before_ anything reaches a collection that, on MongoDB's own side, would happily accept any shape at all.

```js
mongoose.model("User", userSchema); // → collection "users"
mongoose.model("Category", schema, "cats"); // → explicitly force collection "cats"
```

## Documents — what a Mongoose Document actually is

A single record — the closest MongoDB equivalent to a relational row.

```js
{
  _id: ObjectId("64f1a2b3c4d5e6f7a8b9c0d1"),
  name: "Alice",
  age: 30,
}
```

A **Mongoose Document** is a JavaScript object wrapping one of these, with extra behavior layered on top (change tracking, validation, `.save()`, instance methods — `05-models/02-model-vs-document.md` covers this distinction in depth):

```js
const alice = await User.findOne({ name: "Alice" });
alice instanceof mongoose.Document; // true
alice._id instanceof mongoose.Types.ObjectId; // true
```

### `_id`: every document's primary key

Every document has a unique `_id`. Mongoose auto-generates one (an `ObjectId`) when you create a document, exactly like the underlying driver does — you rarely need to think about this at all unless you're deliberately supplying a custom `_id`.

### Document size limit

A hard 16MB limit per document, enforced by MongoDB itself, regardless of Mongoose. Relevant to the embed-vs-reference decision in `11-relationships/02-embedding-vs-referencing-in-mongoose.md` — an unboundedly growing embedded array is the most common way to eventually hit this.

## Common mistakes

- **Expecting an error writing to a database/collection that doesn't exist yet** — both are created implicitly on first write, with no equivalent to a relational "table doesn't exist" error.
- **Forgetting Mongoose pluralizes/lowercases the model name for the collection** — `mongoose.model("Category", ...)` maps to `categories`, which can surprise you when inspecting the raw database in `mongosh`/Compass and not seeing a collection named exactly `Category`.
- **Confusing a Mongoose Document (the JS object with `.save()`, etc.) with the underlying MongoDB document (the raw BSON data)** — related, but not literally the same thing; `05-models/02-model-vs-document.md` draws this out fully.

## Quick summary

- Server → database → collection → document is the hierarchy; a Mongoose Model maps to exactly one collection, and a Mongoose Document wraps exactly one underlying MongoDB document
- Databases/collections are created implicitly on first write
- `_id` is every document's primary key, auto-generated as an `ObjectId` by Mongoose/the driver unless you override it
- The 16MB document size limit is a hard MongoDB constraint that shapes real schema design decisions later in this guide

## Section complete

That's the MongoDB grounding this guide needs. **`01-mongodb-native-driver`** covers the actual Node.js library Mongoose is built directly on top of.
