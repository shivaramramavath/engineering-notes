# Migrations

TypeORM migrations are TypeScript classes with an `up()` that applies a schema change and a `down()` that reverses it. They run in order, once, and are tracked in a `migrations` table. TypeORM can **generate** them by diffing your entities against the live database, which is convenient and also the source of most migration mistakes.

The general principles (expand/contract, safe operations, running migrations as a deploy step) are in [Database foundations: migrations](../02-database-foundations/05-migrations.md). This note covers the TypeORM-specific workflow.

Prerequisites: [Setup](./01-setup.md) (especially the CLI `DataSource`), [entities](./02-entities.md).

## What a migration looks like

```ts
// src/migrations/1700000000000-AddUserRole.ts
import { MigrationInterface, QueryRunner } from 'typeorm';

export class AddUserRole1700000000000 implements MigrationInterface {
  name = 'AddUserRole1700000000000';

  public async up(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query(`ALTER TABLE "users" ADD "role" character varying NOT NULL DEFAULT 'user'`);
  }

  public async down(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query(`ALTER TABLE "users" DROP COLUMN "role"`);
  }
}
```

The class name ends with a timestamp, which determines run order. Generated migrations contain raw SQL via `queryRunner.query`; you can also use the schema-builder helpers on `queryRunner` (`createTable`, `addColumn`, `createIndex`, ...), but plain SQL is easier to review.

## The CLI workflow

The CLI needs a `DataSource` file ([setup](./01-setup.md)) and uses the scripts from there:

```bash
npm run migration:generate -- src/migrations/AddUserRole     # diff entities vs DB → new migration file
npm run migration:run                                         # apply pending migrations
npm run migration:revert                                      # undo the LAST applied migration
npx typeorm-ts-node-commonjs migration:show -d src/data-source.ts   # list applied/pending
npx typeorm-ts-node-commonjs migration:create src/migrations/BackfillSlugs   # empty migration for hand-written work
```

(Exact script names depend on your `package.json`; the underlying commands are `migration:generate`, `migration:create`, `migration:run`, `migration:revert`, `migration:show`, each taking `-d <data-source file>`.)

**The development loop:**

```text
1. change an entity
2. migration:generate           (the DB must be at the CURRENT migration level first)
3. READ the generated SQL; fix it
4. migration:run
5. commit the entity change AND the migration together
```

### Why the database must be up to date before `generate`

`generate` compares entities to the **actual database schema**. If your local DB is behind (pending migrations not yet run) or ahead (manual changes, `synchronize`), the diff will include unrelated changes or miss yours. Run pending migrations first; keep a clean local database; never use `synchronize` on a database you generate against.

## Always review generated SQL

Generation is a diff, not an understanding of your intent:

| You did | Generated (often) | Problem |
|---------|-------------------|---------|
| Renamed a column | `DROP COLUMN old` + `ADD COLUMN new` | **Data loss** |
| Renamed a table/entity | Drop + create | Data loss |
| Added a `NOT NULL` column to a populated table | `ADD ... NOT NULL` without a backfill | Fails on existing rows |
| Changed a column type | Drop + add (for some types) | Data loss |
| Added an index | `CREATE INDEX` (not concurrent) | Blocks writes on large tables |

Fix by hand:

```ts
// rename instead of drop/add
await queryRunner.query(`ALTER TABLE "users" RENAME COLUMN "name" TO "full_name"`);

// add NOT NULL safely: nullable → backfill → constraint
await queryRunner.query(`ALTER TABLE "users" ADD "slug" character varying`);
await queryRunner.query(`UPDATE "users" SET "slug" = lower(replace("name", ' ', '-'))`);
await queryRunner.query(`ALTER TABLE "users" ALTER COLUMN "slug" SET NOT NULL`);
```

For breaking changes across a rolling deploy use expand/contract ([foundations](../02-database-foundations/05-migrations.md)).

## Transactions and special statements

By default TypeORM runs **all pending migrations in one transaction** (`migrationsTransactionMode: 'all'`). Options: `'each'` (one transaction per migration) or `'none'`.

Some statements can't run inside a transaction, notably PostgreSQL's `CREATE INDEX CONCURRENTLY`. Newer TypeORM versions let a migration opt out with a property on the class:

