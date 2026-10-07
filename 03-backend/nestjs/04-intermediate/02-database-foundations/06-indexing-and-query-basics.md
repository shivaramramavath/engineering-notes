# Indexing and Query Basics

Most backend performance problems are database problems, and most database problems are one of three things: **a missing index**, **too many queries (N+1)**, or **fetching more than you need**. This note covers the fundamentals that let you spot and fix them. Examples use PostgreSQL; the ideas apply broadly.

Prerequisites: [Database architecture](./01-database-architecture.md), basic SQL.

## What an index is

An index is a separate, sorted data structure (usually a **B-tree**) that lets the database find rows without reading the whole table, like a book's index versus reading every page.

```text
WITHOUT index on email:   scan every row  → O(n)   "Seq Scan"
WITH index on email:      walk the tree   → O(log n) "Index Scan"
```

Cost: every index uses **storage** and slows **writes** (inserts, updates, deletes must maintain it). Indexes are a read/write trade-off, not a free speedup.

## What to index

| Candidate | Why |
|-----------|-----|
| Columns in `WHERE` filters on large tables | Avoid full scans |
| **Foreign key columns** | Joins and cascading deletes; PostgreSQL does **not** index FKs automatically |
| Columns in `ORDER BY` combined with filters/limits | Avoid sorting large result sets |
| Columns with `UNIQUE` requirements | A unique index also enforces integrity ([database errors](./07-database-errors.md)) |

Don't index everything. A small table (a few thousand rows) is often faster to scan. Columns with very few distinct values (a boolean) rarely benefit from a standalone index.

```sql
CREATE INDEX idx_orders_user_id ON orders (user_id);
CREATE UNIQUE INDEX uq_users_email ON users (email);
```

In ORMs: TypeORM `@Index()`, Prisma `@@index([userId])`, Mongoose `schema.index({ userId: 1 })`. Changes still ship via [migrations](./05-migrations.md).

## Composite indexes and column order

An index on `(a, b)` helps queries that filter on `a`, or on `a` **and** `b`, but generally not on `b` alone (the **leftmost prefix** rule).

```sql
CREATE INDEX idx_orders_user_status ON orders (user_id, status);

WHERE user_id = 1                       -- uses the index
WHERE user_id = 1 AND status = 'paid'   -- uses the index fully
WHERE status = 'paid'                   -- usually can't use it efficiently
```

Put columns used with **equality** first, then range/sort columns. Match the index to your most important query shapes rather than creating one per column.

## Reading query plans: `EXPLAIN`

Never guess. Ask the database how it runs the query:

```sql
EXPLAIN ANALYZE
SELECT * FROM orders WHERE user_id = 42 ORDER BY created_at DESC LIMIT 20;
```

What to look for:

| In the plan | Meaning |
|-------------|---------|
| `Seq Scan` on a big table | Reading everything: likely a missing/unused index |
| `Index Scan` / `Index Only Scan` / `Bitmap Index Scan` | Using an index |
| `Sort` with a large cost | Sorting in memory/disk: an index matching the order could avoid it |
| Estimated vs actual rows far apart | Stale statistics: run `ANALYZE` |

`EXPLAIN ANALYZE` actually **executes** the query (including writes), so be careful with `UPDATE`/`DELETE`. Test on realistic data volumes: the planner may prefer a sequential scan on tiny tables, and that's correct.

## Why an index isn't used

Common reasons a query ignores your index:

- **Function on the column**: `WHERE lower(email) = 'a@b.com'` can't use a plain index on `email`. Create an expression index (`ON users (lower(email))`) or store normalized values.
- **Leading wildcard**: `LIKE '%term'` can't use a B-tree. Use full-text search or trigram indexes ([full-text search](../../08-architecture-and-patterns/04-real-world-patterns/07-full-text-search.md)).
- **Type mismatch** between the column and the parameter.
- **Low selectivity**: the filter matches most rows, so scanning is cheaper.
- **Wrong column order** in a composite index.
- **Stale statistics** after large data changes.

## The N+1 query problem

The most common ORM performance bug: one query to load a list, then one extra query **per item** to load a relation.

