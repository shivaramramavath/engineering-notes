# Contract-First APIs, OpenAPI, and tRPC in TypeScript

> Contract-first vs code-first API design in TypeScript: OpenAPI 3.1 specification, openapi-typescript, tRPC end-to-end type sharing, GraphQL codegen, and contract drift prevention.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Advanced |
| **Prerequisites** | [16-type-safe-apis/00-api-contracts.md](./00-api-contracts.md), [16-type-safe-apis/05-typed-fetch-and-api-client.md](./05-typed-fetch-and-api-client.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In multi-team software engineering, the single greatest source of production outages is **API Drift**: the backend changes a response field or alters validation rules, but client applications continue sending outdated request bodies or expecting deprecated properties.

Two distinct architectural philosophies exist to maintain alignment:
1. **Contract-First (Specification-First)**: The API is defined first in an industry-standard machine-readable specification language (OpenAPI 3.1, Protocol Buffers, or GraphQL Schema). TypeScript interfaces and validation schemas are automatically generated from this specification for both backend servers and frontend clients.
2. **Code-First with Direct Type Sharing (e.g. tRPC)**: When both frontend and backend are written in TypeScript in a monorepo, tRPC exposes server router types directly to client React/Vue applications without code generation or runtime schema bloat.

By adopting contract-first or tRPC workflows, teams eliminate manual synchronization, prevent API drift, and catch breaking schema modifications during continuous integration (CI).

```
Contract-First:
  OpenAPI 3.1 YAML  -->  openapi-typescript  -->  Shared TypeScript Types
         |                                                 |
         v                                                 v
  Backend Mock / Stubs                             Frontend Fetch Client

Code-First (tRPC):
  AppRouter (Backend)  =========================>  createTRPCReact<AppRouter>()
  (Compile-time type export)                       (Zero code generation)
```

---

## 2. Core Concepts

### 1. OpenAPI 3.1 Specification and Codegen
OpenAPI 3.1 aligns directly with JSON Schema 2020-12. Using `openapi-typescript`, an OpenAPI YAML file compiles into static TypeScript type declarations:
```typescript
// Generated from openapi.yaml
export interface paths {
  '/users/{id}': {
    get: {
      parameters: {
        path: { id: string };
      };
      responses: {
        200: {
          content: {
            'application/json': {
              id: string;
              name: string;
              email: string;
            };
          };
        };
        404: {
          content: {
            'application/problem+json': {
              title: string;
              status: 404;
            };
          };
        };
      };
    };
  };
}
```

### 2. tRPC End-to-End Type Safety
In a monorepo, tRPC allows developers to write backend procedures with runtime validation (Zod, ArkType) and directly import the `type AppRouter` into frontend packages:
```typescript
// Backend (server/routers/app.ts)
import { initTRPC } from '@trpc/server';
import { z } from 'zod';

const t = initTRPC.create();

export const appRouter = t.router({
  getUser: t.procedure
    .input(z.object({ id: z.string() }))
    .query(({ input }) => {
      return { id: input.id, name: 'Alice' };
    })
});

export type AppRouter = typeof appRouter;
```

```typescript
// Frontend (client/index.ts)
import { createTRPCProxyClient, httpBatchLink } from '@trpc/client';
import type { AppRouter } from '../server/routers/app';

const trpc = createTRPCProxyClient<AppRouter>({
  links: [httpBatchLink({ url: 'http://localhost:3000/trpc' })]
});

// Fully autocompleted and type-checked!
const user = await trpc.getUser.query({ id: '123' });
```

---

## 3. How It Works Internally

### OpenAPI Type Indexing
A type-safe fetch client (like `openapi-fetch`) accepts the generated `paths` interface as a generic parameter. When calling `client.GET('/users/{id}', options)`, TypeScript indexes:
`paths['/users/{id}']['get']['responses'][200]['content']['application/json']`
to determine the return type.

### tRPC Compiler Erasure
`import type { AppRouter }` emits **zero bytes of JavaScript** to the frontend bundle. The frontend bundle never imports backend dependencies (like database drivers or Node.js built-ins). Only the TypeScript AST type node is transferred across the monorepo boundary.

---

## 4. Syntax & API Reference

### OpenAPI Fetch Client Configuration
```typescript
import createClient from 'openapi-fetch';
import type { paths } from './generated-api-types';

export const apiClient = createClient<paths>({ baseUrl: 'https://api.example.com/v1' });

// Fully type-checked request
const { data, error } = await apiClient.GET('/users/{id}', {
  params: {
    path: { id: 'usr_42' }
  }
});
```

### GraphQL Codegen Types
GraphQL Code Generator compiles `.graphql` queries into typed DocumentNodes:
```typescript
import { TypedDocumentNode } from '@graphql-typed-document-node/core';

export interface GetUserQuery {
  user: { id: string; name: string };
}

export interface GetUserVariables {
  userId: string;
}

export declare const GetUserDocument: TypedDocumentNode<GetUserQuery, GetUserVariables>;
```

---

## 5. Practical Examples

### Example 1: Basic tRPC Router with Mutation
```typescript
import { initTRPC, TRPCError } from '@trpc/server';
import { z } from 'zod';

const t = initTRPC.create();

export const postRouter = t.router({
  createPost: t.procedure
    .input(
      z.object({
        title: z.string().min(5),
        content: z.string()
      })
    )
    .mutation(async ({ input }) => {
      return {
        id: crypto.randomUUID(),
        title: input.title,
        content: input.content,
        createdAt: new Date().toISOString()
      };
    })
});

export type PostRouter = typeof postRouter;
```

### Example 2: Real-World OpenAPI Spec to Client Pipeline
```typescript
// Types generated by openapi-typescript
export interface paths {
  '/analytics/events': {
    post: {
      requestBody: {
        content: {
          'application/json': {
            eventName: string;
            timestamp: number;
            properties: Record<string, string | number>;
          };
        };
      };
      responses: {
        202: {
          content: {
            'application/json': { accepted: boolean };
          };
        };
      };
    };
  };
}

export async function sendAnalytics(
  client: ReturnType<typeof createClient<paths>>,
  eventName: string,
  properties: Record<string, string | number>
) {
  return await client.POST('/analytics/events', {
    body: {
      eventName,
      timestamp: Date.now(),
      properties
    }
  });
}
```

### Example 3: Edge Case - Handling Union Status Responses in OpenAPI
```typescript
type ResponseContent<TPaths, TPath extends keyof TPaths, TMethod extends keyof TPaths[TPath]> =
  TPaths[TPath][TMethod] extends { responses: infer R } ? R : never;

type StatusUnion<TResponses> = {
  [K in keyof TResponses]: {
    status: K;
    body: TResponses[K] extends { content: { 'application/json': infer B } } ? B : never;
  };
}[keyof TResponses];
```

### Example 4: Failure Case - Type Drift Prevention in CI
```typescript
// If the backend router changes 'title' to 'heading', tRPC client catches it instantly!
type RouterMock = {
  getPost: {
    input: { id: string };
    output: { id: string; heading: string }; // Changed from 'title'
  };
};

function renderPost(post: RouterMock['getPost']['output']) {
  // @ts-expect-error TS2339: Property 'title' does not exist on type '{ id: string; heading: string; }'.
  console.log(post.title);
}
```

### Example 5: Idiomatic Fastify OpenAPI Validation Integration
```typescript
import Fastify from 'fastify';

const fastify = Fastify();

const createUserSchema = {
  body: {
    type: 'object',
    required: ['name', 'email'],
    properties: {
      name: { type: 'string' },
      email: { type: 'string', format: 'email' }
    }
  }
} as const;

fastify.post('/api/users', { schema: createUserSchema }, async (req, reply) => {
  // fastify automatically infers body type from the JSON schema definition when using TypeProvider!
  reply.code(201).send({ message: 'User created' });
});
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Accidental Runtime Import of Backend in Frontend**:
   Importing `import { appRouter }` instead of `import type { AppRouter }` in a frontend package pulls server-side code (database secrets, Node APIs) into the client bundle, causing bundler compilation errors (`Module not found: Can't resolve 'fs'`). Always enforce `import type`.
2. **Schema Drift in Contract-First**:
   If developers edit the TypeScript code directly instead of the OpenAPI YAML file, the specification falls out of sync. Use CI linters (such as Spectral) to assert that server implementations conform to the OpenAPI spec.
3. **GraphQL Nullability Gotchas**:
   In GraphQL, fields are nullable by default unless marked with `!`. Generated TypeScript types will contain `string | null | undefined`, forcing extensive null checking on the frontend.
4. **Circular References in Schemas**:
   Recursive schemas (e.g. `Category` having `subcategories: Category[]`) can cause infinite loops during codegen unless properly resolved via `$ref` pointers.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Value Import of Server Router** | `import { AppRouter } from '../server'` | `import type { AppRouter } from '../server'` | Leaks server code and crashes frontend bundlers |
| **Manual Frontend Types** | Hand-writing interfaces mirroring API | Generate types from OpenAPI or tRPC | Inevitable API drift and unannounced breaking changes |
| **Ignoring CI Spec Validation** | Merging PRs without checking OpenAPI lint | Spectral / Optic breaking change check in CI | Breaking API changes shipped silently to production |
| **Loose OpenAPI Schemas** | Omitting `additionalProperties: false` | Enforce strict schemas | Client sends invalid unexpected fields without validation |
| **Duplicating Route Strings** | Hardcoding `/users/update/:id` in 5 places | Centralized type-safe route registry | Route refactoring leaves broken references |
| **Over-relying on Code-First for Public APIs** | Using tRPC for third-party public consumers | OpenAPI 3.1 specification for public APIs | External partners cannot consume proprietary TS routers |

---

## 8. Performance & Type-Checker Complexity

- **Type Generation Speed**: `openapi-typescript` generates pure types without runtime JS wrappers. A 5,000-line OpenAPI specification compiles into TypeScript definitions in ~120ms.
- **TSServer Memory**: Monorepos with massive tRPC routers (1,000+ endpoints) can increase TSServer heap memory usage. Break large monolithic routers into sub-routers (`userRouter`, `billingRouter`, `adminRouter`) to keep compiler checks fast.

---

## 9. Compiler Internals

When importing types across packages in a monorepo:
```typescript
import type { AppRouter } from '@backend/router';
```
The TypeScript compiler's Binder marks the symbol as `SymbolFlags.Type`. During the Emit phase (`tsc`), the emitter completely drops the import statement from the generated `.js` output file, leaving zero traces in runtime code.

---

## 10. Best Practices & Design Guidelines

1. **Use tRPC for Internal Monorepos**: When both frontend and backend are TypeScript within the same repository, tRPC provides the best developer experience.
2. **Use OpenAPI for Multi-Language or Public APIs**: If mobile apps (Swift, Kotlin) or third-party developers consume the API, OpenAPI 3.1 is the universal standard.
3. **Enforce `import type` in ESLint**: Configure `@typescript-eslint/consistent-type-imports` to `error` to prevent accidental bundling of server code.
4. **Automate Codegen via Git Hooks / CI**: Run OpenAPI codegen on commit or in CI to ensure generated types are always up to date.

---

## 11. Real-World Case Study

### Multi-Platform Monorepo Architecture
A high-growth startup maintains a web app (Next.js), mobile app (React Native), and microservices backend (Fastify) in a Turborepo monorepo:

```
apps/
  web/          (Consumes @repo/api-client via tRPC)
  mobile/       (Consumes @repo/api-client via openapi-fetch)
packages/
  api-spec/     (Single source of truth: openapi.yaml + codegen)
  server/       (Fastify implementing OpenAPI routes + tRPC router)
```

By linking `@repo/api-spec` to both web and mobile, every pull request that modifies a database schema or endpoint automatically triggers type checks across all apps. If a field is renamed or removed, `turbo run build` fails immediately in CI before deployment.

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between contract-first and code-first API design?</summary>

In contract-first design, the API specification (OpenAPI, Protobuf, GraphQL schema) is authored before writing any implementation code; backend handlers and client SDKs are generated from the spec. In code-first design, the implementation code (such as a tRPC router or NestJS controller) is written first, and types or specifications are inferred directly from the code.
</details>

<details>
<summary>2. How does tRPC achieve end-to-end type safety without generating code?</summary>

tRPC leverages TypeScript's ability to export and import pure type definitions (`import type { AppRouter }`). The client library uses JavaScript proxies (`Proxy`) at runtime to dynamically intercept property accesses (e.g., `client.user.getById.query(...)`) while TypeScript matches those accesses against the type definition of the server router.
</details>

<details>
<summary>3. Why must we use `import type` when consuming server router types in client code?</summary>

Because TypeScript types are erased during compilation. If a value import is used (`import { AppRouter }`), the bundler (Webpack, Vite, esbuild) attempts to import the actual JavaScript file, which pulls in server-only dependencies (like `node:fs`, `pg`, `bcrypt`) that fail to resolve in browser environments.
</details>

<details>
<summary>4. What are the benefits of OpenAPI 3.1 over OpenAPI 3.0?</summary>

OpenAPI 3.1 achieves 100% dialect compatibility with the latest JSON Schema standard (JSON Schema 2020-12). This allows arbitrary schema composition (`anyOf`, `oneOf`, `prefixItems`), native `null` typing (`type: ["string", "null"]`), and seamless integration with modern schema libraries like Zod and TypeBox.
</details>

<details>
<summary>5. When should a team choose OpenAPI over tRPC?</summary>

Choose OpenAPI when:
1. The API is consumed by clients written in non-TypeScript languages (Python, Go, Swift, Java).
2. The API is public and used by external third-party developers.
3. Backend and frontend teams operate in separate repositories and deploy on independent cadences.
</details>

<details>
<summary>6. How do you prevent breaking API changes in CI with OpenAPI?</summary>

Use schema diffing tools like `optic diff` or `oasdiff` in continuous integration pipelines. These tools compare the new OpenAPI spec against the production branch, detecting deleted endpoints, renamed fields, or newly added required parameters, and fail the CI build if breaking changes occur without proper API versioning.
</details>

<details>
<summary>7. What is `TypedDocumentNode` in GraphQL TypeScript workflows?</summary>

`TypedDocumentNode` is an enhancement of GraphQL's `DocumentNode` that carries phantom type parameters for query variables and result data: `TypedDocumentNode<ResultType, VariablesType>`. GraphQL clients (Apollo, Urql) accept this node to automatically infer types without manual type assertions.
</details>

<details>
<summary>8. How do you handle file uploads in tRPC?</summary>

tRPC supports standard `FormData` payloads via the native Fetch adapter, or developers can create a signed S3/GCS URL procedure in tRPC and have the client upload the file directly to object storage via HTTP PUT.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Create a Generic Schema Route Extractor</summary>

**Problem:** Given a `paths` interface from OpenAPI, write a utility type `GetRouteResponse<TPaths, TPath extends keyof TPaths>` that extracts the 200 response JSON content.

**Solution:**
```typescript
export type GetRouteResponse<TPaths, TPath extends keyof TPaths> =
  TPaths[TPath] extends { get: { responses: { 200: { content: { 'application/json': infer R } } } } }
    ? R
    : never;
```
</details>

<details>
<summary>Exercise 2 (Easy): Type-Safe HTTP Method Filter</summary>

**Problem:** Write a utility type `FilterByMethod<TPaths, TMethod extends 'get' | 'post'>` that returns all path strings that support the given HTTP method.

**Solution:**
```typescript
export type FilterByMethod<TPaths, TMethod extends 'get' | 'post'> = {
  [K in keyof TPaths]: TMethod extends keyof TPaths[K] ? K : never;
}[keyof TPaths];
```
</details>

<details>
<summary>Exercise 3 (Medium): Strongly-Typed Procedure Registry</summary>

**Problem:** Create a type-safe procedure registry where procedures are defined with input and output schemas, and an executor function verifies inputs before invocation.

**Solution:**
```typescript
import { z } from 'zod';

export interface Procedure<TInput, TOutput> {
  schema: z.ZodType<TInput>;
  handler: (input: TInput) => Promise<TOutput>;
}

export function defineProcedure<TInput, TOutput>(
  schema: z.ZodType<TInput>,
  handler: (input: TInput) => Promise<TOutput>
): Procedure<TInput, TOutput> {
  return { schema, handler };
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Simple Proxy-Based Router Client</summary>

**Problem:** Implement a proxy helper that allows calling nested router procedures `client.users.get({ id: '1' })` by mapping property names to `/users.get` HTTP endpoints.

**Solution:**
```typescript
export function createProxyRpcClient(baseUrl: string) {
  return new Proxy({}, {
    get: (_, moduleName: string) => {
      return new Proxy({}, {
        get: (_, methodName: string) => {
          return async (input: unknown) => {
            const res = await fetch(`${baseUrl}/${moduleName}.${methodName}`, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(input)
            });
            return res.json();
          };
        }
      });
    }
  });
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Breaking Change Detector Type</summary>

**Problem:** Write a type `AssertBackwardCompatible<OldSpec, NewSpec>` that yields a compilation error if `NewSpec` makes an existing optional field required or changes an existing field type.

**Solution:**
```typescript
export type AssertBackwardCompatible<OldSpec, NewSpec> =
  NewSpec extends OldSpec
    ? true
    : {
        __ERROR__: 'Breaking change detected: New specification is not assignable to Old specification';
      };
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe Event-Driven Contract Emitter</summary>

**Problem:** Design a contract-first event emitter interface where event topics, payload types, and version headers are strictly typed and immutable.

**Solution:**
```typescript
export interface EventContract {
  'user.created': { version: 1; payload: { userId: string; email: string } };
  'order.completed': { version: 2; payload: { orderId: string; totalAmountCents: number } };
}

export class ContractEventEmitter {
  public emit<K extends keyof EventContract>(
    topic: K,
    event: EventContract[K]
  ): void {
    console.log(`Publishing to [${topic}] v${event.version}:`, event.payload);
  }
}
```
</details>
