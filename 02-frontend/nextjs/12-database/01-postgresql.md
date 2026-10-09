# PostgreSQL

PostgreSQL is the default relational choice for most Next.js apps: strong consistency, transactions, rich types and a large ecosystem of hosts. This note covers the SQL and design you need whichever ORM sits on top, then connecting with the plain `pg` driver.

> General PostgreSQL knowledge plus the Prisma connection guidance and Drizzle setup docs. Check your host's docs for pooling and TLS settings.

## When to use it

| Good fit | Consider something else |
|---|---|
| Users, orders, posts, anything with relations | Pure caching or sessions (Redis) |
| Reporting, filtering, sorting, aggregates | Huge unstructured documents with changing shape ([MongoDB](./04-mongodb-mongoose.md)) |
| Data that must stay consistent ([transactions](./05-transactions.md)) | Full-text search at large scale (a search engine) |

## Designing tables

```sql
CREATE TABLE users (
  id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  email       text NOT NULL UNIQUE,
  name        text NOT NULL,
  role        text NOT NULL DEFAULT 'user' CHECK (role IN ('user','editor','admin')),
  password_hash text,
  created_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE posts (
  id         bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  author_id  uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  title      text NOT NULL,
  body       text NOT NULL DEFAULT '',
  status     text NOT NULL DEFAULT 'draft' CHECK (status IN ('draft','published')),
  created_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX posts_author_created_idx ON posts (author_id, created_at DESC);
```

Principles:

| Principle | Why |
|---|---|
| Every table has a primary key | Identity, joins, ORMs expect it |
| Use `NOT NULL`, `UNIQUE`, `CHECK`, `FOREIGN KEY` | The database enforces rules even if app code has a bug |
| `timestamptz`, not `timestamp` | Stores an instant; avoids time-zone surprises |
| `text` over `varchar(n)` unless you need the limit | No performance difference in Postgres |
| `uuid` or identity for keys | UUIDs are unguessable in URLs; identity columns are compact. Do not expose sequential IDs where enumeration matters |
| Money as integer minor units or `numeric`, never `float` | Exact arithmetic |
| Name consistently (`snake_case`, plural tables) | ORMs map names; consistency avoids quoting |
| Foreign key `ON DELETE` chosen deliberately (`CASCADE`, `SET NULL`, `RESTRICT`) | Defines what happens to children |
| Many-to-many via a join table with a composite key | `post_tags(post_id, tag_id, PRIMARY KEY (post_id, tag_id))` |

`jsonb` is useful for genuinely flexible attributes; do not use it to avoid modeling columns you query often.

## The SQL you need

```sql
-- Read
SELECT id, title FROM posts WHERE author_id = $1 AND status = 'published'
ORDER BY created_at DESC LIMIT 20;

-- Join
SELECT p.id, p.title, u.name AS author
FROM posts p JOIN users u ON u.id = p.author_id
WHERE p.status = 'published';

-- Insert and get the row back
INSERT INTO posts (author_id, title) VALUES ($1, $2) RETURNING id;

-- Update with ownership in the WHERE clause (IDOR-safe)
UPDATE posts SET title = $1 WHERE id = $2 AND author_id = $3 RETURNING id;

-- Upsert
INSERT INTO users (email, name) VALUES ($1, $2)
ON CONFLICT (email) DO UPDATE SET name = EXCLUDED.name;

-- Aggregate
SELECT author_id, count(*) FROM posts GROUP BY author_id HAVING count(*) > 5;

-- Keyset pagination
SELECT id, title FROM posts WHERE id < $1 ORDER BY id DESC LIMIT 20;
```

`$1`, `$2` are **placeholders**: the driver sends values separately from the SQL text, which is what prevents injection.

## Indexes

| Index | When |
|---|---|
| Primary key / `UNIQUE` | Automatic |
| B-tree on a foreign key (`author_id`) | Postgres does **not** auto-index foreign keys; add them for joins and cascades |
| Composite `(author_id, created_at DESC)` | Filter by one column, sort by another |
| Partial (`WHERE status = 'published'`) | Query almost always filters on a subset |
| GIN on `jsonb` or full-text | Searching inside documents or text |

Indexes speed reads and slow writes and use disk. Add them for real query patterns, and check with `EXPLAIN`:

```sql
EXPLAIN ANALYZE SELECT ... ;      -- look for "Seq Scan" on big tables, large row estimates off
```

## Connecting with `pg`

```bash
npm install pg
npm install -D @types/pg
```

```ts
// db/index.ts
import "server-only";
import { Pool } from "pg";

declare global { var __pgPool: Pool | undefined; }

function createPool() {
  return new Pool({
    connectionString: process.env.DATABASE_URL,
    max: 10,                         // per process; keep (max × instances) under the DB limit
    idleTimeoutMillis: 30_000,
    connectionTimeoutMillis: 5_000,
    ssl: process.env.NODE_ENV === "production" ? { rejectUnauthorized: true } : undefined,
  });
}

export const pool = globalThis.__pgPool ?? createPool();
if (process.env.NODE_ENV !== "production") globalThis.__pgPool = pool;
```

