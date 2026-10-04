# HTTP and REST Basics

HTTP is the request/response protocol that every NestJS controller speaks, and REST is the set of conventions for designing APIs on top of it. Every decorator in `02-fundamentals` (`@Get`, `@Body`, `@HttpCode`, `@Header`) maps directly to a piece of HTTP. If you can read a raw request and response, choose the right method and status code, and explain how headers, cookies, caching, and CORS behave, you will make better API decisions and debug faster.

---

## Overview

**What it is.** HTTP (HyperText Transfer Protocol) is a stateless, text-oriented, request/response application protocol. A client sends a **request** (method, target, headers, optional body). A server returns a **response** (status code, headers, optional body). **REST** (Representational State Transfer) is an architectural style that models an API as **resources** identified by URLs and manipulated through HTTP's uniform interface.

**Why it exists.** HTTP gives clients and servers a shared vocabulary so that browsers, mobile apps, proxies, caches, and CDNs can interoperate without knowing about your application. REST uses that vocabulary consistently so that APIs are predictable.

**Where it is used.** Web APIs, browsers, mobile backends, microservice-to-microservice calls, webhooks, and third-party integrations.

**Why you should understand it.**

- Correct methods and status codes make an API self-describing, cache-friendly, and safe to retry.
- Most "mysterious" bugs (CORS errors, cookies not being sent, `401` vs `403`, stale caches) are HTTP semantics, not framework bugs.
- Idempotency, safe methods, and caching rules are the basis of reliable client retries and of [API Design](../04-intermediate/08-api-design/README.md).

---

## Mental Model

```text
 Client                                                        Server
 ──────                                                        ──────
   │  1. Resolve DNS, open TCP (+ TLS for HTTPS)                  │
   │ ───────────────────────────────────────────────────────────► │
   │                                                               │
   │  2. REQUEST                                                   │
   │     POST /users HTTP/1.1                                      │
   │     Host: api.example.com                                     │
   │     Content-Type: application/json                            │
   │     Authorization: Bearer <token>                             │
   │                                                               │
   │     {"email":"a@b.c"}                                         │
   │ ───────────────────────────────────────────────────────────► │
   │                                       3. Route, validate,     │
   │                                          authorize, execute   │
   │  4. RESPONSE                                                  │
   │     HTTP/1.1 201 Created                                      │
   │     Location: /users/42                                       │
   │     Content-Type: application/json                            │
   │                                                               │
   │     {"id":42,"email":"a@b.c"}                                 │
   │ ◄─────────────────────────────────────────────────────────── │
```

Two ideas to hold on to:

1. **A request says "do this verb to that noun."** The method is the verb, the URL is the noun.
2. **HTTP is stateless.** Each request must carry everything the server needs to process it (credentials, identifiers). The server does not remember the previous request unless you build state on top (cookies, sessions, tokens).

---

## Core Concepts

### Anatomy of a Request and a Response

```http
POST /users?notify=true HTTP/1.1
Host: api.example.com
Content-Type: application/json
Accept: application/json
Authorization: Bearer eyJhbGciOi...
Content-Length: 22

{"email":"a@b.c"}
```

| Part | Example | Notes |
|---|---|---|
| Method | `POST` | The action |
| Target | `/users?notify=true` | Path plus query string |
| Version | `HTTP/1.1` | Message framing differs between versions, semantics are shared |
| Headers | `Host`, `Content-Type`, ... | Metadata, case-insensitive names |
| Body | JSON payload | Optional. Not allowed to be meaningful for `GET`/`HEAD` in practice |

```http
HTTP/1.1 201 Created
Content-Type: application/json; charset=utf-8
Location: /users/42
Content-Length: 29

{"id":42,"email":"a@b.c"}
```

| Part | Example |
|---|---|
| Status line | `201 Created` (code + reason phrase) |
| Headers | `Content-Type`, `Location`, ... |
| Body | The representation of the result |

### URL Structure

```text
https://api.example.com:443/v1/users/42/orders?status=paid&page=2#section
└─┬──┘   └──────┬──────┘└┬─┘└──────────┬──────┘└───────┬───────┘└──┬───┘
scheme       host      port          path          query string  fragment
                                                              (never sent to the server)
```

| Piece | Used for | NestJS decorator |
|---|---|---|
| Path segments (`/users/42`) | Identify the resource | `@Param('id')` |
| Query string (`?status=paid`) | Filter, sort, paginate, options | `@Query('status')` |
| Body | Create or update payload | `@Body()` |
| Headers | Metadata: auth, content negotiation, caching | `@Headers('authorization')` |
| Cookies | Browser-managed small state | `@Req()` with `cookie-parser` |

### HTTP Methods

| Method | Meaning | Request body | Safe | Idempotent | Typical success |
|---|---|---|---|---|---|
| `GET` | Retrieve a representation | No | Yes | Yes | `200` |
| `HEAD` | Like `GET`, headers only | No | Yes | Yes | `200` |
| `OPTIONS` | Describe communication options (CORS preflight) | Rare | Yes | Yes | `204` / `200` |
| `POST` | Submit data, create a subordinate resource, trigger a process | Yes | No | **No** | `201` / `200` / `202` |
| `PUT` | Replace the target resource entirely (or create at a known URL) | Yes | No | **Yes** | `200` / `204` / `201` |
| `PATCH` | Partially modify | Yes | No | **Not guaranteed** | `200` / `204` |
| `DELETE` | Remove | Rare | No | **Yes** | `204` / `200` |

Definitions (from HTTP semantics, RFC 9110):

- **Safe:** the client does not request or expect a state change. Safe methods should be read-only in effect. This lets crawlers, prefetchers, and caches call them freely.
- **Idempotent:** making the same request **N times has the same intended effect on the server as making it once**. This lets clients and proxies retry after timeouts safely.

Notes that surprise people:

- Idempotent does **not** mean "same response every time". `DELETE /users/1` returns `204` the first time and perhaps `404` the second. The server state (user gone) is the same.
- `PATCH` is not defined as idempotent. `{"op":"increment"}` is not. `{"name":"Ada"}` effectively is. Design deliberately.
- `POST` is not idempotent, which is why retrying a payment `POST` after a timeout can double-charge. The fix is an **idempotency key**. See [Idempotency](../04-intermediate/08-api-design/06-idempotency.md).
- A `GET` must never delete or modify data, even if "it's just a link".

### Status Codes

Classes:

| Range | Meaning |
|---|---|
| `1xx` | Informational (rare in app code) |
| `2xx` | Success |
| `3xx` | Redirection |
| `4xx` | **Client** error: the request is wrong, retrying unchanged will not help |
| `5xx` | **Server** error: the server failed, retrying later may help |

The codes you will use constantly:

| Code | Name | Use when |
|---|---|---|
| `200` | OK | Successful `GET`, or `PUT`/`PATCH`/`POST` that returns a body |
| `201` | Created | A resource was created. Include `Location` |
| `202` | Accepted | Request accepted for **asynchronous** processing (queued job) |
| `204` | No Content | Success with no body (`DELETE`, some `PUT`/`PATCH`) |
| `301` / `308` | Moved Permanently / Permanent Redirect | Resource has a new permanent URL (`308` preserves method) |
| `302` / `307` | Found / Temporary Redirect | Temporary redirect (`307` preserves method) |
| `304` | Not Modified | Conditional `GET` hit: client's cached copy is still valid |
| `400` | Bad Request | Malformed syntax, invalid JSON, failed validation |
| `401` | Unauthorized | **Not authenticated** (missing or invalid credentials) |
| `403` | Forbidden | Authenticated, but **not allowed** |
| `404` | Not Found | Resource does not exist (or you hide its existence) |
| `405` | Method Not Allowed | Path exists, method does not. Include `Allow` |
| `409` | Conflict | State conflict: duplicate unique value, version mismatch |
| `413` | Content Too Large | Body exceeds limits |
| `415` | Unsupported Media Type | Unsupported `Content-Type` |
| `422` | Unprocessable Content | Syntactically valid, semantically invalid (often used for validation errors) |
| `429` | Too Many Requests | Rate limit exceeded. Send `Retry-After` |
| `500` | Internal Server Error | Unexpected server failure |
| `502` | Bad Gateway | Upstream returned an invalid response |
| `503` | Service Unavailable | Overloaded or in maintenance. May send `Retry-After` |
| `504` | Gateway Timeout | Upstream did not respond in time |

Common decisions:

| Question | Answer |
|---|---|
| `401` or `403`? | `401` = "I don't know who you are." `403` = "I know who you are, and you can't do this." Despite its name, `401` means unauthenticated |
| `400` or `422`? | Both are used for validation failure. `400` for malformed or unparsable requests, `422` for well-formed but invalid data. Pick one convention and apply it consistently (NestJS `ValidationPipe` returns `400` by default) |
| `404` or `403` for a resource the user may not see? | Return `404` when revealing existence is itself a leak |
| `200` with `{"error": ...}` | Avoid it. Clients, proxies, and monitoring rely on the status code |
| `204` or `200` after `DELETE`? | `204` if no body. `200` if you return the deleted representation |
| `500` for bad input? | Never. Bad input is `4xx`. A `500` in logs should mean "our bug" |

### Headers

Headers are `Name: value` pairs. Names are case-insensitive.

**Request headers**

| Header | Purpose |
|---|---|
| `Host` | Target host (required in HTTP/1.1) |
| `Authorization` | Credentials: `Bearer <token>`, `Basic <b64>` |
| `Content-Type` | Format of the **request** body (`application/json`) |
| `Accept` | Formats the client can handle (content negotiation) |
| `Accept-Language`, `Accept-Encoding` | Language, compression (`gzip`, `br`) |
| `User-Agent` | Client software |
| `Cookie` | Cookies the browser attaches |
| `Origin` | Origin of a cross-origin request (CORS) |
| `If-None-Match`, `If-Modified-Since` | Conditional requests for caching |
| `Idempotency-Key` | Not standardized in all contexts. Widely used convention for safe `POST` retries |
| `X-Forwarded-For`, `X-Forwarded-Proto`, `Forwarded` | Original client info when behind a proxy |

**Response headers**

| Header | Purpose |
|---|---|
| `Content-Type` | Format of the **response** body |
| `Content-Length` / `Transfer-Encoding: chunked` | Body framing |
| `Location` | Target of a redirect, or URL of a created resource |
| `Set-Cookie` | Instruct the browser to store a cookie |
| `Cache-Control` | Caching policy |
| `ETag`, `Last-Modified` | Validators for conditional requests |
| `Retry-After` | When to retry (`429`, `503`) |
| `Allow` | Methods supported (with `405`) |
| `WWW-Authenticate` | How to authenticate (with `401`) |
| `Access-Control-Allow-*` | CORS permission headers |
| `Strict-Transport-Security`, `Content-Security-Policy`, `X-Content-Type-Options` | Security headers (see [Security Headers and CORS](../07-production/01-security/02-http-security-headers-and-cors.md)) |

### Content Types and Bodies

| `Content-Type` | Body format | Typical use |
|---|---|---|
| `application/json` | JSON | Standard API payloads |
| `application/x-www-form-urlencoded` | `a=1&b=2` | HTML forms |
| `multipart/form-data` | Parts with boundaries | File uploads |
| `text/plain`, `text/html` | Text | Simple responses, pages |
| `application/octet-stream` | Raw bytes | Downloads |
| `application/problem+json` | RFC 9457 error object | Structured errors |

The server must parse the body according to `Content-Type`. A missing or wrong header is a top cause of "my body is empty" bugs.

### Cookies

A cookie is a small key/value the server sets with `Set-Cookie`. The browser automatically sends it back with later requests to the matching site.

```http
Set-Cookie: sid=abc123; Path=/; Max-Age=3600; HttpOnly; Secure; SameSite=Lax
```

