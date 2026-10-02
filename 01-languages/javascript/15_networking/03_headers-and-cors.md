# Headers and CORS

**Headers** carry metadata on requests and responses. **CORS** (Cross-Origin Resource Sharing) is the header-based protocol that lets servers permit browsers to share responses with pages from other origins.

## The Headers object

```js
const headers = new Headers({ Accept: "application/json" });
headers.set("Authorization", `Bearer ${token}`);     // replace
headers.append("X-Tag", "a");
headers.append("X-Tag", "b");                          // combined: "a, b"
headers.get("accept");                                  // case-insensitive
headers.has("x-tag");
headers.delete("x-tag");
for (const [name, value] of headers) console.log(name, value);

await fetch(url, { headers });                          // Headers, plain object, or array of pairs
```

Header names are case-insensitive. Some headers are **forbidden** for scripts in browsers (`Host`, `Cookie`, `Origin`, `Content-Length`, `User-Agent`, `Referer`, `Connection`, `Sec-*`, `Proxy-*`...): the browser controls them.

## Common request headers

| Header | Purpose |
|--------|---------|
| `Accept` | media types the client can handle (`application/json`) |
| `Accept-Language`, `Accept-Encoding` | language and compression preferences |
| `Authorization` | credentials (`Bearer <token>`, `Basic ...`) |
| `Content-Type` | type of the **request** body |
| `Content-Length` | body size (set by the runtime) |
| `Cookie` | cookies (set by the browser) |
| `Origin` | where the request came from (CORS, CSRF checks) |
| `Referer` | previous page URL (see `Referrer-Policy`) |
| `User-Agent` | client identity |
| `If-None-Match`, `If-Modified-Since` | conditional requests (caching) |
| `Range` | request part of a resource (`bytes=0-1023`) |
| `X-Request-Id`, `traceparent` | tracing and correlation |
| `Idempotency-Key` | safe retries of `POST` |

## Common response headers

| Header | Purpose |
|--------|---------|
| `Content-Type`, `Content-Length`, `Content-Encoding` | body description (`gzip`, `br`, `zstd`) |
| `Cache-Control`, `ETag`, `Last-Modified`, `Expires`, `Age`, `Vary` | caching |
| `Set-Cookie` | create/update cookies |
| `Location` | redirect target or created resource URL |
| `Retry-After` | when to retry (with `429`/`503`) |
| `Content-Disposition` | `attachment; filename="report.pdf"` for downloads |
| `Link` | pagination, preload (`rel="next"`) |
| `Access-Control-*` | CORS policy |
| `Server-Timing` | backend timing metrics visible in DevTools |
| `WWW-Authenticate` | challenge on `401` |

## Security headers (set by the server)

| Header | Effect |
|--------|--------|
| `Strict-Transport-Security: max-age=31536000; includeSubDomains` | force HTTPS |
| `Content-Security-Policy: default-src 'self'; script-src 'self'` | restrict where scripts, styles and connections may load/go; mitigates XSS |
| `X-Content-Type-Options: nosniff` | stop MIME sniffing |
| `Referrer-Policy: strict-origin-when-cross-origin` | limit referrer leakage |
| `Permissions-Policy: geolocation=(), camera=()` | disable powerful features |
| `Content-Security-Policy: frame-ancestors 'none'` (or `X-Frame-Options: DENY`) | prevent clickjacking |
| `Cross-Origin-Opener-Policy`, `Cross-Origin-Embedder-Policy`, `Cross-Origin-Resource-Policy` | isolation (needed for `SharedArrayBuffer`), cross-origin resource protection |
| `Set-Cookie: ...; HttpOnly; Secure; SameSite=Lax` | safer cookies |

Libraries such as **helmet** (Express) set a sensible baseline.

## Same-origin policy

Two URLs have the **same origin** only if **scheme, host and port** all match.

| URL | Same origin as `https://app.example.com`? |
|-----|--------------------------------------------|
| `https://app.example.com/other` | yes |
| `http://app.example.com` | no (scheme) |
| `https://api.example.com` | no (host) |
| `https://app.example.com:8443` | no (port) |

By default, a page may **send** requests to other origins (images, scripts, forms, `fetch`), but scripts **cannot read** the response unless the other server opts in via CORS. This protects users' data on other sites.

