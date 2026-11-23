# Typed Fetch and Type-Safe API Client Architecture in TypeScript

> Designing end-to-end type-safe HTTP clients with TypeScript: route-level type inference, template literal path interpolation, request/response inference, interceptors, and error unwrapping.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type-Safe APIs |
| **Difficulty** | Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [10-advanced-types/04-template-literal-types.md](../10-advanced-types/04-template-literal-types.md), [16-type-safe-apis/00-api-contracts.md](./00-api-contracts.md) |
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

Standard HTTP client usage with the native Fetch API or Axios is fraught with invisible type errors:
```typescript
const res = await fetch('/api/users/123');
const data = await res.json(); // data is any!
```
When `data` is typed as `any`, TypeScript disables all compiler safety checks. A typo like `data.usrname` goes undetected during compilation and crashes at runtime in production.

A **Type-Safe API Client** bridges client and server contracts without code generation runtime bloat. It provides:
1. **Route Autocompletion**: The IDE autocompletes paths like `/api/v1/users/:id`.
2. **Path Parameter Inference**: Extracting `:id` from the URL string via template literal types and forcing the caller to supply `{ params: { id: string } }`.
3. **Query and Body Enforcement**: Enforcing required query parameters and JSON body payloads based on the matched endpoint schema.
4. **Automatic Response Unwrapping**: Returning strictly typed DTOs and strongly typed RFC 9457 errors.

```
client.get('/users/:id', { params: { id: '42' } })
           |
   TypeScript Parser:
   Path matches: '/users/:id'
   Params required: { id: string }
   Response type inferred: Promise<UserDTO>
```

---

## 2. Core Concepts

### 1. Route Map Specification
An interface mapping HTTP methods and paths to request and response shapes:
```typescript
export interface ApiSchema {
  'GET /users': {
    query: { role?: 'admin' | 'user'; limit?: number };
    response: { users: { id: string; name: string }[] };
  };
  'GET /users/:id': {
    params: { id: string };
    response: { id: string; name: string; email: string };
  };
  'POST /users': {
    body: { name: string; email: string };
    response: { id: string; createdAt: string };
  };
}
```

### 2. Path Parameter Parsing via Template Literal Types
TypeScript can parse colon parameters directly from a URL string at compile time:
```typescript
export type ExtractRouteParams<T extends string> =
  T extends `${string}:${infer Param}/${infer Rest}`
    ? { [K in Param | keyof ExtractRouteParams<`/${Rest}`>]: string }
    : T extends `${string}:${infer Param}`
    ? { [K in Param]: string }
    : {};
```

---

## 3. How It Works Internally

The API client uses generic method signatures indexed by the endpoint route key:

```
  Client Invocation: client.get('/users/:id', { params: { id: '10' } })
                              |
    1. Generic TPath constrained to keyof ApiRoutes['GET']
    2. ExtractRouteParams<TPath> -> { id: string }
    3. Type checker validates options argument matches { params: { id: string } }
    4. Return type resolved to Promise<ApiRoutes['GET'][TPath]['response']>
```

At runtime, the client interpolates `:id` with the passed parameter value, appends query parameters, executes `fetch`, and unwraps the typed JSON payload.

---

## 4. Syntax & API Reference

### Complete Type-Safe Fetch Client Interface
```typescript
export interface ClientOptions {
  baseUrl: string;
  headers?: Record<string, string>;
  fetch?: typeof fetch;
}

export type HttpMethod = 'GET' | 'POST' | 'PUT' | 'PATCH' | 'DELETE';

export interface EndpointDef {
  params?: Record<string, string>;
  query?: Record<string, string | number | boolean | undefined>;
  body?: unknown;
  response: unknown;
}
```

---

## 5. Practical Examples

### Example 1: Basic Path Parameter Extractor
```typescript
type RouteParams<T extends string> = T extends `${string}:${infer P}/${infer R}`
  ? { [K in P | keyof RouteParams<`/${R}`>]: string }
  : T extends `${string}:${infer P}`
  ? { [K in P]: string }
  : Record<string, never>;

type UserProfileParams = RouteParams<'/orgs/:orgId/users/:userId/posts/:postId'>;
// Inferred: { orgId: string; userId: string; postId: string }
```

