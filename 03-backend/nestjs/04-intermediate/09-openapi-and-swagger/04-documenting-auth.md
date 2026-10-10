# Documenting Authentication

OpenAPI separates two ideas: **security schemes** (how authentication works: a bearer token, an API key, a cookie, OAuth 2.0) and **security requirements** (which operations need which scheme). Documenting both correctly is what makes Swagger UI's **Authorize** button work and what lets generated SDKs know how to authenticate.

Prerequisites: [Swagger setup](./01-swagger-setup.md), [documenting endpoints](./02-documenting-endpoints.md), [authentication](../06-authentication/README.md).

> Documentation does **not** enforce anything. `@ApiBearerAuth()` doesn't protect a route; your guards do. The two must be kept in sync ([composed decorators](#keeping-docs-and-guards-in-sync)).

## Two steps: define the scheme, apply the requirement

```ts
// 1. define schemes on the document (main.ts)
const config = new DocumentBuilder()
  .setTitle('Orders API')
  .addBearerAuth(
    { type: 'http', scheme: 'bearer', bearerFormat: 'JWT', description: 'Access token from POST /auth/login' },
    'access-token',                           // the scheme's name; reuse it in @ApiBearerAuth
  )
  .build();
```

```ts
// 2. apply the requirement where it's needed
@ApiBearerAuth('access-token')                // controller level: every route in it requires the token
@Controller('orders')
export class OrdersController {}
```

If you call `addBearerAuth()` with no arguments, the scheme is named `bearer`, and `@ApiBearerAuth()` with no arguments refers to it. Using **explicit names** avoids mismatches when you have several schemes.

A mismatch is the classic bug: the name in `@ApiBearerAuth('x')` must match a scheme defined in the document, or the lock icon appears but "Authorize" has nothing to attach.

## Schemes `DocumentBuilder` supports

```ts
.addBearerAuth(options?, name?)       // Authorization: Bearer <token> (JWT or opaque)
.addBasicAuth(options?, name?)        // HTTP Basic
.addApiKey({ type: 'apiKey', name: 'X-API-Key', in: 'header' }, 'api-key')   // header/query/cookie API key
.addCookieAuth('sid', { type: 'apiKey', in: 'cookie', name: 'sid' }, 'session')   // cookie-based
.addOAuth2({ type: 'oauth2', flows: { authorizationCode: { authorizationUrl, tokenUrl, scopes: { 'read:orders': 'Read orders' } } } }, 'oauth2')
.addSecurity('custom', { type: 'http', scheme: 'digest' })   // any other OpenAPI scheme
```

Matching decorators on controllers/routes:

| Scheme | Decorator |
|--------|-----------|
| Bearer | `@ApiBearerAuth(name?)` |
| Basic | `@ApiBasicAuth(name?)` |
| API key | `@ApiSecurity('api-key')` |
| Cookie | `@ApiCookieAuth(name?)` |
| OAuth 2.0 | `@ApiOAuth2(['read:orders'], 'oauth2')` (scopes per operation) |
| Anything | `@ApiSecurity(name, scopes?)` |

Notes per scheme:

- **Bearer (JWT):** `bearerFormat: 'JWT'` is informational. Swagger UI's Authorize dialog asks for the token and sends `Authorization: Bearer <token>` on "Try it out" ([JWT](../06-authentication/04-jwt.md)).
- **API key:** `in: 'header' | 'query' | 'cookie'`. Avoid query-string keys in real APIs (they leak into logs); the docs should reflect what your API actually accepts.
- **Cookie:** Swagger UI cannot reliably send `HttpOnly` session cookies set by another origin; for cookie/session APIs "Try it out" works best when the UI is served from the same origin as the API and you've logged in through the browser ([cookie authentication](../06-authentication/06-cookie-authentication.md)).
- **OAuth 2.0:** declare the flow (authorization code, client credentials) with URLs and scopes; Swagger UI can then run the flow. It needs a registered client and redirect URI for the docs page. Scopes should match your actual permission model ([OAuth 2.0](../06-authentication/08-oauth2.md), [authorization](../07-authorization/README.md)).

## Applying requirements: per controller, per route, or globally

