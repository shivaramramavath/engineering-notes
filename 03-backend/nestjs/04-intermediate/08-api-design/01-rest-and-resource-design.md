# REST and Resource Design

REST-style HTTP APIs model your domain as **resources** (things with identity: users, orders, invoices) addressed by URLs and manipulated with a small set of HTTP **methods**. Most "REST" APIs are really "resource-oriented HTTP APIs", and that's fine. The goal isn't purity; it's an API that's **predictable**: a developer who has seen one endpoint can guess the rest.

Prerequisites: [HTTP and REST basics](../../00-prerequisites/04-http-and-rest-basics.md), [controllers](../../02-fundamentals/03-controllers.md).

## Resources and URLs

Use **nouns** (plural collections), not verbs; the HTTP method is the verb.

```text
GET    /orders              list orders
POST   /orders              create an order
GET    /orders/42           read one order
PUT    /orders/42           replace an order
PATCH  /orders/42           partially update an order
DELETE /orders/42           delete an order

GET    /orders/42/items     list the items of order 42   (sub-resource)
POST   /orders/42/items     add an item
GET    /users/7/orders      orders belonging to user 7
```

Guidelines:

- **Plural nouns** consistently (`/orders`, not `/order` sometimes), lowercase, hyphenated for multiword paths (`/purchase-orders`).
- **Identify by id in the path**, filter by attributes in the query string (`/orders?status=paid`), see [filtering](./04-filtering-sorting-and-search.md).
- **Limit nesting to one or two levels.** `/users/7/orders/42/items/3/refunds` is brittle. Once a child has its own identity, expose it at the top level too (`/order-items/3`) and link.
- **No verbs and no file extensions** (`/getOrders`, `/orders.json`).
- **Stable, opaque ids**, never leaking sequential internals if enumeration matters (and always authorize regardless: [BOLA](../07-authorization/01-authorization-fundamentals.md)).
- Don't expose your **database schema** 1:1. Design resources around client use cases, then map ([DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md)).

## Methods: semantics that clients and infrastructure rely on

| Method | Meaning | Safe? | Idempotent? | Body |
|--------|---------|-------|-------------|------|
| `GET` | Read | **Yes** (no side effects) | Yes | None |
| `HEAD` | Like GET without the body | Yes | Yes | None |
| `POST` | Create / perform an action | No | **No** | Yes |
| `PUT` | Replace the resource at a known URL | No | **Yes** | Full representation |
| `PATCH` | Partial update | No | Not guaranteed | Partial changes |
| `DELETE` | Remove | No | **Yes** | Usually none |

- **Safe** means a client may call it freely (prefetching, crawlers, caches assume this). **Never change state on GET.**
- **Idempotent** means repeating the request has the same effect as sending it once, which is what makes automatic retries safe. `POST` isn't, so duplicate submissions need [idempotency keys](./06-idempotency.md).
- `DELETE` on an already-deleted resource: either `404` or `204` is defensible; pick one and be consistent (idempotence is about **state**, not identical responses).

### PUT vs PATCH

```http
PUT /users/7        {"name":"Ann","email":"a@x.com","bio":"hi"}      ← replaces; omitted fields are reset/cleared
PATCH /users/7      {"bio":"hi"}                                      ← changes only what's provided
```

Most real-world updates are `PATCH`. In Nest, define a separate `UpdateXDto` where every field is optional ([mapped types](../../03-core-concepts/02-validation-and-serialization/01-dto.md)). Decide how to represent "clear this field": usually explicit `null` means clear, an absent key means leave unchanged. (The JSON Merge Patch media type `application/merge-patch+json` formalizes this; JSON Patch `application/json-patch+json` describes operation lists. Plain partial JSON is common and acceptable if you document the rules.)

## Status codes: say what happened

