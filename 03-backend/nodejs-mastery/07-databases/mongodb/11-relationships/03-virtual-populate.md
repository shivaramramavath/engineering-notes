# Virtual Populate

Populating the "other side" of a relationship — finding all the documents that reference a given parent, without storing a redundant array of IDs on that parent to keep in sync.

## The problem it solves

```js
const postSchema = new mongoose.Schema({
  title: String,
  author: { type: mongoose.Schema.Types.ObjectId, ref: "User" },
});
```

Given this (the normal, one-directional reference from `01-references-and-populate.md`), how do you find **all the posts a given user has written**? The reference only points one way — from post to author, not the reverse.

### The tempting-but-wrong fix: a redundant array on the parent

```js
// ❌ requires manually keeping this array in sync every time a post is created/deleted
const userSchema = new mongoose.Schema({
  name: String,
  postIds: [{ type: mongoose.Schema.Types.ObjectId, ref: "Post" }],
});
```

Maintaining this array means updating it every time a post is created, deleted, or reassigned to a different author — a genuine synchronization burden, and a real risk of the array drifting out of sync with reality if any code path forgets to update it.

### The actual fix: just query the other collection directly

```js
const posts = await Post.find({ author: userId });
```

This is often sufficient on its own — no special mechanism needed, just a normal query on the `Post` collection filtering by `author`. Virtual populate exists for when you specifically want this relationship to feel like a natural, populatable field _on the User model itself_, rather than a separate explicit query.

---

## Declaring a virtual populate field

```js
const userSchema = new mongoose.Schema(
  { name: String },
  { toJSON: { virtuals: true } },
);

userSchema.virtual("posts", {
  ref: "Post", // which model to look in
  localField: "_id", // the field on THIS (User) document
  foreignField: "author", // the field on the Post documents that references back to this user
});
```

```js
const user = await User.findById(userId).populate("posts");
user.posts; // every Post document where post.author === user._id
```

This uses the exact same virtual mechanism from `04-schemas/06-schema-methods-statics-virtuals.md` — `localField`/`foreignField`/`ref` describe how to look up the relationship, and `.populate("posts")` triggers that lookup, exactly like populating a normal reference field.

---

## Virtual populate is read-only

```js
user.posts = [somePost]; // this doesn't actually do anything meaningful — no data is written
await user.save(); // the User document has no "posts" field at all in the database
```

Unlike a real stored field, a virtual populate field has **no corresponding data in the database** — it's purely computed at query time, by `.populate()` running the underlying `Post.find({ author: user._id })` query for you. You cannot assign to it and expect the assignment to persist.

---

## `count: true` — counting instead of fetching

```js
userSchema.virtual("postCount", {
  ref: "Post",
  localField: "_id",
  foreignField: "author",
  count: true,
});
```

```js
const user = await User.findById(userId).populate("postCount");
user.postCount; // just a number, not the actual array of posts
```

Meaningfully cheaper than populating the full array when you only need a count — MongoDB counts matching documents rather than fetching and transferring all of them.

---

## Options: limiting, sorting, and filtering the populated results

```js
userSchema.virtual("recentPosts", {
  ref: "Post",
  localField: "_id",
  foreignField: "author",
  options: { sort: { createdAt: -1 }, limit: 5 },
});
```

```js
const user = await User.findById(userId).populate("recentPosts");
user.recentPosts; // the 5 most recent posts by this user
```

The `options` object accepts the same kind of query-refinement options covered in `06-crud-methods/05-query-chaining-and-cursor-methods.md` — a genuinely useful way to define a specific, reusable "view" of the relationship (like "this user's 5 most recent posts") directly on the schema.

---

## `justOne: true` — for a one-to-one relationship

```js
userSchema.virtual("profile", {
  ref: "Profile",
  localField: "_id",
  foreignField: "user",
  justOne: true,
});
```

```js
const user = await User.findById(userId).populate("profile");
user.profile; // a single Profile document, or null — not an array
```

Without `justOne: true`, virtual populate always returns an array (even if only one document happens to match); this option is for relationships that are genuinely one-to-one, returning a single document (or `null`) directly.

---

## When to use virtual populate vs. just querying directly

```js
// virtual populate — feels like a natural field on the model
const user = await User.findById(userId).populate("posts");

// direct query — arguably just as clear, and avoids an extra abstraction layer
const user = await User.findById(userId);
const posts = await Post.find({ author: user._id });
```

Both achieve the same result. Virtual populate is worth it when the relationship is genuinely central to how the model is used throughout the app (worth the schema-level investment to make it feel like a first-class field), or when it needs to compose with other `.populate()` calls in one query. A direct query is often simpler and just as readable for a one-off need, without adding schema complexity for a relationship only used in one place.

## Common mistakes

- **Trying to assign to a virtual populate field expecting it to persist** — it's entirely computed at query time; there's no underlying stored data to write to.
- **Maintaining a redundant array of IDs on the parent** to represent a relationship that virtual populate (or just a direct query) already handles cleanly — extra synchronization burden for no real benefit.
- **Forgetting `count: true` exists** and populating the full array just to get `.length` — wasteful when only a count is actually needed.
- **Not setting `justOne: true` for a genuinely one-to-one relationship** — results in an unexpected array wrapping a single document.

## Quick summary

- Virtual populate lets you query "the other side" of a relationship (e.g. all posts by a user) as if it were a normal populatable field, without storing a redundant, hard-to-maintain array of IDs
- It's entirely computed at query time via `localField`/`foreignField`/`ref` — read-only, with nothing actually stored in the database for it
- `count: true` for a cheap count instead of the full array; `justOne: true` for a genuinely one-to-one relationship; `options` for sorting/limiting the populated results
- A plain, direct query on the related collection is often just as good for a one-off need — virtual populate earns its place for relationships used repeatedly or composed with other populates

## Next

**`04-populate-performance.md`** covers the real cost of all this `.populate()` machinery, and when to reach for an aggregation `$lookup` instead.