| Attribute | Effect |
|---|---|
| `HttpOnly` | JavaScript (`document.cookie`) cannot read it. Mitigates theft via XSS |
| `Secure` | Sent only over HTTPS |
| `SameSite=Strict` | Never sent on cross-site requests |
| `SameSite=Lax` | Sent on top-level navigations (links) but not on cross-site subrequests (most browsers' default when unspecified) |
| `SameSite=None` | Sent on cross-site requests. **Requires `Secure`** |
| `Domain` / `Path` | Scope of where the cookie is sent |
| `Max-Age` / `Expires` | Lifetime. Without them it is a session cookie |
| `__Host-` prefix | Enforces `Secure`, `Path=/`, and no `Domain`. Strong default for session cookies |

Cookies are attached **automatically by the browser**, which is both their convenience (no client code) and their risk (CSRF). See [Cookie Authentication](../04-intermediate/06-authentication/06-cookie-authentication.md) and [CSRF](../07-production/01-security/03-csrf.md).

### Caching

HTTP caching lets clients, CDNs, and proxies reuse responses.

```http
Cache-Control: public, max-age=3600
ETag: "v42"
```

| Directive | Meaning |
|---|---|
| `max-age=N` | Fresh for N seconds |
| `no-store` | Do not store at all (sensitive data) |
| `no-cache` | May store, but **must revalidate** before reuse |
| `private` | Only the end user's browser may cache |
| `public` | Shared caches (CDNs) may cache |
| `must-revalidate` | Once stale, must revalidate |
| `s-maxage=N` | Freshness for shared caches |

Conditional requests save bandwidth:

```text
1st:  GET /report            → 200, ETag: "v42", body
2nd:  GET /report
      If-None-Match: "v42"   → 304 Not Modified (no body)
```

Only `GET` (and `HEAD`) responses are cached by default. Authenticated or per-user responses should usually be `Cache-Control: private` or `no-store`.

### Content Negotiation

The client states preferences (`Accept`, `Accept-Language`, `Accept-Encoding`). The server chooses a representation and may reply with `Vary` to tell caches which headers affected the choice (`Vary: Accept-Encoding`).

### CORS (Cross-Origin Resource Sharing)

Browsers enforce the **same-origin policy**: JavaScript on `https://app.example.com` cannot read responses from `https://api.example.com` unless the API opts in. An **origin** is scheme + host + port.

```text
Browser (page at https://app.example.com)               API (https://api.example.com)
        │                                                      │
        │  Non-simple request (e.g. JSON + Authorization)      │
        │  1. PREFLIGHT                                        │
        │  OPTIONS /users                                      │
        │  Origin: https://app.example.com                     │
        │  Access-Control-Request-Method: POST                 │
        │  Access-Control-Request-Headers: authorization,      │
        │                                  content-type        │
        │ ───────────────────────────────────────────────────► │
        │  204                                                 │
        │  Access-Control-Allow-Origin: https://app.example.com│
        │  Access-Control-Allow-Methods: POST                  │
        │  Access-Control-Allow-Headers: authorization,        │
        │                                content-type          │
        │ ◄─────────────────────────────────────────────────── │
        │                                                      │
        │  2. ACTUAL REQUEST (only if preflight allowed it)    │
        │  POST /users  Origin: https://app.example.com        │
        │ ───────────────────────────────────────────────────► │
        │  200  Access-Control-Allow-Origin: ...               │
        │ ◄─────────────────────────────────────────────────── │
```

Key facts:

- CORS is enforced **by browsers**, not by servers. `curl` and server-to-server calls ignore it. It does not protect your API from non-browser clients.
- A **preflight** (`OPTIONS`) is sent when the request is not "simple" (for example custom headers like `Authorization`, or `Content-Type: application/json`).
- `Access-Control-Allow-Origin: *` cannot be combined with credentials (cookies). For credentialed requests, return the exact origin and `Access-Control-Allow-Credentials: true`.
- A CORS error in the browser console usually means the **server did not send the right headers**. The request may well have reached the server.

### HTTP Versions

| Version | Characteristics |
|---|---|
| HTTP/1.1 | Text-based framing. One request at a time per connection (head-of-line blocking). Persistent connections with keep-alive |
| HTTP/2 | Binary framing, multiplexed streams over one connection, header compression |
| HTTP/3 | HTTP semantics over **QUIC** (UDP), avoids TCP head-of-line blocking |

The **semantics** (methods, status codes, headers) are the same across versions. Your NestJS code usually sees HTTP/1.1 because TLS termination and HTTP/2 or HTTP/3 are commonly handled by a reverse proxy or load balancer in front of the app.

### HTTPS / TLS

HTTPS is HTTP inside TLS. TLS provides **encryption**, **integrity**, and **server authentication** (via certificates). Without it, credentials, cookies, and tokens travel in plaintext. In production, always use HTTPS and set `Strict-Transport-Security`.

### REST Principles

REST is a style, not a spec. In practice, a "RESTful API" means:

| Principle | In practice |
|---|---|
| **Resources** | Model *things* (`/users`, `/orders/42`), not actions (`/getUser`) |
| **Uniform interface** | Use standard methods and status codes with consistent semantics |
| **Stateless** | Each request carries what the server needs (token, ids). No server-side per-client conversation state required |
| **Representations** | A resource can be represented as JSON, XML, etc. The client manipulates the representation |
| **Cacheable** | Responses declare whether they can be cached |
| **Layered system** | Clients cannot tell whether they talk to the origin, a proxy, or a CDN |
| **HATEOAS** (hypermedia) | Responses include links to next actions. Rarely implemented fully |

> Most production APIs are "REST-ish": resource-oriented JSON over HTTP, without full HATEOAS. That is a valid, common choice. Be precise when interviewers ask.

### Resource and URL Design

```text
GET    /users              list users
POST   /users              create a user
GET    /users/42           get user 42
PUT    /users/42           replace user 42
PATCH  /users/42           partially update user 42
DELETE /users/42           delete user 42

GET    /users/42/orders    orders belonging to user 42  (sub-resource)
GET    /orders?userId=42   alternative: filter a top-level collection
```

| Guideline | Example |
|---|---|
| Use **plural nouns** for collections | `/users`, not `/user` or `/getUsers` |
| Use nouns, not verbs, in paths | `POST /orders`, not `POST /createOrder` |
| Use `kebab-case` or consistent casing in paths | `/order-items` |
| Keep nesting shallow (one or two levels) | `/users/42/orders`, not `/a/1/b/2/c/3/d` |
| Use query strings for filter/sort/paginate | `/orders?status=paid&sort=-createdAt&page=2` |
| Actions that are not CRUD are acceptable as sub-resources | `POST /orders/42/cancel` |
| Version deliberately | `/v1/users` or header-based. See [API Versioning](../04-intermediate/08-api-design/02-api-versioning.md) |

---

## How It Works

The journey of a request, from the browser to your handler and back:

```text
Browser / client
    │  1. DNS lookup (api.example.com → IP)
    ▼
    │  2. TCP handshake, then TLS handshake (HTTPS)
    ▼
    │  3. HTTP request is sent
    ▼
CDN / load balancer / reverse proxy (nginx, cloud LB)
    │  terminates TLS, may speak HTTP/2 to client and HTTP/1.1 to app,
    │  adds X-Forwarded-* headers, enforces limits, may cache
    ▼
Node.js HTTP server (Express or Fastify under NestJS)
    │  4. Parse request line, headers, body (by Content-Type)
    ▼
Framework routing
    │  5. Match method + path → controller handler
    ▼
Your code (validation, auth, business logic, database)
    │  6. Produce result or throw an error
    ▼
Framework response handling
    │  7. Map result/exception → status code, headers, serialized body
    ▼
Back through proxy/CDN → client
```

Statelessness in practice: between steps 3 and 7 the server holds no memory of earlier requests from this client except what you explicitly added (a session store, a cache, a database row, a token the client sends again).

---

## Basic Example

A minimal HTTP server using only Node.js built-ins, so you can see the raw protocol before any framework:

```typescript
// server.ts
import { createServer } from 'node:http';

const users = new Map<number, { id: number; email: string }>();
let nextId = 1;

createServer(async (req, res) => {
  const url = new URL(req.url ?? '/', `http://${req.headers.host}`);
  res.setHeader('Content-Type', 'application/json');

  if (req.method === 'GET' && url.pathname === '/users') {
    res.statusCode = 200;
    return res.end(JSON.stringify([...users.values()]));
  }

  if (req.method === 'POST' && url.pathname === '/users') {
    let raw = '';
    for await (const chunk of req) raw += chunk;
    try {
      const body = JSON.parse(raw) as { email?: string };
      if (!body.email) {
        res.statusCode = 400;
        return res.end(JSON.stringify({ message: 'email is required' }));
      }
      const user = { id: nextId++, email: body.email };
      users.set(user.id, user);
      res.statusCode = 201;
      res.setHeader('Location', `/users/${user.id}`);
      return res.end(JSON.stringify(user));
    } catch {
      res.statusCode = 400;
      return res.end(JSON.stringify({ message: 'invalid JSON' }));
    }
  }

  res.statusCode = 404;
  res.end(JSON.stringify({ message: 'not found' }));
}).listen(3000, () => console.log('listening on :3000'));
```

```bash
npx ts-node server.ts

