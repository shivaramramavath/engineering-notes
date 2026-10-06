# Fetch

`fetch` is the browser's built-in API for HTTP requests. It's promise-based, available everywhere modern JavaScript runs (browsers, Node 18+), and needs no dependency. It's also **lower-level than most people expect**, and the gaps are where bugs come from.

## The basics

```ts
const res = await fetch("/api/projects")
const projects = await res.json()
```

That's two steps because `fetch` resolves **as soon as headers arrive**. The body is a stream you read separately (`res.json()`, `res.text()`, `res.blob()`, `res.formData()`, `res.arrayBuffer()`). Each body can be read **only once**; call `res.clone()` first if you need it twice.

## The big gotcha: HTTP errors don't reject

```ts
const res = await fetch("/api/projects/999")   // server returns 404
// ✗ no exception. The promise resolved.
```

`fetch` rejects **only on network-level failure** (offline, DNS, CORS block, aborted). A `404`, `401`, or `500` is a successful *HTTP exchange* as far as `fetch` is concerned. You must check:

```ts
const res = await fetch("/api/projects")
if (!res.ok) {                       // ok === status 200–299
  throw new Error(`Request failed: ${res.status}`)
}
const data = await res.json()
```

Forgetting `res.ok` is the most common fetch bug: error bodies get parsed as if they were data. Every project ends up wrapping this once; see [02 — API client](./02-api-client.md).

## Sending data

```ts
const res = await fetch("/api/projects", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ name: "Roadmap" }),
})
```

- `body` must be a string, `FormData`, `Blob`, `URLSearchParams`, or stream; **not** a plain object. Objects become `"[object Object]"`.
- `Content-Type: application/json` is not automatic when you pass a string.
- For file uploads with `FormData`, **do not set `Content-Type` yourself**. The browser must add the `multipart/form-data; boundary=…` value, and setting it manually drops the boundary and breaks the upload.

```ts
const form = new FormData()
form.append("file", file)
await fetch("/api/upload", { method: "POST", body: form })   // no Content-Type header
```

## Query strings

Don't concatenate by hand; values need encoding.

```ts
const params = new URLSearchParams({ q: "react hooks", page: "2" })
const res = await fetch(`/api/search?${params}`)
```

`URLSearchParams` only takes strings, so convert numbers and booleans and skip `undefined` (it would become the text `"undefined"`).

## Headers, credentials, and CORS

```ts
fetch(url, {
  headers: { Authorization: `Bearer ${token}` },
  credentials: "include",       // send/receive cookies cross-origin
})
```

| `credentials` | Cookies sent |
|---|---|
| `"same-origin"` (default) | Only to the same origin |
| `"include"` | Also cross-origin (server must opt in) |
| `"omit"` | Never |

**CORS** is enforced by the *browser*, based on headers the *server* sends. The server decides which origins may read responses. If your request fails with a CORS error, the fix is on the server (or a dev proxy), not in `fetch` options. Key rules:

- Requests with custom headers or non-simple methods trigger a **preflight** `OPTIONS` request first.
- With `credentials: "include"`, the server must respond with `Access-Control-Allow-Credentials: true` and a **specific** `Access-Control-Allow-Origin` (not `*`).
- A CORS failure looks like a generic `TypeError: Failed to fetch`. Open the network tab for the real reason.

In development, avoid cross-origin entirely with Vite's proxy:

```ts
// vite.config.ts
server: { proxy: { "/api": "http://localhost:3000" } }
```

## Cancellation and timeouts

Use an `AbortController`. Aborting makes `fetch` reject with a `DOMException` named `AbortError`:

```ts
const controller = new AbortController()

fetch("/api/search?q=react", { signal: controller.signal })
  .catch((err) => { if (err.name !== "AbortError") throw err })

controller.abort()
```

`fetch` has **no default timeout**; a hung request can wait for the browser's own limit. In modern browsers:

```ts
fetch(url, { signal: AbortSignal.timeout(8000) })   // rejects with a TimeoutError after 8s
```

Timeouts reject with `TimeoutError`, not `AbortError`, so handle both. To combine a caller's signal with a timeout, `AbortSignal.any([a, b])` is available in recent browsers; check support for your targets.

