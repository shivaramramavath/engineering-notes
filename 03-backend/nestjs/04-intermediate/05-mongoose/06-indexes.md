# Indexes

Without an index, MongoDB answers a query by scanning every document in the collection (a **COLLSCAN**). Indexes let it jump straight to matching documents (an **IXSCAN**). They also enforce uniqueness and enable special features like TTL expiry and text search. The general ideas are in [indexing and query basics](../02-database-foundations/06-indexing-and-query-basics.md); this note covers MongoDB and Mongoose specifics.

Prerequisites: [Schemas and models](./02-schemas-and-models.md), [repositories](./03-repositories.md).

## Declaring indexes

```ts
@Schema({ timestamps: true })
export class Order {
  @Prop({ required: true, unique: true })            // unique index
  reference: string;

  @Prop({ type: Types.ObjectId, ref: 'User', index: true })   // simple index on a reference
  userId: Types.ObjectId;

  @Prop({ required: true }) status: string;
}

export const OrderSchema = SchemaFactory.createForClass(Order);

OrderSchema.index({ userId: 1, status: 1 });                     // compound index
OrderSchema.index({ createdAt: -1 });                            // descending
OrderSchema.index({ reference: 1 }, { unique: true });           // equivalent to unique: true
```

- `1` ascending, `-1` descending (direction matters mainly for compound indexes used in sorting).
- `_id` is always indexed automatically.
- `@Prop({ unique: true })` creates a **unique index**; it's not a validator ([schemas](./02-schemas-and-models.md)).

## Compound indexes and the ESR rule

A compound index `{ a: 1, b: 1 }` serves queries on `a`, or on `a` and `b` (the **leftmost prefix**), but not on `b` alone. Order the fields using **ESR**:

1. **E**quality fields first (`userId = X`),
2. then **S**ort fields (`createdAt`),
3. then **R**ange fields (`price > 10`).

```ts
// query: find({ userId, status: 'paid' }).sort({ createdAt: -1 })
OrderSchema.index({ userId: 1, status: 1, createdAt: -1 });
```

This lets MongoDB filter and return results in sorted order straight from the index, with no in-memory sort. Design indexes from your **actual query shapes**, and prefer one well-chosen compound index over many single-field ones. Don't duplicate prefixes: `{ userId: 1, status: 1 }` already covers queries on `userId` alone.

## Special index types

| Type | Declaration | Use |
|------|-------------|-----|
| **Unique** | `{ unique: true }` | Enforce uniqueness (e.g. email) |
| **TTL** | `schema.index({ expiresAt: 1 }, { expireAfterSeconds: 0 })` | Auto-delete documents after a time (sessions, tokens, OTPs) |
| **Partial** | `{ partialFilterExpression: { active: true } }` | Index only matching documents; smaller, and enables "unique among active" |
| **Sparse** | `{ sparse: true }` | Skip documents lacking the field |
| **Text** | `schema.index({ title: 'text', body: 'text' })` | Basic full-text search with `$text` |
| **Geospatial** | `{ location: '2dsphere' }` | Location queries |
| **Multikey** | automatic on array fields | Query inside arrays |

```ts
// unique email only among non-deleted users (re-registration after soft delete)
UserSchema.index({ email: 1 }, { unique: true, partialFilterExpression: { deletedAt: null } });

// delete sessions at their expiresAt timestamp
SessionSchema.index({ expiresAt: 1 }, { expireAfterSeconds: 0 });
```

Notes:

- **TTL deletion is a background task** that runs periodically (roughly every minute), so documents can outlive their expiry briefly; don't use TTL for precise timing. TTL indexes need a single-field `Date` index.
- **Unique + missing values:** a unique index treats missing/`null` as a value, so a second document without the field **violates** uniqueness. Use a partial (or sparse) unique index for optional unique fields.
- **Text indexes** are limited (one per collection, basic relevance). For serious search use Atlas Search or a search engine ([full-text search](../../08-architecture-and-patterns/04-real-world-patterns/07-full-text-search.md)).
- **Case-insensitive uniqueness:** store a normalized value (`lowercase: true` on the prop), or use a **collation** on the index/queries; queries must use the same collation to use the index.

## `autoIndex`: don't build indexes on startup in production

By default Mongoose calls `ensureIndexes` for each model when the app starts. That's convenient in development and risky in production:

- Building an index on a large collection takes time and resources, slowing the database while your instances all try at once.
- A failing build (for example a unique index over existing duplicate data) surfaces as startup errors.

```ts
MongooseModule.forRootAsync({
  useFactory: (config: ConfigService) => ({
    uri: config.getOrThrow('MONGODB_URI'),
    autoIndex: config.get('NODE_ENV') !== 'production',
  }),
  inject: [ConfigService],
});
```

