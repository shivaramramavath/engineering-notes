# Prisma

Prisma is a schema-first ORM: you describe your models in a schema file, Prisma generates a fully typed client and manages migrations. It is a good default when you want productive queries, relations and a clear schema as the source of truth.

> **Version warning.** As of these docs (October 2026), **Prisma ORM 8 is the current release, as a release candidate**, with a different file layout and API. **Prisma ORM 7 remains supported** and is what this note teaches (schema file, generated client, driver adapter). The Prisma 8 differences are listed at the end. Always follow the quickstart for the version you install: Prisma changes setup details between majors.

## What it is, and when to use it

| Piece | Role |
|---|---|
| `schema.prisma` | Models, relations, enums, generator settings |
| `prisma.config.ts` | Project config for the CLI: schema path, migrations path, datasource URL |
| Prisma Client | Generated, typed query API (`prisma.user.findMany(...)`) |
| Prisma Migrate | Creates and applies SQL migrations |
| Driver adapter (`@prisma/adapter-pg`) | Connects the client to PostgreSQL through the `pg` driver |
| Prisma Studio | Browser UI for data (`npx prisma studio`) |

| Use Prisma when | Consider Drizzle when |
|---|---|
| You like a declarative schema and generated types | You want to write SQL-shaped queries in TypeScript |
| Relations and nested writes are central | You want a very thin layer with minimal generation |
| Team wants one tool for schema, migrations and client | You prefer the schema in TypeScript |

## Setup (Prisma 7, PostgreSQL)

The Prisma 7 quickstart uses an ESM TypeScript project and these packages:

```bash
npm install prisma@prev @types/pg --save-dev
npm install @prisma/client@7 @prisma/adapter-pg pg dotenv
```

`@prisma/client@7` and `prisma@prev` pin the Prisma 7 line while version 8 is the default tag; check the current quickstart for the right tags. `package.json` needs `"type": "module"` in the quickstart's setup. In a Next.js app, your existing TypeScript and module settings apply; follow the quickstart's tsconfig advice for the CLI and keep Next's own settings for the app.

```bash
npx prisma init --datasource-provider postgresql --output ../generated/prisma
```

This creates `prisma/schema.prisma`, `.env`, and a config file. The v7 quickstart names it `prisma7.config.ts` to avoid a clash with Prisma 8; the Prisma 7 CLI also finds `prisma.config.ts`.

```ts
// prisma.config.ts
import "dotenv/config";
import { defineConfig } from "prisma/config";

export default defineConfig({
  schema: "prisma/schema.prisma",
  migrations: { path: "prisma/migrations" },
  datasource: { url: process.env["DATABASE_URL"] },
});
```

```prisma
// prisma/schema.prisma
generator client {
  provider = "prisma-client"
  output   = "../generated/prisma"
}

datasource db {
  provider = "postgresql"
}

model User {
  id    Int     @id @default(autoincrement())
  email String  @unique
  name  String?
  posts Post[]
}

model Post {
  id        Int     @id @default(autoincrement())
  title     String
  content   String?
  published Boolean @default(false)
  author    User    @relation(fields: [authorId], references: [id])
  authorId  Int
}
```

```bash
# .env  (the Prisma CLI reads this)
DATABASE_URL="postgresql://user:password@localhost:5432/mydb?schema=public"
```

```bash
npx prisma migrate dev --name init     # create and apply a migration (dev)
npx prisma generate                    # generate the client into ../generated/prisma
```

Details:

- The **generated client lives in your project** (`generated/prisma`), so import from there, not from `@prisma/client`. Add the folder to `.gitignore` or commit it by team convention, and regenerate in CI.
- The datasource URL is not in `schema.prisma` in this layout; the CLI gets it from `prisma.config.ts`, and the **running app** gets it from the adapter (below).
- On Vercel or Netlify, dependency caching can skip generation. Add `"postinstall": "prisma generate"` (the recommended fix) or put `prisma generate` before `next build`.

## The client singleton

```ts
// db/prisma.ts
import "server-only";
import { PrismaPg } from "@prisma/adapter-pg";
import { PrismaClient } from "../../generated/prisma/client";   // adjust to your path

function createClient() {
  const adapter = new PrismaPg({ connectionString: process.env.DATABASE_URL });
  return new PrismaClient({ adapter });
}

const globalForPrisma = globalThis as unknown as { prisma?: ReturnType<typeof createClient> };

export const prisma = globalForPrisma.prisma ?? createClient();

if (process.env.NODE_ENV !== "production") globalForPrisma.prisma = prisma;
```

