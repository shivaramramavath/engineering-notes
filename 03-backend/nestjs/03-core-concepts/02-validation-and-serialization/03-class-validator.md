# class-validator

`class-validator` provides the decorators that declare validation rules on DTO properties. Nest's [`ValidationPipe`](./02-validation-pipe.md) runs them for you; this note is about writing correct rules and avoiding the subtle gotchas.

```bash
npm i class-validator
```

## Anatomy of a rule

```ts
import { IsEmail, IsString, Length, IsOptional } from 'class-validator';

export class CreateUserDto {
  @IsEmail({}, { message: 'Please provide a valid email' })
  email: string;

  @IsString()
  @Length(8, 72)
  password: string;

  @IsOptional()
  @IsString()
  nickname?: string;
}
```

- Multiple decorators on one property are **ANDed**: all must pass.
- The last argument of every decorator is `ValidationOptions` (`message`, `each`, `groups`, `always`).
- A property with **no** decorator isn't validated and, with `whitelist: true`, gets stripped.

## Decorator catalog (the useful ones)

Not exhaustive; see the library's README for the full list.

| Category | Decorators |
|----------|-----------|
| Presence | `@IsDefined()`, `@IsOptional()`, `@IsNotEmpty()`, `@IsEmpty()` |
| Types | `@IsString()`, `@IsNumber()`, `@IsInt()`, `@IsBoolean()`, `@IsDate()`, `@IsArray()`, `@IsObject()`, `@IsEnum(E)` |
| Numbers | `@Min(n)`, `@Max(n)`, `@IsPositive()`, `@IsNegative()`, `@IsDivisibleBy(n)` |
| Strings | `@Length(min, max)`, `@MinLength()`, `@MaxLength()`, `@Matches(regex)`, `@IsEmail()`, `@IsUrl()`, `@IsUUID(version?)`, `@IsISO8601()`, `@IsDateString()`, `@IsJSON()`, `@IsStrongPassword()` |
| Arrays | `@ArrayNotEmpty()`, `@ArrayMinSize(n)`, `@ArrayMaxSize(n)`, `@ArrayUnique()`, `@ArrayContains([...])` |
| Values | `@IsIn([...])`, `@IsNotIn([...])`, `@Equals(v)` |
| Dates | `@MinDate(d)`, `@MaxDate(d)` |
| Nested / conditional | `@ValidateNested()`, `@ValidateIf(fn)`: see [note 05](./05-nested-and-conditional-validation.md) |
| Escape hatch | `@Allow()`: keep a property without validating it |

Some validators depend on optional packages or have regional rules (for example `@IsPhoneNumber()` needs a region and uses libphonenumber). Check the docs before relying on them.

## Presence: the part everyone gets wrong

| Decorator | Passes when |
|-----------|-------------|
| `@IsDefined()` | value is not `undefined` and not `null` |
| `@IsNotEmpty()` | value is not `''`, `null`, or `undefined` |
| `@IsOptional()` | **skips all other validators** if value is `undefined` **or `null`** |

Consequences:

```ts
@IsOptional() @IsString() nickname?: string;
// undefined → OK, null → OK (!), "abc" → OK, 42 → fails IsString
```

`@IsOptional()` treats `null` as "absent". If `null` must be rejected, add `@IsDefined()` logic through `@ValidateIf((o) => o.nickname !== undefined)` instead.

Without `@IsOptional()`, a missing property is **not automatically an error**: each validator runs against `undefined`. `@IsString()` fails on `undefined`, but a validator like `@Length(0, 10)` on `undefined` also fails. In practice, put the type validator on every required field and you get "required" behavior for free. Use `@IsDefined()` when no other validator would reject `undefined`.

## Validating arrays: `each`

```ts
@IsArray()
@IsString({ each: true })
@ArrayMaxSize(10)
tags: string[];

@IsEnum(Role, { each: true })
roles: Role[];
```

`{ each: true }` applies the rule to every element. `@IsArray()` ensures it's an array to begin with; without it, a string passes `each` checks in surprising ways.

## Enums