### Example 2: Real-World Strongly-Typed API Client
```typescript
export class TypedHttpClient<TSchema extends Record<string, EndpointDef>> {
  constructor(private readonly baseUrl: string) {}

  public async request<TKey extends keyof TSchema & string>(
    endpoint: TKey,
    config: (TSchema[TKey]['params'] extends object ? { params: TSchema[TKey]['params'] } : { params?: never }) &
      (TSchema[TKey]['query'] extends object ? { query: TSchema[TKey]['query'] } : { query?: never }) &
      (TSchema[TKey]['body'] extends object ? { body: TSchema[TKey]['body'] } : { body?: never })
  ): Promise<TSchema[TKey]['response']> {
    const [method, path] = endpoint.split(' ') as [string, string];
    let resolvedPath = path;

    const anyConfig = config as any;
    if (anyConfig.params) {
      for (const [key, val] of Object.entries(anyConfig.params)) {
        resolvedPath = resolvedPath.replace(`:${key}`, encodeURIComponent(String(val)));
      }
    }

    const url = new URL(resolvedPath, this.baseUrl);
    if (anyConfig.query) {
      for (const [key, val] of Object.entries(anyConfig.query)) {
        if (val !== undefined) url.searchParams.set(key, String(val));
      }
    }

    const res = await fetch(url.toString(), {
      method,
      headers: anyConfig.body ? { 'Content-Type': 'application/json' } : undefined,
      body: anyConfig.body ? JSON.stringify(anyConfig.body) : undefined
    });

    if (!res.ok) {
      throw new Error(`API error: ${res.status} ${res.statusText}`);
    }

    return res.json() as Promise<TSchema[TKey]['response']>;
  }
}
```

### Example 3: Edge Case - Query Parameter Serialization with Arrays
```typescript
export function serializeQuery(query: Record<string, unknown>): string {
  const params = new URLSearchParams();
  for (const [key, value] of Object.entries(query)) {
    if (value === undefined || value === null) continue;
    if (Array.isArray(value)) {
      for (const item of value) {
        params.append(`${key}[]`, String(item));
      }
    } else {
      params.set(key, String(value));
    }
  }
  return params.toString();
}
```

### Example 4: Failure Case - Missing Required Route Parameters
```typescript
type MyEndpoints = {
  'GET /users/:id': {
    params: { id: string };
    response: { id: string; name: string };
  };
};

const client = new TypedHttpClient<MyEndpoints>('https://api.example.com');

// @ts-expect-error TS2345: Property 'params' is missing in type '{}' but required in type '{ params: { id: string; }; }'.
client.request('GET /users/:id', {});
```

### Example 5: Idiomatic Interceptor Pipeline (Auth & Refresh)
```typescript
export type RequestInterceptor = (req: Request) => Promise<Request> | Request;
export type ResponseInterceptor = (res: Response) => Promise<Response> | Response;

export class ApiInterceptorManager {
  private requestInterceptors: RequestInterceptor[] = [];
  private responseInterceptors: ResponseInterceptor[] = [];

  public useRequest(interceptor: RequestInterceptor) {
    this.requestInterceptors.push(interceptor);
  }

  public useResponse(interceptor: ResponseInterceptor) {
    this.responseInterceptors.push(interceptor);
  }

  public async execute(req: Request): Promise<Response> {
    let currentReq = req;
    for (const interceptor of this.requestInterceptors) {
      currentReq = await interceptor(currentReq);
    }

    let res = await fetch(currentReq);

    for (const interceptor of this.responseInterceptors) {
      res = await interceptor(res);
    }
    return res;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **URL Encoding**: Path parameters with special characters (`/`, `?`, `#`, spaces) will break route resolution unless wrapped in `encodeURIComponent`.
2. **Missing Leading Slash**: Instantiating `new URL(path, baseUrl)` when `baseUrl` lacks a trailing slash and `path` lacks a leading slash can cause path truncation (e.g. `https://api.com/v1` + `users` -> `https://api.com/users` instead of `https://api.com/v1/users`).
3. **Double Serialization**: Passing a JSON string to `body` when the client automatically runs `JSON.stringify(body)` leads to escaped quotes (`"\"{\\\"name\\\":\\\"Alice\\\"}\""`).
4. **Connection Hang on Node.js**: Failing to read `res.body` or cancel it with `res.body.cancel()` on unhandled error responses in Node.js can cause connection pool leaks.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Direct Any Cast** | `(await res.json()) as User` | Schema parse with Zod / TypeBox | Bypasses all runtime verification |
| **Manual String Concatenation** | `fetch(url + '/users/' + id)` | Strongly typed template literal client | Susceptible to path traversal and encoding bugs |
| **Ignoring AbortSignal** | Neglecting `signal?: AbortSignal` | Pass `signal` to fetch options | Memory leaks and race conditions on unmounted UI |
| **Ignoring HTTP 4xx/5xx** | `const data = await (await fetch(url)).json()` | Inspect `res.ok` before parsing | Throws opaque syntax errors or sets state to error JSON |
| **Stringified Numbers in Query** | Passing unparsed strings | Type query params strictly and serialize | Server receives `"NaN"` or unexpected type |
| **Hardcoded Bearer Tokens** | Storing token directly in class property | Supplier callback `() => getFreshToken()` | Fails after JWT token expiration |

