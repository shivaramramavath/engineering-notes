---
title: "Metadata and Reflection in NestJS"
description: "In-depth examination of reflect-metadata, decorator mechanics, Reflector service APIs, and custom metadata pipelines in NestJS"
tags: [nestjs, internals, reflection, metadata, decorators, reflector]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Metadata and Reflection in NestJS

## Conceptual Overview
NestJS achieves its declarative syntax through TypeScript decorators and runtime metadata reflection. Every architectural construct—controllers, route handlers, guards, pipes, interceptors, and filters—relies on metadata attached to class prototypes and method descriptors via the ECMAScript reflection shim (`reflect-metadata`).

Rather than hardcoding routing tables or middleware arrays, NestJS inspects metadata at boot time to construct the routing tree, and at runtime to evaluate security policies (e.g., extracting allowed roles in `@Roles('admin')`).

Understanding the internal reflection model empowers engineers to:
- Build reusable custom decorators that combine multiple capabilities into single declarative annotations.
- Build custom policy evaluators using the `Reflector` service.
- Extract method-level and class-level metadata to implement custom audit, telemetry, and rate-limiting enhancers.

```
+-------------------------------------------------------------------------------+
|                       Metadata Reflection Architecture                        |
|                                                                               |
|  [ Source Code ]                                                              |
|  @Controller('users')                                                         |
|  export class UsersController {                                               |
|    @Roles('admin', 'editor')                                                  |
|    @Get(':id')                                                                |
|    getUser() {}                                                               |
|  }                                                                            |
|         |                                                                     |
|         v (TypeScript Compilation)                                            |
|  Reflect.defineMetadata('path', 'users', UsersController)                     |
|  Reflect.defineMetadata('roles', ['admin', 'editor'], target, 'getUser')      |
|  Reflect.defineMetadata('path', ':id', target, 'getUser')                     |
|  Reflect.defineMetadata('method', RequestMethod.GET, target, 'getUser')       |
|         |                                                                     |
|         v (Runtime Execution in Guards / Interceptors)                        |
|  reflector.getAllAndOverride<string[]>('roles', [context.getHandler(), ...])  |
|  -> ['admin', 'editor']                                                       |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. The `reflect-metadata` Shim
TypeScript decorators leverage the proposed ECMAScript Reflection API polyfilled by the `reflect-metadata` package. It provides low-level primitives:
- `Reflect.defineMetadata(metadataKey, metadataValue, target, propertyKey)`: Attaches arbitrary data to a class constructor or method.
- `Reflect.getMetadata(metadataKey, target, propertyKey)`: Retrieves metadata, traversing the prototype inheritance chain if not found directly on the target.
- `Reflect.getOwnMetadata(metadataKey, target, propertyKey)`: Retrieves metadata defined exclusively on the immediate target, ignoring prototype inheritance.

### 2. Internal Metadata Keys in NestJS Core
NestJS uses reserved string symbols internally to tag classes and functions:
- `PATH_METADATA` (`'path'`): Stores the route prefix on controllers and route paths on handler methods.
- `METHOD_METADATA` (`'method'`): Stores HTTP verb enums (`RequestMethod.GET`, etc.).
- `GUARDS_METADATA` (`'__guards__'`): Stores guard classes attached via `@UseGuards()`.
- `INTERCEPTORS_METADATA` (`'__interceptors__'`): Stores interceptors attached via `@UseInterceptors()`.
- `PIPES_METADATA` (`'__pipes__'`): Stores validation/transformation pipes attached via `@UsePipes()`.
- `EXCEPTION_FILTERS_METADATA` (`'__filters__'`): Stores exception filters attached via `@UseFilters()`.

### 3. The `Reflector` Service
`@nestjs/core` exposes the `Reflector` helper to simplify metadata retrieval from execution contexts:
- `reflector.get<T>(metadataKey, target)`: Retrieves metadata from a specific target (usually `context.getHandler()`).
- `reflector.getAllAndOverride<T>(metadataKey, [context.getHandler(), context.getClass()])`: Retrieves metadata, giving precedence to method-level metadata over class-level metadata.
- `reflector.getAllAndMerge<T>(metadataKey, [context.getHandler(), context.getClass()])`: Merges array or object metadata from both method-level and class-level decorators.

---

## Implementation Details & NestJS APIs

### 1. Defining Custom Metadata with `SetMetadata` and Custom Decorators

```typescript
// src/auth/decorators/roles.decorator.ts
import { SetMetadata, CustomDecorator } from '@nestjs/common';

