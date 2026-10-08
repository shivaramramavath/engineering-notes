# Contract-First APIs

In a **contract-first** workflow, you write the API's specification before (or independently of) the code, and everything else is derived from or checked against it: server types, client types, validators, documentation, mocks, and tests. The usual format is an **OpenAPI** document (with JSON Schema inside). The alternative, **code-first**, generates the spec from your implementation. This note covers the workflow, the TypeScript tooling around it, what generated types do and do not guarantee, and how to keep the spec and the running service in agreement.

**Prerequisites:**
- [API contracts](./00-api-contracts.md)
- [Typed fetch and API client](./05-typed-fetch-and-api-client.md)
- [Other validation libraries](../15-runtime-validation/03-other-validation-libraries.md) (JSON Schema and TypeBox)

---

## Contract-first vs code-first

| | Contract-first | Code-first |
|---|---|---|
| Source of truth | the spec file (OpenAPI / JSON Schema) | the server code (types, schemas, decorators) |
| Spec comes from | written by hand (or designed in a tool) | **generated** from code |
| Good for | public APIs, multiple client languages, separate frontend and backend teams, design review before implementation | full-stack TypeScript teams that own both sides, fast iteration |
| Risk | spec and implementation drift unless checked | the spec describes what the code does, not what it **should** do; breaking changes can slip in as refactors |
| TypeScript fit | generate types and clients from the spec | derive the spec from schemas (Zod/TypeBox) or framework decorators |

Neither is always better. Contract-first emphasizes **design and agreement before code**. Code-first emphasizes **one implementation, one truth**. Many teams combine them: a schema library that produces both TypeScript types and JSON Schema, with the generated OpenAPI checked into the repo and diffed in CI.

## What an OpenAPI document looks like

A trimmed example (YAML):

```yaml
openapi: 3.1.0
info:
  title: Users API
  version: 1.0.0
paths:
  /users/{id}:
    get:
      operationId: getUser
      parameters:
        - name: id
          in: path
          required: true
          schema: { type: string }
      responses:
        "200":
          description: The user
          content:
            application/json:
              schema: { $ref: "#/components/schemas/User" }
        "404":
          description: Not found
          content:
            application/json:
              schema: { $ref: "#/components/schemas/ErrorResponse" }
components:
  schemas:
    User:
      type: object
      required: [id, name, email]
      properties:
        id: { type: string }
        name: { type: string }
        email: { type: string, format: email }
    ErrorResponse:
      type: object
      required: [error]
      properties:
        error:
          type: object
          required: [code, message]
          properties:
            code: { type: string }
            message: { type: string }
```

It describes paths, methods, parameters, request bodies, and a schema per response status. It is the contract from [API contracts](./00-api-contracts.md) written in a machine-readable form. OpenAPI 3.1 aligns its schemas with JSON Schema, while 3.0 uses a slightly different dialect, so check which version your tools support.

## The workflow

```text
  1. design the spec  ->  2. lint it  ->  3. generate  ->  4. implement  ->  5. verify  ->  6. guard changes
     (review it)          (style,        types, clients,   against the        server vs      breaking-change
                           consistency)   mocks, docs       generated types    spec in tests  diff in CI
```

1. **Design the spec** and review it like code, with the frontend, backend, and any consumers. Agreement happens here, before implementation.
2. **Lint it** with a spec linter (for example Spectral) to enforce naming, error shapes, and pagination conventions.
3. **Generate** TypeScript types, a client, and mock servers from it.
4. **Implement** the server against the generated types.
5. **Verify** the running service matches the spec (see below).
6. **Guard changes** by diffing spec versions in CI to catch breaking changes.

## Generating TypeScript from a spec

The `openapi-typescript` package turns a spec into TypeScript types:

```bash
npx openapi-typescript ./openapi.yaml -o ./src/api/schema.d.ts
```

It produces interfaces such as `paths` and `components` that mirror the document. You can use them directly:

