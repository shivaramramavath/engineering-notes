# Custom Decorators

Nest's built-ins (`@Body()`, `@Param()`, `@UseGuards()`) are just decorators. You can write your own to remove repetition and make intent readable at the call site. Three kinds matter in practice:

| Kind | Purpose | Tool |
|------|---------|------|
| **Parameter decorator** | Extract a value for a handler argument (`@CurrentUser()`) | `createParamDecorator` |
| **Metadata decorator** | Attach data that guards/interceptors read (`@Roles('admin')`, `@Public()`) | `SetMetadata` / `Reflector.createDecorator` |
| **Composed decorator** | Bundle several decorators into one (`@Auth('admin')`) | `applyDecorators` |

Prerequisites: [Decorators](../../02-fundamentals/06-decorators.md) and [Execution context](./09-execution-context.md).

## Parameter decorators

```ts
// current-user.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const CurrentUser = createParamDecorator(
  (data: string | undefined, ctx: ExecutionContext) => {
    const user = ctx.switchToHttp().getRequest().user;
    return data ? user?.[data] : user;
  },
);
```

```ts
@Get('me')
me(@CurrentUser() user: UserPayload) {}

@Get('me/id')
myId(@CurrentUser('id') id: string) {}
```

- `data` is whatever you pass in the parentheses (`'id'` above).
- The decorator **only reads**; it doesn't authenticate. `req.user` exists because a guard put it there. Without that guard, `@CurrentUser()` returns `undefined`.
- For type safety, type `data` as `keyof UserPayload` instead of `string`:

```ts
export const CurrentUser = createParamDecorator(
  (key: keyof UserPayload | undefined, ctx: ExecutionContext) => { /* ... */ },
);
```

### Using pipes with custom decorators

Pipes work on the value a custom decorator returns:

```ts
@Get()
find(@CurrentUser(new ValidationPipe({ validateCustomDecorators: true })) user: UserDto) {}
```

`validateCustomDecorators: true` is required. By default `ValidationPipe` skips values from custom decorators (`metadata.type === 'custom'`).

### Multi-transport

If the decorator may run outside HTTP (WebSocket gateways, microservices), branch on `ctx.getType()` or write a separate decorator per transport. See [execution context](./09-execution-context.md).

## Metadata decorators

These don't do anything themselves; they label a handler or class. A guard or interceptor reads the label.

### `SetMetadata`

```ts
import { SetMetadata } from '@nestjs/common';

export const ROLES_KEY = 'roles';
export const Roles = (...roles: string[]) => SetMetadata(ROLES_KEY, roles);

export const IS_PUBLIC_KEY = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);
```

Read with `Reflector` ([guards](./04-guards.md) shows full guard code):

```ts
this.reflector.getAllAndOverride<string[]>(ROLES_KEY, [context.getHandler(), context.getClass()]);
```

### `Reflector.createDecorator` (typed, v10+)

Removes the string key and gives type inference end to end:

```ts
// roles.decorator.ts
import { Reflector } from '@nestjs/core';
export const Roles = Reflector.createDecorator<string[]>();

// usage
@Roles(['admin'])
@Delete(':id')
remove() {}

// in a guard: no key needed, result is string[] | undefined
const roles = this.reflector.get(Roles, context.getHandler());
```

Note the usage difference: `Reflector.createDecorator<string[]>()` takes the value as one argument (`@Roles(['admin'])`), unlike the variadic `SetMetadata` wrapper above. A decorator created with `createDecorator` also works with `getAllAndOverride(Roles, [...])` and `getAllAndMerge(Roles, [...])`.

Pick one style per codebase. `createDecorator` is nicer for new code; `SetMetadata` is everywhere in existing docs and projects.

## Composing decorators

`applyDecorators` merges several **method/class** decorators into one:

