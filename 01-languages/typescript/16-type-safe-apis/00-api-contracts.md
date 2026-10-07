# API Contracts

An **API contract** is the agreement between a client and a server about what requests are valid and what responses look like. When both sides are written in TypeScript it is tempting to say "we share types, so it is type-safe". That is only partly true. Types are erased, the network is untrusted, and client and server deploy independently. This note covers what a contract contains, the ways to share one, and how to keep both sides honest.

**Prerequisites:**
- [Interfaces](../04-objects-and-interfaces/00-interfaces.md)
- [Type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)
- [Trust boundaries](../15-runtime-validation/00-trust-boundaries.md)

---

## What a contract contains

For each endpoint:

| Part | Examples |
|---|---|
| **Identity** | method and path: `GET /users/:id` |
| **Request** | path params, query string, headers, body |
| **Responses** | one body shape **per status code** (`200`, `201`, `400`, `404`, ...) |
| **Errors** | a consistent error body ([error response types](./04-error-response-types.md)) |
| **Auth** | which credentials, which permissions |
| **Behavior** | idempotency, pagination, rate limits, ordering guarantees |
| **Evolution rules** | versioning, deprecation, what counts as breaking |

Types can express the structure of the first four. The rest must be documented and tested.

## The drift problem

```ts
// server
res.json({ id: user.id, name: user.name });

// client (written separately)
interface User { id: number; name: string; email: string }
const user: User = await (await fetch("/api/users/1")).json();
user.email.toLowerCase();   // crashes: the server never sends email
```

Both sides compile. Each has a type that *claims* to describe the same thing, and nothing connects them. The contract drifted, and the compiler cannot see it.

Making an API type-safe means making the **contract a single source of truth** that both sides derive from, and **checking at runtime** where the compiler cannot.

## Ways to share a contract

| Approach | Source of truth | Runtime checking | Best for |
|---|---|---|---|
| **Shared TypeScript types** | `.ts` types in a shared package | none, types only | monorepos, small teams, low risk |
| **Shared schemas** (Zod and similar) | runtime schemas, types derived | yes, both sides can validate | full-stack TypeScript |
| **Contract-first** (OpenAPI / JSON Schema) | a language-neutral spec | generated validators or tests | public APIs, multiple languages, separate teams |
| **RPC frameworks** (for example tRPC) | server procedures, types inferred by the client | usually schema validation on input | full-stack TypeScript monorepos |
| **GraphQL + codegen** | the GraphQL schema | server validates queries | graph-shaped data, many clients |

Details of the main options:

- **Shared types** are the cheapest and the weakest. They catch compile-time mismatches between two codebases that are built together, and nothing else. Wire-format realities (dates become strings, `undefined` disappears) are easy to get wrong.
- **Shared schemas** give you validation as well. The server validates requests, the client validates responses, and the types come from the same definition ([schema validation](../15-runtime-validation/01-schema-validation.md)).
- **Contract-first** puts an OpenAPI document at the center and generates types, clients, docs, and mocks from it ([contract-first APIs](./06-contract-first-apis.md)).
- **RPC and GraphQL** are alternatives to REST-style contracts. Their tooling makes the contract implicit in the server code or the schema. Check each project's current documentation for how it works and what it requires.

There is no single right answer. A solo full-stack app, a public API for third parties, and a platform with five services written in different languages need different things.

## A contract as a typed route table

Even with no framework, you can model the contract as one type, keyed by method and path:

```ts
interface UserDto { id: string; name: string; email: string }
interface CreateUserBody { name: string; email: string }

type ApiRoutes = {
  "GET /users/:id":  { params: { id: string };  response: UserDto };
  "POST /users":     { body: CreateUserBody;    response: UserDto };
  "DELETE /users/:id": { params: { id: string }; response: void };
};

type Endpoint = keyof ApiRoutes;
type ResponseOf<E extends Endpoint> = ApiRoutes[E]["response"];

type R = ResponseOf<"GET /users/:id">;   // UserDto
```

