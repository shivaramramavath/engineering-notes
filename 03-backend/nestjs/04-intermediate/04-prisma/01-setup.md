# Prisma Setup

Setting up Prisma in Nest means: a schema with a generator, a config file for the CLI, a **generate** step that produces the client, a driver adapter, and an injectable `PrismaService`. Most first-day problems come from the generate step and import paths.

> Targets **Prisma ORM 7**. Prisma 5/6 differences are noted in the version box. Check the current Prisma + NestJS guide for exact commands, since details move between releases.

Prerequisites: [Database connection](../02-database-foundations/02-database-connection.md), [configuration](../../03-core-concepts/03-configuration/README.md).

## Install

```bash
npm i @prisma/client @prisma/adapter-pg pg
npm i -D prisma
npx prisma init --datasource-provider postgresql
```

`prisma init` creates `prisma/schema.prisma`, a `prisma.config.ts`, and an `.env` with a placeholder `DATABASE_URL`. Packages: `prisma` is the CLI (dev dependency), `@prisma/client` is the runtime, `@prisma/adapter-pg` is the PostgreSQL driver adapter (other databases have their own adapters).

## The schema header

```prisma
// prisma/schema.prisma
generator client {
  provider     = "prisma-client"
  output       = "../src/generated/prisma"     // required in v7: where the client is generated
  moduleFormat = "cjs"                         // Nest projects are CommonJS by default
}

datasource db {
  provider = "postgresql"
}

model User {
  id    String @id @default(uuid())
  email String @unique
}
```

- **`output` is required** and the client is generated **into your source tree**, so Nest's TypeScript build compiles it. Add the folder to `.gitignore` (it's a build artifact) and regenerate in CI.
- **`moduleFormat = "cjs"`** matters for CommonJS Nest apps; the generated code otherwise targets ESM. If you see errors about `import.meta` or module syntax at startup, check this setting (and your `tsconfig` module settings).
- The connection **URL no longer lives in the schema** in v7; the CLI reads it from `prisma.config.ts` and the runtime connection comes from the adapter below.

## `prisma.config.ts` (CLI configuration)

```ts
// prisma.config.ts
import 'dotenv/config';                         // v7 does NOT load .env for you
import { defineConfig, env } from 'prisma/config';

export default defineConfig({
  schema: 'prisma/schema.prisma',
  migrations: {
    path: 'prisma/migrations',
    seed: 'ts-node prisma/seed.ts',             // used by `prisma db seed` (choose your own runner)
  },
  datasource: {
    url: env('DATABASE_URL'),
  },
});
```

This file configures the **CLI** (migrate, generate, studio, seed). It is not used by your running Nest app. Install `dotenv` (`npm i dotenv`) for the import above.

## Generate the client

```bash
npx prisma generate
```

Run it after **every schema change**, after a fresh clone/install, and in CI and Docker builds **before** `nest build`. Don't assume other commands run it for you (in v7 you should run it explicitly). A typical `package.json`:

```json
{
  "scripts": {
    "prisma:generate": "prisma generate",
    "build": "prisma generate && nest build",
    "postinstall": "prisma generate"
  }
}
```

## `PrismaService`

```ts
// prisma/prisma.service.ts
import { Injectable, OnModuleDestroy, OnModuleInit } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { PrismaPg } from '@prisma/adapter-pg';
import { PrismaClient } from '../generated/prisma/client';   // path matches your generator `output`

@Injectable()
export class PrismaService extends PrismaClient implements OnModuleInit, OnModuleDestroy {
  constructor(config: ConfigService) {
    const adapter = new PrismaPg({
      connectionString: config.getOrThrow<string>('DATABASE_URL'),
      // pg pool options such as `max` can be passed here too
    });
    super({ adapter });
  }

  async onModuleInit() {
    await this.$connect();          // fail fast on startup
  }

  async onModuleDestroy() {
    await this.$disconnect();       // release the pool on shutdown
  }
}
```

```ts
// prisma/prisma.module.ts
@Global()
@Module({
  providers: [PrismaService],
  exports: [PrismaService],
})
export class PrismaModule {}
```

Import `PrismaModule` once in `AppModule`; every module can then inject `PrismaService` ([global modules](../../03-core-concepts/04-modules-and-di/02-global-modules.md)). One instance means **one connection pool**; never `new PrismaClient()` in multiple places.

Using it:

