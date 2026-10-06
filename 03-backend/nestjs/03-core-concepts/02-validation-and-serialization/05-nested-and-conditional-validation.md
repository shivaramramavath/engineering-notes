# Nested and Conditional Validation

Real payloads aren't flat. They contain nested objects, arrays of objects, fields that only matter in certain cases, and sometimes polymorphic shapes. This note covers how to validate all of those with class-validator and class-transformer.

Prerequisites: [class-validator](./03-class-validator.md), [class-transformer](./04-class-transformer.md).

## Nested objects

Nested validation needs **two** decorators, and both are required:

```ts
import { Type } from 'class-transformer';
import { IsString, IsDefined, ValidateNested } from 'class-validator';

export class AddressDto {
  @IsString() street: string;
  @IsString() city: string;
}

export class CreateUserDto {
  @IsString() name: string;

  @IsDefined()
  @ValidateNested()
  @Type(() => AddressDto)
  address: AddressDto;
}
```

- `@Type(() => AddressDto)` makes class-transformer **instantiate** `AddressDto`. Without it, `address` stays a plain object with no decorators attached.
- `@ValidateNested()` tells class-validator to descend into it.
- `@IsDefined()` makes the nested object **required**. Don't assume `@ValidateNested()` alone rejects a missing value; be explicit.

Forgetting `@Type` is the classic bug: the request passes validation with garbage nested data because nothing is attached to validate.

## Arrays of objects

```ts
export class CreateOrderDto {
  @IsArray()
  @ArrayMinSize(1)
  @ArrayMaxSize(100)
  @ValidateNested({ each: true })
  @Type(() => OrderItemDto)
  items: OrderItemDto[];
}

export class OrderItemDto {
  @IsUUID() productId: string;
  @IsInt() @Min(1) quantity: number;
}
```

`@Type` receives the **element** class. Always bound array length (`@ArrayMaxSize`) so one request can't submit a million items.

## Error shape for nested failures

Nested errors are not flattened in the default message list. `ValidationPipe` flattens them into messages like `address.city must be a string`, but if you build errors yourself via `exceptionFactory`, nested errors live in `ValidationError.children`:

```ts
// ValidationError for a nested failure
{
  property: 'address',
  children: [{ property: 'city', constraints: { isString: 'city must be a string' } }],
}
```

Flatten recursively with a path prefix:

```ts
function flatten(errors: ValidationError[], parent = ''): { field: string; messages: string[] }[] {
  return errors.flatMap((e) => {
    const field = parent ? `${parent}.${e.property}` : e.property;
    const own = e.constraints ? [{ field, messages: Object.values(e.constraints) }] : [];
    return [...own, ...flatten(e.children ?? [], field)];
  });
}
```

Array indexes appear as property names (`items.0.quantity`).

## Whitelisting applies to nested objects

With `whitelist: true`, unknown properties are stripped from nested objects **only if** they are validated via `@ValidateNested`. Nested classes need decorators on every legitimate property, same as top-level DTOs.

## Conditional validation with `@ValidateIf`

`@ValidateIf(fn)` runs the property's **other** validators only when `fn(object, value)` returns true.

```ts
export class PaymentDto {
  @IsIn(['card', 'bank_transfer'])
  method: 'card' | 'bank_transfer';

  @ValidateIf((o) => o.method === 'card')
  @IsCreditCard()
  cardNumber?: string;

  @ValidateIf((o) => o.method === 'bank_transfer')
  @IsString() @Length(15, 34)
  iban?: string;
}
```

