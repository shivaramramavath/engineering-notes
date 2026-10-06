# Custom Validators

The built-in class-validator decorators cover common cases. When you need a rule they don't provide (a domain format, a cross-field check, a database lookup), you write your own. There are two levels of effort, depending on whether you need dependency injection.

Prerequisites: [class-validator](./03-class-validator.md), [ValidationPipe](./02-validation-pipe.md).

## Option 1: a simple decorator (no DI)

Use `registerDecorator` for stateless rules.

```ts
// is-slug.decorator.ts
import { registerDecorator, ValidationOptions } from 'class-validator';

export function IsSlug(validationOptions?: ValidationOptions) {
  return (object: object, propertyName: string) => {
    registerDecorator({
      name: 'isSlug',
      target: object.constructor,
      propertyName,
      options: validationOptions,
      validator: {
        validate(value: unknown) {
          return typeof value === 'string' && /^[a-z0-9]+(?:-[a-z0-9]+)*$/.test(value);
        },
        defaultMessage() {
          return '$property must be a lowercase slug (letters, digits, hyphens)';
        },
      },
    });
  };
}
```

```ts
export class CreatePostDto {
  @IsSlug()
  slug: string;
}
```

Notes:

- `validate(value, args)` returns `boolean` or `Promise<boolean>`.
- `defaultMessage(args)` can use `$property`, `$value`, `$constraint1`; or read `args.constraints`.
- Accept `ValidationOptions` and pass it through so callers can use `{ each: true }`, `{ message }`, `{ groups }`.
- Guard on type first. Validators receive whatever the client sent.

### With parameters

```ts
export function MaxWords(max: number, validationOptions?: ValidationOptions) {
  return (object: object, propertyName: string) => {
    registerDecorator({
      name: 'maxWords',
      target: object.constructor,
      propertyName,
      constraints: [max],
      options: validationOptions,
      validator: {
        validate(value: unknown, args: ValidationArguments) {
          const [limit] = args.constraints;
          return typeof value === 'string' && value.trim().split(/\s+/).length <= limit;
        },
        defaultMessage: (args) => `$property must have at most ${args.constraints[0]} words`,
      },
    });
  };
}
```

## Cross-field validation

The validator gets the whole object as `args.object`:

```ts
export function Match(property: string, validationOptions?: ValidationOptions) {
  return (object: object, propertyName: string) => {
    registerDecorator({
      name: 'match',
      target: object.constructor,
      propertyName,
      constraints: [property],
      options: validationOptions,
      validator: {
        validate(value: unknown, args: ValidationArguments) {
          const [related] = args.constraints;
          return value === (args.object as Record<string, unknown>)[related];
        },
        defaultMessage: (args) => `$property must match ${args.constraints[0]}`,
      },
    });
  };
}

export class RegisterDto {
  @IsString() @MinLength(8) password: string;

  @Match('password')
  passwordConfirm: string;
}
```

Remember `Match` must run on a property that has at least this decorator (so `whitelist` keeps it).

## Option 2: a constraint class with DI (async, database lookups)

When the rule needs a service (for example "email must not already exist"), implement `ValidatorConstraintInterface` as an injectable class.

```ts
// is-email-unique.validator.ts
import { Injectable } from '@nestjs/common';
import {
  registerDecorator, ValidationOptions, ValidatorConstraint, ValidatorConstraintInterface,
} from 'class-validator';
import { UsersService } from '../users/users.service';

@ValidatorConstraint({ name: 'isEmailUnique', async: true })
@Injectable()
export class IsEmailUniqueConstraint implements ValidatorConstraintInterface {
  constructor(private readonly users: UsersService) {}

  async validate(email: string) {
    if (typeof email !== 'string') return false;
    return !(await this.users.existsByEmail(email));
  }

  defaultMessage() {
    return 'Email $value is already registered';
  }
}

export function IsEmailUnique(validationOptions?: ValidationOptions) {
  return (object: object, propertyName: string) => {
    registerDecorator({
      target: object.constructor,
      propertyName,
      options: validationOptions,
      validator: IsEmailUniqueConstraint,
    });
  };
}
```

