# Pipes

A pipe runs **just before the handler** and receives a single argument value. It either **transforms** it (string `"42"` → number `42`) or **validates** it (throw if invalid). If a pipe throws, the handler never runs.

Pipes are the reason a typed handler parameter actually has the type you declared.

## Why you need them

TypeScript types are erased at runtime. This does **not** give you a number:

```ts
@Get(':id')
findOne(@Param('id') id: number) {
  console.log(typeof id); // 'string'. The annotation changes nothing at runtime
}
```

Route params and query values arrive as strings. A pipe does the real conversion:

```ts
@Get(':id')
findOne(@Param('id', ParseIntPipe) id: number) {
  console.log(typeof id); // 'number'
}
```

`GET /cats/abc` now returns `400 Bad Request` before your code runs:

```json
{ "message": "Validation failed (numeric string is expected)", "error": "Bad Request", "statusCode": 400 }
```

## Built-in pipes

All exported from `@nestjs/common`:

| Pipe | Does |
|------|------|
| `ValidationPipe` | Validates DTOs via class-validator, optionally transforms via class-transformer ([details](../02-validation-and-serialization/02-validation-pipe.md)) |
| `ParseIntPipe` / `ParseFloatPipe` / `ParseBoolPipe` | String → number / float / boolean |
| `ParseArrayPipe` | Parses/validates arrays (e.g., comma-separated query values with a `separator`) |
| `ParseUUIDPipe` | Validates a UUID (optional `version`) |
| `ParseEnumPipe` | Validates the value is a member of an enum |
| `ParseDatePipe` | String → `Date` (added in v10) |
| `ParseFilePipe` | Validates uploaded files (size/type validators), see [file upload](../../05-advanced/07-integrations/01-file-upload.md) |
| `DefaultValuePipe` | Supplies a default when the value is `undefined` |

## Binding scopes

```ts
// 1. Parameter: most specific
findOne(@Param('id', ParseIntPipe) id: number) {}

// 2. Handler: applies to every parameter of that method
@UsePipes(new ValidationPipe({ whitelist: true }))
@Post()
create(@Body() dto: CreateCatDto) {}

// 3. Controller
@UsePipes(ValidationPipe)
@Controller('cats')
export class CatsController {}

// 4. Global (all routes)
app.useGlobalPipes(new ValidationPipe());          // no DI
// or: { provide: APP_PIPE, useClass: ValidationPipe }  // DI-aware
```

Pass a **class** to let Nest instantiate it (and inject dependencies); pass an **instance** to configure it:

```ts
@Query('page', new ParseIntPipe({ errorHttpStatusCode: HttpStatus.NOT_ACCEPTABLE })) page: number
```

## Chaining and defaults

Pipes on one parameter run left to right:

```ts
@Get()
list(
  @Query('page', new DefaultValuePipe(1), ParseIntPipe) page: number,
  @Query('limit', new DefaultValuePipe(20), ParseIntPipe) limit: number,
) {}
```

`DefaultValuePipe` must come first, otherwise `ParseIntPipe` rejects `undefined` before the default is applied.

## Writing a custom pipe

Implement `PipeTransform`. `transform(value, metadata)` returns the (possibly changed) value or throws.

```ts
// trim-string.pipe.ts
import { ArgumentMetadata, Injectable, PipeTransform } from '@nestjs/common';

@Injectable()
export class TrimPipe implements PipeTransform<string, string> {
  transform(value: string, metadata: ArgumentMetadata) {
    return typeof value === 'string' ? value.trim() : value;
  }
}
```

`ArgumentMetadata` tells you what you're processing:

```ts
interface ArgumentMetadata {
  type: 'body' | 'query' | 'param' | 'custom'; // where it came from
  metatype?: Type<unknown>;                    // declared TS class (e.g. CreateCatDto), undefined for primitives/interfaces
  data?: string;                               // the key passed to the decorator, e.g. 'id' in @Param('id')
}
```

A pipe that validates against an existing entity is a good real-world case:

```ts
@Injectable()
export class UserByIdPipe implements PipeTransform<string, Promise<User>> {
  constructor(private readonly users: UsersService) {}

  async transform(id: string) {
    const user = await this.users.findById(id);
    if (!user) throw new NotFoundException(`User ${id} not found`);
    return user;
  }
}

// @Get(':id') find(@Param('id', ParseUUIDPipe, UserByIdPipe) user: User) {}
```

This works, but be deliberate: a pipe that hits the database hides a query inside a parameter decorator. For anything beyond a lookup-and-404, handle it in the service.

## Important behavior

- Pipes run **inside the exceptions zone**: a thrown `HttpException` is handled by [exception filters](./08-exception-filters.md) like any other.
- Pipes receive each decorated argument **separately** (`@Body()`, `@Query()`, `@Param()`, custom decorators). `@Req()` and `@Res()` objects are not piped.
- A pipe that returns a new value **replaces** the argument, so transformations stick.
- `ValidationPipe` only validates when `metatype` is a real class. Interfaces and `type` aliases are erased, so use classes for DTOs.
- Global `ValidationPipe` with `transform: true` also converts primitives based on the declared TS type (`@Param('id') id: number` becomes a number). Without it, you get strings. This is a frequent source of "why is it a string?" bugs.
- Validation of **custom parameter decorators** is off by default; enable with `new ValidationPipe({ validateCustomDecorators: true })` ([custom decorators](./10-custom-decorators.md)).

## Customizing the error

```ts
new ParseIntPipe({
  exceptionFactory: (error) => new BadRequestException(`id must be an integer (${error})`),
});
```

Most built-in parse pipes accept `errorHttpStatusCode` and `exceptionFactory`. For a project-wide error shape, normalize errors in an [exception filter](./08-exception-filters.md) rather than at every pipe.

## Common mistakes

- **Relying on the TypeScript annotation** (`id: number`) without a pipe or `transform: true`.
- **Order of pipes**: `ParseIntPipe` before `DefaultValuePipe` rejects missing values.
- **Using an interface for a DTO**: validation silently does nothing since there's no runtime metatype.
- **Heavy logic or I/O in pipes** that's really business logic.
- **Instantiating a pipe with `new` when it needs injected dependencies**; pass the class instead.

## Debugging

- Log `metadata` inside a throwaway pipe to see exactly what Nest passes (`type`, `metatype`, `data`).
- Getting strings instead of numbers? Check for a pipe or `ValidationPipe({ transform: true })`.
- Pipe not firing? Confirm the scope binding and that the parameter uses a Nest decorator (`@Body`, `@Query`, `@Param`).

## Quick Summary

- Pipes transform or validate one argument right before the handler.
- TS types don't convert anything at runtime; `ParseIntPipe` and friends do.
- Bind at parameter, handler, controller, or global scope; use `APP_PIPE` if DI is needed.
- Chain left to right; put `DefaultValuePipe` first.
- Custom pipes implement `PipeTransform` and get `ArgumentMetadata`.
- Errors thrown from pipes flow through exception filters.

## Next

[Guards →](./04-guards.md). For DTO validation specifics see [ValidationPipe](../02-validation-and-serialization/02-validation-pipe.md).