| Code | Use |
|------|-----|
| `200 OK` | Success with a body |
| `201 Created` | Resource created; include a `Location` header and usually the new resource |
| `202 Accepted` | Request accepted for **asynchronous** processing |
| `204 No Content` | Success, no body (DELETE, some updates) |
| `301/302/307/308` | Redirects |
| `304 Not Modified` | Conditional GET: cached copy still valid |
| `400 Bad Request` | Malformed or invalid request |
| `401 Unauthorized` | Not authenticated |
| `403 Forbidden` | Authenticated but not allowed |
| `404 Not Found` | Resource doesn't exist (or hidden) |
| `405 Method Not Allowed` | Wrong method for this URL |
| `409 Conflict` | Conflicts with current state (duplicate, version mismatch) |
| `410 Gone` | Permanently removed |
| `412 Precondition Failed` | `If-Match`/`If-Unmodified-Since` failed |
| `415 Unsupported Media Type` | Unsupported `Content-Type` |
| `422 Unprocessable Content` | Well-formed but semantically invalid (many APIs use `400` for validation instead; pick one) |
| `429 Too Many Requests` | Rate limited; send `Retry-After` |
| `500/502/503/504` | Server-side failures |

Don't return `200` with `{ "error": ... }`. Clients, proxies, and monitoring rely on status codes. Don't invent semantics (`418` for business errors). In Nest:

```ts
@Post()
@HttpCode(201)                                                     // POST defaults to 201 in Nest already
async create(@Body() dto: CreateOrderDto, @Res({ passthrough: true }) res: Response) {
  const order = await this.orders.create(dto);
  res.location(`/orders/${order.id}`);                             // Location header for the new resource
  return order;
}

@Delete(':id')
@HttpCode(204)
remove(@Param('id') id: string) { return this.orders.remove(id); }
```

Nest defaults: `GET` 200, `POST` 201, and other methods 200; override with `@HttpCode()` where needed. Use [HTTP exceptions](../../03-core-concepts/01-request-pipeline/07-http-exceptions.md) for error statuses.

## Actions that don't fit CRUD

Some operations are commands, not state edits: cancel an order, publish a post, reset a password. Model them as **sub-resources or action endpoints with POST**:

```text
POST /orders/42/cancel
POST /posts/9/publish
POST /users/7/password-reset
POST /payments/5/refunds          ← creates a refund resource (preferred when the result has identity)
```

Prefer creating a resource (`/refunds`) when the action produces something you'll want to read, list, or track. Use a verb sub-path when it's a pure command. Avoid smuggling actions into `PATCH` with magic fields (`{"status":"cancelled"}` is OK when you validate allowed transitions server-side, and the state machine is simple).

## Long-running work: `202 Accepted`

If processing takes longer than a request should, accept and process in the background:

```http
POST /reports            → 202 Accepted
                           Location: /reports/abc123      ← where to check progress
GET  /reports/abc123     → 200 { "status": "processing" }   (later: "done", with a result link)
```

Don't hold the HTTP request open for minutes. Pair with queues ([background processing](../../05-advanced/02-background-processing/01-queues-fundamentals.md)) and, for push notification of completion, [webhooks](./07-webhooks.md).

## Representations and conventions

- **JSON** with consistent naming (`camelCase` or `snake_case`; pick one and never mix).
- **Dates and times:** ISO 8601 in **UTC** (`2025-01-15T10:30:00Z`); never locale formats or bare epoch numbers unless documented.
- **Money:** integer minor units (`amountCents: 1999`) plus a currency code, or decimal strings; never floats.
- **IDs as strings** (even if numeric internally) to avoid JavaScript precision issues with large integers.
- **Booleans are booleans**, not `"true"`/`"yes"`/`1`.
- **`null` vs absent:** document the difference; be consistent about whether empty fields are omitted or `null`.
- **Enums as stable strings** (`"paid"`), documented; new values are a compatibility concern ([versioning](./02-api-versioning.md)).
- **Content negotiation:** respect `Accept`/`Content-Type`; return `415`/`406` appropriately.
- **Idempotent, cache-aware reads:** support `ETag`/`If-None-Match` or `Cache-Control` where it makes sense.
- **Concurrency control for updates:** `ETag` + `If-Match` (return `412` on mismatch) prevents lost updates.

