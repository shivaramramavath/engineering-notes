# Drizzle

Drizzle is a TypeScript ORM that stays close to SQL: your schema is TypeScript code, queries read like SQL (`select().from().where()`), and types are inferred from the schema with no code-generation step for the client. `drizzle-kit` handles migrations.

> Verified against the Drizzle PostgreSQL setup and transactions docs. The setup page installs the **`@rc`** tag of `drizzle-orm` and `drizzle-kit`, so the current line is a release candidate; older 0.x code you find online may differ in small ways (for example, column names are passed as the first argument in older examples). Check the docs for your installed version.

## What it is, and when to use it

| Piece | Role |
|---|---|
| `drizzle-orm` | The query builder and relational API |
| `schema.ts` | Tables as TypeScript (`pgTable(...)`) |
| `drizzle-kit` | CLI: generate, migrate, push |
| `drizzle.config.ts` | Tells the CLI where the schema is and how to connect |
| Driver | `pg`, `postgres.js`, or a serverless driver |

| Use Drizzle when | Consider Prisma when |
|---|---|
| You are comfortable with SQL and want it visible | You prefer a declarative schema language and nested writes |
| You want a small runtime and no generated client | You want Prisma Studio and its migration workflow |
| You like the schema living in TypeScript | |

## Setup (PostgreSQL with `pg`)

Per the docs:

```bash
npm i drizzle-orm@rc pg dotenv
npm i -D drizzle-kit@rc tsx @types/pg
```

```bash
# .env
DATABASE_URL=postgresql://user:password@localhost:5432/mydb
```

```ts
// drizzle.config.ts
import "dotenv/config";
import { defineConfig } from "drizzle-kit";

export default defineConfig({
  out: "./drizzle",
  schema: "./src/db/schema.ts",
  dialect: "postgresql",
  dbCredentials: { url: process.env.DATABASE_URL! },
});
```

`drizzle.config.ts` runs in the CLI, which is why it loads `dotenv/config` itself.

## Schema

```ts
// src/db/schema.ts
import { integer, pgTable, text, timestamp, boolean, varchar, index } from "drizzle-orm/pg-core";
import { relations } from "drizzle-orm";

export const users = pgTable("users", {
  id: integer().primaryKey().generatedAlwaysAsIdentity(),
  name: varchar({ length: 255 }).notNull(),
  email: varchar({ length: 255 }).notNull().unique(),
  createdAt: timestamp({ withTimezone: true }).notNull().defaultNow(),
});

export const posts = pgTable(
  "posts",
  {
    id: integer().primaryKey().generatedAlwaysAsIdentity(),
    authorId: integer().notNull().references(() => users.id, { onDelete: "cascade" }),
    title: text().notNull(),
    published: boolean().notNull().default(false),
    createdAt: timestamp({ withTimezone: true }).notNull().defaultNow(),
  },
  (t) => [index("posts_author_idx").on(t.authorId)],
);

export const usersRelations = relations(users, ({ many }) => ({ posts: many(posts) }));
export const postsRelations = relations(posts, ({ one }) => ({
  author: one(users, { fields: [posts.authorId], references: [users.id] }),
}));

export type User = typeof users.$inferSelect;
export type NewUser = typeof users.$inferInsert;
```

The docs' example defines columns like `integer().primaryKey().generatedAlwaysAsIdentity()` with no name argument. The relations helper and the exact signature of the index callback have changed across Drizzle versions, so confirm them against your version's docs; the index-and-relations parts above are general patterns, not copied from the setup page.

## The client

```ts
// src/db/index.ts
import "server-only";
import { drizzle } from "drizzle-orm/node-postgres";
import { Pool } from "pg";
import * as schema from "./schema";

declare global { var __pool: Pool | undefined; }

const pool =
  globalThis.__pool ??
  new Pool({ connectionString: process.env.DATABASE_URL, max: 10 });

if (process.env.NODE_ENV !== "production") globalThis.__pool = pool;

export const db = drizzle({ client: pool, schema });
```

The docs show three ways to create the instance: a connection string (`drizzle(process.env.DATABASE_URL!)`), a config object (`drizzle({ connection: { connectionString, ssl: true } })`), or your own `Pool` (`drizzle({ client: pool })`). Next.js needs the third form so that **you** control the pool and can keep it on `globalThis` across hot reloads. Passing `schema` enables the relational query API (`db.query.users...`).

## Migrations

```bash
npx drizzle-kit generate     # compare schema.ts to the last snapshot, write SQL into ./drizzle
npx drizzle-kit migrate      # apply generated migrations
npx drizzle-kit push         # push the schema straight to the DB (rapid local iteration)
```

Use `generate` + `migrate` for anything shared; reserve `push` for a throwaway local database. Commit the `drizzle/` folder. In CI or release scripts run `drizzle-kit migrate`, not at app start-up.

## Queries

```ts
import { and, desc, eq, ilike, sql } from "drizzle-orm";
import { db } from "@/db";
import { users, posts } from "@/db/schema";

// Create
const [created] = await db.insert(users).values({ name: "Alice", email: "a@example.com" }).returning();

// Read
const all = await db.select().from(users);

const recent = await db
  .select({ id: posts.id, title: posts.title, author: users.name })
  .from(posts)
  .innerJoin(users, eq(users.id, posts.authorId))
  .where(and(eq(posts.published, true), ilike(posts.title, "%next%")))
  .orderBy(desc(posts.createdAt))
  .limit(20);

// Update, scoped by owner
const updated = await db
  .update(posts)
  .set({ title: "New title" })
  .where(and(eq(posts.id, 5), eq(posts.authorId, 1)))
  .returning({ id: posts.id });                 // empty array => not found or not yours

// Delete
await db.delete(posts).where(eq(posts.id, 5));

// Upsert
await db.insert(users).values({ name: "A", email: "a@example.com" })
  .onConflictDoUpdate({ target: users.email, set: { name: "A" } });

// Count
const [{ count }] = await db.select({ count: sql<number>`count(*)::int` }).from(posts);
```

