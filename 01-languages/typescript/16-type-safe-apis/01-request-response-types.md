# Request and Response Types in TypeScript

> Structuring robust HTTP contracts: typing query strings, route parameters, headers, discriminated response envelopes, and REST verb mappings in TypeScript.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Intermediate |
| **Prerequisites** | [16-type-safe-apis/00-api-contracts.md](./00-api-contracts.md), [01-fundamentals/03-objects.md](../01-fundamentals/03-objects.md) |
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
An HTTP transaction consists of four primary data channels:
1. **Route/Path Parameters** (e.g. `/users/:userId`)
2. **Query String Parameters** (e.g. `?page=1&sort=desc`)
3. **Headers** (e.g. `Authorization`, `X-Api-Key`)
4. **Request & Response Body** (JSON payloads)

In loosely typed codebases, developers often only type the request and response body, leaving URL parameters, headers, and query strings as `any` or un-annotated. This causes subtle routing errors, casing mismatches, and unhandled pagination edge cases. Strongly typing the full request envelope guarantees that callers supply all required parameters and correctly handle both success and error responses.

---

## 2. Core Concepts
- **Route Parameters**: Segments of the URL path (`/tenants/:tenantId/orders/:orderId`). In TypeScript, template literal types can extract route parameters automatically:
  ```typescript
  type ExtractParams<Path extends string> = ...
  ```
- **Query Parameters**: Key-value pairs in the query string. Values in raw Node.js/Express are parsed as `string | string[] | undefined`.
- **Discriminated Response Envelopes**: Wrapping responses in a standard structure `{ success: true, data: T } | { success: false, error: ApiError }` to force callers to check status before reading payload data.
- **HTTP Status Code Mapping**: Mapping specific HTTP status codes (200, 201, 400, 401, 404, 500) to distinct response payload types.

---

## 3. How It Works Internally

```
                      Client Request Envelope
  +-------------------------------------------------------------+
  | Path: "/users/:id"      -> Params: { id: string }           |
  | Query: "?detailed=true" -> Query:  { detailed?: boolean }   |
  | Body:  { name: "Bob" }  -> Body:   CreateUserDto            |
  +------------------------------+------------------------------+
                                 |
                                 v
                     Backend Controller Execution
                                 |
                                 v
                     Server Response Envelope
  +-------------------------------------------------------------+
  | Success: { ok: true,  status: 200, data: UserRecord }       |
  | Failure: { ok: false, status: 404, error: "User Not Found" }|
  +-------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

```typescript
// 1. Discriminated Response Envelope
export type ApiResponse<TData, TError = ApiErrorDto> =
  | { readonly success: true; readonly data: TData; readonly meta?: ResponseMeta }
  | { readonly success: false; readonly error: TError };

export interface ApiErrorDto {
  code: string;
  message: string;
  details?: Record<string, string[]>;
}

export interface ResponseMeta {
  page?: number;
  total?: number;
  timestamp: string;
}

// 2. Comprehensive Endpoint Contract
export interface Endpoint<TParams, TQuery, TBody, TResponse> {
  params: TParams;
  query: TQuery;
  body: TBody;
  response: ApiResponse<TResponse>;
}
```

---

## 5. Practical Examples

### Example 1: Template Literal Route Parameter Extraction
```typescript
type ExtractRouteParams<Path extends string> =
  Path extends `${string}:${infer Param}/${infer Rest}`
    ? { [K in Param | keyof ExtractRouteParams<`/${Rest}`>]: string }
    : Path extends `${string}:${infer Param}`
      ? { [K in Param]: string }
      : Record<string, never>;

type OrderRoute = ExtractRouteParams<'/tenants/:tenantId/orders/:orderId'>;
// Inferred as: { tenantId: string; orderId: string }
```

### Example 2: Strongly-Typed Express Route Definition
```typescript
import { Request, Response } from 'express';

interface GetOrderParams {
  orderId: string;
}

interface GetOrderQuery {
  includeItems?: string;
}

interface OrderDto {
  id: string;
  totalCents: number;
}

export function getOrderHandler(
  req: Request<GetOrderParams, ApiResponse<OrderDto>, never, GetOrderQuery>,
  res: Response<ApiResponse<OrderDto>>
) {
  const { orderId } = req.params;
  const isDetailed = req.query.includeItems === 'true';

  if (orderId === 'not_found') {
    return res.status(404).json({
      success: false,
      error: { code: 'NOT_FOUND', message: `Order ${orderId} not found` },
    });
  }

  return res.status(200).json({
    success: true,
    data: { id: orderId, totalCents: 4500 },
    meta: { timestamp: new Date().toISOString() },
  });
}
```

### Example 3: Edge Case - Multi-Status HTTP Response Unions
```typescript
type UserResponse =
  | { status: 200; body: { id: string; name: string } }
  | { status: 201; body: { id: string; name: string; created: true } }
  | { status: 400; body: { error: 'VALIDATION_FAILED'; fields: string[] } }
  | { status: 404; body: { error: 'USER_NOT_FOUND' } };

