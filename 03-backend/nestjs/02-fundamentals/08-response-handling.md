# Response Handling

Whatever your handler returns, Nest turns it into an HTTP response: it picks a status code, serializes the value, sets headers, and sends it. You control that process with a small set of tools: the **return value**, **`@HttpCode()`**, **`@Header()`**, **`@Redirect()`**, **`StreamableFile`**, **exceptions**, and, as an escape hatch, the platform response object. This file explains the two response modes, what each kind of return value becomes on the wire, how to set status codes and headers (statically and dynamically), how to stream files, and how errors become responses. The most important rule: **return values and let Nest respond, and reach for `@Res()` only with `passthrough: true`.**

---

## Overview

**What it is.** The part of the framework between your handler's return value (or thrown exception) and the bytes sent to the client.

**Why it exists.** So handlers can be plain methods that return data. Status codes, serialization, and header handling are applied consistently, and interceptors and filters can transform results and errors uniformly.

**Where it is used.** Every endpoint. Choosing the right status and shape is part of API design ([REST and Resource Design](../04-intermediate/08-api-design/01-rest-and-resource-design.md)). File downloads, redirects, and cookies are common special cases.

**Why you should understand it.** Hung requests (`@Res()` without sending), ignored decorators, leaked fields in JSON, wrong status codes, and failed downloads are all response-handling problems.

---

## Mental Model

```text
              ┌──────────────── your handler ────────────────┐
              │  returns value          or        throws      │
              └───────┬──────────────────────────────┬────────┘
                      │                              │
                      ▼                              ▼
      interceptors (after) may map the value   exception filters build the error response
                      │                              │
                      ▼                              ▼
       ┌───────── Nest standard response handling ─────────┐
       │  status:   200 (POST: 201) or @HttpCode / exception│
       │  headers:  @Header(...) + content-type by value    │
       │  body:     object/array → JSON · primitive → as-is │
       │            StreamableFile → stream · undefined → empty│
       └───────────────────────┬───────────────────────────┘
                               ▼
                    adapter writes the response (Express / Fastify)

   Escape hatch:  @Res() → you write the response yourself (Nest steps aside)
```

---

## Core Concepts

### Two Response Modes

| Mode | How | What Nest does |
|---|---|---|
| **Standard** (recommended) | Return a value | Serializes it, applies status/headers, runs interceptors on it |
| **Library-specific** | Inject `@Res()` / `@Response()` or `@Next()` | Steps aside. You must send the response. Interceptors that map the return value, `@HttpCode()`, and `@Header()` stop applying |
| **Library-specific with `passthrough`** | `@Res({ passthrough: true })` | You can touch the platform response (cookies, headers, status) **and** Nest still handles the return value |

```typescript
@Get()
findAll(@Res({ passthrough: true }) res: Response) {
  res.status(HttpStatus.OK);        // or res.cookie(...), res.header(...)
  return [];                        // Nest still serializes and sends this
}
```

If you inject `@Res()` without `passthrough` and never call `send`/`json`/`end`, **the request hangs.**

### What Each Return Value Becomes

| You return | Status | Body / content type |
|---|---|---|
| Object or array | 200 (POST: 201) | JSON (`application/json`) |
| `string` | 200 (POST: 201) | Sent as-is (Express typically labels strings `text/html`) |
| `number`, `boolean` | 200 (POST: 201) | Sent as-is without JSON serialization |
| `undefined` / `void` / `null` | 200 (POST: 201) | Empty body |
| `Promise<T>` | as `T` | Resolved first, then handled as `T` |
| `Observable<T>` | as the final emission | Nest subscribes and uses the last emitted value when the stream completes |
| `StreamableFile` | 200 | Streamed bytes (set `type`, `disposition`, `length`) |
| A class instance | 200 | JSON via `JSON.stringify` (**all enumerable properties**, see Serialization) |
| A thrown `HttpException` | its status | Error JSON (see Exceptions) |
| Any other thrown `Error` | 500 | Generic internal server error |

### Status Codes

Defaults: `200` for everything except `POST`, which is `201`.

```typescript
@Post()
@HttpCode(204)
create() {}
```

`@HttpCode()` is static. For conditional status codes:

- throw an exception (`throw new NotFoundException()`), or
- use `@Res({ passthrough: true })` and `res.status(...)`.

Use the `HttpStatus` enum for readability:

```typescript
@HttpCode(HttpStatus.ACCEPTED)      // 202
```

Choosing the right code: [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md).

### Headers

