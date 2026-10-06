# Configuration Validation

A missing `DATABASE_URL` should crash the app **at startup**, with a clear message, not at 3 a.m. when the first request touches the database. Configuration validation checks all environment variables when `ConfigModule` initializes and refuses to boot if anything is wrong.

It also gives you a place to **convert types** (string → number/boolean) and **apply defaults** once, instead of at every call site.

Prerequisite: [Configuration basics](./01-configuration-basics.md).

## Option 1: Joi schema (`validationSchema`)

The most common approach, and the one the official docs lead with.

```bash
npm i joi
```

```ts
import * as Joi from 'joi';

ConfigModule.forRoot({
  isGlobal: true,
  validationSchema: Joi.object({
    NODE_ENV: Joi.string().valid('development', 'test', 'production').default('development'),
    PORT: Joi.number().port().default(3000),
    DATABASE_URL: Joi.string().uri().required(),
    JWT_SECRET: Joi.string().min(32).required(),
    ENABLE_CACHE: Joi.boolean().default(false),
  }),
  validationOptions: {
    allowUnknown: true,   // the environment has many unrelated variables (PATH, HOME, ...)
    abortEarly: false,    // report every problem, not just the first
  },
});
```

On failure the app throws during bootstrap, listing all invalid keys:

```text
Error: Config validation error: "DATABASE_URL" is required. "JWT_SECRET" is required
```

What you get:

- **Required checks** (`.required()`), **allowed values** (`.valid(...)`), **ranges and formats**.
- **Conversion**: Joi converts `'3000'` to a number and `'true'` to a boolean by default.
- **Defaults** (`.default(...)`).

Read values through `ConfigService` so you receive the converted values rather than the raw strings in `process.env`:

```ts
config.get<number>('PORT');           // 3000 (number)
config.get<boolean>('ENABLE_CACHE');  // false (boolean)
```

`allowUnknown` defaults to `true` and `abortEarly` to `false` in `@nestjs/config`; stating them makes intent explicit. Don't flip `allowUnknown` to `false`, because your OS and platform inject variables you didn't declare.

## Option 2: a `validate` function with class-validator

Reuses tools from [request validation](../02-validation-and-serialization/02-validation-pipe.md), with typed config as a bonus.

```ts
// env.validation.ts
import { plainToInstance } from 'class-transformer';
import { IsEnum, IsInt, IsNotEmpty, IsString, Max, Min, validateSync } from 'class-validator';

enum Environment { Development = 'development', Test = 'test', Production = 'production' }

export class EnvironmentVariables {
  @IsEnum(Environment)
  NODE_ENV: Environment = Environment.Development;

  @IsInt() @Min(1) @Max(65535)
  PORT: number = 3000;

  @IsString() @IsNotEmpty()
  DATABASE_URL: string;

  @IsString() @IsNotEmpty()
  JWT_SECRET: string;
}

export function validate(config: Record<string, unknown>) {
  const validated = plainToInstance(EnvironmentVariables, config, {
    enableImplicitConversion: true,
  });
  const errors = validateSync(validated, { skipMissingProperties: false });
  if (errors.length > 0) {
    throw new Error(errors.map((e) => Object.values(e.constraints ?? {}).join(', ')).join('; '));
  }
  return validated;
}
```

```ts
ConfigModule.forRoot({ isGlobal: true, validate });
```

Notes:

- `validate` receives the raw merged config (`.env` plus `process.env`) and must **return** the config to use (or throw).
- `enableImplicitConversion` converts by the declared TS type. Careful with booleans: `Boolean('false')` is `true`. Use an explicit `@Transform` for boolean flags ([class-transformer](../02-validation-and-serialization/04-class-transformer.md)).
- Without `skipMissingProperties: false` semantics being explicit, a missing key can slip past some validators; keep a type validator (`@IsString`, `@IsInt`) on every required key.
- Defaults live in the class (`PORT: number = 3000`) and apply because `plainToInstance` constructs the class first.

## Option 3: Zod

Any function that throws on invalid input and returns the parsed config works:

