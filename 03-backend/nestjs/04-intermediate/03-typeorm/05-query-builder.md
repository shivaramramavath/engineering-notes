# Query Builder

When `find` options aren't expressive enough (dynamic filters, joins with conditions, aggregates, subqueries, bulk updates), TypeORM's **QueryBuilder** lets you compose SQL step by step while keeping parameters safe and results mapped to entities.

Prerequisites: [Repositories](./04-repositories.md), [relations](./03-relations.md), basic SQL.

## Basics

```ts
const users = await this.usersRepo
  .createQueryBuilder('u')                         // 'u' is the alias for User
  .where('u.active = :active', { active: true })
  .andWhere('u.email ILIKE :q', { q: `%${term}%` })
  .orderBy('u.createdAt', 'DESC')
  .take(20)
  .skip(0)
  .getMany();
```

Two rules that prevent most bugs:

1. **Values go in parameters** (`:name` + object), **never** concatenated into the string. Parameters are escaped by the driver.
2. Refer to **property names** with the alias (`u.createdAt`), not raw column names, so TypeORM maps them for you.

## Getting results

| Method | Returns |
|--------|---------|
| `getMany()` / `getOne()` | Entities (hydrated, relations from `...AndSelect` joins) |
| `getManyAndCount()` | `[entities, totalCount]` ignoring `take`/`skip` for the count |
| `getCount()` | Number of matching rows |
| `getRawMany()` / `getRawOne()` | Plain row objects with raw column aliases |
| `getQuery()` / `getSql()` | The SQL string (for debugging) |
| `execute()` | Result of an insert/update/delete builder |
| `stream()` | A stream of raw rows for very large result sets |

Use `getRaw*` when you select aggregates or computed columns that don't map to an entity:

```ts
const stats = await this.ordersRepo
  .createQueryBuilder('o')
  .select('o.userId', 'userId')
  .addSelect('COUNT(*)', 'orders')
  .addSelect('SUM(o.total)', 'revenue')
  .where('o.status = :status', { status: 'paid' })
  .groupBy('o.userId')
  .having('COUNT(*) > :min', { min: 5 })
  .getRawMany();
// [{ userId: '...', orders: '7', revenue: '123.45' }]   ← aggregates come back as strings
```

Aggregates (`COUNT`, `SUM`) arrive as **strings** from PostgreSQL; convert them.

## Joins

```ts
const posts = await this.postsRepo
  .createQueryBuilder('p')
  .leftJoinAndSelect('p.author', 'a')              // join and load into post.author
  .leftJoinAndSelect('p.tags', 't')
  .where('a.active = :active', { active: true })
  .andWhere('t.name = :tag', { tag })
  .getMany();
```

| Method | Meaning |
|--------|---------|
| `innerJoin` / `leftJoin` | Join for filtering/ordering only; relation **not** loaded |
| `innerJoinAndSelect` / `leftJoinAndSelect` | Join **and** load into the entity |
| `leftJoin(Entity, 'alias', 'condition')` | Join an unrelated entity/table with an explicit condition |

Watch out: filtering on a joined collection in `where` (`t.name = :tag`) also **filters the loaded collection**, so a post matching one tag only shows that tag in `post.tags`. To filter parents by a child condition without trimming the collection, join twice (once to filter, once to select) or use a subquery/`EXISTS`.

A `where` on a left-joined table can turn it into an inner join in effect. Put the condition in the join's `ON` clause if you need to keep parent rows without matches:

```ts
.leftJoinAndSelect('p.comments', 'c', 'c.approved = :approved', { approved: true })
```

## Dynamic filters

The query builder's main strength is building queries from optional criteria:

```ts
async search(filter: { q?: string; role?: Role; from?: Date }, page: number, limit: number) {
  const qb = this.usersRepo.createQueryBuilder('u');

  if (filter.q) qb.andWhere('(u.email ILIKE :q OR u.name ILIKE :q)', { q: `%${filter.q}%` });
  if (filter.role) qb.andWhere('u.role = :role', { role: filter.role });
  if (filter.from) qb.andWhere('u.createdAt >= :from', { from: filter.from });

  return qb.orderBy('u.createdAt', 'DESC').addOrderBy('u.id', 'DESC')
    .take(limit).skip((page - 1) * limit)
    .getManyAndCount();
}
```

Notes:

- Start with `where`/`andWhere`; calling `.where()` again **replaces** earlier conditions. Use `andWhere`/`orWhere` to add.
- Group `OR` logic with `Brackets` so precedence is explicit:

```ts
import { Brackets } from 'typeorm';

qb.where('u.active = :active', { active: true })
  .andWhere(new Brackets((b) => b.where('u.role = :a', { a: 'admin' }).orWhere('u.role = :m', { m: 'manager' })));
```

- **Parameter names are global to the query.** Reusing `:q` with different values in separate conditions overwrites the earlier value. Use unique names (`:q1`, `:q2`).
- Arrays use the spread form: `.where('u.id IN (:...ids)', { ids })` (an empty array produces invalid SQL in some cases; guard it).

## Security: SQL injection and dynamic identifiers

Parameters protect **values**. They don't protect **identifiers** (column names, sort direction):

```ts
// ❌ injection risk: sort comes from the request
qb.orderBy(`u.${req.query.sort}`, req.query.dir);

// ✅ whitelist
const SORTABLE = { createdAt: 'u.createdAt', email: 'u.email' } as const;
const column = SORTABLE[sort as keyof typeof SORTABLE] ?? SORTABLE.createdAt;
qb.orderBy(column, dir === 'ASC' ? 'ASC' : 'DESC');
```

