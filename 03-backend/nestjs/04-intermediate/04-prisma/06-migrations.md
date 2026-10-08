# Migrations

Prisma Migrate turns changes in `schema.prisma` into **SQL migration files** that you commit, review, and apply in order. Prisma records what has been applied in a `_prisma_migrations` table. General principles (expand/contract, safe operations, deploy ordering) are in [Database foundations: migrations](../02-database-foundations/05-migrations.md); this note covers the Prisma workflow.

Prerequisites: [Setup](./01-setup.md), [Schema](./02-schema.md).

> Command behavior changes slightly between Prisma versions (for example, whether `migrate dev` also runs `generate` and seeds). Always run `prisma generate` explicitly rather than assuming, and check the docs for your version.

## What a migration looks like

```text
prisma/
├── schema.prisma
└── migrations/
    ├── migration_lock.toml                       # pins the database provider
    ├── 20250101120000_init/
    │   └── migration.sql
    └── 20250110093000_add_user_role/
        └── migration.sql
```

```sql
-- prisma/migrations/20250110093000_add_user_role/migration.sql
ALTER TABLE "users" ADD COLUMN "role" TEXT NOT NULL DEFAULT 'USER';
```

Each migration is a folder with a timestamped name and plain SQL. You commit these with your code, and they're the same in every environment.

## The two core commands

| Command | Use it | What it does |
|---------|--------|--------------|
| `prisma migrate dev` | **Development only** | Compares schema to migrations, creates a new migration, applies pending ones; may **reset** the dev database if it detects drift |
| `prisma migrate deploy` | **CI/staging/production** | Applies pending committed migrations. No prompts, no generation, no reset |

```bash
# development loop
npx prisma migrate dev --name add_user_role
npx prisma generate                       # be explicit

# deployment
npx prisma migrate deploy
```

**Never run `migrate dev` against a shared or production database.** It can reset data and needs interactive confirmation. `migrate deploy` is the only command meant for production.

### The development loop

```text
1. edit schema.prisma
2. prisma migrate dev --name <what_changed>     (creates + applies migration; use --create-only to review first)
3. READ migration.sql; adjust if needed
4. prisma generate
5. commit schema.prisma + the migration folder together
```

`migrate dev` uses a **shadow database**, a temporary database it creates and drops to compute drift. Your database user needs permission to create databases locally (managed databases often require a separately provisioned shadow database URL; check the docs for configuring it in your version).

## Review and edit the SQL

Prisma generates SQL from a schema diff. Like any generated diff, it can misread intent:

| You did | Generated (often) | Problem |
|---------|-------------------|---------|
| Renamed a field/column | `DROP COLUMN` + `ADD COLUMN` | **Data loss** |
| Added a required column to a populated table | `ADD ... NOT NULL` (no default) | Fails on existing rows (Prisma usually warns) |
| Changed a column type | Alter or drop/add | Possible data loss |
| Added an index | `CREATE INDEX` | Blocks writes on big tables |

The safe pattern is **`--create-only`**: generate the migration without applying it, then edit the SQL:

```bash
npx prisma migrate dev --create-only --name rename_user_name
```

```sql
-- hand-edited: rename instead of drop + add
ALTER TABLE "users" RENAME COLUMN "name" TO "full_name";
```

```bash
npx prisma migrate dev          # now applies the edited migration
```

Edit **before** the migration is applied anywhere. After it has been applied to a shared environment, never edit it; add a new migration. If you edit an applied migration locally, Prisma detects the checksum change and asks to reset.

Features the schema can't express (partial indexes, expression indexes such as `lower(email)`, `CREATE INDEX CONCURRENTLY`, check constraints, triggers) also go in hand-written SQL inside a migration. Be aware that Prisma may later report drift for objects it doesn't model, so check how your version handles them.

## Data migrations and backfills

Put data changes in the SQL of a migration, separated from schema changes where possible:

```sql
ALTER TABLE "users" ADD COLUMN "slug" TEXT;
UPDATE "users" SET "slug" = lower(replace("name", ' ', '-')) WHERE "slug" IS NULL;
ALTER TABLE "users" ALTER COLUMN "slug" SET NOT NULL;
```

For big tables, backfill in batches, and consider doing it as a background job rather than in the migration. Use the expand/contract sequence for anything that must stay compatible with the running app version ([foundations](../02-database-foundations/05-migrations.md)).

Whether a migration's statements run in a single transaction is database- and statement-dependent. `CREATE INDEX CONCURRENTLY` in PostgreSQL can't run inside a transaction block, so put it alone in its own migration and verify it applies on a copy of production data.

## Deploying migrations

- Run `prisma migrate deploy` as a **separate pipeline step before starting the new app version** (or as an init job), not from every app replica at startup.
- Use a **direct database URL** for migrations. Poolers in transaction mode can break migration commands (advisory locks, session features). Keep the pooled URL for the app ([setup](./01-setup.md)).
- Make sure `prisma.config.ts` provides `DATABASE_URL` in that environment, and that `dotenv` loading isn't relied on in containers (inject real environment variables).
- Apply migrations to a fresh database in CI to prove they work from scratch.

