# Request Data

Every request carries data in five places: the **path** (route parameters), the **query string**, the **body**, the **headers**, and the **cookies**. NestJS gives you a decorator for each (`@Param()`, `@Query()`, `@Body()`, `@Headers()`, and cookie access) and a standard way to convert and validate what arrives (pipes and DTO classes). The central fact to internalize: **everything arriving over HTTP is untrusted text.** Route and query values are strings, bodies are whatever the client sent, and a TypeScript type annotation does not change that at runtime. This file covers how to read each source, how to turn raw input into typed, validated values, and the traps around query parsing, boolean conversion, and body limits.

---

## Overview

**What it is.** Parameter decorators that tell Nest where each handler argument comes from, plus pipes that transform and validate the values before your handler runs.

**Why it exists.** Handlers should receive clean, typed arguments, not raw `req` objects. Declaring sources with decorators keeps handlers platform-independent and testable, and routing the values through pipes centralizes conversion and validation.

**Where it is used.** Every endpoint that accepts input: filters and pagination (`@Query`), resource ids (`@Param`), create/update payloads (`@Body`), auth tokens and request IDs (`@Headers`), sessions (`@Req`/cookies), webhooks (raw body).

**Why you should understand it.** Input handling is where most security bugs and "works in Postman, fails in the app" issues originate: unvalidated bodies, string-vs-number surprises, mass assignment, oversized payloads, and spoofable headers.

---

## Mental Model

```text
  Raw HTTP request
  ┌────────────────────────────────────────────────────────────────┐
  │ POST /users/42/orders?page=2&status=paid HTTP/1.1              │
  │ Authorization: Bearer ...        Cookie: sid=abc               │
  │ Content-Type: application/json                                 │
  │                                                                │
  │ {"items":[{"sku":"A1","qty":2}]}                               │
  └──────┬──────────────┬──────────────┬────────────┬──────────────┘
         │ path         │ query        │ headers/   │ body
         ▼              ▼              ▼ cookies    ▼
     @Param('id')   @Query()       @Headers()   @Body()
         │              │              │            │
         └───── strings / untrusted objects ────────┘
                         │
                         ▼
                  PIPES: convert + validate   (ParseIntPipe, ValidationPipe, schema)
                         │
                         ▼
        handler(id: number, query: PaginationQueryDto, dto: CreateOrderDto)
```

Rule of thumb: **decorator = where it comes from, pipe = what it must look like.**

---

## Core Concepts

### The Decorators

| Decorator | Source | Without a key | With a key |
|---|---|---|---|
| `@Param(key?)` | Route path tokens | `req.params` object | `req.params[key]` |
| `@Query(key?)` | Query string | `req.query` object | `req.query[key]` |
| `@Body(key?)` | Request body | `req.body` | `req.body[key]` |
| `@Headers(name?)` | Headers | `req.headers` | `req.headers[name]` (lower-case names) |
| `@Req()` / `@Request()` | Native request | The whole `req` | n/a |
| `@Ip()` | Client address | `req.ip` | n/a |
| `@HostParam(key?)` | Host tokens (sub-domain routing) | `req.hosts` | one token |
| `@Session()` | `req.session` | Session object | n/a |
| `@RawBody()` | Raw body | Raw bytes (needs raw body enabled) | n/a |
| `@Cookies(name?)` / `@SignedCookies(name?)` | Cookies | All cookies | One cookie. *Listed in the official docs, see the Cookies chapter and verify for your version* |

### Route Parameters (`@Param`)

```typescript
@Get(':userId/orders/:orderId')
findOrder(@Param('userId') userId: string, @Param('orderId') orderId: string) {}

@Get(':id')
findOne(@Param() params: { id: string }) {}     // whole object
```

Always `string`. Convert with a pipe:

```typescript
@Get(':id')
findOne(@Param('id', ParseIntPipe) id: number) {}
@Get(':id')
findOne(@Param('id', ParseUUIDPipe) id: string) {}
```

### Query Parameters (`@Query`)

```typescript
@Get()
findAll(@Query('age') age: string, @Query('breed') breed: string) {}
// GET /cats?age=2&breed=Persian  →  age === '2' (a string), breed === 'Persian'
```

The `number` annotation does **not** convert. Use a pipe, or a DTO with `transform`:

```typescript
@Get()
findAll(@Query('age', ParseIntPipe) age: number) {}
```

**Query string parsing depends on the platform's parser:**

| Input | Default Express (simple parser) | With `extended` parser / `qs` |
|---|---|---|
| `?a=1` | `{ a: '1' }` | same |
| `?a=1&a=2` | `{ a: ['1','2'] }` | same |
| `?item[]=1&item[]=2` | not parsed as an array under the simple parser | `{ item: ['1','2'] }` |
| `?filter[where][name]=John` | not nested under the simple parser | nested object |

To enable nested objects and bracket arrays:

```typescript
const app = await NestFactory.create<NestExpressApplication>(AppModule);
app.set('query parser', 'extended');
```

