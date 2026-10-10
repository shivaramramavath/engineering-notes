# DTO Documentation

DTOs are where most of an OpenAPI document's content comes from: every request body, query object, and response is a schema built from class properties. `@ApiProperty()` (and the CLI plugin) turns those properties into schema fields with types, constraints, descriptions, and examples. Getting DTO documentation right is what makes generated docs and SDKs actually useful.

Prerequisites: [DTO](../../03-core-concepts/02-validation-and-serialization/01-dto.md), [Swagger setup](./01-swagger-setup.md) (the CLI plugin), [documenting endpoints](./02-documenting-endpoints.md).

## Plugin vs explicit decorators

With the **CLI plugin** enabled (`classValidatorShim`, `introspectComments`), plain DTOs document themselves:

```ts
export class CreateUserDto {
  /** The user's email address */
  @IsEmail()
  email: string;

  @IsString() @MinLength(8) @MaxLength(72)
  password: string;

  @IsOptional() @IsInt() @Min(18)
  age?: number;
}
```

Without the plugin (or when you need more), use `@ApiProperty()`:

```ts
export class CreateUserDto {
  @ApiProperty({ description: "The user's email address", example: 'ann@example.com', format: 'email' })
  @IsEmail()
  email: string;

  @ApiProperty({ minLength: 8, maxLength: 72, writeOnly: true })       // never returned in responses
  @IsString() @MinLength(8) @MaxLength(72)
  password: string;

  @ApiPropertyOptional({ minimum: 18, example: 30 })
  @IsOptional() @IsInt() @Min(18)
  age?: number;
}
```

- `@ApiPropertyOptional()` is shorthand for `@ApiProperty({ required: false })`.
- Validation decorators and documentation decorators are **separate**: `@Min(18)` validates at runtime; the schema's `minimum: 18` comes from the plugin's shim or from you repeating it. If you document constraints by hand they can drift from the validators, which is a strong argument for the plugin.
- Keep the **TypeScript type** truthful: `age?: number` (optional) and `@IsOptional()` agree with the schema's `required` list.

## Common `@ApiProperty` options

| Option | Purpose |
|--------|---------|
| `description`, `example`, `default` | Human-facing info (examples power "Try it out" and mocks) |
| `type` | Explicit type (`String`, `Number`, `() => OtherDto`) when it can't be inferred |
| `isArray: true` | Array of `type` |
| `required: false` | Optional (or use `@ApiPropertyOptional`) |
| `nullable: true` | Value may be `null` (distinct from optional) |
| `enum`, `enumName` | Allowed values and a **reusable** schema name |
| `format` | `email`, `uuid`, `date-time`, `uri`, `binary`, ... |
| `minimum`, `maximum`, `minLength`, `maxLength`, `pattern`, `minItems`, `maxItems` | Constraints |
| `readOnly` / `writeOnly` | Only in responses / only in requests (ids, passwords) |
| `deprecated: true` | Mark a field deprecated |

**Optional vs nullable** are different contracts: *optional* means the key may be absent; *nullable* means the key may be present with value `null`. Document each deliberately ([response conventions](../08-api-design/05-response-and-error-format.md)).

## Types that need explicit help

The plugin and TypeScript reflection can't infer everything. Be explicit for:

### Arrays and nested objects

```ts
export class OrderDto {
  @ApiProperty({ type: () => [OrderItemDto] })      // array of nested DTOs
  items: OrderItemDto[];

  @ApiProperty({ type: () => AddressDto })           // nested DTO
  shippingAddress: AddressDto;

  @ApiProperty({ type: [String] })                   // primitive array
  tags: string[];
}
```

Using the **lazy form `() => X`** avoids "cannot access before initialization" problems with circular imports and is required for circular types.

### Enums

```ts
export enum OrderStatus { Pending = 'pending', Paid = 'paid', Shipped = 'shipped' }

export class OrderDto {
  @ApiProperty({ enum: OrderStatus, enumName: 'OrderStatus', example: OrderStatus.Paid })
  status: OrderStatus;
}
```

`enumName` makes Swagger emit **one shared `OrderStatus` schema** referenced everywhere, instead of repeating the inline enum on each property. That matters for SDK generation (a single named enum type) and for readability. Use string enums for stable API contracts.

