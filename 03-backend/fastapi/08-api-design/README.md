# Module 08: REST API Design & Contracts

Welcome to **Module 08: REST API Design & Contracts**. This module establishes standards for enterprise API design, contract governance, hypermedia, and multi-protocol interfaces.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-rest-design-with-fastapi.md](./01-rest-design-with-fastapi.md) | **REST Design Standards** | Resource naming, HTTP verbs, and idempotency conventions. |
| [02-pagination-strategies.md](./02-pagination-strategies.md) | **Pagination Strategies** | Offset/Limit, Cursor-based, and Keyset pagination patterns. |
| [03-filtering-sorting-search-dsls.md](./03-filtering-sorting-search-dsls.md) | **Filtering, Sorting & Search** | Building flexible URL query DSLs for dynamic data grids. |
| [04-versioning-and-deprecation.md](./04-versioning-and-deprecation.md) | **Versioning & Sunset Headers**| Managing breaking changes and HTTP `Sunset` deprecation headers. |
| [05-error-format-and-problem-details.md](./05-error-format-and-problem-details.md) | **RFC 7807 Problem Details** | Standardizing error payloads with `application/problem+json`. |
| [06-idempotency-keys.md](./06-idempotency-keys.md) | **Idempotency Keys** | Redis-backed idempotency filters for safe mutation retries. |
| [07-etags-and-conditional-requests.md](./07-etags-and-conditional-requests.md) | **ETags & Cache-Control** | HTTP 304 Not Modified and conditional `If-Match` headers. |
| [08-bulk-and-batch-endpoints.md](./08-bulk-and-batch-endpoints.md) | **Bulk & Batch Operations** | Atomic vs partial-failure bulk record mutations. |
| [09-long-running-operations.md](./09-long-running-operations.md) | **Long-Running Async APIs** | 202 Accepted status codes and task polling patterns. |
| [10-openapi-customisation.md](./10-openapi-customisation.md) | **OpenAPI Customization** | Injecting custom servers, security schemes, and extensions. |
| [11-client-sdk-generation.md](./11-client-sdk-generation.md) | **Client SDK Generation** | Generating TypeScript, Python, and Go SDKs via OpenAPI. |
| [12-hypermedia-overview.md](./12-hypermedia-overview.md) | **HATEOAS & Hypermedia** | Embedding hypermedia navigation links into response schemas. |
| [13-graphql-with-strawberry.md](./13-graphql-with-strawberry.md) | **GraphQL with Strawberry** | Type-hinted code-first GraphQL server integration. |
| [14-grpc-with-python-overview.md](./14-grpc-with-python-overview.md) | **gRPC & Protobuf Overview** | High-performance binary RPC bridging with FastAPI. |
| [15-api-gateway-and-bff.md](./15-api-gateway-and-bff.md) | **API Gateway & BFF** | Backend-for-Frontend aggregation layer architecture. |
