# Repositories

A Mongoose `Model` is already a data-access API (`find`, `create`, `updateOne`...). Whether to wrap it is the usual judgment call from [the repository pattern](../02-database-foundations/03-repository-pattern.md). With Mongoose there are extra reasons to: chained query objects are painful to mock, hydrated documents vs plain objects is a recurring source of bugs, and **NoSQL injection** protection is easiest to enforce in one place.

Prerequisites: [Schemas and models](./02-schemas-and-models.md), [repository pattern](../02-database-foundations/03-repository-pattern.md).

## Core operations

```ts
// read
await model.find({ active: true }).sort({ createdAt: -1 }).limit(20).lean().exec();
await model.findOne({ email }).lean().exec();                 // T | null
await model.findById(id).lean().exec();                       // T | null
await model.countDocuments({ active: true });                 // exact count with a filter
await model.exists({ email });                                // { _id } | null

// write
const user = await model.create({ email, passwordHash });                    // hydrated document
await model.insertMany(rows);                                                // bulk insert
await model.updateOne({ _id: id }, { $set: { active: false } });             // UpdateResult
const updated = await model.findOneAndUpdate(
  { _id: id },
  { $set: { name } },
  { returnDocument: 'after', runValidators: true },                           // 'after' returns the new document
).lean().exec();
await model.deleteOne({ _id: id });                                          // DeleteResult
await model.bulkWrite(ops);                                                  // many mixed operations, one round trip
```

Notes:

- Queries are **thenables (`Query` objects)**, not real Promises. They execute when awaited or when `.exec()` is called. Calling `.exec()` gives proper Promises and better stack traces; be consistent.
- `findOneAndUpdate` returns the **old** document by default; pass `returnDocument: 'after'` (older code uses `{ new: true }`) to get the updated one.
- `updateOne`/`findOneAndUpdate` **don't run schema validators** unless `runValidators: true`, and they bypass `save` middleware ([hooks](./05-middleware-hooks.md), [schemas](./02-schemas-and-models.md)).
- Use **update operators** (`$set`, `$inc`, `$push`, `$pull`, `$addToSet`) so updates are atomic at the document level: `$inc` avoids read-modify-write races.
- `countDocuments(filter)` is exact but scans matches; `estimatedDocumentCount()` is fast and approximate (whole collection, no filter).

## `lean()`: plain objects vs documents

| | Hydrated document (default) | `lean()` result |
|-|-----------------------------|-----------------|
| Type | `HydratedDocument<T>` | Plain object |
| Methods, virtuals, `id`, `save()` | Yes | **No** |
| Applies `toJSON` transforms | Yes | No |
| Change tracking | Yes | No |
| Memory/CPU | Heavier | Much lighter |

Use **`lean()` for reads** that go straight to a response or mapping; use hydrated documents when you'll modify and `save()`. Mixing them up causes bugs like `user.id` being `undefined`, `user.save is not a function`, or a `toJSON` transform that doesn't apply.

## A repository

```ts
// users.repository.ts
@Injectable()
export class UsersRepository {
  constructor(@InjectModel(User.name) private readonly model: Model<User>) {}

  findById(id: string) {
    this.assertObjectId(id);
    return this.model.findById(id).lean().exec();
  }

  findByEmail(email: string) {
    return this.model.findOne({ email: String(email).toLowerCase() }).lean().exec();   // String(): see injection below
  }

  findCredentialsByEmail(email: string) {
    return this.model.findOne({ email: String(email).toLowerCase() }).select('+passwordHash').lean().exec();
  }

  async create(data: { email: string; passwordHash: string }) {
    try {
      const doc = await this.model.create(data);
      return doc.toObject();
    } catch (err) {
      if (isDuplicateKey(err, 'email')) throw new EmailAlreadyUsedError();
      throw err;
    }
  }

  async list(page: number, limit: number) {
    const filter = { active: true };
    const [items, total] = await Promise.all([
      this.model.find(filter).sort({ createdAt: -1, _id: -1 }).skip((page - 1) * limit).limit(limit).lean().exec(),
      this.model.countDocuments(filter),
    ]);
    return { items, total };
  }

  private assertObjectId(id: string) {
    if (!isValidObjectId(id)) throw new BadRequestException('Invalid id');
  }
}
```

- `select('+passwordHash')` loads a field marked `select: false`, in the one method that needs it.
- Always `sort` before `skip/limit`, with a unique tiebreaker (`_id`), and cap `limit`. For deep pages use cursor pagination on an indexed field (for example `_id < lastId`) ([pagination cost](../02-database-foundations/06-indexing-and-query-basics.md), [API pagination](../08-api-design/03-pagination.md)).
- Return plain objects from the repository so callers don't depend on Mongoose documents. If you define a port (abstract class), type it with your own types ([repository pattern](../02-database-foundations/03-repository-pattern.md)).

## Security: NoSQL operator injection

MongoDB queries are objects, and objects can contain operators. If user input reaches a filter unvalidated:

```ts
// POST /login  { "email": { "$ne": null }, "password": "x" }
await model.findOne({ email: req.body.email });   // ⚠️ matches the first user with any email!
```

The attacker supplied an operator object instead of a string. Defenses (use more than one):

