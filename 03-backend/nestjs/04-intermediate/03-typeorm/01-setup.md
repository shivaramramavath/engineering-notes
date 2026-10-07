# TypeORM Setup

Getting TypeORM running in Nest takes three pieces: the **root connection** (`forRoot`/`forRootAsync`), per-module **entity registration** (`forFeature`), and a separate **`DataSource` file** for the CLI (migrations). Most setup bugs come from mixing those up.

Prerequisites: [Database connection](../02-database-foundations/02-database-connection.md), [dynamic modules](../../03-core-concepts/04-modules-and-di/03-dynamic-modules.md), [configuration](../../03-core-concepts/03-configuration/README.md).

## Install

```bash
npm i @nestjs/typeorm typeorm pg        # pg = PostgreSQL driver
# other drivers: mysql2, better-sqlite3 / sqlite3, mssql, ...
```

You must install the driver for your database yourself; TypeORM doesn't bundle it.

## Root connection

Configure asynchronously from validated config (never read `process.env` at import time):

```ts
// app.module.ts
@Module({
  imports: [
    ConfigModule.forRoot({ isGlobal: true }),
    TypeOrmModule.forRootAsync({
      inject: [ConfigService],
      useFactory: (config: ConfigService) => ({
        type: 'postgres',
        url: config.getOrThrow<string>('DATABASE_URL'),
        autoLoadEntities: true,           // entities registered via forFeature are picked up
        synchronize: false,               // schema changes go through migrations
        logging: config.get('NODE_ENV') === 'development' ? ['query', 'error'] : ['error'],
        retryAttempts: 5,
        retryDelay: 3000,
        extra: { max: 10 },               // pg pool size
      }),
    }),
    UsersModule,
  ],
})
export class AppModule {}
```

Key options:

| Option | Notes |
|--------|-------|
| `type` | `'postgres'`, `'mysql'`, `'mariadb'`, `'sqlite'`, `'mssql'`, ... |
| `url` or `host/port/username/password/database` | Prefer a single URL from env |
| `autoLoadEntities` | Entities in any `forFeature` are added automatically; avoids maintaining an `entities` list or glob |
| `entities` | Alternative: explicit classes or a glob (globs behave differently under `ts-node` vs compiled `dist`) |
| `synchronize` | Auto-alters the schema from entities. **Never in shared environments** ([migrations](./06-migrations.md)) |
| `logging` | `true`, or an array: `'query'`, `'error'`, `'schema'`, `'warn'`, `'migration'` |
| `retryAttempts` / `retryDelay` | Nest-level startup retries when the DB isn't ready |
| `migrationsRun` | Run pending migrations on app start. Convenient locally; avoid with multiple replicas |
| `ssl` | Required by many managed databases; configure certificate verification properly |
| `extra` | Passed to the driver (pool settings, timeouts) |
| `namingStrategy` | Custom table/column naming (for example snake_case via a community strategy package) |

Env vars for the example:

```bash
DATABASE_URL=postgres://app:secret@localhost:5432/app
```

## Registering entities per module

```ts
// users/users.module.ts
@Module({
  imports: [TypeOrmModule.forFeature([User])],
  providers: [UsersService],
  exports: [UsersService],
})
export class UsersModule {}
```

```ts
// users/users.service.ts
@Injectable()
export class UsersService {
  constructor(@InjectRepository(User) private readonly users: Repository<User>) {}

  findAll() {
    return this.users.find();
  }
}
```

`forFeature([User])` does two things: registers `User` with the connection (when `autoLoadEntities` is on) and creates an injectable `Repository<User>` provider. The repository is available only in **that module** unless you export `TypeOrmModule` (or wrap it in a service and export that, the better habit).

Also injectable anywhere the root module is set up:

```ts
constructor(
  @InjectDataSource() private readonly dataSource: DataSource,   // the connection; use for transactions
  private readonly entityManager: EntityManager,
) {}
```

## A separate `DataSource` for the CLI