Fastify:

```typescript
import qs from 'qs';
new FastifyAdapter({ querystringParser: (str) => qs.parse(str) });
```

Without a parser change, the same parameter can be a string **or** an array depending on what the client sends (`?a=1` vs `?a=1&a=2`). Validate the shape.

### Request Body (`@Body`)

```typescript
@Post()
create(@Body() dto: CreateCatDto) {}
@Post()
create(@Body('name') name: string) {}
```

The platform parses JSON and URL-encoded bodies by default, based on `Content-Type`. A missing or wrong `Content-Type` is the top cause of "my body is empty". Multipart uploads need dedicated handling ([File Upload](../05-advanced/07-integrations/01-file-upload.md)).

### DTOs: Classes, Not Interfaces

A **Data Transfer Object** describes the shape of incoming data.

```typescript
export class CreateCatDto {
  name!: string;
  age!: number;
  breed!: string;
}
```

Use **classes**. Interfaces are erased at compile time, so pipes cannot inspect them at runtime, and features such as validation rely on the class as a runtime value. Import DTO classes with a normal `import`. Details: [DTOs](../03-core-concepts/02-validation-and-serialization/01-dto.md).

### Pipes: Conversion and Validation

Pipes run **after** guards and **before** your handler, on each argument.

| Built-in pipe | Does |
|---|---|
| `ParseIntPipe` | String to integer (400 if not numeric) |
| `ParseFloatPipe` | String to float |
| `ParseBoolPipe` | `'true'`/`'false'` to boolean |
| `ParseArrayPipe` | Parse and validate arrays (optionally split on a separator) |
| `ParseUUIDPipe` | Validate a UUID (with optional version) |
| `ParseEnumPipe` | Validate against an enum |
| `ParseDatePipe` | String to `Date` |
| `DefaultValuePipe` | Supply a default when the value is `undefined` |
| `ValidationPipe` | Validate and (optionally) transform against a DTO class |
| `ParseFilePipe` | Validate uploaded files |

Pipes compose left to right:

```typescript
@Get()
findAll(
  @Query('page', new DefaultValuePipe(1), ParseIntPipe) page: number,
  @Query('active', new DefaultValuePipe(false), ParseBoolPipe) active: boolean,
) {}
```

### Validating DTOs With `ValidationPipe`

Install the libraries `class-validator` and `class-transformer`, decorate the DTO, and enable the pipe.

```typescript
import { IsEmail, IsOptional, IsString, MinLength } from 'class-validator';

export class CreateUserDto {
  @IsEmail()                 email!: string;
  @IsString() @MinLength(8)  password!: string;
  @IsOptional() @IsString()  nickname?: string;
}
```

```typescript
// main.ts
app.useGlobalPipes(
  new ValidationPipe({
    whitelist: true,                // strip properties without validation decorators
    forbidNonWhitelisted: true,     // reject (400) instead of silently stripping
    transform: true,                // convert payloads into DTO instances (and primitive params to declared types)
  }),
);
```

| Option | Effect |
|---|---|
| `whitelist` | Remove properties not declared in the DTO (prevents mass assignment) |
| `forbidNonWhitelisted` | With `whitelist`, throw instead of stripping |
| `transform` | Convert plain objects to DTO class instances, and params to their declared primitive types |
| `transformOptions.enableImplicitConversion` | Convert by declared type without `@Type()`. **Caution:** it uses `Boolean(value)`, so `'false'` becomes `true` |
| `stopAtFirstError` | Report one error per property |
| `disableErrorMessages` | Hide validation details (production hardening) |
| `errorHttpStatusCode` | Change the status for validation failure (default `400`) |

Full treatment: [Validation Pipe](../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md), [class-validator](../03-core-concepts/02-validation-and-serialization/03-class-validator.md), [class-transformer](../03-core-concepts/02-validation-and-serialization/04-class-transformer.md).

### Standard Schema Validation (NestJS 12)

