# Request and Response

Route Handlers and the proxy speak the Web `Request` and `Response` APIs. Next.js adds two small extensions, `NextRequest` and `NextResponse`, for cookies, URLs and common responses. This note is the working reference for reading input and building output.

> Verified against the Next.js 16.4 docs.

## `Request` → `NextRequest`

The first argument of a handler is a `NextRequest`, a standard `Request` plus conveniences. Type it only when you need those extras.

```ts
import type { NextRequest } from "next/server";

export async function GET(request: NextRequest) {
  request.method;                       // "GET"
  request.headers.get("user-agent");
  request.nextUrl.pathname;             // "/api/search"
  request.nextUrl.searchParams.get("q");
  request.cookies.get("theme")?.value;
}
```

| Member | Use |
|---|---|
| `request.nextUrl` | Parsed URL: `pathname`, `searchParams`, `basePath`, `buildId` |
| `request.cookies` | `get`, `getAll`, `has`, `set`, `delete`, `clear` |
| `request.headers` | Standard `Headers` |
| `request.url` | Full URL string |
| `request.json()`, `.text()`, `.formData()`, `.arrayBuffer()`, `.blob()` | Read the body |

`request.ip` and `request.geo` were removed in Next.js 15. Take the client IP from your host's forwarded headers (for example `x-forwarded-for`), and treat it as untrusted unless your proxy sets it.

### Query parameters

```ts
export async function GET(request: NextRequest) {
  const { searchParams } = request.nextUrl;
  const q = searchParams.get("q") ?? "";
  const page = Number(searchParams.get("page") ?? "1");
  const tags = searchParams.getAll("tag");           // ?tag=a&tag=b

  if (!Number.isInteger(page) || page < 1) {
    return Response.json({ error: "Invalid page" }, { status: 400 });
  }
  return Response.json({ q, page, tags });
}
```

Query values are strings. Validate and convert them, as with `FormData`.

### Reading the body

Pick the method that matches the content type. **A body can be read only once.**

```ts
// JSON
const data = await request.json();

// Form posts (application/x-www-form-urlencoded or multipart/form-data)
const form = await request.formData();
const name = form.get("name");          // string | File | null

// Raw text (webhooks that need the exact bytes)
const raw = await request.text();

// Binary
const buf = await request.arrayBuffer();
```

To read it twice, clone first:

```ts
const copy = request.clone();
const raw = await request.text();
const json = JSON.parse(await copy.text());
```

`GET` and `HEAD` requests have no body.

Always handle parse failures; `request.json()` throws on invalid JSON:

```ts
let body: unknown;
try {
  body = await request.json();
} catch {
  return Response.json({ error: "Invalid JSON" }, { status: 400 });
}
```

Then validate `body` with a schema before using it. See [Validation](../07-server-actions/02-validation.md); the same Zod approach applies.

### Headers and cookies via `next/headers`

Inside a handler you can also use the async helpers, which work anywhere on the server:

```ts
import { cookies, headers } from "next/headers";

export async function GET() {
  const cookieStore = await cookies();
  const session = cookieStore.get("session")?.value;

  const h = await headers();
  const referer = h.get("referer");

  return Response.json({ signedIn: Boolean(session), referer });
}
```

`headers()` is **read-only**. To set response headers, build the response with them. Under Cache Components, reading `cookies()`/`headers()` makes the handler run at request time.

## Building responses

### The Web way

```ts
return new Response("plain text", { status: 200 });
return new Response(null, { status: 204 });
return Response.json({ ok: true }, { status: 201 });
return Response.redirect(new URL("/login", request.url), 307);
```

`Response.json()` sets `Content-Type: application/json` for you.

### `NextResponse` helpers

```ts
import { NextResponse } from "next/server";

NextResponse.json({ error: "Bad" }, { status: 400 });
NextResponse.redirect(new URL("/login", request.url));   // needs an absolute URL
NextResponse.rewrite(new URL("/other", request.url));    // serve another URL, keep this one in the browser
NextResponse.next();                                     // proxy only: continue to the route
```

`NextResponse` also exposes `response.cookies` (`set`, `get`, `getAll`, `has`, `delete`) and `response.headers`.

`redirect()` and `rewrite()` require **absolute** URLs. Build them from `request.url`.

## Status codes you will use

| Code | Meaning | Typical use |
|---|---|---|
| 200 | OK | Successful read |
| 201 | Created | After a successful `POST` (send the new resource) |
| 204 | No content | Successful delete / no body |
| 301 / 308 | Permanent redirect | Moved URL |
| 302 / 307 | Temporary redirect | Login redirect (307 keeps the method) |
| 400 | Bad request | Invalid JSON or input |
| 401 | Unauthorized | Not signed in |
| 403 | Forbidden | Signed in, not allowed |
| 404 | Not found | Missing resource |
| 405 | Method not allowed | Next.js sends this for unexported methods |
| 409 | Conflict | Duplicate, version clash |
| 413 | Payload too large | Body over your limit |
| 415 | Unsupported media type | Wrong `Content-Type` |
| 422 | Unprocessable | Semantically invalid input |
| 429 | Too many requests | Rate limited (add `Retry-After`) |
| 500 | Server error | Unexpected failure |

