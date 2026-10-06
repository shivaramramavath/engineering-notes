# Dynamic Modules

A normal module is fixed at write time: its providers and imports are literals in the decorator. A **dynamic module** is configured by its **consumer**. You import it by calling a static method, passing options, and the module builds its providers from them.

You've used this already: `ConfigModule.forRoot({...})`, `TypeOrmModule.forRoot({...})`, `JwtModule.register({...})`. This note explains how to write your own.

Prerequisites: [Modules](./01-feature-and-shared-modules.md), [custom providers](./05-custom-providers.md) (read together if `useFactory` is new to you).

## Why they exist

You want a reusable module (storage client, mailer, feature-flag SDK) that behaves differently per app, such as bucket name, API key, or timeout, without hard-coding it or reading `process.env` inside the module.

## Core concept: a static method returning `DynamicModule`

```ts
// storage.module.ts
import { DynamicModule, Module } from '@nestjs/common';

export const STORAGE_OPTIONS = Symbol('STORAGE_OPTIONS');

export interface StorageOptions { bucket: string; region?: string }

@Module({})
export class StorageModule {
  static forRoot(options: StorageOptions): DynamicModule {
    return {
      module: StorageModule,
      providers: [
        { provide: STORAGE_OPTIONS, useValue: options },
        StorageService,
      ],
      exports: [StorageService],
    };
  }
}
```

```ts
// storage.service.ts
@Injectable()
export class StorageService {
  constructor(@Inject(STORAGE_OPTIONS) private readonly options: StorageOptions) {}
}
```

```ts
// app.module.ts
imports: [StorageModule.forRoot({ bucket: 'my-bucket', region: 'eu-west-1' })]
```

