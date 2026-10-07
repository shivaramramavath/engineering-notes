# Migrations

A migration is a **versioned, repeatable script that changes your database schema**: add a table, add a column, create an index. Migrations live in git next to the code, run in order, and are recorded in the database so each runs exactly once. They turn "what does production's schema look like?" from folklore into a reproducible answer.

Prerequisites: [Database architecture](./01-database-architecture.md), [database connection](./02-database-connection.md).

## Why not just let the ORM sync the schema?

Some ORMs can auto-create or alter tables from your models (TypeORM's `synchronize: true`, Prisma's `db push`). That's convenient for prototypes and dangerous beyond them:

- It can **drop columns or data** when a model changes, with no review.
- There's **no history**, so you can't reproduce or audit changes, or roll forward predictably.
- Different environments drift apart.
- You can't control **how** the change is applied (locking, ordering, data backfills).

Rule: `synchronize`/`db push` only for disposable local databases. Anything shared uses migrations.

## How migrations work

```text
migrations/
├── 1700000000000-CreateUsers.ts
├── 1700000100000-AddEmailIndex.ts
└── 1700000200000-AddUserRole.ts

database: table "migrations" (or "_prisma_migrations") records which have run
```

- Each migration has a **sortable identifier** (usually a timestamp).
- The tool compares the folder with the recorded history and runs the **pending** ones in order.
- Applied migrations are **immutable**: never edit one that has run anywhere shared. Add a new migration to change course.

## Two ways to author them

| | Generated | Hand-written |
|-|-----------|--------------|
| How | The tool diffs models against the database and emits SQL | You write the SQL/DDL |
| Good for | Routine schema changes | Data backfills, concurrent index builds, renames, complex changes |
| Risk | May produce destructive SQL (drop + add instead of rename) | Mistakes in raw SQL |

**Always read generated SQL before committing it.** A column rename often comes out as "drop old column, add new column", which loses data. Generate, review, adjust.

## Workflows per tool (high level)

Details live in the ORM sections; this is the shape.

**TypeORM** ([details](../03-typeorm/06-migrations.md))

```bash
npm run typeorm migration:generate -- -d src/data-source.ts src/migrations/AddUserRole
npm run typeorm migration:run -- -d src/data-source.ts
npm run typeorm migration:revert -- -d src/data-source.ts
```

(Scripts typically wrap the TypeORM CLI; exact flags depend on your version and setup.)

**Prisma** ([details](../04-prisma/06-migrations.md))

```bash
npx prisma migrate dev --name add_user_role   # development: create + apply, may reset the dev DB
npx prisma migrate deploy                     # production/CI: apply committed migrations only
npx prisma migrate reset                      # dev/test only: drop, recreate, re-apply
```

`migrate dev` is for development machines; `migrate deploy` is what runs in CI and production.

**Mongoose / MongoDB** ([details](../05-mongoose/README.md)): there's no built-in schema migration, since the schema lives in application code. Data shape changes are handled by scripts or a migration tool (such as `migrate-mongo`), plus tolerant reads of old-shaped documents.

## Running migrations safely in deployments

- **Run them as a separate deploy step**, before starting the new version. Running on app boot with several instances racing is risky (tools use locks, but a failed migration crash-looping every pod is a bad failure mode).
- Use the **same migration files** in every environment: dev, test, CI, staging, production.
- Take a **backup or snapshot** before destructive changes.
- Apply migrations to a fresh database in CI to prove they work from scratch, and use that database in [integration tests](../01-testing/05-integration-testing.md).

## Zero-downtime changes: expand and contract

During a rolling deploy, **old and new app versions run at the same time against the same schema**. A migration that breaks the old version causes errors mid-deploy.

Make breaking changes in steps ("expand/contract"):

```text
Rename users.name → users.full_name

1. EXPAND   add column full_name (nullable); deploy code that writes BOTH, reads new-with-fallback
2. BACKFILL copy name → full_name (in batches)
3. SWITCH   deploy code that uses only full_name
4. CONTRACT drop column name (a later migration, after nothing uses it)
```

