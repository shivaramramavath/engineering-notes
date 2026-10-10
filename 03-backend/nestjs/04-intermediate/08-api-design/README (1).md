# API Design

An API is a product with a long tail of users you can't update. Code you can refactor tomorrow; a response shape, a status code, or a URL that clients depend on is a **contract** you'll live with for years. This section covers the design decisions that are cheap to get right up front and expensive to change later.

```text
 resources & URLs ─► versioning ─► pagination ─► filtering/sorting ─► response & error shape
        │                                                                      │
        └──────────────── safe retries (idempotency) ◄─────────────────────────┘
                                       │
                          events to others (webhooks)
```

> Applies to NestJS 10/11. Some behaviors depend on the HTTP platform (Express 5 in Nest 11 changed query-string parsing defaults; see the filtering note). Standards referenced (HTTP semantics, RFC 9457 problem details, the `Idempotency-Key` header draft) evolve, so check current specifications before treating details as final.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [REST and resource design](./01-rest-and-resource-design.md) | URLs, methods, status codes, PUT vs PATCH, actions, async operations |
| 02 | [API versioning](./02-api-versioning.md) | URI/header/media-type versioning in Nest, evolving without breaking clients |
| 03 | [Pagination](./03-pagination.md) | Offset vs cursor, response shapes, limits, stable ordering |
| 04 | [Filtering, sorting, and search](./04-filtering-sorting-and-search.md) | Query parameter conventions, whitelisting, safe dynamic queries |
| 05 | [Response and error format](./05-response-and-error-format.md) | Consistent success/error bodies, RFC 9457, error codes |
| 06 | [Idempotency](./06-idempotency.md) | Safe retries, `Idempotency-Key`, concurrency |
| 07 | [Webhooks](./07-webhooks.md) | Sending and receiving signed, retried event callbacks |

## Prerequisites

- [Controllers](../../02-fundamentals/03-controllers.md) and [request data](../../02-fundamentals/07-request-data.md)
- [Validation and serialization](../../03-core-concepts/02-validation-and-serialization/README.md)
- [HTTP basics](../../00-prerequisites/04-http-and-rest-basics.md)

## Related

- [OpenAPI and Swagger](../09-openapi-and-swagger/README.md) (documenting the contract)
- [Exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md) and [interceptors](../../03-core-concepts/01-request-pipeline/05-interceptors.md)
- [System design: scalable REST API](../../11-system-design/02-scalable-rest-api.md)
