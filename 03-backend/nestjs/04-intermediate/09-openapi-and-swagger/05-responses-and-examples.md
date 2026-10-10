# Responses and Examples

A response definition tells a client **what shape to expect for each status code**. Good response documentation covers the success shape, every error shape, headers, and realistic examples. It's what makes generated clients type-safe and error handling predictable. Because Nest's runtime behavior (interceptors, filters, serialization) transforms what handlers return, **the code's return type and the documented response often differ**, so you have to document them deliberately.

Prerequisites: [Documenting endpoints](./02-documenting-endpoints.md), [DTO documentation](./03-dto-documentation.md), [response and error format](../08-api-design/05-response-and-error-format.md).

## Response decorators

```ts
@ApiOkResponse({ type: OrderDto, description: 'The order' })
@ApiNotFoundResponse({ type: ErrorResponseDto, description: 'Order not found' })
@Get(':id')
findOne(@Param('id') id: string): Promise<OrderDto> {}
```

| Decorator | Status |
|-----------|--------|
| `@ApiOkResponse` | 200 |
| `@ApiCreatedResponse` | 201 |
| `@ApiAcceptedResponse` | 202 |
| `@ApiNoContentResponse` | 204 |
| `@ApiBadRequestResponse` | 400 |
| `@ApiUnauthorizedResponse` | 401 |
| `@ApiForbiddenResponse` | 403 |
| `@ApiNotFoundResponse` | 404 |
| `@ApiConflictResponse` | 409 |
| `@ApiUnprocessableEntityResponse` | 422 |
| `@ApiTooManyRequestsResponse` | 429 |
| `@ApiInternalServerErrorResponse` | 500 |
| `@ApiDefaultResponse` | Anything not listed ("default") |
| `@ApiResponse({ status: N, ... })` | Any status |

Rules of thumb:

- Document the status your handler **actually returns**. `POST` defaults to `201` in Nest; if you use `@HttpCode(200)`, document `@ApiOkResponse` ([REST design](../08-api-design/01-rest-and-resource-design.md)).
- A **`type`** is what produces a schema. Without it (and without the CLI plugin inferring it from a `Promise<Dto>` return type), the response is documented as an empty object.
- Lists: `type: OrderDto, isArray: true`.
- `@ApiNoContentResponse()` for `204` has no body schema.
- Don't forget **error statuses**; clients can only handle what they know exists.

## A shared error schema

Define your error shape once, as a DTO that matches your [global exception filter](../08-api-design/05-response-and-error-format.md), and reuse it:

```ts
export class ErrorResponseDto {
  @ApiProperty({ example: 404 }) status: number;
  @ApiProperty({ example: 'NOT_FOUND', description: 'Stable machine-readable code' }) code: string;
  @ApiProperty({ example: 'Order ord_42 not found' }) message: string;
  @ApiPropertyOptional({ type: () => [FieldErrorDto] }) errors?: FieldErrorDto[];
  @ApiPropertyOptional({ example: 'b7c1e0d2-...' }) requestId?: string;
}

export class FieldErrorDto {
  @ApiProperty({ example: 'email' }) field: string;
  @ApiProperty({ example: 'must be an email' }) message: string;
}
```