```typescript
@Post()
@Header('Cache-Control', 'no-store')
create() {}
```

`@Header()` values are static. For dynamic headers use `passthrough`:

```typescript
@Post()
create(@Res({ passthrough: true }) res: Response) {
  const task = this.tasks.create(/* ... */);
  res.location(`/tasks/${task.id}`);          // Express; on Fastify use res.header('Location', ...)
  return task;                                // 201 by default for POST
}
```

### Redirection

```typescript
@Get('docs')
@Redirect('https://docs.nestjs.com', 302)     // status defaults to 302
getDocs(@Query('version') version: string) {
  if (version === '5') return { url: 'https://docs.nestjs.com/v5/' };   // overrides decorator values
}
```

Return an object matching `HttpRedirectResponse` (`{ url, statusCode? }`) to decide dynamically. Redirect codes: `301`/`308` permanent, `302`/`307` temporary (`307`/`308` preserve the method).

### Streaming Files

Returning a raw `fs` stream bypasses Nest's header handling. Wrap it in `StreamableFile`:

```typescript
import { createReadStream } from 'node:fs';
import { join } from 'node:path';
import { Controller, Get, StreamableFile } from '@nestjs/common';

@Controller('files')
export class FilesController {
  @Get('report')
  getReport(): StreamableFile {
    const file = createReadStream(join(process.cwd(), 'report.pdf'));
    return new StreamableFile(file, {
      type: 'application/pdf',
      disposition: 'attachment; filename="report.pdf"',
    });
  }
}
```

`StreamableFile` accepts a `Buffer`, `Uint8Array`, or a `Readable`. Options: `type` (Content-Type), `disposition` (Content-Disposition), `length` (Content-Length). With `StreamableFile`, interceptors and `@Header()` still work.

### Server-Sent Events and Long-Lived Responses

`@Sse()` returns an `Observable` of events and keeps the connection open. See [Server-Sent Events and Streaming](../05-advanced/04-realtime/06-server-sent-events-and-streaming.md).

### Exceptions Become Responses

Throw an HTTP exception anywhere in the call stack:

```typescript
import { NotFoundException, BadRequestException, ForbiddenException } from '@nestjs/common';

throw new NotFoundException('Task 42 not found');
```

Built-in exception classes include `BadRequestException` (400), `UnauthorizedException` (401), `ForbiddenException` (403), `NotFoundException` (404), `ConflictException` (409), `UnprocessableEntityException` (422), `InternalServerErrorException` (500), `ServiceUnavailableException` (503), and the base `HttpException(response, status)`. For statuses without a dedicated class, such as `429`, throw the base class: `new HttpException('Too Many Requests', HttpStatus.TOO_MANY_REQUESTS)` (the throttler package ships its own `ThrottlerException`).

Default error body (approximate):

```json
{ "message": "Task 42 not found", "error": "Not Found", "statusCode": 404 }
```

A non-HTTP error (`throw new Error('boom')`) becomes:

```json
{ "statusCode": 500, "message": "Internal server error" }
```

Nest logs the original error and does **not** leak its message to the client.

NestJS 12 adds an optional `errorCode` to `HttpException` options, serialized into the response body:

```typescript
throw new BadRequestException('Password is too weak', { errorCode: 'WEAK_PASSWORD' });
// → { "statusCode": 400, "message": "Password is too weak", "error": "Bad Request", "errorCode": "WEAK_PASSWORD" }
```

Custom exception classes and global filters: [HTTP Exceptions](../03-core-concepts/01-request-pipeline/07-http-exceptions.md) and [Exception Filters](../03-core-concepts/01-request-pipeline/08-exception-filters.md).

### Serialization: What Actually Gets Sent

`JSON.stringify` is applied to returned objects.

| Value | Serialized as |
|---|---|
| `Date` | ISO 8601 string |
| `undefined` property | omitted |
| `Map`, `Set` | `{}` (not their contents) |
| `BigInt` | **throws** a `TypeError` |
| Circular references | **throws** a `TypeError` |
| Class instance | all enumerable own properties (including passwords or internal fields) |
| Getter/prototype members | not included |

Never return entities with sensitive fields directly. Use response DTOs or `ClassSerializerInterceptor` with `@Exclude()`/`@Expose()`. See [Serialization](../03-core-concepts/02-validation-and-serialization/07-serialization.md).

### Setting Cookies

Cookies are written through the platform response, so use `passthrough`:

