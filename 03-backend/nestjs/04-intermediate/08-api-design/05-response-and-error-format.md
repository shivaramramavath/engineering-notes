# Response and Error Format

Clients write code against your response shapes, especially your **errors**. A consistent format means one error handler on the client instead of twenty, predictable logging and alerting on your side, and an API that feels designed rather than accumulated. The choices that matter: how to shape success responses, how to shape errors (with stable codes), and how to enforce it centrally in Nest.

Prerequisites: [REST and resource design](./01-rest-and-resource-design.md), [HTTP exceptions](../../03-core-concepts/01-request-pipeline/07-http-exceptions.md), [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md).

## Success responses

Two common styles:

```json
// A) Resource directly (use HTTP status for success/failure)
{ "id": "42", "status": "paid", "total": 1999 }

// list: an object, not a bare array, so metadata can be added later
{ "items": [ ... ], "meta": { "page": 1, "limit": 20, "total": 412 } }
```

```json
// B) Envelope on everything
{ "data": { "id": "42", "status": "paid" }, "meta": { "requestId": "..." } }
```

| | Direct resource | Envelope (`{ data, meta }`) |
|-|-----------------|-----------------------------|
| Simplicity | Simpler, closer to HTTP | Extra nesting everywhere |
| Metadata | Awkward for single resources | Natural place for pagination, request id, warnings |
| Consistency | Lists differ from single items | Uniform shape |
| Tooling/OpenAPI | Straightforward | Every response type needs a wrapper schema |

Neither is wrong. **Pick one and apply it consistently.** HTTP already conveys success vs failure through status codes; an envelope's `success: true` field adds nothing. If you use an envelope, implement it centrally with an [interceptor](../../03-core-concepts/01-request-pipeline/06-interceptor-recipes.md) that has an opt-out (health checks, file downloads, webhooks), and document the wrapper in Swagger (it won't be inferred automatically: [responses and examples](../09-openapi-and-swagger/05-responses-and-examples.md)). Always return **objects at the top level** so you can add fields without breaking clients.

## Error responses

Errors need three things: a **correct status code** ([status codes](./01-rest-and-resource-design.md)), a **machine-readable code** clients can branch on, and a **human-readable message** (for developers, not necessarily end users).

### What Nest returns by default

```json
{ "message": "User 7 not found", "error": "Not Found", "statusCode": 404 }

{ "message": ["email must be an email", "password is too short"], "error": "Bad Request", "statusCode": 400 }
```

Two inconsistencies to handle: `message` is a **string for some errors and an array for validation errors**, and there's **no stable error code**. Clients end up parsing message text, which breaks when you reword it. Fix both with a custom format.

### A standard format: RFC 9457 Problem Details

RFC 9457 (which obsoletes RFC 7807) defines a standard JSON error shape with media type `application/problem+json`:

```json
{
  "type": "https://api.example.com/problems/insufficient-funds",
  "title": "Insufficient funds",
  "status": 422,
  "detail": "Account 123 has 10.00 available but 25.00 is required.",
  "instance": "/payments/987",
  "code": "INSUFFICIENT_FUNDS",
  "requestId": "b7c1...",
  "errors": [{ "field": "amount", "message": "must be at most 10.00" }]
}
```

| Member | Meaning |
|--------|---------|
| `type` | URI identifying the problem **type** (can be a stable URL or `about:blank`) |
| `title` | Short, stable summary of the type |
| `status` | HTTP status (duplicates the response status) |
| `detail` | Explanation of **this occurrence** |
| `instance` | Identifies this specific occurrence (request path, id) |
| *extensions* | Your own fields: `code`, `errors`, `requestId`, ... |

You don't have to adopt the RFC wholesale, but using its member names (and the content type) gives clients familiar structure and existing libraries. At minimum, define **your own** consistent shape with: `status`, a stable `code`, a `message`, optional `details`/`errors`, and a `requestId`.

### Stable error codes

```ts
export enum ErrorCode {
  ValidationFailed = 'VALIDATION_FAILED',
  NotFound = 'NOT_FOUND',
  EmailTaken = 'EMAIL_TAKEN',
  InsufficientFunds = 'INSUFFICIENT_FUNDS',
  RateLimited = 'RATE_LIMITED',
  Internal = 'INTERNAL_ERROR',
}
```