Why: Next.js hot reloading re-runs modules, and each run would create a new client and connection pool. Prisma's own Next.js guidance is the same global-variable pattern, "ensuring only one instance exists, even during hot-reloading". Pool settings live on the **adapter**, not in URL parameters. Do not call `$disconnect()` after each query in a long-running server.

`import "server-only"` is a Next.js addition that makes any client-side import a build error.

## Queries

```ts
import { prisma } from "@/db/prisma";

// Create (with a nested write)
const user = await prisma.user.create({
  data: {
    email: "alice@example.com",
    name: "Alice",
    posts: { create: { title: "Hello", published: true } },
  },
  include: { posts: true },
});

// Read
const one   = await prisma.user.findUnique({ where: { email: "alice@example.com" } });
const many  = await prisma.post.findMany({
  where: { published: true, title: { contains: "next", mode: "insensitive" } },
  orderBy: { id: "desc" },
  take: 20,
  select: { id: true, title: true, author: { select: { name: true } } },
});

// Update / delete: scope by owner in the where
const updated = await prisma.post.updateMany({
  where: { id: 5, authorId: 1 },
  data: { title: "New title" },
});                                   // updated.count === 0 means "not found or not yours"

// Upsert
await prisma.user.upsert({
  where: { email: "a@example.com" },
  update: { name: "A" },
  create: { email: "a@example.com", name: "A" },
});

// Aggregate
const total = await prisma.post.count({ where: { published: true } });
```

Key behaviors:

| Topic | Rule |
|---|---|
| `select` vs `include` | `select` picks fields (preferred for DTOs); `include` adds relations to all scalar fields |
| `findUnique` | Needs a unique field (`id`, `@unique`) |
| `update` / `delete` | Need a unique `where`; use `updateMany` / `deleteMany` to add other filters like `authorId` |
| Pagination | `take`/`skip`, or cursor: `cursor: { id }, skip: 1, take: 20` |
| N+1 | Use `include` / nested `select` to load relations in fewer queries |
| Errors | `Prisma.PrismaClientKnownRequestError` with `code`: `P2002` unique violation, `P2025` record not found, `P2034` write conflict (retry) |

Handling a unique violation:

```ts
import { Prisma } from "../../generated/prisma/client";

try {
  await prisma.user.create({ data: { email } });
} catch (e) {
  if (e instanceof Prisma.PrismaClientKnownRequestError && e.code === "P2002") {
    return { errors: { email: ["Email already in use."] } };
  }
  throw e;
}
```

(Check the import location of `Prisma` for your generator output; with a custom output it comes from the generated module.)

## In the Next.js DAL

```ts
// data/posts.ts
import "server-only";
import { cache } from "react";
import { prisma } from "@/db/prisma";
import { verifySession } from "@/app/lib/dal";

export const getMyPosts = cache(async () => {
  const { userId } = await verifySession();
  const posts = await prisma.post.findMany({
    where: { authorId: Number(userId) },
    select: { id: true, title: true, published: true },     // DTO shape
    orderBy: { id: "desc" },
  });
  return posts;
});
```

```tsx
// app/posts/page.tsx
import { getMyPosts } from "@/data/posts";

export default async function Page() {
  const posts = await getMyPosts();
  return <ul>{posts.map((p) => <li key={p.id}>{p.title}</li>)}</ul>;
}
```

```ts
// app/actions/posts.ts
"use server";
import { revalidatePath } from "next/cache";
import { prisma } from "@/db/prisma";
import { verifySession } from "@/app/lib/dal";

export async function renamePost(id: number, title: string) {
  const { userId } = await verifySession();
  const { count } = await prisma.post.updateMany({
    where: { id, authorId: Number(userId) },
    data: { title: title.trim() },
  });
  if (count === 0) return { message: "Not found" };
  revalidatePath("/posts");
  return { success: true };
}
```