`@Body()`, `@Query()`, `@Param()`, and `@RawBody()` accept an options object with `schema` and `pipes`, so any [Standard Schema](https://standardschema.dev/)-compatible library (Zod, Valibot, ArkType) can describe the input.

```typescript
import { z } from 'zod';

const createCatSchema = z.object({ name: z.string(), age: z.number().int(), breed: z.string() });

@Post()
create(@Body({ schema: createCatSchema }) createCatDto: CreateCatDto) {
  return this.catsService.create(createCatDto);
}

@Get(':id')
findOne(@Param('id', { schema: z.coerce.number().int().positive() }) id: number) {
  return this.catsService.findOne(id);
}
```

On their own these options only **attach the schema as metadata**. To validate, register the built-in `StandardSchemaValidationPipe` (or a custom pipe that reads `metadata.schema`). See [Schema Validation Alternatives](../03-core-concepts/02-validation-and-serialization/08-schema-validation-alternatives.md).

### Headers

```typescript
@Get()
list(@Headers('x-request-id') requestId?: string, @Headers() all?: Record<string, string | string[] | undefined>) {}
```

Node.js lower-cases header names. Header values can be `string | string[] | undefined`. Treat all headers as client-controlled, including `X-Forwarded-For`, `Host`, and `User-Agent`.

### Cookies

Using the platform request is the long-standing approach:

```typescript
import cookieParser from 'cookie-parser';
app.use(cookieParser());

@Get()
read(@Req() req: Request) {
  return req.cookies['sid'];
}
```

The official docs also list `@Cookies()` and `@SignedCookies()` decorators in the decorator table and a Cookies chapter with setup. *Verify the exact setup for your installed version.* Setting cookies is a response concern: [Response Handling](./08-response-handling.md).

### Raw Body

Webhook signature checks need the exact bytes the client sent.

```typescript
const app = await NestFactory.create<NestExpressApplication>(AppModule, { rawBody: true });

@Post('webhook')
handle(@Req() req: RawBodyRequest<Request>) {
  const raw = req.rawBody;          // Buffer
}
```

`@RawBody()` exposes the same data through a parameter decorator (raw body must be enabled). See the Raw body FAQ in the official docs.

### Client IP and Proxies

`@Ip()` returns `req.ip`. Behind a reverse proxy or load balancer that is the proxy's address unless you configure trust:

```typescript
app.set('trust proxy', 1);      // Express: trust one proxy hop; use the right value for YOUR topology
```

Trusting `X-Forwarded-For` blindly lets clients spoof their IP.

### Platform Objects (`@Req()`)

`@Req()` gives the native request. Type it with `import type { Request } from 'express'` (and install `@types/express`). Use it sparingly: it ties the handler to the platform and complicates tests. Prefer dedicated decorators or a custom parameter decorator.

---

## How It Works

For each request, Nest builds the argument list from the handler's parameter metadata:

```text
 handler: create(@Param('id', ParseIntPipe) id: number, @Body() dto: CreateOrderDto)

 metadata recorded at load time:
   arg 0 → source: params, key: 'id',  pipes: [ParseIntPipe],  metatype: Number
   arg 1 → source: body,   key: none,  pipes: [],              metatype: CreateOrderDto

 per request (after guards, interceptors-before):
   1. extract raw values:      arg0 ← req.params.id ('42')      arg1 ← req.body
   2. run pipes per argument:  global pipes → controller → handler → parameter-level
        arg0: ParseIntPipe('42') → 42
        arg1: ValidationPipe(body, metatype=CreateOrderDto) → validate, strip, transform
   3. any pipe throws? → 400 via exception filters; the handler never runs
   4. call handler(42, dtoInstance)
```

`metatype` comes from `design:paramtypes`, which is why DTOs must be classes imported with a normal `import`.

---

## Basic Example

```typescript
// dto/list-tasks.query.ts
import { Type } from 'class-transformer';
import { IsIn, IsInt, IsOptional, Max, Min } from 'class-validator';

export class ListTasksQuery {
  @IsOptional() @IsIn(['open', 'done'])           status?: 'open' | 'done';
  @IsOptional() @Type(() => Number) @IsInt() @Min(1)             page: number = 1;
  @IsOptional() @Type(() => Number) @IsInt() @Min(1) @Max(100)   limit: number = 20;
}
```

```typescript
// dto/create-task.dto.ts
import { IsString, MaxLength, MinLength } from 'class-validator';

export class CreateTaskDto {
  @IsString() @MinLength(1) @MaxLength(200) title!: string;
}
```

```typescript
// tasks.controller.ts
@Controller('tasks')
export class TasksController {
  constructor(private readonly tasks: TasksService) {}

  @Get()
  list(@Query() query: ListTasksQuery) {
    return this.tasks.list(query.status, query.page, query.limit);
  }

  @Get(':id')
  findOne(@Param('id', ParseIntPipe) id: number) {
    return this.tasks.findOne(id);
  }

  @Post()
  create(@Body() dto: CreateTaskDto) {
    return this.tasks.create(dto.title);
  }
}
```

```typescript
// main.ts
app.useGlobalPipes(new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }));
```

```bash
curl 'http://localhost:3000/tasks?status=done&page=2'           # 200, page is the number 2
curl 'http://localhost:3000/tasks?limit=1000'                   # 400, limit must not be greater than 100
curl http://localhost:3000/tasks/abc                            # 400, Validation failed (numeric string is expected)
curl -X POST http://localhost:3000/tasks -H 'Content-Type: application/json' -d '{"title":"x","admin":true}'
                                                                # 400, property admin should not exist
```

What happens:

1. Query values are strings. `@Type(() => Number)` plus `transform: true` converts `page` and `limit`, and the validators then check ranges.
2. `ParseIntPipe` converts the route parameter or rejects it with `400`.
3. `whitelist` plus `forbidNonWhitelisted` rejects the unexpected `admin` property (mass-assignment protection).
4. Validation errors never reach your handler.

---

## Practical Examples

### 1. Basic: Read Each Source

```typescript
@Post(':id')
demo(
  @Param('id') id: string,
  @Query('dry') dry: string,
  @Headers('user-agent') ua: string,
  @Body('name') name: string,
  @Ip() ip: string,
) {}
```

### 2. Common: Defaults and Parsing

```typescript
@Get()
list(
  @Query('page', new DefaultValuePipe(1), ParseIntPipe) page: number,
  @Query('sort', new DefaultValuePipe('createdAt')) sort: string,
) {}
```

`DefaultValuePipe` must come before `ParseIntPipe`, because `ParseIntPipe` would reject `undefined`.

### 3. Common: Comma-Separated and Repeated Values

```typescript
@Get()
byIds(
  @Query('ids', new ParseArrayPipe({ items: Number, separator: ',' })) ids: number[],
) {}
// GET /items?ids=1,2,3  →  [1, 2, 3]
```

Without the pipe, `?ids=1,2,3` is the single string `'1,2,3'`, and `?ids=1&ids=2` is an array. Normalize explicitly.

### 4. Common: Enum and UUID Params

```typescript
enum Status { Open = 'open', Done = 'done' }

@Get(':id')
find(@Param('id', new ParseUUIDPipe({ version: '4' })) id: string,
     @Query('status', new ParseEnumPipe(Status)) status: Status) {}
```

### 5. Real-World: Partial Updates That Reuse the Create DTO

```typescript
import { PartialType } from '@nestjs/mapped-types';

export class UpdateTaskDto extends PartialType(CreateTaskDto) {}

@Patch(':id')
update(@Param('id', ParseIntPipe) id: number, @Body() dto: UpdateTaskDto) {}
```

`PartialType` keeps validation decorators and marks fields optional. (If you use `@nestjs/swagger`, import mapped types from there so OpenAPI metadata is preserved.)

### 6. Real-World: Standard Schema With Zod

```typescript
const listSchema = z.object({ status: z.enum(['open', 'done']).optional(), page: z.coerce.number().int().min(1).default(1) });

@Get()
list(@Query({ schema: listSchema }) query: z.infer<typeof listSchema>) {}
```

Register `StandardSchemaValidationPipe` (globally or per route) so the schema is enforced.

### 7. Real-World: Verify a Webhook Signature

```typescript
@Post('stripe')
async webhook(@Req() req: RawBodyRequest<Request>, @Headers('stripe-signature') sig: string) {
  const event = this.stripe.verify(req.rawBody!, sig);    // needs the exact bytes
  return { received: true };
}
```

Parsing the body into an object and re-stringifying it will not reproduce the signed bytes.

### 8. Real-World: A Typed Request-Context Decorator

```typescript
export const TenantId = createParamDecorator((_: unknown, ctx: ExecutionContext): string =>
  ctx.switchToHttp().getRequest().headers['x-tenant-id'],
);

@Get() list(@TenantId() tenant: string) {}
```

Validate or derive the tenant from a trusted source (the authenticated user), not only from a client header.

### 9. Edge Case: Booleans From Query Strings

```text
GET /items?active=false
```

```typescript
// Wrong: 'false' is a non-empty string, so it is truthy
@Query('active') active: string  →  if (active) { /* runs for 'false' too */ }

// Right
@Query('active', new DefaultValuePipe(false), ParseBoolPipe) active: boolean

// DTO pitfall: enableImplicitConversion turns 'false' into true (Boolean('false'))
@Transform(({ value }) => value === 'true' || value === true)
active?: boolean;
```

### 10. Edge Case: Number Conversion Surprises

```typescript
Number('')        // 0       (an empty query value becomes 0)
Number(' 12 ')    // 12
Number('1e3')     // 1000
Number('0x10')    // 16
parseInt('12abc') // 12
```

`ParseIntPipe` is strict (it rejects `'12abc'`). Hand-written `Number(x)` is not. Validate ranges (`@Min`, `@Max`) after conversion.

### 11. Edge Case: Extra Properties Silently Pass Through

```typescript
// no whitelist configured
@Post() create(@Body() dto: CreateUserDto) { return this.users.create(dto); }
// body {"email":"a@b.c","password":"...","isAdmin":true}  →  dto.isAdmin is present if you spread dto into the database
```

This is **mass assignment**. Enable `whitelist: true` (and ideally `forbidNonWhitelisted: true`), and never spread untrusted objects into persistence calls.

### 12. Edge Case: Array vs String Ambiguity

```text
GET /items?tag=a            → { tag: 'a' }
GET /items?tag=a&tag=b      → { tag: ['a','b'] }
```

```typescript
@Transform(({ value }) => (Array.isArray(value) ? value : value === undefined ? [] : [value]))
@IsString({ each: true })
tag: string[] = [];
```

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `@Param`, `@Query`, `@Body`, `@Headers`, `@Req`, `@Ip`, `@HostParam`, `@Session`, `@RawBody` | Parameter decorators |
| `@Body({ schema, pipes })` (and `@Query`, `@Param`, `@RawBody`) | NestJS 12: attach a Standard Schema and/or pipes |
| `ParseIntPipe`, `ParseFloatPipe`, `ParseBoolPipe`, `ParseArrayPipe`, `ParseUUIDPipe`, `ParseEnumPipe`, `ParseDatePipe`, `DefaultValuePipe`, `ParseFilePipe` | Conversion and validation pipes |
| `ValidationPipe`, `StandardSchemaValidationPipe` | DTO and schema validation |
| `app.useGlobalPipes(...)` | Apply pipes to all routes |
| `app.set('query parser', 'extended')` | Nested and bracket query parsing (Express) |
| `FastifyAdapter({ querystringParser })` | Custom query parsing (Fastify) |
| `NestFactory.create(AppModule, { rawBody: true })` | Keep raw bodies |
| `RawBodyRequest<Request>` | Typed request with `rawBody` |
| `app.useBodyParser('json', { limit: '5mb' })` | Adjust body size limits (`NestExpressApplication`). *Verify for your version* |
| `app.set('trust proxy', n)` | Trust proxy hops for IP and protocol (Express) |
| `@nestjs/mapped-types`: `PartialType`, `PickType`, `OmitType`, `IntersectionType` | Derive DTO classes |

---

## Important Rules

1. **All HTTP input is untrusted.** Validate every source: body, query, params, headers, cookies.
2. **Params, query values, and header values are strings (or arrays).** A TypeScript `number` annotation converts nothing.
3. **Use classes for DTOs, with normal imports.** Interfaces and `import type` erase the runtime information pipes need.
4. **Enable `ValidationPipe` globally** with `whitelist: true` (and consider `forbidNonWhitelisted: true`) to prevent mass assignment.
5. **Pipes run after routing and guards.** A pipe cannot fix a wrongly routed request, and unauthenticated users are rejected before validation runs.
6. **Order pipes deliberately.** `DefaultValuePipe` before `ParseIntPipe`.
7. **`'false'` is truthy.** Convert booleans explicitly. Beware `enableImplicitConversion`.
8. **Query shape depends on the parser.** Normalize or validate arrays and nested objects.
9. **Set body size limits** at the application and at the proxy.
10. **Headers and IPs are client-controlled** unless they come from your trusted proxy.
11. **Webhook signature checks need the raw body.**
12. **`@Req()` ties you to the platform.** Prefer dedicated or custom parameter decorators.
13. **NestJS 12 `schema` options only attach metadata.** A validation pipe must be registered to enforce them.

---

## Under the Hood

### Where Each Source Comes From

| Decorator | Express | Fastify |
|---|---|---|
| `@Param` | `req.params` | `req.params` |
| `@Query` | `req.query` (parser: `query parser` setting) | `req.query` (parser: `querystringParser`) |
| `@Body` | `req.body` (body parsers) | `req.body` (content-type parsers) |
| `@Headers` | `req.headers` | `req.headers` |

Nest reads from the native request. The adapter parses bodies and queries before the Nest pipeline begins.

### Parameter Metadata

Each parameter decorator writes `{ index, type, data }` (type = body, query, param...) into route-arguments metadata. `ValidationPipe` receives an `ArgumentMetadata` object `{ type, metatype, data }`, where `metatype` comes from `design:paramtypes`. If `metatype` is `Object`, `String`, `Number`, `Boolean`, `Array`, or `Function`, `ValidationPipe` skips class validation (nothing to validate against). That is why an interface-typed `@Body()` is not validated, and why a primitive `@Query('page') page: number` is only converted when `transform: true`.

### How `transform: true` Converts Primitives

With `transform: true`, `ValidationPipe` converts `@Param()`/`@Query()` primitives to their **declared** primitive type using the metatype (`Number('2')`, `Boolean('false')`, `String(...)`). That is why `Boolean('false') === true` surprises people, and why explicit pipes or `@Transform` are safer for booleans.

### Body Parsing and Limits

JSON and URL-encoded bodies are parsed by the platform's body parsers according to `Content-Type`. Express's JSON parser applies a default size limit (commonly 100 KB). Larger bodies are rejected with `413`. *Verify the default for your installed version.* Fastify has its own `bodyLimit`.

### Order of Pipe Execution

For one argument: global pipes, then controller-level, then handler-level, then parameter-level, each in the order you listed them. See [Request Lifecycle](./09-request-lifecycle.md).

### Standard Schema Flow (NestJS 12)

`@Body({ schema })` stores the schema on the argument metadata. `StandardSchemaValidationPipe` reads `metadata.schema`, runs the schema's `~standard.validate()` (the shared Standard Schema interface), and either returns the parsed value or throws a `400` with the issues.

---

## Common Patterns

### Query DTO for List Endpoints

A `PaginationQueryDto` (page, limit, sort, filters) reused across controllers.

### Global `ValidationPipe`

One configuration in `main.ts`. Routes opt out only deliberately.

### Derived DTOs

`PartialType`, `PickType`, `OmitType` for update, patch, and projection DTOs.

### Parameter Decorators for Context

`@CurrentUser()`, `@TenantId()`, `@RequestId()` hide request plumbing from handlers.

### Strict Boundary, Trusted Interior

Validate once at the edge. Inside services, work with typed, validated objects.

### Schema-First Alternatives

Zod/Valibot/ArkType with the NestJS 12 `schema` option when you prefer schemas to class decorators.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Treating `@Param('id') id: number` as a number | `'1' + 1 === '11'`, failed comparisons | Params are strings | `ParseIntPipe` or `transform: true` |
| Interface as the DTO type | No validation, no stripping | Interfaces erased, metatype is `Object` | Use a class |
| `import type` on a DTO class | Validation skipped | Metatype becomes `Object` | Normal import |
| No `ValidationPipe` registered | Invalid data reaches services | Decorators only declare rules | `app.useGlobalPipes(new ValidationPipe(...))` |
| No `whitelist` | Mass assignment, e.g. `isAdmin: true` persisted | Extra properties pass through | `whitelist: true`, `forbidNonWhitelisted: true` |
| `?active=false` handled as truthy | Filter always applied | Non-empty string | `ParseBoolPipe` or explicit `@Transform` |
| `enableImplicitConversion` with booleans | `'false'` becomes `true` | Uses `Boolean(value)` | Explicit `@Transform` |
| Empty body in handler | `dto` is `{}` or `undefined` | Missing `Content-Type: application/json` or unsupported type | Send the right header, check the parser |
| `?ids=1,2,3` treated as an array | Only one element | It is one string | `ParseArrayPipe({ separator: ',' })` |
| Nested query objects flat/absent | `filter[where]` not parsed | Simple query parser | `app.set('query parser', 'extended')` |
| `ParseIntPipe` then `DefaultValuePipe` | `400` when param absent | Pipe order | `DefaultValuePipe` first |
| Webhook verification fails | Signature mismatch | Body parsed and re-serialized | Use the raw body |
| Trusting `X-Forwarded-For` | Spoofed IPs defeat rate limiting | Client-set header | Configure trusted proxies |
| No body limit awareness | `413` or memory pressure | Defaults or no proxy limit | Set explicit limits at app and proxy |
| `@Req()` everywhere | Hard to test, platform-bound | Convenience | Use decorators and DTOs |
| `schema` option with no pipe (v12) | Invalid input accepted | Option only attaches metadata | Register `StandardSchemaValidationPipe` |

---

## Debugging

### Common Responses

| Response | Likely cause |
|---|---|
| `400 Validation failed (numeric string is expected)` | `ParseIntPipe` rejected a non-numeric param |
| `400` with an array of messages | `ValidationPipe` DTO failures: read the messages |
| `400 property X should not exist` | `forbidNonWhitelisted` and an unexpected property |
| `413 Payload Too Large` | Body exceeds app or proxy limit |
| `415 Unsupported Media Type` | Content type the parser does not handle |
| Handler receives `undefined` for body | Wrong `Content-Type`, or no body parser |

### Techniques

```bash
curl -i -X POST http://localhost:3000/tasks -H 'Content-Type: application/json' -d '{"title":"x"}'
curl -i 'http://localhost:3000/tasks?page=abc'
curl -i -X POST http://localhost:3000/tasks -d 'title=x'        # form-encoded: does your app accept it?
```

```typescript
// Temporarily inspect exactly what arrives and what the pipe sees
@Post() create(@Body() dto: CreateTaskDto) {
  console.log(dto instanceof CreateTaskDto, dto);                // false means no transform
  return dto;
}
```

```typescript
console.log(Reflect.getMetadata('design:paramtypes', TasksController.prototype, 'create'));
// [ [class CreateTaskDto] ]   healthy · [ [Function: Object] ] interface / import type
```

Checklist: right `Content-Type`? `ValidationPipe` registered and given `transform`? DTO a class with a normal import? Decorators present on every property (with `whitelist`, undecorated properties are stripped)? See [Debugging](../01-getting-started/06-debugging.md).

---

## Performance

- **Body parsing** is synchronous per chunk and can block on large payloads. Enforce limits and prefer streaming for large uploads.
- **Validation cost** grows with DTO size and nesting. For hot endpoints keep DTOs flat and avoid heavy custom validators (especially async ones that hit the database).
- **`transform: true`** instantiates DTO classes per request (small cost).
- **`stopAtFirstError`** reduces work on invalid input.
- **Extended query parsing** (`qs`) costs more than the simple parser and is a larger attack surface. Enable it only if you need nested queries.
- **Large arrays in query strings** inflate URLs. Move big filters into a `POST` body.

---

## Security

- **Mass assignment:** always whitelist. Never spread request bodies into ORM calls.
- **Injection:** validation is not sanitization. Use parameterized queries and encode output. See [Injection and XSS Prevention](../07-production/01-security/05-injection-and-xss-prevention.md).
- **Prototype pollution and parameter pollution:** nested parsers and `Object.assign` on untrusted input can create `__proto__` or duplicated-key surprises. Keep `qs` current, validate shapes, and avoid merging untrusted objects into configuration.
- **Resource exhaustion:** limit body size, array lengths (`@ArrayMaxSize`), string lengths (`@MaxLength`), and nesting depth.
- **Header trust:** `Host`, `X-Forwarded-*`, and custom headers are client-controlled. Derive tenant and identity from authenticated context, not headers alone.
- **Validation error detail:** verbose messages help attackers map your schema. Consider `disableErrorMessages` or generic messages in production.
- **IDs:** validating UUID format does not prove authorization. Check ownership separately.
- **Webhooks:** verify signatures over the raw body with a constant-time comparison, and reject stale timestamps.
- **Cookies:** read only `HttpOnly; Secure; SameSite` cookies for sessions, and verify signatures for signed cookies.

---

## Production Considerations

- **Register `ValidationPipe` globally** in the same bootstrap function used by e2e tests, so tests exercise real validation.
- **Set limits consistently:** application body limit, reverse proxy `client_max_body_size`-style limit, and per-route overrides for uploads.
- **Configure proxy trust** correctly so rate limiting and logging see the real client IP.
- **Standardize validation error format** (see [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md)).
- **Document inputs** with OpenAPI (DTO classes and the Swagger plugin generate most of it). See [DTO Documentation](../04-intermediate/09-openapi-and-swagger/03-dto-documentation.md).
- **Monitor `4xx` rates** per route. A spike in `400`s often signals a client release or an attack.
- **Version DTOs** when changing contracts. Add fields, avoid changing meanings.

---

## Best Practices

### Recommended

```typescript
app.useGlobalPipes(new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true, transform: true }));

@Get(':id')
findOne(@Param('id', ParseUUIDPipe) id: string) {}

@Get()
list(@Query() query: ListTasksQuery) {}               // DTO class with validators and @Type

@Post()
create(@Body() dto: CreateTaskDto) {}                 // class, normal import
```

### Avoid

```typescript
@Get(':id')
findOne(@Req() req: Request) {
  const id = Number(req.params.id);                   // manual, platform-bound, no validation
}

interface CreateTaskDto { title: string }             // erased
@Post() create(@Body() dto: CreateTaskDto) { return this.repo.save({ ...dto }); }   // spreads untrusted data

@Query('active') active: boolean                      // annotation only: it is the string 'false'
```

Why: pipes and DTO classes make the contract explicit and enforced. Manual parsing, interfaces, and spreading raw bodies skip validation and invite mass assignment.

Additional guidance:

- Keep DTOs close to the controller that uses them (`dto/` folder per feature).
- One DTO per use case (`CreateXDto`, `UpdateXDto`, `ListXQuery`). Do not reuse an entity as a DTO.
- Use response DTOs/serialization too, so entity fields do not leak (see [Serialization](../03-core-concepts/02-validation-and-serialization/07-serialization.md)).
- Prefer `ParseUUIDPipe` and `ParseEnumPipe` over hand-written checks.
- Test validation: an e2e test that sends bad input and asserts the `400`.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `schema` option on `@Body()`, `@Query()`, `@Param()`, `@RawBody()` | New (Standard Schema), enforce via `StandardSchemaValidationPipe` | Not available |
| `@RawBody()` | Available (raw body must be enabled) | `RawBodyRequest` with `@Req()` |
| `@Cookies()`, `@SignedCookies()` | In the official decorator table. *Verify for your version* | `req.cookies` via `cookie-parser` |
| Express | v5 line, where the default query parser is the simple one and nested queries need `extended` | Express 4 defaulted differently |
| `ParseDatePipe` | Available | Added in the v10 line. *Verify the exact version* |
| `@nestjs/config` validation | Standard Schema (Zod, Valibot, ArkType, Joi 18+) | Joi-centric |
| `import type` for interfaces, normal import for DTO classes | Required under the generated `isolatedModules` setup | n/a |

*Verify against the official controllers and pipes chapters and the migration guide.*

---

## Real-World Use Cases

- **List endpoints** with pagination, sorting, and filters via a query DTO.
- **Create and update endpoints** with strict, whitelisted DTOs.
- **Resource lookups** with UUID or integer params.
- **Webhook receivers** with raw-body signature verification.
- **Multi-tenant APIs** deriving tenant from authenticated context (and optionally a validated header).
- **Search endpoints** with bounded arrays and nested filters (extended parser, schema-validated).
- **Internal APIs** using Zod schemas shared with clients via the Standard Schema option.

---

## Interview Questions

### Beginner

1. How do you read a route parameter and a query parameter?
   - `@Param('id')` and `@Query('name')`.
2. What type do route and query values have, regardless of annotation?
   - Strings (or arrays of strings for repeated query keys).
3. What is a DTO and why should it be a class?
   - A shape for incoming data. Classes survive compilation, so pipes and validators can inspect them. Interfaces are erased.
4. How do you convert a route parameter to a number?
   - `@Param('id', ParseIntPipe) id: number`.

### Intermediate

1. What do `whitelist` and `forbidNonWhitelisted` do?
   - `whitelist` strips properties without validation decorators. `forbidNonWhitelisted` rejects the request instead. Both protect against mass assignment.
2. Why is `?active=false` a problem with `active: boolean`?
   - The annotation converts nothing, and the string `'false'` is truthy. Use `ParseBoolPipe` or an explicit `@Transform`.
3. In what order should `DefaultValuePipe` and `ParseIntPipe` be used?
   - `DefaultValuePipe` first, because `ParseIntPipe` rejects `undefined`.
4. How do you receive nested query objects?
   - Enable the extended query parser (`app.set('query parser', 'extended')`) or Fastify's `querystringParser` with `qs`, and validate the shape.
5. Why might `@Body()` be empty?
   - Missing or unsupported `Content-Type`, no body parser for that type, or a body above the limit.

### Advanced

1. How does `ValidationPipe` know which class to validate against?
   - It receives `metatype` from the emitted `design:paramtypes`. Interfaces and primitives yield `Object`/primitive constructors and are skipped.
2. What does `transform: true` do to primitive parameters, and what pitfall does it have?
   - Converts them to the declared primitive types using constructors, so `Boolean('false')` becomes `true`.
3. How does the NestJS 12 `schema` option work?
   - It attaches a Standard Schema to the argument metadata. A registered `StandardSchemaValidationPipe` validates and returns the parsed value.
4. How do you verify a webhook signature correctly?
   - Enable `rawBody` and verify the signature over the exact raw bytes with a constant-time comparison, rejecting stale timestamps.
5. How would you prevent mass assignment and resource exhaustion at the input layer?
   - Global whitelisting DTOs, explicit length/size/array limits, body size limits at the app and proxy, and never spreading raw bodies into persistence calls.
6. How do you get the true client IP behind a load balancer?
   - Configure trust for the proxy hops (`app.set('trust proxy', n)` on Express) so `req.ip` derives from `X-Forwarded-For` safely.

---

## Quick Reference

```text
Sources        @Param(key?) path · @Query(key?) query · @Body(key?) body · @Headers(name?) headers
               @Req() native · @Ip() · @HostParam() · @Session() · @RawBody() · cookies (see Cookies chapter)
Types          everything arrives as string/array/untyped object
Convert        ParseIntPipe · ParseFloatPipe · ParseBoolPipe · ParseUUIDPipe · ParseEnumPipe
               ParseArrayPipe({ items, separator }) · ParseDatePipe · DefaultValuePipe (before Parse*)
Validate       app.useGlobalPipes(new ValidationPipe({ whitelist, forbidNonWhitelisted, transform }))
               DTO = CLASS + class-validator decorators + normal import
v12 schema     @Body({ schema }) / @Query / @Param / @RawBody + StandardSchemaValidationPipe
Query parser   simple (default Express) · app.set('query parser','extended') · Fastify querystringParser
Raw body       NestFactory.create(App, { rawBody: true }) → RawBodyRequest / @RawBody()
Proxy          app.set('trust proxy', n)
Pitfalls       'false' is truthy · Number('') = 0 · ?a=1 vs ?a=1&a=2 · enableImplicitConversion · no whitelist
Order          guards → pipes (global → controller → route → param) → handler
```

---

## Key Takeaways

- Requests carry data in the path, query, body, headers, and cookies. Each has a decorator, and all of it is untrusted input.
- Annotations do not convert types. Use pipes (`ParseIntPipe`, `ParseBoolPipe`, `DefaultValuePipe`) and DTO classes with `ValidationPipe`.
- DTOs must be classes imported normally, or validation silently disappears.
- Global `ValidationPipe` with `whitelist` and `forbidNonWhitelisted` is the baseline defense against mass assignment.
- Query parsing depends on the platform parser. Booleans, empty strings, and single-vs-repeated keys are classic traps.
- Set body limits, array and string limits, and proxy trust deliberately. Use raw bodies for webhook signatures.
- NestJS 12 adds Standard Schema support to the route decorators, but it only enforces anything once a validation pipe is registered.

---

## Related Topics

```text
06 Decorators
      ↓
[07 Request Data]
      ↓
08 Response Handling  →  09 Request Lifecycle
      ↓
03-core-concepts/02 Validation and Serialization  →  03-core-concepts/01 Pipes
```

- [Fundamentals Overview](./README.md)
- [Controllers](./03-controllers.md)
- [Decorators](./06-decorators.md)
- [Response Handling](./08-response-handling.md)
- [Request Lifecycle](./09-request-lifecycle.md)
- [Pipes](../03-core-concepts/01-request-pipeline/03-pipes.md)
- [DTOs](../03-core-concepts/02-validation-and-serialization/01-dto.md)
- [Validation Pipe](../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)
- [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md)
- [File Upload](../05-advanced/07-integrations/01-file-upload.md)