Clients branch on `code` (`EMAIL_TAKEN` → show "email already registered"), never on `message`. Treat codes as **part of the contract**: don't rename or reuse them. Messages can be improved freely.

## Enforcing the format centrally in Nest

One [global exception filter](../../03-core-concepts/01-request-pipeline/08-exception-filters.md) converts every error into your shape, so individual handlers just throw.

```ts
// common/api-exception.ts: carry a code with an HttpException
export class ApiException extends HttpException {
  constructor(status: HttpStatus, public readonly code: ErrorCode, message: string, public readonly errors?: unknown[]) {
    super({ code, message, errors }, status);
  }
}

throw new ApiException(HttpStatus.CONFLICT, ErrorCode.EmailTaken, 'Email already registered');
```

```ts
// common/all-exceptions.filter.ts
@Catch()
export class AllExceptionsFilter implements ExceptionFilter {
  private readonly logger = new Logger(AllExceptionsFilter.name);
  constructor(private readonly adapterHost: HttpAdapterHost) {}

  catch(exception: unknown, host: ArgumentsHost) {
    const { httpAdapter } = this.adapterHost;
    const ctx = host.switchToHttp();
    const req = ctx.getRequest();

    const { status, body } = this.toProblem(exception, req);

    if (status >= 500) this.logger.error(exception instanceof Error ? exception.stack : String(exception));

    httpAdapter.reply(ctx.getResponse(), body, status);
  }

  private toProblem(exception: unknown, req: any) {
    const requestId = req.id ?? req.headers['x-request-id'];

    if (exception instanceof HttpException) {
      const status = exception.getStatus();
      const res = exception.getResponse() as any;

      // validation errors: message is an array
      if (status === 400 && Array.isArray(res?.message)) {
        return { status, body: { status, code: ErrorCode.ValidationFailed, message: 'Validation failed', errors: res.message, requestId } };
      }
      return {
        status,
        body: {
          status,
          code: res?.code ?? defaultCodeFor(status),
          message: typeof res === 'string' ? res : res?.message ?? exception.message,
          errors: res?.errors,
          requestId,
        },
      };
    }

    // unknown error: never leak internals
    return { status: 500, body: { status: 500, code: ErrorCode.Internal, message: 'Internal server error', requestId } };
  }
}
```

Register it with `APP_FILTER` ([why](../../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)). Map **domain/ORM errors** to the right status and code in repositories or additional filters so `500` is only for genuine surprises ([database errors](../02-database-foundations/07-database-errors.md)).

### Better validation errors

Instead of a flat array of sentences, return structured, per-field errors so UIs can attach messages to inputs:

```ts
new ValidationPipe({
  exceptionFactory: (errors) =>
    new ApiException(HttpStatus.UNPROCESSABLE_ENTITY, ErrorCode.ValidationFailed, 'Validation failed',
      errors.flatMap((e) => Object.values(e.constraints ?? {}).map((message) => ({ field: e.property, message })))),
});
```

(Flatten nested `children` recursively for nested DTOs, and decide between `400` and `422` for validation failures; consistency matters more than the choice: [nested validation](../../03-core-concepts/02-validation-and-serialization/05-nested-and-conditional-validation.md).) Ensure `validationError: { value: false }` if submitted values (passwords) shouldn't be echoed back.

## Security: don't leak

- **No stack traces, SQL, file paths, or internal class names** in responses. Log them; return a generic message and a `requestId`.
- **Authentication errors** should be uniform ("Invalid credentials"), not revealing whether the account exists ([auth architecture](../06-authentication/01-authentication-architecture.md)).
- **404 vs 403:** use `404` to hide existence of other users' resources ([authorization](../07-authorization/01-authorization-fundamentals.md)).
- Don't include user-submitted values in error messages without care (injection into logs and downstream consumers).

## Request IDs and correlation

Include a `requestId` in every error (and often success `meta`/headers) so support can find the matching logs:

```ts
// middleware: reuse an incoming id or generate one, and echo it back
req.id = req.headers['x-request-id'] ?? randomUUID();
res.setHeader('x-request-id', req.id);
```