```ts
@ApiBearerAuth('access-token')                  // whole controller
@Controller('orders')
export class OrdersController {
  @Get()
  list() {}                                     // requires the token

  @ApiBearerAuth('admin-token')                 // can add a second scheme on one route
  @Delete(':id')
  remove() {}
}
```

Multiple decorators combine as **alternatives or both** depending on how they're declared; OpenAPI security requirements are "any of these sets". If an endpoint accepts *either* a bearer token *or* an API key, document them as alternatives; if it needs *both*, they belong in the same requirement object (use `@ApiSecurity` with care and check the generated JSON).

### Global requirement with public exceptions

Most APIs protect everything except login, register, health checks, and webhooks, mirroring a global auth guard with `@Public()` ([guards](../../03-core-concepts/01-request-pipeline/04-guards.md)). Documentation should mirror that:

```ts
// make every operation require the bearer scheme by default
new DocumentBuilder().addBearerAuth({ type: 'http', scheme: 'bearer' }, 'access-token')
  .addSecurityRequirements('access-token')     // available in recent versions; check yours
  .build();
```

Then public routes must **remove** the requirement. How cleanly that's done depends on your `@nestjs/swagger` version (an empty security requirement on the operation overrides the global one). Because this is fiddly, many teams do the opposite: don't add a global requirement, and apply documentation through a **composed `@Auth()` decorator** used on protected routes (below). Whichever you pick, verify the generated `security` entries for a public and a protected route in `/docs-json`.

## Keeping docs and guards in sync

The most common documentation bug in secured APIs is drift: the guard says authenticated, the docs say public (or the reverse). One decorator can do both:

```ts
export function Auth(...roles: Role[]) {
  return applyDecorators(
    UseGuards(JwtAuthGuard, RolesGuard),                 // behavior
    Roles(roles),
    ApiBearerAuth('access-token'),                       // documentation
    ApiUnauthorizedResponse({ type: ErrorResponseDto, description: 'Missing or invalid token' }),
    ApiForbiddenResponse({ type: ErrorResponseDto, description: 'Insufficient permissions' }),
  );
}

@Auth(Role.Admin)
@Delete(':id')
remove(@Param('id') id: string) {}
```

With a **global** guard and `@Public()`, make `@Public()` also document itself: have it call `SetMetadata` and apply the documentation override (no security requirement) so public routes are marked public in the spec too. See [custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md).

Also describe **who can call it** in the operation description (roles/permissions, rate limits): OpenAPI's security model only expresses scopes, not your full policy ([RBAC](../07-authorization/02-rbac.md), [ABAC](../07-authorization/03-abac-and-policies.md)).

## Documenting the auth endpoints themselves

Login, refresh, and logout are ordinary endpoints; document them as such, including response DTOs that tell clients how to use the tokens:

```ts
export class TokenResponseDto {
  @ApiProperty({ description: 'JWT, valid for 15 minutes', example: 'eyJhbGciOi...' })
  accessToken: string;

  @ApiProperty({ description: 'Opaque token used only with POST /auth/refresh' })
  refreshToken: string;

  @ApiProperty({ example: 900, description: 'Access token lifetime in seconds' })
  expiresIn: number;
}

@Public()
@ApiOperation({ summary: 'Log in with email and password' })
@ApiOkResponse({ type: TokenResponseDto })
@ApiUnauthorizedResponse({ type: ErrorResponseDto, description: 'Invalid credentials' })
@ApiTooManyRequestsResponse({ type: ErrorResponseDto, description: 'Too many attempts' })
@Post('login')
login(@Body() dto: LoginDto) {}
```

- If tokens travel in **cookies** (`Set-Cookie`), document that in the description and as a response header; the schema can't express "sets a cookie" cleanly, so say it in prose ([responses](./05-responses-and-examples.md)).
- Document the refresh flow ("when you get 401, call `/auth/refresh` with the refresh token") and logout semantics ([refresh tokens](../06-authentication/05-refresh-tokens-and-logout.md)).
- Document **2FA** challenges (`mfaRequired` responses and the verify endpoint) as distinct response shapes ([two-factor authentication](../06-authentication/10-two-factor-authentication.md)).

## Swagger UI and tokens

```ts
SwaggerModule.setup('docs', app, document, {
  swaggerOptions: { persistAuthorization: true },     // keeps the token across page reloads
});
```

