# Module 09: API Development & Protocol Engineering

> Designing, documenting, and scaling enterprise APIs: REST, OpenAPI, Idempotency, Webhooks, GraphQL, tRPC, and gRPC.

---

## Module Overview

Modern backends expose diverse API interfaces depending on client requirements: external third-party public REST APIs, real-time GraphQL endpoints, internal microservice gRPC streams, and end-to-end type-safe fullstack RPCs via tRPC.

---

## Contents

| Index | Note | Core Focus |
| :---: | :--- | :--- |
| **01** | [01-rest-api-design.md](./01-rest-api-design.md) | REST design principles, resource modeling, Richardson Maturity Model. |
| **02** | [02-versioning-and-pagination.md](./02-versioning-and-pagination.md) | API versioning strategies (URI, header), cursor vs offset pagination. |
| **03** | [03-filtering-and-sorting.md](./03-filtering-and-sorting.md) | URL query param conventions, dynamic SQL filtering, multi-field sorting. |
| **04** | [04-validation.md](./04-validation.md) | Request payload validation with Zod and Joi, sanitization. |
| **05** | [05-error-responses.md](./05-error-responses.md) | Standardized error formats, RFC 7807 Problem Details. |
| **06** | [06-idempotency.md](./06-idempotency.md) | Idempotency keys (`Idempotency-Key`), Redis locking, safe retries. |
| **07** | [07-webhooks.md](./07-webhooks.md) | Outgoing webhook delivery, retry policies with backoff, signature signing. |
| **08** | [08-graphql-with-node-apollo-and-yoga.md](./08-graphql-with-node-apollo-and-yoga.md) | GraphQL Yoga & Apollo Server, schema design, query depth limits. |
| **09** | [09-trpc-and-grpc-in-node.md](./09-trpc-and-grpc-in-node.md) | End-to-end type safety with tRPC, binary microservices with gRPC. |
| **10** | [10-openapi-first-and-sdk-generation.md](./10-openapi-first-and-sdk-generation.md) | OpenAPI 3.1 contract-first development, SDK generation with openapi-typescript. |

---

## Next Steps
Proceed to [10-architecture](../10-architecture/README.md) to explore Clean Architecture, Hexagonal boundaries, and Dependency Injection.