## Setting cookies

On a `NextResponse`:

```ts
const response = NextResponse.json({ ok: true });
response.cookies.set({
  name: "session",
  value: token,
  httpOnly: true,   // not readable by JavaScript
  secure: true,     // HTTPS only
  sameSite: "lax",
  path: "/",
  maxAge: 60 * 60 * 24 * 7,
});
return response;
```

Or with `cookies()` from `next/headers` inside a handler (`(await cookies()).set(...)`). Session cookies should be `httpOnly`, `secure` and `sameSite`.

## Setting headers

```ts
return new Response(csv, {
  headers: {
    "Content-Type": "text/csv; charset=utf-8",
    "Content-Disposition": 'attachment; filename="report.csv"',
    "Cache-Control": "private, no-store",
  },
});
```

Be deliberate about which headers go to the client. Do not copy all incoming request headers onto the response, since that can leak `authorization` or cookies.

## CORS

CORS matters only when a **browser on another origin** calls your endpoint. Same-origin calls, server-to-server calls and webhooks do not need it.

For a single handler, set headers and answer the preflight:

```ts
const ALLOWED = new Set(["https://app.example.com"]);

function corsHeaders(origin: string | null) {
  const headers: Record<string, string> = {
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization",
    Vary: "Origin",
  };
  if (origin && ALLOWED.has(origin)) headers["Access-Control-Allow-Origin"] = origin;
  return headers;
}

export async function OPTIONS(request: Request) {
  return new Response(null, { status: 204, headers: corsHeaders(request.headers.get("origin")) });
}

export async function GET(request: Request) {
  return Response.json({ ok: true }, { headers: corsHeaders(request.headers.get("origin")) });
}
```

Rules:

- Echo back an origin only if it is on your **allow-list**. `*` is acceptable only for truly public, credential-free data.
- Never combine `Access-Control-Allow-Origin: *` with credentials.
- Add `Vary: Origin` when the value depends on the request origin, so caches do not mix responses up.
- For many routes, set CORS once in the [proxy](./02-proxy.md) or in `next.config.ts` `headers()`.
- CORS is enforced by browsers. It does **not** protect your API from curl, scripts or servers. Authenticate anyway.

## Errors: one consistent shape

Pick one error body and use it everywhere:

```ts
// lib/http.ts
import { NextResponse } from "next/server";

export function problem(status: number, message: string, details?: unknown) {
  return NextResponse.json({ error: { message, details } }, { status });
}
```

```ts
if (!session) return problem(401, "Sign in required");
if (!parsed.success) return problem(400, "Invalid input", z.flattenError(parsed.error).fieldErrors);
```

Clients can then handle every failure the same way.

## Calling your API from the client

```tsx
"use client";

async function createPost(title: string) {
  const res = await fetch("/api/posts", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ title }),
  });
  if (!res.ok) throw new Error((await res.json()).error?.message ?? "Request failed");
  return res.json();
}
```

`fetch` does not reject on `404` or `500`. Always check `res.ok`. See [Client Fetching](../05-data-fetching/01-client-fetching.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Reading `request.json()` twice | "Body is unusable" error | Read once or `request.clone()` |
| Relative URL in `NextResponse.redirect("/login")` | Invalid URL error | `new URL("/login", request.url)` |
| Parsing JSON without `try/catch` | 500 on malformed input | Return `400` |
| `*` CORS with cookies | Browser blocks, or credentials exposed | Allow-list origins |
| Treating CORS as security | Anyone can still call from curl | Authenticate and authorize |
| Using `request.ip` | `undefined` (removed in 15) | Read forwarded headers from your host |
| Forgetting `Content-Type` on non-JSON bodies | Browser mis-renders or downloads | Set it explicitly |
| Cookies without `httpOnly`/`secure` | Session theft risk | Set the flags |
| Not validating query params | `NaN` or injection | Convert and check |
| Returning `200` with `{ error }` | Clients cannot detect failure | Use the right status |

## Quick Summary

- `NextRequest` = `Request` + `nextUrl` + `cookies`; read the body once with `json()`, `text()` or `formData()`.
- Parse defensively: bad JSON is a `400`, not a crash.
- Build responses with `Response.json`, `NextResponse.json`, `redirect`, `rewrite`; redirects need absolute URLs.
- Set cookies with `httpOnly`, `secure`, `sameSite`.
- CORS is a browser rule, not authentication; allow-list origins.
- Use consistent status codes and one error shape.

## Next

- [Proxy](./02-proxy.md)
- [Webhooks](./03-webhooks.md)
- [Client Fetching](../05-data-fetching/01-client-fetching.md)