```ts
@Injectable()
export class UsersService {
  constructor(private readonly prisma: PrismaService) {}

  findAll() {
    return this.prisma.user.findMany();
  }
}
```

Enable [shutdown hooks](../../03-core-concepts/04-modules-and-di/09-application-lifecycle.md) (`app.enableShutdownHooks()`) so `onModuleDestroy` runs on `SIGTERM`.

### Version differences (5/6)

| | Prisma 5/6 | Prisma 7 |
|-|------------|----------|
| Generator | `prisma-client-js` (default output in `node_modules`) | `prisma-client` with required `output` |
| Import | `import { PrismaClient } from '@prisma/client'` | Import from your generated folder |
| URL | `url = env("DATABASE_URL")` in the schema | In `prisma.config.ts` (CLI) and the driver adapter (runtime) |
| Driver adapter | Optional | Required for SQL databases |
| `.env` loading | Automatic | Load it yourself (`dotenv`) |

When upgrading, follow the official upgrade guide rather than patching by hand.

## Pool and connection configuration

With adapters, pool settings come from the underlying driver (for `pg`: `max`, `idleTimeoutMillis`, `connectionTimeoutMillis`). Size it using the formula in [database connection](../02-database-foundations/02-database-connection.md): instances × pool max must stay under the database limit. For serverless or many instances, put a connection pooler (such as PgBouncer or a managed equivalent) in front, and check Prisma's guidance on pooler compatibility.

For migrations through a pooler, use a **direct** connection URL for the CLI (poolers in transaction mode often break migrations), and the pooled one for the app.

## Logging

```ts
super({ adapter, log: ['warn', 'error'] });                     // production
super({ adapter, log: ['query', 'warn', 'error'] });            // development: see every SQL query
```

Watch queries in development to catch N+1 patterns early ([indexing and queries](../02-database-foundations/06-indexing-and-query-basics.md)).

## Testing setup

- **Unit tests:** provide a fake for `PrismaService` with just the methods you use:

```ts
{ provide: PrismaService, useValue: { user: { findUnique: jest.fn(), create: jest.fn() } } }
```

- **Integration/E2E:** point `DATABASE_URL` at a test database, apply migrations with `prisma migrate deploy` (or `migrate reset` locally), and close the client in `afterAll` ([integration testing](../01-testing/05-integration-testing.md)).

## Common mistakes

- **Forgetting `prisma generate`** (fresh clone, CI, Docker build) and getting "cannot find module" for the generated client.
- **Wrong import path** after changing the generator `output`.
- **Committing the generated client** (large, noisy diffs). Generate in the build instead.
- **Not loading `.env` for the CLI** in v7, so `DATABASE_URL` is undefined.
- **Missing `moduleFormat = "cjs"`** in a CommonJS Nest app.
- **Creating multiple `PrismaClient` instances**, multiplying pools.
- **Reading `process.env` in the service** instead of `ConfigService` ([configuration](../../03-core-concepts/03-configuration/01-configuration-basics.md)).
- **No shutdown hooks**, so the pool isn't closed gracefully.
- **Running migrations through a transaction-mode pooler.**

## Debugging

- `Cannot find module '../generated/prisma/client'`: run `prisma generate`; check the `output` path and that the build includes it.
- Errors about `import.meta`/ESM at startup: generator `moduleFormat` and `tsconfig` module settings disagree.
- "Environment variable not found: DATABASE_URL" from the CLI: add `import 'dotenv/config'` to `prisma.config.ts`, or export the variable.
- A constructor error about a missing adapter (v7): pass `{ adapter }` to `super()`.
- Types out of date after a schema change: regenerate and restart the TypeScript server.
- Connection errors: see [database connection debugging](../02-database-foundations/02-database-connection.md).

## Quick Summary

- Schema (`generator` + `datasource`) → `prisma generate` → generated client in your source tree → `PrismaService` → inject.
- v7: generator `prisma-client` with `output` and `moduleFormat = "cjs"`, `prisma.config.ts` for the CLI, a driver adapter at runtime, explicit `dotenv`.
- Run `prisma generate` in dev, CI, and Docker before `nest build`; gitignore the output.
- One global `PrismaService` = one pool; connect in `onModuleInit`, disconnect in `onModuleDestroy`.
- Use a direct URL for migrations, pooled for the app; log queries in development.

## Next

[Schema →](./02-schema.md)