```typescript
@Post('login')
login(@Res({ passthrough: true }) res: Response) {
  res.cookie('sid', token, { httpOnly: true, secure: true, sameSite: 'lax', maxAge: 3_600_000 });
  return { ok: true };
}
```

Cookie attributes and trade-offs: [Cookie Authentication](../04-intermediate/06-authentication/06-cookie-authentication.md).

### Views (MVC)

`@Render('template')` renders a template with the returned object as context. Server-rendered pages are covered in the MVC chapter of the official docs. Most APIs return JSON.

---

## How It Works

```text
 handler returns value V (or throws E)
        │
        ├─ threw E ───────────────────────────────────────────────┐
        │                                                          ▼
        ▼                                          exception filters (route → controller → global)
 interceptors (after): map/transform V                             │ build status + body
        │                                                          │
        ▼                                                          │
 ┌─ is the route in library-specific mode (@Res/@Next without passthrough)? ─┐
 │      yes → Nest does nothing. Your code must send.                         │
 │      no  → continue                                                        │
 └────────────────────────────────────────────────────────────────────────────┘
        │
        ▼
 status = @HttpCode or default (POST 201, else 200)
 headers = @Header(...) (+ any set via passthrough)
 body:  StreamableFile → pipe stream
        object/array   → JSON.stringify + application/json
        primitive      → send as-is
        undefined      → empty
        │
        ▼
 HttpAdapter.reply(response, body, statusCode)  → Express res.send / Fastify reply.send
```

---

## Basic Example

```typescript
import {
  Body, Controller, Delete, Get, Header, HttpCode, NotFoundException,
  Param, ParseIntPipe, Post, Redirect, Res, StreamableFile,
} from '@nestjs/common';
import type { Response } from 'express';
import { createReadStream } from 'node:fs';

@Controller('tasks')
export class TasksController {
  constructor(private readonly tasks: TasksService) {}

  @Post()
  create(@Body() dto: CreateTaskDto, @Res({ passthrough: true }) res: Response) {
    const task = this.tasks.create(dto);
    res.location(`/tasks/${task.id}`);          // dynamic header
    return task;                                // 201 Created + JSON
  }

  @Get(':id')
  findOne(@Param('id', ParseIntPipe) id: number) {
    const task = this.tasks.find(id);
    if (!task) throw new NotFoundException(`Task ${id} not found`);   // 404 JSON
    return task;                                                      // 200 JSON
  }

  @Delete(':id')
  @HttpCode(204)                                // empty body
  remove(@Param('id', ParseIntPipe) id: number) {
    this.tasks.remove(id);
  }

  @Get('export/csv')
  @Header('Cache-Control', 'no-store')
  export(): StreamableFile {
    return new StreamableFile(createReadStream('tasks.csv'), {
      type: 'text/csv',
      disposition: 'attachment; filename="tasks.csv"',
    });
  }

  @Get('go/docs')
  @Redirect('https://docs.nestjs.com', 302)
  docs() {}
}
```

```bash
curl -i -X POST localhost:3000/tasks -H 'Content-Type: application/json' -d '{"title":"x"}'
# HTTP/1.1 201 Created   Location: /tasks/1   {"id":1,"title":"x","done":false}
curl -i localhost:3000/tasks/99           # 404 {"message":"Task 99 not found","error":"Not Found","statusCode":404}
curl -i -X DELETE localhost:3000/tasks/1  # 204 No Content
curl -i localhost:3000/tasks/export/csv   # Content-Disposition: attachment ...
curl -i localhost:3000/tasks/go/docs      # 302 Location: https://docs.nestjs.com
```

What happens:

1. `create` returns an object, so Nest responds `201` with JSON. `passthrough` adds a dynamic `Location` header without giving up standard handling.
2. `findOne` turns "missing" into a `404` by throwing. The handler needs no status code logic.
3. `@HttpCode(204)` plus no return value gives an empty `204`.
4. `StreamableFile` lets Nest set `Content-Type`/`Content-Disposition` and pipe the file.
5. `@Redirect` returns a `302` with `Location`.

---

## Practical Examples

### 1. Basic: Return Types

```typescript
@Get('a') a() { return { ok: true }; }           // JSON
@Get('b') b() { return 'plain text'; }           // text
@Get('c') c() { return 42; }                     // "42" as text
@Get('d') async d() { return this.service.load(); }   // awaited then handled
@Get('e') e(): Observable<number[]> { return of([1, 2]); }
```

### 2. Common: Accepted for Async Work

