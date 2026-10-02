# Request and Response

`Request` and `Response` are standard objects (the Fetch standard) used by `fetch`, **service workers**, Cloudflare Workers, Deno, Bun, Node.js and many server frameworks (Hono, Remix, SvelteKit, Next.js route handlers). Learning them pays off on both client and server.

```js
const request = new Request("https://api.example.com/users", { method: "POST", body: JSON.stringify({ name: "Ada" }) });
const response = await fetch(request);
```

## Request

```js
const req = new Request("/api/users", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ name: "Ada" }),
  credentials: "same-origin",
  signal: AbortSignal.timeout(5000),
});

req.method;          // "POST"
req.url;             // absolute URL
req.headers.get("content-type");
req.mode; req.credentials; req.cache; req.redirect; req.signal;
await req.json();    // read the body (once)

const copy = req.clone();                 // before reading the body
const modified = new Request(req, { headers: { ...Object.fromEntries(req.headers), "X-Trace": "1" } });
```

| Property | Meaning |
|----------|---------|
| `url`, `method`, `headers` | basics |
| `body`, `bodyUsed` | `ReadableStream` and consumption flag |
| `mode`, `credentials`, `cache`, `redirect`, `referrer`, `integrity`, `keepalive`, `signal` | fetch options |
| `json()`, `text()`, `formData()`, `arrayBuffer()`, `blob()` | read the body |
| `clone()` | duplicate |

## Response

```js
const res = new Response(JSON.stringify({ ok: true }), {
  status: 201,
  statusText: "Created",
  headers: { "Content-Type": "application/json", Location: "/users/43" },
});

Response.json({ ok: true }, { status: 200 });             // shortcut: sets Content-Type
Response.redirect("https://example.com/login", 302);
Response.error();                                           // network-error response
new Response(null, { status: 204 });
new Response("<h1>Hi</h1>", { headers: { "Content-Type": "text/html; charset=utf-8" } });
```

| Property | Meaning |
|----------|---------|
| `ok`, `status`, `statusText` | outcome |
| `headers`, `url`, `redirected`, `type` | metadata |
| `body`, `bodyUsed` | stream and flag |
| `json()`, `text()`, `blob()`, `arrayBuffer()`, `formData()`, `bytes()` | consume the body |
| `clone()` | duplicate (to read twice or to cache) |

`Response.type`:

| Type | Meaning |
|------|---------|
| `basic` | same-origin |
| `cors` | cross-origin with CORS allowed (limited headers) |
| `opaque` | `no-cors` cross-origin: status 0, no readable body |
| `opaqueredirect` | `redirect: "manual"` result |
| `error` | network error |

## Choosing the body reader

| Method | Returns | Use |
|--------|---------|-----|
| `json()` | parsed value | JSON APIs (`SyntaxError` if invalid/empty) |
| `text()` | string | HTML, plain text, custom formats |
| `blob()` | `Blob` | images, downloads, `URL.createObjectURL` |
| `arrayBuffer()` / `bytes()` | binary | parsing binary formats, hashing |
| `formData()` | `FormData` | multipart/urlencoded bodies |
| `body` (stream) | `ReadableStream` | large/streaming data |

Bodies are **streams** consumed once. For large payloads, process chunks instead of reading everything into memory.

## Safe JSON parsing

```js
async function readBody(response) {
  if (response.status === 204 || response.status === 205) return null;
  const type = response.headers.get("content-type") ?? "";
  if (type.includes("application/json") || type.includes("+json")) return response.json();
  if (type.startsWith("text/")) return response.text();
  return response.blob();
}
```

## Error response formats

Return **structured** errors so clients can react programmatically. A widely used standard is **Problem Details** (RFC 9457):

```http
HTTP/1.1 422 Unprocessable Content
Content-Type: application/problem+json

{
  "type": "https://example.com/problems/validation",
  "title": "Validation failed",
  "status": 422,
  "detail": "2 fields are invalid",
  "instance": "/users",
  "errors": [
    { "field": "email", "message": "Must be a valid email" },
    { "field": "age", "message": "Must be at least 13" }
  ]
}
```

```js
if (!res.ok) {
  const problem = res.headers.get("content-type")?.includes("json") ? await res.json() : { title: res.statusText };
  throw new ApiError(res.status, problem);
}
```

## Building responses on the server (fetch-style handlers)

```js
// Cloudflare Workers / Deno / Bun / Hono-style handler
export default {
  async fetch(request) {
    const url = new URL(request.url);

    if (request.method === "GET" && url.pathname === "/api/health") {
      return Response.json({ status: "ok" });
    }
    if (request.method === "POST" && url.pathname === "/api/users") {
      const body = await request.json().catch(() => null);
      if (!body?.name) return Response.json({ error: "name required" }, { status: 400 });
      return Response.json({ id: 1, ...body }, { status: 201, headers: { Location: "/api/users/1" } });
    }
    return new Response("Not found", { status: 404 });
  },
};
```