## Fetching in React: the race condition

The naive approach:

```tsx
useEffect(() => {
  fetch(`/api/users/${id}`).then((r) => r.json()).then(setUser)
}, [id])
```

If `id` changes from 1 → 2 and the response for 1 arrives *after* the response for 2, the UI shows the wrong user. Fix with cleanup:

```tsx
useEffect(() => {
  const controller = new AbortController()

  async function load() {
    try {
      const res = await fetch(`/api/users/${id}`, { signal: controller.signal })
      if (!res.ok) throw new Error(String(res.status))
      setUser(await res.json())
    } catch (err) {
      if ((err as Error).name === "AbortError") return   // expected on cleanup
      setError(err as Error)
    }
  }
  load()

  return () => controller.abort()
}, [id])
```

In development, React's StrictMode runs effects twice (mount → cleanup → mount), so you'll see your request fire, abort, and fire again. That's the cleanup being exercised, not a bug.

Even this correct version lacks caching, deduplication, retries, background refresh, and loading/error state management. That's why [TanStack Query](../12-server-state/03-tanstack-query.md) exists, and why [you might not need an effect](../03-hooks/03-you-might-not-need-an-effect.md) for data fetching. Learn the raw version to understand what the library replaces.

## Typing responses

```ts
const data = (await res.json()) as Project[]
```

This is a **type assertion**, not validation. TypeScript trusts you; if the server changes its shape, the runtime breaks silently. For data you don't control, validate at the boundary with a schema (see [02](./02-api-client.md#validating-responses)).

## Other common patterns

**Empty bodies.** `204 No Content` (and many `DELETE`s) have no body, and `res.json()` throws on empty input. Check `res.status === 204` or read `res.text()` first.

**Downloads.**

```ts
const blob = await (await fetch("/api/report.pdf")).blob()
const url = URL.createObjectURL(blob)
// use in <a download href={url}>, then URL.revokeObjectURL(url) when done
```

**Streaming.** `res.body` is a `ReadableStream`, useful for large downloads or token-by-token responses. See [realtime](./06-realtime-communication.md).

**Parallel requests.**

```ts
const [user, projects] = await Promise.all([getUser(), getProjects()])
```

Use `Promise.allSettled` when one failure shouldn't cancel the rest.

## fetch vs axios at a glance

| | `fetch` | axios |
|---|---|---|
| Dependency | Built in | ~npm package |
| HTTP errors reject | No (check `res.ok`) | Yes (non-2xx by default) |
| JSON | Manual (`res.json()`, `JSON.stringify`) | Automatic |
| Interceptors | No (wrap it yourself) | Built in |
| Timeout | `AbortSignal.timeout` | `timeout` option |
| Upload progress | Not supported | `onUploadProgress` |

Details in [01 — Axios](./01-axios.md).

## Common mistakes

- **Not checking `res.ok`**, treating 4xx/5xx bodies as data.
- **Passing an object as `body`** without `JSON.stringify`.
- **Missing `Content-Type: application/json`** on JSON posts, so servers ignore the body.
- **Setting `Content-Type` on `FormData`**, which breaks multipart boundaries.
- **Calling `res.json()` on an empty response** (204).
- **Reading a body twice** ("body stream already read").
- **No cleanup in `useEffect`**, causing race conditions and `setState` after navigation.
- **Treating `AbortError` as a real failure** and showing an error toast for a normal cancellation.
- **Building query strings by hand** with unencoded input.
- **`credentials: "include"` with a wildcard CORS origin**, which the browser rejects.
- **Trusting `as Type`** for data from the network.

## Quick summary

- `fetch` resolves on any HTTP response; **only network failure rejects**. Always check `res.ok`.
- Bodies are streams read once; JSON needs explicit `stringify` / `json()` and the right header.
- `FormData` sets its own `Content-Type`.
- Use `AbortController` for cancellation and `AbortSignal.timeout` for timeouts.
- Effect-based fetching needs cleanup to avoid races, and still lacks caching and retries.
- Wrap `fetch` once ([02](./02-api-client.md)) instead of repeating the boilerplate.

## Next

[01 — Axios](./01-axios.md)