```typescript
@Post('reports')
@HttpCode(202)
@Header('Retry-After', '5')
async request(@Body() dto: ReportDto, @Res({ passthrough: true }) res: Response) {
  const job = await this.reports.enqueue(dto);
  res.location(`/reports/jobs/${job.id}`);
  return { jobId: job.id };
}
```

A `202` with a status resource is the standard shape for queued work. See [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md).

### 3. Common: Domain Error to HTTP Error

```typescript
async findOne(id: number) {
  const task = await this.repo.find(id);
  if (!task) throw new NotFoundException(`Task ${id} not found`);
  return task;
}

async create(email: string) {
  try { return await this.repo.insert({ email }); }
  catch (e) { if (isUniqueViolation(e)) throw new ConflictException('Email already registered'); throw e; }
}
```

### 4. Common: Custom Error Body With a Code (NestJS 12)

```typescript
throw new ConflictException('Email already registered', { errorCode: 'EMAIL_TAKEN' });
```

Clients can switch on `errorCode` instead of parsing messages.

### 5. Real-World: Download With Dynamic Filename and Length

```typescript
@Get(':id/download')
async download(@Param('id') id: string): Promise<StreamableFile> {
  const { path, name, size, mime } = await this.files.locate(id);
  return new StreamableFile(createReadStream(path), {
    type: mime,
    disposition: `attachment; filename="${encodeURIComponent(name)}"`,
    length: size,
  });
}
```

Authorize before streaming, sanitize the filename, and make sure `path` cannot be influenced by the user (path traversal).

### 6. Real-World: Cookie on Login

```typescript
@Post('login')
async login(@Body() dto: LoginDto, @Res({ passthrough: true }) res: Response) {
  const { accessToken } = await this.auth.login(dto);
  res.cookie('access_token', accessToken, { httpOnly: true, secure: true, sameSite: 'lax' });
  return { ok: true };
}
```

### 7. Real-World: Response DTO Instead of Entity

```typescript
export class UserResponse {
  id!: number;
  email!: string;
  static from(u: UserEntity): UserResponse { return { id: u.id, email: u.email }; }
}

@Get(':id')
async find(@Param('id', ParseIntPipe) id: number): Promise<UserResponse> {
  return UserResponse.from(await this.users.get(id));       // password hash never reaches the client
}
```

### 8. Edge Case: `@Res()` Without Sending

```typescript
@Get()
broken(@Res() res: Response) {
  return { hello: 'world' };      // ignored: request hangs
}

@Get()
fixed(@Res({ passthrough: true }) res: Response) {
  res.status(200);
  return { hello: 'world' };      // sent by Nest
}
```

### 9. Edge Case: `@HttpCode` and `@Header` Ignored

```typescript
@Post()
@HttpCode(204)
@Header('X-Demo', '1')
create(@Res() res: Response) { res.status(201).json({}); }    // decorators have no effect in library-specific mode
```

### 10. Edge Case: Unserializable Values

```typescript
@Get('big') big() { return { total: 10n }; }                  // TypeError: Do not know how to serialize a BigInt
@Get('map') map() { return { m: new Map([['a', 1]]) }; }      // { "m": {} }
```

Convert before returning (`total.toString()`, `Object.fromEntries(map)`).

### 11. Edge Case: Redirect Preserving the Method

```typescript
@Post('old')
@Redirect('/new', 307)     // or 308 (permanent): the client repeats the POST at /new
old() {}
```

`302` can cause clients to switch a `POST` to a `GET`. Use `307`/`308` when the method must be preserved.

---

## Syntax / API / Commands

| Item | Purpose |
|---|---|
| `@HttpCode(n)` | Static success status |
| `@Header(name, value)` | Static response header |
| `@Redirect(url, status?)` | Redirect (default 302). Return `{ url, statusCode }` to override dynamically |
| `@Res({ passthrough: true })` | Platform response access without losing standard handling |
| `@Res()` / `@Next()` | Full manual mode |
| `StreamableFile(source, { type, disposition, length })` | Stream bytes with headers |
| `HttpStatus` | Status code enum |
| `HttpException(response, status, options?)` | Base exception (`options.cause`, v12 `errorCode`) |
| `NotFoundException`, `BadRequestException`, `UnauthorizedException`, `ForbiddenException`, `ConflictException`, `UnprocessableEntityException`, ... | Built-in HTTP errors |
| `@Sse()` | Server-sent events (returns `Observable`) |
| `@Render('view')` | Template rendering |
| `ClassSerializerInterceptor`, `@Exclude()`, `@Expose()` | Control serialized fields |
| `res.cookie(...)`, `res.location(...)`, `res.header(...)` (via passthrough) | Platform response helpers (Express/Fastify names differ) |