Under Cache Components, wrap cross-request reads in `"use cache"` with `cacheTag`, and call `updateTag` in the action ([Database Architecture](./00-database-architecture.md#reading-data-in-server-components)).

## Migrations

```bash
npx prisma migrate dev --name add_phone     # dev: diff schema, write SQL, apply, regenerate
npx prisma migrate deploy                   # CI/production: apply pending migrations only
npx prisma studio                           # inspect data
```

- `migrate dev` may ask to **reset** the database when it detects drift; that deletes data. Never run it against production.
- Commit `prisma/migrations/`.
- Use a **direct** (non-pooled) URL for migrations when you use a pooler, configured in `prisma.config.ts`.
- Edit generated SQL when a change needs custom steps (backfills) before applying.

## Transactions

Nested writes are transactional automatically. For more, see [Transactions](./05-transactions.md#prisma): `prisma.$transaction([...])` and interactive `prisma.$transaction(async (tx) => ...)`.

## Prisma 8 (release candidate) differences

From the current "from scratch" guide:

| Area | Prisma 7 (this note) | Prisma 8 RC |
|---|---|---|
| Schema file | `prisma/schema.prisma` with `generator` and `datasource` blocks | `prisma/contract.prisma`, no `datasource` or `generator` blocks |
| Connection | Adapter in client code, URL in `prisma.config.ts` | Connection in `prisma.config.ts` via `@prisma/orm-postgres/config` |
| Generate | `prisma generate` | `prisma contract emit` |
| Migrate | `prisma migrate dev` / `deploy` | `prisma migration plan`, then `prisma db migrate`; `db init`, `db update` |
| Client package | `@prisma/client` | `@prisma/orm-postgres` (and `/runtime`) |
| Query API | `prisma.user.findMany(...)` | `db.orm.public.User.where({...}).update({...})`, `.all()`, `.create(...)` |
| `db push` | Exists | Does not exist |

Requirements in the 8 docs: Node.js 22.18+, TypeScript 5.9+, and npm 11.6+ for the scaffold. Prisma recommends 8 for new projects, but it is an RC, so pin versions, read the migration guide before moving an existing app, and verify the Next.js integration pattern against the current docs.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `Cannot find module '../generated/prisma/client'` | Client not generated, wrong output path | `npx prisma generate`; fix the import path |
| Works locally, fails on Vercel | Generation skipped by dependency cache | `postinstall: prisma generate` |
| "Too many clients" in dev | New client per reload | `globalThis` singleton |
| Pool timeout errors | Many parallel queries, small pool | Raise the adapter's pool size; reduce parallelism |
| `P2002` | Unique constraint | Handle and return a field error |
| `P2025` | `update` / `delete` target missing | Treat as not found |
| `P2034` | Write conflict in a serializable transaction | Retry the transaction |
| Environment variable not found by the CLI | `.env.local` is not read by Prisma | Put it in `.env` or load it in `prisma.config.ts` |
| `Do not know how to serialize a BigInt` / Date / Decimal in props | Non-plain values to a Client Component | Convert in the DTO |
| `migrate dev` wants to reset | Drift between migrations and the database | Dev: accept on a throwaway DB; Prod: use `migrate deploy` and fix drift deliberately |
| Imports break after switching to Prisma 8 | Different package and API | Follow the Prisma 8 guide fully |

## Common mistakes

| Mistake | Fix |
|---|---|
| New `PrismaClient()` in every file | One shared instance |
| Importing the client into a Client Component | `server-only`, DAL |
| Returning full model objects | `select` DTOs |
| `update` with only `id` for an owner-scoped action | `updateMany` with `authorId`, or check first |
| Running `migrate dev` in production | `migrate deploy` |
| Not committing migrations | Commit `prisma/migrations` |
| Ignoring `P2002` | Handle uniqueness errors |
| Mixing v7 and v8 instructions | Pick one major per project |

## Quick Summary

- Prisma 7: `schema.prisma` + `prisma.config.ts` + a generated client that you import from your own output folder, connected with `@prisma/adapter-pg`.
- Create one client per process, with a `globalThis` guard in development; wrap it in `server-only`.
- Use `select` for DTOs, scope updates by owner, handle `P2002` / `P2025` / `P2034`.
- `migrate dev` locally, `migrate deploy` in CI; generate the client on install.
- Prisma 8 (RC) uses a contract file and a different client API; do not mix instructions.

## Next

- [Drizzle](./03-drizzle.md)
- [Transactions](./05-transactions.md)
- [PostgreSQL](./01-postgresql.md)

Sources: [Prisma 7 PostgreSQL quickstart](https://www.prisma.io/docs/v7/prisma-orm/quickstart/postgresql), [Prisma from scratch (v8 RC)](https://www.prisma.io/docs/prisma-orm/from-scratch), [Prisma and Next.js](https://www.prisma.io/docs/orm/more/help-and-troubleshooting/nextjs-help), [Prisma connection management](https://www.prisma.io/docs/orm/prisma-client/setup-and-configuration/databases-connections)
