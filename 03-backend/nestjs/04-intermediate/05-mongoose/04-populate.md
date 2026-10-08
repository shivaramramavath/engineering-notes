# Populate

MongoDB has no foreign keys and (conceptually) no joins; related data is either **embedded** inside a document or **referenced** by id. `populate()` is Mongoose's way to replace referenced ids with the actual documents. The decisions that matter most are **whether to embed or reference** and **how populate actually works** (it's an application-level join, not a database one).

Prerequisites: [Schemas and models](./02-schemas-and-models.md), [repositories](./03-repositories.md).

## Embed or reference? (decide first)

| | **Embed** (subdocument in the same document) | **Reference** (store `ObjectId`, look up separately) |
|-|----------------------------------------------|------------------------------------------------------|
| Read together? | Yes, always: one query | Sometimes: extra query (or `$lookup`) |
| Size | Small and **bounded** | Unbounded or large |
| Shared by many parents? | No (owned by one) | Yes |
| Updated independently? | Rarely | Often |
| Atomicity | Single-document update is atomic | Needs a [transaction](./07-transactions.md) across documents |
| Integrity | Lives and dies with the parent | **No foreign key enforcement**; dangling refs possible |

Rules of thumb:

- A user's **addresses** or an order's **line items** (owned, bounded, read together) → **embed**.
- A post's **author**, a product's **category**, a **comment thread** that can grow without limit → **reference**.
- Never embed an array that can grow without bound: documents have a 16 MB limit and large arrays hurt performance. The usual alternative is to reference "up" from the many side (each comment stores `postId`) rather than storing an ever-growing array of comment ids in the post.
- Duplicating a few fields (denormalizing, for example storing `authorName` on a post) trades update complexity for read speed. It's legitimate in MongoDB, but you must keep copies in sync.

## Defining a reference

```ts
@Schema({ timestamps: true })
export class Post {
  @Prop({ required: true }) title: string;

  @Prop({ type: Types.ObjectId, ref: User.name, required: true, index: true })
  author: Types.ObjectId;                          // stores the id; `ref` names the model to populate from
}
export const PostSchema = SchemaFactory.createForClass(Post);
```

`ref` takes the **model name** (the same string you registered in `forFeature`, i.e. `User.name`). The referenced model must be registered on the same connection for populate to work. Index reference fields you query or populate through.

## Populating

```ts
const post = await this.postModel
  .findById(id)
  .populate('author')                              // replaces the id with the full User document
  .lean()
  .exec();

// select fields, filter, and populate nested paths
await this.postModel.find().populate({
  path: 'author',
  select: 'email name -_id',                       // only what the response needs
  match: { active: true },                         // if no match, author becomes null
});

// nested
await this.postModel.find().populate({
  path: 'comments',
  populate: { path: 'author', select: 'name' },
});

// multiple paths
await this.postModel.find().populate(['author', 'tags']);
```

Always `select` the fields you need, especially to keep sensitive ones (`passwordHash`) out of populated documents. `select: false` on the schema also helps ([schemas](./02-schemas-and-models.md)).

## How populate works (and what it costs)

`populate` is **not** a database join. Mongoose runs the main query, collects the referenced ids, then issues **additional queries** (one per populated path, batched with `$in`) and stitches the results together in your application.

```text
find posts  ──►  collect author ids  ──►  find users where _id IN [...]  ──►  attach to posts
   query 1                                         query 2
```

Consequences:

- It is **not N+1** (queries are batched per path), but each populated path adds a round trip, and nested populates multiply them.
- Large `$in` lists and deep populates get slow; keep populated sets small and select few fields.
- `limit` inside `populate` applies **per query across all parents**, not per document, so it doesn't give "5 comments per post". For per-parent limits use a different data model or an aggregation.
- Results are consistent only per query; the data isn't read as one snapshot.

## `$lookup`: database-level joins

For joins performed in MongoDB itself, use an aggregation:

```ts
await this.postModel.aggregate([
  { $match: { published: true } },
  { $lookup: { from: 'users', localField: 'author', foreignField: '_id', as: 'author' } },
  { $unwind: '$author' },
  { $project: { title: 1, 'author.email': 1 } },
]);
```

