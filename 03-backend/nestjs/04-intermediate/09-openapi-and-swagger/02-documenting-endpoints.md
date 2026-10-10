# Documenting Endpoints

Once Swagger is set up ([setup](./01-swagger-setup.md)), each controller and handler needs enough metadata that someone can use the endpoint without reading your code: what it does, what it accepts, what it returns. `@nestjs/swagger` provides decorators for each part. The skill is documenting **what clients need** without drowning controllers in decorators.

Prerequisites: [Swagger setup](./01-swagger-setup.md), [REST and resource design](../08-api-design/01-rest-and-resource-design.md).

## Grouping: tags

```ts
@ApiTags('orders')
@Controller('orders')
export class OrdersController {}
```

Tags group endpoints in Swagger UI and become sections in generated docs and SDK client classes (many generators create one API class per tag). Use **one tag per resource/area**, consistently named. Add descriptions and ordering via `DocumentBuilder().addTag('orders', 'Order management')`.

## Describing an operation

```ts
@ApiOperation({
  summary: 'Create an order',                              // one line, shown in the list
  description: 'Creates an order for the authenticated user. Requires an `Idempotency-Key` header.',   // longer Markdown
  operationId: 'createOrder',                              // optional; default is Controller_method
})
@Post()
create(@Body() dto: CreateOrderDto) {}
```

- **`summary`** should be a short imperative phrase. Generated SDK docs and UI lists show it.
- **`description`** is where behavior that isn't obvious belongs: side effects, idempotency, rate limits, required permissions, edge cases, async behavior (`202`).
- **`operationId`** becomes the SDK method name; unique per document. Setting a global `operationIdFactory` is usually better than hand-setting each one ([SDK generation](./06-client-sdk-generation.md)).
- Mark retired endpoints: `@ApiOperation({ deprecated: true })`, together with real [deprecation headers](../08-api-design/02-api-versioning.md).

## Path, query, and header parameters

The plugin and Nest's own decorators already tell Swagger about `@Param`, `@Query` (when they use DTO classes), and `@Body` types. Add decorators to improve descriptions or document what can't be inferred.

```ts
@ApiParam({ name: 'id', description: 'Order id', example: 'ord_01HZX' })
@Get(':id')
findOne(@Param('id', ParseUUIDPipe) id: string) {}

@ApiQuery({ name: 'status', required: false, enum: OrderStatus, description: 'Filter by status' })
@ApiQuery({ name: 'limit', required: false, type: Number, example: 20 })
@Get()
list(@Query('status') status?: OrderStatus, @Query('limit') limit?: number) {}

@ApiHeader({ name: 'Idempotency-Key', required: true, description: 'Unique key per logical operation' })
@Post()
create() {}
```

**Prefer a query DTO class** over many loose `@Query('x')` parameters: Swagger expands DTO properties into individual query parameters, you get validation and documentation from one definition, and the plugin infers most details.

```ts
export class ListOrdersQueryDto {
  @ApiPropertyOptional({ enum: OrderStatus, description: 'Filter by status' })
  @IsOptional() @IsEnum(OrderStatus)
  status?: OrderStatus;

  @ApiPropertyOptional({ minimum: 1, maximum: 100, default: 20 })
  @IsOptional() @Type(() => Number) @IsInt() @Min(1) @Max(100)
  limit?: number = 20;
}

@Get()
list(@Query() query: ListOrdersQueryDto) {}          // documented as separate query parameters
```

For array query parameters and styles (`?status=a,b` vs `?status=a&status=b`), specify `isArray`/`style`/`explode` options so the docs match how your server actually parses them ([filtering](../08-api-design/04-filtering-sorting-and-search.md)).

## Request bodies

`@Body() dto: CreateOrderDto` is enough when `CreateOrderDto` is a class (interfaces and type aliases have no runtime metadata, so they produce **empty** schemas: use classes, as always for [DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md)). Use `@ApiBody` to add examples or to document bodies that aren't a single DTO:

