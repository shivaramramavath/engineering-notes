# Decorators

Almost everything you write in NestJS is a decorated class: `@Module`, `@Controller`, `@Injectable`, `@Get`, `@Body`, `@UseGuards`. A Nest decorator is rarely a piece of logic. It is a **label** that records metadata, which the framework reads later to wire routes, inject dependencies, and run the request pipeline. This file is a catalog of the built-in decorators, organized by what they attach to, with a clear answer to the question that matters most: **who reads this metadata, and what happens if nothing does?** The language mechanics are in [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md). Writing your own is in [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

---

## Overview

**What it is.** Nest decorators are TypeScript (legacy `experimentalDecorators`) decorators exported by `@nestjs/common` and other `@nestjs/*` packages. Most only store metadata with `reflect-metadata`. A few also change behavior.

**Why it exists.** Decorators keep configuration next to the code it describes: the route sits on the method, the dependency on the constructor parameter, the guard on the controller. No separate routing or wiring files.

**Where it is used.** Every Nest class. Third-party modules (`@nestjs/swagger`, `@nestjs/typeorm`, `class-validator`) use the same mechanism.

**Why you should understand it.** Two beliefs cause most decorator bugs: "the decorator enforces it" (it only declares it) and "order does not matter" (sometimes it does). Knowing what reads each decorator makes behavior predictable.

---

## Mental Model

```text
  YOUR CODE                     MODULE LOAD                     STARTUP / REQUEST
  ─────────                     ───────────                     ─────────────────
  @Controller('cats')    ──►    writes metadata        ──►      scanner / router / injector /
  class CatsController {        to the class                    guard / pipe READS it and ACTS
    @Get(':id')          ──►    writes metadata
    find(@Param('id') id)       to the method + param index
  }
```

A decorator is a sticky note. The framework is the person who reads the notes and does the work. If nobody reads a note, nothing happens (for example `@Roles('admin')` without a guard that reads it).

---

## Core Concepts

### The Four Places a Decorator Can Sit

| Target | Example | Typical purpose |
|---|---|---|
| **Class** | `@Controller()`, `@Injectable()`, `@Module()` | Declare what the class *is* |
| **Method** | `@Get()`, `@HttpCode()`, `@UseGuards()` | Declare how a handler behaves |
| **Parameter** | `@Body()`, `@Param()`, `@Inject()` | Declare where an argument comes from |
| **Property** | `@Inject()` (property injection), validation decorators | Declare injection or rules for a field |

### Class Decorators

| Decorator | Declares | Read by |
|---|---|---|
| `@Module({...})` | A module and its imports, controllers, providers, exports | Module scanner |
| `@Controller(prefix \| { path, host, version })` | A controller and route prefix | Router explorer |
| `@Injectable({ scope? })` | An injectable provider (and its scope) | DI container |
| `@Global()` | A module whose exports are global | Container |
| `@Catch(...types)` | An exception filter and the exceptions it handles | Exception zone |
| `@WebSocketGateway()`, `@Resolver()` | Gateways and GraphQL resolvers | Respective modules |

### Method Decorators

| Decorator | Declares | Read by |
|---|---|---|
| `@Get()`, `@Post()`, `@Put()`, `@Patch()`, `@Delete()`, `@Options()`, `@Head()`, `@All()` | HTTP method and path | Router explorer |
| `@QueryMethod()` | The HTTP `QUERY` method | Router explorer |
| `@HttpCode(n)` | Success status code | Response handling |
| `@Header(name, value)` | A response header | Response handling |
| `@Redirect(url, status)` | A redirect | Response handling |
| `@Render(view)` | A view template (MVC) | Response handling |
| `@Sse(path)` | A server-sent events endpoint | Router explorer |
| `@Version(v)` | Route version | Versioning |
| `@UseGuards(...)`, `@UsePipes(...)`, `@UseInterceptors(...)`, `@UseFilters(...)` | Pipeline components for this handler | Pipeline wiring |
| `@SetMetadata(key, value)` | Arbitrary metadata | `Reflector` in your guards/interceptors |

`@UseGuards`, `@UsePipes`, `@UseInterceptors`, and `@UseFilters` can also decorate a **class** to apply to every handler in it.

### Parameter Decorators

| Decorator | Supplies | Notes |
|---|---|---|
| `@Param(key?)` | `req.params` / one param | Strings until converted |
| `@Query(key?)` | `req.query` / one value | Strings until converted |
| `@Body(key?)` | `req.body` / one field | Use DTO **classes** |
| `@Headers(name?)` | `req.headers` / one header | Header names are lower-case |
| `@Req()` / `@Request()` | Native request | Platform-bound |
| `@Res()` / `@Response()` | Native response | Library-specific mode |
| `@Next()` | `next` | Library-specific mode |
| `@Session()` | `req.session` | Needs session middleware |
| `@Ip()` | Client IP | Behind a proxy, configure trust |
| `@HostParam(key?)` | Host parameters | Sub-domain routing |
| `@UploadedFile()`, `@UploadedFiles()` | Uploaded files | With file interceptors |
| `@RawBody()` | Raw body | Needs raw-body enabled |
| `@Inject(token)` | A provider by token | Constructor parameter or property |
| `@Optional()` | Marks a dependency optional | Constructor parameter. v12: not inherited |

`@Body()`, `@Query()`, `@Param()`, and `@RawBody()` also accept an options object with `schema` (a Standard Schema) and `pipes` in NestJS 12. `@Cookies()` and `@SignedCookies()` appear in the official decorator table. See the Cookies chapter for setup, and verify availability for your version.

### Metadata Decorators and `Reflector`

`@SetMetadata(key, value)` stores data on a handler or class. A guard, interceptor, or filter reads it with `Reflector`.

```typescript
import { SetMetadata } from '@nestjs/common';

export const ROLES_KEY = 'roles';
export const Roles = (...roles: string[]) => SetMetadata(ROLES_KEY, roles);
```

NestJS 12's `nest generate decorator` scaffolds the typed `Reflector.createDecorator()` form:

```typescript
import { Reflector } from '@nestjs/core';

export const Roles = Reflector.createDecorator<string[]>();
```

Reading it in a guard:

```typescript
@Injectable()
export class RolesGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(context: ExecutionContext): boolean {
    const roles = this.reflector.get(Roles, context.getHandler());   // typed: string[] | undefined
    if (!roles) return true;
    const { user } = context.switchToHttp().getRequest();
    return roles.some((r) => user?.roles?.includes(r));
  }
}
```

Usage:

```typescript
@Roles(['admin'])
@Delete(':id')
remove(@Param('id') id: string) { /* ... */ }
```

(With `createDecorator`, the argument is the typed payload you declared, here an array.) The `@SetMetadata` form with `this.reflector.get(ROLES_KEY, context.getHandler())` is equivalent and still common. Reading and combining metadata across handler and class (`getAllAndOverride`, `getAllAndMerge`) is covered in [Execution Context](../03-core-concepts/01-request-pipeline/09-execution-context.md).

### Enhancer Decorators and the Pipeline

`@UseGuards`, `@UsePipes`, `@UseInterceptors`, and `@UseFilters` bind pipeline components at three levels:

```typescript
@UseGuards(AuthGuard)                 // class level: every handler in the controller
@Controller('cats')
export class CatsController {
  @UseGuards(RolesGuard)              // method level: this handler only
  @Delete(':id')
  remove(@Param('id') id: string) {}
}
```

Plus **global** registration (`app.useGlobalGuards(...)` or the `APP_GUARD` provider token). Execution order across levels: [Request Lifecycle](./09-request-lifecycle.md).

### Composition

Combine several decorators into one:

```typescript
import { applyDecorators, UseGuards } from '@nestjs/common';

export function Auth(...roles: string[]) {
  return applyDecorators(Roles(roles), UseGuards(AuthGuard, RolesGuard));
}
```

### Custom Parameter Decorators

```typescript
import { createParamDecorator, ExecutionContext } from '@nestjs/common';

export const CurrentUser = createParamDecorator(
  (data: string | undefined, ctx: ExecutionContext) => {
    const user = ctx.switchToHttp().getRequest().user;
    return data ? user?.[data] : user;
  },
);

// usage
@Get('me')
me(@CurrentUser() user: User, @CurrentUser('email') email: string) {}
```

Details: [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

### Decorators From Other Packages

| Package | Examples | Reads it |
|---|---|---|
| `class-validator` | `@IsString()`, `@IsEmail()`, `@ValidateNested()` | `ValidationPipe` |
| `class-transformer` | `@Type()`, `@Exclude()`, `@Expose()`, `@Transform()` | `ValidationPipe` (transform), `ClassSerializerInterceptor` |
| `@nestjs/swagger` | `@ApiProperty()`, `@ApiTags()`, `@ApiResponse()` | `SwaggerModule` |
| `@nestjs/typeorm` / `typeorm` | `@Entity()`, `@Column()`, `@InjectRepository()` | ORM and Nest integration |
| `@nestjs/mongoose` | `@Schema()`, `@Prop()`, `@InjectModel()` | Mongoose integration |
| `@nestjs/schedule` | `@Cron()`, `@Interval()` | Schedule module |
| `@nestjs/microservices` | `@MessagePattern()`, `@EventPattern()` | Microservice server |

---

## How It Works

Two phases, strictly separated:

```text
 MODULE LOAD (once, when each file is imported)
 ──────────────────────────────────────────────
 class body evaluated
   → parameter and member decorators run first (per member)
   → class decorators run last
   → each decorator calls Reflect.defineMetadata(...)  (no behavior yet)
   → compiler-emitted design:paramtypes / design:type / design:returntype stored (decorated declarations only)

 STARTUP / REQUEST (later)
 ─────────────────────────
 scanner reads @Module, @Controller          → builds module graph and routes
 injector reads design:paramtypes, @Inject   → builds providers
 router reads @Get, @Param, @HttpCode        → registers handlers and argument sources
 pipeline reads @UseGuards, @SetMetadata...  → runs guards/pipes/interceptors; Reflector exposes metadata
```

The decorator itself ran at module load. The **effect** happens when the matching component reads the metadata.

Stacked decorators on one declaration: factories are evaluated top to bottom, the resulting decorators are applied bottom to top. Class decorators run after member decorators.

---

## Basic Example

One controller showing decorators on all four targets, with a note on who reads each:

```typescript
import {
  Body, Controller, Get, HttpCode, Param, Post, SetMetadata, UseGuards,
} from '@nestjs/common';

@Controller('articles')                         // class → router: prefix "articles"
@UseGuards(AuthGuard)                           // class → pipeline: guard for every handler
export class ArticlesController {
  constructor(private readonly articles: ArticlesService) {}   // @Injectable on the service enabled this

  @Get(':id')                                   // method → router: GET /articles/:id
  find(@Param('id') id: string) {               // param → router: arg 0 from req.params.id
    return this.articles.find(id);
  }

  @Post()                                       // method → router: POST /articles
  @HttpCode(202)                                // method → response: status 202
  @SetMetadata('roles', ['editor'])             // method → Reflector: read by RolesGuard
  create(@Body() dto: CreateArticleDto) {       // param → router: arg 0 from req.body
    return this.articles.create(dto);
  }
}
```

What happens:

1. Class decorators register the controller and mark a guard.
2. `@Get`/`@Post` register routes. `@Param`/`@Body` define argument sources.
3. `@HttpCode` changes the response status. `@SetMetadata` does **nothing** on its own, because it only matters if a guard or interceptor reads `'roles'` with `Reflector`.
4. `@UseGuards(AuthGuard)` does not authenticate by itself. It registers `AuthGuard`, whose `canActivate()` does the work at request time.

---

## Practical Examples

### 1. Basic: Request Parameters

```typescript
@Get(':id')
findOne(@Param('id') id: string, @Query('verbose') verbose?: string, @Headers('x-request-id') rid?: string) {}
```

### 2. Common: A Public-Route Opt-Out Decorator

```typescript
export const IS_PUBLIC = 'isPublic';
export const Public = () => SetMetadata(IS_PUBLIC, true);

@Injectable()
export class AuthGuard implements CanActivate {
  constructor(private readonly reflector: Reflector) {}

  canActivate(context: ExecutionContext) {
    const isPublic = this.reflector.getAllAndOverride<boolean>(IS_PUBLIC, [
      context.getHandler(),
      context.getClass(),
    ]);
    if (isPublic) return true;
    // ...normal authentication
    return false;
  }
}

@Public()
@Get('health')
health() { return { ok: true }; }
```

Register `AuthGuard` globally and opt out routes with `@Public()`. Secure by default.

### 3. Common: Compose Decorators

```typescript
export const ApiAuth = (...roles: string[]) =>
  applyDecorators(Roles(roles), UseGuards(AuthGuard, RolesGuard), ApiBearerAuth());
```

### 4. Real-World: Validation Decorators on a DTO

```typescript
export class CreateUserDto {
  @IsEmail()               email!: string;
  @IsString() @MinLength(8) password!: string;
  @IsOptional() @IsString() nickname?: string;
}
```

These record rules. Nothing validates until `ValidationPipe` is applied (globally or on the route). See [Validation Pipe](../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md).

### 5. Real-World: Property Injection

```typescript
@Injectable()
export class BaseRepository {
  @Inject('DB') protected readonly db!: Database;
}
```

Useful in base classes, otherwise prefer constructor injection.

### 6. Edge Case: Metadata With No Reader

```typescript
@Roles(['admin'])          // records metadata
@Delete(':id')
remove() {}                // no RolesGuard is registered → ANYONE can call it
```

`@Roles` is intent. A guard is enforcement. Always test that a protected route rejects an unauthorized caller.

### 7. Edge Case: Decorator Order

```typescript
@UseGuards(A)
@UseGuards(B)
@Get()
handler() {}
```

Multiple `@UseGuards` decorators and the arguments of a single `@UseGuards(A, B)` are not interchangeable to reason about. Prefer one `@UseGuards(A, B)`, where guards run left to right, to avoid depending on stacked-decorator evaluation order.

### 8. Edge Case: `@Res()` Disables Other Response Decorators

```typescript
@Get()
@HttpCode(201)                       // ignored in library-specific mode
@Header('X-Demo', '1')               // ignored in library-specific mode
broken(@Res() res: Response) { res.json({}); }
```

Use `@Res({ passthrough: true })` to keep them.

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `SetMetadata(key, value)` | Store metadata on a handler or class |
| `Reflector.createDecorator<T>()` | Typed metadata decorator (generated by `nest g decorator` in v12) |
| `Reflector#get(keyOrDecorator, target)` | Read metadata from one target |
| `Reflector#getAllAndOverride(key, [handler, class])` | First defined value, handler before class |
| `Reflector#getAllAndMerge(key, [handler, class])` | Merge values from handler and class |
| `applyDecorators(...decorators)` | Compose decorators |
| `createParamDecorator((data, ctx) => ...)` | Custom parameter decorator |
| `UseGuards`, `UsePipes`, `UseInterceptors`, `UseFilters` | Bind pipeline components |
| `nest g decorator <name>` | Generate a custom decorator |

---

## Important Rules

1. **Decorators describe, components act.** A decorator without a reader does nothing.
2. **They run at module load, not per request.** Only the components that read the metadata run per request.
3. **Authorization decorators are not enforcement.** A guard must exist and be registered.
4. **Prefer global guards with explicit opt-outs** (`@Public()`) over per-route opt-ins you can forget.
5. **Classes, not interfaces, for anything a decorator or pipe must inspect** (DTOs).
6. **`@Res()` and `@Next()` switch off standard response handling** and the decorators that rely on it, unless `passthrough: true`.
7. **Use parentheses** (`@Injectable()`, `@Controller()`). Most Nest decorators are factories.
8. **Metadata keys are global strings.** Export them as constants and namespace them to avoid collisions.
9. **Keep Nest on legacy decorators.** `experimentalDecorators` and `emitDecoratorMetadata` must stay on. Do not mix in standard decorators.
10. **Do not put logic in decorator factories that depends on request or instance state.** The factory runs once, at load time.

---

## Under the Hood

### Where Metadata Lives

Nest uses `reflect-metadata` with internal keys (`path`, `method`, `__guards__`, `design:paramtypes`, and so on) on classes, prototypes, and method descriptors. `Reflector` is a thin, typed wrapper that reads it. Internal keys are implementation details, so do not depend on their names.

### How `@UseGuards` Works

It writes the guard class(es) into metadata on the target. At startup the pipeline builder collects guards from global, controller, and handler metadata and, for each request, runs them in order. Guard classes are resolved through the DI container, which is why guards can inject services.

### How Parameter Decorators Reach Your Argument

`@Param('id')` records `{ index, type: ROUTE_PARAM, key: 'id' }` for the method. At request time Nest builds the argument array from that record, runs pipes per argument, and calls your method.

### Metadata Emitted by the Compiler

`@Injectable()` (or any decorator) on a class triggers `design:paramtypes`. A decorator on a method or property triggers `design:type`/`design:returntype`/`design:paramtypes` for it. This is how `@Body() dto: CreateUserDto` lets the pipe know the DTO class.

### Legacy vs Standard Decorators

Nest's generated `tsconfig.json` keeps `experimentalDecorators` and `emitDecoratorMetadata` on, including for new NestJS 12 ESM projects. TypeScript 5's standard decorators have different signatures and no parameter decorators or `design:*` emission, so they do not work with Nest's metadata approach. See [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md).

---

## Common Patterns

### Metadata + Guard

`@Roles()`/`@Public()`/`@Permissions()` plus a guard reading them with `Reflector`.

### Composite Decorators

`@Auth()` = metadata + guards + docs decorators in one.

### Param Decorators for Request Context

`@CurrentUser()`, `@TenantId()`, `@RequestId()`: small, typed accessors over the request.

### Class-Level Defaults, Method-Level Overrides

`@UseGuards` on the controller, `@Public()` on a handler. `getAllAndOverride` expresses the override.

### Declarative DTOs

Validation and transformation rules as property decorators on DTO classes.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| `@Roles()` without a guard | Endpoint is open | Metadata only | Register a guard that reads it |
| Guard registered but metadata key mismatch | Guard never sees roles | Different key/string | Export key constants, use `Reflector.createDecorator` |
| Missing parentheses (`@Injectable`) | Odd errors or no effect | Factory not called | Use `@Injectable()` |
| `@Res()` plus `@HttpCode()`/`@Header()` | Decorators ignored | Library-specific mode | `@Res({ passthrough: true })` |
| Interface as DTO | No validation | Erased type | Use a class |
| Validation decorators but no `ValidationPipe` | Invalid data accepted | Decorators only declare rules | Apply `ValidationPipe` |
| Relying on stacked `@UseGuards` order | Surprising order | Evaluation is bottom to top | One `@UseGuards(A, B)` call |
| Removing `emitDecoratorMetadata` | DI and pipes break | No metadata | Keep it `true` |
| `import type` on a DTO/injected class | Pipes/DI see `Object` | Import erased | Normal import |
| Stateful logic in a decorator factory | State shared across everything | Factory runs once | Move state to providers |
| Mixing standard and legacy decorators | Signature mismatch | Different systems | Stay on legacy for Nest |
| Reading metadata from the wrong target | `undefined` | Handler vs class mix-up | `getAllAndOverride` with `[handler, class]` |

---

## Debugging

### Inspect Metadata

```typescript
import 'reflect-metadata';
console.log(Reflect.getMetadataKeys(CatsController.prototype, 'findOne'));
console.log(Reflect.getMetadata('design:paramtypes', CatsService));
```

### Questions to Ask

```text
□ Did the decorator run (module imported)? Add a log in a custom decorator factory.
□ Who is supposed to READ this metadata? Is that component registered and active?
□ Same key on both sides? Handler vs class target?
□ Is the guard/pipe registered globally, or bound where I expected?
□ Is the route in library-specific mode (@Res without passthrough)?
□ Does the compiled output contain __decorate and __metadata calls? (build/transformer problem)
```

### Tests

Write a test that calls the route without credentials and asserts a `401`/`403`. This is the only reliable check that "decorator + guard" actually enforces something. See [E2E Testing](../04-intermediate/01-testing/06-e2e-testing.md).

---

## Performance

- Decorators cost startup time only (metadata writes, scanning). Per-request cost comes from the components they enable (guards, pipes, interceptors).
- `Reflector` lookups happen per request in guards and interceptors. Metadata reads are cheap, but avoid heavy computation around them.
- Compose decorators with `applyDecorators` for readability. The runtime cost is the same as applying them separately.
- Wrapping method decorators (custom ones that replace the method) add a call layer. Avoid on hot paths and prefer metadata-only decorators.

---

## Security

- **Decorators declare intent, not protection.** An `@Roles('admin')` that no guard reads is a vulnerability disguised as a feature.
- **Secure by default:** global `APP_GUARD` plus `@Public()` opt-outs, so a new route is protected until someone consciously opens it.
- **Check the whole chain:** decorator → metadata key → `Reflector` read → guard registration → test.
- **Class-level guards apply to later-added methods automatically.** Method-level guards do not.
- **Do not trust decorator input derived from the request** (for example a custom `@Param`-like decorator returning raw headers). Validate it.
- **Validation decorators protect only validated paths.** Routes that bypass `ValidationPipe` (`@Res()` raw usage, microservice handlers without pipes) are unprotected.

---

## Production Considerations

- **Keep metadata keys centralized** and exported, so refactors do not silently break guards.
- **Test enforcement**, not just behavior: unauthenticated and unauthorized calls must fail.
- **Prefer a small set of well-named, composable decorators** (`@Auth()`, `@Public()`, `@CurrentUser()`) over scattering `@UseGuards(...)` combos.
- **Review generated decorators** from `nest g decorator`. v12 emits the `Reflector.createDecorator()` form.
- **Watch builder changes.** If you switch to SWC/Rspack, verify decorator metadata still emits (DI, pipes, validation all rely on it).

---

## Best Practices

### Recommended

```typescript
export const Roles = Reflector.createDecorator<string[]>();      // typed, single source of truth
export const Public = Reflector.createDecorator<void>();

// global guard registered via APP_GUARD, reads Public and Roles with getAllAndOverride
@Roles(['admin'])
@Delete(':id')
remove(@Param('id', ParseUUIDPipe) id: string) {}
```

### Avoid

```typescript
@SetMetadata('role', 'admin')            // untyped string key, different spelling elsewhere ('roles')
@Delete(':id')
remove(@Param('id') id: string) {}       // and no guard reads 'role'

@Injectable                              // missing parentheses
export class Svc {}
```

Why: typed metadata decorators remove key typos and make the reader's expectations explicit. Raw strings drift, and missing parentheses silently fail.

Additional guidance:

- Name decorators for intent (`@Public`, `@Roles`, `@CurrentUser`), not mechanism (`@MetaHelper`).
- Keep decorators single-purpose and compose them.
- Prefer metadata-only decorators to wrapping ones.
- Document, next to each metadata decorator, which component reads it.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `nest g decorator` output | `Reflector.createDecorator()` form | `SetMetadata` form |
| `@QueryMethod()` | Available | Not available |
| `@Body()` / `@Query()` / `@Param()` / `@RawBody()` options | `{ schema, pipes }` (Standard Schema) | Pipes only |
| `@Optional()` inheritance | Not inherited | Inherited |
| Decorator model | Legacy (`experimentalDecorators`, `emitDecoratorMetadata`) in generated `tsconfig` | Same |
| `isolatedModules` in generated ESM tsconfig | On: classes use normal imports, interfaces and types use `import type` | n/a |
| `@Cookies()` / `@SignedCookies()` | In the official decorator table. *Verify for your version* | Custom decorators or `req.cookies` |

*Verify against the official controllers, custom decorators, and migration chapters.*

---

## Real-World Use Cases

- **Authentication and authorization:** `@Public()`, `@Roles()`, `@Permissions()`, `@CurrentUser()`.
- **API documentation:** `@ApiProperty()`, `@ApiTags()`, `@ApiBearerAuth()`.
- **Validation and serialization:** class-validator and class-transformer decorators on DTOs.
- **Caching and throttling:** `@CacheTTL()`, `@Throttle()`, `@SkipThrottle()`.
- **Scheduling and messaging:** `@Cron()`, `@MessagePattern()`, `@EventPattern()`, `@Process()`.
- **Multi-tenancy:** `@TenantId()` param decorators and tenant metadata.
- **ORM mapping:** `@Entity()`, `@Column()`, `@Schema()`, `@Prop()`.

---

## Interview Questions

### Beginner

1. What is a decorator in NestJS?
   - A `@name` annotation that attaches metadata (or behavior) to a class, method, parameter, or property, which Nest reads to wire the application.
2. Name decorators for a class, a method, and a parameter.
   - `@Controller()` (class), `@Get()` (method), `@Body()` (parameter).
3. What does `@Injectable()` do?
   - Marks a class as injectable and triggers constructor metadata emission.
4. What does `@UseGuards(AuthGuard)` do?
   - Binds the guard to the controller or handler. The guard runs at request time.

### Intermediate

1. Does `@Roles('admin')` protect a route by itself?
   - No. It records metadata. A guard must read it (via `Reflector`) and be registered.
2. How do you read custom metadata inside a guard?
   - Inject `Reflector` and call `get`, `getAllAndOverride`, or `getAllAndMerge` with the key or typed decorator and targets `[context.getHandler(), context.getClass()]`.
3. When does a decorator run?
   - At module load, when the class is defined. Per-request behavior comes from components reading the metadata.
4. What is `applyDecorators` for?
   - Composing several decorators into one reusable decorator.
5. Why must DTOs be classes?
   - Interfaces are erased. Pipes and validators need the runtime class and its decorator metadata.

### Advanced

1. How does `@Param('id')` end up supplying an argument?
   - It records an argument descriptor (index, source, key) in metadata. At request time Nest builds the argument array from it, runs pipes, and calls the method.
2. Why does Nest still use legacy decorators with TypeScript 5?
   - It depends on parameter decorators and `emitDecoratorMetadata`, which standard decorators do not provide.
3. What are the trade-offs between `SetMetadata` and `Reflector.createDecorator`?
   - `createDecorator` is typed and avoids key typos. `SetMetadata` is simpler but untyped and string-keyed.
4. How would you make an API secure by default?
   - Register a global guard (`APP_GUARD`) and use a `@Public()` decorator that the guard reads with `getAllAndOverride` for explicit opt-outs.
5. What breaks if you change the builder to one that does not emit decorator metadata?
   - DI (`design:paramtypes`), pipes (parameter metatypes), and validation, usually surfacing as unresolved dependencies or skipped validation.

---

## Quick Reference

```text
Class       @Module @Controller @Injectable @Global @Catch @WebSocketGateway
Method      @Get @Post @Put @Patch @Delete @Options @Head @All @QueryMethod
            @HttpCode @Header @Redirect @Render @Sse @Version @SetMetadata
            @UseGuards @UsePipes @UseInterceptors @UseFilters
Parameter   @Param @Query @Body @Headers @Req @Res @Next @Session @Ip @HostParam
            @UploadedFile(s) @RawBody @Inject @Optional
Metadata    SetMetadata / Reflector.createDecorator<T>()  →  Reflector.get / getAllAndOverride / getAllAndMerge
Compose     applyDecorators(...)   ·   Param decorator: createParamDecorator((data, ctx) => ...)
Runs when   module load (writes metadata) → components read it at startup/request
Rule        decorator = intent; guard/pipe/interceptor = enforcement
Order       factories top→bottom, applied bottom→top; class decorators last
Needs       experimentalDecorators + emitDecoratorMetadata (keep on)
```

---

## Key Takeaways

- Nest decorators are mostly labels. Components such as the router, injector, guards, and pipes read them and act.
- Decorators run once, at module load. Request-time behavior comes from the components that read the metadata.
- A decorator without a reader does nothing. `@Roles('admin')` protects nothing unless a registered guard enforces it.
- Prefer global guards with `@Public()` opt-outs, typed metadata decorators, and `applyDecorators` composition.
- `@Res()` and `@Next()` disable standard response handling and the decorators that depend on it, unless `passthrough: true`.
- DTOs and injected types must be classes with normal imports so metadata survives compilation.
- Nest depends on legacy decorators and `emitDecoratorMetadata`, including in NestJS 12 projects.

---

## Related Topics

```text
05 Dependency Injection
      ↓
[06 Decorators]
      ↓
07 Request Data  →  08 Response Handling  →  09 Request Lifecycle
      ↓
03-core-concepts/01 Custom Decorators, Guards, Execution Context
```

- [Fundamentals Overview](./README.md)
- [Dependency Injection](./05-dependency-injection.md)
- [Request Data](./07-request-data.md)
- [Request Lifecycle](./09-request-lifecycle.md)
- [TypeScript Decorators](../00-prerequisites/02-typescript-decorators.md)
- [Custom Decorators](../03-core-concepts/01-request-pipeline/10-custom-decorators.md)
- [Guards](../03-core-concepts/01-request-pipeline/04-guards.md)
- [Execution Context](../03-core-concepts/01-request-pipeline/09-execution-context.md)
- [Metadata and Reflection](../06-internals/03-metadata-and-reflection.md)