```ts
import type { components, paths } from "./api/schema";

type User = components["schemas"]["User"];

type GetUserResponse =
  paths["/users/{id}"]["get"]["responses"]["200"]["content"]["application/json"];
```

Indexing by path and status is verbose, so most projects use a typed client built on those types. For example, `openapi-fetch`:

```ts
import createClient from "openapi-fetch";
import type { paths } from "./api/schema";

const client = createClient<paths>({ baseUrl: "https://api.example.com" });

const { data, error } = await client.GET("/users/{id}", {
  params: { path: { id: "1" } },
});

if (error) {
  // typed from the spec's error responses
} else {
  data;   // User
}
```

Paths, parameters, bodies, and response types all come from the spec, so a spec change that is not reflected in your calls becomes a compile error after regeneration. (Tool names, options, and APIs evolve, so follow the current documentation of whichever generator you pick. Other generators produce full clients or server stubs, including ones for other languages.)

Practices for generated code:

- **Never edit generated files.** Change the spec and regenerate.
- **Regenerate in CI and fail on a diff**, so committed output always matches the spec.
- **Pin the generator version,** since output format changes between releases.
- Keep the spec in the repo (or a shared package) where both sides can reach it.

## What generated types do not do

This is the important caveat. Generated types are still **types**:

- They are **erased**. Nothing checks the server's actual response against them at runtime ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)).
- They assume the server follows the spec. If the implementation drifts, the client types are wrong in the same silent way hand-written ones are.
- JSON-to-TypeScript mapping is imperfect. A `format: date-time` string is just `string`. `nullable` versus optional handling, `oneOf`/`anyOf` unions, and discriminators can produce types that are looser or tighter than you expect.

So add runtime checks where it matters:

- **Validate responses at the client boundary** with validators built from the same spec (for example Ajv compiled from the JSON Schema, or a schema library that can import JSON Schema), at least in development, tests, and for critical endpoints ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md)).
- **Validate requests on the server** against the spec. Frameworks such as Fastify validate with JSON Schema natively, and middleware exists for others.

## Keeping the spec and the server in agreement

Drift is the central risk of contract-first, so automate checks:

| Technique | What it catches |
|---|---|
| **Request/response validation middleware** driven by the spec | the server accepting or producing data the spec does not allow |
| **Contract tests** that call the running service and validate each response against the spec | implementation drift, undocumented status codes or fields |
| **Property-based / fuzz API testing** from the spec (tools such as Schemathesis) | crashes and spec violations on unusual inputs |
| **Consumer-driven contract testing** (for example Pact) | a provider change that breaks a specific consumer's expectations |
| **Spec diffing in CI** (breaking-change detectors for OpenAPI) | removed fields, tightened types, changed required-ness |
| **Mock servers** generated from the spec | frontend development against the contract before the backend exists |

A minimal, high-value start: one test per endpoint that calls it and validates the response body and status against the spec's schema.

```ts
import Ajv from "ajv";

const ajv = new Ajv({ strict: false });                 // configure formats and options for your spec
const validateUser = ajv.compile(userJsonSchema);       // the User schema extracted from the spec

it("GET /users/:id matches the contract", async () => {
  const res = await fetch(`${baseUrl}/users/1`);
  expect(res.status).toBe(200);
  const body = await res.json();
  expect(validateUser(body)).toBe(true);
});
```

(Handling `$ref` resolution and formats takes some setup. Use a spec-aware helper where one exists.)

## Code-first in TypeScript: deriving the spec

If you prefer code-first, you can still get a spec **out** of your code, so clients and docs stay in sync:

- **Schema libraries:** Zod 4 can produce JSON Schema directly, and companion packages exist for generating OpenAPI documents from Zod schemas. **TypeBox** schemas already *are* JSON Schema.
- **Framework support:** NestJS can generate OpenAPI from decorators with its Swagger module ([NestJS](../20-nodejs-backend/08-nestjs.md)), and Fastify can derive documentation from route schemas.
- **Check the output into the repo** and diff it in CI, so an accidental breaking change shows up in review, even though no one wrote the spec by hand.