See [request logging and correlation IDs](../../07-production/03-observability/03-request-logging-and-correlation-id.md).

## Headers worth setting

| Header | Use |
|--------|-----|
| `Content-Type: application/json` (or `application/problem+json` for RFC 9457 errors) | Always accurate |
| `Location` | After `201`, points to the new resource |
| `Retry-After` | With `429`/`503`, tells clients when to retry |
| `ETag`, `Cache-Control` | Caching/conditional requests |
| `X-Request-Id` | Correlation |
| `RateLimit-*` headers | Rate-limit visibility (standardization is evolving; check current drafts) |

## Conventions that prevent client pain

- **Dates:** ISO 8601 UTC strings; **money:** integer minor units + currency; **ids:** strings ([resource design](./01-rest-and-resource-design.md)).
- **Null vs omitted:** document and be consistent.
- **Empty collections** are `[]`, not `null`.
- **Casing:** one convention across all payloads (including error members).
- **Don't change error `code`s or shapes** without versioning ([versioning](./02-api-versioning.md)); error shape changes are breaking changes.
- **Localization:** if you localize `message`, keep `code` language-independent; prefer clients localizing from codes.

## Documenting errors

Describe each endpoint's possible error responses and the shared error schema in OpenAPI (`@ApiResponse`, shared `ErrorResponseDto`) so generated clients get typed errors ([documenting endpoints](../09-openapi-and-swagger/02-documenting-endpoints.md), [responses](../09-openapi-and-swagger/05-responses-and-examples.md)). Publish the list of error codes.

## Testing

- Assert the **entire error shape** for each class of error: validation (`400/422`), not found (`404`), conflict (`409`), unauthorized/forbidden, rate limited, and an unexpected exception (`500` with no leaked details).
- Check that `message` for validation errors always has the same type (array/structured).
- Check `requestId` presence and that it matches the `x-request-id` header.
- Contract tests so error shapes can't change accidentally ([E2E testing](../01-testing/06-e2e-testing.md), [testing pipeline components](../01-testing/04-testing-pipeline-components.md)).

```ts
await request(app.getHttpServer()).post('/users').send({ email: 'nope' }).expect(422).expect((res) => {
  expect(res.body).toMatchObject({ status: 422, code: 'VALIDATION_FAILED' });
  expect(res.body.errors[0]).toHaveProperty('field', 'email');
});
```

## Common mistakes

- **Clients parsing `message` text** because there's no stable `code`.
- **Inconsistent shapes** (string vs array `message`, different shapes across modules).
- **`200` with an error body**, or wrong status codes.
- **Leaking stack traces, SQL, or internal messages** in `500`s.
- **Envelope applied to everything**, breaking health checks, downloads, and webhooks.
- **Undocumented envelope/error types in Swagger.**
- **Reusing or renaming error codes** across releases.
- **Echoing sensitive input** in validation errors.
- **No request id**, making support cases unsolvable.
- **Different error handling per controller** instead of one global filter.

## Debugging

- Unexpected default Nest error shape: your global filter isn't registered (`APP_FILTER`), or the error originated before the exception layer (for example, in Express-level middleware you added).
- A domain error returns `500`: no mapping exists; add a repository translation or a specific filter.
- Validation errors in the old shape: the `ValidationPipe` `exceptionFactory` isn't configured in the pipe actually used (global vs route-level, or tests that skip `main.ts` setup: [mirror main.ts](../01-testing/06-e2e-testing.md)).
- Missing `requestId`: the middleware runs after the failing layer, or the field isn't set on the request.
- Use `curl -i` to compare status, headers, and body exactly as a client sees them.

## Quick Summary

- Choose one success shape (direct resource or `{ data, meta }`) and always return objects at the top level.
- Errors: correct status + **stable machine-readable `code`** + human `message` + optional structured `errors` + `requestId`; RFC 9457 `application/problem+json` is a standard to borrow from.
- Enforce it with one global exception filter (`APP_FILTER`) and a custom `exceptionFactory` for validation; map domain/DB errors to proper statuses.
- Never leak internals; log details server-side; use `404` to hide existence.
- Treat error shapes and codes as part of the contract; document them in OpenAPI; test the full error bodies.

## Next

[Idempotency →](./06-idempotency.md)