```ts
export enum Role { USER = 'user', ADMIN = 'admin' }

@IsEnum(Role)
role: Role;
```

`@IsEnum` checks against enum **values**. For numeric TypeScript enums the reverse-mapped keys also exist at runtime, which can accept unexpected values; prefer string enums for API contracts.

## Numbers and strings from the query

Query values are strings, so `@IsNumber()` alone fails for `?page=2`. Convert first:

```ts
@Type(() => Number) @IsInt() @Min(1)
page: number;
```

Booleans from `?active=false` need an explicit `@Transform` ([class-transformer](./04-class-transformer.md)).

## Custom messages

```ts
@MinLength(8, { message: 'Password must be at least $constraint1 characters' })
password: string;

@IsString({
  message: (args) => `${args.property} must be text (got ${typeof args.value})`,
})
```

Placeholders: `$property`, `$value`, `$constraint1`, `$constraint2`... Be careful with `$value` on sensitive fields (passwords).

## Validation groups

Reuse one DTO for different situations:

```ts
export class UserDto {
  @IsString({ groups: ['create'] })
  @IsOptional({ groups: ['update'] })
  name?: string;

  @IsEmail({}, { always: true })   // runs regardless of group
  email: string;
}

@UsePipes(new ValidationPipe({ groups: ['create'] }))
```

Groups work, but they get hard to read fast. Separate `Create`/`Update` DTOs with `PartialType` ([DTO](./01-dto.md)) are usually clearer.

## Using class-validator outside Nest

```ts
import { validate, validateOrReject } from 'class-validator';
import { plainToInstance } from 'class-transformer';

const dto = plainToInstance(CreateUserDto, raw);
const errors = await validate(dto, { whitelist: true });  // ValidationError[]
// or: await validateOrReject(dto);
```

Useful for validating queue payloads, CSV rows, or message-broker events where there's no HTTP pipe. Always `plainToInstance` first; validating a plain object does nothing.

`ValidationError` shape:

```ts
{ property: 'email', value: 'x', constraints: { isEmail: 'email must be an email' }, children: [] }
```

## Security and robustness

- **Bound everything user-controlled**: `@MaxLength` on strings, `@ArrayMaxSize` on arrays, `@Max` on page sizes. Unbounded input is an easy DoS and storage vector.
- Use **`whitelist` + `forbidNonWhitelisted`** to stop mass assignment ([ValidationPipe](./02-validation-pipe.md)).
- Don't validate email *deliverability* or uniqueness with regexes; see [custom validators](./06-custom-validators.md).
- Validation protects your app's input contract; it does **not** replace parameterized queries or output encoding ([injection and XSS prevention](../../07-production/01-security/05-injection-and-xss-prevention.md)).

## Common mistakes

- **Relying on `@IsOptional()` to allow only `undefined`**; it also allows `null`.
- **Forgetting `@IsArray()` with `each`.**
- **Using `@IsNumber()` on query params without `@Type`/transform.**
- **Applying `@IsDate()` to a JSON date string.** JSON has no dates; use `@IsDateString()`/`@IsISO8601()` for strings, or `@Type(() => Date)` + `@IsDate()` to convert first.
- **Adding no decorator to a field** and watching it vanish under `whitelist`.
- **Validating a plain object** (outside Nest) without `plainToInstance`.
- **Overusing groups** where separate DTOs would do.

## Debugging

- Log `errors` from `validate()` and read `constraints` and `children`; the failing decorator's name is the key (`isEmail`, `minLength`).
- A rule "doesn't run"? Check for `@IsOptional()` earlier on the property, a mismatched `groups`, or the pipe not being registered.
- Unexpected pass? The value may have been transformed first (`@Transform`, `enableImplicitConversion`).

## Quick Summary

- Decorators on properties are ANDed; options include `message`, `each`, `groups`, `always`.
- `@IsOptional()` skips on both `undefined` and `null`.
- Put a type validator on every required field; use `{ each: true }` plus `@IsArray()` for arrays.
- Query/param strings need conversion before numeric validation.
- Bound strings, arrays, and numbers.

## Next

[class-transformer →](./04-class-transformer.md)
