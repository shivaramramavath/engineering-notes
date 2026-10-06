# Validation and Serialization

Two sides of the same boundary: **validation** protects your app from bad input; **serialization** protects your clients (and your secrets) from bad output.

```text
          INPUT                                          OUTPUT
raw JSON / query / params                        entity / domain object
        │                                                  │
        ▼                                                  ▼
 ValidationPipe                               ClassSerializerInterceptor
  ├─ class-transformer: plain → DTO instance     └─ class-transformer: instance → plain
  └─ class-validator: check decorators                 (@Exclude / @Expose / @Transform)
        │                                                  │
        ▼                                                  ▼
   handler(dto)  ───────────────►  service  ───────────►  JSON response
```

Both rely on the same two libraries (`class-validator`, `class-transformer`) and on **classes with decorators**, so the DTO note comes first.

> Applies to NestJS 10/11 with `class-validator` and `class-transformer`. Schema-library alternatives (Zod, Joi) are in note 08.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [DTO](./01-dto.md) | What is a DTO? Why classes? Create/update/response DTOs, mapped types |
| 02 | [ValidationPipe](./02-validation-pipe.md) | Setup, options (`whitelist`, `transform`), error shape |
| 03 | [class-validator](./03-class-validator.md) | The decorator catalog and its gotchas |
| 04 | [class-transformer](./04-class-transformer.md) | `@Type`, `@Transform`, `@Expose`, `@Exclude`, implicit conversion |
| 05 | [Nested and conditional validation](./05-nested-and-conditional-validation.md) | Nested objects/arrays, `@ValidateIf`, polymorphic payloads |
| 06 | [Custom validators](./06-custom-validators.md) | Custom decorators, async validators, DI |
| 07 | [Serialization](./07-serialization.md) | Hiding fields, `ClassSerializerInterceptor`, response DTOs |
| 08 | [Schema validation alternatives](./08-schema-validation-alternatives.md) | Zod/Joi pipes and when to choose them |

## Prerequisites

- [Pipes](../01-request-pipeline/03-pipes.md) and [interceptors](../01-request-pipeline/05-interceptors.md)
- [TypeScript decorators](../../00-prerequisites/02-typescript-decorators.md)
- [Request data](../../02-fundamentals/07-request-data.md)

## Related

- [Exception filters](../01-request-pipeline/08-exception-filters.md) for reshaping validation errors
- [Swagger DTO documentation](../../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md)
- [Response and error format](../../04-intermediate/08-api-design/05-response-and-error-format.md)
- [Configuration validation](../03-configuration/02-configuration-validation.md)