### Unions, `oneOf`, and polymorphism

```ts
@ApiExtraModels(CardPaymentDto, BankPaymentDto)
export class PayDto {
  @ApiProperty({
    oneOf: [{ $ref: getSchemaPath(CardPaymentDto) }, { $ref: getSchemaPath(BankPaymentDto) }],
    discriminator: { propertyName: 'type' },
  })
  method: CardPaymentDto | BankPaymentDto;
}
```

`@ApiExtraModels` registers classes that aren't referenced directly by a controller, so `$ref`s resolve. Keep the **validation** ([discriminated unions](../../03-core-concepts/02-validation-and-serialization/05-nested-and-conditional-validation.md)) and the **documentation** aligned.

### Maps / free-form objects

```ts
@ApiProperty({ type: 'object', additionalProperties: { type: 'string' } })
metadata: Record<string, string>;
```

### Dates and money

```ts
@ApiProperty({ type: String, format: 'date-time', example: '2025-01-15T10:30:00Z' })
createdAt: Date;                                       // JSON serializes Date as an ISO string

@ApiProperty({ type: Number, description: 'Amount in minor units (cents)', example: 1999 })
amountCents: number;
```

Document the **wire format** (an ISO string), not the in-memory type. For `bigint`/`Decimal` fields (Prisma), document the string form they serialize to ([schema gotchas](../04-prisma/02-schema.md)).

## Reuse: mapped types from `@nestjs/swagger`

Derive DTOs without losing documentation by importing the helpers from `@nestjs/swagger`, **not** `@nestjs/mapped-types`:

```ts
import { PartialType, PickType, OmitType, IntersectionType } from '@nestjs/swagger';

export class UpdateUserDto extends PartialType(CreateUserDto) {}
export class LoginDto extends PickType(CreateUserDto, ['email', 'password'] as const) {}
export class PublicUserDto extends OmitType(UserDto, ['passwordHash'] as const) {}
```

The Swagger versions copy both validation and `@ApiProperty` metadata. Using the plain `mapped-types` versions silently drops the OpenAPI metadata, leaving derived DTOs with empty schemas. Use one package consistently ([DTO note](../../03-core-concepts/02-validation-and-serialization/01-dto.md)).

## Request vs response DTOs

Separate classes for input and output, each documented for its own audience:

```ts
export class UserDto {                                  // response
  @ApiProperty({ readOnly: true, example: 'usr_01HZ' }) id: string;
  @ApiProperty() email: string;
  @ApiProperty({ enum: Role, enumName: 'Role' }) role: Role;
  @ApiProperty({ format: 'date-time' }) createdAt: Date;
}

export class CreateUserDto { /* email, password (writeOnly) */ }   // request
```

Benefits: accurate schemas (no `id`/`passwordHash` in create bodies), `readOnly`/`writeOnly` clarity, and responses you can explicitly map and return so you never leak entity fields ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)). Annotate **response** DTOs even though nothing validates them at runtime: they're documentation (and the contract for SDKs). If you return entities directly, the documented schema may not match what's really serialized.

Hide properties from the docs with `@ApiHideProperty()` (documentation only, not a serialization or security measure).

## Generic and wrapper types (pagination, envelopes)

TypeScript generics are erased, so `Paginated<OrderDto>` can't be inferred. Document it with a small custom decorator ([from the Nest docs](https://docs.nestjs.com/openapi/types-and-parameters#generics-and-interfaces)):

```ts
export class PaginatedDto<T> {
  items: T[];
  meta: { page: number; limit: number; total: number };
}

export const ApiPaginatedResponse = <TModel extends Type<unknown>>(model: TModel) =>
  applyDecorators(
    ApiExtraModels(PaginatedDto, model),
    ApiOkResponse({
      schema: {
        allOf: [
          { $ref: getSchemaPath(PaginatedDto) },
          {
            properties: {
              items: { type: 'array', items: { $ref: getSchemaPath(model) } },
            },
          },
        ],
      },
    }),
  );

@ApiPaginatedResponse(OrderDto)
@Get()
list(@Query() q: ListOrdersQueryDto): Promise<PaginatedDto<OrderDto>> {}
```

