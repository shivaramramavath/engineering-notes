# Axios

Axios is an HTTP client library that wraps the browser's networking and smooths over the rough edges of [`fetch`](./00-fetch.md): it rejects on HTTP errors, parses JSON for you, and provides **interceptors**, which are hooks that run on every request and response.

```bash
npm install axios
```

## Do you need it?

Modern `fetch` plus a ~50-line wrapper ([02](./02-api-client.md)) covers most apps. Axios earns its place when you want:

- **Interceptors** out of the box (auth headers, token refresh, logging)
- **Upload progress** (`onUploadProgress`)
- **Automatic JSON** and consistent error objects without writing a wrapper
- A team that already knows it, and consistent behavior across older environments

It costs bundle size and one more dependency to keep updated. Neither choice is wrong; pick one and use it **consistently**.

## Basic usage

```ts
import axios from "axios"

const { data } = await axios.get<Project[]>("/api/projects", {
  params: { status: "open", page: 2 },     // → ?status=open&page=2
})

await axios.post("/api/projects", { name: "Roadmap" })   // object auto-serialized as JSON
```

Differences from `fetch` you'll notice immediately:

- The result is a response object; the parsed body is `response.data`.
- Request bodies can be plain objects (JSON is the default).
- `params` builds the query string. `undefined` values are skipped.
- **Non-2xx responses reject.** No `res.ok` check needed.

## Create an instance

Never use the global `axios` for app requests. Create a configured instance:

```ts
// lib/api/axios-client.ts
import axios from "axios"

export const http = axios.create({
  baseURL: import.meta.env.VITE_API_URL ?? "/api",
  timeout: 10_000,
  headers: { Accept: "application/json" },
  withCredentials: true,                    // send cookies cross-origin (like fetch's credentials: "include")
})
```

One instance = one place for base URL, timeout, and interceptors. `timeout` is in milliseconds, and `0` (the default) means no timeout.

## Interceptors

### Request: attach the auth token

```ts
http.interceptors.request.use((config) => {
  const token = tokenStore.get()
  if (token) config.headers.Authorization = `Bearer ${token}`
  return config
})
```

### Response: handle errors centrally

```ts
http.interceptors.response.use(
  (response) => response,                    // 2xx passes through
  (error) => {
    if (axios.isAxiosError(error) && error.response?.status === 401) {
      // refresh the token or log out; see 04
    }
    return Promise.reject(error)             // always re-throw so callers can still catch
  }
)
```

The second argument runs for failures. Forgetting to re-reject **swallows the error**, and callers then receive `undefined` as if the request had succeeded.

## Errors

An axios failure comes in three shapes; check which one you have:

```ts
try {
  await http.get("/projects")
} catch (err) {
  if (axios.isAxiosError(err)) {
    if (err.response) {
      // Server responded with non-2xx
      err.response.status      // 404
      err.response.data        // parsed error body
    } else if (err.request) {
      // Request sent, no response (network down, timeout, CORS)
      err.code                 // "ERR_NETWORK", "ECONNABORTED" (timeout), "ERR_CANCELED"
    }
  } else {
    // Something else threw (bug in your code)
  }
}
```

`axios.isAxiosError` is a type guard, so inside the branch TypeScript knows the shape. See [05 — API error handling](./05-api-error-handling.md) for turning this into one app-wide error type.

## Cancellation

Axios accepts the standard `AbortController` signal, exactly like `fetch`:

```ts
const controller = new AbortController()
http.get("/search", { params: { q }, signal: controller.signal })
controller.abort()     // rejects with a CanceledError (code "ERR_CANCELED")
```

The older `CancelToken` API is deprecated; use `signal`.

## Upload progress

```ts
await http.post("/upload", formData, {
  onUploadProgress: (e) => {
    if (e.total) setProgress(Math.round((e.loaded / e.total) * 100))
  },
})
```

Like `fetch`, pass `FormData` and let the browser set the multipart header. `fetch` has no equivalent for upload progress.

## Adapters

By default in browsers Axios uses `XMLHttpRequest` under the hood. Recent versions also offer a `fetch`-based adapter (`adapter: "fetch"`), which can be useful for streaming responses. Check the axios docs for your version before relying on it.

## Typing

```ts
const { data } = await http.get<Project>(`/projects/${id}`)
```

The generic types `data`, but as with `fetch` it's an assertion, not validation. Validate with a schema when you don't control the server ([02](./02-api-client.md#validating-responses)).

## Using it with TanStack Query

Axios plugs straight into `queryFn`:

```ts
useQuery({
  queryKey: ["projects", filters],
  queryFn: async ({ signal }) => (await http.get<Project[]>("/projects", { params: filters, signal })).data,
})
```

Pass the `signal` from the query context so that TanStack Query can cancel outdated requests. See [fetching data](../12-server-state/01-fetching-data.md).

## Common mistakes

- **Using the global `axios`** instead of an instance, so there's no shared config.
- **Forgetting `.data`**: `const projects = await http.get(...)` gives you the whole response.
- **Not re-rejecting in a response interceptor**, silently swallowing errors.
- **Assuming `error.response` always exists.** Network errors and timeouts have none.
- **Registering interceptors inside components or effects**, which stacks a new one every render. Register once at module level (or eject with `interceptors.request.eject(id)`).
- **Setting `Content-Type` manually for `FormData`.**
- **Mixing axios and fetch** in the same codebase without a reason, which gives two error models.
- **Using `CancelToken`** in new code.
- **Refresh logic that triggers itself**: the refresh call goes through the same instance and hits the same 401 interceptor ([04](./04-refresh-token-flow.md)).

## Quick summary

- Axios = `fetch` + JSON handling + rejection on HTTP errors + interceptors.
- Always create an **instance** with `baseURL`, `timeout`, and credentials settings.
- Request interceptors add headers; response interceptors centralize errors, and must re-reject.
- Errors have a `response` (server replied), a `request` (no reply), or neither (your bug).
- Cancel with `AbortController`; upload progress is a real advantage over `fetch`.
- Choose axios **or** a fetch wrapper, not both.

## Next

[02 — API client](./02-api-client.md)
