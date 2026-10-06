# Request Pipeline

Every HTTP request in NestJS passes through a fixed sequence of building blocks before a response goes out: **middleware → guards → interceptors → pipes → handler → interceptors → exception filters**. Each block has one job. Most architectural mistakes in Nest apps (auth in the wrong place, validation in services, inconsistent error bodies) come from putting logic in the wrong block.

This section teaches each block on its own, then how they fit together.

```text
Request
  ↓
Middleware      "touch the raw req/res"
  ↓
Guards          "may this request proceed?"
  ↓
Interceptors    "wrap the handler" (before)
  ↓
Pipes           "validate / transform arguments"
  ↓
Handler         your controller method
  ↓
Interceptors    "map / cache / time the result" (after)
  ↓
Exception Filters  "turn thrown errors into responses"
  ↓
Response
```

> Applies to NestJS 10 and 11. Version-specific differences are flagged inline.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Pipeline overview and execution order](./01-pipeline-overview-and-execution-order.md) | Which block runs when? Where do I put what? |
| 02 | [Middleware](./02-middleware.md) | Raw request/response hooks, route matching, body parser tweaks |
| 03 | [Pipes](./03-pipes.md) | Parsing and transforming handler arguments |
| 04 | [Guards](./04-guards.md) | Authentication/authorization gates, `Reflector`, public routes |
| 05 | [Interceptors](./05-interceptors.md) | RxJS-based wrapping of the handler |
| 06 | [Interceptor recipes](./06-interceptor-recipes.md) | Logging, envelopes, timeout, cache, error mapping |
| 07 | [HTTP exceptions](./07-http-exceptions.md) | `HttpException`, built-ins, custom exceptions |
| 08 | [Exception filters](./08-exception-filters.md) | Centralized error handling and mapping |
| 09 | [Execution context](./09-execution-context.md) | `ArgumentsHost`, `ExecutionContext`, multi-protocol code |
| 10 | [Custom decorators](./10-custom-decorators.md) | Param decorators, metadata decorators, composition |

## Prerequisites

- [Request lifecycle](../../02-fundamentals/09-request-lifecycle.md) (the short version of this section)
- [Dependency injection](../../02-fundamentals/05-dependency-injection.md)
- [TypeScript decorators](../../00-prerequisites/02-typescript-decorators.md)
- Basic RxJS familiarity helps for interceptors (`map`, `tap`, `catchError`, `timeout`)

## Related

- [Validation and serialization](../02-validation-and-serialization/README.md) builds on pipes and interceptors
- [Authentication](../../04-intermediate/06-authentication/README.md) and [authorization](../../04-intermediate/07-authorization/README.md) build on guards
- [Testing pipeline components](../../04-intermediate/01-testing/04-testing-pipeline-components.md)