```ts
export class AddOrdersIndex1700000300000 implements MigrationInterface {
  transaction = false;       // check that your TypeORM version supports this per-migration flag

  async up(queryRunner: QueryRunner) {
    await queryRunner.query(`CREATE INDEX CONCURRENTLY "idx_orders_user" ON "orders" ("user_id")`);
  }
  async down(queryRunner: QueryRunner) {
    await queryRunner.query(`DROP INDEX CONCURRENTLY "idx_orders_user"`);
  }
}
```

If your version lacks it, set `migrationsTransactionMode` to `'each'`/`'none'` as appropriate, or apply that statement outside the migration tool. Verify behavior on a copy of production data.

## Data migrations

Use `queryRunner.query` with SQL, in batches for large tables:

```ts
await queryRunner.query(`
  UPDATE "users" SET "slug" = ...
  WHERE "id" IN (SELECT "id" FROM "users" WHERE "slug" IS NULL LIMIT 1000)
`);
```

**Don't import your current entity classes** into migrations to do data work: entities change over time, and an old migration would then break or behave differently. Use SQL, or define minimal local shapes.

## Running migrations in each environment

**Development/CI:** use the TS CLI as above, or `dataSource.runMigrations()` in test setup ([integration testing](../01-testing/05-integration-testing.md)).

**Production:** run the **compiled** migrations as a separate deploy step, before starting the new app version:

```bash
nest build
npx typeorm migration:run -d dist/data-source.js
```

Because the compiled `data-source.js` lives under `dist`, make sure the `migrations` glob uses `__dirname` (as in the [setup](./01-setup.md) example) so it resolves `.js` files there and `.ts` files under `ts-node`.

Avoid `migrationsRun: true` with multiple replicas: several instances race on startup, and a failing migration crash-loops the whole fleet. Migrations take a lock in the migrations table, but a dedicated deploy step is safer and clearer.

## Rolling back

`migration:revert` undoes **only the most recent** migration by running its `down()`. It can't restore dropped data, and by the time you need it the app may have written new-shape rows. In production the practical rollback is a **new forward migration**; treat `down()` as a development convenience (and write it anyway so local iteration works).

## Naming and conventions

- Descriptive names: `AddUserRole`, `CreateOrdersTable`, `BackfillUserSlugs`.
- One logical change per migration.
- Never edit a migration that has run anywhere shared; add a new one.
- Commit migrations together with the entity change.
- Keep the CLI `DataSource` and Nest config consistent (naming strategy, entities, database).
- Name constraints and indexes explicitly when you'll need to recognize them in errors ([database errors](../02-database-foundations/07-database-errors.md)).

## Common mistakes

- **Using `synchronize: true`** alongside migrations, so the DB drifts and `generate` produces nonsense.
- **Generating against an out-of-date database.**
- **Committing generated SQL unread**: renames become drop/add.
- **Editing a migration after it ran elsewhere.**
- **Different CLI and app configuration** (naming strategy or entities), producing migrations that don't match runtime behavior.
- **`migrationsRun: true` with many replicas.**
- **Importing entity classes** into data migrations.
- **Non-concurrent index builds** on big tables.
- **One huge backfill statement** locking the table.
- **Relying on `down()`** for production recovery.
- **Forgetting to build** (or to point at `dist`) when running migrations in production.

## Debugging

- `No migrations are pending`: the file isn't matched by the `migrations` glob, or it's already recorded as run.
- `generate` says "No changes in database schema were found" when you expected some: the entity isn't in the CLI `DataSource`, or the DB is already ahead.
- `generate` produces a huge unrelated diff: the DB isn't at the current migration level, or the naming strategy differs from the one used to build the schema.
- `QueryFailedError: relation ... already exists`: a migration partially ran or the schema was changed manually; fix the state before re-running.
- `Cannot use import statement outside a module`: wrong CLI binary for your module system (`typeorm-ts-node-commonjs` vs `-esm`), or run compiled JS.
- Check the `migrations` table to see what the database believes has run.

## Quick Summary

- Migrations are timestamped classes with `up`/`down`, tracked in a `migrations` table; the CLI needs its own `DataSource`.
- Loop: change entity → `migration:generate` (on an up-to-date DB) → **review and fix SQL** → `migration:run` → commit both.
- Generated diffs turn renames into drop/add and ignore backfills and concurrency; hand-edit.
- Run compiled migrations as a deploy step; avoid `migrationsRun` with replicas; never use `synchronize` in shared environments.
- Use SQL (not entities) for data migrations; prefer rolling forward over `revert`.

## Next

[Transactions →](./07-transactions.md)
