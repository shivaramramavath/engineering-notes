# 12 · Database

Where database code lives in a Next.js App Router app, and how to use PostgreSQL with Prisma or Drizzle, MongoDB with Mongoose, and transactions without breaking data in the process.

> Verified against the Next.js 16.4 Data Security guide, the Prisma 7 and 8 docs, Drizzle's PostgreSQL and transactions docs, and the Mongoose Next.js and transactions guides (July to October 2026 pages). ORM versions move fast; the version-sensitive parts are marked in each note.

## Start here: pick the pieces

```text
Relational data, most apps ────► PostgreSQL
   ├─ Schema-first, generated client, great DX ──► Prisma   (02)
   └─ SQL-like, TypeScript schema, thin layer ───► Drizzle  (03)
Document data, flexible shape ─► MongoDB + Mongoose        (04)
Several writes that must succeed together ────► a transaction (05)
```

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Database Architecture](./00-database-architecture.md) | Where DB code lives, the DAL, connections in dev and serverless, migrations, security |
| 01 | [PostgreSQL](./01-postgresql.md) | Schema design, indexes, SQL you need, the `pg` driver, pooling |
| 02 | [Prisma](./02-prisma.md) | Prisma 7 setup, client singleton, queries, migrations, Prisma 8 differences |
| 03 | [Drizzle](./03-drizzle.md) | Schema in TypeScript, `drizzle-kit`, queries, relations |
| 04 | [MongoDB and Mongoose](./04-mongodb-mongoose.md) | Connecting from Next.js, models, serialization, indexes |
| 05 | [Transactions](./05-transactions.md) | ACID, isolation, retries, per-tool APIs, patterns for Server Actions |

## The rules to remember

1. **Database access happens on the server only,** inside a `server-only` Data Access Layer.
2. **Only the DAL reads `process.env`** for connection strings and secrets.
3. **One client per process.** Reuse it; guard against hot-reload duplicates in development.
4. **Parameterize every query.** ORMs do it for you; with raw SQL use placeholders, never string concatenation.
5. **Return DTOs,** not rows or documents, to components and from actions.
6. **Run migrations in CI or a release step,** not on app start-up in every instance.
7. **Several writes that depend on each other go in one transaction,** and transactions stay short.
8. **Mutations end in revalidation:** the database is the source of truth, the Next.js cache is a copy.

## Next

[13 · Next chapter](../README.md): see the repo root README for the folder name.
