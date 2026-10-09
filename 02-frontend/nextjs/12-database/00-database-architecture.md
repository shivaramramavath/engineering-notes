# Database Architecture

Server Components, Server Actions and Route Handlers all run on the server, so they can talk to a database directly. That is powerful and easy to get wrong. This note covers where the code lives, how connections behave in Next.js, and the security and caching rules that apply whichever database and ORM you pick.

> Verified against the Next.js 16.4 Data Security guide and the Prisma, Drizzle and Mongoose docs.

## What it is

A database layer has four jobs:

| Job | Where it lives |
|---|---|
| Connect (a pool or client, configured from env vars) | `db/index.ts`, `server-only` |
| Describe data (schema, types) | Schema file (Prisma schema, Drizzle `schema.ts`, Mongoose models) |
| Query (reads and writes) | The **Data Access Layer** (DAL) |
| Evolve (change tables or collections over time) | Migrations, run outside the app |

## Why a Data Access Layer

The Data Security guide offers three approaches and says to pick one and not mix them:

| Approach | Use for |
|---|---|
| HTTP APIs (call an existing REST or GraphQL backend) | Existing large organizations with separate backend teams |
| **Data Access Layer** | **New projects** |
| Component-level queries (query inside a Server Component) | Prototypes and learning |

A DAL is an internal library that:

- runs **only on the server** (`import "server-only"`),
- performs **authorization checks** ([chapter 11](../11-authentication/05-authorization.md)),
- returns **minimal DTOs**,
- is the **only place** that imports the database client and reads `process.env` for secrets.

Querying inside components makes it easy to pass a whole row to a Client Component, which then ships every column to the browser.

## Suggested layout

```text
src/
├── db/
│   ├── index.ts          # the client / pool (server-only)
│   └── schema.ts         # tables (Drizzle) or models; Prisma keeps prisma/schema.prisma
├── data/                 # the DAL: one file per domain
│   ├── users.ts
│   └── posts.ts
├── app/
│   ├── actions/          # thin "use server" files that call the DAL
│   └── posts/page.tsx    # Server Component: calls data/posts.ts
```

```ts
// data/posts.ts
import "server-only";
import { cache } from "react";
import { db } from "@/db";
import { verifySession } from "@/app/lib/dal";

export type PostDTO = { id: string; title: string; excerpt: string };

export const getMyPosts = cache(async (): Promise<PostDTO[]> => {
  const { userId } = await verifySession();
  const rows = await db.posts.findManyByAuthor(userId);        // pseudocode: your ORM call
  return rows.map((p) => ({ id: p.id, title: p.title, excerpt: p.body.slice(0, 140) }));
});
```

```ts
// app/actions/posts.ts
"use server";
import { revalidatePath } from "next/cache";
import { createPost } from "@/data/posts";

export async function createPostAction(formData: FormData) {
  await createPost(formData);          // auth, validation, authorization, insert happen in the DAL
  revalidatePath("/posts");
}
```

Both `"use server"` files and the DAL may `import "server-only"`.

## Runtime and imports

- Database drivers (`pg`, the Prisma client, `mongoose`) need the **Node.js runtime**. Mongoose is explicitly unsupported on the Edge Runtime because it needs the Node.js `net` API.
- Never import the DB module from a file with `"use client"`. `server-only` turns that mistake into a build error.
- Do not query the database in **Proxy**: it runs on every matched request, including prefetches ([Protecting Routes](../11-authentication/04-protecting-routes.md)).

## Connections: the part that bites in Next.js