Never interpolate user input into `where` strings, `orderBy`, `select`, or table names. See [injection and XSS prevention](../../07-production/01-security/05-injection-and-xss-prevention.md).

## Pagination: `take/skip` vs `limit/offset`

| | `take()` / `skip()` | `limit()` / `offset()` |
|-|---------------------|------------------------|
| Join-safe | **Yes**: TypeORM selects distinct parent ids first | No: applies to the joined row set, so parents can be cut off or duplicated |
| Performance | May use an extra query when joins are present | Single query |

Use `take/skip` with entity queries that join collections. Use `limit/offset` for raw/aggregate queries with no row multiplication.

Keyset pagination with the builder:

```ts
qb.where('(u.createdAt, u.id) < (:createdAt, :id)', { createdAt: cursor.createdAt, id: cursor.id })
  .orderBy('u.createdAt', 'DESC').addOrderBy('u.id', 'DESC')
  .take(limit);
```

(Row-value comparison syntax works in PostgreSQL and MySQL; check your database. Cost explanation: [indexing and query basics](../02-database-foundations/06-indexing-and-query-basics.md).)

## Subqueries

```ts
const qb = this.usersRepo.createQueryBuilder('u')
  .where((qb) => {
    const sub = qb.subQuery()
      .select('1')
      .from(Order, 'o')
      .where('o.userId = u.id')
      .andWhere('o.status = :status')
      .getQuery();
    return `EXISTS ${sub}`;
  })
  .setParameter('status', 'paid');
```

`EXISTS` is usually clearer and faster than joining just to filter. Parameters for subqueries are set on the outer builder.

## Insert, update, delete builders

```ts
// bulk insert
await dataSource.createQueryBuilder().insert().into(User)
  .values([{ email: 'a@x.com' }, { email: 'b@x.com' }])
  .orIgnore()                                     // ON CONFLICT DO NOTHING (PostgreSQL)
  .execute();

// upsert (PostgreSQL): overwrite columns, conflict target
await dataSource.createQueryBuilder().insert().into(User)
  .values({ email, name })
  .orUpdate(['name'], ['email'])
  .execute();

// update with an expression
await dataSource.createQueryBuilder().update(Account)
  .set({ balance: () => 'balance - :amt' })
  .where('id = :id AND balance >= :amt', { id, amt: amount })
  .execute();                                     // check result.affected

// delete
await dataSource.createQueryBuilder().delete().from(Session).where('expiresAt < now()').execute();
```

Like `update`/`delete` on repositories, these **skip entity hooks and cascades**, and the atomic `balance = balance - :amt` form avoids read-modify-write races ([transactions](./07-transactions.md)). `.softDelete()` and `.restore()` builders exist for soft delete.

## Eager relations and soft deletes

- `eager: true` relations are **not** loaded by QueryBuilder; use `...JoinAndSelect`.
- Soft-deleted rows are excluded automatically for `@DeleteDateColumn` entities; use `.withDeleted()` to include them.

## When to use it

| Use | Instead of |
|-----|------------|
| Dynamic, optional filters | Building `where` objects with many conditionals (often fine too) |
| Aggregates and `GROUP BY` | Loading rows and aggregating in JS |
| Conditional joins, `EXISTS`, subqueries | Multiple round trips |
| Bulk update/delete/upsert | Loading entities to modify them one by one |

For simple lookups `find` options are shorter and less error-prone. For genuinely complex reporting SQL, `dataSource.query()` with parameters can be clearer than a deeply nested builder.

## Common mistakes

- **Interpolating user input** into SQL strings or `orderBy`.
- **Calling `.where()` twice**, discarding the first condition.
- **Reusing a parameter name** with different values.
- **Using `limit/offset` with joined collections.**
- **Filtering a joined collection and unintentionally trimming it.**
- **Forgetting aggregates are strings**, or that `getRawMany` returns raw alias keys.
- **Empty arrays in `IN (:...ids)`.**
- **Expecting `eager` relations or hooks** to apply in the builder.
- **Unbracketed `OR`** breaking precedence.
- **Raw column names** instead of aliased property names, breaking under a naming strategy.

## Debugging

- Print the SQL: `console.log(qb.getQuery(), qb.getParameters())`, or enable `logging: ['query']`.
- Wrong results with joins: check `leftJoin` vs `innerJoin` and where conditions on joined aliases.
- "Parameter ... was not provided": a `:name` in the SQL without a matching key (or a typo).
- Duplicate or missing parents with pagination: switch from `limit/offset` to `take/skip`.
- Slow query: `EXPLAIN ANALYZE` the printed SQL ([indexing](../02-database-foundations/06-indexing-and-query-basics.md)).

## Quick Summary

- QueryBuilder composes SQL with safe parameters and entity mapping: `createQueryBuilder('alias')` → `where/join/orderBy` → `getMany/getRawMany/execute`.
- Values go in `:params`; **identifiers must be whitelisted**, never interpolated.
- Use `andWhere` (not repeated `where`), `Brackets` for OR groups, unique parameter names.
- `take/skip` is join-safe; `limit/offset` is not. Aggregates come back as strings.
- Insert/update/delete builders are great for bulk and atomic ops but skip hooks and cascades.

## Next

[Migrations →](./06-migrations.md)