## How CORS works

```
Page at https://app.example.com  ──fetch──►  https://api.example.com/data
                                  Origin: https://app.example.com
                                  ◄── Access-Control-Allow-Origin: https://app.example.com
browser: allowed headers match? → expose the response to JS; otherwise block it
```

CORS is enforced **by the browser**. The request usually **reaches the server** either way; the browser only decides whether your JavaScript may see the response.

### Simple requests

Sent directly, with CORS checked on the response. A request is "simple" when:

- Method is `GET`, `HEAD` or `POST`
- Only CORS-safelisted headers (`Accept`, `Accept-Language`, `Content-Language`, `Content-Type` limited to `application/x-www-form-urlencoded`, `multipart/form-data`, `text/plain`)
- No event listeners on `XMLHttpRequest.upload` / no readable stream body

### Preflighted requests

Anything else (JSON `Content-Type`, `Authorization` header, `PUT`/`PATCH`/`DELETE`, custom headers) triggers an automatic **preflight** `OPTIONS` request first:

```http
OPTIONS /users HTTP/1.1
Origin: https://app.example.com
Access-Control-Request-Method: POST
Access-Control-Request-Headers: content-type, authorization
```

The server must answer:

```http
HTTP/1.1 204 No Content
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 600
Vary: Origin
```

Only then does the browser send the real request. `Access-Control-Max-Age` caches the preflight result (browsers cap it, e.g. 2 hours in Chromium, 24 hours in Firefox).

## CORS response headers

| Header | Meaning |
|--------|---------|
| `Access-Control-Allow-Origin` | allowed origin: one exact origin, or `*` |
| `Access-Control-Allow-Methods` | methods allowed (preflight response) |
| `Access-Control-Allow-Headers` | request headers allowed (preflight response) |
| `Access-Control-Allow-Credentials: true` | allow cookies/HTTP auth with the request |
| `Access-Control-Expose-Headers` | extra response headers JavaScript may read |
| `Access-Control-Max-Age` | seconds to cache the preflight |
| `Vary: Origin` | required when the allowed origin varies per request (so caches do not mix them up) |

## Credentials rules

Sending cookies cross-origin requires **both** sides:

```js
fetch("https://api.example.com/me", { credentials: "include" });
```

```http
Access-Control-Allow-Origin: https://app.example.com     (NOT *)
Access-Control-Allow-Credentials: true
```

With credentials, the wildcard `*` is **not allowed** for `Allow-Origin`, `Allow-Headers`, `Allow-Methods` or `Expose-Headers`. Cookies must also satisfy `SameSite` (cross-site cookies need `SameSite=None; Secure`) and third-party cookie restrictions in browsers.

## Fetch `mode`

| `mode` | Behavior |
|--------|----------|
| `"cors"` (default) | normal CORS: response readable if allowed |
| `"same-origin"` | cross-origin requests fail |
| `"no-cors"` | request is sent but the response is **opaque** (status 0, empty body): useless for reading data |
| `"navigate"` | page navigations |

`no-cors` does **not** fix CORS errors: it just hides the response.

## Server examples

### Express with the `cors` package

```js
import express from "express";
import cors from "cors";

const app = express();
const allowed = new Set(["https://app.example.com", "https://admin.example.com"]);

app.use(cors({
  origin: (origin, cb) => cb(null, !origin || allowed.has(origin)),    // no Origin header = non-browser client
  methods: ["GET", "POST", "PUT", "PATCH", "DELETE"],
  allowedHeaders: ["Content-Type", "Authorization"],
  exposedHeaders: ["X-Request-Id"],
  credentials: true,
  maxAge: 600,
}));
```

### Manual (framework-free) logic

```js
function corsHeaders(req) {
  const origin = req.headers.origin;
  if (!origin || !allowed.has(origin)) return {};
  return {
    "Access-Control-Allow-Origin": origin,
    "Access-Control-Allow-Credentials": "true",
    "Vary": "Origin",
  };
}

if (req.method === "OPTIONS") {
  res.writeHead(204, {
    ...corsHeaders(req),
    "Access-Control-Allow-Methods": "GET,POST,PUT,PATCH,DELETE",
    "Access-Control-Allow-Headers": req.headers["access-control-request-headers"] ?? "Content-Type",
    "Access-Control-Max-Age": "600",
  });
  return res.end();
}
```