---

## Important Rules

1. **Prefer returning values.** Standard mode keeps interceptors, serialization, and decorators working.
2. **`@Res()` or `@Next()` without `passthrough` disables standard handling.** You must send, or the request hangs.
3. **`@HttpCode()` and `@Header()` are static.** Use `passthrough` for dynamic values.
4. **`POST` defaults to `201`.** Override when the operation does not create a resource.
5. **Throw exceptions for errors.** Do not return `200` with an error body.
6. **Non-HTTP errors become generic `500`s.** The real error is logged, not sent.
7. **Never return entities with secrets.** Use response DTOs or serialization.
8. **`JSON.stringify` limitations apply:** `BigInt` and cycles throw, `Map`/`Set` become `{}`.
9. **Stream large payloads** with `StreamableFile`, not by loading them into memory.
10. **Use `307`/`308` to preserve the HTTP method on redirects.**
11. **Authorize before streaming or redirecting.** A download link is an endpoint like any other.
12. **Platform response APIs differ** between Express and Fastify. Use `passthrough` helpers carefully or stick to Nest decorators for portability.

---

## Under the Hood

### The Response Controller

Nest's router builds a "route handler" per route. After your method returns, the **router response controller** applies: the status (decorator or default), headers, and then `apply(result, response, status)`, which hands the result to the HTTP adapter's `reply()`. In library-specific mode (detected from `@Res()`/`@Next()` in the argument metadata), that last step is skipped.

### Observable Handling

For an `Observable`, the response controller converts it with `lastValueFrom`-like semantics: it waits for completion and uses the final value. For `@Sse()` routes, it instead keeps the stream open and writes each event.

### How Streams Are Piped

`StreamableFile` exposes `getStream()` and `getHeaders()`. The response controller sets headers from `getHeaders()` and pipes the stream to the response with error handling, so interceptors can still run and `@Header()` still applies.

### Exceptions Flow

Anything thrown (from middleware, guards, interceptors, pipes, or handlers) is caught by the exceptions zone. The base exception filter maps `HttpException` to its status and body, and everything else to a `500` with a generic message while logging the original error. Custom filters (`@Catch()`) intercept this first.

### Adapter Differences

| Operation | Express | Fastify |
|---|---|---|
| Set status | `res.status(n)` | `reply.status(n)` |
| Set header | `res.header(k, v)` / `res.setHeader` | `reply.header(k, v)` |
| Cookie | `res.cookie(...)` | requires `@fastify/cookie`, `reply.setCookie(...)` |
| Location | `res.location(url)` | `reply.header('Location', url)` |
| Default ETag on `send` | Weak ETag generated | Not generated by default |

Because `@Res()` hands you the native object, code using it is not portable. *Verify adapter-specific API names for your installed versions.*

### Graceful Shutdown (NestJS 12)

On Express, the adapter now drains in-flight requests on shutdown before closing, so responses in progress finish rather than being cut off. See [Graceful Shutdown](../07-production/04-deployment/02-graceful-shutdown-and-process-management.md).

---

## Common Patterns

### Return Data, Throw Errors

Handlers return values for success and throw `HttpException` subclasses for failure.

### Response DTOs

Explicit output shapes per endpoint (and `ClassSerializerInterceptor` for entity classes), so internal fields never leak.

### Envelope or No Envelope

Decide once: bare resources (`{ id, ... }`) or an envelope (`{ data, meta }`). Implement envelopes with an interceptor, not in every handler. See [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md).

### Created Resource: `201` + `Location`

Return the resource and add the `Location` header with `passthrough`.

### Accepted: `202` + Status Resource

Queue the work, return a job URL.

### Download: `StreamableFile`

Set `type`, `disposition`, and `length`. Authorize and sanitize first.

### Passthrough for Cookies