The TypeORM CLI (migrations) doesn't run Nest, so it can't use `forRootAsync`. Give it its own `DataSource` file:

```ts
// src/data-source.ts
import 'dotenv/config';
import { DataSource } from 'typeorm';

export default new DataSource({
  type: 'postgres',
  url: process.env.DATABASE_URL,
  entities: [__dirname + '/**/*.entity{.ts,.js}'],
  migrations: [__dirname + '/migrations/*{.ts,.js}'],
});
```

Add scripts to `package.json`:

```json
{
  "scripts": {
    "typeorm": "typeorm-ts-node-commonjs",
    "migration:generate": "npm run typeorm -- migration:generate -d src/data-source.ts",
    "migration:run": "npm run typeorm -- migration:run -d src/data-source.ts",
    "migration:revert": "npm run typeorm -- migration:revert -d src/data-source.ts"
  }
}
```

(`typeorm-ts-node-commonjs` is for CommonJS projects; ESM projects use `typeorm-ts-node-esm`.) Details in [migrations](./06-migrations.md).

Keep this file and the Nest config **consistent** (same database, same naming strategy, same entity set). Divergence is a common source of "generated migration doesn't match the app" problems. Some teams build the Nest options from a shared function to avoid duplication.

## Verifying it works

Start the app with query logging on. On boot you should see TypeORM connect (and, with `logging: ['schema']`, nothing destructive). Then run a trivial query:

```ts
await this.dataSource.query('SELECT 1');
```

For a readiness check in production use a [health check](../../07-production/03-observability/04-health-checks.md) that pings the database.

## Testing setup

For [integration/E2E tests](../01-testing/05-integration-testing.md), point `forRootAsync` at a test database and apply migrations; don't use `synchronize: true` just for tests. For unit tests, mock the repository:

```ts
import { getRepositoryToken } from '@nestjs/typeorm';

{ provide: getRepositoryToken(User), useValue: { findOneBy: jest.fn(), save: jest.fn() } }
```

## Common mistakes

- **Reading `process.env` at import time** inside `forRoot(...)` instead of `forRootAsync`.
- **`synchronize: true` in staging/production**, risking data loss.
- **Forgetting the driver package** (`pg`, `mysql2`, ...), producing "driver not found" errors.
- **Forgetting `forFeature([Entity])`**, then `Nest can't resolve dependencies ... RepositoryUser`.
- **Entities glob that works in dev but not in `dist`** (`.ts` vs `.js`). Prefer `autoLoadEntities`.
- **A CLI `DataSource` that differs from the app's configuration.**
- **Relying on `migrationsRun: true`** with multiple replicas starting at once.
- **Skipping TLS** on managed databases.
- **Exporting repositories across modules** instead of exporting a service.

## Debugging

- `Unable to connect to the database. Retrying (n)...`: host/port/credentials or network (inside containers `localhost` isn't the host); check `retryAttempts`.
- `Nest can't resolve dependencies of the UsersService (?)... RepositoryUser`: missing `TypeOrmModule.forFeature([User])` in that module.
- `No metadata for "X" was found` / `EntityMetadataNotFoundError`: the entity isn't registered (missing `forFeature`, glob mismatch, or not in the CLI data source).
- Turn on `logging: ['query', 'error']` temporarily to see the SQL.
- `Cannot use import statement outside a module` when running the CLI: use the right `typeorm-ts-node-*` binary for your module system, or run the compiled JS.

## Quick Summary

- `TypeOrmModule.forRootAsync` creates the connection (from validated config); `forFeature([Entity])` registers entities and provides `Repository<Entity>` per module.
- Use `autoLoadEntities: true`, `synchronize: false`, startup retries, and a sensible pool size.
- The CLI needs its own `DataSource` file (kept consistent with the app's config) for migrations.
- Inject `DataSource`/`EntityManager` for transactions; inject repositories via `@InjectRepository`.
- Mock repositories with `getRepositoryToken(Entity)` in unit tests.

## Next

[Entities →](./02-entities.md)