| | `populate` | `$lookup` |
|-|-----------|-----------|
| Where it runs | Mongoose (extra queries) | MongoDB (one aggregation) |
| Returns | Mongoose documents (or lean) | Plain objects, no casting/virtuals |
| Filtering/sorting on joined fields | Awkward | Natural |
| Sharded collections | Fine | Has limitations on sharded `from` collections |
| Ergonomics | Simple | Verbose; `from` is the **collection** name, not the model name |

Use `populate` for simple "attach the related docs" reads; use `$lookup` when you need to filter, sort, or aggregate **by** joined data. If you find yourself `$lookup`-ing constantly, reconsider the data model.

## Virtual populate (the reverse direction without arrays of ids)

To get "all posts of a user" without storing a growing `posts: [ObjectId]` array on the user:

```ts
UserSchema.virtual('posts', {
  ref: 'Post',
  localField: '_id',
  foreignField: 'author',                           // the field on Post that points back
});
UserSchema.set('toJSON', { virtuals: true });
UserSchema.set('toObject', { virtuals: true });

await this.userModel.findById(id).populate('posts').lean({ virtuals: true }).exec();
```

Needs an **index on `Post.author`** or it scans the collection. Virtuals aren't included in `lean()` results unless you ask (the `virtuals` option needs the `mongoose-lean-virtuals` plugin in some versions; check the docs for yours).

## Typing populated results

A path typed `Types.ObjectId` is a `User` after populate, which TypeScript can't know. Options:

- Define separate types: `type PostWithAuthor = Omit<Post, 'author'> & { author: User }` and cast at the repository boundary.
- Use `PopulatedDoc<User>` in the schema type (it becomes a union, so you need narrowing).
- Return **mapped response DTOs** from the repository, so callers never see either shape.

Hide the cast inside the [repository](./03-repositories.md) rather than spreading unions through services.

## Integrity: no cascades, no foreign keys

- Deleting a user does **not** delete or null their posts. Handle it in the service (inside a [transaction](./07-transactions.md) if needed), or with hooks ([middleware](./05-middleware-hooks.md)), knowing hooks don't cover bulk operations.
- A reference to a deleted/missing document populates as **`null`** (or is dropped from an array) with no error, which can hide data problems. Handle `null` explicitly.
- Orphan cleanup jobs are sometimes needed.

If you need strong referential integrity everywhere, that's a signal a relational database may fit better ([architecture](../02-database-foundations/01-database-architecture.md)).

## Common mistakes

- **Referencing when embedding is simpler** (and the reverse: embedding unbounded arrays).
- **Populating full documents** and leaking sensitive fields; always `select`.
- **Deep populate chains** that multiply round trips.
- **Expecting `populate({ limit })` to limit per parent.**
- **Forgetting an index** on the foreign field of virtual populate or reference lookups.
- **Not handling `null`** from dangling references or `match`.
- **Wrong `ref` name** (model name vs collection name): `populate` returns the unpopulated id, or `MissingSchemaError` is thrown if the model isn't registered.
- **Using `populate` where `$lookup`** is needed to filter or sort by joined fields.
- **Storing ever-growing arrays of ids** on the parent.
- **Assuming cascading deletes.**

## Debugging

- Populated field still an id: the `ref` doesn't match a registered model, or the path name is wrong (`strictPopulate` normally throws for unknown paths in recent versions).
- `MissingSchemaError: Schema hasn't been registered for model "User"`: the referenced model isn't registered in `forFeature` on that connection.
- Populated value is `null`: the referenced document is missing, or `match` excluded it.
- Slow populate: log queries (`mongoose.set('debug', true)` in development) and check indexes and `select`.
- `toJSON` missing virtuals: set `toJSON: { virtuals: true }` on the schema (and note `lean()` differences).

## Quick Summary

- Embed small, bounded, owned, read-together data; reference unbounded, shared, or independently changing data.
- `populate` is an application-level join (extra batched queries); use `select`, keep it shallow, and index reference fields.
- `$lookup` joins inside MongoDB and suits filtering/sorting by joined data; virtual populate avoids unbounded id arrays.
- References aren't enforced: no cascades, dangling refs become `null`.
- Hide populated-type casts inside the repository.

## Next

[Middleware and hooks →](./05-middleware-hooks.md)
