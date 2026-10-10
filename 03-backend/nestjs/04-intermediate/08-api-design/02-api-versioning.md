# API Versioning

Versioning lets you make **breaking changes** without breaking existing clients: old clients keep calling `v1` while new clients adopt `v2`. It's a tool for a specific problem, not a default ritual. The best versioning strategy is usually **not needing a new version**: evolve additively, deprecate gently, and reserve new versions for real breaks.

Prerequisites: [REST and resource design](./01-rest-and-resource-design.md), [controllers](../../02-fundamentals/03-controllers.md).

## What counts as a breaking change

| Breaking | Non-breaking (additive) |
|----------|-------------------------|
| Removing or renaming a field/endpoint | Adding a new optional response field |
| Changing a field's type or meaning | Adding a new endpoint |
| Making an optional request field required | Adding a new optional request field |
| Tightening validation (rejecting previously valid input) | Loosening validation |
| Changing status codes, error shapes, or auth requirements | Adding new enum values *(only safe if clients tolerate unknown values; document it)* |
| Changing pagination/sort defaults | Performance improvements |

Tell clients up front: **ignore unknown fields** and **tolerate new enum values**. That one convention prevents most "breaking" additive changes from breaking anyone.

## Strategies

| Strategy | Example | Pros | Cons |
|----------|---------|------|------|
| **URI** | `/v1/orders`, `/v2/orders` | Obvious, easy to route, cache, test, and document; visible in logs | Version is part of the "resource identity"; URL changes between versions |
| **Header** | `X-API-Version: 2` | Clean URLs | Invisible in the browser, harder to test with `curl`, caching must `Vary` on the header |
| **Media type** | `Accept: application/vnd.myapp.v2+json` | Arguably "purest" REST; per-representation versioning | Least discoverable; tooling and clients find it awkward |
| **Query param** | `/orders?version=2` | Easy to try | Pollutes URLs and caching; usually discouraged |

**URI versioning is the pragmatic default** for public and partner APIs because it's the easiest for humans and tools. Use header or media-type versioning if you have a strong reason (or an existing convention).

## Versioning in Nest

Enable it once at bootstrap:

```ts
// main.ts
import { VersioningType } from '@nestjs/common';

app.setGlobalPrefix('api');
app.enableVersioning({
  type: VersioningType.URI,          // /api/v1/...
  defaultVersion: '1',               // routes without an explicit version get this
});
```

(Nest places the version after the global prefix: `/api/v1/orders`. The URI type defaults to the prefix `v`; you can change or drop it with the `prefix` option. Check the docs for the exact options in your version.)

Declare versions per **controller** or per **route**:

```ts
@Controller({ path: 'orders', version: '1' })
export class OrdersV1Controller { /* GET /api/v1/orders */ }

@Controller({ path: 'orders', version: '2' })
export class OrdersV2Controller { /* GET /api/v2/orders */ }

// or: one controller, route-level overrides
@Controller('orders')
export class OrdersController {
  @Get()                       // uses defaultVersion
  listV1() {}

  @Version('2')
  @Get()
  listV2() {}

  @Version(['1', '2'])         // same handler for several versions
  @Get(':id')
  findOne() {}
}
```

Routes that must work **regardless of version** (health checks, webhooks):

```ts
import { VERSION_NEUTRAL } from '@nestjs/common';

@Controller({ path: 'health', version: VERSION_NEUTRAL })
export class HealthController {}
```

### Other types

```ts
// Header
app.enableVersioning({ type: VersioningType.HEADER, header: 'X-API-Version' });

// Media type
app.enableVersioning({ type: VersioningType.MEDIA_TYPE, key: 'v=' });
// client: Accept: application/json;v=2

// Custom: extract the version from the request any way you like
app.enableVersioning({
  type: VersioningType.CUSTOM,
  extractor: (req: any) => req.headers['x-tenant-api-version'] ?? '1',
});
```