```ts
// ❌ 1 query for orders + N queries for each order's user
const orders = await ordersRepo.find();
for (const order of orders) {
  order.user = await usersRepo.findOneBy({ id: order.userId });
}
```

With 100 orders that's 101 round trips. Fixes:

```ts
// TypeORM: load the relation in the same query
await ordersRepo.find({ relations: { user: true } });

// Prisma
await prisma.order.findMany({ include: { user: true } });

// Mongoose
await Order.find().populate('user');
```

Or batch with an `IN` query (`WHERE id IN (...)`), or use a batching layer such as DataLoader for GraphQL ([DataLoader and N+1](../../05-advanced/05-graphql/02-dataloader-and-n-plus-one.md)).

Spot it by enabling **query logging** in development and watching for repeated near-identical queries per request. Careful the other direction too: eager-loading deep relation graphs can produce huge joins; load only what the endpoint needs.

## Fetch only what you need

- **Select specific columns** instead of `SELECT *` / full entities when you need two fields (TypeORM `select`, Prisma `select`). It reduces I/O, memory, and can enable index-only scans.
- **Always paginate** list endpoints; never return an unbounded table.
- **Avoid loading rows to count them**: use `COUNT(*)` (and know that exact counts on huge tables are slow).
- **Batch writes** (`INSERT ... VALUES (...), (...)`, `createMany`) instead of one query per row.

## Pagination cost

**Offset pagination** (`LIMIT 20 OFFSET 100000`) makes the database walk and discard 100,000 rows, getting slower on deeper pages and unstable when rows change between requests.

**Keyset (cursor) pagination** seeks directly using an indexed column:

```sql
SELECT * FROM orders
WHERE (created_at, id) < ($1, $2)       -- values from the last row of the previous page
ORDER BY created_at DESC, id DESC
LIMIT 20;
```

Needs a stable, indexed sort key (include a unique tiebreaker like `id`). Use offset for small/admin datasets, keyset for large or infinite-scroll feeds. See [pagination](../08-api-design/03-pagination.md).

## Practical workflow

1. Turn on query logging in development; check query count per request.
2. Find slow queries (slow query log, `pg_stat_statements`, APM).
3. `EXPLAIN ANALYZE` them on realistic data.
4. Add the **minimum** index that fixes the plan; verify it's used and the speedup is real.
5. Re-check write performance if the table is write-heavy.
6. Ship the index through a migration (use `CONCURRENTLY` on big tables, see [migrations](./05-migrations.md)).

More in [database performance](../../07-production/02-performance/03-database-performance.md).

## Common mistakes

- **No index on foreign keys** and filter columns of large tables.
- **Indexing every column** "just in case", slowing writes.
- **Composite index in the wrong order** for the real queries.
- **Wrapping an indexed column in a function** or using `LIKE '%x'`.
- **Not checking `EXPLAIN`**; assuming an index is used.
- **N+1 via lazy loops**, or the opposite: eager-loading everything.
- **`SELECT *` and unbounded lists.**
- **Deep offset pagination** on large tables.
- **Testing performance on tiny local data**, then discovering problems in production.
- **Creating indexes on huge tables without `CONCURRENTLY`**, blocking writes.

## Debugging

- Slow endpoint: count queries first (N+1?), then examine the slowest with `EXPLAIN ANALYZE`.
- Index exists but plan shows `Seq Scan`: check for functions on the column, type mismatch, wildcard, low selectivity, or stale stats (`ANALYZE`).
- Writes slowed after adding an index: too many indexes on that table; remove unused ones (check index usage stats).
- Timeouts under load: look for long-running queries holding connections ([connection](./02-database-connection.md)).

## Quick Summary

- An index speeds reads via a sorted structure at the cost of storage and write speed; index FKs, filters, and sort columns that matter.
- Composite indexes follow the leftmost-prefix rule; put equality columns first.
- Use `EXPLAIN ANALYZE` to verify; common index-killers are functions on columns, leading wildcards, and type mismatches.
- Fix N+1 with joins/`include`/`populate` or batching; select only needed columns; always paginate.
- Prefer keyset pagination for large datasets; ship indexes via migrations.

## Next

[Database errors →](./07-database-errors.md)