This DTO is **documentation only**: nothing validates your real error bodies against it. The filter produces the actual shape, so keep them in sync, and test that real errors match ([testing](#testing)). Wrap the repeated error decorators in a composed decorator so every endpoint documents errors the same way:

```ts
export const ApiStandardErrors = () =>
  applyDecorators(
    ApiBadRequestResponse({ type: ErrorResponseDto, description: 'Validation failed' }),
    ApiUnauthorizedResponse({ type: ErrorResponseDto }),
    ApiForbiddenResponse({ type: ErrorResponseDto }),
    ApiTooManyRequestsResponse({ type: ErrorResponseDto, description: 'Rate limit exceeded' }),
    ApiInternalServerErrorResponse({ type: ErrorResponseDto }),
  );
```

Use endpoint-specific decorators (`@ApiNotFoundResponse`, `@ApiConflictResponse`) in addition for errors that particular operation can produce.

## Examples

Examples make docs understandable, power "Try it out" and mock servers, and appear in generated SDK documentation. There are several levels.

### On properties

```ts
@ApiProperty({ example: 'ord_01HZX', description: 'Order id' })
id: string;
```

Property examples compose into a full example body for the schema automatically.

### On request bodies (named examples)

```ts
@ApiBody({
  type: CreateOrderDto,
  examples: {
    single: { summary: 'One item', value: { items: [{ productId: 'p_1', quantity: 1 }] } },
    discounted: { summary: 'With a coupon', value: { items: [{ productId: 'p_1', quantity: 2 }], coupon: 'SPRING' } },
  },
})
@Post()
create(@Body() dto: CreateOrderDto) {}
```

### On responses (including per-status examples)

```ts
@ApiResponse({
  status: 409,
  description: 'Email already registered',
  content: {
    'application/json': {
      schema: { $ref: getSchemaPath(ErrorResponseDto) },
      examples: {
        emailTaken: { summary: 'Duplicate email', value: { status: 409, code: 'EMAIL_TAKEN', message: 'Email already registered' } },
      },
    },
  },
})
```

Error examples are especially valuable, because clients rarely see real error bodies until production. Examples should be **realistic, valid against the schema, free of real data or secrets**, and kept current. Stale examples are worse than none.

## Response headers

```ts
@ApiCreatedResponse({
  type: OrderDto,
  headers: {
    Location: { description: 'URL of the new order', schema: { type: 'string', example: '/orders/ord_42' } },
  },
})
```

Document headers clients depend on: `Location` (after `201`), `Retry-After` (with `429`/`503`), `ETag`, rate-limit headers, `Idempotent-Replayed`, and `Set-Cookie` for cookie-based login (the schema can't express cookie semantics, so also state them in the description: [documenting auth](./04-documenting-auth.md)). Document **request** headers with `@ApiHeader` ([endpoints](./02-documenting-endpoints.md)).

## Envelopes and interceptors: the invisible transformation

An [interceptor](../../03-core-concepts/01-request-pipeline/06-interceptor-recipes.md) that wraps every response (`{ data: ... }`) is invisible to Swagger. If you document `type: OrderDto` while the wire shape is `{ data: OrderDto }`, **the docs are wrong**, and every generated SDK will mis-parse responses.

Fix with wrapper schemas (using the same generic technique as for pagination, from [DTO documentation](./03-dto-documentation.md)):

```ts
export class ApiEnvelope<T> { data: T }

export const ApiEnvelopedResponse = <T extends Type<unknown>>(model: T, opts: { isArray?: boolean } = {}) =>
  applyDecorators(
    ApiExtraModels(ApiEnvelope, model),
    ApiOkResponse({
      schema: {
        allOf: [
          { $ref: getSchemaPath(ApiEnvelope) },
          { properties: { data: opts.isArray
              ? { type: 'array', items: { $ref: getSchemaPath(model) } }
              : { $ref: getSchemaPath(model) } } },
        ],
      },
    }),
  );
```

The same applies to **serialization** (fields hidden by `@Exclude`, `ClassSerializerInterceptor`) and **exception filters** (the shape of errors): the documented schema must describe the final serialized output, not the class you return ([serialization](../../03-core-concepts/02-validation-and-serialization/07-serialization.md)). The cleanest approach is returning explicit, annotated **response DTOs** so the code, the runtime shape, and the docs are the same object.

## Pagination responses

Document your pagination envelope once (generic decorator from [DTO documentation](./03-dto-documentation.md)) and use it on every list endpoint, along with the pagination query parameters (`page`/`limit` or `cursor`/`limit`) via a shared query DTO ([pagination](../08-api-design/03-pagination.md)). State max page size and ordering guarantees in the description.

## Files, streams, and non-JSON responses

```ts
@ApiProduces('application/pdf')
@ApiOkResponse({
  description: 'The invoice as a PDF',
  content: { 'application/pdf': { schema: { type: 'string', format: 'binary' } } },
})
@Get(':id/pdf')
download(@Param('id') id: string, @Res() res: Response) {}
```

For server-sent events or streaming, document the media type (`text/event-stream`) and describe the event format in prose; OpenAPI can't model streams precisely ([SSE and streaming](../../05-advanced/04-realtime/06-server-sent-events-and-streaming.md)). WebSocket APIs aren't covered by OpenAPI at all (AsyncAPI is the analogous standard).

## Async operations and redirects

- `202 Accepted`: document the response body (a job/status resource) and the `Location` header pointing to the status endpoint, plus the polling or webhook behavior in the description ([REST design](../08-api-design/01-rest-and-resource-design.md), [webhooks](../08-api-design/07-webhooks.md)).
- Redirects (`@Redirect()`): document `302`/`301` with a `Location` header and no body.

## Polymorphic responses

When a response can be one of several shapes, use `oneOf` and a discriminator:

```ts
@ApiExtraModels(CardPaymentDto, BankPaymentDto)
@ApiOkResponse({
  schema: {
    oneOf: [{ $ref: getSchemaPath(CardPaymentDto) }, { $ref: getSchemaPath(BankPaymentDto) }],
    discriminator: { propertyName: 'type' },
  },
})
```

Generated clients handle `oneOf` with varying quality; keep polymorphism simple, and verify what your target generator produces ([SDK generation](./06-client-sdk-generation.md)).

## Deprecations

Mark operations or fields deprecated in the schema (`@ApiOperation({ deprecated: true })`, `@ApiProperty({ deprecated: true })`) **and** explain the replacement and sunset date in the description, in step with real [deprecation headers](../08-api-design/02-api-versioning.md).

## Testing: keep documentation and behavior aligned

Documentation drifts unless something checks it.

- **Spec validity:** lint the generated document (Spectral or an OpenAPI validator) in CI.
- **Contract tests:** in E2E tests, validate real response bodies and status codes against the documented schemas for key endpoints (JSON-schema validation of the response against the spec's schema), including error responses:

```ts
const res = await request(app.getHttpServer()).get('/orders/does-not-exist').expect(404);
expect(res.body).toMatchObject({ status: 404, code: 'NOT_FOUND', message: expect.any(String) });
// ...and/or validate res.body against components.schemas.ErrorResponseDto with a JSON-schema validator
```

- **Breaking-change detection:** diff the generated spec against the previous version's spec on each pull request ([SDK generation and CI](./06-client-sdk-generation.md)).
- Test that **every** route documents at least its success response and the standard errors (a script over the generated document's paths).

See [E2E testing](../01-testing/06-e2e-testing.md).

## Common mistakes

- **No `type` on responses**, so docs show `{}` and SDKs return `any`.
- **Documenting the DTO but returning something else** (entities with extra fields, interceptor envelopes, serializer changes).
- **Missing error responses**, or error schemas that don't match the real filter output.
- **Wrong success status** (`201` documented, `200` returned).
- **Stale, invalid, or sensitive examples.**
- **Forgetting response headers** (`Location`, `Retry-After`, `Set-Cookie`).
- **Binary responses documented as JSON**, breaking SDK downloads.
- **Overusing `oneOf`/`anyOf`** that generators handle poorly.
- **Copy-pasting error decorators** per endpoint instead of composing them.
- **No automated check** that docs match behavior.

## Debugging

- SDK method returns `any`/`unknown`: the response has no schema (`type` missing) or a `Promise<T>` the plugin couldn't infer.
- Clients break on responses after adding an interceptor: the envelope wasn't documented.
- Example doesn't appear in Try-it-out: examples must be under the right `content` media type or on the body/property decorators; check the generated JSON.
- Generic wrapper schemas show as empty: the model class isn't registered with `@ApiExtraModels`.
- `$ref` unresolved errors in tooling: referenced classes missing from `@ApiExtraModels`.
- Compare `/docs-json` with a real `curl -i` response (status, headers, body) to find the mismatch.

## Quick Summary

- Document every response status the endpoint can return, with `type` schemas; use shared, composed decorators for standard errors and a shared `ErrorResponseDto` that matches your exception filter.
- Document what the **client actually receives**: interceptor envelopes, serializers, and filters need explicit wrapper schemas; returning annotated response DTOs keeps code, runtime, and docs aligned.
- Add realistic examples (properties, bodies, per-status responses, especially errors) and document headers (`Location`, `Retry-After`, `Set-Cookie`).
- Non-JSON (files, streams) and polymorphic responses need explicit media types or `oneOf`; keep them simple for SDK generators.
- Enforce alignment with spec linting, contract tests against real responses, and breaking-change diffs in CI.

## Next

[Client SDK generation →](./06-client-sdk-generation.md)
