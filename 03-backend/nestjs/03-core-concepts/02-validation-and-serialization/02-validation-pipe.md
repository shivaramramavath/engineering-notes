# ValidationPipe

`ValidationPipe` is the built-in [pipe](../01-request-pipeline/03-pipes.md) that validates incoming data against a [DTO class](./01-dto.md). It converts the plain payload into a DTO instance (class-transformer), runs the decorators (class-validator), and throws `400 Bad Request` if anything fails.

## Setup

```bash
npm i class-validator class-transformer
```

Register it **globally** so every route is covered by default:

```ts
// main.ts
app.useGlobalPipes(
  new ValidationPipe({
    whitelist: true,
    forbidNonWhitelisted: true,
    transform: true,
  }),
);
```

Or, DI-aware and picked up by e2e tests that build `AppModule` directly:

```ts
// app.module.ts
providers: [
  {
    provide: APP_PIPE,
    useValue: new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }),
  },
]
```

`useGlobalPipes` in `main.ts` is **not** applied in tests that bypass `main.ts`, which is a common reason "validation works in dev but not in e2e". Prefer `APP_PIPE`, or configure the test app identically.

## What happens to a request

```text
JSON body ──► plainToInstance(CreateUserDto, body)   (class-transformer: @Type, @Transform, defaults)
          ──► validate(instance)                      (class-validator: decorators)
          ──► errors? ──► BadRequestException (400)
          ──► ok      ──► handler receives the instance (if transform: true) or the original payload (if false)
```

## The options that matter

| Option | Effect |
|--------|--------|
| `whitelist: true` | Strips properties that have **no validation decorator** |
| `forbidNonWhitelisted: true` | With `whitelist`, throws instead of silently stripping |
| `transform: true` | Handler receives a **DTO instance** and primitives converted to declared types |
| `transformOptions` | Passed to class-transformer, e.g. `{ enableImplicitConversion: true }` |
| `stopAtFirstError: true` | Report only the first failing constraint per property |
| `disableErrorMessages: true` | Return a bare 400 without details (production hardening, hurts debuggability) |
| `errorHttpStatusCode` | Use e.g. 422 instead of 400 |
| `exceptionFactory` | Build your own exception from the `ValidationError[]` |
| `groups` / `always` | Apply [validation groups](./03-class-validator.md) |
| `validateCustomDecorators` | Also validate values from [custom param decorators](../01-request-pipeline/10-custom-decorators.md) |
| `forbidUnknownValues` | Reject unknown objects (default `true` in current class-validator) |
| `validationError: { target, value }` | Whether errors include the target object / offending value (both default to exposing them; set `false` to avoid echoing input back) |

### `whitelist` and `forbidNonWhitelisted`

```ts
class CreateUserDto { @IsEmail() email: string }

// POST { "email": "a@b.com", "role": "admin" }
// whitelist:true                      → handler gets { email } (role silently removed)
// whitelist + forbidNonWhitelisted    → 400 "property role should not exist"
```

This is your main defense against mass assignment. **Important:** whitelisting is based on *decorators*. A DTO property with no class-validator decorator is treated as "not whitelisted" and stripped. Add `@Allow()` for a property that needs no validation but must be kept.

### `transform: true`

Without it, validation happens on a transformed copy, but your handler still receives the **original plain object**. TypeScript says it's a `CreateUserDto`; at runtime it's not (`instanceof` fails, `@Transform` results are lost).

With it:

- `@Body()` becomes a real DTO instance (defaults, `@Transform` results, nested `@Type` instances).
- Primitive params are converted to the declared type:

```ts
@Get(':id')
findOne(@Param('id') id: number) {
  typeof id; // 'number' with transform:true, otherwise 'string'
}
```

This conversion is limited to simple types (`number`, `string`, `boolean`); a failed conversion (`"abc"` → `NaN`) isn't rejected by itself. Use `ParseIntPipe` on the parameter when you need guaranteed integer validation.

### `enableImplicitConversion`

```ts
new ValidationPipe({ transform: true, transformOptions: { enableImplicitConversion: true } })
```

Converts query/body values based on the **TypeScript type** without `@Type()` on each field. Convenient for query DTOs, but be careful: booleans use JavaScript truthiness (`"false"` → `true`). For booleans use an explicit `@Transform` ([class-transformer](./04-class-transformer.md)).

## Error response

```json
{
  "message": [
    "email must be an email",
    "password must be longer than or equal to 8 characters"
  ],
  "error": "Bad Request",
  "statusCode": 400
}
```

`message` is an **array** for validation errors, a string for other exceptions; clients and filters must handle both.

### Custom error format

```ts
new ValidationPipe({
  exceptionFactory: (errors: ValidationError[]) =>
    new UnprocessableEntityException({
      code: 'VALIDATION_FAILED',
      errors: errors.map((e) => ({
        field: e.property,
        messages: Object.values(e.constraints ?? {}),
      })),
    }),
});
```

Nested errors live in `error.children`; flatten recursively if you use [nested validation](./05-nested-and-conditional-validation.md).

## Using it at narrower scopes

```ts
@Post()
@UsePipes(new ValidationPipe({ groups: ['create'] }))
create(@Body() dto: CreateUserDto) {}

@Post()
create(@Body(new ValidationPipe({ whitelist: true })) dto: CreateUserDto) {}
```

The most specific pipe is applied alongside global ones (it doesn't replace them), so double validation is possible. Prefer one global configuration with narrow overrides only where genuinely needed.

## Validating arrays and primitives

`ValidationPipe` doesn't validate a top-level array body against `Dto[]` by itself. Use `ParseArrayPipe`:

```ts
@Post('bulk')
createMany(@Body(new ParseArrayPipe({ items: CreateUserDto })) dtos: CreateUserDto[]) {}
```

For query arrays like `?ids=1,2,3`: `new ParseArrayPipe({ items: Number, separator: ',' })`.

## Common mistakes

- **No global pipe at all**: DTO decorators are silently ignored. The most common validation bug.
- **DTO is an `interface`**, or imported with `import type`.
- **`whitelist: true` strips everything** because the DTO fields have no decorators.
- **Expecting `transform: false` to give DTO instances.**
- **`enableImplicitConversion` + boolean query params** (`?active=false` becomes `true`).
- **Validating in e2e tests without registering the same pipe.**
- **Missing `@Type(() => Nested)`/`@ValidateNested()`**, so nested objects pass unvalidated ([nested validation](./05-nested-and-conditional-validation.md)).
- **Leaking input via errors**: by default errors can contain the submitted value. Disable with `validationError: { value: false }` if inputs may be sensitive (passwords).
- **Hand-rolled validation in services** duplicating what the pipe already guarantees.

## Debugging

- Nothing validated? Confirm the pipe is registered (log in a test), the param type is a class, and `emitDecoratorMetadata` is on.
- Everything rejected with "property X should not exist"? Missing decorators on legitimate fields.
- Handler gets strings instead of numbers? Missing `transform: true` / `@Type`.
- `instanceof CreateDto` is false? `transform` is off.
- Inspect exactly what failed by logging the raw `ValidationError[]` inside an `exceptionFactory`.

## Quick Summary

- Register one global `ValidationPipe` (prefer `APP_PIPE`) with `whitelist`, `forbidNonWhitelisted`, and `transform`.
- It transforms plain → instance, then validates; failures give 400 with `message: string[]`.
- `whitelist` depends on decorators; `transform` is required for real DTO instances and primitive conversion.
- Use `exceptionFactory` to standardize error output; use `ParseArrayPipe` for array bodies.

## Next

[class-validator →](./03-class-validator.md)