`@Res({ passthrough: true })` only to set cookies and headers.

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| `@Res()` without sending | Request hangs | Standard handling disabled | `res.send/json`, or `passthrough: true` |
| `@HttpCode`/`@Header` with `@Res()` | Ignored | Library-specific mode | `passthrough: true` |
| `200` with an error payload | Clients and monitoring miss failures | Returning errors instead of throwing | Throw `HttpException` subclasses |
| Returning entities directly | Password hashes or internal fields exposed | Class instances serialize fully | Response DTOs, `ClassSerializerInterceptor` |
| `BigInt` in a response | `TypeError: Do not know how to serialize a BigInt` | JSON cannot represent it | Convert to string or number |
| Returning `Map`/`Set` | `{}` in the response | `JSON.stringify` behavior | Convert to object/array |
| Circular references | `TypeError: Converting circular structure to JSON` | Entities with back-references | Map to DTOs |
| Returning a raw `fs` stream | Missing headers, odd behavior | Not wrapped | `new StreamableFile(stream, { ... })` |
| Loading a big file into memory | Memory spikes, slow responses | `readFile` then return | Stream it |
| Wrong redirect code | `POST` becomes `GET` | `302` semantics | `307`/`308` |
| `201` for non-creating `POST` | Semantically wrong status | Default | `@HttpCode(200)` or `204` |
| Dynamic status via `@HttpCode` | Always the same status | Decorator is static | Throw, or `passthrough` and `res.status` |
| Leaking raw error messages | Internal details visible | Custom code returns `err.message` | Let Nest map to generic `500`, log the detail |
| Express-only response APIs on Fastify | Runtime errors after switching | Different native APIs | Use Nest abstractions or adapter-specific code consciously |
| Header injection from user input | Response splitting, odd headers | Unvalidated values in headers | Validate and encode (for example `encodeURIComponent` in `Content-Disposition`) |

---

## Debugging

| Symptom | Check |
|---|---|
| Request never completes | `@Res()`/`@Next()` without `passthrough` and no `send` |
| Status not what you set | `@HttpCode` with `@Res()`, or an interceptor/filter changing it |
| Header missing | Library-specific mode, or an upstream proxy stripping it, or Fastify vs Express API difference |
| Empty response body | Handler returned `undefined`, or `204` |
| `500` with no details | Non-HTTP error: check server logs for the logged exception |
| `TypeError` about serializing | `BigInt` or circular structure in the returned object |
| Download corrupt or without a name | Missing `type`/`disposition`, text/binary mismatch, double-wrapping |
| CORS errors on downloads | Missing `Access-Control-Expose-Headers` for `Content-Disposition` |

```bash
curl -i http://localhost:3000/tasks/1            # status + headers + body
curl -I http://localhost:3000/tasks/export/csv   # headers only
curl -v -o /dev/null http://localhost:3000/tasks/export/csv   # large download: just the headers and transfer
```

```typescript
@Get() debug() { const r = { total: 1n }; return JSON.parse(JSON.stringify(r, (_k, v) => typeof v === 'bigint' ? v.toString() : v)); }
```

See [Debugging](../01-getting-started/06-debugging.md).

---

## Performance

- **JSON serialization is synchronous** and CPU-bound. Huge responses block the event loop. Paginate and project fields.
- **Stream large files** instead of buffering. `StreamableFile` with `createReadStream` keeps memory flat.
- **Compression** (gzip/brotli) saves bandwidth but uses CPU. Prefer the reverse proxy or CDN for it.
- **Caching headers** (`Cache-Control`, `ETag`) let clients and CDNs skip work. See [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md).
- **Avoid `@Res()`** on hot paths unless needed: it forfeits interceptors that might provide caching or mapping.
- **Serialize once.** Do not stringify manually and then return the string (double work and wrong content type).
- **Observables:** unbounded streams that never complete will never respond in standard mode. Use `@Sse()` for streaming.

---

## Security

- **Do not leak fields.** Serialize explicit response shapes. Hash columns, tokens, and internal ids should never be in a returned entity.
- **Do not leak internals in errors.** Let unexpected errors become generic `500`s. Log details server-side. Do not echo SQL errors or stack traces.
- **Header injection:** never place unvalidated user input into headers (`Location`, `Content-Disposition`, `Set-Cookie`). Encode filenames.
- **Open redirects:** validate redirect targets from query parameters (allow-list hosts or paths) before using them in `@Redirect` or `res.redirect`.
- **Path traversal on downloads:** map ids to server-side paths. Never join user-supplied paths.
- **Authorization on downloads and redirects:** check ownership before streaming.
- **Content-Type safety:** serve user-generated files with the correct `type` and `X-Content-Type-Options: nosniff`, and consider `Content-Disposition: attachment` to avoid inline script execution.
- **Cookies:** `HttpOnly`, `Secure`, `SameSite`, and a narrow `Path`/`Domain`. Set them through `passthrough`.
- **Security headers** (CSP, HSTS, etc.) belong in Helmet. See [HTTP Security Headers and CORS](../07-production/01-security/02-http-security-headers-and-cors.md).

