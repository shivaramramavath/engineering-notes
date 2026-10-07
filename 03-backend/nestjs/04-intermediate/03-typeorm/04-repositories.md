# Repositories

A TypeORM `Repository<T>` is the object you use to read and write one entity: `find`, `save`, `update`, `delete`. In Nest you get one injected per entity via `forFeature`. This note covers the methods you'll use daily, the difference between `save` and `update`, and the gotchas that cause real bugs (especially `undefined` in `where`).

Prerequisites: [Setup](./01-setup.md), [entities](./02-entities.md), [repository pattern](../02-database-foundations/03-repository-pattern.md).

## Getting a repository

```ts
@Injectable()
export class UsersService {
  constructor(@InjectRepository(User) private readonly users: Repository<User>) {}
}
```

Requires `TypeOrmModule.forFeature([User])` in the module. Inside a transaction you use `manager.getRepository(User)` instead ([transactions](./07-transactions.md)).

## Reading

```ts
await users.find();                                     // all (add a limit in real APIs)
await users.findBy({ active: true });                   // shorthand where
await users.findOneBy({ id });                          // User | null
await users.findOneByOrFail({ id });                    // throws EntityNotFoundError if missing
await users.findAndCount({ where: { active: true }, take: 20, skip: 0 });   // [rows, total]
await users.count({ where: { active: true } });
```

Full options:

```ts
await users.find({
  where: { active: true, role: Role.Admin },            // object = AND
  select: { id: true, email: true },
  relations: { posts: true },
  order: { createdAt: 'DESC' },
  take: 20,
  skip: 40,
  withDeleted: false,
});
```

### `where` operators

```ts
import { In, Like, ILike, Between, MoreThan, LessThanOrEqual, Not, IsNull, Or } from 'typeorm';

where: { id: In([1, 2, 3]) }
where: { email: ILike('%@example.com') }                // ILike is case-insensitive (PostgreSQL)
where: { createdAt: Between(from, to) }
where: { age: MoreThan(18) }
where: { deletedAt: IsNull() }
where: { role: Not(Role.Admin) }
where: [{ role: Role.Admin }, { active: true }]         // array = OR
where: { posts: { published: true } }                   // filter by a relation's column
```

### The `undefined` trap (important)

```ts
await users.findOneBy({ id: undefined });   // ⚠️ returns the FIRST user, not null
await users.find({ where: { email: undefined } });   // ⚠️ condition ignored, returns everyone
```