export const ROLES_KEY = 'roles';
export type Role = 'admin' | 'user' | 'billing' | 'support';

/**
 * Attaches permitted roles to a route handler or controller class
 */
export const Roles = (...roles: Role[]): CustomDecorator<string> =>
  SetMetadata(ROLES_KEY, roles);
```

```typescript
// src/auth/decorators/public.decorator.ts
import { SetMetadata } from '@nestjs/common';

export const IS_PUBLIC_KEY = 'isPublic';
/**
 * Bypasses global authentication guards
 */
export const Public = () => SetMetadata(IS_PUBLIC_KEY, true);
```

### 2. Consuming Metadata in a Guard with `Reflector`

```typescript
// src/auth/guards/roles.guard.ts
import { Injectable, CanActivate, ExecutionContext } from '@nestjs/common';
import { Reflector } from '@nestjs/core';
import { ROLES_KEY, Role } from '../decorators/roles.decorator';
import { IS_PUBLIC_KEY } from '../decorators/public.decorator';

@Injectable()
export class RolesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(context: ExecutionContext): boolean {
    // 1. Check if route is marked as @Public()
    const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC_KEY, [
      context.getHandler(),
      context.getClass(),
    ]);

    if (isPublic) {
      return true;
    }

    // 2. Extract required roles, prioritizing method-level over class-level
    const requiredRoles = this.reflector.getAllAndOverride<Role[]>(ROLES_KEY, [
      context.getHandler(),
      context.getClass(),
    ]);

    if (!requiredRoles || requiredRoles.length === 0) {
      return true; // No roles restricted
    }

    // 3. Evaluate user against required roles
    const request = context.switchToHttp().getRequest();
    const user = request.user;

    if (!user || !user.roles) {
      return false;
    }

    return requiredRoles.some((role) => user.roles.includes(role));
  }
}
```

### 3. Composing Multiple Decorators (`applyDecorators`)

NestJS provides `applyDecorators` to bundle multiple decorators into a single reusable abstraction:

```typescript
// src/auth/decorators/auth.decorator.ts
import { applyDecorators, UseGuards } from '@nestjs/common';
import { ApiBearerAuth, ApiUnauthorizedResponse } from '@nestjs/swagger';
import { JwtAuthGuard } from '../guards/jwt-auth.guard';
import { RolesGuard } from '../guards/roles.guard';
import { Roles, Role } from './roles.decorator';

export function Auth(...roles: Role[]) {
  return applyDecorators(
    Roles(...roles),
    UseGuards(JwtAuthGuard, RolesGuard),
    ApiBearerAuth(),
    ApiUnauthorizedResponse({ description: 'Unauthorized access' }),
  );
}

// Usage in controller:
// @Auth('admin')
// @Get('sensitive-metrics')
```

---

## Production Patterns & Best Practices

### 1. Method vs Class Metadata Merging
When decorating both the class and a specific method, choose the correct resolution strategy:
- Use `reflector.getAllAndOverride()` when a method decorator should **override / replace** class-level configuration (e.g. `@Public()` on one route of an authenticated `@Controller()`).
- Use `reflector.getAllAndMerge()` when values should **accumulate** (e.g. `@Scopes('read')` on controller and `@Scopes('write')` on method should combine into `['read', 'write']`).

### 2. Custom Parameter Decorators with `createParamDecorator`
Extracting deeply nested request properties:

```typescript
// src/common/decorators/current-user.decorator.ts
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const CurrentUser = createParamDecorator(
  (data: string | undefined, ctx: ExecutionContext) => {
    const request = ctx.switchToHttp().getRequest();
    const user = request.user;

    return data ? user?.[data] : user;
  },
);