`persistAuthorization` stores credentials in the browser's local storage so developers don't re-paste tokens after each refresh. That's convenient for development and **risky on shared machines or in production docs**; use it only for non-production docs, and never paste production tokens into a public docs site.

A typical developer workflow: call `POST /auth/login` in the UI, copy `accessToken`, click **Authorize**, paste it (no `Bearer ` prefix, since the UI adds it). Some teams add a convenience (a dev-only endpoint, or a Swagger UI request interceptor/response handler) that auto-fills the token after login; keep such helpers out of production builds.

## Securing the docs themselves

Documented auth doesn't protect the docs page. If the API is private, restrict or disable `/docs*` in production ([setup](./01-swagger-setup.md)). Don't include real tokens, keys, or credentials in examples or descriptions.

## SDK generation implications

Generators read `components.securitySchemes` and per-operation `security` to configure authentication in generated clients (set a bearer token, send an API key header). Incorrect or missing security metadata produces SDKs that don't send credentials, or that send them to public endpoints needlessly ([client SDK generation](./06-client-sdk-generation.md)). Keep scheme names stable (renaming `access-token` renames config options in generated code).

## Testing

- Fetch `/docs-json` in a test and assert:
  - `components.securitySchemes` contains your schemes,
  - protected operations have the right `security` entries,
  - public operations (login, health) have **none**.
- A **drift test**: iterate over routes, and for each documented-as-protected route call it without a token and expect `401`; for documented-as-public routes expect success (or validation errors), never `401` ([E2E testing](../01-testing/06-e2e-testing.md)).
- Lint the spec (Spectral) for missing `401`/`403` responses on secured operations.

```ts
const doc = SwaggerModule.createDocument(app, config);
expect(doc.components?.securitySchemes).toHaveProperty('access-token');
expect(doc.paths['/auth/login'].post?.security).toBeUndefined();         // or [] if you override globally
expect(doc.paths['/orders'].get?.security).toEqual([{ 'access-token': [] }]);
```

## Common mistakes

- **Scheme name mismatch** between `addBearerAuth(..., name)` and `@ApiBearerAuth(name)`.
- **Documenting auth that the guard doesn't enforce** (or the reverse): drift.
- **Public routes appearing locked** (or protected ones appearing open) under a global requirement.
- **Forgetting `401`/`403` response documentation** on secured endpoints.
- **Typing `Bearer <token>` into the Authorize box**, ending up with `Bearer Bearer ...`.
- **`persistAuthorization` in public/production docs.**
- **Cookie auth "Try it out" confusion** (cross-origin or `HttpOnly` cookies not sent by the UI).
- **OAuth2 scopes in the docs that don't match** the real permission model.
- **Exposing docs publicly for a private API**, and examples containing real credentials.
- **Renaming scheme names casually**, breaking generated clients.

## Debugging

- Authorize button missing: no security scheme defined (`addBearerAuth` etc. not called), or an invalid scheme definition.
- Authorize works but requests have no `Authorization` header: the operation has no `security` requirement for that scheme (missing `@ApiBearerAuth`), or its name doesn't match.
- Public endpoint still sends credentials / shows a lock: global requirement not overridden for that operation.
- `401` in "Try it out" despite a valid token: token expired, wrong environment (server URL), or the UI is hitting a different origin than expected.
- Inspect `/docs-json` → `components.securitySchemes` and the operation's `security` array to see exactly what Swagger believes.

## Quick Summary

- Two parts: **define schemes** (`addBearerAuth`/`addApiKey`/`addCookieAuth`/`addOAuth2`, with explicit names) and **apply requirements** (`@ApiBearerAuth(name)`, `@ApiSecurity`, `@ApiOAuth2(scopes)`).
- Documentation never enforces auth: keep docs and guards together with a composed `@Auth()` decorator (and a `@Public()` that documents itself if using a global guard).
- Document `401`/`403`, token lifecycle (login, refresh, logout, cookies, 2FA), and permission details in descriptions.
- `persistAuthorization` is dev-only convenience; secure or disable docs in production; keep real credentials out of examples.
- Test the generated `securitySchemes`/`security` entries and add a drift test against real guard behavior; stable scheme names matter for SDKs.

## Next

[Responses and examples →](./05-responses-and-examples.md)
