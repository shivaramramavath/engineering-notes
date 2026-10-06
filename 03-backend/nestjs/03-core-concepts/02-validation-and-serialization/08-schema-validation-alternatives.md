# Schema Validation Alternatives

`class-validator` + `class-transformer` is Nest's default, decorator-based approach. It isn't the only one. Schema libraries like **Zod** and **Joi** describe the shape as a value (a schema object) instead of decorators on a class, and plug into Nest through a custom [pipe](../01-request-pipeline/03-pipes.md).

This note shows the pattern, the trade-offs, and how to choose.

## Why consider an alternative

Decorator-based validation has real friction:

- DTO classes + decorators + `@Type` for every nested/array property.
- Type and rules can drift (the TS type says `number`, the decorators say otherwise).
- Unions and discriminated shapes are awkward ([nested validation](./05-nested-and-conditional-validation.md)).
- Transformation is split across two libraries with subtle ordering rules.

Schema libraries make the schema the single source of truth, and (with Zod) the TypeScript type is *derived* from it.

## The general pattern: a schema pipe

Any library with a "parse and report errors" function works the same way:

```text
raw value ──► schema.parse(value) ──► typed, validated (and transformed) value
                    └─ failure ──► BadRequestException
```

## Zod

```bash
npm i zod
```

### A reusable pipe

```ts
// zod-validation.pipe.ts
import { BadRequestException, PipeTransform } from '@nestjs/common';
import { ZodType } from 'zod';

export class ZodValidationPipe implements PipeTransform {
  constructor(private readonly schema: ZodType) {}

  transform(value: unknown) {
    const result = this.schema.safeParse(value);
    if (!result.success) {
      throw new BadRequestException({
        message: 'Validation failed',
        errors: result.error.issues.map((i) => ({ path: i.path.join('.'), message: i.message })),
      });
    }
    return result.data;   // the parsed value: coerced/defaulted/stripped per the schema
  }
}
```

`result.error.issues` is the stable, low-level list of problems. Zod's higher-level formatting helpers changed between major versions, so check the docs for your Zod version if you use them.

### Schema and derived type

```ts
// create-cat.schema.ts
import { z } from 'zod';

export const createCatSchema = z.object({
  name: z.string().min(1).max(50),
  age: z.number().int().min(0),
  tags: z.array(z.string()).max(10).default([]),
});

export type CreateCatDto = z.infer<typeof createCatSchema>;
```

### Using it

```ts
@Post()
@UsePipes(new ZodValidationPipe(createCatSchema))
create(@Body() dto: CreateCatDto) {}

// or per parameter
create(@Body(new ZodValidationPipe(createCatSchema)) dto: CreateCatDto) {}
```

Query/param coercion is explicit in the schema:

```ts
const listQuery = z.object({
  page: z.coerce.number().int().min(1).default(1),
  active: z.enum(['true', 'false']).transform((v) => v === 'true').optional(),
});
```

Unlike `class-validator`, Zod objects **strip unknown keys by default** (unless you use `.strict()` to reject them or `.passthrough()` to keep them), so the mass-assignment protection of `whitelist` is built in. Choose `.strict()` if you want unknown keys to be a 400.

### Things to know

- A schema-based DTO is a **type**, not a class: no `instanceof`, no decorators. Nest's `ValidationPipe` can't see it, so you must apply your Zod pipe on every route or register it in a way that knows the schema (global registration needs a schema lookup, which is why a per-route pipe is the norm).
- **Swagger:** `@nestjs/swagger` reads class decorators/CLI plugin metadata, not Zod schemas. You either maintain Swagger annotations separately, or use a community package that bridges Zod and OpenAPI (for example `nestjs-zod`; evaluate its maintenance status and compatibility with your Zod and Nest versions before adopting).
- Transforms in the schema (`.transform`, `.default`, `.coerce`) run as part of parsing, so the handler receives the converted value.

## Joi

Joi is older and widely used in the Nest ecosystem, notably for **environment variable validation** in `@nestjs/config` ([configuration validation](../03-configuration/02-configuration-validation.md)).

```bash
npm i joi
```