---

## 8. Performance & Type-Checker Complexity

- **Template Literal String Splitting**: Recursive conditional types parsing route parameters have a depth limit of ~50 segment recursions in TypeScript. Real-world routes rarely exceed 6 segments, making compilation virtually instantaneous (<10ms).
- **Bundle Footprint**: A lightweight typed client written using native Fetch requires zero external dependencies and compiles down to under 1.5 KB minified.

---

## 9. Compiler Internals

When extracting route parameters:
```typescript
type Extract<T> = T extends `${string}:${infer P}/${infer R}` ? ... : ...;
```
The TypeScript compiler's scanner parses the string literal type by matching string prefixes. If the pattern matches, it introduces a new inference variable `P` in the symbol table, associating it with the substring up to the next `/`.

---

## 10. Best Practices & Design Guidelines

1. **Provide Autocompletion for All Endpoints**: Structure method calls as `client.get('/users/:id', ...)` to trigger instant IDE path suggestions.
2. **Support AbortSignal by Default**: Always accept an optional `signal` in client request options.
3. **Decouple Transport from Schema**: Write the HTTP engine once, feeding it different schemas generated from OpenAPI or tRPC.
4. **Implement Automatic Token Refresh**: Use response interceptors to handle 401 Unauthorized by executing a token refresh and retrying the request.

---

## 11. Real-World Case Study

### High-Reliability Enterprise SaaS SDK
An enterprise SDK provides customers with a type-safe client that retries on 429 rate limits and 503 transient gateway errors:

```typescript
export interface SaaSClientConfig {
  apiKey: string;
  maxRetries?: number;
}

export class EnterpriseApiClient {
  private maxRetries: number;

  constructor(private config: SaaSClientConfig) {
    this.maxRetries = config.maxRetries ?? 3;
  }

  public async fetchWithRetry(url: string, init?: RequestInit): Promise<Response> {
    let attempt = 0;
    while (attempt < this.maxRetries) {
      try {
        const response = await fetch(url, {
          ...init,
          headers: {
            ...init?.headers,
            'Authorization': `Bearer ${this.config.apiKey}`
          }
        });

        if (response.status === 429 || response.status >= 500) {
          const retryAfter = Number(response.headers.get('Retry-After')) || Math.pow(2, attempt);
          await new Promise((r) => setTimeout(r, retryAfter * 1000));
          attempt++;
          continue;
        }

        return response;
      } catch (err) {
        attempt++;
        if (attempt >= this.maxRetries) throw err;
      }
    }
    throw new Error('Max retries exceeded');
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. How do TypeScript template literal types enable type-safe URL routing?</summary>

TypeScript 4.1 introduced template literal types with pattern matching. By writing a recursive conditional type `T extends \`${string}:${infer Param}/${infer Rest}\``, TypeScript can extract parameter identifiers from the URL string at compile time and require them as keys in a `params` object.
</details>

<details>
<summary>2. Why is `(await res.json()) as User` considered dangerous in TypeScript?</summary>

Type assertions (`as Type`) bypass TypeScript's type checker without performing any runtime validation. If the backend changes its response format, returns a 500 error page, or omits a required property, the client code still assumes the object is a valid `User`, leading to runtime crashes like `Cannot read property 'id' of undefined`.
</details>

<details>
<summary>3. How do you implement request and response interceptors with native Fetch?</summary>

By wrapping `fetch` in an execution pipeline: pass the `Request` through an array of asynchronous request transforms before invoking `fetch`, then pass the resulting `Response` through an array of response transforms before returning it to the caller.
</details>

<details>
<summary>4. What is the proper way to handle `AbortSignal` in a type-safe API client?</summary>

Accept `signal?: AbortSignal` in the client method's options, forward it directly to `fetch(url, { ...options, signal })`, and catch `DOMException` with `name === 'AbortError'` if graceful cancellation handling is required.
</details>

<details>
<summary>5. How can you infer query parameters from an endpoint schema?</summary>

By defining the schema with an optional `query` property:
```typescript
type EndpointConfig<T> = T extends { query: infer Q } ? { query: Q } : { query?: never };
```
If the schema defines `query`, the client caller is required to supply it; otherwise, it is disallowed.
</details>

<details>
<summary>6. How do you handle file uploads in a type-safe client?</summary>

Use `FormData` or typed Blob bodies:
```typescript
interface UploadEndpoint {
  body: FormData;
  response: { fileUrl: string };
}
```
When `body instanceof FormData`, omit the `'Content-Type'` header so the browser automatically sets `multipart/form-data` with the correct boundary parameter.
</details>