```bash
# typical production deploy step
npx prisma migrate deploy
```

## Baselining an existing database

If the database already exists (created by hand or another tool), create a baseline migration so Prisma's history starts from the current state:

```bash
mkdir -p prisma/migrations/0_init
npx prisma migrate diff --from-empty --to-schema prisma/schema.prisma --script > prisma/migrations/0_init/migration.sql
npx prisma migrate resolve --applied 0_init
```

(Flag spelling for `migrate diff` has varied across versions; for instance older versions use `--to-schema-datamodel`. Check `npx prisma migrate diff --help` for your version.) `resolve --applied` marks the baseline as already applied without running it. `prisma db pull` can introspect an existing database into a schema first.

## Handling failures and drift

| Situation | Command / action |
|-----------|-------------------|
| See what's applied/pending | `npx prisma migrate status` |
| A migration failed halfway in production | Fix the database state manually if needed, then `migrate resolve --rolled-back <name>` (to retry) or `--applied <name>` (if you completed it manually) |
| Dev database drifted | `migrate dev` offers to reset; or `prisma migrate reset` (drops, re-applies, may seed) |
| Production drifted from migrations | Create a corrective migration; don't reset |
| Compare two states | `prisma migrate diff` |

`migrate reset` and `db push` are **development tools**. `prisma db push` syncs the schema without creating migration files, which is fine for prototypes and throwaway databases and wrong for anything shared (no history, can cause data loss).

## Seeding

```ts
// prisma/seed.ts
import { PrismaClient } from '../src/generated/prisma/client';
import { PrismaPg } from '@prisma/adapter-pg';

const prisma = new PrismaClient({ adapter: new PrismaPg({ connectionString: process.env.DATABASE_URL }) });

async function main() {
  await prisma.user.upsert({
    where: { email: 'admin@example.com' },
    update: {},
    create: { email: 'admin@example.com', passwordHash: 'hash', role: 'ADMIN' },
  });
}

main().finally(() => prisma.$disconnect());
```

Configure the seed command in `prisma.config.ts` under `migrations.seed` ([setup](./01-setup.md)), then run `npx prisma db seed`. Older versions configured it in `package.json`. Make seeds **idempotent** (`upsert`) so rerunning is safe, and keep production reference data separate from development sample data.

## Testing with migrations

- Apply migrations to the test database with `prisma migrate deploy` (or `migrate reset --force` locally) before the suite, so tests run against the real schema ([integration testing](../01-testing/05-integration-testing.md)).
- Don't use `db push` for the test database unless production also uses it.

## Conventions

- Descriptive names: `add_user_role`, `create_orders`, `backfill_slugs`.
- One logical change per migration; commit schema and migration together.
- Review SQL in code review like application code.
- Never edit migrations that have been applied beyond your machine.
- Treat roll-forward as the rollback strategy; Prisma Migrate has no `down` migrations.
- Keep `migration_lock.toml` in git.

## Common mistakes

- **Running `migrate dev` in production** (or against a shared database).
- **Using `db push` outside local prototypes.**
- **Applying generated SQL without reading it**, so renames become drop/add and lose data.
- **Editing a migration after it was applied elsewhere.**
- **Forgetting `prisma generate`** after schema changes.
- **Running migrations on every replica's startup.**
- **Migrating through a transaction-mode pooler.**
- **Not giving the shadow database permissions**, so `migrate dev` fails.
- **Expecting down migrations.**
- **Non-concurrent index creation** on large tables.
- **Not baselining** an existing database before adopting Migrate.

## Debugging

- "Drift detected": the database differs from the migration history (manual changes, or an edited migration). In dev, reset; in production, write a corrective migration.
- `migrate dev` fails creating the shadow database: permissions, or configure a shadow database URL.
- "Migration ... failed to apply cleanly to the shadow database": the SQL has an error or an ordering issue; fix the SQL (you edited it) and retry.
- A failed production migration blocks `migrate deploy`: inspect with `migrate status`, repair the state, then `migrate resolve`.
- Schema and client out of sync at runtime: you didn't run `prisma generate` in the build, or deployed code without its migration.

## Quick Summary

- Migrate = schema diff → timestamped SQL folders → applied in order and tracked in `_prisma_migrations`.
- `migrate dev` for development only; `migrate deploy` for CI/production, as a deploy step with a direct connection.
- Use `--create-only` to review and edit SQL (renames, backfills, concurrent indexes); never edit applied migrations.
- Baseline existing databases with `migrate diff` + `migrate resolve --applied`; fix failures with `migrate resolve`.
- No down migrations: roll forward. Use `db push` only for disposable local work; seed idempotently.

## Next

[Transactions →](./07-transactions.md)
