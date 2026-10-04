# Controllers

A controller receives incoming requests and returns responses. It is a class decorated with `@Controller()` whose methods, decorated with `@Get()`, `@Post()`, and similar, are bound to routes. Nest reads that metadata at startup to build a routing map. A good controller is thin: it translates HTTP into a method call on a service and the result back into an HTTP response. This file covers routing, route parameters, wildcards, route order and conflicts (with the new NestJS 12 diagnostics), status codes, and the two response modes. Reading request data and shaping responses get their own files next.

---

## Overview

**What it is.** A class annotated with `@Controller(prefix?)`. Each handler method is annotated with an HTTP method decorator and an optional path. The final route is `prefix + path`.

**Why it exists.** To separate the HTTP surface of your application (URLs, methods, status codes) from the logic behind it. Decorators attach routing metadata that Nest turns into registered routes.

**Where it is used.** Every HTTP application. REST and server-rendered endpoints are all controllers. GraphQL uses resolvers, WebSockets use gateways, and microservices use message handlers, which are separate but related concepts.

**Why you should understand it.** Routing mistakes (shadowed routes, missing registration, wrong prefix) are among the most common beginner bugs. Controllers are also where you decide how much framework behavior you keep or give up (`@Res()`).

---

## Mental Model

```text
 HTTP request
   GET /cats/42?verbose=true
        │
        ▼
 ┌─────────────── routing map (built at startup from decorators) ───────────────┐
 │  GET  /cats        → CatsController.findAll                                  │
 │  GET  /cats/:id    → CatsController.findOne      ◄── matches                 │
 │  POST /cats        → CatsController.create                                   │
 └──────────────────────────────────────────────────────────────────────────────┘
        │
        ▼
 CatsController.findOne(@Param('id') id)  →  delegates to CatsService  →  return value
        │
        ▼
 Nest serializes the return value (object → JSON) and sets the status code
```

Route = **controller prefix** + **handler path**. The method name is irrelevant to Nest.

---

## Core Concepts

### `@Controller()` and Route Prefixes

```typescript
import { Controller, Get } from '@nestjs/common';

@Controller('cats')
export class CatsController {
  @Get()
  findAll(): string {
    return 'This action returns all cats';
  }

  @Get('breed')
  findBreeds(): string {
    return 'This action returns all breeds';
  }
}
```

- `GET /cats` maps to `findAll()`.
- `GET /cats/breed` maps to `findBreeds()`.
- `@Controller()` with no argument mounts handlers at the root (`/`).
- The method name is arbitrary. Nest attaches no meaning to it.

### HTTP Method Decorators

| Decorator | HTTP method |
|---|---|
| `@Get()` | `GET` |
| `@Post()` | `POST` |
| `@Put()` | `PUT` |
| `@Patch()` | `PATCH` |
| `@Delete()` | `DELETE` |
| `@Options()` | `OPTIONS` |
| `@Head()` | `HEAD` |
| `@QueryMethod()` | `QUERY` (named to avoid clashing with the `@Query()` parameter decorator) |
| `@All()` | Every method |

Each accepts an optional path (`@Get(':id')`, `@Post('bulk')`).

### Route Parameters

Declare a token with a colon and read it with `@Param()`.

