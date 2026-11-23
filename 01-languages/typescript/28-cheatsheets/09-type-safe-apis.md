# Module 28: Type-Safe APIs & Contracts Cheatsheet

> **Quick Reference**: Quick syntax reference for end-to-end type safety with tRPC, Fetch client typing, and OpenAPI generation.

---

## 1. Type-Safe Fetch Wrapper

```typescript
type HttpMethod = "GET" | "POST" | "PUT" | "DELETE";

interface RequestConfig<TBody = unknown> {
  method?: HttpMethod;
  headers?: Record<string, string>;
  body?: TBody;
}

export async function apiRequest<TResponse, TBody = unknown>(
  url: string,
  config: RequestConfig<TBody> = {}
): Promise<TResponse> {
  const { method = "GET", headers, body } = config;
  const res = await fetch(url, {
    method,
    headers: { "Content-Type": "application/json", ...headers },
    body: body ? JSON.stringify(body) : undefined,
  });

  if (!res.ok) {
    throw new Error(`API Request failed with status ${res.status}: ${res.statusText}`);
  }

  return (await res.json()) as TResponse;
}
```

---

## 2. Discriminated API Response Pattern

```typescript
export type ApiResponse<TData> =
  | { success: true; data: TData; timestamp: number }
  | { success: false; error: { code: string; message: string }; timestamp: number };

function handleResponse<T>(res: ApiResponse<T>): T {
  if (!res.success) {
    throw new Error(`[${res.error.code}] ${res.error.message}`);
  }
  return res.data; // Narrows automatically to TData!
}
```\n