```ts
@ApiBody({
  type: CreateOrderDto,
  examples: {
    simple: { summary: 'One item', value: { items: [{ productId: 'p_1', quantity: 1 }] } },
    bulk:   { summary: 'Many items', value: { items: [{ productId: 'p_1', quantity: 5 }, { productId: 'p_2', quantity: 1 }] } },
  },
})
@Post()
create(@Body() dto: CreateOrderDto) {}

// array body
@ApiBody({ type: [CreateOrderDto] })
@Post('bulk')
createMany(@Body(new ParseArrayPipe({ items: CreateOrderDto })) dtos: CreateOrderDto[]) {}
```

Field-level docs live on the DTO ([DTO documentation](./03-dto-documentation.md)).

## Responses

```ts
@ApiCreatedResponse({ type: OrderDto, description: 'The order was created' })
@ApiBadRequestResponse({ type: ErrorResponseDto, description: 'Validation failed' })
@ApiConflictResponse({ type: ErrorResponseDto, description: 'Duplicate Idempotency-Key payload' })
@Post()
create(@Body() dto: CreateOrderDto): Promise<OrderDto> {}
```

Shortcut decorators exist per status: `@ApiOkResponse`, `@ApiCreatedResponse`, `@ApiAcceptedResponse`, `@ApiNoContentResponse`, `@ApiBadRequestResponse`, `@ApiUnauthorizedResponse`, `@ApiForbiddenResponse`, `@ApiNotFoundResponse`, `@ApiConflictResponse`, `@ApiUnprocessableEntityResponse`, `@ApiTooManyRequestsResponse`, `@ApiInternalServerErrorResponse`, and the generic `@ApiResponse({ status, ... })`. Lists use `type: OrderDto, isArray: true`. Details, error schemas, envelopes, and examples are in [Responses and examples](./05-responses-and-examples.md).

Document the **success status your handler actually returns**: if you set `@HttpCode(200)` or return `204`, the documented status must match, or generated clients mis-handle it.

## Content types and files

```ts
@ApiConsumes('multipart/form-data')
@ApiBody({
  schema: {
    type: 'object',
    properties: {
      file: { type: 'string', format: 'binary' },
      title: { type: 'string' },
    },
    required: ['file'],
  },
})
@UseInterceptors(FileInterceptor('file'))
@Post('upload')
upload(@UploadedFile() file: Express.Multer.File, @Body('title') title: string) {}
```

- `@ApiConsumes` / `@ApiProduces` declare request/response media types (`application/json` is the default).
- File uploads need an explicit `format: 'binary'` schema; the plugin can't infer multipart bodies ([file upload](../../05-advanced/07-integrations/01-file-upload.md)).
- Downloads: document the response as `{ type: 'string', format: 'binary' }` with the right media type ([responses](./05-responses-and-examples.md)).

## Security per endpoint

Apply authentication requirements with `@ApiBearerAuth()` (or your composed decorator) on controllers or routes, and describe `401`/`403` responses. Full treatment, including global requirements and public routes, is in [Documenting authentication](./04-documenting-auth.md).

## Don't repeat yourself: composed decorators

Most protected endpoints share the same set of annotations. Bundle them with `applyDecorators` ([custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md)):

```ts
export function ApiCrudErrors() {
  return applyDecorators(
    ApiBadRequestResponse({ type: ErrorResponseDto }),
    ApiUnauthorizedResponse({ type: ErrorResponseDto }),
    ApiForbiddenResponse({ type: ErrorResponseDto }),
    ApiTooManyRequestsResponse({ type: ErrorResponseDto }),
  );
}

export function Auth(...roles: Role[]) {
  return applyDecorators(
    Roles(roles),
    UseGuards(JwtAuthGuard, RolesGuard),
    ApiBearerAuth('access-token'),
    ApiUnauthorizedResponse({ type: ErrorResponseDto }),
    ApiForbiddenResponse({ type: ErrorResponseDto }),
  );
}

@Auth(Role.Admin)
@ApiCrudErrors()
@Delete(':id')
remove(@Param('id') id: string) {}
```