More generally, each migration should be compatible with the **previous** app version, and each app version with the **next** migration.

## Risk by operation (PostgreSQL-flavored)

| Operation | Risk | Safer approach |
|-----------|------|----------------|
| Add nullable column | Low | Fine |
| Add `NOT NULL` column without default to a big table | Breaks inserts from old code; can lock | Add nullable → backfill → add constraint |
| Add column with a constant default | Low on modern PostgreSQL, heavier on older versions/other engines | Check your engine/version behavior |
| Create index on a large table | Blocks writes while building | `CREATE INDEX CONCURRENTLY` (can't run inside a transaction block, so the migration tool must be told not to wrap it) |
| Add foreign key / `CHECK` on a large table | Full-table validation under lock | Add as `NOT VALID`, then `VALIDATE CONSTRAINT` separately |
| Rename a column/table | Breaks the running old version | Expand/contract |
| Change a column type | May rewrite the table, take locks | New column + backfill + switch |
| Drop a column/table | Irreversible; breaks old code that still reads it | Only after nothing uses it; keep a backup |

Other engines (MySQL, SQL Server) have different locking behavior; check the docs for yours.

## Data migrations

Separate **schema** changes from **data** changes where you can:

- Backfill in **batches** (for example 1,000 rows at a time), not one giant `UPDATE`, to avoid long locks and bloated transactions.
- Make data migrations **idempotent** so a retry is safe.
- For very large backfills, run them as a background job outside the migration tool.

## Rolling back

Down/revert migrations exist but are unreliable in production: they can't restore dropped data, and the app may have already written new-shape data. The practical rollback is **roll forward**: write a new migration that fixes the problem, relying on backups for disasters. Write `down` steps for development convenience if your tool supports them, but don't plan production recovery around them.

## Conventions

- One logical change per migration; descriptive names (`AddUserRole`, not `Update1`).
- Never edit a migration that has been applied beyond your machine.
- Commit migrations with the code that needs them, in the same pull request.
- Review migration SQL in code review like application code.
- Keep migrations independent of your current entity classes (don't import models that may change later); use SQL or minimal inline definitions.
- Don't rely on `synchronize`, even "just in staging".

## Common mistakes

- **`synchronize: true` in shared environments**, risking data loss and drift.
- **Committing generated migrations unread** (silent drops, missed renames).
- **Editing an already-applied migration**, so environments diverge.
- **Running migrations on app startup** across many replicas.
- **Dropping or renaming in one step** while old code is still running.
- **Building indexes without `CONCURRENTLY`** on big tables.
- **One huge backfill statement** locking the table.
- **Migrations that import live entity classes**, which break when the entities change later.
- **Different migration history per environment** (manual hotfixes on the database).

## Debugging

- "relation/column does not exist" at runtime: migrations didn't run in that environment, or ran out of order.
- Migration fails halfway: check whether your tool wraps migrations in transactions (DDL is transactional in PostgreSQL, not in MySQL), and fix the state before re-running.
- Schema drift (database differs from migrations): compare with the tool's diff/status command (`migrate status`, `migrate diff`, or the equivalent) and bring them back in sync with a new migration.
- Deploy errors from the old version after migrating: the migration wasn't backward compatible; use expand/contract next time.
- Hangs during deploy: a blocking DDL statement waiting for locks; check active/blocked queries.

## Quick Summary

- Migrations are versioned, ordered, run-once schema changes stored in git; never edit applied ones.
- Use `synchronize`/`db push` only for disposable local databases.
- Generate then **review** the SQL; hand-write for backfills, renames, and concurrent index builds.
- Run migrations as a deploy step; make each one compatible with the running app version (expand/contract).
- Prefer rolling forward over reverting; backfill data in idempotent batches.

## Next

[Indexing and query basics →](./06-indexing-and-query-basics.md)