// In controller:
// @Get('profile')
// getProfile(@CurrentUser('id') userId: string) {}
```

---

## Performance & Scale Considerations

- **Reflection Overhead**: Reflect calls (`Reflect.getMetadata`) access internal V8 hidden classes and property maps. While very fast ($< 0.001	ext{ms}$), avoid invoking `Reflect.getMetadata` inside tight computation loops. Guard and interceptor invocations run once per HTTP turn, which is negligible compared to network I/O.
- **Memory Footprint**: Metadata is retained in memory for the lifetime of the process. Avoid storing massive JSON blobs or multi-megabyte configurations in metadata keys; store pointers or keys instead.

---

## Security & Failure Modes

- **Metadata Key Collisions**: Storing metadata under generic string keys (like `'roles'` or `'data'`) can collide across third-party libraries. Use uniquely namespaced symbols or descriptive prefixed strings (e.g., `'app:security:roles'`).
- **Missing `reflect-metadata` Import**: If `import 'reflect-metadata';` is omitted at the root of the application entry point, `Reflect.getMetadata` returns `undefined` silently or throws a `TypeError`, breaking the entire routing and dependency injection system.

---

## Anti-Patterns & Common Pitfalls

- **Mutating Metadata at Runtime**: Metadata should be treated as strictly immutable after bootstrap. Mutating metadata objects dynamically during request handling introduces thread-safety race conditions under concurrency.
- **Using `reflector.get` instead of `getAllAndOverride`**: Calling `this.reflector.get(KEY, context.getHandler())` fails to inspect class-level decorators, causing controller-wide security annotations to be bypassed.

---

## Testing Strategies

```typescript
// src/auth/guards/roles.guard.spec.ts
import { RolesGuard } from './roles.guard';
import { Reflector } from '@nestjs/core';
import { ExecutionContext } from '@nestjs/common';

describe('RolesGuard Metadata Resolution', () => {
  let guard: RolesGuard;
  let reflector: Reflector;

  beforeEach(() => {
    reflector = new Reflector();
    guard = new RolesGuard(reflector);
  });

  it('should allow access when route is marked public', () => {
    jest.spyOn(reflector, 'getAllAndOverride').mockReturnValue(true);

    const mockContext = {
      getHandler: () => {},
      getClass: () => {},
      switchToHttp: () => ({ getRequest: () => ({ user: null }) }),
    } as unknown as ExecutionContext;

    expect(guard.canActivate(mockContext)).toBe(true);
  });
});
```

---

## Migration & Version Evolution

- **TC39 Decorator Standard (Stage 3)**: TypeScript 5 introduced native support for stage 3 decorators. However, because standard decorators do not emit parameter type metadata (`design:paramtypes`), NestJS continues to rely on legacy experimental decorators (`"experimentalDecorators": true`, `"emitDecoratorMetadata": true`).

---

## Real-World Case Study: Declarative Audit Logging

An enterprise healthcare platform required HIPAA audit trails on every endpoint accessing protected health information (PHI):
- **Implementation**: Created an `@Audited({ resource: 'PatientRecord', action: 'READ' })` decorator and an `AuditInterceptor`.
- **Execution**: The interceptor inspected `context.getHandler()` via `Reflector`. If present, it emitted structured HIPAA audit events to an encrypted Kafka topic including patient IDs, user identity, and timestamp.
- **Outcome**: Replaced 2,400 lines of repetitive manual audit logging boilerplate across 80 controllers with a single, declarative decorator, achieving 100% compliance verification in external security audits.

---

## Key Takeaways & Cheat Sheet

- NestJS metadata relies on `reflect-metadata` and TypeScript decorator metadata emission.
- Always use `reflector.getAllAndOverride()` when method-level decorators should supersede class-level decorators.
- Use `reflector.getAllAndMerge()` when combining permissions or scopes from both class and method levels.
- Bundle common decorators together using `applyDecorators()`.
- Use `createParamDecorator()` for clean, type-safe extraction of request properties.

---

## Cross-References
- [04-request-lifecycle-internals.md](file:///03-backend/nestjs/06-internals/04-request-lifecycle-internals.md) - How ExecutionContext passes metadata to guards and interceptors
- [06-reflect-metadata-and-decorator-mechanics.md](../../00-prerequisites/06-reflect-metadata-and-decorator-mechanics.md) - Low-level decorator mechanics
- [07-authorization](../../04-intermediate/07-authorization/README.md) - Role-based and claims-based authorization