```ts
import { z } from 'zod';

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'test', 'production']).default('development'),
  PORT: z.coerce.number().int().min(1).max(65535).default(3000),
  DATABASE_URL: z.string().url(),
  JWT_SECRET: z.string().min(32),
  ENABLE_CACHE: z.enum(['true', 'false']).default('false').transform((v) => v === 'true'),
});

export type Env = z.infer<typeof envSchema>;

ConfigModule.forRoot({
  isGlobal: true,
  validate: (config) => envSchema.parse(config),
});
```

`envSchema.parse` throws a `ZodError` listing the invalid keys. Use `z.coerce.number()` for numeric env vars and an explicit mapping for booleans (`z.coerce.boolean()` has the same `'false'` → `true` trap). Zod strips unknown keys by default, which is fine here because only the declared keys are validated. See [schema alternatives](../02-validation-and-serialization/08-schema-validation-alternatives.md) for the Zod vs Joi vs class-validator trade-offs.

## Choosing

| | Joi | class-validator | Zod |
|-|-----|-----------------|-----|
| Extra dependency | `joi` | Already present in most Nest apps | `zod` |
| Type conversion | Built in | `enableImplicitConversion` / `@Transform` | `z.coerce` / `.transform` |
| Derived TS type | No (manual) | Class *is* the type | `z.infer` |
| Wiring | `validationSchema` | `validate` | `validate` |

Pick whatever the rest of the project uses. The important thing is that **some** validation exists.

## Typed access to validated config

```ts
// config.service typed with your env type
constructor(private readonly config: ConfigService<Env, true>) {}

const port = this.config.get('PORT', { infer: true });   // number, inferred from Env
```

With `ConfigService<Env, true>`, the second generic (`WasValidated = true`) removes `| undefined` from inferred results, since validation guarantees the keys exist. Only use `true` if your validation really does guarantee them. For richer structures see [custom configuration](./03-custom-configuration.md).

## What to validate

- **Everything the app can't start without**: database URL, secrets, ports, third-party credentials.
- **Formats and bounds**: URLs, ports, minimum secret length, allowed `NODE_ENV` values.
- **Conditional requirements**: for example, `S3_BUCKET` is required only when `STORAGE_DRIVER=s3` (Joi `.when(...)`, Zod `.superRefine`/discriminated union).
- Don't validate values that are truly optional beyond their format.

## Common mistakes

- **No validation at all**, so the app boots "healthy" and fails on first use.
- **`allowUnknown: false`** and a startup failure caused by `PATH` or platform variables.
- **Reading `process.env.X` directly** after validating, so you get the unconverted string instead of the validated value.
- **Boolean traps**: `Boolean('false')`, `z.coerce.boolean()`, `Joi` accepting unexpected strings. Test the `'false'`/`'0'` cases.
- **Defaults hiding mistakes**: a default `JWT_SECRET` in code means production can silently run with a known secret. Don't default secrets.
- **Validation errors that print secret values.** Report keys and reasons, not values.
- **Different validation per environment** that lets production-only keys go unchecked in CI. Run the validator in CI with production-shaped dummy values.

## Debugging

- App won't start with a config error? Read the key names in the message; compare with the real environment (`printenv | grep KEY`).
- Value is still a string? You're reading `process.env` instead of `ConfigService`, or your schema has no conversion.
- Works locally, fails in CI/prod? A variable is missing or named differently there (`DATABASE_URL` vs `DB_URL`).
- Validation not running? Confirm `validate`/`validationSchema` is on the **same** `forRoot` call that loads the file and that you only have one `ConfigModule.forRoot` with `isGlobal`.

## Quick Summary

- Validate config at startup so misconfiguration fails fast with a clear message.
- Joi via `validationSchema`, or any parse function via `validate` (class-validator, Zod).
- Validation also converts types and applies defaults; read through `ConfigService` to get converted values.
- Don't default secrets; keep `allowUnknown` on; beware boolean coercion.
- Use `ConfigService<Env, true>` for inferred types once validation guarantees the keys.

## Next

[Custom configuration →](./03-custom-configuration.md)
