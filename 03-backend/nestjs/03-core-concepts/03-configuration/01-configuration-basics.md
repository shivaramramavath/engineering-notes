# Configuration Basics

Nest's official configuration package, `@nestjs/config`, loads environment variables (from `.env` files and `process.env`) and exposes them through an injectable `ConfigService`. You could read `process.env` directly everywhere. This package gives you one loading point, DI-friendly access, defaults, validation, and testability.

Principle behind it: store config in the **environment**, not in code ([twelve-factor](https://12factor.net/config)). Keep the artifact identical across dev, staging, and production.

## Setup

```bash
npm i @nestjs/config
```

```ts
// app.module.ts
import { ConfigModule } from '@nestjs/config';

@Module({
  imports: [ConfigModule.forRoot({ isGlobal: true })],
})
export class AppModule {}
```

```bash
# .env  (never commit this file)
PORT=3000
DATABASE_URL=postgres://user:pass@localhost:5432/app
JWT_SECRET=dev-only-secret
```

`isGlobal: true` makes `ConfigService` injectable everywhere without importing `ConfigModule` into each feature module ([global modules](../04-modules-and-di/02-global-modules.md)).

## Reading values

```ts
import { ConfigService } from '@nestjs/config';

@Injectable()
export class MailService {
  constructor(private readonly config: ConfigService) {}

  send() {
    const host = this.config.get<string>('SMTP_HOST');                 // string | undefined
    const port = this.config.get<number>('SMTP_PORT', 587);            // with default
    const key = this.config.getOrThrow<string>('SMTP_API_KEY');        // throws if missing
  }
}
```

| Method | Behavior |
|--------|----------|
| `get(key)` | Returns the value or `undefined` |
| `get(key, default)` | Returns the default when the value is `undefined` |
| `getOrThrow(key)` | Throws if the value is `undefined`. Use for required settings |

Use `getOrThrow` for anything the app can't run without, so a missing value fails loudly where it is used. Better still, validate everything at startup ([next note](./02-configuration-validation.md)).

The generic (`get<number>`) is a **type assertion, not a conversion**. See "Values are strings" below.

### In `main.ts`

```ts
const app = await NestFactory.create(AppModule);
const config = app.get(ConfigService);
await app.listen(config.get<number>('PORT', 3000));
```

## `.env` files and precedence

```ts
ConfigModule.forRoot({
  envFilePath: ['.env.local', '.env'],   // default is ['.env'] in the project root
});
```

Rules:

- **`process.env` wins over `.env` files.** A variable set in the shell, Docker, or your platform overrides the file.
- With multiple files, the **first file listed wins** for a duplicated key.
- A missing file is skipped silently.

Common layouts:

```ts
envFilePath: [`.env.${process.env.NODE_ENV}.local`, `.env.${process.env.NODE_ENV}`, '.env']
```

In production you normally don't ship a `.env` at all; the platform injects real environment variables:

```ts
ConfigModule.forRoot({ ignoreEnvFile: process.env.NODE_ENV === 'production' });
```

Commit a **`.env.example`** with every key and a safe placeholder, so new developers know what to set. Add `.env*` (except the example) to `.gitignore`.

## Values are strings

Environment variables are always strings.

```ts
process.env.ENABLE_CACHE = 'false';
if (config.get('ENABLE_CACHE')) { /* runs! 'false' is a truthy string */ }

config.get<number>('PORT');   // '3000' at runtime, despite <number>
```

Fix by converting explicitly, or let validation do it ([validation](./02-configuration-validation.md)):

```ts
const enabled = config.get('ENABLE_CACHE') === 'true';
const port = Number(config.get('PORT') ?? 3000);
```

Avoid `parseInt(x) || default`; it treats a legitimate `0` as missing.

## The timing trap: `process.env` at import time

`ConfigModule.forRoot()` loads `.env` when Nest builds the module graph. Code that runs **earlier** (at import time) sees an empty environment:

```ts
// ❌ evaluated when the file is imported, before .env is loaded
@Module({
  imports: [JwtModule.register({ secret: process.env.JWT_SECRET })],   // undefined!
})
```

Fix with the async variant, which runs after config is loaded:

```ts
// ✅ evaluated during module initialization
JwtModule.registerAsync({
  inject: [ConfigService],
  useFactory: (config: ConfigService) => ({ secret: config.getOrThrow<string>('JWT_SECRET') }),
});
```

The same pattern applies to `TypeOrmModule.forRootAsync`, `BullModule.forRootAsync`, `CacheModule.registerAsync`, etc. ([dynamic modules](../04-modules-and-di/03-dynamic-modules.md)).

Top-level code outside Nest, such as a standalone TypeORM CLI data source file, doesn't get `ConfigModule` at all. Load `dotenv` yourself there.

## Useful `forRoot` options

| Option | Purpose |
|--------|---------|
| `isGlobal` | Make `ConfigService` available app-wide |
| `envFilePath` | One path or an ordered list |
| `ignoreEnvFile` | Only use `process.env` (typical in production) |
| `cache: true` | Cache values read from `process.env` for faster repeated reads |
| `expandVariables: true` | Support `${VAR}` references inside `.env` values |
| `load` | Register [custom configuration factories](./03-custom-configuration.md) |
| `validationSchema` / `validate` | [Validate](./02-configuration-validation.md) at startup |

```bash
# expandVariables: true
APP_URL=http://localhost:${PORT}
```

## Testing

Don't depend on a real `.env` in tests. Supply values directly:

```ts
const moduleRef = await Test.createTestingModule({
  imports: [
    ConfigModule.forRoot({ ignoreEnvFile: true, isGlobal: true, load: [() => ({ JWT_SECRET: 'test' })] }),
    AuthModule,
  ],
}).compile();
```

or mock the service:

```ts
{ provide: ConfigService, useValue: { get: jest.fn().mockReturnValue('test') } }
```

## Common mistakes

- **Reading `process.env` in decorators or top-level code** before `ConfigModule` has run.
- **Committing `.env`** with real secrets. Rotate anything that leaked, since git history keeps it.
- **Trusting `get<number>()`** to convert.
- **Treating `'false'` as false.**
- **Scattering `process.env.X` through the codebase**, which defeats validation and testing. Read config through `ConfigService` or typed namespaces.
- **Forgetting `isGlobal`** and then hitting "Nest can't resolve dependencies of X (ConfigService?)" in feature modules.
- **Expecting runtime reload.** Config is read at startup; changing env vars needs a restart.
- **Logging the whole config** (it contains secrets).

## Debugging

- Value `undefined`? Check the file path (relative to the **working directory**, not the source file), spelling, and whether `process.env` is overriding the file.
- Works locally, missing in Docker/CI? The `.env` file isn't in the image (correct), and the variable wasn't passed to the container.
- `Can't resolve dependencies of X (ConfigService)`: import `ConfigModule` in that module or set `isGlobal`.
- Unsure what's loaded? Temporarily log **keys only**: `Object.keys(process.env).filter(k => k.startsWith('APP_'))`.

## Quick Summary

- Use `@nestjs/config`: `ConfigModule.forRoot({ isGlobal: true })` + `ConfigService`.
- `process.env` beats `.env`; with several env files, the first listed wins.
- Env values are strings; convert explicitly or via validation.
- Never read `process.env` at import time; use `forRootAsync` / `registerAsync` with `ConfigService`.
- Use `getOrThrow` for required values; keep `.env` out of git and commit `.env.example`.

## Next

[Configuration validation →](./02-configuration-validation.md)