```typescript
@Get(':id')
findOne(@Param('id') id: string): string {
  return `This action returns a #${id} cat`;
}
```

Route parameters are **strings**. Convert and validate them with pipes (see [Request Data](./07-request-data.md)).

### Wildcards

```typescript
@Get('abcd/*')
findAll() {
  return 'This route uses a wildcard';
}
```

`abcd/*` matches `abcd/`, `abcd/123`, `abcd/abc`, and so on. Notes from the official documentation:

- In string-based paths, `-` and `.` are interpreted literally.
- Express v5 made routing stricter and requires named wildcards in plain Express (for example `abcd/{*splat}`). Nest provides a compatibility layer, so an unnamed trailing `*` still works.
- Wildcards in the **middle** of a route need named wildcards in Express (`ab{*splat}cd`). Fastify does not support them at all.

### Route Order and Conflicts

Nest registers routes **in declaration order**. On order-sensitive adapters (Express, the default), a parametric route can silently shadow a more specific one:

```typescript
@Controller('users')
export class UsersController {
  @Get(':id')
  findOne() {}

  @Get('me')          // never reached: ':id' matches "me" first
  findMe() {}
}
```

The app boots without warning and the wrong handler runs. Pipes such as `ParseIntPipe` do not help, because routing selects the handler **before** any pipe runs.

**The simple fix:** declare static paths before parameterized ones.

**NestJS 12 adds two opt-in options** (both default to the previous behavior):

```typescript
const app = await NestFactory.create(AppModule, {
  routeConflictPolicy: { duplicate: 'error', shadow: 'warn' },
  routeResolutionStrategy: 'specificity',
});
```

| Option | Values | Meaning |
|---|---|---|
| `routeConflictPolicy.duplicate` | `'off'` (default), `'warn'`, `'error'` | Two routes share an identical method, path, host, and version |
| `routeConflictPolicy.shadow` | `'off'` (default), `'warn'`, `'error'` | Two route patterns can match the same request (for example `/users/me` and `/users/:id`) |
| `routeResolutionStrategy` | `'declaration'` (default), `'specificity'` | `'specificity'` registers the most specific routes first: literals before parameters before wildcards |

With `'error'`, all conflicts are aggregated into a single `RouteConflictException` thrown when the application initializes. `ExpressAdapter` is order-sensitive. `FastifyAdapter` is not (its router ranks routes by specificity), so on Fastify the `shadow` policy is a no-op and `'specificity'` has no effect, while the `duplicate` policy applies to both. The types `RouteConflictPolicy`, `RouteConflictPolicyLevel`, and `RouteResolutionStrategy` are exported from `@nestjs/common`.

### Status Code, Headers, Redirection (Quick View)

```typescript
@Post()
@HttpCode(204)
@Header('Cache-Control', 'no-store')
create() { /* ... */ }

@Get('docs')
@Redirect('https://docs.nestjs.com', 302)
docs() {}
```

Defaults: status `200`, except `POST`, which is `201`. Details: [Response Handling](./08-response-handling.md).

### Request Data Decorators (Quick View)

| Decorator | Reads |
|---|---|
| `@Param(key?)` | Route parameters |
| `@Query(key?)` | Query string |
| `@Body(key?)` | Request body |
| `@Headers(name?)` | Request headers |
| `@Req()` / `@Request()` | Native request object |
| `@Res()` / `@Response()` | Native response object (library-specific mode) |
| `@Next()` | `next` function |
| `@Session()` | `req.session` |
| `@Ip()` | Client IP |
| `@HostParam()` | Host parameters from sub-domain routing |

Details: [Request Data](./07-request-data.md).

### Two Response Modes

| Mode | How | Behavior |
|---|---|---|
| **Standard** (recommended) | Return a value | Objects and arrays become JSON. Primitives are sent as-is. Status defaults apply. Interceptors, `@HttpCode()`, `@Header()` all work |
| **Library-specific** | Inject `@Res()` (or `@Next()`) | You send the response yourself with the platform object. Nest turns off its standard handling for that route |

If you inject `@Res()` and never send a response, **the request hangs**. To use the platform object while keeping standard handling, set `passthrough: true`:

```typescript
@Get()
findAll(@Res({ passthrough: true }) res: Response) {
  res.status(HttpStatus.OK);   // or set cookies/headers
  return [];                   // Nest still serializes the return value
}
```

### Sub-Domain Routing

```typescript
@Controller({ host: 'admin.example.com' })
export class AdminController {
  @Get()
  index() { return 'Admin page'; }
}

@Controller({ host: ':account.example.com' })
export class AccountController {
  @Get()
  getInfo(@HostParam('account') account: string) { return account; }
}
```

Fastify does not support nested routers, so use Express if you rely on host routing.

### Asynchronicity

Handlers can return values, promises, or RxJS observables. Nest resolves them.

```typescript
@Get()
async findAll(): Promise<Cat[]> {
  return this.catsService.findAll();
}
```

An observable is subscribed to internally and the last emitted value is sent when the stream completes.

### State Sharing

Controllers are singletons, like most providers. Singletons are shared across requests, which is safe in Node.js's single-threaded model as long as they hold no per-request state. Request-scoped controllers are possible but have a cost. See [Scopes and Request Context](../03-core-concepts/04-modules-and-di/07-scopes-and-request-context.md).

### Registration

A controller must be listed in a module's `controllers` array. Otherwise Nest never instantiates it and its routes do not exist.

```typescript
@Module({
  controllers: [CatsController],
  providers: [CatsService],
})
export class CatsModule {}
```

---

## How It Works

```text
 Startup
 ───────
 1. Scanner finds CatsController in CatsModule.controllers
 2. Reads class metadata:    path prefix 'cats'
 3. Reads method metadata:   @Get(':id') → { method: GET, path: ':id' } for findOne
                             @Param('id') → parameter 0 comes from req.params.id
 4. Builds route 'GET /cats/:id' and registers ONE handler with the HTTP adapter

 Request  GET /cats/42
 ───────
 5. Adapter matches the route → calls Nest's route handler
 6. Handler runs the pipeline (middleware already ran; guards, interceptors, pipes)
 7. Resolves arguments from metadata:  id = req.params.id  ('42')
 8. Calls  catsController.findOne('42')
 9. Takes the return value → picks status/headers → serializes → adapter sends the response