For header/media-type versioning, set `Vary` on the header (so caches don't mix versions) and decide what an **unversioned** request gets (usually the default or oldest stable version, **not** "latest", which silently breaks clients on release).

## Structuring code across versions

Avoid copy-pasting a whole codebase per version:

```text
src/orders/
├── orders.service.ts           ← one implementation of the business logic
├── v1/
│   ├── orders.controller.ts    ← thin: maps v1 DTOs to/from the service
│   └── dto/
├── v2/
│   ├── orders.controller.ts
│   └── dto/
```

- **Version the edges, not the core**: controllers and DTOs (the contract) differ per version; services and domain logic stay shared.
- Map between **version-specific DTOs** and your internal model in the controller (or a mapper), so a v2 change doesn't leak into v1.
- If versions diverge heavily in behavior, that's a sign the "versions" are really different products; extract shared domain logic and keep separate adapters.
- Version-specific **validation**, serialization, and Swagger documents should be separate so each version's spec is accurate ([Swagger setup](../09-openapi-and-swagger/01-swagger-setup.md)).

## Evolving without a new version

Before cutting `v2`, ask whether an additive path works:

- **Add** the new field next to the old; keep the old one until clients migrate.
- **Add** a new endpoint for the new behavior; leave the old.
- Accept **both** old and new request shapes during a transition.
- Use **feature negotiation** (an opt-in flag/header) for risky changes.

Every live version multiplies your testing, documentation, monitoring, and security-patching burden. Fewer versions is better.

## Deprecation and sunsetting

Treat retirement as a process:

1. **Announce** (docs, changelog, email to registered clients) with a **date**.
2. **Signal in responses**: the `Deprecation` and `Sunset` HTTP headers (standardized in RFCs; check current specifications) plus a `Link` header to migration docs.
3. **Measure usage** per version (and per client/API key) so you know who's still on the old one.
4. **Brown out** if needed (temporary short failures) to flush out forgotten callers, then **turn it off** after the sunset date, returning `410 Gone` with a helpful message.

```ts
@Version('1')
@Get()
list(@Res({ passthrough: true }) res: Response) {
  res.setHeader('Deprecation', 'true');
  res.setHeader('Sunset', 'Wed, 31 Dec 2026 23:59:59 GMT');
  return this.orders.list();
}
```

(The exact header value formats are specified in the relevant RFCs; verify before shipping.) A reusable [interceptor](../../03-core-concepts/01-request-pipeline/05-interceptors.md) can add these headers for all routes of a version.

## Internal and microservice APIs

For APIs consumed only by code you control and deploy together, versioning is often unnecessary: change both sides in one release. It matters when clients **can't be updated in lockstep** (mobile apps in the wild, partners, long-lived integrations). For service-to-service calls in a microservice setup, prefer **backward-compatible schema evolution** (and, for gRPC/Protobuf, its compatibility rules) over URL versions ([gRPC](../../05-advanced/06-microservices/06-grpc-fundamentals-and-protobuf.md)).

## Testing

- **Contract tests** per version: snapshot the OpenAPI spec per version and fail CI on breaking diffs (tools exist to diff OpenAPI documents).
- E2E tests that call each live version and assert status codes and response shapes ([E2E testing](../01-testing/06-e2e-testing.md)).
- Confirm version-neutral routes work with and without version info.

## Common mistakes

- **Creating a new version for every change**, or never versioning and then breaking clients.
- **"Latest by default"** for unversioned requests.
- **Copy-pasting entire modules per version** instead of versioning the edges.
- **Forgetting `VERSION_NEUTRAL`** for health checks/webhooks, so they 404 under versioning.
- **Header/media-type versions without `Vary`**, causing cache poisoning across versions.
- **No deprecation policy or usage metrics**, making old versions impossible to retire.
- **Treating "added enum value" or "stricter validation"** as non-breaking.
- **Versioning in several places at once** (URL and header and media type), creating ambiguity.

## Debugging

- `404` after enabling versioning: the route has no version and no `defaultVersion` is set, or the client omitted the version (`/api/orders` instead of `/api/v1/orders`), or the global prefix order is misunderstood.
- Wrong version served: check `defaultVersion`, route-level `@Version`, and (for header/media-type) the exact header key.
- Health check broken after enabling versioning: mark it `VERSION_NEUTRAL`.
- Swagger shows mixed versions: generate separate documents per version.

## Quick Summary

- Version only for **breaking** changes; prefer additive evolution and tell clients to ignore unknown fields.
- **URI versioning** is the pragmatic default; header/media-type are alternatives with caching and discoverability costs.
- In Nest: `app.enableVersioning({ type, defaultVersion })`, `@Controller({ version })`, `@Version()`, `VERSION_NEUTRAL`.
- Version the **edges** (controllers/DTOs), share services; keep the number of live versions small.
- Plan **deprecation** (announce, `Deprecation`/`Sunset` headers, usage metrics, brownouts, `410`), and test with per-version contract/E2E tests.

## Next

[Pagination →](./03-pagination.md)
