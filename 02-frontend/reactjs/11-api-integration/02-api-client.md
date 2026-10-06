# API Client

Scattering `fetch("/api/...")` calls across components means every call re-implements the base URL, JSON headers, auth, `res.ok` checks, and error handling, slightly differently each time. An **API client** is one small module that does those things once, so the rest of the app just says `api.get("/projects")`.

This note builds a typed, fetch-based client. Axios users can apply the same structure to an [axios instance](./01-axios.md); only the internals differ.

## What the client is responsible for

| Concern | Handled in the client |
|---|---|
| Base URL | From env var, never hardcoded |
| Query params | Encoded, `undefined` skipped |
| JSON in/out | Stringify bodies, parse responses, handle `204` |
| Auth | Attach the access token ([03](./03-authentication.md)) |
| Errors | Throw one consistent `ApiError` |
| Cancellation | Forward `AbortSignal` |
| Not its job | Caching, loading state, retries on screen → [TanStack Query](../12-server-state/03-tanstack-query.md) |

## The error type

```ts
// lib/api/api-error.ts
export class ApiError extends Error {
  status: number                 // 0 = network failure
  code?: string                  // machine-readable, e.g. "VALIDATION_FAILED"
  details?: unknown              // field errors, server payload

  constructor(init: { status: number; message: string; code?: string; details?: unknown }) {
    super(init.message)
    this.name = "ApiError"
    this.status = init.status
    this.code = init.code
    this.details = init.details
  }
}
```

Fields are assigned explicitly (rather than with TypeScript parameter properties) because Vite's current TypeScript templates enable `erasableSyntaxOnly`, which disallows parameter properties.

## The token store

The client needs the current access token without importing React. A tiny module-level store works:

```ts
// lib/api/token-store.ts
let accessToken: string | null = null

export const tokenStore = {
  get: () => accessToken,
  set: (token: string | null) => { accessToken = token },
}
```

Why not React context? The client is plain TypeScript, called from loaders, query functions, and event handlers, not just components. How the token gets *into* the store is covered in [03](./03-authentication.md).

## The client

```ts
// lib/api/client.ts
import { ApiError } from "./api-error"
import { tokenStore } from "./token-store"

const BASE_URL = import.meta.env.VITE_API_URL ?? "/api"

type Params = Record<string, string | number | boolean | null | undefined>

export type RequestOptions = Omit<RequestInit, "body" | "method"> & {
  params?: Params
  body?: unknown
}

function buildUrl(path: string, params?: Params) {
  const url = new URL(`${BASE_URL}${path}`, window.location.origin)
  if (params) {
    for (const [key, value] of Object.entries(params)) {
      if (value !== undefined && value !== null) url.searchParams.set(key, String(value))
    }
  }
  return url
}

async function parseBody(res: Response): Promise<unknown> {
  if (res.status === 204) return undefined
  const text = await res.text()
  if (!text) return undefined
  const isJson = res.headers.get("content-type")?.includes("json")
  return isJson ? JSON.parse(text) : text
}

async function request<T>(method: string, path: string, options: RequestOptions = {}): Promise<T> {
  const { params, body, headers, ...init } = options

  const finalHeaders = new Headers(headers)
  finalHeaders.set("Accept", "application/json")

  const token = tokenStore.get()
  if (token && !finalHeaders.has("Authorization")) {
    finalHeaders.set("Authorization", `Bearer ${token}`)
  }

  let payload: BodyInit | undefined
  if (body instanceof FormData) {
    payload = body                                   // browser sets the multipart header
  } else if (body !== undefined) {
    finalHeaders.set("Content-Type", "application/json")
    payload = JSON.stringify(body)
  }

  let res: Response
  try {
    res = await fetch(buildUrl(path, params), { ...init, method, headers: finalHeaders, body: payload })
  } catch (err) {
    if (err instanceof DOMException && (err.name === "AbortError" || err.name === "TimeoutError")) throw err
    throw new ApiError({ status: 0, code: "NETWORK_ERROR", message: "Network error. Check your connection." })
  }

  const data = await parseBody(res).catch(() => undefined)

  if (!res.ok) {
    const problem = (data ?? {}) as { message?: string; code?: string; errors?: unknown }
    throw new ApiError({
      status: res.status,
      code: problem.code,
      message: problem.message ?? (res.statusText || `Request failed (${res.status})`),
      details: problem.errors ?? data,
    })
  }

  return data as T
}

export const api = {
  get:    <T>(path: string, options?: RequestOptions) => request<T>("GET", path, options),
  post:   <T>(path: string, body?: unknown, options?: RequestOptions) => request<T>("POST", path, { ...options, body }),
  put:    <T>(path: string, body?: unknown, options?: RequestOptions) => request<T>("PUT", path, { ...options, body }),
  patch:  <T>(path: string, body?: unknown, options?: RequestOptions) => request<T>("PATCH", path, { ...options, body }),
  delete: <T = void>(path: string, options?: RequestOptions) => request<T>("DELETE", path, options),
}
```