```

Registration order in step 4 follows declaration order within a controller, and controllers in the order they appear (unless `routeResolutionStrategy: 'specificity'`).

---

## Basic Example

```typescript
// cats/cats.controller.ts
import { Body, Controller, Get, Param, Post } from '@nestjs/common';
import { CatsService } from './cats.service.js';
import { CreateCatDto } from './dto/create-cat.dto.js';

@Controller('cats')
export class CatsController {
  constructor(private readonly catsService: CatsService) {}

  @Post()
  create(@Body() dto: CreateCatDto) {
    return this.catsService.create(dto);
  }

  @Get()
  findAll() {
    return this.catsService.findAll();
  }

  @Get(':id')
  findOne(@Param('id') id: string) {
    return this.catsService.findOne(id);
  }
}
```

```typescript
// cats/dto/create-cat.dto.ts
export class CreateCatDto {
  name!: string;
  age!: number;
  breed!: string;
}
```

```typescript
// cats/cats.module.ts
@Module({ controllers: [CatsController], providers: [CatsService] })
export class CatsModule {}
```

```bash
curl -i -X POST http://localhost:3000/cats -H 'Content-Type: application/json' \
  -d '{"name":"Tom","age":2,"breed":"Persian"}'      # 201 Created
curl -i http://localhost:3000/cats                    # 200 OK
curl -i http://localhost:3000/cats/1                  # 200 OK
```

What happens:

1. `@Controller('cats')` sets the prefix. The three handlers become `POST /cats`, `GET /cats`, `GET /cats/:id`.
2. The constructor receives `CatsService` by injection.
3. `@Body()` supplies the parsed JSON body, typed as a DTO **class** (a class, not an interface, because Nest needs the runtime type for pipes).
4. The controller does no work itself. It delegates.

---

## Practical Examples

### 1. Basic: A Complete Resource Controller

```typescript
import { Controller, Delete, Get, Param, Patch, Post, Put, Body, Query } from '@nestjs/common';

@Controller('cats')
export class CatsController {
  @Post()               create(@Body() dto: CreateCatDto) { return 'adds a cat'; }
  @Get()                findAll(@Query() query: ListAllEntities) { return `returns cats (limit: ${query.limit})`; }
  @Get(':id')           findOne(@Param('id') id: string) { return `returns cat #${id}`; }
  @Put(':id')           update(@Param('id') id: string, @Body() dto: UpdateCatDto) { return `updates #${id}`; }
  @Delete(':id')        remove(@Param('id') id: string) { return `removes #${id}`; }
}
```

`nest g resource cats` generates this shape for you.

### 2. Common: Declare Static Routes First

```typescript
@Controller('users')
export class UsersController {
  @Get('me')       findMe() { /* ... */ }                       // static first
  @Get(':id')      findOne(@Param('id') id: string) { /* ... */ } // parametric after
}
```

### 3. Common: Detect Shadowed Routes During Development

```typescript
const app = await NestFactory.create(AppModule, {
  routeConflictPolicy: {
    duplicate: 'error',
    shadow: process.env.NODE_ENV === 'production' ? 'off' : 'warn',
  },
});
```

Enable `shadow: 'warn'` locally so conflicts show up at boot, not at runtime.

### 4. Real-World: Let Nest Order Routes by Specificity

```typescript
const app = await NestFactory.create(AppModule, { routeResolutionStrategy: 'specificity' });
```

Now `@Get(':id')` declared before `@Get('me')` still routes `/users/me` to `findMe()`. This is useful when routes come from several controllers whose relative order is hard to control. Behavior only differs on Express.

### 5. Real-World: Global Prefix and Versioning

```typescript
app.setGlobalPrefix('api');          // GET /api/cats
```

```typescript
@Controller({ path: 'cats', version: '1' })    // requires app.enableVersioning(...)
export class CatsV1Controller {}
```

See [Global Prefix](https://docs.nestjs.com/faq/global-prefix) and [API Versioning](../04-intermediate/08-api-design/02-api-versioning.md).

### 6. Real-World: Platform Object Without Losing Nest Features

```typescript
import type { Response } from 'express';