---

## Production Considerations

- **One error format** across the API (a global exception filter or a standard `HttpException` payload). Include a stable `errorCode` (NestJS 12) or `type` field for clients.
- **Never return stack traces** to clients in production.
- **Correct status codes** make monitoring meaningful. Alert on `5xx` rates and track `4xx` by route.
- **Cache policy per endpoint class:** public static, public dynamic, private, sensitive (`no-store`).
- **Large downloads:** consider presigned object-storage URLs instead of proxying bytes through the app. See [Cloud Storage](../05-advanced/07-integrations/02-cloud-storage.md).
- **Timeouts:** an interceptor-based timeout turns hung handlers into `504`/`408`-style responses. See [Interceptor Recipes](../03-core-concepts/01-request-pipeline/06-interceptor-recipes.md).
- **Graceful shutdown** so in-flight responses finish (NestJS 12 drains in-flight Express requests, but enable shutdown hooks and test it).
- **Compression, TLS, HTTP/2** at the edge.

---

## Best Practices

### Recommended

```typescript
@Post()
async create(@Body() dto: CreateTaskDto, @Res({ passthrough: true }) res: Response): Promise<TaskResponse> {
  const task = await this.tasks.create(dto);
  res.location(`/tasks/${task.id}`);
  return TaskResponse.from(task);                      // explicit output shape, 201 by default
}

@Get(':id')
async findOne(@Param('id', ParseIntPipe) id: number) {
  return this.tasks.getOrThrow(id);                    // service throws NotFoundException
}

@Delete(':id')
@HttpCode(204)
remove(@Param('id', ParseIntPipe) id: number) { return this.tasks.remove(id); }
```

### Avoid

```typescript
@Post()
async create(@Req() req: Request, @Res() res: Response) {
  try {
    const task = await this.tasks.create(req.body);    // unvalidated, platform-bound
    return res.status(200).json(task);                 // entity returned as-is, wrong status for a creation
  } catch (e) {
    return res.status(200).json({ error: e.message }); // 200 with an error and leaked message
  }
}
```

Why: the recommended handlers are validated, return explicit shapes, use correct status codes, and let Nest map errors. The avoided handler gives up interceptors and filters, hides failures behind `200`, and leaks details.

Additional guidance:

- Return the created or updated resource when it saves a client round trip.
- Use `204` for deletes and updates that return nothing.
- Put cross-cutting response shaping (envelopes, timing headers) in interceptors.
- Map persistence errors to HTTP errors in one place (a filter) rather than in every handler.
- Test status codes and headers in e2e tests.

---

## Version / Compatibility Notes

| Item | NestJS 12 | Earlier |
|---|---|---|
| `HttpException` `errorCode` option | New, serialized into the body | Not available |
| Express graceful shutdown | Adapter drains in-flight requests | Not drained |
| HTTP adapter error mapping | Reworked in v12 (see migration guide). *Review custom filters that depend on adapter-specific error shapes* | Previous behavior |
| `@QueryMethod()` | Available for the HTTP `QUERY` method | Not available |
| Standard handling vs `@Res()` | Unchanged | Same |
| `StreamableFile` | Unchanged API (`type`, `disposition`, `length`) | Same |
| Express version | v5 line (Nest compatibility layer for paths) | Express 4 before Nest 11 |
| `HttpStatus` / built-in exception classes | Unchanged. There is no built-in `429` class, so use `HttpException` with `HttpStatus.TOO_MANY_REQUESTS` | Same |

*Verify against the official controllers chapter, the exception filters chapter, and the migration guide.*

---

## Real-World Use Cases

- **REST resource endpoints** returning JSON with correct statuses.
- **Create flows** with `201`, `Location`, and a response DTO.
- **Async job submission** with `202` and a status URL.
- **Report and export downloads** streamed from disk or object storage.
- **Login flows** setting `HttpOnly` cookies.
- **Short links and legacy URL handling** with redirects.
- **API error contracts** with stable `errorCode`s.
- **Streaming endpoints** (SSE, LLM token streams).

---

## Interview Questions

### Beginner

1. What does a NestJS handler return, and what happens to it?
   - A value or promise. Nest serializes objects to JSON, sends primitives as-is, and applies the default status.
2. What is the default status code for `POST`?
   - `201`. Others default to `200`.
3. How do you change the status code?
   - `@HttpCode(n)`, or throw an exception for error statuses.