When the condition is false, all validators on that property are skipped, including any that would reject unexpected values. Combine with `forbidNonWhitelisted` if a field must not appear at all in other modes (it won't be flagged here, since it has a decorator).

Common uses:

```ts
// required only if another field is present
@ValidateIf((o) => o.endDate !== undefined)
@IsDate() startDate?: Date;

// "optional but not null": reject null explicitly
@ValidateIf((o) => o.nickname !== undefined)
@IsString() nickname?: string;
```

`@IsOptional()` is effectively `@ValidateIf` for "not null/undefined". Use whichever states the intent better. Don't stack both on the same property expecting additive behavior; the first non-matching condition skips validation.

## Cross-field rules

Per-property decorators can't compare two fields on their own. Options:

1. `@ValidateIf` with a dependent check (above).
2. A [custom validator](./06-custom-validators.md) that receives `args.object` (e.g., "password confirmation matches").
3. Check in the service and throw `BadRequestException`/`UnprocessableEntityException` for rules that need domain context.

## Polymorphic payloads (discriminated unions)

When the shape depends on a `type` field, class-transformer supports discriminators:

```ts
export abstract class BaseEventDto {
  @IsString() type: string;
}
export class ClickEventDto extends BaseEventDto {
  @IsString() elementId: string;
}
export class ViewEventDto extends BaseEventDto {
  @IsString() page: string;
}

export class TrackDto {
  @ValidateNested()
  @Type(() => BaseEventDto, {
    keepDiscriminatorProperty: true,
    discriminator: {
      property: 'type',
      subTypes: [
        { value: ClickEventDto, name: 'click' },
        { value: ViewEventDto, name: 'view' },
      ],
    },
  })
  event: ClickEventDto | ViewEventDto;
}
```

`keepDiscriminatorProperty: true` keeps `type` on the instance so it can be validated and used later. An unknown discriminator value won't match any subtype; add `@IsIn(['click', 'view'])` on `type` to reject it explicitly. Test this one thoroughly: it's the least forgiving part of the stack. If unions get complicated, a schema library like Zod models them more naturally ([note 08](./08-schema-validation-alternatives.md)).

## Update DTOs with nested objects

`PartialType` makes **top-level** properties optional but doesn't change the nested DTO's own rules:

```ts
export class UpdateUserDto extends PartialType(CreateUserDto) {}
// address is optional, but IF provided, it must be a complete valid AddressDto
```

For partial nested updates, give the nested property its own partial type: `@Type(() => PartialAddressDto)` where `PartialAddressDto extends PartialType(AddressDto)`.

## Limits worth enforcing

- Array sizes (`@ArrayMaxSize`).
- String lengths on every nested string.
- Nesting depth: class-validator doesn't cap recursion by itself; keep DTO trees shallow. For arbitrary-depth structures, validate in a dedicated step with a depth limit.
- Overall body size (configure at the HTTP layer, e.g. the body parser limit).

## Common mistakes

- **`@ValidateNested()` without `@Type()`**: nested data passes unchecked.
- **`@Type()` without `@ValidateNested()`**: instances created but never validated.
- **Missing `{ each: true }`** on arrays.
- **Nested DTO properties without decorators** under `whitelist`, so valid fields get stripped.
- **Assuming a missing nested object fails.** Add `@IsDefined()`.
- **Stacking `@IsOptional()` and `@ValidateIf()`** and confusing which one skips validation.
- **Unbounded arrays and depth.**

## Debugging

- Test the DTO in isolation: `plainToInstance` then `validate`, and print `JSON.stringify(errors, null, 2)` to see `children`.
- Check `instanceof AddressDto` on the nested value after transformation. If false, `@Type` is missing or wrong.
- Remember array error paths include indexes (`items.2.quantity`).

## Quick Summary

- Nested object = `@ValidateNested()` + `@Type(() => Class)` (+ `@IsDefined()` if required).
- Nested array = add `{ each: true }` and `@IsArray()` plus size limits; `@Type` takes the element class.
- `@ValidateIf` skips a property's validators when its condition is false.
- Cross-field rules need custom validators or service-level checks.
- Polymorphic payloads use `@Type` discriminators, or consider a schema library.

## Next

[Custom validators →](./06-custom-validators.md)