```ts
// data/posts.ts
import "server-only";
import { pool } from "@/db";

export async function getPublishedPosts(limit = 20) {
  const { rows } = await pool.query<{ id: string; title: string }>(
    "SELECT id, title FROM posts WHERE status = 'published' ORDER BY created_at DESC LIMIT $1",
    [limit],
  );
  return rows;
}
```

Notes:

- `pool.query` checks out a connection, runs, and returns it. Use `pool.connect()` only when you need several statements on **one** connection (a [transaction](./05-transactions.md)), and always `release()` in `finally`.
- `pg` returns `bigint` and `numeric` as **strings**, and `timestamptz` as `Date`. Convert in the DTO (`Date` is not a plain value for Client Component props; serialize to ISO strings).
- Another popular driver is `postgres` (postgres.js), which uses tagged templates (`` sql`SELECT ... WHERE id = ${id}` ``) that parameterize automatically.
- Drizzle ships adapters for `pg`, `postgres.js` and serverless drivers; Prisma 7 uses `@prisma/adapter-pg` over `pg`.

## Connection pooling and serverless

Each Postgres connection is a process on the server, so connection counts are limited (often 100 or fewer on small plans).

| Layer | What it does |
|---|---|
| In-app pool (`pg.Pool`) | Reuses connections inside one process |
| External pooler (PgBouncer, Supabase pooler, provider "pooled" URL) | Multiplexes many app connections onto few database connections |
| HTTP / serverless driver (provider-specific) | Queries over HTTP or WebSockets; no long-lived TCP per function instance |

Guidance:

- Long-running Node server: one pool, `max` around 5 to 20.
- Serverless: small `max` (1 to 5) per instance, **plus** a pooler, and cap function concurrency below the database limit.
- Use the **pooled** URL for the app and the **direct** URL for migrations. A pooler in transaction mode does not support some features (session-level settings, prepared statements on some setups, advisory locks); check your pooler's notes.
- AWS RDS Proxy gives no pooling benefit with Prisma Client, per Prisma's docs, because of connection pinning.

## Local development

```bash
docker run --name pg -e POSTGRES_PASSWORD=postgres -p 5432:5432 -d postgres:17
# DATABASE_URL=postgresql://postgres:postgres@localhost:5432/postgres
```

Use `psql "$DATABASE_URL"` to inspect. For TLS in production, use `sslmode=require` or the driver's `ssl` option, and a trusted CA bundle if your host needs one.

## Row-level security (note)

PostgreSQL can enforce per-row access with policies (`ENABLE ROW LEVEL SECURITY`, `CREATE POLICY`). Platforms that expose Postgres directly to browsers (Supabase) rely on it. In a Next.js app where only the server talks to the database, application-level checks in the DAL are the usual approach; RLS is an extra safety net that requires setting a per-request role or claim on the connection.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `ECONNREFUSED` | Database not running, wrong host or port | Check `DATABASE_URL`, container status |
| `password authentication failed` | Wrong credentials or URL-encoding of special characters in the password | Percent-encode (`@` → `%40`) |
| `no pg_hba.conf entry` / SSL required | Remote DB requires TLS | `sslmode=require` / `ssl` option |
| `remaining connection slots are reserved` | Too many connections | Pooler, smaller `max`, dev singleton |
| `Connection terminated unexpectedly` | Idle timeout, pooler or network drop | Handle `pool.on("error")`; shorter `idleTimeoutMillis`; retry idempotent reads |
| `relation "x" does not exist` | Migration not applied to this database, wrong schema | Run migrations; check `search_path` |
| `duplicate key value violates unique constraint` | Unique conflict | Catch error code `23505`; return a field error |
| `violates foreign key constraint` | Referenced row missing | Error code `23503`; validate or create the parent first |
| `invalid input syntax for type uuid` | Non-UUID string passed | Validate params before querying |
| Numbers come back as strings | `bigint` / `numeric` mapping | Parse in the DTO |
| Slow query | Missing index, Seq Scan, N+1 | `EXPLAIN ANALYZE`; add index; batch |

Common SQLSTATE codes to handle: `23505` unique violation, `23503` foreign key violation, `23502` not null violation, `40001` serialization failure and `40P01` deadlock (both retry in a [transaction](./05-transactions.md)).

## Common mistakes

| Mistake | Fix |
|---|---|
| String-concatenated SQL | `$1` placeholders |
| No index on foreign keys | Add them |
| `timestamp` without time zone | `timestamptz` |
| Floats for money | Integer cents or `numeric` |
| Pool per request | Module-level pool with a dev guard |
| Forgetting `client.release()` | `try/finally` |
| Using the app role for migrations | Separate roles |
| Relying only on app code for uniqueness | Add `UNIQUE` constraints |
| `OFFSET` pagination on huge tables | Keyset pagination |

## Quick Summary

- PostgreSQL fits relational data and transactions; let the database enforce constraints.
- Use `timestamptz`, constraints, foreign keys with indexes, and indexes shaped by real queries (`EXPLAIN ANALYZE`).
- Query with `$n` placeholders, scope updates by owner, convert driver types in DTOs.
- One pool per process with a dev `globalThis` guard; use a pooler for serverless and a direct URL for migrations.
- Handle common error codes (`23505`, `23503`, `40001`, `40P01`).

## Next

- [Prisma](./02-prisma.md)
- [Drizzle](./03-drizzle.md)
- [Transactions](./05-transactions.md)