4. How do you return an error response?
   - Throw an `HttpException` subclass such as `NotFoundException`.

### Intermediate

1. What happens when you inject `@Res()`?
   - Nest enters library-specific mode and stops sending the response, so you must send it yourself. Interceptors that map the result, `@HttpCode`, and `@Header` stop applying.
2. What does `passthrough: true` do?
   - Lets you use the platform response for side effects while Nest still handles the return value and decorators.
3. How do you set a dynamic header such as `Location`?
   - `@Res({ passthrough: true })` and `res.location(...)` (Express) or `res.header('Location', ...)`.
4. How do you stream a file?
   - Return `new StreamableFile(stream, { type, disposition, length })`.
5. What does Nest do with a non-HTTP error like `throw new Error('x')`?
   - Returns a generic `500` and logs the original error without leaking its message.

### Advanced

1. How would you prevent entity fields such as password hashes from leaking?
   - Return explicit response DTOs, or use `ClassSerializerInterceptor` with `@Exclude()`/`@Expose()`, and never return raw entities.
2. What can break `JSON.stringify` on a returned object?
   - `BigInt` values and circular references throw. `Map`/`Set` serialize to `{}`.
3. When would you choose `307`/`308` over `302` for a redirect?
   - When the HTTP method and body must be preserved (for example redirecting a `POST`).
4. Walk through how a returned `Observable` is handled.
   - Nest subscribes, waits for completion, and uses the last emitted value (standard routes). For `@Sse()` it streams each event.
5. How do you design the error contract for an API in NestJS 12?
   - A global exception filter (or consistent `HttpException` bodies) with stable codes via the `errorCode` option, no stack traces in production, and logging of the underlying cause.
6. What are the portability risks of `@Res()`?
   - It exposes native Express or Fastify objects with different APIs, complicates testing, and bypasses Nest features.

---

## Quick Reference

```text
Return value      object/array → JSON · string/number/boolean → as-is · undefined → empty
                  Promise → awaited · Observable → last value · StreamableFile → stream
Status            200, POST 201 · @HttpCode(n) static · throw HttpException for errors · res.status via passthrough
Headers           @Header(k, v) static · res.header/location/cookie via @Res({ passthrough: true })
Redirect          @Redirect(url, 302) · return { url, statusCode } to override · 307/308 keep the method
File              new StreamableFile(stream, { type, disposition, length })
Errors            HttpException subclasses → status + { message, error, statusCode }
                  other Error → 500 generic · v12: { errorCode } option
@Res()/@Next()    manual mode → you must send (else the request hangs) · passthrough: true → hybrid
JSON traps        BigInt throws · cycles throw · Map/Set → {} · class instances leak all fields
Rules             return values · throw for errors · DTOs for output · stream big files · authorize downloads
```

---

## Key Takeaways

- Return a value and Nest builds the response: status, headers, serialization, and content type are handled for you.
- `@Res()` hands you the platform object but turns off standard handling. Use `@Res({ passthrough: true })` when you only need a cookie or a dynamic header.
- `@HttpCode()` and `@Header()` are static. Errors are expressed by throwing exceptions, not by returning error payloads.
- Unexpected errors become generic `500`s. Details go to logs. NestJS 12's `errorCode` option gives clients a stable machine-readable code.
- Returned class instances serialize every property, so return response DTOs or use serialization to avoid leaking sensitive fields.
- `JSON.stringify` has sharp edges (`BigInt`, cycles, `Map`/`Set`).
- Stream large files with `StreamableFile`, authorize first, and never build paths from user input.

---

## Related Topics

```text
07 Request Data
      ↓
[08 Response Handling]
      ↓
09 Request Lifecycle
      ↓
03-core-concepts/01 HTTP Exceptions, Exception Filters, Interceptor Recipes  →  04-intermediate/08 Response and Error Format
```

- [Fundamentals Overview](./README.md)
- [Controllers](./03-controllers.md)
- [Request Data](./07-request-data.md)
- [Request Lifecycle](./09-request-lifecycle.md)
- [HTTP and REST Basics](../00-prerequisites/04-http-and-rest-basics.md)
- [HTTP Exceptions](../03-core-concepts/01-request-pipeline/07-http-exceptions.md)
- [Exception Filters](../03-core-concepts/01-request-pipeline/08-exception-filters.md)
- [Serialization](../03-core-concepts/02-validation-and-serialization/07-serialization.md)
- [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md)
- [Server-Sent Events and Streaming](../05-advanced/04-realtime/06-server-sent-events-and-streaming.md)