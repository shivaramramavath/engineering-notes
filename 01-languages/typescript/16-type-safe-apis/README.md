# 16 - Type-Safe APIs

How to make the boundary between a client and a server as safe as the code on either side of it. The earlier sections gave you the tools: types, generics, validation. This section applies them to HTTP APIs: describing the contract, typing requests and responses, separating wire types from domain types, paginating lists, shaping errors, building a typed client, and keeping a spec and a running service in agreement.

The central problem is **drift**. Two sides that each declare their own types will eventually disagree, and the compiler cannot see across the network. Everything here is a way to make one definition the source of truth, and to check it at runtime where types cannot.

## Prerequisites

- [15 Runtime Validation](../15-runtime-validation/README.md): schemas and trust boundaries underpin this entire section
- [06 Generics](../06-generics/README.md) and [10 Advanced Types](../10-advanced-types/README.md): generic page types, route-table typing
- [11 Error Handling](../11-error-handling/README.md): error classes and the Result pattern

## Notes in this section

| # | Note | Covers |
|---|---|---|
| 00 | [API contracts](./00-api-contracts.md) | What a contract contains, the drift problem, ways to share one, evolving it compatibly |
| 01 | [Request and response types](./01-request-response-types.md) | Typing params, query, body, responses per status, wire vs domain types, Express generics |
| 02 | [The DTO pattern](./02-dto-pattern.md) | Separating input/output types from entities, mappers, allow-lists, preventing leaks |
| 03 | [Pagination types](./03-pagination-types.md) | Offset vs cursor, generic page types, typed cursors, async iteration |
| 04 | [Error response types](./04-error-response-types.md) | Consistent error bodies, discriminated error unions, Problem Details, client-side handling |
| 05 | [Typed fetch and API client](./05-typed-fetch-and-api-client.md) | From a naive wrapper to a route-table client with derived types and validation |
| 06 | [Contract-first APIs](./06-contract-first-apis.md) | OpenAPI workflow, generated types and clients, keeping spec and service in agreement |

Read 00 to 02 first. 03 and 04 are independent and can follow in either order. 05 pulls the earlier notes together into a client, and 06 shows the spec-driven alternative.

## Which note answers my question?

| Question | Go to |
|---|---|
| How do I stop client and server types from drifting apart? | [00](./00-api-contracts.md) |
| Which contract approach fits my team (shared types, schemas, OpenAPI, RPC)? | [00](./00-api-contracts.md) |
| What changes are breaking for existing clients? | [00](./00-api-contracts.md) |
| Why is my `Date` a string on the client? | [01](./01-request-response-types.md) |
| How do I type an Express handler properly? | [01](./01-request-response-types.md) |
| How do I avoid sending `passwordHash` or accepting `role` from clients? | [02](./02-dto-pattern.md) |
| Offset or cursor pagination? | [03](./03-pagination-types.md) |
| How do I type and validate a pagination cursor? | [03](./03-pagination-types.md) |
| What should my error responses look like? | [04](./04-error-response-types.md) |
| How should a client handle a gateway's HTML error page? | [04](./04-error-response-types.md) |
| How do I make `fetch` calls type-safe? | [05](./05-typed-fetch-and-api-client.md) |
| How do I get path parameter types from `"/users/:id"`? | [05](./05-typed-fetch-and-api-client.md) |
| Should I write an OpenAPI spec first, or generate it? | [06](./06-contract-first-apis.md) |
| How do I catch a breaking API change in CI? | [06](./06-contract-first-apis.md) |

## Ideas that recur across the section

- **One source of truth.** A shared schema, a route table, or an OpenAPI spec. Types derived from it cannot disagree with it.
- **Types do not cross the network.** Validate at the boundary on both sides, and treat error bodies as untrusted too.
- **Wire types are not domain types.** JSON changes `Date`, drops `undefined`, and cannot carry `bigint`. Convert explicitly.
- **Allow-list what crosses the boundary.** DTOs and mappers decide exactly which fields leave and enter.
- **Model every outcome.** A response is a union of statuses. An error is a stable `code` plus structured details.
- **Be tolerant when reading, careful when changing.** Ignore unknown fields, handle unknown codes, evolve additively.
- **Specs are promises only if something checks them.** Lint, regenerate, test, and diff in CI.

## Related sections

- [06 Generics](../06-generics/README.md): generic response and page types
- [07 Utility Types](../07-utility-types/README.md): deriving DTOs with `Pick`, `Omit`, `Partial`
- [10 Advanced Types: template literal types](../10-advanced-types/04-template-literal-types.md): path parameter extraction
- [11 Error Handling](../11-error-handling/README.md): server-side error classes and boundaries
- [12 Async and Iteration](../12-async-and-iteration/README.md): cancellation and iterating pages
- [15 Runtime Validation](../15-runtime-validation/README.md): Zod, recipes, trust boundaries
- [17 Design Patterns: repository](../17-design-patterns/04-repository.md)
- [19 React and Frontend: server state](../19-react-and-frontend/07-server-state-tanstack-query.md): using a typed client with TanStack Query
- [20 Node.js Backend](../20-nodejs-backend/README.md): Express, middleware, NestJS
- [23 Security](../23-security/README.md): input validation and not leaking internals
- [26 Projects: type-safe API client](../26-projects/02-type-safe-api-client/README.md) and [REST API](../26-projects/03-rest-api/README.md)

## Next

[17 Design Patterns](../17-design-patterns/README.md)