```ts
// auth.decorator.ts
import { applyDecorators, UseGuards } from '@nestjs/common';
import { ApiBearerAuth, ApiForbiddenResponse, ApiUnauthorizedResponse } from '@nestjs/swagger';

export function Auth(...roles: string[]) {
  return applyDecorators(
    Roles(roles),
    UseGuards(JwtAuthGuard, RolesGuard),
    ApiBearerAuth(),
    ApiUnauthorizedResponse({ description: 'Unauthorized' }),
    ApiForbiddenResponse({ description: 'Forbidden' }),
  );
}
```

```ts
@Auth('admin')
@Delete(':id')
remove() {}
```

Benefits: one line instead of five, and the combination can't be half-applied. Guard order inside `UseGuards(...)` still matters (authenticate before authorize). Swagger side: [documenting auth](../../04-intermediate/09-openapi-and-swagger/04-documenting-auth.md).

`applyDecorators` returns a method and class decorator. It can't compose **parameter** decorators.

## Practical examples

### Pagination params from the query

```ts
export interface Pagination { page: number; limit: number }

export const Paginate = createParamDecorator((_: unknown, ctx: ExecutionContext): Pagination => {
  const q = ctx.switchToHttp().getRequest().query;
  const page = Math.max(parseInt(q.page, 10) || 1, 1);
  const limit = Math.min(Math.max(parseInt(q.limit, 10) || 20, 1), 100);
  return { page, limit };
});

@Get()
list(@Paginate() { page, limit }: Pagination) {}
```

Keep clamping/defaults in one place instead of repeating `DefaultValuePipe`/`ParseIntPipe` per route. See [pagination](../../04-intermediate/08-api-design/03-pagination.md).

### Client IP / headers

```ts
export const ClientIp = createParamDecorator((_, ctx: ExecutionContext) => {
  const req = ctx.switchToHttp().getRequest();
  return req.ip;
});
```

Behind a proxy `req.ip` is only correct when `trust proxy` is configured. See [reverse proxy](../../07-production/04-deployment/04-reverse-proxy-and-nginx.md).

## Important behavior

- Decorators are evaluated **at class definition time**, but the function inside `createParamDecorator` runs **per request**.
- Metadata decorators can be applied at **handler or class** level; guards should check both (`getAllAndOverride` with `[handler, class]`).
- A param decorator runs when Nest resolves the handler's arguments, i.e. **after guards and interceptors' pre-logic** and **before pipes finish**. So `req.user` set by a guard is available.
- Param decorators can't use dependency injection; they're plain functions. If you need a service, do the work in a guard/interceptor/pipe and let the decorator just read the result from `req`.

## Common mistakes

- **Assuming `@CurrentUser()` authenticates.** It reads `req.user` only.
- **Mixing `SetMetadata` key strings** between decorator and guard. Export the key constant from one file, or use `Reflector.createDecorator`.
- **Forgetting `validateCustomDecorators: true`** and wondering why DTO validation is skipped for a custom decorator.
- **Trying to inject services into `createParamDecorator`.** Not supported.
- **Using `applyDecorators` for parameter decorators.** Compose method/class decorators only.
- **Reading metadata from the handler only**, so class-level `@Roles()` is ignored.
- **Doing heavy work (DB calls) inside a param decorator.** It hides cost and can't use DI.

## Debugging

- Decorator returns `undefined`: check whether the guard that populates `req.user` ran (order, `@Public()` skipping it, non-HTTP context).
- Metadata not found: log the key and the target you pass to `Reflector`. Confirm the decorator is applied on the same handler/class.
- Composed decorator misbehaves: expand `applyDecorators` mentally into its parts and check each one individually.

## Quick Summary

- Param decorators (`createParamDecorator`) pull values from the context; they read, they don't authenticate.
- Metadata decorators (`SetMetadata`, `Reflector.createDecorator`) label routes for guards and interceptors to read.
- `applyDecorators` bundles method/class decorators like auth + roles + Swagger into one.
- No DI inside param decorators; use `validateCustomDecorators: true` to validate their output.
- Share metadata keys from one place and read handler + class metadata.

## Next

Section complete. Continue with [Validation and serialization](../02-validation-and-serialization/README.md), which builds directly on pipes and interceptors.

← Back to [Request pipeline overview](./README.md)
