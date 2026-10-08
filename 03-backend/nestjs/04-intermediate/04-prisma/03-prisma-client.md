# Prisma Client

The generated Prisma Client is the query API: one typed delegate per model (`prisma.user`, `prisma.post`) with methods for reading and writing. Its types follow your schema and, importantly, follow the **shape of each query** (`select`/`include`), so the compiler knows exactly what a result contains.

Prerequisites: [Setup](./01-setup.md), [Schema](./02-schema.md).

## Reading

```ts
await prisma.user.findUnique({ where: { id } });                 // by @id/@unique → User | null
await prisma.user.findUniqueOrThrow({ where: { email } });       // throws if not found
await prisma.user.findFirst({ where: { active: true }, orderBy: { createdAt: 'desc' } });
await prisma.user.findMany({ where: { active: true }, take: 20, skip: 0 });
await prisma.user.count({ where: { active: true } });
```

| Method | Use when |
|--------|----------|
| `findUnique` | Filtering by a unique field/combination |
| `findFirst` | Any other "first match" |
| `findMany` | Lists (always bound with `take`) |
| `*OrThrow` variants | You want a thrown `NotFoundError`-style error (`P2025`) instead of `null` |

## Filtering

```ts
await prisma.user.findMany({
  where: {
    active: true,
    role: { in: ['USER', 'ADMIN'] },
    email: { contains: 'example.com', mode: 'insensitive' },   // case-insensitive (PostgreSQL)
    createdAt: { gte: from, lt: to },
    OR: [{ name: { startsWith: 'A' } }, { name: null }],
    NOT: { role: 'ADMIN' },
    posts: { some: { published: true } },                       // relation filter: some / every / none
  },
});
```

Operators include `equals`, `not`, `in`, `notIn`, `lt/lte/gt/gte`, `contains`, `startsWith`, `endsWith`, plus `AND`/`OR`/`NOT`. `null` matches SQL `NULL`.

### The `undefined` trap

Prisma treats `undefined` as "**don't apply this filter**":

```ts
await prisma.user.findMany({ where: { email: undefined } });   // ⚠️ returns EVERY user
```

If `email` is missing from a request, you silently lose the filter, the same class of bug as in [TypeORM](../03-typeorm/04-repositories.md). `findUnique` with an `undefined` unique field throws a validation error at runtime, which is safer, but `findMany`/`updateMany`/`deleteMany` happily widen. Validate inputs at the boundary ([ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)) and treat `undefined` carefully in code that builds `where` objects. Use `null` when you really mean "is NULL".

## `select` vs `include`

```ts
// select: choose exactly the fields you want (including nested relations)
const u = await prisma.user.findUnique({
  where: { id },
  select: { id: true, email: true, posts: { select: { id: true, title: true } } },
});

// include: all scalar fields PLUS the listed relations
const u2 = await prisma.user.findUnique({ where: { id }, include: { posts: true } });
```

- You can't use `select` and `include` at the **same level** of one query (use `select` with nested `select` instead).
- The result **type is inferred** from the query: in the first example, `u.passwordHash` doesn't exist at compile time.
- Prefer `select` for API responses: it limits data transferred and is the simplest way to keep sensitive columns (`passwordHash`) out.
- Recent versions also offer an **`omit`** option to exclude fields (and a global omit setting on the client). Check that your version supports it before relying on it.

To name a result type:

```ts
import { Prisma } from '../generated/prisma/client';

type UserWithPosts = Prisma.UserGetPayload<{ include: { posts: true } }>;
```

## Writing

```ts
const user = await prisma.user.create({ data: { email, passwordHash } });

await prisma.user.createMany({ data: rows, skipDuplicates: true });   // one INSERT, returns { count }

await prisma.user.update({ where: { id }, data: { name: 'New' } });   // throws P2025 if not found
await prisma.user.updateMany({ where: { active: false }, data: { role: 'USER' } });   // returns { count }

await prisma.user.upsert({
  where: { email },
  create: { email, passwordHash },
  update: { name },
});

await prisma.user.delete({ where: { id } });                          // throws P2025 if not found
await prisma.user.deleteMany({ where: { active: false } });
```

Key differences:

- `update`/`delete` target **one** record by a unique filter and **throw** (`P2025`) if it doesn't exist. `updateMany`/`deleteMany` take any filter and return a **count**, never throwing for zero matches.
- `create` returns the created record (all scalars, or per `select`/`include`). It doesn't need a follow-up read.
- `upsert` is atomic only when the `where` is on a unique field and the database supports it natively; otherwise it's emulated (check Prisma's docs for conditions), so a race can still raise a unique violation. Handle `P2002` ([database errors](../02-database-foundations/07-database-errors.md)).

### Atomic number operations

```ts
await prisma.account.update({
  where: { id },
  data: { balance: { decrement: amount }, loginCount: { increment: 1 } },
});
```

