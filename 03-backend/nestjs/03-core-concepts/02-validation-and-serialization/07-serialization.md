# Serialization

Serialization is the **last transformation before data leaves your API**: deciding which fields, in which shape, a client receives. Its most important job is negative: making sure `passwordHash`, internal flags, and other private fields never appear in a response.

Nest offers `ClassSerializerInterceptor` (decorator-driven, built on class-transformer), and you can always map to **response DTOs** by hand. Both are valid; they have different failure modes.

Prerequisites: [Interceptors](../01-request-pipeline/05-interceptors.md), [class-transformer](./04-class-transformer.md).

## The problem

```ts
@Get(':id')
findOne(@Param('id') id: string) {
  return this.users.findById(id);   // returns the whole entity, including passwordHash
}
```

Whatever the handler returns is serialized to JSON as is. Without a serialization step, every field on the object is sent.

## `ClassSerializerInterceptor`

### 1. Annotate the class

```ts
// user.entity.ts
import { Exclude, Expose, Transform } from 'class-transformer';

export class UserEntity {
  id: string;
  email: string;

  @Exclude()
  passwordHash: string;

  @Expose()
  get displayName(): string {
    return this.email.split('@')[0];
  }

  @Transform(({ value }) => value.toISOString(), { toPlainOnly: true })
  createdAt: Date;

  constructor(partial: Partial<UserEntity>) {
    Object.assign(this, partial);
  }
}
```

### 2. Return **instances**, not plain objects

```ts
@UseInterceptors(ClassSerializerInterceptor)
@Get(':id')
async findOne(@Param('id') id: string) {
  const row = await this.users.findById(id);
  return new UserEntity(row);   // ← must be a class instance
}
```

### 3. Enable it

```ts
// per controller/route
@UseInterceptors(ClassSerializerInterceptor)

// globally (DI-aware)
providers: [{ provide: APP_INTERCEPTOR, useClass: ClassSerializerInterceptor }]

// globally in main.ts (needs the Reflector)
app.useGlobalInterceptors(new ClassSerializerInterceptor(app.get(Reflector)));
```

The interceptor runs `instanceToPlain` on the handler's return value (including arrays and, recursively, nested class instances), honoring `@Exclude`, `@Expose`, and `@Transform`.

## The #1 gotcha: it only works on class instances

`@Exclude()` is metadata on a **class**. If the object returned isn't an instance of that class, the interceptor has nothing to apply, and the field **leaks**.

| What you return | Serialized correctly? |
|-----------------|----------------------|
| `new UserEntity(row)` | Yes |
| TypeORM entity instance (class with decorators) | Yes, if the entity class uses `@Exclude` |
| **Prisma results** (plain objects) | **No**; map them first |
| **Mongoose `.lean()`** results / `toObject()` | **No**; wrap in a class |
| `{ ...user }` spread of an entity | **No**; the prototype is lost |
| Raw query results (`getRawMany`, `$queryRaw`) | **No** |

Fix with `plainToInstance` or a constructor:

```ts
return plainToInstance(UserEntity, row);          // or: new UserEntity(row)
return rows.map((r) => new UserEntity(r));        // arrays: map each
```

Test the leak case explicitly (see below). It fails silently.

## Allow-list strategy (safer default)

`@Exclude()` is a **deny-list**: a new sensitive column is public until someone remembers to exclude it. Flip it to an allow-list:

Option A: class-level `@Exclude()` on the entity, then `@Expose()` on each property you want public:

```ts
@Exclude()
export class UserEntity {
  @Expose() id: string;
  @Expose() email: string;
  passwordHash: string;          // not exposed → never serialized
}
```

Option B: set the strategy for a controller or route with `@SerializeOptions` (options are forwarded to class-transformer):

```ts
@SerializeOptions({ strategy: 'excludeAll' })
@Controller('users')
export class UsersController {}
```

With `excludeAll`, only properties decorated with `@Expose()` are serialized. Verify the result for your setup with a test.

Other `@SerializeOptions` values pass through too, for example:

```ts
@SerializeOptions({ groups: ['admin'] })        // pairs with @Expose({ groups: ['admin'] })
@SerializeOptions({ excludePrefixes: ['_'] })   // hide properties starting with _
```

## Response DTOs (explicit mapping)

The alternative is to never return entities at all:

