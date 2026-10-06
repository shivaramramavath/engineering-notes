# class-transformer

`class-transformer` converts between **plain objects** (what JSON gives you) and **class instances** (what decorators attach to). Nest uses it in two places:

- **Input:** `ValidationPipe` calls `plainToInstance` so class-validator has a real DTO to inspect ([ValidationPipe](./02-validation-pipe.md)).
- **Output:** `ClassSerializerInterceptor` calls `instanceToPlain` to apply `@Exclude`/`@Expose` ([Serialization](./07-serialization.md)).

```bash
npm i class-transformer
```

## The two core functions

```ts
import { plainToInstance, instanceToPlain } from 'class-transformer';

const dto = plainToInstance(CreateUserDto, { email: 'a@b.com' }); // plain → class instance
const json = instanceToPlain(dto);                                // class instance → plain
```

Nothing happens to a property unless a decorator (or an option like implicit conversion) tells class-transformer what to do with it. The decorators:

| Decorator | Purpose |
|-----------|---------|
| `@Type(() => X)` | Tell the transformer which class to instantiate for a property |
| `@Transform(fn)` | Custom conversion of a value |
| `@Expose()` | Include a property (or rename/derive it) |
| `@Exclude()` | Omit a property |

## `@Type`: nested objects, arrays, dates, numbers

TypeScript types are erased, so class-transformer cannot know that `address` should be an `Address` instance. `@Type` tells it.

```ts
export class AddressDto {
  @IsString() city: string;
}

export class CreateUserDto {
  @ValidateNested()
  @Type(() => AddressDto)
  address: AddressDto;

  @ValidateNested({ each: true })
  @Type(() => AddressDto)
  previousAddresses: AddressDto[];

  @Type(() => Date)
  @IsDate()
  birthday: Date;

  @Type(() => Number)
  @IsInt()
  age: number;
}
```

Without `@Type(() => AddressDto)`, `address` stays a plain object and `@ValidateNested()` has nothing to validate. This is the #1 nested-validation bug ([note 05](./05-nested-and-conditional-validation.md)).

For arrays, `@Type` takes the **element** class.

## `@Transform`: custom conversion

The callback receives `{ value, key, obj, type }` and returns the new value.

```ts
import { Transform } from 'class-transformer';

export class CreateUserDto {
  @Transform(({ value }) => (typeof value === 'string' ? value.trim() : value))
  @IsString()
  name: string;

  @Transform(({ value }) => (typeof value === 'string' ? value.toLowerCase().trim() : value))
  @IsEmail()
  email: string;
}
```

**Always guard on type.** Clients can send anything. `value.trim()` on a number or `null` throws a `TypeError`, which becomes a 500 instead of a clean 400.

### Booleans from query strings

```ts
@Transform(({ value }) => {
  if (value === 'true' || value === true) return true;
  if (value === 'false' || value === false) return false;
  return value; // leave invalid input untouched so @IsBoolean() rejects it
})
@IsBoolean()
active: boolean;
```

Don't rely on `@Type(() => Boolean)` here. It uses JavaScript's `Boolean()`, and `Boolean("false") === true`.

### Comma-separated list to array

```ts
@Transform(({ value }) => (typeof value === 'string' ? value.split(',').map((s) => s.trim()) : value))
@IsArray()
@IsString({ each: true })
tags: string[];   // ?tags=a,b,c
```

### Direction-specific transforms

By default `@Transform` runs in both directions. Restrict it:

```ts
@Transform(({ value }) => value.toLowerCase(), { toClassOnly: true })   // input only
@Transform(({ value }) => value.toISOString(), { toPlainOnly: true })   // output only
```

## `@Expose` and `@Exclude`

Used mostly on the **output** side:

```ts
export class UserEntity {
  id: string;
  email: string;

  @Exclude()
  passwordHash: string;

  @Expose()
  get displayName() { return this.email.split('@')[0]; }   // include a getter in output
}
```

Full usage, including strategies and groups, is in [Serialization](./07-serialization.md).

## Implicit conversion

```ts
new ValidationPipe({ transform: true, transformOptions: { enableImplicitConversion: true } })
```

Class-transformer reads the TS design type of each property (via `emitDecoratorMetadata`) and converts accordingly, so `page: number` works without `@Type(() => Number)`.

Pitfalls:

- Booleans follow JS truthiness (`"false"` → `true`).
- `Number("")` is `0`, `Number("abc")` is `NaN`. Pair with `@IsInt()`/`@IsNumber()` so garbage is rejected afterwards.
- It doesn't help for nested classes or arrays; those still need `@Type`.
- Behavior is implicit; for public APIs, explicit `@Type`/`@Transform` is easier to reason about.

## Order of operations in Nest

```text
plainToInstance (@Type, @Transform, implicit conversion)  →  validate (class-validator)
```

Transforms run **before** validation, so `@Transform` can normalize input (trim, lowercase) *before* `@IsEmail()` sees it, and validators see the **converted** value, not the raw input.

## Property defaults

Class property initializers apply when the key is missing from the input:

```ts
export class ListQueryDto {
  @Type(() => Number) @IsInt() page: number = 1;
}
```

This works because `plainToInstance` constructs the class first. Remember it only applies to **instances**; with `transform: false` your handler gets the raw object without defaults.

## Common mistakes

- **Missing `@Type` on nested objects and arrays**, so nested validation silently does nothing.
- **`@Type(() => Boolean)` for query flags.** `"false"` becomes `true`.
- **Unguarded `@Transform` callbacks** that throw on unexpected types and return 500.
- **Expecting transforms with `transform: false`.** Validation sees converted values, but the handler gets the original.
- **Using `@Exclude()` on an input DTO and expecting validation to ignore the property.** `@Exclude` affects transformation, not class-validator.
- **Circular `@Type` references** that cause import cycles; use the lazy `() => X` form (already required) and keep DTO files cohesive.
- **Forgetting that `@Transform` runs for `undefined` too** when the key is missing in some configurations. Guard against it.

## Debugging

- Log `plainToInstance(Dto, raw)` in a test and inspect the result with `instanceof` checks on nested properties.
- A value arrives as the wrong type? Check for `@Type`, `transform: true`, and `enableImplicitConversion`.
- A `@Transform` seems ignored? Confirm the pipe has `transform: true` and the decorator is on the property being validated.
- A 500 on malformed input usually means an unguarded `@Transform`.

## Quick Summary

- class-transformer converts plain ↔ class; Nest uses it in `ValidationPipe` (input) and `ClassSerializerInterceptor` (output).
- `@Type` for nested objects, arrays, dates, and numbers; `@Transform` for custom conversion (always type-guard).
- Booleans need an explicit `@Transform`, since `Boolean("false")` is `true`.
- Transforms run before validation; the handler only sees them with `transform: true`.
- `@Expose`/`@Exclude` mainly matter for serialization.

## Next

[Nested and conditional validation →](./05-nested-and-conditional-validation.md)