Walking through the decisions:

- **`new URL(..., window.location.origin)`** handles both a relative base (`/api`, via the Vite proxy) and an absolute one (`https://api.example.com`).
- **Abort and timeout errors pass through untouched**, so callers and TanStack Query can recognize cancellations. Other thrown errors are network failures, which become `ApiError` with `status: 0`.
- **The error body is read defensively.** Servers return JSON errors, HTML error pages from proxies, or nothing. Parsing failures never mask the real HTTP status.
- **`details`** keeps the server payload so form code can read field errors ([05](./05-api-error-handling.md)).
- **Generics** (`api.get<Project[]>`) type the return value. See below for runtime validation.

## Usage: resource modules

Components shouldn't know URLs. Group endpoints by resource:

```ts
// features/projects/api.ts
import { api } from "@/lib/api/client"

export type Project = { id: string; name: string; status: "open" | "closed" }
export type ProjectFilters = { q?: string; status?: string; page?: number }

export const projectsApi = {
  list:   (filters: ProjectFilters, signal?: AbortSignal) =>
            api.get<{ items: Project[]; total: number }>("/projects", { params: filters, signal }),
  get:    (id: string, signal?: AbortSignal) => api.get<Project>(`/projects/${id}`, { signal }),
  create: (input: Pick<Project, "name">) => api.post<Project>("/projects", input),
  update: (id: string, input: Partial<Project>) => api.patch<Project>(`/projects/${id}`, input),
  remove: (id: string) => api.delete(`/projects/${id}`),
}
```

And call them from TanStack Query:

```ts
useQuery({
  queryKey: ["projects", filters],
  queryFn: ({ signal }) => projectsApi.list(filters, signal),
})

useMutation({ mutationFn: projectsApi.create })
```

The component sees `useQuery` and a typed result. Changing an endpoint touches one file. The query `signal` lets TanStack Query cancel stale requests ([fetching data](../12-server-state/01-fetching-data.md)).

## Validating responses

`api.get<Project>()` is an assertion; if the server returns something else, you get a runtime crash far from the cause. For external or evolving APIs, validate with a schema at the boundary:

```ts
import { z } from "zod"

const projectSchema = z.object({ id: z.string(), name: z.string(), status: z.enum(["open", "closed"]) })
export type Project = z.infer<typeof projectSchema>

get: async (id: string, signal?: AbortSignal) =>
  projectSchema.parse(await api.get<unknown>(`/projects/${id}`, { signal })),
```

A mismatch now fails loudly **at the API boundary** with a clear message, and the schema is also your type. The trade-off is bundle size and CPU on large payloads, so many teams validate only for third-party APIs or critical endpoints.

## Environment configuration

```bash
# .env.development
VITE_API_URL=/api
# .env.production
VITE_API_URL=https://api.example.com
```

Anything prefixed `VITE_` is **embedded in the public bundle**. Only put non-secret config here, never API keys or secrets. See [environment variables](../19-production/00-environment-variables.md).

## Testing

Because everything goes through `fetch`, you can intercept at the network level with [MSW](../18-testing-and-debugging/04-mocking-and-msw.md) instead of mocking your own modules, so tests exercise the real client, headers, and error handling.

## Where this fits

This note covers the *client module*. How it's organized in a larger app (feature folders, shared types, layering) is in [API architecture](../20-frontend-architecture/02-api-architecture.md).

## Common mistakes

- **Calling `fetch` directly in components**, duplicating headers, error handling, and URLs.
- **Hardcoded base URLs** (`http://localhost:3000`) that break in production.
- **Not checking `res.ok`** in a custom wrapper (the very thing the client exists for).
- **Throwing plain `Error`s with no status**, so callers can't distinguish 404 from 500 from offline.
- **Parsing the error body without a guard**, so an HTML 502 page throws a JSON parse error and hides the real failure.
- **Swallowing abort errors as failures**, or converting them into generic network errors.
- **Setting `Content-Type: application/json` on `FormData`.**
- **Secrets in `VITE_` variables.**
- **Letting a component know endpoint paths** instead of using resource modules.
- **Building retries, caching, and loading state into the client**; that's the job of the server-state layer.

## Quick summary

- One client module owns base URL, headers, JSON, auth header, and error shape.
- Throw a single `ApiError` (`status`, `code`, `details`); status `0` means network failure.
- Pass abort/timeout errors through unchanged; read error bodies defensively.
- Group endpoints into resource modules; components call those, usually via TanStack Query.
- `<T>` generics are assertions; validate with a schema when the data isn't yours.
- Keep secrets out of `VITE_` env vars.

## Next

[03 — Authentication](./03-authentication.md)