TypeORM **drops properties whose value is `undefined`** from `where`, so a missing id or email silently becomes "no filter". This has caused real data exposure bugs (a request without an id returns someone's record). Defend against it:

- Validate inputs at the boundary (DTO/pipe: `@IsUUID()`, `ParseIntPipe`) so you never pass `undefined` ([ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)).
- Guard in the repository: `if (!id) throw ...` before querying.
- Use `IsNull()` to match SQL `NULL`; passing `null` directly isn't a reliable way to query for nulls.

## Writing

### `create`, `save`

```ts
const user = users.create({ email, passwordHash });     // builds an instance, does NOT touch the DB
const saved = await users.save(user);                    // INSERT, returns entity with generated id/dates
```

`save` is an **upsert-like** operation: no primary key → `INSERT`; has primary key → it loads the existing row, diffs, and `UPDATE`s changed columns (or inserts if not found). It also runs entity hooks and cascades.

### `insert`, `update`, `delete`, `upsert`

```ts
await users.insert({ email, passwordHash });            // INSERT only, no hooks/cascades, no pre-select
await users.update({ id }, { active: false });          // UPDATE ... WHERE, returns UpdateResult
await users.update(id, { active: false });              // by primary key
await users.delete({ id });                              // DELETE ... WHERE
await users.upsert({ email, name }, ['email']);         // INSERT ... ON CONFLICT (email) DO UPDATE
await users.increment({ id }, 'loginCount', 1);         // atomic UPDATE SET x = x + 1
```

### `save` vs `update`

| | `save(entity)` | `update(criteria, partial)` |
|-|----------------|-----------------------------|
| Extra `SELECT` first | Yes (when a primary key is present) | No |
| Runs hooks (`@BeforeUpdate`...) | Yes | **No** |
| Handles cascades/relations | Yes | No |
| Updates `@UpdateDateColumn` / `@VersionColumn` | Yes | `updatedAt` yes (set by the DB/ORM layer); version handling differs, so verify |
| Returns | The entity | `UpdateResult` (affected count) |
| Best for | Aggregates with relations; need hooks | Simple column changes; performance; atomic ops |

`update` and `delete` **don't tell you whether a row existed** unless you check `result.affected`:

```ts
const result = await users.update({ id }, { active: false });
if (!result.affected) throw new NotFoundException();
```

### Safety rails

- `delete({})`/`update({}, ...)` with **empty criteria** is rejected by TypeORM 0.3 (to prevent wiping a table). `clear()` truncates a table intentionally; keep it out of application code.
- `save` of a plain object with a primary key may **insert** if the row doesn't exist, which can mask bugs. Use `findOneByOrFail` first when you require existence.

### Soft delete and restore

```ts
await users.softDelete(id);          // sets deletedAt
await users.restore(id);
await users.find({ withDeleted: true });
```

Requires a `@DeleteDateColumn()` ([entities](./02-entities.md)).

## Partial updates from DTOs

```ts
async update(id: string, dto: UpdateUserDto) {
  const user = await this.users.findOneByOrFail({ id });
  this.users.merge(user, dto);               // copies defined props onto the entity
  return this.users.save(user);
}
```

or, when you don't need hooks/cascades, a single statement:

```ts
const result = await this.users.update({ id }, dto);
```

Make sure `dto` is a whitelisted DTO, never raw request input, to avoid mass assignment ([DTO](../../03-core-concepts/02-validation-and-serialization/01-dto.md)). Note that `merge`/`update` with `undefined` properties skip them, while explicit `null` writes `NULL`.

## Pagination

```ts
async list(page: number, limit: number) {
  const [items, total] = await this.users.findAndCount({
    where: { active: true },
    order: { createdAt: 'DESC', id: 'DESC' },     // stable ordering: add a tiebreaker
    take: limit,
    skip: (page - 1) * limit,
  });
  return { items, total, page, limit };
}
```

Always set `order` when paginating, and cap `limit` in validation. For deep pages on big tables use keyset pagination instead ([pagination cost](../02-database-foundations/06-indexing-and-query-basics.md), [API pagination](../08-api-design/03-pagination.md)).

## Custom repositories

In TypeORM 0.3, the recommended pattern in Nest is an **injectable class that wraps** the injected repository, exposing intent-named methods:

```ts
@Injectable()
export class UsersRepository {
  constructor(@InjectRepository(User) private readonly repo: Repository<User>) {}

  findActiveByEmail(email: string) {
    return this.repo.findOneBy({ email, active: true });
  }

  async listAdmins() {
    return this.repo.find({ where: { role: Role.Admin }, order: { createdAt: 'DESC' } });
  }
}
```

Register it in `providers` and export a service, not the repository. Whether to add this layer at all is discussed in [repository pattern](../02-database-foundations/03-repository-pattern.md). (`dataSource.getRepository(User).extend({ ... })` is another way to add methods, but it doesn't integrate with Nest's DI as cleanly.)

## Raw queries

```ts
await users.query('SELECT * FROM users WHERE email = $1', [email]);   // parameter placeholders are driver-specific ($1 for PostgreSQL, ? for MySQL)
```

Always use parameters; never concatenate input into SQL. For anything non-trivial prefer the [query builder](./05-query-builder.md).

## Error handling

- `findOneOrFail`/`findOneByOrFail` throw `EntityNotFoundError`; map it to `404` in a filter or catch it.
- Constraint violations throw `QueryFailedError` (check `driverError.code`) ([database errors](../02-database-foundations/07-database-errors.md)).

```ts
try {
  await this.users.insert(data);
} catch (err) {
  if (err instanceof QueryFailedError && (err as any).driverError?.code === '23505') {
    throw new ConflictException('Email already registered');
  }
  throw err;
}
```

## Testing

Unit tests mock the repository with `getRepositoryToken(User)`; verify queries and constraints in [integration tests](../01-testing/05-integration-testing.md) against a real database.

## Common mistakes

- **`undefined` in `where`**, turning a lookup into "first row" or "everyone".
- **Using `save` for everything** and paying an extra `SELECT` per call (or `update` and expecting hooks).
- **Ignoring `result.affected`** for `update`/`delete`.
- **No `order` with `take`/`skip`**, producing unstable pages.
- **Unbounded `find()`** on large tables.
- **Passing raw request bodies** to `update`/`merge`.
- **Concatenating SQL strings** in `query()`.
- **Loading entities to modify one column** when `update`/`increment` would do (and avoid read-modify-write races).
- **Exporting repositories** from modules rather than services.
- **Expecting `@UpdateDateColumn`/hooks to behave identically across `save` and `update`.**

## Debugging

- Wrong or "random" row returned: look for `undefined` in `where` (log the values).
- Extra queries: enable query logging and compare `save` vs `update` behavior.
- `EntityNotFoundError` surfacing as 500: add a mapping in an [exception filter](../../03-core-concepts/01-request-pipeline/08-exception-filters.md).
- Update seems to do nothing: check `affected` and whether the `where` matched.
- Relations missing after `save`: `save` returns the entity you passed (with generated columns), not a re-fetched graph; reload if you need relations.

## Quick Summary

- Inject `Repository<T>` via `@InjectRepository` + `forFeature`; use `find*`, `save`, `update`, `delete`, `upsert`, `increment`.
- **`undefined` in `where` is ignored**: validate inputs and guard against it.
- `save` = select + insert/update with hooks and cascades; `update` = single statement, no hooks, check `affected`.
- Always order and cap paginated queries; use partial updates from whitelisted DTOs.
- Wrap repositories in an injectable class for intent-named methods; parameterize raw SQL.

## Next

[Query builder →](./05-query-builder.md)
