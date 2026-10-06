# Custom Configuration

Flat `ConfigService.get('DATABASE_HOST')` calls scale badly: keys are stringly typed, grouping is by naming convention, and parsing is repeated. **Custom configuration files** (config factories) solve this by turning raw environment variables into a **structured, typed object**, grouped into namespaces like `database` and `auth`.

Prerequisites: [Configuration basics](./01-configuration-basics.md), [validation](./02-configuration-validation.md).

## A config factory

A factory is a function returning a plain object. `load` merges it into the config store.

```ts
// config/app.config.ts
export default () => ({
  port: parseInt(process.env.PORT ?? '3000', 10),
  database: {
    host: process.env.DATABASE_HOST ?? 'localhost',
    port: parseInt(process.env.DATABASE_PORT ?? '5432', 10),
  },
});
```

```ts
ConfigModule.forRoot({ isGlobal: true, load: [appConfig] });
```

```ts
this.config.get<number>('database.port');   // dot notation into nested objects
this.config.get('database')                 // the whole object
```

Reading `process.env` here is safe: the factory runs after `.env` is loaded, unlike top-level code in a decorator ([timing trap](./01-configuration-basics.md)).

## Namespaces with `registerAs`

`registerAs` gives a factory a **namespace** and a typed injection token.

```ts
// config/database.config.ts
import { registerAs } from '@nestjs/config';

export default registerAs('database', () => ({
  host: process.env.DATABASE_HOST ?? 'localhost',
  port: parseInt(process.env.DATABASE_PORT ?? '5432', 10),
  name: process.env.DATABASE_NAME,
  ssl: process.env.DATABASE_SSL === 'true',
}));
```

```ts
// config/auth.config.ts
export default registerAs('auth', () => ({
  jwtSecret: process.env.JWT_SECRET,
  accessTtl: process.env.JWT_ACCESS_TTL ?? '15m',
}));
```

```ts
ConfigModule.forRoot({ isGlobal: true, load: [databaseConfig, authConfig] });
```

Two ways to consume it:

```ts
// 1. By path, flexible but stringly typed
this.config.get<string>('database.host');

// 2. By injection token, fully typed
import { ConfigType } from '@nestjs/config';

@Injectable()
export class DbService {
  constructor(
    @Inject(databaseConfig.KEY)
    private readonly db: ConfigType<typeof databaseConfig>,
  ) {}

  url() {
    return `${this.db.host}:${this.db.port}`;   // db.host is typed
  }
}
```

`ConfigType<typeof databaseConfig>` infers the exact shape from the factory, so renaming a property is a compile error rather than a runtime `undefined`.

## Feeding async module factories

Typed namespaces fit `forRootAsync` well:

```ts
TypeOrmModule.forRootAsync({
  inject: [databaseConfig.KEY],
  useFactory: (db: ConfigType<typeof databaseConfig>) => ({
    type: 'postgres',
    host: db.host,
    port: db.port,
    database: db.name,
    ssl: db.ssl,
  }),
});
```

See [TypeORM setup](../../04-intermediate/03-typeorm/01-setup.md) and [database connection](../../04-intermediate/02-database-foundations/02-database-connection.md).

## Partial registration with `forFeature`

If only one module needs a namespace, register it there instead of globally:

```ts
@Module({
  imports: [ConfigModule.forFeature(mailConfig)],
  providers: [MailService],
})
export class MailModule {}
```

The namespace is then available to providers in that module (via `mailConfig.KEY`, or `ConfigService` where the module sees it). This keeps feature configuration next to the feature that owns it.

## Validate first, then structure

Validation and `load` factories are separate steps:

```text
.env + process.env ──► validate / validationSchema ──► load factories (registerAs) ──► ConfigService
```

A good pattern: validate the **raw** env vars (so you catch missing/invalid values at startup) and let factories do the grouping and light parsing. If validation already converts types (Joi/Zod), your factories can rely on them being well-formed. Reading values through `ConfigService` after validation is the safest way to get the converted forms. Check this in your own app with a quick log of types if you mix `process.env` reads with validated values.

## Other sources: YAML, JSON, remote

A factory can return anything from any source. YAML example:

```ts
import { readFileSync } from 'node:fs';
import * as yaml from 'js-yaml';
import { join } from 'node:path';

export default () =>
  yaml.load(readFileSync(join(__dirname, 'config.yaml'), 'utf8')) as Record<string, any>;
```