function handleResponse(res: UserResponse) {
  if (res.status === 200 || res.status === 201) {
    console.log(`User: ${res.body.name}`);
  } else {
    console.error(`Failed with code ${res.status}: ${res.body.error}`);
  }
}
```

### Example 4: Compiler Failure Case - Unchecked Response Access
```typescript
declare const response: ApiResponse<{ id: string; balance: number }>;

// @ts-expect-error TS2339: Property 'data' does not exist on type 'ApiResponse<...>'.
console.log(response.data.balance);

// Safe Narrowing:
if (response.success) {
  console.log(response.data.balance); // Safe!
} else {
  console.error(response.error.message); // Safe!
}
```

### Example 5: Idiomatic Headers Contract
```typescript
export interface SecureRequestHeaders {
  readonly 'authorization': `Bearer ${string}`;
  readonly 'x-request-id': string;
  readonly 'x-client-version'?: string;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Query Params Array Traps**: In Express and Fastify, sending `?tag=a&tag=b` produces `{ tag: ['a', 'b'] }`, whereas `?tag=a` produces `{ tag: 'a' }`. If you expect multiple items, always normalize with `Array.isArray(x) ? x : [x]`.
2. **Headers are Case-Insensitive**: HTTP headers are case-insensitive according to RFC 7230, but JavaScript object key lookups are case-sensitive! Node.js normalizes all incoming headers to lowercase (`req.headers['content-type']`). Always type headers in lowercase.
3. **Empty Body on GET/HEAD**: Attempting to send a JSON body on `GET` or `HEAD` requests violates HTTP specs and causes some proxies to reject requests. Type `body: never` on GET routes.
4. **Status Codes Not Visible in JSON**: The HTTP status code in the HTTP packet header (e.g. 404) is distinct from fields in the JSON body (`{ success: false }`). Ensure client fetch wrappers inspect `res.status` or `res.ok`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Typing headers with uppercase letters (`Authorization`) | Fails to match Node's lowercase headers | Type in lowercase: `'authorization'` |
| Assuming query parameters are parsed as numbers | `req.query.limit` is string `"10"`, not number | Use `z.coerce.number()` or explicit parse |
| Returning flat objects without error flags | Client cannot easily differentiate success from error | Use discriminated `ApiResponse<T>` envelope |
| Typing GET route body as `any` | Accidental inclusion of request body | Annotate body as `never` |
| Reading `res.data` before checking `res.success` | **TS2339**: Property 'data' does not exist | Check `if (res.success)` first |

---

## 8. Performance & Type-Checker Complexity
- Template literal route parameter extractors evaluate recursively in $\mathcal{O}(S)$ where $S$ is the number of path segments (typically $\le 6$).
- Discriminated response envelopes evaluate in $\mathcal{O}(1)$ in control-flow narrowing.

---

## 9. Compiler Internals
In TypeScript's template literal parser:
- The recursive pattern `${string}:${infer Param}/${infer Rest}` parses path segments left-to-right.
- Once matched, the compiler binds the inferred string literal to the property name in a mapped type.

---

## 10. Best Practices & Design Guidelines
- Standardize on a single discriminated response envelope (`{ success: true, data } | { success: false, error }`) across all microservices.
- Always declare HTTP header contracts using lowercase keys.
- Use template literal types to extract path parameters from route strings.
- Normalize query string parameters using schema coercions.
- Include an `x-request-id` header in all request/response contracts for distributed tracing.

---

## 11. Real-World Case Study
A financial reporting platform had an endpoint `/reports/:reportId?format=pdf`. A developer assumed `format` was an optional string defaulting to `'pdf'`. The frontend called `/reports/123?format=csv&format=xlsx` (a multi-select filter). On the server, `req.query.format` was parsed as an array `['csv', 'xlsx']`. Calling `req.query.format.toLowerCase()` threw a runtime `TypeError: format.toLowerCase is not a function`, taking down report generation for 4 hours.

**Remediation:**
The team introduced strong query parameter schemas:
```typescript
const ReportQuerySchema = z.object({
  format: z.union([z.string(), z.array(z.string())]).transform(val =>
    Array.isArray(val) ? val[0] : val
  ).default('pdf'),
});
```
Query parameters were deterministically normalized, and similar crashes were permanently prevented.

---

## 12. Interview Questions

<details>
<summary>1. How do you extract route parameters from a URL string like `/users/:id` at compile time in TypeScript?</summary>

Using template literal types with conditional inference:
```typescript
type ExtractParams<Path extends string> =
  Path extends `${string}:${infer Param}/${infer Rest}`
    ? { [K in Param | keyof ExtractParams<`/${Rest}`>]: string }
    : Path extends `${string}:${infer Param}`
      ? { [K in Param]: string }
      : Record<string, never>;
```
</details>

<details>
<summary>2. Why are incoming HTTP headers always represented in lowercase in Node.js?</summary>

The HTTP specification specifies that header field names are case-insensitive. To make lookups predictable and prevent case-mismatch bugs, Node.js automatically converts all incoming request header keys to lowercase.
</details>

<details>
<summary>3. What is a discriminated response envelope?</summary>

A response type structured as a discriminated union (e.g. `{ success: true, data: T } | { success: false, error: E }`). Callers cannot access `.data` without first checking `if (response.success)`.
</details>

<details>
<summary>4. What type should be assigned to the body parameter of an HTTP GET endpoint?</summary>

`never` or `undefined`, because HTTP GET requests do not specify a request body according to RFC guidelines.
</details>

<details>
<summary>5. What can happen if an API returns multiple query parameters with the same name (`?id=1&id=2`)?</summary>

Most server parsers (Express, Fastify) parse them as an array of strings (`['1', '2']`) rather than a single string. If the code expects a string, calling string methods on the array will crash with a runtime `TypeError`.
</details>

<details>
<summary>6. How do you type an endpoint that returns different HTTP status codes with different payloads?</summary>

Using a discriminated union keyed on `status`:
```typescript
type Response =
  | { status: 200; body: SuccessData }
  | { status: 404; body: NotFoundError };
```
</details>

<details>
<summary>7. How should pagination metadata be structured in an API response?</summary>

In a separate `meta` property within the response envelope:
```typescript
{
  success: true,
  data: items,
  meta: { page: 1, limit: 20, total: 100 }
}
```
</details>

<details>
<summary>8. How do you enforce that a token header has the `Bearer <token>` format at compile time?</summary>

Using a template literal type:
```typescript
interface AuthHeader {
  authorization: `Bearer ${string}`;
}
```
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Bearer Header Type**: Define a type `BearerToken` that strictly matches strings starting with `"Bearer "`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type BearerToken = `Bearer ${string}`;
   ```
   </details>

2. **Success Envelope**: Create a generic helper function `makeSuccess<T>(data: T): { success: true; data: T }`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function makeSuccess<T>(data: T): { success: true; data: T } {
     return { success: true, data };
   }
   ```
   </details>

### Medium
3. **Template Literal Param Extractor**: Implement `ExtractId<Path extends string>` that extracts the parameter name from a path containing a single `:id` token.
   <details>
   <summary>Solution</summary>

