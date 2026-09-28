# References & Populate

Declaring a reference from one document to another, and resolving that reference into the actual related data with `.populate()`.

## Declaring a reference

```js
const postSchema = new mongoose.Schema({
  title: String,
  author: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
});
```

`ref: "User"` tells Mongoose which model this `ObjectId` field points to — it doesn't create any kind of database-level foreign key constraint (MongoDB has no such concept natively); it's purely metadata Mongoose uses later, specifically to know which collection `.populate()` should look in.

```js
const post = await Post.create({
  title: "Hello World",
  author: someUserId, // just a plain ObjectId, stored as-is
});
```

Without populating, `post.author` is just the raw `ObjectId` — nothing more.

---

## `.populate()` — resolving the reference

```js
const post = await Post.findById(postId).populate("author");
```

```js
post.author; // no longer just an ObjectId — now the ACTUAL User document
post.author.name; // directly accessible
```

Under the hood, `.populate()` performs a **separate query** against the `User` collection (using the `ref` from the schema) to fetch the referenced document(s), then substitutes them in place of the raw `ObjectId`s in the result — this is not a database-level join; it's two (or more) separate queries that Mongoose stitches together for you.

---

## Populating only specific fields

```js
const post = await Post.findById(postId).populate("author", "name email");
```

```js
post.author; // only { _id, name, email } — not the full User document
```

Same field-selection string syntax as `.select()` (`06-crud-methods/05-query-chaining-and-cursor-methods.md`) — worth doing routinely, since fetching an entire referenced document (including fields you don't need, like a password hash) is wasteful when you only actually need a couple of fields.

---

## Populating multiple reference fields

```js
const postSchema = new mongoose.Schema({
  author: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
  category: { type: mongoose.Schema.Types.ObjectId, ref: "Category" },
});
```

```js
const post = await Post.findById(postId)
  .populate("author", "name")
  .populate("category", "name");
```

Chain `.populate()` as many times as needed, once per reference field.

---

## Populating an array of references

```js
const postSchema = new mongoose.Schema({
  tags: [{ type: mongoose.Schema.Types.ObjectId, ref: "Tag" }],
});
```

```js
const post = await Post.findById(postId).populate("tags");
```

```js
post.tags; // an array of the actual Tag documents, not ObjectIds
```

Works the same way for an array of references — `.populate()` fetches every referenced document across the array in one additional query (not one query per array element).

---

## Nested populate

```js
const commentSchema = new mongoose.Schema({
  text: String,
  author: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
});

const postSchema = new mongoose.Schema({
  title: String,
  comments: [{ type: mongoose.Schema.Types.ObjectId, ref: "Comment" }],
});
```

```js
const post = await Post.findById(postId).populate({
  path: "comments",
  populate: { path: "author", select: "name" }, // populate a field ON the populated documents
});
```

```js
post.comments[0].author.name; // resolved two levels deep
```

The object form of `.populate()` (`{ path, populate, select, match, ... }`) is needed once you go beyond a single flat reference — nested `populate` lets you resolve a reference _on_ an already-populated document.

---

## Filtering which referenced documents get populated: `match`

```js
const post = await Post.findById(postId).populate({
  path: "comments",
  match: { isApproved: true },
});
```

Only populates comments matching this additional filter — comments that don't match aren't included in the populated array at all (they're filtered out, not just left as `null`). Useful for something like only showing approved comments, without a separate query.

---

## What happens if the referenced document doesn't exist

```js
post.author = someDeletedUserId; // the User was deleted, but the reference still points at that ID
```

```js
const populatedPost = await Post.findById(postId).populate("author");
populatedPost.author; // null — Mongoose can't find a matching User, so populate resolves to null
```

A dangling reference (pointing at a document that no longer exists — exactly the kind of thing a soft delete, `09-mongodb-basics/`'s earlier coverage, or a proper cascading delete/transaction, `10-middleware-hooks/03-common-hook-patterns.md`, helps prevent) resolves to `null` rather than throwing an error — worth checking for in application code before assuming a populated field is always genuinely present.

---

## `populate()` vs a manual second query

```js
// what .populate("author") is roughly equivalent to, done manually:
const post = await Post.findById(postId);
const author = await User.findById(post.author);
post.author = author;
```

`.populate()` is genuinely just a convenience over exactly this pattern — worth knowing, since it demystifies what's actually happening (two real queries, not database-level magic), and directly explains the performance characteristics covered in `04-populate-performance.md`.

## Common mistakes

- **Assuming `ref` creates a real foreign-key constraint** — it doesn't; MongoDB has no such concept, and nothing prevents a reference from pointing at a document that later gets deleted, leaving a dangling reference.
- **Populating an entire referenced document when only a couple of fields are needed** — always pass a field-selection string as the second argument when you don't need everything.
- **Not handling a `null` result from a dangling reference** — `.populate()` silently resolves to `null` rather than throwing, so downstream code needs to check for it.
- **Forgetting nested populate needs the object form** (`{ path, populate: {...} }`), not a second flat `.populate()` call, when resolving a reference on an already-populated document.

## Quick summary

- `ref` in a schema tells `.populate()` which model to look up a stored `ObjectId` in — it's Mongoose-level metadata, not a database-enforced foreign key
- `.populate()` performs a genuinely separate query and stitches the result in, replacing the raw `ObjectId`(s) with the actual referenced document(s)
- Field-selection (`.populate("field", "onlyThese fields")`) and nested populate (the object form) cover most real needs
- A dangling reference resolves to `null` when populated, rather than throwing — always account for this possibility

## Next

**`02-embedding-vs-referencing-in-mongoose.md`** covers the actual decision this whole mechanism exists to support: when should a relationship be a reference at all, versus embedded directly?