Two wiring steps that are easy to forget:

**1. Register the constraint as a provider** in a module that can see `UsersService`:

```ts
@Module({
  imports: [UsersModule],              // exports UsersService
  providers: [IsEmailUniqueConstraint],
})
export class ValidatorsModule {}
```

**2. Tell class-validator to use Nest's container** in `main.ts`:

```ts
import { useContainer } from 'class-validator';

const app = await NestFactory.create(AppModule);
useContainer(app.select(AppModule), { fallbackOnErrors: true });
```

Without `useContainer`, class-validator instantiates the constraint itself with `new`, so `this.users` is `undefined` and validation throws at runtime. `fallbackOnErrors: true` lets plain (non-DI) constraints keep working.

Usage:

```ts
export class RegisterDto {
  @IsEmail()
  @IsEmailUnique()
  email: string;
}
```

Async validators run alongside sync ones, so a DB query may execute even when `@IsEmail()` has already failed. Order decorators sensibly and keep queries cheap.

## Should uniqueness live in a validator?

Usually **not as the only line of defense.**

- It's a **check-then-act race**: two requests can both pass the check, then both insert.
- The real guarantee is a **unique constraint** in the database. Catch the violation and map it to `409 Conflict` ([exception filters](../01-request-pipeline/08-exception-filters.md), [database errors](../../04-intermediate/02-database-foundations/07-database-errors.md)).
- A validator-level check is a UX nicety (early, field-level error). Keep it if you want that, but don't skip the constraint.

Also: validators that hit the database need request context (tenant, soft-deleted rows) that DTO-level code rarely has cleanly. Business rules that need that context belong in the service.

## Alternative: a pipe

For rules that need the full request or aren't tied to a property, a [custom pipe](../01-request-pipeline/03-pipes.md) may be simpler than a constraint class. Use validators for reusable **property-level** rules, and pipes/services for the rest.

## Testing

Validators are testable without Nest for the sync case:

```ts
const dto = plainToInstance(CreatePostDto, { slug: 'Not A Slug' });
const errors = await validate(dto);
expect(errors[0].constraints).toHaveProperty('isSlug');
```

For DI-based constraints, build a testing module (or instantiate the constraint with a mocked service and call `validate()` directly). See [mocking](../../04-intermediate/01-testing/03-mocking.md).

## Common mistakes

- **Forgetting `useContainer`**, so injected services are `undefined`.
- **Forgetting to list the constraint in `providers`.**
- **Not accepting/passing `ValidationOptions`**, so `{ each: true }` and `{ message }` are ignored.
- **No type guard** before string operations, which throws and returns a 500.
- **Treating the validator as the uniqueness guarantee** instead of a DB constraint.
- **Expensive async validators** that run on every request, even when cheaper checks fail first.
- **Leaking sensitive data** via `$value` in messages (for example, echoing a password).
- **Reaching for a custom validator** when `@Matches`, `@IsIn`, or `@ValidateIf` already do the job.

## Debugging

- `Cannot read properties of undefined` inside a constraint? DI isn't wired: check `useContainer` and `providers`.
- Validator never runs? Confirm `registerDecorator`'s `target`/`propertyName` and that the pipe is registered.
- Wrong message? `defaultMessage` placeholders only work with `$property`, `$value`, `$constraintN`; use a function for anything else.

## Quick Summary

- Stateless rule → `registerDecorator` with a `validate` function.
- Needs a service → `@ValidatorConstraint` + `@Injectable()` class, register it as a provider, call `useContainer(...)` in `main.ts`.
- Cross-field checks read `args.object`.
- Always pass `ValidationOptions` through and type-guard the value.
- Use DB unique constraints for real uniqueness; validators are only the friendly early check.

## Next

[Serialization →](./07-serialization.md)