@Get('login-demo')
login(@Res({ passthrough: true }) res: Response) {
  res.cookie('seen', '1', { httpOnly: true, sameSite: 'lax' });
  return { ok: true };            // interceptors, serialization, status defaults still apply
}
```

`import type` is correct for `Response`: it is only used as a type.

### 7. Edge Case: `@Res()` Without Sending

```typescript
@Get()
broken(@Res() res: Response) {
  return { hello: 'world' };      // never sent: standard handling is disabled; request hangs
}
```

Either call `res.json(...)`, or use `@Res({ passthrough: true })`.

### 8. Edge Case: Duplicate Routes

```typescript
@Controller('cats') export class AController { @Get() a() {} }
@Controller('cats') export class BController { @Get() b() {} }   // same method + path
```

Only one wins (the first registered on Express). With `routeConflictPolicy: { duplicate: 'error' }` this fails at boot with a `RouteConflictException`.

### 9. Edge Case: Nested Query Objects

```text
?filter[where][name]=John&item[]=1&item[]=2
```

The default Express query parser does not produce nested objects. Configure the extended parser:

```typescript
const app = await NestFactory.create<NestExpressApplication>(AppModule);
app.set('query parser', 'extended');
```

For Fastify use the `querystringParser` option of `FastifyAdapter` (for example with `qs`).

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `@Controller('path')` / `@Controller({ path, host, version })` | Declare a controller with prefix, host, version |
| `@Get/@Post/@Put/@Patch/@Delete/@Options/@Head/@All/@QueryMethod(path?)` | Declare handler routes |
| `@Param`, `@Query`, `@Body`, `@Headers`, `@Req`, `@Res`, `@Next`, `@Session`, `@Ip`, `@HostParam` | Parameter decorators |
| `@HttpCode(n)`, `@Header(name, value)`, `@Redirect(url, status)` | Response metadata |
| `@Res({ passthrough: true })` | Use the platform response and keep standard handling |
| `NestFactory.create(AppModule, { routeConflictPolicy, routeResolutionStrategy })` | v12 route diagnostics (opt-in) |
| `app.setGlobalPrefix('api')` | Prefix every route |
| `nest g controller <name>` | Generate a controller |
| `nest g resource <name>` | Generate a CRUD resource |

---

## Important Rules

1. **A controller must be in a module's `controllers` array.**
2. **Route = controller prefix + method path.** Method names do not matter.
3. **Declare static routes before parameterized ones** (on Express), or enable `routeResolutionStrategy: 'specificity'`.
4. **Routing happens before pipes.** A pipe cannot rescue a shadowed route.
5. **`POST` defaults to `201`, everything else to `200`.**
6. **Return values for standard mode.** Injecting `@Res()` or `@Next()` disables it unless `passthrough: true`.
7. **If you inject `@Res()` without `passthrough`, you must send the response** or the request hangs.
8. **Route and query parameters are strings.** Convert them with pipes.
9. **Use classes for DTOs.** Interfaces are erased and pipes cannot see them.
10. **Keep controllers thin.** Delegate to services.
11. **Fastify cannot do host-based nested routing, mid-route wildcards, or rely on declaration order.** Check adapter differences when switching.

---

## Under the Hood

### Metadata

`@Controller()` stores the path (and host, version) on the class. `@Get()` stores `{ method, path }` on the method. Parameter decorators store which argument comes from which source. The router explorer reads all of it at startup. See [Request Lifecycle Internals](../06-internals/04-request-lifecycle-internals.md).

### Route Registration Order

Within the scanner, routes are collected per controller in method declaration order and handed to the adapter. On Express, the adapter registers them in that order, which is what makes ordering matter. `routeResolutionStrategy: 'specificity'` sorts collected routes by specificity before registering. Fastify's router (`find-my-way`) ranks by specificity itself.

### Argument Resolution

For each call, Nest builds the argument array from the stored parameter metadata (body, query, params...), runs pipes over each, then invokes your method with them. This is why method signatures are the "contract" and decorators are the wiring.

### Library-Specific Mode Detection

When `@Res()` or `@Next()` appears in a handler's parameter metadata (without `passthrough`), Nest marks the route to skip its own response writing. That is why interceptors that map the return value and `@HttpCode()` / `@Header()` stop working there.

### Express 5 Compatibility Layer

The default platform uses Express v5 routing, which rejects some older path patterns. Nest translates common patterns (like a trailing `*`) so existing routes keep working. Pure Express path-to-regexp syntax can still differ. *Verify unusual patterns against your installed Express version.*

---

## Common Patterns

### Thin Controller

```typescript
@Get(':id')
findOne(@Param('id', ParseIntPipe) id: number) {
  return this.service.findOne(id);
}
```

### Resource-Style Controllers

One controller per resource with the standard five handlers (`GET` list, `GET` one, `POST`, `PUT`/`PATCH`, `DELETE`).

### Controller per Version or Audience

`UsersV1Controller`, `UsersV2Controller`, or `AdminUsersController` with a `host` or prefix when audiences differ.

### Sub-Resource Routing

```typescript
@Controller('users/:userId/orders')
export class UserOrdersController {
  @Get() list(@Param('userId') userId: string) { /* ... */ }
}
```

Keep nesting shallow (one level).

### Pass-Through for Cookies and Headers

`@Res({ passthrough: true })` when you only need the platform object for a side effect.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Controller not in `controllers` | `404 Cannot GET /x` | Never instantiated | Register it in a module |
| `@Get(':id')` before `@Get('me')` | `/me` handled by the wrong method | Express registers in declaration order | Reorder, or enable `routeResolutionStrategy: 'specificity'` |
| Injecting `@Res()` and returning a value | Request hangs or empty response | Standard handling disabled | `res.send(...)`, or `passthrough: true` |
| Using `@Res()` for everything | Interceptors, `@HttpCode`, serialization stop working | Library-specific mode | Return values instead |
| Interface as DTO type | Validation never runs | Interface erased | Use a class |
| Treating `@Param('id')` as a number | `'1' + 1 === '11'`, broken comparisons | Params are strings | `ParseIntPipe` (see [Request Data](./07-request-data.md)) |
| Wrong prefix | Unexpected URL | Forgot global prefix or versioning | Check `setGlobalPrefix`, `enableVersioning`, controller path |
| Duplicate route in two controllers | One handler never called | Same method and path | Remove duplicate or set `duplicate: 'error'` |
| Logic in the controller | Hard to test, reuse | Controller doing service work | Move it to a service |
| Relying on host routing with Fastify | Routes missing | Fastify lacks nested routers | Use Express or restructure |
| Nested query objects not parsed | `req.query` flat | Default parser | `app.set('query parser', 'extended')` |
| Mid-route wildcard on Fastify | Route fails to register | Unsupported | Redesign the route |

---

## Debugging

| Symptom | Check |
|---|---|
| `404 Cannot GET /path` | Controller registered? Module imported from root? Prefix/version? HTTP method? |
| Wrong handler runs | Declaration order. Enable `routeConflictPolicy: { shadow: 'warn' }` |
| Request hangs | `@Res()` without sending or `passthrough` |
| `400 Bad Request` on body routes | Invalid JSON or missing `Content-Type: application/json` |
| Params wrong type | Missing pipes |
| Startup `RouteConflictException` | Read the aggregated list of duplicate/shadowed routes |

```bash
curl -v http://localhost:3000/users/me
```

With the `debug` / `verbose` log level, Nest logs each mapped route at startup:

```text
[RoutesResolver] CatsController {/cats}:
[RouterExplorer] Mapped {/cats, GET} route
[RouterExplorer] Mapped {/cats/:id, GET} route
```

(Format varies by version.) Compare these lines to what you expect. See [Debugging](../01-getting-started/06-debugging.md).

---

## Performance

- Controller dispatch overhead is small. Handler time is dominated by what the service does.
- Singleton controllers have no per-request construction cost. Request-scoped controllers instantiate per request (and make every dependent request-scoped).
- Returning large objects costs JSON serialization time on the event loop. Paginate. See [Node.js Async and Event Loop](../00-prerequisites/03-nodejs-async-and-event-loop.md).
- Streaming (`StreamableFile`) avoids loading large files into memory.
- Express route matching is linear in the number of routes before the match. Fastify's router is faster for large route tables. Measure if you have thousands of routes.

---

## Security

- **A route that exists is exposed.** Nothing is protected by default. Apply guards (preferably global with opt-outs).
- **Never trust route, query, or body input.** Validate with DTO classes and pipes.
- **Shadowed routes can bypass guards you thought applied.** If `:id` captures `admin` and runs an unguarded handler, you have a hole. Use conflict diagnostics in development.
- **`@Res()` bypasses interceptors and filters' response handling** (including serialization that strips sensitive fields). Avoid it unless needed.
- **`@All()` accepts every method.** Use it deliberately.
- **Do not echo unvalidated input in responses** (injection, XSS in HTML responses).
- **Sub-domain routing relies on the `Host` header,** which clients control. It is routing, not authorization.

---

## Production Considerations

- **Enable conflict diagnostics** (`duplicate: 'error'`, `shadow: 'warn'` or `'error'`) so routing mistakes fail the build or deployment.
- **Standardize URL design** (plural nouns, shallow nesting). See [REST and Resource Design](../04-intermediate/08-api-design/01-rest-and-resource-design.md).
- **Version intentionally** before you have external consumers. See [API Versioning](../04-intermediate/08-api-design/02-api-versioning.md).
- **Document routes** with OpenAPI. See [Swagger Setup](../04-intermediate/09-openapi-and-swagger/01-swagger-setup.md).
- **Observe per route.** Route patterns (`GET /cats/:id`) are the right unit for latency and error metrics. See [Metrics](../07-production/03-observability/05-metrics.md).
- **Keep handlers fast and non-blocking.** Offload slow work to queues.

---

## Best Practices

### Recommended

```typescript
@Controller('tasks')
export class TasksController {
  constructor(private readonly tasks: TasksService) {}