This keeps behavior (guards) and documentation (security, errors) in **one place**, so they can't drift apart, which is a very common source of docs saying an endpoint is public when it's protected, or the reverse.

## Versions, prefixes, and exclusion

- Routes appear with their **full path** (global prefix and URI version included). Use one document per version ([setup](./01-swagger-setup.md)).
- Hide internal routes with `@ApiExcludeEndpoint()` / `@ApiExcludeController()`. Remember this only affects documentation, not access.
- Version-neutral routes (`VERSION_NEUTRAL`) appear in every version's document if you include their module.

## What good documentation includes

For each endpoint, a client should be able to answer:

| Question | Where documented |
|----------|------------------|
| What does it do, and what are side effects? | `summary` + `description` |
| What must I send? (params, query, headers, body) | DTO property docs, `@ApiQuery`/`@ApiHeader` |
| What can I get back? (success shape, status) | `@Api*Response` with `type` |
| What can go wrong? (error statuses and codes) | Error responses + shared error schema |
| Who can call it? (auth, permissions) | Security decorators + description |
| Is it safe to retry? | Description (idempotency key / natural idempotency) |
| Limits? (rate limits, max page size, file size) | Description and schema constraints |

## Keeping docs honest

- **Generate from code** (types, validators) so the docs can't drift, and add prose only for what code can't say.
- **Review the generated spec in code review** or diff it in CI to catch accidental contract changes ([SDK generation and CI checks](./06-client-sdk-generation.md)).
- Add **E2E/contract tests** that fail if a documented status or shape diverges from reality ([E2E testing](../01-testing/06-e2e-testing.md)).
- Make documentation decorators part of the **definition of done** for new endpoints; consider an ESLint rule or a test that scans routes for missing summaries/responses.

## Common mistakes

- **Interfaces/types as DTOs**, producing empty schemas.
- **No response types**: handlers return untyped values and the docs show `{}`.
- **Documented status doesn't match reality** (`@HttpCode(200)` with `@ApiCreatedResponse`).
- **Missing error responses** (clients only learn about `401/403/409/422` by trial).
- **`@ApiQuery` copies of a DTO** that drift from the real validation.
- **Auth documented inconsistently** (guards in code, `@ApiBearerAuth` forgotten or vice versa).
- **Huge pre-handler decorator stacks** copied everywhere instead of composed decorators.
- **File upload endpoints without `@ApiConsumes('multipart/form-data')`** and a binary schema.
- **Duplicate operation ids** or reliance on ugly defaults.
- **Documenting internal endpoints in the public spec.**

## Debugging

- Endpoint shows no body/parameters: the DTO isn't a class, the plugin didn't process the file, or the param type is a plain object/primitive without `@ApiBody`/`@ApiQuery`.
- Response shows `{}` or the wrong type: add `type` to the response decorator, or enable the plugin so return types are inferred; `Promise<Dto>` works but unions and generics need explicit decorators.
- "Try it out" sends the wrong thing for arrays or files: specify `isArray`/`style`/`explode`, `@ApiConsumes`, and a binary schema.
- Endpoint missing: module excluded by `include`, route excluded by decorator, or created before the route was registered.
- Look at `/docs-json` directly: it's the source of truth for what generators and tools will see.

## Quick Summary

- Group with `@ApiTags`; describe with `@ApiOperation` (summary, description, `operationId`, `deprecated`).
- Prefer DTO classes for query/body so docs, validation, and types come from one definition; use `@ApiParam`/`@ApiQuery`/`@ApiHeader`/`@ApiBody` for what can't be inferred, plus examples.
- Document every response status the handler can return, including errors, matching `@HttpCode` reality.
- Files need `@ApiConsumes('multipart/form-data')` and a binary schema; downloads need a binary response schema.
- Compose repeated decorators (auth, errors) with `applyDecorators`; generate from code, diff the spec, and test the contract.

## Next

[DTO documentation →](./03-dto-documentation.md)
