# OpenAPI and Swagger

An **OpenAPI document** is a machine-readable description of your HTTP API: paths, parameters, request and response schemas, authentication, errors. **Swagger UI** renders it as interactive documentation. In Nest, `@nestjs/swagger` generates the document **from your code** (controllers, DTOs, decorators), so the spec stays close to reality and unlocks tooling: docs, client SDKs, mock servers, contract tests, and linting.

```text
 controllers + DTOs + decorators (+ CLI plugin)
              │
              ▼
   SwaggerModule.createDocument()  ──►  OpenAPI JSON/YAML  ──►  Swagger UI (human docs, "Try it out")
                                              │
                                              ├──► generated client SDKs
                                              ├──► mock servers, contract/breaking-change checks
                                              └──► Postman import, API gateways, linting
```

> Applies to NestJS 10/11 with `@nestjs/swagger` (the major version tracks Nest's; check that yours supports your Nest version). Terminology: "Swagger" was the original name; **OpenAPI** is the specification (3.0/3.1), and Swagger UI/Editor are tools. `@nestjs/swagger` generates OpenAPI 3.x documents. Option names and plugin behavior evolve, so confirm details in the current docs.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Swagger setup](./01-swagger-setup.md) | Install, `DocumentBuilder`, serving UI and JSON, CLI plugin, securing docs in production |
| 02 | [Documenting endpoints](./02-documenting-endpoints.md) | `@ApiTags`, `@ApiOperation`, params, queries, bodies, files, versions |
| 03 | [DTO documentation](./03-dto-documentation.md) | `@ApiProperty`, enums, arrays, circular types, mapped types, generics |
| 04 | [Documenting authentication](./04-documenting-auth.md) | Bearer/API key/cookie/OAuth2 schemes, security requirements, public routes |
| 05 | [Responses and examples](./05-responses-and-examples.md) | Status codes, error schemas, examples, envelopes, pagination, downloads |
| 06 | [Client SDK generation](./06-client-sdk-generation.md) | Exporting the spec, generators, operation ids, CI checks |

## Prerequisites

- [DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md) and [ValidationPipe](../../03-core-concepts/02-validation-and-serialization/02-validation-pipe.md)
- [REST and resource design](../08-api-design/01-rest-and-resource-design.md) and [response and error format](../08-api-design/05-response-and-error-format.md)
- [Custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md) (composing documentation decorators)

## Related

- [API versioning](../08-api-design/02-api-versioning.md) (one document per version)
- [Authentication](../06-authentication/README.md) and [authorization](../07-authorization/README.md)
- [Production security](../../07-production/01-security/README.md) (don't expose docs carelessly)