  @Get('mine')                                   // static first
  mine(@CurrentUser() user: User) { return this.tasks.forUser(user.id); }

  @Get(':id')
  findOne(@Param('id', ParseUUIDPipe) id: string) { return this.tasks.findOne(id); }

  @Post()
  create(@Body() dto: CreateTaskDto) { return this.tasks.create(dto); }
}
```

### Avoid

```typescript
@Controller('tasks')
export class TasksController {
  @Get(':id')                                    // shadows 'mine'
  findOne(@Req() req: Request, @Res() res: Response) {
    const id = Number(req.params.id);            // manual conversion, platform-bound
    // ...queries the database, builds JSON, sets headers by hand
    res.status(200).json({ id });
  }

  @Get('mine') mine() { /* never reached on Express */ }
}
```

Why: the recommended controller is thin, ordered correctly, validated by pipes, and platform-independent. The avoided one is order-dependent, platform-bound, and loses Nest's response features.

Additional guidance:

- Prefer returning values. Use `@Res({ passthrough: true })` when you need the platform object.
- Use one controller per resource, one module per feature.
- Use DTO classes for bodies and queries.
- Pick one status-code convention and apply it consistently.
- Do not name handler methods after HTTP verbs just because you can (`getOne` over `get`). Names help stack traces and logs.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `routeConflictPolicy`, `routeResolutionStrategy` | New, opt-in | Not available |
| `@QueryMethod()` | Available (maps to the HTTP `QUERY` method) | Not available |
| `schema` option on `@Body()`, `@Query()`, `@Param()`, `@RawBody()` | New (Standard Schema) | Not available |
| Express version | Express v5 routing semantics (named wildcards, with Nest's compatibility layer) | Express 4 before Nest 11 |
| `@Cookies()`, `@SignedCookies()` | Listed in the official controllers decorator table. See the Cookies chapter for setup. *Verify availability for your version* | Cookie access commonly via `req.cookies` or custom decorators |
| `HttpException` options `errorCode` | New | Not available |
| `decorator` schematic output | `Reflector.createDecorator()` form | Older template |
| ESM projects | `.js` extensions in relative imports | No extensions |

*Verify against the official controllers chapter and migration guide for your installed version.*

---

## Real-World Use Cases

- **REST resource endpoints** for users, orders, products.
- **BFF/gateway controllers** that aggregate several services.
- **Webhook receivers** (raw body access, signature verification).
- **File download and upload endpoints.**
- **Admin APIs** on a separate host or prefix.
- **Health and readiness endpoints.**
- **Versioned public APIs** running `v1` and `v2` side by side.

---

## Interview Questions

### Beginner

1. What does `@Controller('cats')` do?
   - Declares a controller and sets the route prefix `cats` for its handlers.
2. How do you define a `GET /cats/:id` handler?
   - `@Get(':id')` on a method, with `@Param('id')` to read the value.
3. What is the default status code for `POST`?
   - `201`. Other methods default to `200`.
4. What must you do for Nest to know a controller exists?
   - Add it to the `controllers` array of a module.

### Intermediate

1. Standard vs library-specific response handling?
   - Standard: return a value and Nest serializes and sends it. Library-specific: inject `@Res()`/`@Next()` and send it yourself, which disables standard handling unless `passthrough: true`.
2. Why might `GET /users/me` hit the `:id` handler?
   - Express matches in registration order, so a parametric route declared first shadows the static one.
3. Why use DTO classes instead of interfaces?
   - Interfaces are erased at compile time. Pipes and validators need the runtime class.
4. How do you read the Express request object?
   - `@Req()`, typically typed with `import type { Request } from 'express'`.
5. What do `@Get('abcd/*')` and Express 5 have to do with each other?
   - Express 5 requires named wildcards in plain Express, but Nest's compatibility layer still accepts a trailing `*`.

### Advanced

1. How does NestJS 12 help with route conflicts?
   - `routeConflictPolicy` reports duplicate and shadowed routes at bootstrap (`'off'`, `'warn'`, `'error'`), and `routeResolutionStrategy: 'specificity'` registers more specific routes first. Both are opt-in, and shadow handling matters only on Express.
2. Why can't a pipe fix a shadowed route?
   - Routing selects the handler before pipes run.
3. What does `passthrough: true` change?
   - You get the platform response object for side effects (cookies, headers) while Nest still handles the return value, interceptors, and decorators.
4. How does Nest turn decorators into routes?
   - It reads controller and method metadata at startup, builds route definitions, and registers handlers with the HTTP adapter.
5. What are the risks of `@Res()` in a codebase that wants platform independence?
   - It ties code to Express or Fastify APIs, complicates testing, and bypasses interceptors and standard response features.

---

## Quick Reference

```text
@Controller('prefix')                      route prefix (also { path, host, version })
@Get/@Post/@Put/@Patch/@Delete/@Options/@Head/@QueryMethod/@All('path')
Route                                       prefix + path  (method name irrelevant)
Registered in                               Module.controllers
Defaults                                    200, POST → 201
Return value                                object/array → JSON · primitive → as-is · Promise/Observable resolved
@Res() / @Next()                            library mode → you must send (or passthrough: true)
Order (Express)                             declaration order → static before :param
v12 options                                 routeConflictPolicy { duplicate, shadow: off|warn|error }
                                            routeResolutionStrategy: 'declaration' | 'specificity'
