# Common Hook Patterns

Three genuinely common, practical uses of `pre`/`post` hooks: hashing a password before save, auto-generating a URL-friendly slug, and cascading a delete to related documents.

## Pattern 1: Hashing a password before save

```js
import bcrypt from "bcrypt";

const userSchema = new mongoose.Schema({
  email: { type: String, required: true, unique: true },
  password: { type: String, required: true, select: false },
});

userSchema.pre("save", async function () {
  if (!this.isModified("password")) return; // only hash if the password actually changed
  this.password = await bcrypt.hash(this.password, 10);
});
```

### Why `isModified()` matters here

```js
const user = await User.findById(userId).select("+password");
user.name = "New Name";
await user.save(); // triggers pre("save") — but the password field wasn't touched
```

Without the `isModified("password")` check, **every** save — even one that only changes an unrelated field like `name` — would re-hash the password, hashing an already-hashed value all over again and corrupting it. `isModified(path)` tells you whether that specific field was actually changed since the document was loaded (or is new), which is exactly the condition that should trigger re-hashing.

### Covering the update path too

```js
userSchema.pre("findOneAndUpdate", async function (next) {
  const update = this.getUpdate();
  if (update.password) {
    update.password = await bcrypt.hash(update.password, 10);
  }
  next();
});
```

Per `02-document-vs-query-vs-aggregate-middleware.md`, the `pre("save")` hook above only covers the document-loading path — a direct `User.findOneAndUpdate({...}, { password: newPassword })` call needs its own query-middleware hook to hash the password on that path too, since it's a genuinely separate middleware category.

---

## Pattern 2: Auto-generating a slug

```js
import slugify from "slugify";

const postSchema = new mongoose.Schema({
  title: { type: String, required: true },
  slug: { type: String, unique: true },
});

postSchema.pre("save", function () {
  if (this.isModified("title")) {
    this.slug = slugify(this.title, { lower: true, strict: true });
  }
});
```

```js
const post = await Post.create({ title: "Hello, World! A Guide" });
post.slug; // "hello-world-a-guide"
```

Same `isModified()` principle — only regenerate the slug when the title actually changed, so editing an unrelated field (say, the post's body) doesn't unexpectedly change its URL.

### Handling slug collisions

```js
postSchema.pre("save", async function () {
  if (!this.isModified("title")) return;

  let baseSlug = slugify(this.title, { lower: true, strict: true });
  let slug = baseSlug;
  let counter = 1;

  while (await mongoose.models.Post.exists({ slug, _id: { $ne: this._id } })) {
    slug = `${baseSlug}-${counter}`;
    counter++;
  }

  this.slug = slug;
});
```

Appends `-1`, `-2`, etc. if the base slug is already taken by a _different_ document (`_id: { $ne: this._id }` excludes the current document itself, so re-saving an unchanged title doesn't falsely flag a collision with itself) — a genuinely common real requirement once a blog/CMS has more than a handful of posts with potentially similar titles.

---

## Pattern 3: Cascading deletes to related documents

```js
const userSchema = new mongoose.Schema({ name: String });

userSchema.pre(
  "deleteOne",
  { document: true, query: false },
  async function (next) {
    await Post.deleteMany({ author: this._id });
    await Comment.deleteMany({ author: this._id });
    next();
  },
);
```

```js
const user = await User.findById(userId);
await user.deleteOne(); // also deletes all of this user's posts and comments automatically
```

Note the `{ document: true, query: false }` option (`02-document-vs-query-vs-aggregate-middleware.md`) — this hook specifically targets `document.deleteOne()`, the form called on an already-loaded instance, where `this._id` refers to the specific user being deleted.

### Covering the Model-level delete path too

```js
userSchema.pre(
  "deleteOne",
  { document: false, query: true },
  async function (next) {
    const userToDelete = await this.model.findOne(this.getFilter());
    if (userToDelete) {
      await Post.deleteMany({ author: userToDelete._id });
      await Comment.deleteMany({ author: userToDelete._id });
    }
    next();
  },
);
```

```js
await User.deleteOne({ _id: userId }); // this form needs its OWN hook, per the document-vs-query distinction
```

Since the Model-level `deleteOne` never loads a document, this second hook has to explicitly query for the user first (using `this.getFilter()` to get the same filter the delete itself will use) before it can know which related documents to clean up — a real, somewhat awkward consequence of query middleware not having a document to work with directly.

### Should this be a transaction instead?

```js
const session = await mongoose.startSession();
session.startTransaction();
try {
  await Post.deleteMany({ author: userId }, { session });
  await Comment.deleteMany({ author: userId }, { session });
  await User.deleteOne({ _id: userId }, { session });
  await session.commitTransaction();
} catch (err) {
  await session.abortTransaction();
  throw err;
} finally {
  session.endSession();
}
```

Worth genuinely considering: a hook-based cascade (above) can leave data in a partially-cleaned-up state if one of the `deleteMany` calls fails partway through (the user might already be gone, but their posts remain, or vice versa, depending on hook ordering). A transaction (`13-transactions/`) guarantees all-or-nothing — either everything is deleted together, or none of it is. For anything where partial cleanup would be a real problem, prefer an explicit transaction in a service function over relying on hooks alone.

---

## A general principle across all three patterns

Every pattern here follows the same shape: **check if the relevant condition actually applies** (`isModified()`, which form of delete is firing) **before doing the work** — a hook that runs unconditionally on every save/delete, doing unnecessary work (re-hashing an unchanged password, regenerating an unchanged slug, querying for a document that doesn't need cascading), is both wasteful and a common source of subtle bugs.

## Common mistakes

- **Forgetting `isModified()` before re-hashing a password** — corrupts the password on every unrelated save.
- **Only implementing a cascading-delete hook for one form of `deleteOne`** (document or query), leaving the other form to silently skip the cascade entirely.
- **Not handling slug collisions**, assuming titles will always be unique in practice — works until two posts genuinely do share a title.
- **Using a hook-based cascade for delete operations where partial failure would be a real problem**, instead of an explicit transaction that guarantees all-or-nothing.

## Quick summary

- Password hashing: hook `pre("save")` for the document path, and separately hook the relevant query middleware (e.g. `findOneAndUpdate`) if passwords can also be updated that way — always gate on `isModified()`/checking the update payload to avoid re-hashing unnecessarily
- Slug generation: same `isModified()` gating, plus a collision-check loop excluding the current document's own `_id`
- Cascading deletes: need separate hooks for the document (`{ document: true }`) and query (`{ document: false, query: true }`) forms of `deleteOne`/`deleteMany` — and consider a real transaction instead of hooks when partial failure would leave data in a genuinely bad state

## Section complete

That covers Mongoose middleware in full — the hook syntax, the document/query/aggregate distinction, and three practical, real-world patterns. **`11-relationships`** covers how documents reference each other across collections, and `.populate()`.
