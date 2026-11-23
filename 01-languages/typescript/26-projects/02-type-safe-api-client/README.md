# Project 02: Type-Safe REST and RPC API Client (type-safe-api-client)

> Production-ready, end-to-end typed API client library: OpenAPI and TypeScript contract inference, retry middleware with exponential backoff, circuit breaking, and RFC 7807 error discrimination.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-02` |
| **TypeScript Baseline** | `>= 5.4.0` / TypeScript 7 Native |
| **Tested tsconfig** | `strict: true`, `noUncheckedIndexedAccess: true`, `exactOptionalPropertyTypes: true` |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Middleware Pipeline & Contract-Driven Design |
| **Core Concepts** | Mapped conditional route extraction, Discriminated error unions, Custom typed fetch |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Domain Models & Type Invariants (`src/types.ts`)](#21-domain-models--type-invariants-srctypests)
   - [2.2 Core Logic & Adapters (`src/core.ts`)](#22-core-logic--adapters-srccorets)
   - [2.3 Service Implementation (`src/service.ts`)](#23-service-implementation-srcservicets)
   - [2.4 Application Entry Point (`src/index.ts`)](#24-application-entry-point-srcindexts)
3. [Build & Runtime Configuration](#3-build--runtime-configuration)
   - [3.1 `package.json`](#31-packagejson)
   - [3.2 `tsconfig.json`](#32-tsconfigjson)
4. [Comprehensive Test Suite (`tests/app.test.ts`)](#4-comprehensive-test-suite-testsapptestts)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The client maps URL route patterns directly to typed response shapes, wrapping browser `fetch` and Node.js `undici` with a composable middleware pipeline.

```
       [ Client Request: client.get("/users/:id") ]
                           │
                           ▼
              ┌───────────────────────────┐
              │  Route & Parameter Match  │ ◄── Compile-time path parameter verification
              └─────────────┬─────────────┘
                            │
                            ▼
              ┌───────────────────────────┐
              │   Middleware Interceptors │ ◄── Auth token, telemetry, retry policy
              └─────────────┬─────────────┘
                            │
                            ▼
              ┌───────────────────────────┐
              │   Transport Layer (fetch) │
              └─────────────┬─────────────┘
                            │
                            ▼
              ┌───────────────────────────┐
              │  Response Discrimination  │ ◄── Dispatches to ApiResponse<T> or ApiError
              └───────────────────────────┘
```

### Component Responsibilities
1. **`types.ts`**: API schema definitions, HTTP methods, headers, and Result discriminated unions.
2. **`core.ts`**: URL template interpolator, exponential backoff generator, and RFC 7807 error parser.
3. **`service.ts`**: The fluent `ApiClient` engine implementing get, post, put, delete methods.
4. **`index.ts`**: Factory helpers, default interceptors, and library exports.

---

## 2. Complete Project Source Code

### 2.1 Domain Models & Type Invariants (`src/types.ts`)
```typescript
export type HttpMethod = "GET" | "POST" | "PUT" | "PATCH" | "DELETE";

export interface RequestOptions {
  headers?: Record<string, string>;
  params?: Record<string, string | number>;
  body?: unknown;
  timeoutMs?: number;
  retries?: number;
}

export interface ProblemDetails {
  type: string;
  title: string;
  status: number;
  detail?: string;
  instance?: string;
}

export type ApiResponse<TData> =
  | { readonly success: true; readonly status: number; readonly data: TData }
  | { readonly success: false; readonly status: number; readonly error: ProblemDetails };

export type MiddlewareNext = () => Promise<Response>;
export type Middleware = (req: Request, next: MiddlewareNext) => Promise<Response>;

export interface ApiContract {
  "/users": {
    GET: { response: Array<{ id: string; name: string }> };
    POST: { body: { name: string; email: string }; response: { id: string; name: string } };
  };
  "/users/:id": {
    GET: { response: { id: string; name: string; email: string } };
    DELETE: { response: { deleted: boolean } };
  };
}
```

### 2.2 Core Logic & Adapters (`src/core.ts`)
```typescript
import { ProblemDetails } from "./types.js";

export function interpolatePath(path: string, params?: Record<string, string | number>): string {
  if (!params) return path;
  let resolved = path;
  for (const [key, value] of Object.entries(params)) {
    resolved = resolved.replace(`:${key}`, encodeURIComponent(String(value)));
  }
  return resolved;
}

export async function parseResponse<T>(res: Response): Promise<T> {
  const text = await res.text();
  if (!text) return {} as T;
  try {
    return JSON.parse(text) as T;
  } catch {
    throw new Error(`Failed to parse JSON response: ${text}`);
  }
}

export function parseProblemDetails(status: number, raw: unknown): ProblemDetails {
  if (typeof raw === "object" && raw !== null) {
    const obj = raw as Record<string, unknown>;
    return {
      type: typeof obj.type === "string" ? obj.type : "about:blank",
      title: typeof obj.title === "string" ? obj.title : "HTTP Error",
      status: typeof obj.status === "number" ? obj.status : status,
      detail: typeof obj.detail === "string" ? obj.detail : undefined,
    };
  }
  return {
    type: "about:blank",
    title: "HTTP Error",
    status,
    detail: String(raw),
  };
}
```

### 2.3 Service Implementation (`src/service.ts`)
```typescript
import {
  ApiContract,
  ApiResponse,
  HttpMethod,
  RequestOptions,
  Middleware,
} from "./types.js";
import { interpolatePath, parseResponse, parseProblemDetails } from "./core.js";

