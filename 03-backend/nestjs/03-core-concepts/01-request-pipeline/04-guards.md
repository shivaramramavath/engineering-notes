# Guards

A guard decides **whether a request may reach the handler**. It runs after middleware and before interceptors and pipes, and it has access to the [`ExecutionContext`](./09-execution-context.md), so it knows which controller and handler is about to run.

That last point is what separates guards from middleware: a guard can read metadata (`@Roles('admin')`, `@Public()`) attached to the target handler and decide accordingly.

## Core concept

A guard is a class implementing `CanActivate`:

```ts
export interface CanActivate {
  canActivate(context: ExecutionContext): boolean | Promise<boolean> | Observable<boolean>;
}
```

- Return `true` → continue down the pipeline.
- Return `false` → Nest throws `ForbiddenException` (403).
- Throw an exception → that exception is used (e.g. `UnauthorizedException` for 401).

> **401 vs 403:** `401 Unauthorized` means "I don't know who you are" (authentication failed). `403 Forbidden` means "I know who you are, and you can't do this" (authorization failed). Returning `false` always gives 403, so throw `UnauthorizedException` yourself when authentication is the problem.

## Basic example

```ts
// auth.guard.ts
import { CanActivate, ExecutionContext, Injectable, UnauthorizedException } from '@nestjs/common';
import { Request } from 'express';

@Injectable()
export class ApiKeyGuard implements CanActivate {
  canActivate(context: ExecutionContext): boolean {
    const req = context.switchToHttp().getRequest<Request>();
    const key = req.header('x-api-key');
    if (key !== process.env.API_KEY) {
      throw new UnauthorizedException('Invalid API key');
    }
    return true;
  }
}
```

(In real code, inject `ConfigService` instead of reading `process.env` directly. See [configuration](../03-configuration/01-configuration-basics.md).)

## Binding scopes

```ts
// Route
@UseGuards(ApiKeyGuard)
@Get('secret')
secret() {}

// Controller
@UseGuards(ApiKeyGuard)
@Controller('admin')
export class AdminController {}

// Global, without DI
app.useGlobalGuards(new ApiKeyGuard());

// Global, with DI (preferred)
{ provide: APP_GUARD, useClass: ApiKeyGuard }
```

`@UseGuards(A, B)` runs `A` then `B`, stopping at the first that denies. Across scopes the order is global → controller → route.

## Metadata-driven guards (roles)

The usual pattern: a decorator attaches metadata, a guard reads it with `Reflector`.

```ts
// roles.decorator.ts
import { SetMetadata } from '@nestjs/common';
export const ROLES_KEY = 'roles';
export const Roles = (...roles: string[]) => SetMetadata(ROLES_KEY, roles);
```

```ts
// roles.guard.ts
import { CanActivate, ExecutionContext, Injectable } from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { ROLES_KEY } from './roles.decorator';

@Injectable()
export class RolesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(context: ExecutionContext): boolean {
    const required = this.reflector.getAllAndOverride<string[]>(ROLES_KEY, [
      context.getHandler(),
      context.getClass(),
    ]);
    if (!required?.length) return true; // no @Roles() → no restriction

    const { user } = context.switchToHttp().getRequest();
    return required.some((role) => user?.roles?.includes(role));
  }
}
```

```ts
@Controller('users')
@UseGuards(RolesGuard)
export class UsersController {
  @Roles('admin')
  @Delete(':id')
  remove(@Param('id') id: string) {}
}
```

`Reflector` lookup methods:

| Method | Behavior |
|--------|----------|
| `get(key, target)` | Reads metadata from one target |
| `getAllAndOverride(key, [handler, class])` | First defined value wins (handler overrides class) |
| `getAllAndMerge(key, [handler, class])` | Merges arrays/objects from all targets |

Full RBAC design lives in [RBAC](../../04-intermediate/07-authorization/02-rbac.md).

## Practical usage: global auth with opt-out