| Situation | What happens | What to do |
|---|---|---|
| **Development with hot reload** | Modules re-run on save, so each reload can create a new client and pool until the database says "too many connections" | Store the client on `globalThis` in development |
| **Long-running Node server** | One process, one pool | One module-level client |
| **Serverless / many instances** | Each instance has its own pool; connections multiply with traffic | Small pools per instance, a connection pooler (PgBouncer or the provider's pooler), or an HTTP/serverless driver |
| **Build time** | `next build` may prerender pages that query the database | Ensure the database is reachable, or make the page dynamic |

Prisma's connection guidance, which applies in spirit to every driver: create the client once and reuse it, do not disconnect after each query in a long-running app, and size pools so that `pool size × number of instances` stays under the database's connection limit. Use a pooled URL at runtime and a **direct** URL for migrations.

The dev-mode singleton pattern (the same idea appears for Prisma, Drizzle and Mongoose below):

```ts
const globalForDb = globalThis as unknown as { db?: ReturnType<typeof createDb> };
export const db = globalForDb.db ?? createDb();
if (process.env.NODE_ENV !== "production") globalForDb.db = db;
```

## Reading data in Server Components

Database queries are not `fetch` calls, so the **Data Cache does not apply to them automatically**. How you cache depends on the caching model in use ([Caching](../06-caching/README.md)):

| You want | Do |
|---|---|
| Fresh data each request | Query in a dynamic Server Component (reading `cookies()` or similar makes it request-time) |
| Reuse within one render | Wrap the DAL function in `React.cache` |
| Reuse across requests | Cache Components: wrap the function in `"use cache"`, set `cacheLife`, tag with `cacheTag`, and invalidate in the action with `updateTag` |
| Per-user data cached on the server | Resolve the user first and pass the ID as an argument ([Protecting Routes](../11-authentication/04-protecting-routes.md#cache-components)) |

With `cacheComponents` on, uncached request-time work, including a database query that is not wrapped in `use cache`, belongs behind a `<Suspense>` boundary so the static shell can still prerender. See [Cache Components](../06-caching/05-cache-components.md).

```ts
// data/products.ts
import { cacheLife, cacheTag } from "next/cache";
import { db } from "@/db";

export async function getProducts() {
  "use cache";
  cacheTag("products");
  cacheLife("hours");
  return db.products.findMany();      // pseudocode
}
```

```ts
// in the Server Action that changes products
import { updateTag } from "next/cache";
updateTag("products");
```

## Writing data

1. A `"use server"` action receives `FormData` or arguments.
2. It authenticates, validates, authorizes (chapter 11, [Validation](../07-server-actions/02-validation.md)).
3. It writes through the DAL, in a [transaction](./05-transactions.md) if several writes depend on each other.
4. It revalidates (`updateTag`, `revalidateTag`, `revalidatePath`) **after** the write commits.
5. It returns a small result, not the row.

## Migrations

A migration is a versioned, reviewed change to the schema.

| Tool | Create | Apply |
|---|---|---|
| Prisma 7 | `prisma migrate dev --name x` | `prisma migrate deploy` in CI/production |
| Drizzle | `drizzle-kit generate` | `drizzle-kit migrate` |
| Plain SQL | A numbered `.sql` file | A migration runner of your choice |
| Mongoose | No schema migrations by default; use scripts for data changes | Run once, deliberately |

Rules:

- Commit migration files.
- Run them as a **release step**, not at server start-up (every instance would race).
- Prefer **expand and contract** for breaking changes: add the new column, deploy code that writes both, backfill, switch reads, then remove the old column.
- Use quick-iteration commands like `drizzle-kit push` only on throwaway databases.
- Back up before destructive migrations; some tools offer to reset the dev database, and a reset deletes data.

## Security

| Risk | Rule |
|---|---|
| **SQL injection** | Parameterized queries only. ORMs and tagged-template drivers do this; never build SQL by concatenating user input |
| **NoSQL injection** (MongoDB operators in input) | Validate input types (schema validation); never pass raw request objects as a filter |
| **Secrets in the client bundle** | Connection strings in server-only env vars (no `NEXT_PUBLIC_`); read only in the DAL |
| **Over-privileged DB user** | App uses a role with only the rights it needs; migrations use a separate, stronger role |
| **IDOR** | Put ownership in the query (`where id = ? and author_id = ?`) |
| **Leaking columns** | Select only needed fields; map to DTOs |
| **Unencrypted connection** | Require TLS (`sslmode=require` or the driver's SSL option) for remote databases |
| **Logging secrets or PII** | Do not log connection strings or full rows |

## N+1 queries and pagination

- **N+1**: one query for a list, then one per row. Use joins or the ORM's `include` / `with` / `populate` to load relations together.
- **Paginate** every list: `LIMIT`/`OFFSET` is simple; **keyset** (cursor) pagination (`WHERE id > $last ORDER BY id LIMIT n`) stays fast on large tables.
- **Index** columns used in `WHERE`, `JOIN` and `ORDER BY`.

## Environment variables

```bash
# .env.local   (git-ignored)
DATABASE_URL="postgresql://user:pass@host:5432/db?sslmode=require"
# pooled URL for the app, direct URL for migrations when you use a pooler
```

Next.js loads `.env*` files for the server; only `NEXT_PUBLIC_` variables reach the browser. Some CLIs (Prisma, Drizzle Kit) do **not** read `.env.local` by themselves; they use `.env` or `dotenv/config` in their config file.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| "Too many connections" / "remaining connection slots" in dev | New client per hot reload | Global singleton in dev |
| Works locally, connection errors in serverless | Pool per instance exhausts the database | Smaller pool, pooler, or serverless driver |
| `Module not found: Can't resolve 'net'/'fs'` | DB module imported into a Client Component or Edge code | Keep it in server files; use `server-only` |
| Page shows stale data after a write | Cached read not invalidated | `updateTag` / `revalidatePath` in the action |
| Build fails querying the DB | Prerendering a page that queries an unreachable DB | Provide a DB at build time or render dynamically |
| Env var `undefined` in a CLI tool | CLI does not load `.env.local` | Use `.env` or `dotenv/config` |
| Slow list pages | N+1 or missing index | Batch relations; `EXPLAIN` the query |
| Whole row visible in page payload | Row passed to a Client Component | DTO |
| `Cannot serialize` (Date, BigInt, ObjectId, Decimal) passing props | Non-plain values | Convert to strings or numbers in the DTO |

## Common mistakes

| Mistake | Fix |
|---|---|
| Creating a new client per request | One shared client |
| Querying in Client Components | Server only |
| Mixing the DAL, component queries and a REST backend | Pick one approach |
| `select *` into props | DTO |
| String-built SQL | Placeholders |
| Running migrations on every app boot | Release step |
| Forgetting to revalidate after a write | Revalidate in the action |
| Calling external APIs inside a transaction | Keep transactions DB-only and short |
| Trusting an ID from the client | Scope by the session's user |

## Quick Summary

- Put all database code in a `server-only` DAL; only it imports the client and reads secrets.
- One client per process; use a `globalThis` guard in development, and watch pool sizes in serverless.
- Database reads are not `fetch`: cache with `React.cache` or `use cache` plus tags, and revalidate after writes.
- Parameterize, scope queries by owner, return DTOs, require TLS, least-privilege roles.
- Migrations are reviewed files applied in a release step; wrap dependent writes in transactions.

## Next

- [PostgreSQL](./01-postgresql.md)
- [Data Fetching](../05-data-fetching/README.md)
- [Authorization](../11-authentication/05-authorization.md)