   ```typescript
   export type ExtractId<Path extends string> =
     Path extends `${string}:${infer Id}/${string}` ? Id :
     Path extends `${string}:${infer Id}` ? Id :
     never;

   type Test = ExtractId<'/items/:itemId'>; // 'itemId'
   ```
   </details>

4. **Normalized Query Helper**: Write a function that accepts `string | string[] | undefined` and always returns a clean `string[]`.
   <details>
   <summary>Solution</summary>

   ```typescript
   export function normalizeQueryList(val: string | string[] | undefined): string[] {
     if (!val) return [];
     return Array.isArray(val) ? val : [val];
   }
   ```
   </details>

### Hard
5. **Full REST API Endpoint Router Type**: Author a generic router type `Router<Routes>` that maps path strings to handler functions, where each handler automatically receives strongly-typed `params` extracted from the path string.
   <details>
   <summary>Solution</summary>

   ```typescript
   type ExtractRouteParams<Path extends string> =
     Path extends `${string}:${infer Param}/${infer Rest}`
       ? { [K in Param | keyof ExtractRouteParams<`/${Rest}`>]: string }
       : Path extends `${string}:${infer Param}`
         ? { [K in Param]: string }
         : Record<string, never>;

   export interface TypedRouter {
     get<Path extends string, Res>(
       path: Path,
       handler: (params: ExtractRouteParams<Path>) => Promise<Res>
     ): void;
   }

   // Usage:
   declare const router: TypedRouter;
   router.get('/users/:userId/posts/:postId', async (params) => {
     // params is inferred as: { userId: string; postId: string }
     return { user: params.userId, post: params.postId };
   });
   ```
   </details>

6. **Type-Safe Fetcher with Status Discrimination**: Write a wrapper `fetchWithStatus<TSuccess, TError>(url: string)` that returns `{ status: 200, body: TSuccess } | { status: number, body: TError }`, correctly narrowing the body based on HTTP status.
   <details>
   <summary>Solution</summary>

   ```typescript
   export async function fetchWithStatus<TSuccess, TError>(url: string): Promise<
     | { readonly ok: true; readonly status: 200; readonly body: TSuccess }
     | { readonly ok: false; readonly status: number; readonly body: TError }
   > {
     const res = await fetch(url);
     const json = await res.json();
     if (res.ok) {
       return { ok: true, status: 200, body: json as TSuccess };
     }
     return { ok: false, status: res.status, body: json as TError };
   }
   ```
   </details>