Always **allowlist** origins; never reflect an arbitrary `Origin` with credentials enabled.

## Avoiding CORS with a proxy

| Option | When |
|--------|------|
| **Same-origin deployment** (API under the same domain, `/api`) | simplest and fastest (no preflights) |
| **Dev server proxy** (Vite `server.proxy`, webpack `devServer.proxy`) | local development |
| **Backend-for-frontend / API gateway** | your server calls third-party APIs; secrets stay on the server |
| **Cloudflare Workers / serverless proxy** | when you cannot change the upstream |

```js
// vite.config.js
export default { server: { proxy: { "/api": { target: "http://localhost:3000", changeOrigin: true } } } };
```

## Debugging CORS errors

Typical console messages and fixes:

| Message | Meaning / fix |
|---------|---------------|
| `No 'Access-Control-Allow-Origin' header is present` | server did not send CORS headers (also happens when the **error page** lacks them: check the real status) |
| `... does not match the supplied origin` | allowed origin differs: scheme/host/port exact match |
| `Response to preflight request doesn't pass access control check` | `OPTIONS` not handled, or returned a redirect/401 |
| `Request header field authorization is not allowed by Access-Control-Allow-Headers` | add it to `Allow-Headers` |
| `The value of the 'Access-Control-Allow-Origin' header must not be the wildcard '*' when credentials mode is 'include'` | use an explicit origin |
| `Redirect is not allowed for a preflight request` | preflight endpoints must not redirect |

Checklist:

1. Look at the **Network** panel: find the `OPTIONS` request and the real request
2. Confirm response headers on **both**
3. Make sure errors (4xx/5xx) also include CORS headers
4. Check proxies/CDNs do not strip headers or cache a response without `Vary: Origin`
5. Test with `curl -i -H "Origin: https://app.example.com" https://api.example.com/data`

## CORS is not authentication

- CORS protects **users' browsers**, not your server: `curl` and servers ignore it entirely
- Still authenticate and authorize every request on the server
- `Access-Control-Allow-Origin: *` is fine for **public, credential-less** data; never combine broad access with cookies
- Protect cookie-authenticated endpoints against **CSRF** (`SameSite`, CSRF tokens, `Origin` checks)

## Related cross-origin features

| Feature | Notes |
|---------|-------|
| `<script crossorigin>`, `<img crossorigin>` | opt in to CORS for those loads (needed for detailed error info and canvas pixel reads) |
| CORP (`Cross-Origin-Resource-Policy`) | servers say who may embed their resources |
| COEP/COOP | cross-origin isolation for `SharedArrayBuffer` and high-resolution timers |
| `postMessage` | controlled communication between windows/frames |
| JSONP | obsolete, insecure |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `mode: "no-cors"` to "fix" CORS | Opaque, unreadable response | Configure the server or proxy |
| `*` with credentials | Blocked by browsers | Explicit allowlisted origin |
| Reflecting any `Origin` | Lets any site read user data | Allowlist |
| Forgetting `Vary: Origin` | Cache serves one origin's headers to another | Add it |
| Not handling `OPTIONS` | Preflight fails | Respond `204` with CORS headers |
| CORS headers missing on error responses | Misleading "CORS error" hides the real failure | Add headers to all responses |
| Redirects on preflight | Fails | Serve the final URL directly |
| Thinking CORS secures the API | Non-browser clients bypass it | Real auth + CSRF protection |
| Custom headers everywhere | Forces preflights (latency) | Minimize or same-origin deploy |
| No `Access-Control-Expose-Headers` | JS cannot read custom headers | Expose the ones you need |

## Key takeaways

- Same-origin policy blocks reading cross-origin responses unless the server opts in via CORS headers
- Non-simple requests trigger a preflight `OPTIONS`; handle it and cache it with `Max-Age`
- Credentials need an explicit origin plus `Allow-Credentials: true` (no wildcards)
- CORS is browser enforcement, not security: still authenticate and defend against CSRF
- Prefer same-origin deployment or a proxy over fighting CORS

**Next:** [Request and Response](./04_request-and-response.md)