The returned object **extends** the static `@Module({})` metadata (it doesn't replace it). Properties of `DynamicModule` match `@Module`: `providers`, `exports`, `imports`, `controllers`, plus `module` (required) and `global` (optional).

## Naming conventions

Nest's own packages follow these; matching them makes your module predictable.

| Method | Meaning | Examples |
|--------|---------|----------|
| `register` | Configure the module for **the importing module only**; each importer may use different options | `JwtModule.register`, `CacheModule.register` |
| `forRoot` | Configure **once**, app-wide (usually in `AppModule`) | `ConfigModule.forRoot`, `TypeOrmModule.forRoot` |
| `forFeature` | Use the root configuration, but declare **feature-specific** pieces | `TypeOrmModule.forFeature([User])`, `ConfigModule.forFeature(cfg)` |

Each has an `*Async` twin (`registerAsync`, `forRootAsync`, `forFeatureAsync`) that takes a factory instead of literal values.

## Async configuration

Options often come from other providers (`ConfigService`, a secrets client). The async variant defers building the options until DI is ready. The standard shape:

```ts
export interface StorageAsyncOptions {
  imports?: any[];
  inject?: any[];
  useFactory: (...args: any[]) => Promise<StorageOptions> | StorageOptions;
}

@Module({})
export class StorageModule {
  static forRootAsync(opts: StorageAsyncOptions): DynamicModule {
    return {
      module: StorageModule,
      imports: opts.imports ?? [],
      providers: [
        { provide: STORAGE_OPTIONS, useFactory: opts.useFactory, inject: opts.inject ?? [] },
        StorageService,
      ],
      exports: [StorageService],
    };
  }
}
```

```ts
StorageModule.forRootAsync({
  imports: [ConfigModule],
  inject: [ConfigService],
  useFactory: (config: ConfigService) => ({ bucket: config.getOrThrow('BUCKET') }),
});
```

Official modules also accept `useClass` and `useExisting` (an options-factory class). Writing all three variants by hand is repetitive, which is what the builder below solves.

## `ConfigurableModuleBuilder` (recommended)

Nest ships a helper that generates `register`/`registerAsync` (or your chosen names), the options token, and the async variants.

```ts
// storage.module-definition.ts
import { ConfigurableModuleBuilder } from '@nestjs/common';

export interface StorageModuleOptions { bucket: string; region?: string }

export const {
  ConfigurableModuleClass,
  MODULE_OPTIONS_TOKEN,
  OPTIONS_TYPE,
  ASYNC_OPTIONS_TYPE,
} = new ConfigurableModuleBuilder<StorageModuleOptions>()
  .setClassMethodName('forRoot')        // generates forRoot and forRootAsync
  .setExtras({ isGlobal: false }, (definition, extras) => ({
    ...definition,
    global: extras.isGlobal,            // lets consumers pass isGlobal: true
  }))
  .build();
```

```ts
// storage.module.ts
@Module({ providers: [StorageService], exports: [StorageService] })
export class StorageModule extends ConfigurableModuleClass {}
```

```ts
// storage.service.ts
@Injectable()
export class StorageService {
  constructor(@Inject(MODULE_OPTIONS_TOKEN) private readonly options: StorageModuleOptions) {}
}
```

```ts
// usage
StorageModule.forRoot({ bucket: 'b', isGlobal: true });

StorageModule.forRootAsync({
  imports: [ConfigModule],
  inject: [ConfigService],
  useFactory: (c: ConfigService) => ({ bucket: c.getOrThrow('BUCKET') }),
  isGlobal: true,
});
```

Notes:

- Without `setClassMethodName`, the generated methods are `register` / `registerAsync`.
- `setExtras` adds module-level options (like `isGlobal`) that are **not** passed into the options token.
- `OPTIONS_TYPE` / `ASYNC_OPTIONS_TYPE` are for typing wrapper methods you write on top, e.g. a custom `forRoot` that adds defaults.
- The builder also supports `setFactoryMethodName` to rename the method on `useClass` factories. Check the Nest docs for the full builder API for your version.

## `forFeature`: layering on top of `forRoot`

A common structure: `forRoot` sets up shared infrastructure once; `forFeature` registers per-feature pieces that depend on it.

```ts
static forFeature(collections: string[]): DynamicModule {
  const providers = collections.map((name) => ({
    provide: `COLLECTION_${name}`,
    useFactory: (client: StorageClient) => client.collection(name),
    inject: [StorageClient],
  }));
  return { module: StorageModule, providers, exports: providers.map((p) => p.provide) };
}
```

`TypeOrmModule.forFeature([User])` and `MongooseModule.forFeature([...])` work this way.

## Practical points

- **Importing the same dynamic module with different options** yields separate module instances, each with its own providers. That's the point of `register`. For app-wide singletons use `forRoot` in one place.
- **Global dynamic modules**: set `global: true` in the returned object (or expose `isGlobal`) ([global modules](./02-global-modules.md)).
- **Re-exporting** a dynamic module from a shared module needs the dynamic form in both `imports` and `exports`, or export the static class where Nest permits. If it fails to resolve, check the exact module reference you import.
- **Options belong behind a token** (`MODULE_OPTIONS_TOKEN` or a `Symbol`). Don't read `process.env` in the module; accept options so the module is testable ([configuration](../03-configuration/01-configuration-basics.md)).
- **Validate options** early (throw in the factory) so misconfiguration fails at startup.

## Common mistakes

- **Putting `process.env.X` in the decorator or static method** at import time ([timing trap](../03-configuration/01-configuration-basics.md)). Use the async variant with `ConfigService`.
- **Forgetting `module: StorageModule`** in the returned object.
- **Forgetting `exports`** in the returned object: static `@Module` exports are merged, but any providers you add dynamically need exporting explicitly if consumers should see them.
- **Using `forRoot` for per-importer config** (or `register` for global setup), which breaks conventions and surprises users.
- **Omitting `imports` in `forRootAsync`**, so `inject: [ConfigService]` can't resolve (unless `ConfigModule` is global).
- **Re-implementing async variants by hand** instead of using `ConfigurableModuleBuilder`.

## Debugging

- `Nest can't resolve dependencies of the STORAGE_OPTIONS...` in `forRootAsync`: add the module that provides the injected services to the factory's `imports`.
- Options are `undefined` in the service: wrong injection token, or the module was imported without calling `forRoot()` (you imported the class, not the configured module).
- Two different configs in one app behaving oddly? Check whether you intended a singleton (`forRoot` once) or per-importer instances (`register`).

## Quick Summary

- Dynamic modules are configured by the importer through a static method returning `DynamicModule`.
- Conventions: `register` (per importer), `forRoot` (once, app-wide), `forFeature` (feature slices), each with an `*Async` form.
- Async variants take `imports`, `inject`, `useFactory` so options can depend on other providers.
- Use `ConfigurableModuleBuilder` to generate the boilerplate, tokens, and `isGlobal` support.
- Keep options behind a token; never read `process.env` at import time.

## Next

[Circular dependencies →](./04-circular-dependencies.md)