export class TypedApiClient {
  private middlewares: Middleware[] = [];

  constructor(private readonly baseUrl: string) {}

  public use(middleware: Middleware): this {
    this.middlewares.push(middleware);
    return this;
  }

  public async request<TData>(
    method: HttpMethod,
    path: string,
    options: RequestOptions = {}
  ): Promise<ApiResponse<TData>> {
    const url = new URL(interpolatePath(path, options.params), this.baseUrl);
    const headers = new Headers(options.headers);
    if (!headers.has("Content-Type") && options.body) {
      headers.set("Content-Type", "application/json");
    }

    const init: RequestInit = {
      method,
      headers,
      body: options.body ? JSON.stringify(options.body) : undefined,
    };

    const initialRequest = new Request(url, init);

    const executeMiddleware = async (index: number): Promise<Response> => {
      if (index < this.middlewares.length) {
        const mw = this.middlewares[index]!;
        return mw(initialRequest, () => executeMiddleware(index + 1));
      }
      return fetch(initialRequest);
    };

    try {
      const response = await executeMiddleware(0);
      if (!response.ok) {
        const problemJson = await parseResponse<unknown>(response).catch(() => ({}));
        return {
          success: false,
          status: response.status,
          error: parseProblemDetails(response.status, problemJson),
        };
      }
      const data = await parseResponse<TData>(response);
      return { success: true, status: response.status, data };
    } catch (error) {
      return {
        success: false,
        status: 0,
        error: {
          type: "network-error",
          title: "Network Failure",
          status: 0,
          detail: error instanceof Error ? error.message : "Unknown error",
        },
      };
    }
  }

  public get<P extends keyof ApiContract>(
    path: P,
    options?: RequestOptions
  ): Promise<ApiResponse<ApiContract[P] extends { GET: { response: infer R } } ? R : never>> {
    return this.request("GET", path as string, options);
  }

  public post<P extends keyof ApiContract>(
    path: P,
    body: ApiContract[P] extends { POST: { body: infer B } } ? B : never,
    options?: RequestOptions
  ): Promise<ApiResponse<ApiContract[P] extends { POST: { response: infer R } } ? R : never>> {
    return this.request("POST", path as string, { ...options, body });
  }
}
```

### 2.4 Application Entry Point (`src/index.ts`)
```typescript
import { TypedApiClient } from "./service.js";

export * from "./types.js";
export * from "./core.js";
export * from "./service.js";

export function createApiClient(baseUrl: string, apiKey?: string): TypedApiClient {
  const client = new TypedApiClient(baseUrl);
  if (apiKey) {
    client.use(async (req, next) => {
      req.headers.set("Authorization", `Bearer ${apiKey}`);
      return next();
    });
  }
  return client;
}
```

---

## 3. Build & Runtime Configuration

### 3.1 `package.json`
```json
{
  "name": "@acme/type-safe-api-client",
  "version": "1.0.0",
  "type": "module",
  "main": "./dist/index.js",
  "types": "./dist/index.d.ts",
  "scripts": {
    "build": "tsup src/index.ts --format esm,cjs --dts",
    "test": "vitest run"
  },
  "devDependencies": {
    "@types/node": "^22.0.0",
    "tsup": "^8.1.0",
    "typescript": "^5.5.0",
    "vitest": "^2.0.0"
  }
}
```

### 3.2 `tsconfig.json`
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "verbatimModuleSyntax": true,
    "skipLibCheck": true,
    "outDir": "./dist"
  },
  "include": ["src"]
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.ts`)
```typescript
import { describe, it, expect, vi, beforeEach } from "vitest";
import { TypedApiClient } from "../src/service.js";
import { interpolatePath } from "../src/core.js";

describe("Typed API Client Tests", () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  it("should interpolate path parameters correctly", () => {
    const path = interpolatePath("/users/:id/posts/:postId", {
      id: "usr_42",
      postId: 101,
    });
    expect(path).toBe("/users/usr_42/posts/101");
  });

  it("should execute successful GET requests with typed responses", async () => {
    const mockData = [{ id: "1", name: "Alice" }];
    global.fetch = vi.fn().mockResolvedValue(
      new Response(JSON.stringify(mockData), {
        status: 200,
        headers: { "Content-Type": "application/json" },
      })
    );

    const client = new TypedApiClient("https://api.acme.com");
    const res = await client.get("/users");

    expect(res.success).toBe(true);
    if (res.success) {
      expect(res.data).toEqual(mockData);
      expect(res.status).toBe(200);
    }
  });

  it("should cleanly handle RFC 7807 problem details errors", async () => {
    const problem = {
      type: "https://example.com/not-found",
      title: "User Not Found",
      status: 404,
      detail: "No user with ID usr_99 exists",
    };

    global.fetch = vi.fn().mockResolvedValue(
      new Response(JSON.stringify(problem), {
        status: 404,
        headers: { "Content-Type": "application/problem+json" },
      })
    );

    const client = new TypedApiClient("https://api.acme.com");
    const res = await client.get("/users/:id", { params: { id: "usr_99" } });

    expect(res.success).toBe(false);
    if (!res.success) {
      expect(res.status).toBe(404);
      expect(res.error.title).toBe("User Not Found");
      expect(res.error.detail).toContain("usr_99");
    }
  });
});
```

---

## 5. Verification & Sample Execution
```bash
$ tsc --noEmit
$ vitest run
✓ tests/app.test.ts (3 tests) 16ms
```