Notes:

- Non-TS files like `config.yaml` aren't copied to `dist` by the build by default. Add them to the Nest CLI `assets` option in `nest-cli.json`, or load from a path outside `dist`.
- Secrets pulled from a secret manager (AWS Secrets Manager, Vault) are usually fetched at startup. Factories can be asynchronous in current `@nestjs/config`; verify against your installed version, and see [secrets management](../../07-production/01-security/06-secrets-management.md).
- Don't put real secrets in YAML/JSON files that you commit.

## Structuring a real project

```text
src/config/
├── app.config.ts
├── database.config.ts
├── auth.config.ts
├── mail.config.ts
├── env.validation.ts      # validation schema/class
└── config.module.ts       # optional wrapper
```

```ts
// config/config.module.ts
@Module({
  imports: [
    ConfigModule.forRoot({
      isGlobal: true,
      load: [appConfig, databaseConfig, authConfig, mailConfig],
      validate,
    }),
  ],
})
export class AppConfigModule {}
```

Import `AppConfigModule` once in `AppModule`. Keep one namespace per concern, rather than one giant config object.

## Typed `ConfigService`

For path-based access with inference, describe the shape and pass it as a generic:

```ts
type AppConfig = {
  database: { host: string; port: number };
  auth: { jwtSecret: string };
};

constructor(private readonly config: ConfigService<AppConfig, true>) {}

const host = this.config.get('database.host', { infer: true });   // string
```

This relies on you keeping the type in sync with the factories. Injecting `ConfigType<typeof xConfig>` is safer because the type is derived from the code, so prefer it for new code.

## Practical guidance

- **Group by concern** (`database`, `auth`, `mail`), not by environment. Environments differ by *values*, not by shape.
- **Resolve defaults and parsing in the factory**, once. Consumers shouldn't parse strings.
- **Don't default secrets** in factories.
- **Keep factories pure**: read env, return an object. No I/O unless you intend async loading.
- **Freeze when appropriate**: `Object.freeze` on returned objects prevents accidental mutation of shared config.
- Prefer **injection of the namespace** (`ConfigType`) over `ConfigService.get('a.b.c')` strings in application code.

## Common mistakes

- **Typos in dot paths** (`'databse.host'`) returning `undefined` silently. Prefer typed namespace injection.
- **Using `parseInt(x) || default`** and clobbering a valid `0`.
- **Booleans via truthiness**: `Boolean(process.env.SSL)` is true for `'false'`. Compare to `'true'` explicitly.
- **Forgetting to add a factory to `load`**: the namespace is empty and `@Inject(xConfig.KEY)` fails to resolve.
- **Relying on `forFeature` config from a module that doesn't import it.**
- **A single huge config object** that every service depends on, which makes testing painful.
- **Missing `assets` setup** for YAML/JSON config, so it works with `ts-node` and breaks in `dist`.
- **Mutating config at runtime**, which creates hidden global state.

## Debugging

- `Nest can't resolve dependencies ... CONFIGURATION(database)`: the factory isn't in `load` or `forFeature` for that module.
- Value `undefined`? Print `config.get('database')` and look at the actual object, then check the env var name and whether validation converted or defaulted it.
- Works in dev, fails after build? Check that non-TS config files are included in `dist` (`nest-cli.json` assets).
- Wrong value in one environment? Remember `process.env` overrides `.env`; check what the platform injects.

## Testing

Provide a fake namespace without touching the environment:

```ts
{ provide: databaseConfig.KEY, useValue: { host: 'localhost', port: 5432, name: 'test', ssl: false } }
```

## Quick Summary

- Config factories turn raw env into a structured object; `registerAs` adds a namespace and a typed `KEY`.
- Inject `@Inject(xConfig.KEY) cfg: ConfigType<typeof xConfig>` for compile-time safety.
- `forFeature` keeps module-specific config local; `load` registers it globally.
- Validate raw env first, then group/parse in factories; never default secrets.
- Non-TS config files need build asset handling; async/secret-manager sources are possible but verify support in your version.

## Next

Section complete. Continue with [Modules and DI](../04-modules-and-di/README.md).

← Back to [Configuration overview](./README.md)