Fastify                                     ranks by specificity; no nested routers; no mid-route wildcards
DTOs                                        classes, not interfaces
Rule                                        thin controller → service does the work
```

---

## Key Takeaways

- A controller maps `prefix + path + HTTP method` to a handler. Method names are irrelevant.
- It must be registered in a module, or its routes do not exist.
- Return values for standard behavior. Injecting `@Res()` switches to manual mode and disables Nest's response handling unless you use `passthrough: true`.
- On Express, declaration order matters: static routes before parameterized ones. NestJS 12 adds opt-in conflict diagnostics and specificity-based ordering.
- Routing runs before pipes, so conversion and validation cannot fix a wrongly routed request.
- Keep controllers thin. DTOs are classes. Parameters are strings until converted.
- Fastify differs from Express in routing order, host routing, and wildcard support. Check before switching.

---

## Related Topics

```text
02 Modules
      ↓
[03 Controllers]
      ↓
04 Providers and Services  →  07 Request Data  →  08 Response Handling
```

- [Fundamentals Overview](./README.md)
- [Modules](./02-modules.md)
- [Providers and Services](./04-providers-and-services.md)
- [Request Data](./07-request-data.md)
- [Response Handling](./08-response-handling.md)
- [Request Lifecycle](./09-request-lifecycle.md)
- [REST and Resource Design](../04-intermediate/08-api-design/01-rest-and-resource-design.md)
- [API Versioning](../04-intermediate/08-api-design/02-api-versioning.md)
- [Pipes](../03-core-concepts/01-request-pipeline/03-pipes.md)
