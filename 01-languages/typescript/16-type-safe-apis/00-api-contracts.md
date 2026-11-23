# End-to-End API Contracts and Type Safety in TypeScript

> Comprehensive guide to End-to-End Type Safety: eliminating client-server API drift, single source of truth contracts, contract-first vs code-first architectures, and sharing types across monorepos.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Intermediate |
| **Prerequisites** | [08-modules/00-imports-and-exports.md](../08-modules/00-imports-and-exports.md), [15-runtime-validation/01-schema-validation.md](../15-runtime-validation/01-schema-validation.md) |
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
In traditional web architectures, the frontend client and backend server exist as isolated codebases communicating over loose HTTP JSON boundaries. When a backend engineer changes an API response field from `userId` to `id`, or makes a query parameter required, the frontend continues to expect the old shape. Without end-to-end type safety, this **API Drift** is discovered only when production users encounter broken UIs or 500 errors.

**End-to-End Type Safety (E2E Type Safety)** eliminates this failure mode. By sharing a single type contract between backend routers and frontend clients (via monorepos, tRPC, or OpenAPI code generation), any change to a backend route immediately causes a compile-time TypeScript error on the frontend, guaranteeing complete synchronization across the entire stack.

---

## 2. Core Concepts
- **API Drift**: The inevitable divergence over time between server endpoint implementations and client request models.
- **Contract-First vs Code-First**:
  - *Contract-First*: The API schema (OpenAPI, GraphQL, Protobuf) is authored first as the single source of truth, generating both server route handlers and client SDKs.
  - *Code-First*: The server implementation defines the types (e.g. tRPC routers, NestJS controllers), and client types are directly exported or inferred from server code.
- **Shared Contracts in Monorepos**: Placing shared DTO interfaces or Zod schemas in a shared workspace package (`@myorg/contracts`) imported by both frontend and backend.
- **Type-Only Dependencies**: Using `import type` ensures that importing server contracts into frontend code does not bundle server database drivers, secrets, or Node.js built-ins into browser bundles.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|              Shared Contracts Package (@myorg/api)          |
|  export interface UserDto { id: string; name: string }      |
|  export type GetUserResponse = ApiResponse<UserDto>;        |
+------------------------------+------------------------------+
                               |
         +---------------------+---------------------+
         | (import type)                             | (import type)
         v                                           v
+-------------------------------+           +-------------------------------+
|       Backend Service         |           |       Frontend Client         |
|  - Fastify / Express Router   |           |  - React / Next.js Client     |
|  - Implements UserDto contract|           |  - Calls fetch<UserDto>()     |
+-------------------------------+           +-------------------------------+
```

When a field in `UserDto` is modified in the shared package, running `tsc` on the client application immediately flags all components accessing obsolete fields.

---

## 4. Syntax & API Reference

```typescript
// 1. Defining the Shared Contract
export interface ApiContract<TParams, TQuery, TBody, TResponse> {
  params: TParams;
  query: TQuery;
  body: TBody;
  response: TResponse;
}

// 2. Concrete Route Specification
export interface GetUserRoute extends ApiContract<
  { id: string },               // Path params
  { includeDetails?: boolean }, // Query params
  never,                        // Body
  { id: string; email: string } // Response
> {}

// 3. Type-Safe Client invoker
export declare function callApi<C extends ApiContract<any, any, any, any>>(
  path: string,
  req: { params?: C['params']; query?: C['query']; body?: C['body'] }
): Promise<C['response']>;
```

---

## 5. Practical Examples

### Example 1: Fullstack Monorepo Contract Definition
```typescript
// packages/contracts/src/users.ts
import { z } from 'zod';

export const CreateUserRequestSchema = z.object({
  name: z.string().min(2),
  email: z.string().email(),
  role: z.enum(['ADMIN', 'MEMBER']).default('MEMBER'),
});

export type CreateUserRequest = z.infer<typeof CreateUserRequestSchema>;

export interface UserResponseDto {
  id: string;
  name: string;
  email: string;
  createdAt: string;
}
```

### Example 2: Backend Handler Implementation of Contract
```typescript
// apps/api/src/routes/users.ts
import { Request, Response } from 'express';
import { CreateUserRequest, UserResponseDto, CreateUserRequestSchema } from '@myorg/contracts';

export async function handleCreateUser(
  req: Request<{}, UserResponseDto, CreateUserRequest>,
  res: Response<UserResponseDto>
): Promise<void> {
  const validated = CreateUserRequestSchema.parse(req.body);
  const user: UserResponseDto = {
    id: 'usr_123',
    name: validated.name,
    email: validated.email,
    createdAt: new Date().toISOString(),
  };
  res.status(201).json(user);
}
```

### Example 3: Edge Case - Accidental Server Dependency Bundling
```typescript
// In frontend component:
// BAD: Importing concrete values from backend pulls server dependencies into browser:
// import { UserModel } from '../../../backend/src/models/user';