One type describes every endpoint, and client code, server handlers, and tests can all be typed against it. [Typed fetch and API client](./05-typed-fetch-and-api-client.md) builds a client on exactly this shape, and then upgrades it with runtime schemas.

## What "type-safe" can and cannot promise

Can promise:

- A client call that omits a required parameter or sends the wrong body shape fails to compile.
- Renaming a field in the contract shows every use that must change.
- Handlers on the server return the declared shape.

Cannot promise by types alone:

- That the other side is running the same version.
- That the bytes on the wire match the declared type.
- That an intermediate gateway did not return an HTML error page.
- Behavioral rules (ordering, idempotency, permissions).

Close these gaps with **runtime validation at both ends**, **contract tests**, and **compatible evolution rules**.

## Evolving a contract

Clients and servers upgrade at different times, so assume **old clients talk to new servers and the reverse**.

| Change | Safe for existing clients? |
|---|---|
| Add a new endpoint | yes |
| Add an **optional** request field | yes |
| Add a **response** field | yes, if clients ignore unknown fields |
| Remove or rename a response field | **no** |
| Make an optional request field **required** | **no** |
| Change a field's type or meaning | **no** |
| Add a new value to a response enum or union | **risky**: clients with exhaustive checks may break |
| Remove an endpoint | **no** |
| Change an error code that clients branch on | **no** |

Practices that make evolution manageable:

- **Be tolerant when reading.** Clients should ignore unknown fields and handle unknown enum values gracefully (a `default` branch).
- **Expand, then contract.** Add the new shape alongside the old, migrate clients, then remove the old one.
- **Version deliberately** (path prefix `/v2`, header, or media type) only for breaking changes, and deprecate with notice.
- **Detect breaking changes automatically.** With an OpenAPI document, diff tools can fail CI on a breaking change.
- **Deploy order matters.** For a breaking change, ship the server that supports both shapes first.

## Choosing quickly

| Your situation | Start with |
|---|---|
| Solo or small full-stack TypeScript monorepo | shared schemas (Zod) or an RPC framework |
| Frontend and backend in one repo, types only needed | shared types, plus validation at the client boundary |
| Public API, external consumers | contract-first with OpenAPI |
| Several services in different languages | contract-first (JSON Schema / OpenAPI / protobuf-style tooling) |
| Many different clients with varied data needs | consider GraphQL |

## Common mistakes

- Defining the "same" interface independently on client and server.
- Treating a shared types package as proof that the wire format matches.
- Forgetting that JSON turns `Date` into `string` and drops `undefined` ([request and response types](./01-request-response-types.md)).
- Documenting only the success response.
- Making breaking changes without versioning or a migration path.
- Exposing the database model directly as the contract ([DTO pattern](./02-dto-pattern.md)).
- Assuming exhaustive `switch` statements over server enums are safe when the server can add values.
- Treating the contract as documentation rather than something enforced by tests and CI.

## Debugging

- When client and server disagree, **capture the real payload** and compare it to the declared type. Do not trust either side's types.
- Check **versions**: is the client talking to the server version it was built for?
- Check intermediaries (proxies, gateways, CDNs) that might rewrite or replace responses.
- Add runtime validation at the client boundary. A failing parse gives a precise path to the broken field ([validation recipes](../15-runtime-validation/04-validation-recipes.md)).
- If a breaking change shipped unnoticed, add a contract diff or contract test so it cannot recur.

## Quick summary

- A contract covers method, path, request parts, a body per status code, errors, auth, and behavior. Types express the structure only.
- Independently written types on both sides drift. Make one definition the source of truth.
- Options: shared types, shared schemas, contract-first (OpenAPI), RPC, or GraphQL. Pick by team shape and who consumes the API.
- Types do not verify the wire. Validate at runtime and add contract tests.
- Evolve compatibly: add optional things, be tolerant when reading, version only for breaking changes, and automate breaking-change detection.

**Next:** [Request and response types](./01-request-response-types.md)