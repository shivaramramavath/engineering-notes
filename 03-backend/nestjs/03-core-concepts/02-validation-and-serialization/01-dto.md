# DTO (Data Transfer Object)

A DTO describes the **shape of data crossing a boundary**: what a client may send in, or what you promise to send back. In Nest, DTOs are usually **classes**, because decorators (validation, transformation, Swagger) need something that survives compilation.

## Why classes, not interfaces

TypeScript interfaces and `type` aliases are erased at compile time. Nest's `ValidationPipe` reads the **runtime** type of a parameter (via `emitDecoratorMetadata`) to know what to validate. An interface leaves nothing to read.

```ts
// ❌ interface: no runtime metadata, ValidationPipe does nothing
interface CreateUserDto { email: string }

// ✅ class: metadata exists, decorators attach rules
export class CreateUserDto {
  @IsEmail()
  email: string;
}
```

Required in `tsconfig.json` (Nest projects have these by default):

```json
{ "compilerOptions": { "experimentalDecorators": true, "emitDecoratorMetadata": true } }
```

## Basic example

```ts
// dto/create-user.dto.ts
import { IsEmail, IsString, MinLength } from 'class-validator';

export class CreateUserDto {
  @IsEmail()
  email: string;

  @IsString()
  @MinLength(8)
  password: string;
}
```

```ts
@Post()
create(@Body() dto: CreateUserDto) {
  return this.users.create(dto);
}
```

With a [global `ValidationPipe`](./02-validation-pipe.md), invalid bodies are rejected with `400` before `create()` runs.

> If `strictPropertyInitialization` is on, properties need `!` (`email!: string`) or `strict` relaxed. Nest's default scaffold disables strict property initialization.

## What a DTO is *not*

| Concept | Purpose | Lives at |
|---------|---------|----------|
| **DTO** | Shape of data over the wire | Controller boundary |
| **Entity** | Persistence model (table/document) | Data layer |
| **Domain model** | Business rules and invariants | Core/domain layer |

Mixing them is a classic source of bugs. Two concrete risks of using an entity as an input DTO:

- **Mass assignment**: a client sends `{ "role": "admin" }` and it lands in your database because the entity has a `role` column.
- **Coupled contracts**: renaming a column silently breaks the public API.

Keep input DTOs minimal and explicit, and map to entities in the service.

## One DTO per intent

Separate input from output, and create from update.

```ts
// input
export class CreatePostDto { title: string; body: string }
export class UpdatePostDto { title?: string; body?: string }

// output
export class PostResponseDto { id: string; title: string; createdAt: Date }
```

Don't add `id`, `role`, `createdAt`, or `isAdmin` to input DTOs just because the entity has them.

## Mapped types: derive DTOs instead of copy-pasting

`@nestjs/mapped-types` builds new DTO classes from existing ones **and carries over validation metadata**.

```bash
npm i @nestjs/mapped-types
```

```ts
import { PartialType, PickType, OmitType, IntersectionType } from '@nestjs/mapped-types';

export class UpdateUserDto extends PartialType(CreateUserDto) {}            // all fields optional
export class LoginDto extends PickType(CreateUserDto, ['email', 'password'] as const) {}
export class PublicUserDto extends OmitType(CreateUserDto, ['password'] as const) {}
export class UpdateWithMetaDto extends IntersectionType(UpdateUserDto, MetaDto) {}
```

- `PartialType` makes each property optional: it applies `@IsOptional()` on top of existing validators.
- Pass property names `as const` so TypeScript can infer the picked/omitted keys.

**If you use Swagger**, import these helpers from `@nestjs/swagger` instead. They behave the same but also preserve `@ApiProperty` metadata, so your OpenAPI schema stays correct:

```ts
import { PartialType } from '@nestjs/swagger';
```

Use one package consistently per project (see [DTO documentation](../../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md)).

## Params and queries can have DTOs too

```ts
export class ListUsersQueryDto {
  @IsOptional() @Type(() => Number) @IsInt() @Min(1)
  page?: number = 1;

  @IsOptional() @Type(() => Number) @IsInt() @Min(1) @Max(100)
  limit?: number = 20;
}

@Get()
list(@Query() query: ListUsersQueryDto) {}
```

Query and path values arrive as **strings**. Numeric/boolean DTO fields need `@Type(() => Number)` or `transform: true` with implicit conversion; see [class-transformer](./04-class-transformer.md) and [ValidationPipe](./02-validation-pipe.md).

## Response DTOs

Returning an entity directly can leak fields (`passwordHash`, internal flags). Two approaches, covered in [Serialization](./07-serialization.md):

1. Annotate entities with `@Exclude()` and use `ClassSerializerInterceptor`.
2. Map entities to dedicated response DTOs explicitly (more verbose, harder to leak by accident).

## Conventions

- Name by intent: `CreateOrderDto`, `UpdateOrderDto`, `OrderResponseDto`, `ListOrdersQueryDto`.
- One DTO per file in a `dto/` folder inside the feature module.
- DTOs hold **shape and rules only**, with no business logic and no injected services.
- Don't reuse a request DTO as a response DTO. They diverge quickly.
- Prefer `readonly` only if you aren't relying on `class-transformer` to assign properties after construction (it sets them post-instantiation, which `readonly` types don't prevent at runtime but can complicate typing).

## Common mistakes

- **Using `interface` for DTOs**, so no validation happens and no error is shown.
- **Importing the DTO type with `import type`** in a controller. That erases the runtime reference and breaks `emitDecoratorMetadata`. Use a regular import for DTO classes used in decorated signatures.
- **Forgetting `@IsOptional()`** on `UpdateDto` properties instead of using `PartialType`.
- **Reusing entities as DTOs**, causing mass assignment and accidental data exposure.
- **Mixing `@nestjs/mapped-types` and `@nestjs/swagger`** helpers and losing Swagger metadata.

## Quick Summary

- DTOs are classes so decorators and runtime metadata exist.
- Separate input DTOs, update DTOs, query DTOs, and response DTOs; never expose entities as input.
- Derive with `PartialType` / `PickType` / `OmitType` / `IntersectionType` (use the `@nestjs/swagger` versions when documenting with Swagger).
- Query/param values are strings; convert explicitly or via `transform`.

## Next

[ValidationPipe →](./02-validation-pipe.md)
