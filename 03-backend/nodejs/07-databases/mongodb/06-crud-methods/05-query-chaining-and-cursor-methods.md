# Query Chaining & Cursor Methods

Mongoose query methods like `find()` don't immediately execute — they return a chainable `Query` object, letting you refine it with `.sort()`, `.limit()`, `.select()`, and more before it actually runs. This file covers that chaining mechanism and the methods you'll use on it constantly.

## The key concept: a `Query` is a thenable, not a Promise (but works like one)

```js
const query = User.find({ status: "active" });

query instanceof mongoose.Query; // true — not yet executed
query.sort({ createdAt: -1 }).limit(10); // still not executed — just configuring it further

const results = await query; // NOW it actually runs against the database
```

Nothing hits the database until the query is `await`ed (or `.exec()`/`.then()` is called on it) — this is what makes chaining possible, since each chained method just adds configuration to the same not-yet-run query object, rather than immediately triggering a separate operation.

---

## `.sort()`

```js
User.find().sort({ age: 1 }); // ascending
User.find().sort({ age: -1 }); // descending
User.find().sort({ lastName: 1, firstName: 1 }); // multiple fields
User.find().sort("age"); // string shorthand — ascending
User.find().sort("-age"); // string shorthand — descending (leading "-")
```

---

## `.limit()` / `.skip()`

```js
User.find().limit(10);
User.find().skip(20).limit(10); // a naive pagination approach — see 10-performance/03-pagination.md for better alternatives on large collections
```

---

## `.select()` — projections

```js
User.find().select("name email"); // include only these (space-separated string)
User.find().select("-password -__v"); // exclude these (leading "-")
User.find().select({ name: 1, email: 1 }); // object form, same as raw MongoDB projections
```

```js
User.find().select("+password"); // explicitly INCLUDE a field marked select: false in the schema
```

Same inclusion/exclusion rules as raw MongoDB projections — don't mix inclusion and exclusion styles in one call (aside from the `_id` exception).

---

## `.lean()` — skip Mongoose Document overhead

```js
const users = await User.find().lean();
```

```js
users[0] instanceof mongoose.Document; // false — a plain JavaScript object
users[0].save; // undefined — no Document methods available at all
```

`.lean()` returns plain JavaScript objects instead of full Mongoose Documents — meaningfully faster and lighter on memory for read-only operations, since Mongoose skips wrapping each result with change-tracking, virtuals, and instance methods. The trade-off: no `.save()`, no virtuals (unless manually computed), no instance methods on the result. Full depth on when this trade-off is worth it in `14-performance/02-query-optimization-and-lean.md`.

---

## `.populate()` — resolving references

```js
const posts = await Post.find().populate("author");
```

```js
posts[0].author.name; // the actual referenced User document's field, not just an ObjectId
```

Fetches the referenced document(s) and substitutes them in place of the stored `ObjectId` — full depth (including selecting specific fields to populate, nested populate, and performance considerations) in `11-relationships/01-references-and-populate.md`.

```js
Post.find().populate("author", "name email"); // only populate these fields from the referenced document
```

---

## Chaining order: does it matter?

```js
User.find({ status: "active" }).sort({ age: 1 }).limit(10).select("name email");
User.find({ status: "active" }).select("name email").limit(10).sort({ age: 1 });
```

Both produce the **same result** — you can chain these methods in whatever order reads most naturally in your code; Mongoose assembles the full query configuration regardless of the order the chained calls were written in, then executes it as one operation with the filter, sort, limit, skip, and projection all applied together (in that logical order, at the database level, which is fixed regardless of your chain's written order).

---

## `.exec()` — when you actually need it

```js
const promise = User.find({ status: "active" }).exec();
```

```js
const users = await User.find({ status: "active" }); // works fine without .exec() too
```

For most code, `.exec()` is **optional** — `await`ing a `Query` directly (or calling `.then()` on it) works identically. `.exec()` is mainly useful when you want an explicit, "real" Promise object to pass around or combine with `Promise.all()`, since a Mongoose `Query` is thenable but not, strictly speaking, a native `Promise` instance:

```js
const [users, posts] = await Promise.all([
  User.find().exec(),
  Post.find().exec(),
]);
```

`Promise.all` works fine with plain (non-`.exec()`'d) Mongoose queries too in practice, since they're properly thenable — `.exec()` is more a matter of explicitness/preference than a strict requirement in modern Mongoose.

---

## A fully chained, realistic example

```js
const posts = await Post.find({ published: true })
  .select("title excerpt createdAt author")
  .populate("author", "name")
  .sort({ createdAt: -1 })
  .limit(20)
  .lean();
```

Filter, then project only needed fields, populate the author's name, sort newest-first, cap at 20 results, and skip Mongoose Document overhead since this is a read-only listing page that doesn't need to `.save()` anything.

## Common mistakes

- **Forgetting a `Query` doesn't execute until awaited** — building a query object and never actually consuming it (missing `await`/`.then()`) means the operation never runs at all.
- **Using `.lean()` and then trying to call an instance method or `.save()` on the result** — `.lean()` results are plain objects with none of that available.
- **Assuming chaining order changes the result** — it doesn't; chain in whatever order is most readable.
- **Adding `.exec()` out of habit everywhere** without a real need for an explicit Promise — harmless, but unnecessary in most modern code.

## Quick summary

- `find()`/`findOne()` return a chainable `Query` that doesn't execute until awaited — `.sort()`/`.limit()`/`.skip()`/`.select()`/`.populate()`/`.lean()` all configure it further before that happens
- `.lean()` trades Document features (`.save()`, virtuals, instance methods) for meaningfully better performance on read-only queries
- `.populate()` resolves a reference field into the actual referenced document
- Chaining order doesn't affect the result — write it however reads most clearly
- `.exec()` is optional in most modern code; `await`ing the query directly works the same way

## Next

**`06-bulk-and-batch-methods.md`** covers performing many writes efficiently in a single call, closing out this section.