The same technique documents `{ data: T }` envelopes added by an [interceptor](../../03-core-concepts/01-request-pipeline/06-interceptor-recipes.md), since the interceptor's wrapping is invisible to Swagger ([responses and examples](./05-responses-and-examples.md)).

## Naming and schema collisions

Swagger names schemas after **class names**. Two classes with the same name in different modules (`CreateDto`) collide and produce `CreateDto_1`-style suffixes, or overwrite each other. Name DTOs uniquely and descriptively (`CreateOrderDto`, `CreateInvoiceDto`); that also keeps generated SDK type names meaningful.

## Documentation quality checklist

| Do | Why |
|----|-----|
| Describe fields that aren't self-explanatory (units, formats, semantics) | `amount` could be dollars or cents |
| Give realistic **examples** | They drive Try-it-out, mocks, and comprehension |
| Document **constraints** (min/max/length/pattern) | Clients validate early |
| Mark `readOnly`/`writeOnly`, `nullable`, `deprecated` accurately | Correct SDK types and behavior |
| Use `enumName` for shared enums | One schema, nicer SDKs |
| Keep descriptions in sync with validators (use the plugin shim) | No drift |
| Never put real secrets/PII in examples | Docs are often public |

## Testing

- Snapshot or diff the **generated schema** for key DTOs, so accidental property or constraint changes show up in review ([SDK generation and CI](./06-client-sdk-generation.md)).
- Assert the document validates against OpenAPI (Spectral/validator) and contains expected schemas (`components.schemas.OrderDto` has `status` with enum values).
- In E2E tests, compare a real response with the documented response schema (a JSON-schema check of response bodies against the spec) to catch drift between documentation and behavior ([E2E testing](../01-testing/06-e2e-testing.md)).

## Common mistakes

- **Interfaces/type aliases** as DTOs (empty schemas); **plugin not processing** the file.
- **Using `@nestjs/mapped-types`** instead of `@nestjs/swagger` mapped types, losing OpenAPI metadata.
- **Array/nested types without `type: () => X`**, giving `object`/`array` with no items schema.
- **Circular DTO references** without lazy types.
- **Documenting `Date` as an object** instead of a `date-time` string; `bigint`/`Decimal` ignored.
- **Not setting `enumName`**, duplicating enums everywhere.
- **Confusing optional with nullable.**
- **Hand-written constraints drifting from validators.**
- **No response DTOs**, so responses show entity internals (or nothing).
- **Duplicate class names** colliding in the schema.
- **Exposing sensitive-looking examples**, and `@ApiHideProperty` mistaken for security.

## Debugging

- A property is missing from the schema: the DTO file wasn't processed by the plugin (suffix, build pipeline) and has no `@ApiProperty`; the property is `@ApiHideProperty`; or it's a getter/method.
- `type: object` with no details: add `type: () => Dto` (nested) or `type: [Dto]`/`isArray` for arrays.
- `Cannot access 'X' before initialization`: circular import; use `() => X` and consider restructuring imports ([circular dependencies](../../03-core-concepts/04-modules-and-di/04-circular-dependencies.md)).
- Schema named `CreateDto_1`: duplicate class names; rename.
- Derived DTO empty: switch the mapped-type import to `@nestjs/swagger`.
- Open `/docs-json` and search `components.schemas` to see exactly what was generated.

## Quick Summary

- DTO classes become schemas: use the CLI plugin (with the class-validator shim) to infer most details and `@ApiProperty` to add descriptions, examples, enums (`enumName`), formats, and constraints.
- Be explicit for arrays, nested/circular types (`type: () => X`), unions (`oneOf` + `@ApiExtraModels`), maps, dates, and money.
- Use mapped types from `@nestjs/swagger`; keep separate, annotated request and response DTOs; mark `readOnly`/`writeOnly`/`nullable`/`deprecated` correctly.
- Generic wrappers (pagination, envelopes) need a custom decorator because generics are erased.
- Use unique DTO names, never rely on `@ApiHideProperty` for security, and test the generated schema against real responses.

## Next

[Documenting authentication →](./04-documenting-auth.md)