```ts
export class UserResponseDto {
  id: string;
  email: string;

  static from(u: User): UserResponseDto {
    const dto = new UserResponseDto();
    dto.id = u.id;
    dto.email = u.email;
    return dto;
  }
}

return UserResponseDto.from(user);
```

Or with decorators: `plainToInstance(UserResponseDto, user, { excludeExtraneousValues: true })` plus `@Expose()` on exactly the fields you want. `excludeExtraneousValues` drops everything not exposed.

| | `@Exclude` on entity | Response DTOs |
|-|----------------------|---------------|
| Boilerplate | Low | Higher |
| Leak risk | Higher (deny-list, plain-object gap) | Lower (explicit allow-list) |
| Different shapes per endpoint | Awkward (groups) | Natural |
| Works with Prisma/lean objects | Needs mapping anyway | Needs mapping anyway |
| Swagger accuracy | Needs entity annotations | Clean per-endpoint schemas |

Rule of thumb: small apps with class-based entities can use `@Exclude` + interceptor; anything handling sensitive data, Prisma, or multiple response shapes should use response DTOs, ideally with an allow-list.

## Nested relations and arrays

Nested objects need `@Type` on the **output** side too, or they stay plain and unfiltered:

```ts
export class PostEntity {
  id: string;

  @Type(() => UserEntity)
  author: UserEntity;     // now author.passwordHash is excluded too
}
```

Arrays of instances are handled; `Page<T>` wrappers must be classes (or have their items mapped) for nested rules to apply.

## Interaction with other pieces

- **Swagger:** the serializer doesn't tell Swagger anything. Document responses with `@ApiProperty` on response DTOs ([DTO documentation](../../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md)).
- **Response envelopes:** run the serializer on the **inner** data; order of interceptors matters ([interceptor recipes](../01-request-pipeline/06-interceptor-recipes.md)).
- **`@Res()`:** bypasses interceptors' mapping, so nothing is serialized. Use `{ passthrough: true }`.
- **Streams/files:** not serialized by this interceptor.
- **Performance:** `instanceToPlain` walks the object graph. For very large lists, hand-written mapping can be faster. Measure before optimizing.
- **Logging:** the interceptor only affects HTTP output; passwords can still appear in logs. Redact in your logger ([structured logging](../../07-production/03-observability/02-structured-logging-with-pino.md)).

## Testing for leaks

```ts
it('never returns passwordHash', async () => {
  const res = await request(app.getHttpServer()).get('/users/1').expect(200);
  expect(res.body).not.toHaveProperty('passwordHash');
});
```

Add this for every endpoint that returns a user-like object. See [e2e testing](../../04-intermediate/01-testing/06-e2e-testing.md).

## Common mistakes

- **Returning plain objects** (Prisma, `lean()`, raw queries, spreads) and assuming `@Exclude` works.
- **Using a deny-list** and forgetting to exclude a newly added sensitive column.
- **Forgetting to enable the interceptor**; `@Exclude` alone does nothing.
- **Instantiating `ClassSerializerInterceptor` without `Reflector`** in `useGlobalInterceptors`.
- **Missing `@Type` on nested relations**, so nested secrets leak.
- **Using the same class for input DTO and serialized entity**, mixing `@Exclude` (output) with validation (input).
- **Assuming serialization protects logs, caches, or queue payloads.** It only affects the HTTP response.

## Debugging

- Field still present? Log `value instanceof UserEntity` just before returning. If false, you returned a plain object.
- Interceptor not running? Check `@UseInterceptors` / `APP_INTERCEPTOR`, and that the handler doesn't use `@Res()`.
- Getter not appearing? It needs `@Expose()`, and the value must be on an instance.
- Unexpected fields under `excludeAll`? Ensure the strategy is applied where class-transformer sees it (decorator on the class or route), and test it.

## Quick Summary

- Serialization decides what leaves the API; its main job is to stop private fields leaking.
- `ClassSerializerInterceptor` applies `@Exclude`/`@Expose`/`@Transform`, but **only on class instances**.
- Prisma, `lean()`, raw queries, and spreads return plain objects, so map them to classes first.
- Prefer allow-lists (`excludeAll` + `@Expose`) or explicit response DTOs for sensitive data.
- Add `@Type` for nested relations and write a leak test per endpoint.

## Next

[Schema validation alternatives →](./08-schema-validation-alternatives.md)