# in another terminal:
curl -i -X POST http://localhost:3000/users \
  -H 'Content-Type: application/json' \
  -d '{"email":"ada@example.com"}'

curl -i http://localhost:3000/users
```

Expected first response (headers abbreviated):

```http
HTTP/1.1 201 Created
Content-Type: application/json
Location: /users/1

{"id":1,"email":"ada@example.com"}
```

What happens:

1. Node parses the HTTP message and gives you `req` (method, URL, headers, body stream) and `res`.
2. You route by **method + path**. This is what `@Get('users')` and `@Post('users')` declare in NestJS.
3. You set a **status code** and headers (`Location`) and write a body. NestJS's `@HttpCode()`, `@Header()`, and return values do the same.
4. You reject invalid input with a `4xx`, not by crashing.

---

## Practical Examples

### 1. Basic: Read a Request with `curl -v`

```bash
curl -v https://httpbin.org/get
```

```text
> GET /get HTTP/2
> Host: httpbin.org
> User-Agent: curl/8.x
> Accept: */*
>
< HTTP/2 200
< content-type: application/json
< ...
```

`>` lines are what you sent, `<` lines are the response. `curl -v` is the fastest way to see headers, redirects, and status codes. (Output varies by curl version and server.)

### 2. Common: CRUD Mapped to Methods and Status Codes

| Operation | Request | Success | Common errors |
|---|---|---|---|
| List | `GET /users?page=1&limit=20` | `200` + array/page object | `400` bad query |
| Read | `GET /users/42` | `200` | `404` |
| Create | `POST /users` | `201` + `Location` | `400/422`, `409` duplicate |
| Replace | `PUT /users/42` | `200` or `204` | `404`, `400/422` |
| Partial update | `PATCH /users/42` | `200` or `204` | `404`, `400/422`, `409` |
| Delete | `DELETE /users/42` | `204` | `404` (or `204` if you treat delete as idempotent) |

### 3. Common: Conditional Request with `ETag`

```bash
curl -i http://localhost:3000/report
# HTTP/1.1 200 OK
# ETag: "v42"

curl -i http://localhost:3000/report -H 'If-None-Match: "v42"'
# HTTP/1.1 304 Not Modified
```

### 4. Real-World: A Consistent Error Body (RFC 9457 Problem Details)

```http
HTTP/1.1 422 Unprocessable Content
Content-Type: application/problem+json

{
  "type": "https://api.example.com/errors/validation",
  "title": "Validation failed",
  "status": 422,
  "detail": "email must be a valid email address",
  "instance": "/users",
  "errors": [{ "field": "email", "message": "must be a valid email address" }]
}
```

RFC 9457 standardizes `type`, `title`, `status`, `detail`, and `instance`, and allows extension members such as `errors`. See [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md).

### 5. Real-World: `202 Accepted` for Long-Running Work

```http
POST /reports HTTP/1.1
Content-Type: application/json

{"range":"2025-Q4"}
```

```http
HTTP/1.1 202 Accepted
Location: /reports/jobs/9f1c
Retry-After: 5
```

The client polls `GET /reports/jobs/9f1c` until it returns `200` with the result (or a status field says "done"). This pairs naturally with a job queue. See [Queues Fundamentals](../05-advanced/02-background-processing/01-queues-fundamentals.md).

### 6. Real-World: Pagination with Metadata and Links

```http
GET /orders?page=2&limit=20 HTTP/1.1
```

```json
{
  "items": [ ... ],
  "meta": { "page": 2, "limit": 20, "total": 134 }
}
```

Offset pagination (`page`/`limit`) is simple. Cursor pagination (`after=<token>`) is more stable and faster for large, changing datasets. See [Pagination](../04-intermediate/08-api-design/03-pagination.md).

### 7. Edge Case: Retrying a Non-Idempotent `POST`

```text
Client ── POST /payments (timeout, no response) ──► Server (processed it!)
Client ── POST /payments (retry) ───────────────► Server (processes it AGAIN)  ✗ double charge
```

Fix with an idempotency key:

```http
POST /payments HTTP/1.1
Idempotency-Key: 6f3a-...-b21c
```

The server stores the result under the key and returns the stored result for any repeat of the same key.

### 8. Edge Case: Preflight Failing

```text
Access to fetch at 'https://api.example.com/users' from origin 'https://app.example.com'
has been blocked by CORS policy: Response to preflight request doesn't pass access control check
```

Diagnose:

```bash
curl -i -X OPTIONS https://api.example.com/users \
  -H 'Origin: https://app.example.com' \
  -H 'Access-Control-Request-Method: POST' \
  -H 'Access-Control-Request-Headers: authorization,content-type'
```

Look for `Access-Control-Allow-Origin`, `-Methods`, and `-Headers` in the response. If they are missing or do not include your origin/method/headers, fix the server's CORS configuration.

### 9. Edge Case: Cookie Not Sent

Symptoms and checks:

| Check | Why |
|---|---|
| `Secure` cookie on `http://localhost`? | Browsers may not store or send it over plain HTTP (localhost handling differs by browser) |
| `SameSite=Lax/Strict` on a cross-site `fetch`? | Not sent. Use `SameSite=None; Secure` if cross-site is truly required |
| `fetch(..., { credentials: 'include' })` set? | Without it, browsers do not send cookies cross-origin |
| `Access-Control-Allow-Credentials: true` and exact origin (not `*`)? | Required for credentialed CORS |
| Cookie `Domain`/`Path` match the request? | Scope mismatch |

---

## Syntax / API / Commands

### `curl` Quick Reference

| Command | Purpose |
|---|---|
| `curl -i URL` | Show response headers and body |
| `curl -v URL` | Verbose: request and response, TLS details |
| `curl -I URL` | `HEAD` request (headers only) |
| `curl -X POST URL -H 'Content-Type: application/json' -d '{"a":1}'` | JSON `POST` |
| `curl -H 'Authorization: Bearer $TOKEN' URL` | Send a bearer token |
| `curl -L URL` | Follow redirects |
| `curl -c jar.txt -b jar.txt URL` | Save and send cookies |
| `curl --max-time 5 URL` | Client-side timeout |
| `curl -w '%{http_code} %{time_total}\n' -o /dev/null -s URL` | Print status and total time only |
| `curl --http2 -I https://example.com` | Request HTTP/2 (if supported) |

### Browser and Tool Options

| Tool | Use |
|---|---|
| Browser DevTools → **Network** tab | Inspect requests, headers, timing, preflights, cookies |
| Postman / Insomnia / Bruno / VS Code REST client | Compose and save requests |
| `httpie` (`http POST :3000/users email=a@b.c`) | Friendlier CLI |
| `mitmproxy`, Wireshark | Inspect traffic at proxy/packet level |

### Where HTTP Concepts Appear in NestJS

| HTTP concept | NestJS |
|---|---|
| Method + path | `@Controller('users')`, `@Get()`, `@Post(':id')` |
| Path/query/body/header | `@Param()`, `@Query()`, `@Body()`, `@Headers()` |
| Status code | `@HttpCode(204)`, `HttpException` subclasses (`NotFoundException`) |
| Response headers | `@Header('Cache-Control', 'no-store')`, `@Res({ passthrough: true })` |
| Redirect | `@Redirect('url', 301)` |
| CORS | `app.enableCors({...})` |
| Cookies | `cookie-parser`, `res.cookie()` |
| Content parsing | Body parser (JSON, urlencoded), Multer (multipart) |

See [Request Data](../02-fundamentals/07-request-data.md) and [Response Handling](../02-fundamentals/08-response-handling.md).

---

## Important Rules

1. **`GET` must be safe.** Never change state in a `GET` handler.
2. **`PUT` and `DELETE` are idempotent. `POST` is not.** Make retries safe with idempotency keys on `POST`.
3. **Use status codes truthfully.** Do not return `200` with an error body. Do not return `500` for client mistakes.
4. **`401` means unauthenticated, `403` means unauthorized.**
5. **`4xx` = client fix needed, `5xx` = server problem.** Retry logic and alerting depend on this split.
6. **HTTP is stateless.** Anything that looks like "state" (login, cart) is carried by cookies, tokens, or server-side stores keyed by an identifier.
7. **`Content-Type` tells the receiver how to parse the body.** Mismatches produce empty or unparsable bodies.
8. **Cookies are sent automatically by the browser.** That makes them convenient and exposes you to CSRF. Use `HttpOnly`, `Secure`, `SameSite`.
9. **CORS is a browser rule, not an API security feature.** It never replaces authentication or authorization.
10. **Never put secrets in URLs.** URLs appear in logs, history, and `Referer` headers. Use headers or bodies.
11. **Be careful with caching of authenticated responses.** Use `private`/`no-store` for per-user data.
12. **A query string is part of the resource identity for caching and logging.** Do not put sensitive data in it.

---

## Under the Hood

### Message Framing (HTTP/1.1)

```text
POST /users HTTP/1.1\r\n
Host: api.example.com\r\n
Content-Type: application/json\r\n
Content-Length: 17\r\n
\r\n                         ← blank line separates headers from body
{"email":"a@b.c"}
```

The receiver must know where the body ends: via `Content-Length` (exact bytes) or `Transfer-Encoding: chunked` (streamed in sized chunks, used when length is unknown). Mismatched framing between a proxy and a server is the basis of **HTTP request smuggling** attacks.

### Connections and Keep-Alive

Opening TCP + TLS is expensive (multiple round trips). **Persistent connections** (keep-alive) reuse one connection for many requests. HTTP/2 goes further and **multiplexes** many concurrent streams over one connection. Node's HTTP agents and load balancers have idle-timeout settings. A mismatch (the server closes the idle connection just as the client reuses it) produces intermittent `ECONNRESET` errors.

### Latency Components

```text
DNS lookup → TCP connect → TLS handshake → request sent → server processing → first byte (TTFB) → download
```

`curl -w` can print each phase:

```bash
curl -s -o /dev/null -w \
  'dns:%{time_namelookup} connect:%{time_connect} tls:%{time_appconnect} ttfb:%{time_starttransfer} total:%{time_total}\n' \
  https://example.com
```

### Behind a Proxy

The Node.js process sees the **proxy's** IP and (if TLS terminated at the proxy) plain HTTP. The original client IP and scheme arrive in `X-Forwarded-For` / `X-Forwarded-Proto` (or `Forwarded`). Frameworks only trust these headers when configured to (for example Express `trust proxy`). Without it, rate limiting by IP and `Secure` cookie logic can behave incorrectly. See [Reverse Proxy and Nginx](../07-production/04-deployment/04-reverse-proxy-and-nginx.md).

### Idempotency, Safety, and Intermediaries

Because `GET`/`PUT`/`DELETE` are defined as safe or idempotent, intermediaries (proxies, CDNs, browsers, HTTP client libraries) may retry or prefetch them automatically. If your `GET` mutates data or your `PUT` is not truly idempotent, these automatic behaviors cause real bugs.

---

## Common Patterns

### Pagination, Filtering, and Sorting via Query String

`GET /orders?status=paid&sort=-createdAt&page=2&limit=20` and cursor-based variants. See [Pagination](../04-intermediate/08-api-design/03-pagination.md) and [Filtering, Sorting and Search](../04-intermediate/08-api-design/04-filtering-sorting-and-search.md).

### Create → `201` + `Location`

Return the new resource (or its URL) so clients do not need a second request.

### Asynchronous Operations → `202` + Status Resource

Accept, queue, return a URL to poll (or deliver the result via webhook or WebSocket).

### Optimistic Concurrency with `ETag` and `If-Match`

```http
PUT /documents/7
If-Match: "v3"
```

If the stored version is no longer `v3`, return `412 Precondition Failed` (or `409`). This prevents lost updates between concurrent editors.

### Standard Error Envelope

Use one error shape across the whole API (RFC 9457 `application/problem+json` or a consistent custom envelope).

### Rate Limiting Signals

Return `429 Too Many Requests` with `Retry-After`. Many APIs also expose limit headers (naming varies by provider, standardization is ongoing). See [Rate Limiting](../07-production/01-security/04-rate-limiting-and-brute-force-protection.md).

### Webhooks

The server calls **your** HTTP endpoint when an event occurs. Verify signatures, respond quickly with `2xx`, process asynchronously, and expect retries (so handlers must be idempotent). See [Webhooks](../04-intermediate/08-api-design/07-webhooks.md).

---

## Common Mistakes

| Mistake | Symptom | Why It Happens | Fix |
|---|---|---|---|
| Verbs in URLs (`/getUsers`, `/deleteUser/1`) | Inconsistent, hard-to-guess API | Treating HTTP like RPC | Resource nouns + HTTP methods |
| State change in `GET` | Data deleted by crawlers, prefetch, or link previews | `GET` is assumed safe by intermediaries | Use `POST`/`PUT`/`DELETE` |
| `200 OK` with `{ "error": ... }` | Monitoring shows no failures. Clients mis-handle errors | Status code ignored | Use proper `4xx`/`5xx` |
| `500` for validation errors | Alert noise, clients retry pointlessly | Unhandled exceptions from bad input | Validate input, return `400`/`422` |
| Mixing up `401` and `403` | Clients show login screen on permission errors, or the reverse | Naming confusion | `401` unauthenticated, `403` forbidden |
| Retrying `POST` blindly | Duplicate orders/payments | `POST` is not idempotent | Idempotency keys |
| Missing/incorrect `Content-Type` | Empty `req.body`, parse errors, `415` | Server cannot select a parser | Send `Content-Type: application/json` and parse accordingly |
| "CORS error" treated as a server crash | Debugging the wrong layer | CORS is browser-enforced. The request often succeeded | Inspect the preflight in DevTools. Configure CORS headers |
| `Access-Control-Allow-Origin: *` with cookies | Browser blocks the response | Wildcard is not allowed with credentials | Echo the exact allowed origin plus `Allow-Credentials: true` |
| Cookie not set/sent | User appears logged out | `Secure`/`SameSite`/`Domain` mismatch, or missing `credentials: 'include'` | Review cookie attributes and fetch options |
| Putting tokens in query strings | Tokens leak into logs, history, referrers | URLs are widely logged | Use `Authorization` header or `HttpOnly` cookie |
| Caching authenticated responses publicly | One user sees another user's data | `public` or missing `Cache-Control` behind a shared cache | `Cache-Control: private` or `no-store` |
| Returning different shapes for errors | Clients need special-case parsing | No error contract | One standard error format |
| Deep nesting (`/a/1/b/2/c/3`) | Long, brittle URLs | Modeling relationships as paths | Flatten, use query filters |
| Unbounded list endpoints | Slow responses, memory blow-ups | No pagination | Always paginate, enforce max `limit` |
| Ignoring trailing slash or case rules | Intermittent `404` | Frameworks treat `/users` and `/users/` differently by config | Define and document one convention |
| Trusting `X-Forwarded-For` blindly | Spoofed IPs bypass rate limits | Anyone can set that header | Configure trusted proxies explicitly |
| Large JSON bodies without limits | Memory pressure, DoS | No body size limit | Set body size limits, return `413` |

---

## Debugging

### Common Errors

| Symptom | Likely cause |
|---|---|
| `400 Bad Request` with no detail | Invalid JSON, validation failure, header too large |
| `401 Unauthorized` | Missing/expired/invalid credentials. Wrong header format (`Bearer` prefix) |
| `403 Forbidden` | Authenticated but failing authorization (roles, ownership), or CSRF/WAF block |
| `404 Not Found` | Wrong path, wrong method/route prefix, missing global prefix/versioning segment |
| `405 Method Not Allowed` | Route exists for a different method |
| `413 Content Too Large` | Body limit exceeded (app, proxy, or CDN) |
| `415 Unsupported Media Type` | Wrong or missing `Content-Type` |
| `429 Too Many Requests` | Rate limit hit |
| `502 / 504` | Proxy cannot reach the app or timed out waiting for it |
| `ECONNREFUSED` | Nothing listening on that host/port (app down, wrong port) |
| `ECONNRESET` | Connection closed unexpectedly (idle timeout mismatch, crash) |
| `ETIMEDOUT` | No response within the timeout |
| `CORS policy ... blocked` | Missing/incorrect `Access-Control-*` headers on the response (including preflight) |

### Debugging Commands

```bash
# See everything: request, response, redirects
curl -v -L http://localhost:3000/users

# Check just the status and timing
curl -s -o /dev/null -w '%{http_code} %{time_total}s\n' http://localhost:3000/users

# Test a specific method with a body
curl -i -X PATCH http://localhost:3000/users/1 \
  -H 'Content-Type: application/json' -d '{"nickname":"ada"}'

# Is anything listening on the port?
lsof -i :3000          # macOS/Linux
ss -ltnp | grep 3000   # Linux

# Simulate a preflight
curl -i -X OPTIONS http://localhost:3000/users \
  -H 'Origin: http://localhost:5173' \
  -H 'Access-Control-Request-Method: POST'
```

### Techniques

1. **Reproduce with `curl` first.** If `curl` works but the browser fails, the problem is browser-specific: CORS, cookies, mixed content, or `credentials` settings.
2. **Open DevTools → Network.** Check the preflight request, the actual request, response headers, and the cookies tab.
3. **Compare each hop.** Test the app directly (`localhost:3000`) and then through the proxy. A difference points to proxy config (timeouts, body size, headers).
4. **Read the response body.** Frameworks usually explain `4xx` errors (which field failed validation).
5. **Check logs with a correlation ID.** Trace one request across proxy, app, and downstream services. See [Request Logging and Correlation ID](../07-production/03-observability/03-request-logging-and-correlation-id.md).
6. **Isolate by simplifying.** Remove headers, auth, and body until the minimal failing request is found.

---

## Performance

| Concern | Guidance |
|---|---|
| **Round trips** | Reuse connections (keep-alive, HTTP/2). Avoid chatty APIs where one request could return the needed data |
| **Payload size** | Paginate, support field selection where appropriate, enable compression (`gzip`/`br`) |
| **Caching** | `Cache-Control`, `ETag`/`304` for read-heavy public data. CDN for static and cacheable responses |
| **Time to first byte** | Dominated by server work. Measure DB and downstream calls |
| **Head-of-line blocking** | HTTP/1.1 serializes per connection. HTTP/2 and HTTP/3 reduce this |
| **Large transfers** | Stream responses (`StreamableFile`), use range requests for large files |
| **Serialization cost** | JSON stringification of large objects is synchronous and blocks the event loop. Paginate or stream |
| **Compression CPU cost** | Compression uses CPU and the thread pool. Do it at the proxy/CDN when possible |
| **Over-fetching** | Return what clients need. Consider sparse fieldsets or purpose-specific endpoints |

Quick check of compression and caching headers:

```bash
curl -s -I -H 'Accept-Encoding: gzip' https://example.com | grep -i -E 'content-encoding|cache-control|etag'
```

---

## Security

| Area | Guidance |
|---|---|
| **Transport** | HTTPS everywhere. Redirect HTTP to HTTPS and set `Strict-Transport-Security` |
| **Authentication** | Credentials in `Authorization` header or `HttpOnly; Secure` cookies. Never in URLs |
| **Authorization** | Check on every request, server-side. Never rely on hidden endpoints or CORS |
| **CSRF** | Cookie-based auth is vulnerable to cross-site requests. Use `SameSite`, CSRF tokens, or token-in-header schemes |
| **XSS** | `HttpOnly` cookies limit theft. Escape output. Use `Content-Security-Policy` |
| **CORS** | Allow-list specific origins. Do not reflect arbitrary `Origin` with credentials |
| **Input** | Validate and bound all input (size, type, range). Return `400`/`413`/`422`, not `500` |
| **Information disclosure** | Do not leak stack traces, framework versions, or internal ids in errors. Consider `404` over `403` when existence is sensitive |
| **Rate limiting** | `429` with `Retry-After`. Protect login, signup, password reset, and expensive endpoints |
| **Method handling** | Disallow unneeded methods (`TRACE`). Return `405` with `Allow` |
| **Headers** | `X-Content-Type-Options: nosniff`, `Content-Security-Policy`, `Referrer-Policy`, and related headers via Helmet |
| **Proxies** | Trust `X-Forwarded-*` only from known proxies |
| **Caching** | `no-store` for sensitive responses. Avoid caching per-user responses in shared caches |
| **Logging** | Never log `Authorization` headers, cookies, or full bodies containing secrets |

See [Security Fundamentals](../07-production/01-security/01-security-fundamentals.md) and [HTTP Security Headers and CORS](../07-production/01-security/02-http-security-headers-and-cors.md).

---

## Production Considerations

- **TLS termination:** usually at a load balancer or reverse proxy. Keep certificates automated and monitored for expiry.
- **Timeouts:** set and align them across layers (client, CDN, proxy, app, database). A proxy timeout shorter than your slowest legitimate request produces `504`s.
- **Body size limits:** configure both at the proxy and in the application. Return `413`.
- **Health endpoints:** expose liveness and readiness endpoints for load balancers. See [Health Checks](../07-production/03-observability/04-health-checks.md).
- **Compression and HTTP/2/3:** handle at the edge when possible.
- **Idempotency and retries:** clients and gateways will retry. Design handlers to tolerate duplicates.
- **Versioning and backward compatibility:** never break clients silently. Add fields instead of changing them. Deprecate with notice. Use `Deprecation`/`Sunset` style headers where applicable.
- **Observability:** log method, path (without secrets), status, latency, and a request/correlation ID. Track error rates per status class (`4xx` vs `5xx`) and latency percentiles.
- **Caching strategy:** define `Cache-Control` intentionally per endpoint type (static, public data, private data).
- **API documentation:** publish OpenAPI/Swagger so contracts are explicit. See [OpenAPI and Swagger](../04-intermediate/09-openapi-and-swagger/README.md).
- **Graceful degradation:** return `503` with `Retry-After` during planned maintenance or overload instead of dropping connections.

---

## Best Practices

### Recommended

```http
POST /orders HTTP/1.1
Content-Type: application/json
Idempotency-Key: 6f3a-...

{"items":[{"sku":"A1","qty":2}]}
```

```http
HTTP/1.1 201 Created
Location: /orders/981
Content-Type: application/json

{"id":981,"status":"pending"}
```

```http
HTTP/1.1 404 Not Found
Content-Type: application/problem+json

{"type":"about:blank","title":"Not Found","status":404,"detail":"Order 981 does not exist"}
```

### Avoid

```http
POST /createOrder HTTP/1.1                 ← verb in URL
```

```http
HTTP/1.1 200 OK

{"success":false,"error":"Order not found"}   ← success status for a failure
```

```http
GET /users/delete?id=42 HTTP/1.1           ← state change through GET
GET /login?token=SECRET                     ← secret in the URL
```

Why: the recommended forms are predictable, cache- and retry-aware, and monitorable. The avoided forms break the uniform interface, hide failures from tooling, and leak or endanger data.

Additional guidance:

- Use plural nouns and consistent casing in URLs.
- Return the created or updated resource when it saves a round trip.
- Always paginate collections and cap `limit`.
- Use one error format across the entire API.
- Set `Content-Type` explicitly on every response with a body.
- Prefer `PATCH` for partial updates and `PUT` for full replacement, and document which fields are mutable.
- Do not expose database internals (auto-increment ids can be enumerated). Consider UUIDs or opaque ids where enumeration is a concern, and always enforce authorization.
- Document behavior for retries, rate limits, and error codes.

---

## Version / Compatibility Notes

| Specification / Version | Notes |
|---|---|
| RFC 9110 (HTTP Semantics, 2022) | Current consolidated definition of methods, status codes, and header semantics. Obsoletes earlier RFC 7231 and related documents |
| RFC 9111 (HTTP Caching) | Current caching specification |
| RFC 9112 (HTTP/1.1) | HTTP/1.1 message syntax and connection management |
| RFC 9113 (HTTP/2) | HTTP/2 |
| RFC 9114 (HTTP/3) | HTTP/3 over QUIC |
| RFC 9457 (Problem Details for HTTP APIs) | Obsoletes RFC 7807. Media type `application/problem+json` |
| `422` status name | Historically "Unprocessable Entity", renamed "Unprocessable Content" in RFC 9110. The code is the same |
| `413` status name | Historically "Payload Too Large", renamed "Content Too Large" in RFC 9110. The code is the same |
| `SameSite` default | Many browsers treat cookies without `SameSite` as `Lax`. Behavior and details vary by browser and version. *Verify against current browser documentation* |
| `Idempotency-Key` header | Widely used convention. A standardization effort exists. *Verify current status before depending on a specific header format* |

- The RFC numbers above are as commonly cited. *Verify against the IETF RFC index if you rely on them in formal documentation.*
- **Specification vs implementation:** methods, status codes, and header semantics are specified by the HTTP RFCs. Defaults such as cookie `SameSite` handling, CORS preflight caching limits, and connection idle timeouts are **implementation** behavior that varies across browsers, servers, and proxies.
- NestJS runs on Express by default (or Fastify via an adapter). Some details (body parsing, trust proxy, header casing) depend on the adapter. See [Platform Adapters](../06-internals/05-platform-adapters.md).

---

## Real-World Use Cases

- **Public and private REST APIs:** mobile and web backends, built with NestJS controllers.
- **Microservices:** synchronous service-to-service calls over HTTP (with timeouts, retries, circuit breakers).
- **Webhooks:** payment providers, Git hosts, and messaging platforms calling your endpoints.
- **File delivery and uploads:** `multipart/form-data` uploads, range requests, presigned URLs.
- **Browser applications:** SPAs calling APIs across origins, requiring correct CORS and cookie settings.
- **Edge caching and CDNs:** `Cache-Control`/`ETag` determine what the CDN may store.
- **API gateways:** auth, rate limiting, routing, and protocol translation in front of services.
- **Server-sent events and streaming:** long-lived HTTP responses (for example LLM token streaming).

---

## Interview Questions

### Beginner

1. What is HTTP, and what does "stateless" mean?
   - A request/response protocol. Each request is independent. The server keeps no memory of previous requests unless state is carried explicitly (cookies, tokens, session stores).
2. What is the difference between `GET` and `POST`?
   - `GET` retrieves data and is safe and idempotent. `POST` submits data or triggers an action, is not safe, and not idempotent.
3. What do status code classes `2xx`, `4xx`, and `5xx` mean?
   - Success, client error, server error.
4. What is a REST resource?
   - A named thing (user, order) identified by a URL, manipulated through HTTP methods.

### Intermediate

1. What does idempotent mean? Which methods are idempotent?
   - Repeating the request has the same intended server-side effect as sending it once. `GET`, `HEAD`, `OPTIONS`, `PUT`, `DELETE`. Not `POST`, and `PATCH` is not guaranteed.
2. `401` vs `403`?
   - `401`: not authenticated. `403`: authenticated but not allowed.
3. `PUT` vs `PATCH`?
   - `PUT` replaces the whole resource. `PATCH` applies a partial change.
4. How do `HttpOnly`, `Secure`, and `SameSite` protect cookies?
   - `HttpOnly` hides it from JavaScript, `Secure` restricts it to HTTPS, `SameSite` limits cross-site sending (CSRF mitigation).
5. What is a CORS preflight and when does it happen?
   - A browser-sent `OPTIONS` request before a non-simple cross-origin request, asking the server whether the actual request is allowed.
6. Which status code do you return for a created resource and what header goes with it?
   - `201 Created` with `Location`.

### Advanced

1. How do you make `POST /payments` safe to retry?
   - Client sends an `Idempotency-Key`. The server stores the outcome keyed by it (plus a request fingerprint) and returns the stored result for repeats, with concurrency control to handle simultaneous duplicates.
2. Explain `Cache-Control: no-cache` vs `no-store`.
   - `no-cache` allows storing but requires revalidation. `no-store` forbids storing.
3. How does ETag-based conditional `GET` and `If-Match` for updates work?
   - `If-None-Match` returns `304` when unchanged. `If-Match` on writes enables optimistic concurrency, returning `412` on version mismatch.
4. Why is CORS not a security mechanism for your API?
   - Browsers enforce it. Non-browser clients ignore it. Authentication and authorization must be enforced on the server.
5. What is HTTP request smuggling, and which layer boundaries cause it?
   - Disagreement between a front-end proxy and back-end server about message boundaries (`Content-Length` vs `Transfer-Encoding`).
6. HTTP/1.1 vs HTTP/2 vs HTTP/3?
   - Text framing and per-connection serialization; binary multiplexing over one TCP connection; HTTP over QUIC/UDP removing TCP-level head-of-line blocking.
7. How would you design long-running operations over HTTP?
   - Return `202 Accepted` with a status resource URL, process via a queue, and let clients poll, receive a webhook, or subscribe via WebSocket/SSE.

---

## Quick Reference

```text
Safe methods         GET, HEAD, OPTIONS
Idempotent methods   GET, HEAD, OPTIONS, PUT, DELETE   (POST ✗, PATCH not guaranteed)

200 OK               success with body              201 Created   + Location
202 Accepted         async, check later             204 No Content
301/308 permanent    302/307 temporary              304 Not Modified (cache hit)
400 bad request      401 unauthenticated            403 forbidden
404 not found        405 method not allowed         409 conflict
413 too large        415 unsupported media type     422 invalid data
429 rate limited (+Retry-After)
500 server error     502 bad gateway   503 unavailable   504 gateway timeout

URL                  /resource/{id}?filter=...&page=...     plural nouns, no verbs
Cookie flags         HttpOnly  Secure  SameSite=Lax|Strict|None(+Secure)
Cache-Control        max-age  no-cache(revalidate)  no-store  private  public
Conditional          ETag + If-None-Match → 304      ETag + If-Match → 412
CORS                 Origin → Access-Control-Allow-Origin / -Methods / -Headers / -Credentials
                     preflight = OPTIONS before non-simple cross-origin requests
Auth header          Authorization: Bearer <token>
Error format         application/problem+json (RFC 9457)
Debug                curl -v   |   DevTools → Network   |   curl -w '%{http_code} %{time_total}'
```

---

## Key Takeaways

- HTTP is a stateless request/response protocol. A request is *method + URL + headers + optional body*. A response is *status + headers + optional body*.
- Methods carry guarantees: `GET` is safe, `PUT`/`DELETE` are idempotent, `POST` is neither. Retries, caches, and proxies rely on that.
- Status codes are part of your API contract. Use them truthfully (`401` vs `403`, `4xx` vs `5xx`, never `200` for errors).
- Model REST APIs around resources (plural nouns) and let HTTP methods supply the verbs. Most production APIs are "REST-ish", not fully hypermedia-driven.
- Cookies are auto-sent by browsers. Use `HttpOnly`, `Secure`, and `SameSite`, and understand the CSRF trade-off.
- CORS is enforced by browsers. A CORS error means missing or incorrect response headers, not necessarily a server failure, and it is never a substitute for auth.
- Caching (`Cache-Control`, `ETag`) is powerful and dangerous. Be explicit, especially for authenticated data.
- Behind proxies, the app sees proxy details. Configure trusted proxy handling for client IP and scheme.
- Debug with `curl -v` and the browser Network tab before suspecting the framework.

---

## Related Topics

```text
03 Node.js Async and Event Loop
      ↓
[04 HTTP and REST Basics]
      ↓
01-getting-started  →  02-fundamentals (Controllers, Request Data, Response Handling)
      ↓
04-intermediate/08-api-design  →  07-production/01-security
```

- [Prerequisites Overview](./README.md)
- [Node.js Async and Event Loop](./03-nodejs-async-and-event-loop.md)
- [Controllers](../02-fundamentals/03-controllers.md)
- [Request Data](../02-fundamentals/07-request-data.md)
- [Response Handling](../02-fundamentals/08-response-handling.md)
- [REST and Resource Design](../04-intermediate/08-api-design/01-rest-and-resource-design.md)
- [Response and Error Format](../04-intermediate/08-api-design/05-response-and-error-format.md)
- [Idempotency](../04-intermediate/08-api-design/06-idempotency.md)
- [HTTP Security Headers and CORS](../07-production/01-security/02-http-security-headers-and-cors.md)
- [CSRF](../07-production/01-security/03-csrf.md)