```ts
import * as Joi from 'joi';

export class JoiValidationPipe implements PipeTransform {
  constructor(private readonly schema: Joi.ObjectSchema) {}

  transform(value: unknown) {
    const { error, value: parsed } = this.schema.validate(value, { abortEarly: false, stripUnknown: true });
    if (error) {
      throw new BadRequestException({
        message: 'Validation failed',
        errors: error.details.map((d) => ({ path: d.path.join('.'), message: d.message })),
      });
    }
    return parsed;
  }
}

const createCatSchema = Joi.object({
  name: Joi.string().min(1).max(50).required(),
  age: Joi.number().integer().min(0).required(),
});
```

Joi doesn't derive a TypeScript type from the schema, so you maintain the interface separately. That's the main reason Zod is preferred for request bodies in new TypeScript code, while Joi remains common for config.

`abortEarly: false` returns all errors rather than only the first; `stripUnknown: true` mirrors `whitelist`.

## Comparison

| | class-validator + class-transformer | Zod | Joi |
|-|-------------------------------------|-----|-----|
| Style | Decorators on classes | Schema value | Schema value |
| Type derivation | Class *is* the type (manual sync of rules) | `z.infer` derives type | Manual |
| Nest integration | Built-in `ValidationPipe`, global | Custom pipe, per route | Custom pipe; common for config |
| Swagger | First-class (`@ApiProperty` / CLI plugin) | Needs a bridge or manual docs | Needs manual docs |
| Unions / discriminated | Awkward | Natural (`z.discriminatedUnion`) | Supported (`alternatives`) |
| Transformation | Separate library, ordering subtleties | Built in (`transform`, `coerce`) | Built in (conversion on by default) |
| Unknown keys | Needs `whitelist` | Stripped by default | Error by default; `stripUnknown` to strip |
| Ecosystem fit | Idiomatic Nest | Popular in modern TS | Mature, widely used for env/config |

## Choosing

- **Default to class-validator** for typical Nest REST APIs, especially with Swagger, because the framework's docs, tooling, and community examples assume it.
- **Choose Zod** if you share schemas with a frontend or other services, have complex unions, or want types derived from validation, and you're willing to handle OpenAPI separately.
- **Use Joi** for configuration validation, or when you already have Joi schemas.
- **Don't mix styles in one codebase** without a clear boundary (for example: Zod for webhooks, class-validator for the public API). Mixed approaches confuse contributors and error formats diverge.

Whatever you choose, keep the **error response shape consistent**. Normalize in an [exception filter](../01-request-pipeline/08-exception-filters.md) or the pipes' `exceptionFactory`.

## Common mistakes

- **Leaving a route unvalidated** because a per-route Zod/Joi pipe was forgotten (there is no global safety net like `ValidationPipe`).
- **Using Zod types as if they were classes** (`instanceof`, decorators).
- **Passing raw `error.message` or the library's full error object** to the client; shape it deliberately.
- **Not bounding input** (`.max()` on strings/arrays) just because the schema is concise.
- **Assuming Swagger picks up your schema automatically.**
- **Copying snippets across Zod major versions** without checking changed APIs.
- **Using different error formats** for different validation paths.

## Debugging

- Log the library's raw error (`result.error.issues` for Zod, `error.details` for Joi) before reshaping it.
- Handler receives unexpected types? Make sure you return the **parsed** result from the pipe, not the original `value`.
- Unknown keys appearing or disappearing? Check strip/strict/passthrough (Zod) or `stripUnknown` (Joi).

## Quick Summary

- Schema libraries plug in via a custom pipe: parse, throw `BadRequestException` on failure, return the parsed value.
- Zod derives TS types from schemas and handles unions well, but needs per-route pipes and separate OpenAPI handling.
- Joi is mature and the usual choice for env/config validation.
- class-validator stays the idiomatic default, especially with Swagger.
- Pick one approach per boundary and keep error responses consistent.

## Next

Section complete. Continue with [Configuration](../03-configuration/README.md), where schema validation reappears for environment variables.

← Back to [Validation and serialization overview](./README.md)