In production, manage indexes deliberately: create them in a **migration/deploy script**, or via your database tooling, before the code that depends on them ships. MongoDB has no built-in migration tool; teams use a tool such as `migrate-mongo` or a script that calls `createIndex` ([migrations overview](../02-database-foundations/05-migrations.md)).

```ts
await model.createIndexes();     // create missing indexes declared in the schema
await model.syncIndexes();       // ⚠️ also DROPS indexes not in the schema
```

`syncIndexes()` **drops** any index not declared in your schema, including ones someone created manually for performance. Never run it blindly in production.

### Tests and `init()`

Index creation is asynchronous. In tests, a unique-constraint assertion can race the index build: insert duplicates **before** the index exists and both succeed. Wait for it:

```ts
await model.init();     // resolves when the model's indexes have been built
```

## Checking that an index is used: `explain`

```ts
const plan = await this.orderModel
  .find({ userId, status: 'paid' })
  .sort({ createdAt: -1 })
  .explain('executionStats');
```

Look at:

| Field | Meaning |
|-------|---------|
| `winningPlan.stage` / input stage | `IXSCAN` (index) vs `COLLSCAN` (full scan) |
| `executionStats.totalDocsExamined` vs `nReturned` | Large gap = inefficient (examining many to return few) |
| `executionStats.totalKeysExamined` | Index entries scanned |
| `SORT` stage in the plan | In-memory sort: an index matching the sort could avoid it |
| `executionTimeMillis` | Observed time |

Goal: `totalDocsExamined` close to `nReturned`. In MongoDB Atlas, the Performance Advisor suggests missing indexes from real traffic. Use `mongoose.set('debug', true)` in development to see the queries being sent.

## Things that defeat or limit indexes

- **Unindexed sorts**: MongoDB sorts in memory up to a limit (100 MB by default) and then errors with a sort-memory-limit message. Index your sort fields (ESR).
- **Leading-wildcard or unanchored regex** (`/foo/`) can't use an index efficiently; anchored prefix regexes (`/^foo/`) can (case-sensitive only).
- **`$ne`, `$nin`, negations** are poorly selective.
- **Type mismatches** (querying a string field with a number).
- **Functions/expressions** applied to the field in aggregations (`$expr`) can skip indexes.
- **Low-selectivity** indexes (a boolean on its own) rarely help.
- **Skipping the leading field** of a compound index.

## Index cost

Each index uses memory and disk and slows writes (every insert/update/delete maintains all indexes). Working-set size matters: the hot indexes should fit in RAM. Remove unused indexes (`$indexStats` shows usage) and avoid redundant ones. Don't index every field "just in case".

## Common mistakes

- **No index on fields used in filters and sorts**, causing COLLSCANs as data grows.
- **Wrong field order in compound indexes** (not following equality, sort, range).
- **Leaving `autoIndex` on in production.**
- **Running `syncIndexes()` in production** and dropping hand-made indexes.
- **Unique index on an optional field** without a partial/sparse filter.
- **Expecting TTL deletes to be immediate.**
- **Not awaiting `init()` in tests** before asserting uniqueness.
- **Redundant single-field indexes** covered by compound ones.
- **Never running `explain`**, so slow queries go unnoticed.
- **Creating an index on a huge live collection** without understanding its impact (use a maintenance window or a rolling/background strategy, per MongoDB's guidance for your version).

## Debugging

- Slow query: run `.explain('executionStats')`; check COLLSCAN, docs examined vs returned, and `SORT` stages.
- Duplicate key errors (`11000`) when you didn't expect them: a unique index on an optional field (multiple `null`s), or a stale index from an old schema.
- Duplicates *not* rejected: the unique index wasn't built yet (`autoIndex` off, build failed, or test didn't await `init()`).
- Startup errors about index build: existing data violates a new unique index; clean it up first.
- Index not used: the query shape doesn't match the index prefix, or collation/type differs.
- List what exists: `db.collection.getIndexes()` in `mongosh`, or `model.listIndexes()`.

## Quick Summary

- Index fields you filter and sort on; use compound indexes ordered **Equality, Sort, Range**.
- Know the special types: unique, TTL (approximate timing), partial/sparse (optional-field uniqueness), text, geospatial.
- Turn off `autoIndex` in production; create indexes via deploy scripts; never run `syncIndexes()` blindly.
- Verify with `explain('executionStats')`: look for IXSCAN and docs examined close to returned.
- Indexes cost memory and write speed; remove redundant and unused ones; await `model.init()` in tests.

## Next

[Transactions →](./07-transactions.md)