`increment`, `decrement`, `multiply`, `divide`, `set` run in the database, avoiding read-modify-write races ([transactions](./07-transactions.md)).

## Ordering and pagination

```ts
// offset
await prisma.post.findMany({ orderBy: [{ createdAt: 'desc' }, { id: 'desc' }], take: 20, skip: 40 });

// cursor (keyset-style)
await prisma.post.findMany({
  orderBy: { id: 'asc' },
  take: 20,
  skip: 1,                       // skip the cursor row itself
  cursor: { id: lastSeenId },
});
```

Always add a **unique tiebreaker** to `orderBy` for stable pages. Deep offsets are slow on large tables; prefer cursors ([pagination cost](../02-database-foundations/06-indexing-and-query-basics.md), [API pagination](../08-api-design/03-pagination.md)). For a page plus total, run `findMany` and `count` together in a `$transaction([...])` ([transactions](./07-transactions.md)).

## Aggregations

```ts
await prisma.order.aggregate({ _sum: { total: true }, _avg: { total: true }, _count: true, where: { status: 'PAID' } });

await prisma.order.groupBy({
  by: ['userId'],
  _count: { _all: true },
  _sum: { total: true },
  having: { total: { _sum: { gt: 1000 } } },
});
```

## Raw SQL

When the query builder isn't enough:

```ts
const rows = await prisma.$queryRaw<{ id: string; total: bigint }[]>`
  SELECT user_id AS id, SUM(total) AS total
  FROM orders
  WHERE status = ${status}
  GROUP BY user_id
`;

await prisma.$executeRaw`UPDATE users SET active = false WHERE last_login < ${cutoff}`;
```

- The **tagged template** form sends `${value}` as **parameters**: safe from injection.
- `$queryRawUnsafe`/`$executeRawUnsafe` accept a plain string and are injection-prone if you concatenate input. Avoid them, and never build SQL from user input.
- Raw results are not mapped through your models: column names are the database's, and numeric aggregates may come back as `bigint` or `Decimal` (watch JSON serialization).
- Identifiers (table/column names, sort direction) can't be parameters; whitelist them.

## Client extensions

`prisma.$extends({...})` adds reusable behavior: computed fields, custom model methods, query interceptors (for example, automatically filtering soft-deleted rows). Extensions are the supported replacement for the old `$use` **middleware**, which was removed in Prisma 7. They return a new extended client, so decide how to expose that through Nest DI (a provider that builds and exports the extended client). See the Prisma docs for the current API.

## Results are plain objects

Unlike TypeORM entities, Prisma returns **plain objects**, not class instances. That means:

- `class-transformer` decorators like `@Exclude()` don't apply unless you convert to a class first ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)). Use `select`/`omit` or map to response DTOs.
- There are no entity hooks or lazy relations; relations are loaded only when you ask.
- Results are easy to serialize, except for `BigInt` and `Decimal` ([schema gotchas](./02-schema.md)).

## Logging and debugging queries

```ts
new PrismaClient({ adapter, log: ['query', 'warn', 'error'] });
```

Watch the query log for N+1 and unexpectedly large queries.

## Common mistakes

- **`undefined` in `where`** for `findMany`/`updateMany`/`deleteMany`, widening the filter to everything.
- **Using `include` and returning the whole object**, leaking `passwordHash`.
- **Assuming `update`/`delete` return `null` when missing**; they throw `P2025`.
- **Using `$queryRawUnsafe`** with string concatenation.
- **No tiebreaker** in `orderBy`, producing unstable pagination.
- **Unbounded `findMany`.**
- **Read-modify-write** instead of `{ increment }`.
- **Serializing `BigInt`/`Decimal` fields** without conversion.
- **Expecting class-transformer decorators** to work on query results.
- **Creating a new client per request.**

## Debugging

- Log queries and read the SQL for the endpoint; count them.
- `P2025` ("record to update not found"): the `where` matched nothing; decide whether that's a 404.
- `P2002`: unique constraint failed; map to 409 ([database errors](../02-database-foundations/07-database-errors.md)).
- Result type lacks a field you expect: your `select` didn't include it (types are query-specific).
- A filter seems ignored: look for an `undefined` value in `where`.
- Raw query returns `bigint`s that break `JSON.stringify`: convert them before responding.

## Quick Summary

- One typed delegate per model; result types follow `select`/`include`, so use `select` (or `omit`) to control exposed fields.
- `undefined` means "no filter"; `null` means `NULL`. Validate inputs so filters don't silently disappear.
- `update`/`delete` throw `P2025` when missing; `updateMany`/`deleteMany` return counts; use atomic operations for counters.
- Paginate with `take` and a stable `orderBy`; use cursors for large datasets.
- Raw SQL via the tagged template is parameterized; avoid the `Unsafe` variants.
- Results are plain objects: map or `select` for responses; use `$extends` for cross-cutting behavior.

## Next

[Relations →](./04-relations.md)