This gives code-first teams one of contract-first's main benefits: a reviewable, language-neutral contract artifact.

## JSON Schema and TypeScript

OpenAPI schemas are JSON Schema, so JSON Schema tooling applies:

- **TypeBox** builds JSON Schema in TypeScript and infers static types from it.
- **`json-schema-to-ts`** infers types from a JSON Schema object declared `as const`, with no code generation.
- **Ajv** compiles JSON Schema to fast validators.

These keep the schema and the type derived from one definition, in the same spirit as [schema validation](../15-runtime-validation/01-schema-validation.md).

## Other contract styles

- **GraphQL** has its schema as a contract, with code generators for types and clients.
- **RPC frameworks** such as tRPC make the server's procedure definitions the contract, shared directly via TypeScript, which works best when both sides are TypeScript in one repo.
- **Protocol Buffers / gRPC** use an IDL with generated code for many languages.

Choose by the same criteria as before: who consumes the API, in which languages, and how independently the sides evolve ([API contracts](./00-api-contracts.md)).

## When to choose contract-first

Good fits:

- **Public or partner APIs** where the spec is a product.
- **Multiple client languages** (web, mobile, other services) that need generated SDKs.
- **Separate teams** who need to agree before building.
- **Regulated or long-lived APIs** where breaking changes need strict control.

Less compelling:

- A small full-stack TypeScript app where one team owns both sides. A shared schema package or an RPC tool is lighter.
- Rapidly changing internal prototypes, where a hand-maintained spec becomes busywork.

## Important rules and misconceptions

- **A spec is a promise, not an enforcement.** It matters only if something checks the implementation against it.
- **Generated types are not runtime validation.** They share the weakness of every TypeScript type.
- **Contract-first does not mean slower.** The cost is moving design work earlier. The benefit is parallel work using mocks and generated code.
- **A spec in a wiki is not a contract.** It needs to live with the code, be linted, and be checked in CI.
- **Different OpenAPI versions differ.** Tools may not support every version or feature.
- **Generated optional/nullable types depend on the spec's `required` and `nullable` details,** so write those carefully.

## Common mistakes

- Treating generated types as proof that responses match.
- Editing generated files by hand.
- Not regenerating in CI, so committed output goes stale.
- Letting the spec drift from the implementation with no verification.
- Documenting only success responses.
- Leaving `required` out of schemas, so everything becomes optional in the generated types.
- Using spec-wide `additionalProperties` defaults without thinking about the effect on generated types.
- Making breaking changes without a diff check.
- Mixing hand-written types and generated types for the same payloads.

## Debugging

- If a generated type looks wrong, read the spec's schema first: `required`, `nullable`, `oneOf`, and `additionalProperties` account for most surprises.
- If the client and server disagree at runtime, **capture the response and validate it against the spec's schema** to see which side is wrong.
- If CI reports a spec diff failure, look at the specific change reported (removed property, narrowed type, new required field).
- If a generator update changes output, compare generated files and pin or upgrade deliberately.
- Use a spec visualizer or linter to find structural problems such as unresolved `$ref`s.

## Quick summary

- Contract-first writes the spec first and derives types, clients, mocks, docs, and tests from it. Code-first derives the spec from code.
- OpenAPI plus JSON Schema is the common format. Tools such as `openapi-typescript` and `openapi-fetch` turn it into typed calls.
- Generated types are compile-time only. Add runtime validation and contract tests to catch drift.
- Automate agreement: lint the spec, regenerate in CI, diff for breaking changes, and test the running service against the spec.
- Code-first teams can still produce a spec from schemas or decorators and review it in CI.
- Choose contract-first when the API serves several teams, languages, or outside consumers.

**Next:** [17 Design Patterns](../17-design-patterns/README.md)