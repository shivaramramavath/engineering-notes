# Query Optimization & `.lean()`

Indexes (`01-indexes-in-mongoose.md`) make MongoDB _find_ documents faster. This file covers the other half: reducing how much work Mongoose and your application do with the results — `.lean()`, `.select()`, and avoiding unnecessary queries.

## `.lean()` — skip Mongoose Document overhead

```js
const users = await User.find({ status: "active" }).lean();
```

A normal `find()` wraps every result in a full Mongoose Document: change tracking, getters/setters, virtuals, instance methods, and validation machinery. `.lean()` skips all of it and returns plain JavaScript objects straight from the driver.

```js
users[0] instanceof mongoose.Document; // false — a plain object
users[0].save; // undefined
```

### How much faster?

On large result sets the difference is substantial — commonly a several-fold reduction in both time and memory for read-heavy queries, since Mongoose no longer builds a heavyweight object for every single row. For a query returning 5 documents it's negligible; for one returning 10,000 it matters.

### What you give up with `.lean()`

| Feature                                  | With `.lean()`                                                          |
| ---------------------------------------- | ----------------------------------------------------------------------- |
| `.save()` / `.deleteOne()` on the result | ❌ Not available                                                        |
| Instance methods (`schema.methods`)      | ❌ Not available                                                        |
| Virtuals                                 | ❌ Not included (unless you use a plugin like `mongoose-lean-virtuals`) |
| Getters/setters, casting on read         | ❌ Skipped                                                              |
| `_id` as an `ObjectId`                   | ✅ Still an `ObjectId`, not a string                                    |
| Defaults applied on read                 | ❌ Not applied to fields missing in the stored document                 |

### When to use it

```js
// ✅ read-only listing/API responses — no need for Document features
app.get("/users", async (req, res) => {
  const users = await User.find().select("name email").lean();
  res.json(users);
});

// ❌ you intend to modify and save the result — needs a real Document
const user = await User.findById(id); // no .lean()
user.name = "New Name";
await user.save();
```

A good default for **read-only endpoints**: use `.lean()` unless you specifically need a Document feature. Use a full Document when you're going to modify and `.save()`, or rely on instance methods/virtuals.

---

## `.select()` — fetch only the fields you need

```js
const users = await User.find().select("name email");
```

Every field you don't select is data MongoDB doesn't have to send over the network and Mongoose doesn't have to process. This compounds with `.lean()`:

```js
const users = await User.find({ status: "active" })
  .select("name email avatarUrl") // fewer fields over the wire
  .lean(); // and no Document-building overhead
```

Particularly worthwhile when documents contain large fields (long text bodies, big embedded arrays, base64 blobs) that a given endpoint doesn't need.

---

## Avoid loading a document just to check something

```js
// ❌ loads the whole document to answer a yes/no question
const user = await User.findOne({ email });
if (user) {
  /* exists */
}

// ✅ exists() fetches just the _id
if (await User.exists({ email })) {
  /* exists */
}

// ❌ loads every matching document just to count them
const count = (await User.find({ status: "active" })).length;

// ✅ counts on the server without transferring documents
const count = await User.countDocuments({ status: "active" });
```

Covered in `06-crud-methods/02-read-methods.md` — worth repeating here since choosing the lighter method is one of the cheapest optimizations available.

---

## Direct updates instead of load-modify-save

```js
// ❌ two round trips, plus Document overhead
const user = await User.findById(id);
user.loginCount += 1;
await user.save();

// ✅ one round trip, atomic, no Document loaded
await User.updateOne({ _id: id }, { $inc: { loginCount: 1 } });
```

As discussed in `06-crud-methods/03-update-methods.md` and `05-models/02-model-vs-document.md` — the direct update is faster _and_ avoids a race condition, but remember it skips validators unless `runValidators: true` is passed, and skips `pre("save")` middleware.

---

## Batching instead of looping

```js
// ❌ N separate round trips
for (const id of userIds) {
  const user = await User.findById(id);
  results.push(user);
}

// ✅ one query
const users = await User.find({ _id: { $in: userIds } });
```

A loop that awaits a database call on every iteration is one of the most common accidental performance problems in application code — the same principle as `06-crud-methods/06-bulk-and-batch-methods.md` for writes, applied to reads.

### Running independent queries concurrently

```js
// ❌ sequential — total time = A + B + C
const users = await User.find();
const posts = await Post.find();
const comments = await Comment.find();

// ✅ concurrent — total time ≈ the slowest one
const [users, posts, comments] = await Promise.all([
  User.find(),
  Post.find(),
  Comment.find(),
]);
```

If queries don't depend on each other's results, run them concurrently with `Promise.all`.

---

## Limit results, always

```js
const users = await User.find({ status: "active" }).limit(100);
```

An unbounded `find()` on a growing collection will eventually return an enormous result set. Even when you "expect" only a handful of results, a `.limit()` acts as a safety net — see `03-pagination.md` for doing this properly for user-facing lists.

---

## Watch out for `populate()` costs

```js
const posts = await Post.find().populate("author"); // extra query, full author documents by default
```

Field-select on populate, and consider `$lookup` when populate isn't the right tool — full coverage in `11-relationships/04-populate-performance.md`.

---

## Measure, don't guess

```js
const start = Date.now();
const users = await User.find({ status: "active" }).lean();
console.log(
  `Query took ${Date.now() - start}ms, returned ${users.length} documents`,
);
```

```js
mongoose.set("debug", true); // logs every query Mongoose sends to MongoDB — useful in development
```

`mongoose.set("debug", true)` prints each operation and its arguments to the console, which quickly reveals accidental query loops, unexpected extra queries from `populate`, and queries missing an expected filter. Turn it off in production, where the volume of output is both noisy and a potential information leak.

## Common mistakes

- **Using `.lean()` and then trying to call `.save()` or an instance method on the result** — lean results are plain objects.
- **Using `.lean()` everywhere, including where a Document is genuinely needed** — reserve it for read-only paths.
- **Awaiting a query inside a loop** instead of batching with `$in` or running independent queries concurrently with `Promise.all`.
- **Loading full documents to answer existence or count questions** — `exists()` and `countDocuments()` transfer far less data.
- **Fetching every field when only a few are used** — `.select()` is a cheap, easy win.
- **Optimizing by guesswork** instead of measuring with `explain()`, timing, or `mongoose.set("debug", true)`.

## Quick summary

- `.lean()` returns plain objects and skips Document overhead — a large win for read-only queries, at the cost of `.save()`, instance methods, and virtuals
- `.select()` reduces the data transferred and processed; combine it with `.lean()` for the biggest effect on read paths
- Prefer `exists()`/`countDocuments()`, direct atomic updates, `$in` batching, and `Promise.all` for independent queries over heavier or sequential alternatives
- Always bound result sizes with `.limit()`, and measure with `explain()` and `mongoose.set("debug", true)` rather than guessing

## Next

**`03-pagination.md`** covers returning large result sets in pages — and why the obvious `skip`/`limit` approach stops scaling.