In Node, `http.createServer` uses its own request/response types; frameworks (Hono, Fastify adapters, `@hono/node-server`, Next.js) bridge to the standard objects.

## Service worker example

```js
self.addEventListener("fetch", (event) => {
  event.respondWith(
    caches.match(event.request).then((cached) => cached ?? fetch(event.request).then((res) => {
      const copy = res.clone();                               // one for the cache, one for the page
      caches.open("v1").then((cache) => cache.put(event.request, copy));
      return res;
    })),
  );
});
```

## Binary data and files

```js
// download with the right filename (server)
new Response(fileStream, {
  headers: {
    "Content-Type": "application/pdf",
    "Content-Disposition": 'attachment; filename="report.pdf"',
    "Content-Length": String(size),
  },
});

// upload multiple files with metadata (client)
const form = new FormData();
form.append("meta", new Blob([JSON.stringify({ folder: "docs" })], { type: "application/json" }));
for (const file of files) form.append("files", file, file.name);
await fetch("/upload", { method: "POST", body: form });

// read a response as bytes
const bytes = new Uint8Array(await res.arrayBuffer());
```

## Range requests and partial content

```js
const res = await fetch("/video.mp4", { headers: { Range: "bytes=0-1048575" } });
res.status;                         // 206 Partial Content
res.headers.get("content-range");   // "bytes 0-1048575/9876543"
```

Servers advertise `Accept-Ranges: bytes`. Used for video seeking, resumable downloads and parallel chunk downloads.

## Conditional requests

```js
const first = await fetch("/api/config");
const etag = first.headers.get("etag");

const again = await fetch("/api/config", { headers: { "If-None-Match": etag } });
if (again.status === 304) useCached();             // no body transferred
```

Optimistic concurrency for writes:

```js
await fetch("/api/doc/7", { method: "PUT", headers: { "If-Match": etag }, body });   // 412 if the doc changed meanwhile
```

## Pagination formats

| Style | Request | Notes |
|-------|---------|-------|
| Offset/limit | `?offset=40&limit=20` | simple, drifts when data changes, slow for big offsets |
| Page number | `?page=3&per_page=20` | same trade-offs |
| **Cursor** | `?after=abc123&limit=20` | stable, scalable, no skipping/duplicates |
| `Link` header | `Link: <...?page=4>; rel="next"` | GitHub-style, discoverable |
| Envelope | `{ "items": [], "nextCursor": "..." }` | explicit and easy |

## Streaming a response from the server

```js
// Server-Sent-Events-like or NDJSON stream
const stream = new ReadableStream({
  async start(controller) {
    for await (const row of queryRows()) controller.enqueue(new TextEncoder().encode(JSON.stringify(row) + "\n"));
    controller.close();
  },
});
return new Response(stream, { headers: { "Content-Type": "application/x-ndjson" } });
```

## Typed client pattern

```js
class ApiError extends Error {
  constructor(status, problem) {
    super(problem?.title ?? `HTTP ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.problem = problem;
  }
}

async function request(path, { method = "GET", body, signal } = {}) {
  const res = await fetch(`/api${path}`, {
    method,
    headers: { Accept: "application/json", ...(body && { "Content-Type": "application/json" }) },
    body: body && JSON.stringify(body),
    signal,
  });
  const data = res.status === 204 ? null : await res.json().catch(() => null);
  if (!res.ok) throw new ApiError(res.status, data);
  return data;
}
```

Validate the shape with a schema (Zod, Valibot) before using `data`.

## Content-Length vs chunked transfer

- Known size: `Content-Length` (enables progress bars)
- Streaming/unknown size: `Transfer-Encoding: chunked` (HTTP/1.1) or framed data (HTTP/2/3)
- Compressed responses: `Content-Length` shows **compressed** bytes, so progress bars based on decoded bytes can exceed 100%

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Reading the body twice | `TypeError` | `clone()` first |
| `res.json()` on 204/empty/HTML error pages | `SyntaxError` | Check status and `Content-Type` |
| Caching or returning the same `Response` object twice | Body already consumed | `clone()` |
| Non-JSON error bodies hidden behind generic messages | Impossible debugging | Read `text()` for errors |
| Returning `200` with error JSON | Clients cannot rely on status | Correct status codes + problem details |
| Offset pagination on changing data | Duplicates/skips | Cursor pagination |
| Loading huge responses into memory | Slow, crashes | Stream chunks |
| Wrong `Content-Type` on responses | Browsers misparse, security issues (sniffing) | Set it correctly + `nosniff` |
| Missing `Content-Disposition` for downloads | Browser tries to display | Add `attachment; filename=` |
| Trusting `Content-Length` | Compression, chunking | Use it only as a hint |

## Key takeaways

- `Request` and `Response` are the shared standard behind `fetch`, service workers and modern server runtimes
- Pick the body reader that matches the content type; bodies are single-use streams (`clone()` when needed)
- Use structured error bodies (problem details) and consistent status codes
- Support conditional and range requests, and prefer cursor pagination and streaming for large data

**Next:** [WebSockets and SSE](./05_websockets-and-sse.md)