<details>
<summary>7. How does OpenAPI TypeScript generate types for typed fetch clients?</summary>

Tools like `openapi-typescript` parse an `openapi.yaml` file into a TypeScript AST containing a global `paths` interface. A generic client wrapper (like `openapi-fetch`) indexes this interface to infer parameters, request bodies, and responses for every path.
</details>

<details>
<summary>8. How do you type an API client method that can return different status codes (e.g. 200 vs 201 vs 404)?</summary>

Model responses as a status-keyed map:
```typescript
type Responses = {
  200: UserDTO;
  404: NotFoundProblem;
};
```
The client method can return a discriminated union `{ status: 200; data: UserDTO } | { status: 404; data: NotFoundProblem }`.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Query String Serializer</summary>

**Problem:** Implement a function `toQueryString(params: Record<string, string | number | boolean | undefined>): string` that produces a URL query string without undefined values.

**Solution:**
```typescript
export function toQueryString(params: Record<string, string | number | boolean | undefined>): string {
  const usp = new URLSearchParams();
  for (const [k, v] of Object.entries(params)) {
    if (v !== undefined) {
      usp.set(k, String(v));
    }
  }
  const s = usp.toString();
  return s ? `?${s}` : '';
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Typed Header Builder</summary>

**Problem:** Create a function that merges default headers (`Content-Type`, `Accept`) with custom headers while guaranteeing headers are normalized to lowercase strings.

**Solution:**
```typescript
export function buildHeaders(custom?: Record<string, string>): Headers {
  const headers = new Headers({
    'content-type': 'application/json',
    'accept': 'application/json'
  });
  if (custom) {
    for (const [k, v] of Object.entries(custom)) {
      headers.set(k.toLowerCase(), v);
    }
  }
  return headers;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Route Parameter Parser</summary>

**Problem:** Write a TypeScript utility type `ParseParams<T>` that parses `:param` segments from a path string into an object type where all param values are strings.

**Solution:**
```typescript
export type ParseParams<T extends string> =
  T extends `${string}:${infer P}/${infer R}`
    ? { [K in P | keyof ParseParams<`/${R}`>]: string }
    : T extends `${string}:${infer P}`
    ? { [K in P]: string }
    : Record<string, never>;
```
</details>

<details>
<summary>Exercise 4 (Medium): Path Interpolation Function</summary>

**Problem:** Implement a function `interpolatePath(path: string, params: Record<string, string>): string` that replaces `:param` tokens with values and throws an error if any parameter is missing.

**Solution:**
```typescript
export function interpolatePath(path: string, params: Record<string, string>): string {
  return path.replace(/:([a-zA-Z0-9_]+)/g, (_, key) => {
    if (!(key in params)) {
      throw new Error(`Missing route parameter: ${key}`);
    }
    return encodeURIComponent(params[key]);
  });
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Type-Safe Client Method Generator</summary>

**Problem:** Build a proxy-based API client where invoking `client.users.get({ id: '123' })` constructs a `GET /users/123` request with full typing.

**Solution:**
```typescript
type ApiResource = {
  get: (params: { id: string }) => Promise<unknown>;
};

export function createResourceClient(baseUrl: string): Record<string, ApiResource> {
  return new Proxy({}, {
    get: (_, resourceName: string) => ({
      get: async (params: { id: string }) => {
        const res = await fetch(`${baseUrl}/${resourceName}/${encodeURIComponent(params.id)}`);
        return res.json();
      }
    })
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Fully-Typed Endpoint Registry Client</summary>

**Problem:** Create a type-safe client class that accepts an API schema interface and provides strongly typed `.get()` and `.post()` methods enforcing path params, query parameters, and response types.

**Solution:**
```typescript
type Schema = Record<string, { params?: any; query?: any; body?: any; response: any }>;

export class ApiRegistryClient<TSchema extends Schema> {
  constructor(private baseUrl: string) {}

  public async get<TPath extends keyof TSchema & string>(
    path: TPath,
    options?: {
      params?: TSchema[TPath]['params'];
      query?: TSchema[TPath]['query'];
    }
  ): Promise<TSchema[TPath]['response']> {
    let urlStr = `${this.baseUrl}${path}`;
    if (options?.params) {
      for (const [k, v] of Object.entries(options.params)) {
        urlStr = urlStr.replace(`:${k}`, encodeURIComponent(String(v)));
      }
    }
    const url = new URL(urlStr);
    if (options?.query) {
      for (const [k, v] of Object.entries(options.query)) {
        if (v !== undefined) url.searchParams.set(k, String(v));
      }
    }
    const res = await fetch(url.toString());
    return res.json();
  }
}
```
</details>