The common production setup is "everything requires auth, except routes marked `@Public()`":

```ts
// public.decorator.ts
export const IS_PUBLIC_KEY = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);
```

```ts
// jwt-auth.guard.ts
@Injectable()
export class JwtAuthGuard implements CanActivate {
  constructor(
    private readonly reflector: Reflector,
    private readonly jwt: JwtService,
  ) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [
      context.getHandler(),
      context.getClass(),
    ]);
    if (isPublic) return true;

    const req = context.switchToHttp().getRequest();
    const [type, token] = req.headers.authorization?.split(' ') ?? [];
    if (type !== 'Bearer' || !token) throw new UnauthorizedException();

    try {
      req.user = await this.jwt.verifyAsync(token);
    } catch {
      throw new UnauthorizedException();
    }
    return true;
  }
}
```

```ts
// app.module.ts
providers: [{ provide: APP_GUARD, useClass: JwtAuthGuard }]
```

Secure by default: forgetting to annotate a route fails closed (requires auth), not open. Full JWT flow: [JWT](../../04-intermediate/06-authentication/04-jwt.md).

If you use Passport, `AuthGuard('jwt')` from `@nestjs/passport` is a ready-made guard you can extend with the same `@Public()` check. See [Passport and local strategy](../../04-intermediate/06-authentication/02-passport-and-local-strategy.md).

## Important behavior

- **Order:** guards run after all middleware and before interceptors/pipes. A guard therefore sees **unvalidated, untransformed** input. Don't trust `req.body` shape in a guard.
- **Guards cannot modify the response** beyond throwing. To reshape output use an interceptor.
- **Async is fine**: return a `Promise` or `Observable` (e.g., to look up a user or permission).
- **Attaching to `req.user`** is a convention, not magic. Downstream code (custom param decorators like `@CurrentUser()`) only works because a guard populated it ([custom decorators](./10-custom-decorators.md)).
- **Multiple protocols:** guards also work for WebSockets, microservices, and GraphQL; read the request through `ExecutionContext`, using the right `switchTo*` or adapter ([execution context](./09-execution-context.md)).

## Common mistakes

- **Returning `false` for unauthenticated users.** You get 403 instead of 401. Throw `UnauthorizedException`.
- **Authentication in middleware, authorization in guards, with no shared contract.** Pick one place (usually a guard) to populate `req.user`.
- **Forgetting that `@UseGuards` order matters.** Authenticate first, then authorize: `@UseGuards(JwtAuthGuard, RolesGuard)`.
- **Reading metadata from only the handler** and missing class-level decorators. Pass both `getHandler()` and `getClass()` to `Reflector`.
- **Using `useGlobalGuards(new XGuard())` for a guard with dependencies.** No DI; use `APP_GUARD`.
- **Expecting a guard to run on a route that doesn't exist.** Unmatched routes 404 before any handler-bound guard.
- **Trusting client-supplied role claims** without verifying the token signature or re-checking against the source of truth.

## Debugging

- Always getting 403? Your guard returned `false` (or an upstream guard did). Log the decision and what `Reflector` returned.
- `Reflector` returns `undefined`? Metadata key mismatch, or the decorator wasn't applied to the handler/class you queried.
- Guard injected dependencies are `undefined`? You instantiated it with `new` in `useGlobalGuards` or `@UseGuards(new X())`; use the class or `APP_GUARD`.
- Public route still blocked? A *second* guard (e.g., a Passport `AuthGuard` bound elsewhere) doesn't know about `@Public()`.

## Quick Summary

- A guard answers one question: may this request proceed?
- Return `true` to continue; `false` gives 403; throw for a specific status (401).
- Guards know the handler via `ExecutionContext`, so use `Reflector` + custom decorators for roles/permissions/public routes.
- Run after middleware, before interceptors and pipes.
- Prefer `APP_GUARD` for global, DI-aware guards; combine with `@Public()` for secure-by-default APIs.

## Next

[Interceptors →](./05-interceptors.md)