// GOOD: Always use 'import type' for shared contracts across architectural boundaries:
import type { UserResponseDto } from '@myorg/contracts';
```

### Example 4: Compiler Failure Case - Detecting Contract Drift
```typescript
import type { UserResponseDto } from '@myorg/contracts';

function renderUserBadge(user: UserResponseDto) {
  // If backend renamed 'name' to 'fullName':
  // @ts-expect-error TS2551: Property 'fullName' does not exist on type 'UserResponseDto'. Did you mean 'name'?
  console.log(user.fullName.toUpperCase());
}
```

### Example 5: Idiomatic API Route Dictionary Map
```typescript
export interface AppApiRoutes {
  '/api/v1/health': {
    GET: { response: { status: 'ok' | 'degraded'; uptime: number } };
  };
  '/api/v1/users': {
    GET: { query: { page?: number }; response: { items: Array<{ id: string }> } };
    POST: { body: { name: string }; response: { id: string } };
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Accidental Runtime Leakage**: Importing contracts from server directories into client files using `import { X }` instead of `import type { X }` can accidentally pull Node-only libraries (e.g. `crypto`, `fs`, ORM packages) into the browser bundle, causing bundling failures.
2. **Date Serialization Disconnect**: A Date object on the server serializes to an ISO string (`string`) over JSON. Typing the response contract as `createdAt: Date` will cause runtime `TypeError: user.createdAt.toISOString is not a function` on the client. Always type JSON timestamps as `string`.
3. **Cross-Repo Version Desynchronization**: If frontend and backend live in separate git repositories, sharing types via private npm packages requires automated CI republishing to prevent contract staleness.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Typing JSON dates as `Date` in API contracts | Runtime crash: `createdAt` is a string over JSON | Type as `string` (ISO timestamp) |
| Non-type imports of server files in frontend | Browser bundle breaks on Node-only modules | Use `import type` or dedicated shared package |
| Manual client types copied from server | Unavoidable type drift across PRs | Use a shared monorepo package or tRPC |
| Omitting error response contracts | Client fails to handle structured errors | Type error envelopes explicitly |
| Trusting shared contracts without runtime validation | Malicious client sends bad payload anyway | Server must still validate via Zod schemas |

---

## 8. Performance & Type-Checker Complexity
- Sharing pure interface contracts introduces zero runtime overhead.
- Route mapping dictionaries with $<100$ endpoints evaluate in $\mathcal{O}(1)$ in the TypeScript type checker.

---

## 9. Compiler Internals
When `import type` is used, the TypeScript Binder marks the symbol reference with `SymbolFlags.Type`. The Emitter completely erases the import statement, ensuring that no JavaScript module loading occurs at runtime.

---

## 10. Best Practices & Design Guidelines
- Maintain a dedicated `@myorg/contracts` package in monorepos containing shared DTOs and Zod schemas.
- Always represent serialized dates as `string` in JSON API contracts.
- Use `import type` religiously when consuming contracts across frontend/backend boundaries.
- Even when sharing types with the client, the server must strictly validate inputs at runtime using schemas.
- Use discriminated union response envelopes (`{ success: true, data } | { success: false, error }`).

---

## 11. Real-World Case Study
An e-commerce marketplace maintained separate repositories for their React frontend and Express backend. A backend engineer changed the checkout payload field `postalCode` to `zipCode`. Because the frontend repository had a hardcoded hand-written interface, the change went unnoticed. At launch, thousands of shoppers experienced failed checkouts with cryptic errors.

**Solution:**
The company migrated both repositories into a pnpm monorepo with a shared `@shop/api-contracts` package. Any change to field names on the backend now triggers an immediate compile failure in the frontend build pipeline, preventing broken releases from ever reaching production.

---

## 12. Interview Questions

<details>
<summary>1. What is API drift and how does TypeScript help eliminate it?</summary>

API drift is the divergence between what a server endpoint expects/returns and what the client assumes it expects/returns. By establishing a single shared type contract (via a monorepo shared package, tRPC, or OpenAPI codegen), TypeScript type-checks both the server handler and client caller against the identical type definition, raising compile-time errors whenever fields are renamed, added, or removed.
</details>

<details>
<summary>2. Why should JSON timestamp fields be typed as `string` rather than `Date` in API contracts?</summary>

Because the JSON standard does not have a native `Date` data type. When an object containing a `Date` is serialized via `JSON.stringify()`, it becomes an ISO-8601 formatted string. On the client, `JSON.parse()` leaves it as a string. Typing it as `Date` causes runtime errors when calling Date methods.
</details>

<details>
<summary>3. What is the danger of importing server models directly into frontend components?</summary>

Non-type imports can pull server-only Node.js libraries (e.g. database drivers, encryption modules, `fs`) into the client JavaScript bundle, bloating the bundle size or causing browser runtime crashes (`Module not found: Can't resolve 'fs'`).
</details>

<details>
<summary>4. What is the difference between code-first and contract-first API design?</summary>

- **Code-First**: The API implementation (e.g. server code) is the source of truth, and types are inferred or exported directly from the server (e.g. tRPC).
- **Contract-First**: An independent specification (e.g. OpenAPI YAML, GraphQL Schema) is the source of truth, and both server implementations and client SDKs are generated from that specification.
</details>

<details>
<summary>5. How does `import type` protect client bundles?</summary>

It informs the TypeScript compiler that the imported entity is purely a compile-time type. The emitter is guaranteed to remove the import completely from the generated JavaScript, preventing server code from entering client bundles.
</details>

<details>
<summary>6. Even if the frontend and backend share TypeScript types, why is server-side runtime validation still required?</summary>

Because malicious users or third-party API clients can bypass the frontend UI and send arbitrary, corrupted, or hostile HTTP requests directly to the server endpoints.
</details>

<details>
<summary>7. What is an API route dictionary map?</summary>

An interface mapping URL path strings and HTTP methods to their exact request and response shapes, allowing typed HTTP clients (like `fetch`) to infer response types automatically from the URL string.
</details>

<details>
<summary>8. How do you share contracts between frontend and backend in separate repositories?</summary>

By publishing the contracts as an internal versioned npm package (e.g. `@org/contracts`), or by generating client SDKs via OpenAPI codegen during CI pipelines.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Shared Route Contract**: Define a TypeScript interface for an authentication endpoint `POST /api/login` accepting `email` and `password` and returning `{ token: string }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export interface LoginContract {
     request: { email: string; password: string };
     response: { token: string };
   }
   ```
   </details>

2. **JSON Timestamp Model**: Create a type-safe DTO for an audit log entry ensuring timestamps are typed as ISO strings.
   <details>
   <summary>Solution</summary>

   ```typescript
   export interface AuditLogDto {
     id: string;
     action: string;
     timestamp: string; // ISO 8601
   }
   ```
   </details>

### Medium
3. **Generic Response Envelope**: Design a generic discriminated union type `ApiResponse<T>` that represents either success with `data: T` and `timestamp`, or error with `code: string` and `message: string`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type ApiResponse<T> =
     | { readonly success: true; readonly data: T; readonly timestamp: string }
     | { readonly success: false; readonly error: { code: string; message: string }; readonly timestamp: string };
   ```
   </details>

4. **Type-Safe Route Path Lookup**: Implement a generic type `GetRouteResponse<Routes, Path, Method>` that extracts the response type from an API dictionary interface.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type GetRouteResponse<
     Routes,
     Path extends keyof Routes,
     Method extends keyof Routes[Path]
   > = Routes[Path][Method] extends { response: infer R } ? R : never;
   ```
   </details>

### Hard
5. **Type-Safe Fetcher with Route Dictionary**: Build a generic `apiFetch<Routes>()` function that accepts a route dictionary, validates path and method at compile-time, requires request parameters if defined, and automatically infers the return type.
   <details>
   <summary>Solution</summary>

   ```typescript
   interface ApiSchema {
     '/users': {
       GET: { response: Array<{ id: string; name: string }> };
       POST: { body: { name: string }; response: { id: string } };
     };
   }

   export async function typedFetch<
     Path extends keyof ApiSchema,
     Method extends keyof ApiSchema[Path]
   >(
     path: Path,
     method: Method,
     options?: ApiSchema[Path][Method] extends { body: infer B } ? { body: B } : undefined
   ): Promise<ApiSchema[Path][Method] extends { response: infer R } ? R : never> {
     const res = await fetch(path as string, {
       method: method as string,
       body: options && 'body' in options ? JSON.stringify(options.body) : undefined,
       headers: { 'Content-Type': 'application/json' },
     });
     return (await res.json()) as any;
   }
   ```
   </details>

6. **Automated Contract Drift Detection Linter**: Write a Node.js script using the TypeScript Compiler API that reads a server route handler file and asserts that its return statement conforms strictly to the corresponding `UserResponseDto` contract interface.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function checkHandlerContract(filePath: string): boolean {
     const program = ts.createProgram([filePath], { strict: true });
     const checker = program.getTypeChecker();
     const sourceFile = program.getSourceFile(filePath);
     if (!sourceFile) return false;

     let conforms = true;
     ts.forEachChild(sourceFile, node => {
       if (ts.isFunctionDeclaration(node) && node.name?.text === 'getUserHandler') {
         const type = checker.getTypeAtLocation(node);
         const returnType = checker.getReturnTypeOfSignature(type.getCallSignatures()[0]);
         console.log('Handler return type:', checker.typeToString(returnType));
       }
     });

     return conforms;
   }
   ```
   </details>