1. **Validate types at the boundary**: `@IsString()` on DTO fields so objects are rejected, and `whitelist` so unknown shapes are stripped ([ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)).
2. **Coerce in the repository**: `String(email)` as above.
3. Enable Mongoose's **`sanitizeFilter`** (`mongoose.set('sanitizeFilter', true)`, or per query) so objects in filters are treated as literal values rather than operators; wrap intentional operators with `mongoose.trusted(...)`. Check the docs for its exact behavior in your version.
4. Never pass `req.query`/`req.body` straight into a filter or update. Build filters from whitelisted fields.

Same risk applies to `$where` and `$regex` built from input (regex DoS); avoid `$where`, escape user text before building regexes ([injection prevention](../../07-production/01-security/05-injection-and-xss-prevention.md)).

## Updates from DTOs

```ts
async update(id: string, dto: UpdateUserDto) {
  const res = await this.model.findByIdAndUpdate(
    id,
    { $set: dto },                              // dto must be a whitelisted DTO, never raw input
    { returnDocument: 'after', runValidators: true },
  ).lean().exec();
  if (!res) throw new NotFoundException();
  return res;
}
```

`$set` with `undefined` values: Mongoose strips `undefined` keys by default (no write), while explicit `null` writes `null`. To remove a field use `$unset`.

## Mapping errors

| Error | How to recognize | Typical mapping |
|-------|------------------|-----------------|
| Duplicate key | `MongoServerError` with `code === 11000` (`keyPattern`/`keyValue` say which field) | `409 Conflict` |
| `CastError` | `err.name === 'CastError'` (invalid ObjectId, wrong type) | `400` (better: validate earlier) |
| `ValidationError` | `err.name === 'ValidationError'` (`errors` per path) | `400`/`422` |
| `DocumentNotFoundError` | thrown by `orFail()` | `404` |
| Connection/selection errors | `MongooseServerSelectionError`, network errors | `503` |

```ts
function isDuplicateKey(err: unknown, field?: string) {
  const e = err as { code?: number; keyPattern?: Record<string, unknown> };
  return e?.code === 11000 && (!field || !!e.keyPattern?.[field]);
}
```

Translate meaningful ones in the repository; keep a global filter for the rest ([database errors](../02-database-foundations/07-database-errors.md), [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md)). As elsewhere, rely on the **unique index**, not a find-then-create pre-check, to prevent duplicates under concurrency. Use `.orFail()` on queries to throw when nothing is found instead of checking for `null`.

## Id validation pipe

```ts
@Injectable()
export class ParseObjectIdPipe implements PipeTransform<string, string> {
  transform(value: string) {
    if (!isValidObjectId(value)) throw new BadRequestException('Invalid id');
    return value;
  }
}

@Get(':id')
findOne(@Param('id', ParseObjectIdPipe) id: string) {}
```

`isValidObjectId` is permissive about some inputs (for example 12-character strings); for strictness check `/^[a-f\d]{24}$/i` too. See [pipes](../../03-core-concepts/01-request-pipeline/03-pipes.md).

## Testing

- **Unit tests:** fake the **repository** (easy), not the Mongoose model (chained `find().sort().lean().exec()` mocks are brittle) ([mocking](../01-testing/03-mocking.md)).
- **Repository tests:** run against a real MongoDB (or `mongodb-memory-server`) to verify filters, indexes, and duplicate-key behavior ([integration testing](../01-testing/05-integration-testing.md)).

## Common mistakes

- **Passing request input directly into filters or updates** (operator injection).
- **Forgetting `.lean()`** on read-heavy paths, or using it and then calling document methods.
- **Using `findOneAndUpdate` without `returnDocument: 'after'`** and returning stale data.
- **Assuming updates run validators or `save` hooks.**
- **No `sort` with `skip/limit`**, giving unstable pages; huge `skip` values.
- **Not validating ObjectIds**, so bad input becomes a 500 `CastError`.
- **`$set: dto` with an unvalidated DTO.**
- **Read-modify-write with `find` + `save`** instead of atomic operators.
- **Returning documents with `passwordHash`/`__v`** to clients.
- **Mocking the Mongoose model deeply** instead of the repository.

## Debugging

- Wrong or stale returned document: check `returnDocument`, and whether you got a lean result or a document.
- `x.save is not a function`: it's a lean object.
- `E11000 duplicate key error ... index: email_1 dup key`: unique index hit; map to 409.
- Query returns everything: a filter value was `undefined`/stripped or an operator object slipped in; log the final filter (`query.getFilter()`).
- Slow query: `explain('executionStats')` ([indexes](./06-indexes.md)).

## Quick Summary

- Wrap the model in a repository to centralize `lean()` usage, id validation, injection defenses, and error mapping; return plain objects.
- Use `lean()` for reads, hydrated docs for modify-and-save; use update operators (`$set`, `$inc`) and `returnDocument: 'after'`.
- Validate input types (NoSQL injection), ids (`CastError`), and whitelist update fields; consider `sanitizeFilter`.
- Updates skip validators and `save` hooks unless you opt in (`runValidators`); duplicates surface as error `11000`.
- Paginate with sorted, bounded queries (cursor for deep pages); test repositories against a real MongoDB.

## Next

[Populate →](./04-populate.md)