The select-based API maps to SQL one-to-one, so `EXPLAIN` output matches what you wrote. The docs' basic example uses `eq(...)`, `.select().from(...)`, `.insert().values()`, `.update().set().where()` and `.delete().where()`.

### Relational queries

With `schema` passed to `drizzle(...)`:

```ts
const usersWithPosts = await db.query.users.findMany({
  with: { posts: true },
  limit: 10,
});

const post = await db.query.posts.findFirst({
  where: (p, { eq }) => eq(p.id, 5),
  with: { author: true },
});
```

This loads relations in one go, avoiding N+1.

## In the Next.js DAL

```ts
// data/posts.ts
import "server-only";
import { cache } from "react";
import { and, desc, eq } from "drizzle-orm";
import { db } from "@/db";
import { posts } from "@/db/schema";
import { verifySession } from "@/app/lib/dal";

export const getMyPosts = cache(async () => {
  const { userId } = await verifySession();
  return db
    .select({ id: posts.id, title: posts.title, published: posts.published })   // DTO shape
    .from(posts)
    .where(eq(posts.authorId, Number(userId)))
    .orderBy(desc(posts.createdAt));
});

export async function renamePost(id: number, title: string) {
  const { userId } = await verifySession();
  const rows = await db
    .update(posts)
    .set({ title })
    .where(and(eq(posts.id, id), eq(posts.authorId, Number(userId))))
    .returning({ id: posts.id });
  return rows.length > 0;
}
```

```ts
// app/actions/posts.ts
"use server";
import { revalidatePath } from "next/cache";
import { renamePost } from "@/data/posts";

export async function renamePostAction(id: number, title: string) {
  const ok = await renamePost(id, title.trim());
  if (!ok) return { message: "Not found" };
  revalidatePath("/posts");
  return { success: true };
}
```

## Transactions

```ts
await db.transaction(async (tx) => {
  await tx.update(accounts).set({ balance: sql`${accounts.balance} - 100` }).where(eq(accounts.id, 1));
  await tx.update(accounts).set({ balance: sql`${accounts.balance} + 100` }).where(eq(accounts.id, 2));
});
```

Full coverage, including `tx.rollback()`, savepoints and isolation levels, in [Transactions](./05-transactions.md#drizzle).

## Handling errors

Drizzle surfaces the driver's errors. With `pg`, check the SQLSTATE `code` (`23505` unique, `23503` foreign key). Depending on version the driver error may be on `error.cause`, so check both:

```ts
function pgCode(e: unknown): string | undefined {
  const err = e as { code?: string; cause?: { code?: string } };
  return err?.code ?? err?.cause?.code;
}
```

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `DATABASE_URL` undefined in `drizzle-kit` | Config does not load `.env` | `import "dotenv/config"` in `drizzle.config.ts` |
| `drizzle-kit generate` reports no changes | Wrong `schema` path in the config | Point `schema` at the right file(s) |
| `relation "x" does not exist` | Migrations not applied | `drizzle-kit migrate` |
| `db.query.users` is undefined | `schema` not passed to `drizzle()` | `drizzle({ client, schema })` |
| Too many connections in dev | New pool per reload | `globalThis` pool guard |
| Type of a column is `string` when you expected `number` | Postgres `bigint` / `numeric` mapping | Choose the column mode explicitly; convert in the DTO |
| Dates are not plain values for client props | `Date` objects | `toISOString()` in the DTO |
| `push` drops or alters data unexpectedly | `push` applies diffs directly | Use `generate` + `migrate`; review SQL |
| Tutorial code does not match your version | `0.x` vs `@rc` API differences | Read the docs for the installed version |
| Update changes nothing | `where` omitted or too narrow | Always scope; check `.returning()` result |

## Common mistakes

| Mistake | Fix |
|---|---|
| `db.update(...)` without `.where(...)` | Always include a where (this updates every row) |
| `select()` of everything into props | Select the DTO columns |
| Pool created in module scope without a dev guard | `globalThis` pattern |
| Using `push` against production | Generated migrations |
| Forgetting an index on a foreign key | Add `index().on(...)` |
| Importing `db` in a Client Component | `server-only` |
| Mixing drivers without checking compatibility | Match the `drizzle-orm/<driver>` import to the driver |
| Not passing `schema` and expecting relational queries | Pass it |

## Quick Summary

- Drizzle = TypeScript schema + SQL-like queries + `drizzle-kit` migrations; no generated client.
- Install with `drizzle-orm`, `drizzle-kit`, `pg`; the current docs use the `@rc` tags, so check your version.
- Create the client from your own `Pool` kept on `globalThis` in dev; wrap in `server-only`.
- Always `.where(...)` updates and deletes, scope by owner, select DTO columns.
- `generate` + `migrate` for real changes; `push` only for scratch databases.

## Next

- [MongoDB and Mongoose](./04-mongodb-mongoose.md)
- [Transactions](./05-transactions.md)
- [Prisma](./02-prisma.md)

Sources: [Drizzle PostgreSQL setup](https://orm.drizzle.team/docs/get-started/postgresql-new), [Drizzle transactions](https://orm.drizzle.team/docs/transactions)