## Designing for evolution (cheap habits)

- **Additive changes are safe**: new optional response fields, new endpoints, new optional request fields. Tell clients to **ignore unknown fields**.
- **Breaking changes**: removing/renaming fields, changing types or meanings, making optional input required, tightening validation, changing status codes or error shapes. These need [versioning](./02-api-versioning.md).
- **Return objects, not bare arrays or scalars**, at the top level (`{ "items": [...] }`), so you can add metadata like pagination later without breaking clients ([pagination](./03-pagination.md)).
- **Don't expose internal ids, enum names, or table structure** you may want to change.
- **Document every endpoint** with OpenAPI ([Swagger](../09-openapi-and-swagger/README.md)) and treat the spec as the contract.

## Nest implementation shape

```ts
@Controller('orders')
export class OrdersController {
  constructor(private readonly orders: OrdersService) {}

  @Get()
  list(@Query() query: ListOrdersQueryDto) { return this.orders.list(query); }

  @Get(':id')
  findOne(@Param('id', ParseUUIDPipe) id: string) { return this.orders.findOne(id); }

  @Post()
  create(@Body() dto: CreateOrderDto, @CurrentUser() user: AuthUser) { return this.orders.create(dto, user); }

  @Patch(':id')
  update(@Param('id', ParseUUIDPipe) id: string, @Body() dto: UpdateOrderDto) { return this.orders.update(id, dto); }

  @Post(':id/cancel')
  @HttpCode(200)
  cancel(@Param('id', ParseUUIDPipe) id: string) { return this.orders.cancel(id); }

  @Delete(':id')
  @HttpCode(204)
  remove(@Param('id', ParseUUIDPipe) id: string) { return this.orders.remove(id); }
}
```

Controllers stay thin: HTTP concerns only (parsing, DTOs, status codes); behavior lives in services ([architecture](../02-database-foundations/01-database-architecture.md)). Route order matters: declare static routes (`/orders/stats`) **before** dynamic ones (`/orders/:id`), or `:id` will swallow them.

## Common mistakes

- **Verbs in URLs** (`/createOrder`) and inconsistent plural/singular naming.
- **State changes on `GET`.**
- **`200` for errors**, or wrong status codes (`500` for validation failures, `403` for unauthenticated).
- **Using `PUT` as partial update** (silently clearing omitted fields).
- **Deeply nested URLs.**
- **Bare array responses** that can't grow metadata.
- **Floats for money, locale-formatted dates, numeric ids that overflow JS numbers.**
- **Mixed naming conventions** (`createdAt` here, `created_at` there).
- **Exposing database rows** directly, coupling clients to your schema.
- **Static routes declared after `:id` routes** in a controller.
- **Long synchronous requests** that should be `202` + background work.

## Debugging

- A route returns the wrong handler's result: check declaration order (`/orders/stats` vs `/orders/:id`).
- A `POST` returns `201` when you wanted `200` (or vice versa): add `@HttpCode()`.
- `Location` header missing: use `@Res({ passthrough: true })` and `res.location(...)`.
- Clients surprised by changes: look for non-additive changes; add contract tests against the OpenAPI spec.
- Inspect real traffic with `curl -i` to see status codes and headers exactly as clients do.

## Quick Summary

- Model **resources** with plural-noun URLs; methods carry the verb; limit nesting; filter via query strings.
- Know **safe** and **idempotent** methods; never mutate on GET; PATCH for partial updates, PUT for replacement.
- Use accurate **status codes** (201 + `Location`, 202 for async, 204, 409, 422/400, 429); never `200` with an error body.
- Model commands as `POST` sub-resources or action endpoints; use `202` + status resources for long-running work.
- Standardize representations (UTC ISO dates, integer money, string ids, consistent casing); prefer additive evolution; return objects, not bare arrays.

## Next

[API versioning →](./02-api-versioning.md)
