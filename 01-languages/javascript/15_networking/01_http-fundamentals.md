# HTTP Fundamentals

**HTTP** (HyperText Transfer Protocol) is the request/response protocol of the web. A client sends a **request**; the server returns a **response**. Each exchange is **stateless**: the server does not remember earlier requests unless you send identifying data (cookies, tokens).

```http
GET /api/users/42 HTTP/1.1
Host: api.example.com
Accept: application/json
Authorization: Bearer eyJ...

HTTP/1.1 200 OK
Content-Type: application/json
Cache-Control: max-age=60

{"id":42,"name":"Ada"}
```

## Anatomy of a URL

```
https://user@api.example.com:8443/v1/users?role=admin&page=2#section
└─┬──┘ └──┬──┘└──────┬──────┘└─┬─┘└───┬───┘└──────┬───────┘└──┬───┘
scheme  userinfo   host      port  path        query       fragment
```

| Part | Notes |
|------|-------|
| **Origin** | scheme + host + port (`https://api.example.com:8443`): the unit of the same-origin policy |
| **Path** | identifies the resource (`/v1/users`) |
| **Query** | `key=value` pairs separated by `&`, URL-encoded |
| **Fragment** | `#section`: handled by the browser, **never sent** to the server |

```js
const url = new URL("https://api.example.com/users");
url.searchParams.set("q", "ada lovelace");     // encoded automatically
url.toString();                                 // https://api.example.com/users?q=ada+lovelace
encodeURIComponent("a&b=c");                    // for a single value
```

## Request methods

| Method | Purpose | Body | Safe | Idempotent |
|--------|---------|------|------|------------|
| `GET` | read a resource | no | yes | yes |
| `HEAD` | like GET, headers only | no | yes | yes |
| `OPTIONS` | ask what is allowed (CORS preflight) | no | yes | yes |
| `POST` | create / perform an action | yes | no | **no** |
| `PUT` | replace a resource | yes | no | yes |
| `PATCH` | partially update | yes | no | not necessarily |
| `DELETE` | remove | optional | no | yes |

- **Safe**: does not change server state (can be cached, prefetched)
- **Idempotent**: repeating gives the same server state as doing it once, so it is **safe to retry** after a network error

`POST` retries can create duplicates: use **idempotency keys** (`Idempotency-Key` header) for payments and orders.

## Status codes

| Class | Meaning | Common codes |
|-------|---------|--------------|
| **1xx** | informational | `101 Switching Protocols` (WebSocket upgrade) |
| **2xx** | success | `200 OK`, `201 Created`, `202 Accepted`, `204 No Content`, `206 Partial Content` |
| **3xx** | redirect / cache | `301 Moved Permanently`, `302 Found`, `303 See Other`, `304 Not Modified`, `307 Temporary Redirect`, `308 Permanent Redirect` |
| **4xx** | client error | `400 Bad Request`, `401 Unauthorized` (unauthenticated), `403 Forbidden` (not allowed), `404 Not Found`, `405 Method Not Allowed`, `408 Request Timeout`, `409 Conflict`, `410 Gone`, `413 Payload Too Large`, `415 Unsupported Media Type`, `422 Unprocessable Content`, `429 Too Many Requests` |
| **5xx** | server error | `500 Internal Server Error`, `502 Bad Gateway`, `503 Service Unavailable`, `504 Gateway Timeout` |

How clients usually react:

| Status | Reaction |
|--------|----------|
| 400 / 422 | show validation errors; do not retry |
| 401 | refresh the token or send the user to log in |
| 403 | explain lack of permission; do not retry |
| 404 | "not found" state |
| 409 | conflict: reload or merge |
| 429 | back off; honor `Retry-After` |
| 500 / 502 / 503 / 504 | may retry **idempotent** requests with backoff |

Redirect semantics: `301`/`302` may change `POST` to `GET` in clients; `307`/`308` **preserve** method and body; `303` forces `GET`.

## Headers (overview)

Metadata about the message: `Content-Type`, `Accept`, `Authorization`, `Cache-Control`, `Set-Cookie`, `Location`... See [Headers and CORS](./03_headers-and-cors.md).

## Bodies and content types

| `Content-Type` | Body |
|----------------|------|
| `application/json` | JSON text |
| `application/x-www-form-urlencoded` | `a=1&b=2` |
| `multipart/form-data; boundary=...` | fields and files |
| `text/plain`, `text/html` | text |
| `application/octet-stream`, `image/png`, ... | binary |
| `text/event-stream` | Server-Sent Events |

## Content negotiation

The client states preferences; the server picks.

```http
Accept: application/json
Accept-Language: en-GB, en;q=0.8
Accept-Encoding: gzip, br, zstd
```

The server answers with `Content-Type`, `Content-Language`, `Content-Encoding`, and `Vary` to say which request headers affected the response (important for caches).

## HTTPS and TLS

HTTPS = HTTP over **TLS**: encryption, integrity, server authentication via certificates.

- Required for modern APIs (service workers, geolocation, clipboard, HTTP/2 in browsers...)
- Use **HSTS** (`Strict-Transport-Security`) to force HTTPS
- Never send secrets over plain HTTP
- Mixed content (HTTP resources on an HTTPS page) is blocked

## HTTP versions

| Version | Highlights |
|---------|-----------|
| **HTTP/1.1** | text protocol, keep-alive, one request at a time per connection (browsers open ~6 per host) |
| **HTTP/2** | binary, **multiplexing** many requests on one connection, header compression, server push (deprecated) |
| **HTTP/3** | HTTP over **QUIC** (UDP): faster handshakes, no TCP head-of-line blocking, better on mobile/lossy networks |

You usually get the best available version automatically; design for fewer, cacheable requests rather than domain-sharding hacks.

## What happens when you request a URL

1. **DNS** lookup: host name to IP
2. **Connect**: TCP handshake (or QUIC)
3. **TLS** handshake (HTTPS)
4. Send the **request**
5. Server processes (may hit databases, other services)
6. **Response** streams back (headers first, then body)
7. Browser parses, caches, renders; connection may be kept alive for reuse

Tools to inspect: DevTools **Network** panel (Timing tab: DNS, Connect, TLS, TTFB, content download), `curl -v`, `curl -I`.

```bash
curl -i https://api.example.com/users/42
curl -X POST https://api.example.com/users -H "Content-Type: application/json" -d '{"name":"Ada"}'
```

## Caching basics

Caching avoids requests entirely or makes them cheap.

| Header | Meaning |
|--------|---------|
| `Cache-Control: max-age=3600` | fresh for 1 hour: no request needed |
| `Cache-Control: no-cache` | may store, but **revalidate** before use |
| `Cache-Control: no-store` | do not store (sensitive data) |
| `Cache-Control: public` / `private` | shared caches (CDN) allowed / browser only |
| `Cache-Control: immutable` | will never change (hashed assets) |
| `Cache-Control: stale-while-revalidate=60` | serve stale while refreshing in the background |
| `ETag: "abc"` + `If-None-Match: "abc"` | validator: server replies `304 Not Modified` with no body |
| `Last-Modified` + `If-Modified-Since` | time-based validator |
| `Vary: Accept-Encoding, Origin` | cache key includes these request headers |

Typical strategy:

- Hashed static assets (`app.3f9a1c.js`): `Cache-Control: public, max-age=31536000, immutable`
- HTML and API data: `no-cache` (revalidate) or short `max-age` plus `ETag`
- Personal data: `private` or `no-store`

## REST conventions

| Operation | Request | Typical success |
|-----------|---------|-----------------|
| List | `GET /users?page=2` | `200` + array |
| Read | `GET /users/42` | `200` |
| Create | `POST /users` | `201` + `Location: /users/43` |
| Replace | `PUT /users/42` | `200` / `204` |
| Update | `PATCH /users/42` | `200` / `204` |
| Delete | `DELETE /users/42` | `204` |

Resources are nouns in paths, methods are verbs, status codes carry outcomes, and responses are usually JSON. Alternatives: **GraphQL** (one endpoint, client-chosen fields), **gRPC / gRPC-web**, **JSON-RPC**, **tRPC**.

## Same-origin policy (preview)

Browsers restrict how a page can read responses from **other origins**. Servers opt in with **CORS** headers (see the headers and CORS file).

## Cookies and authentication (preview)

| Approach | How |
|----------|-----|
| **Session cookie** | server sets `Set-Cookie`; browser sends it automatically (`HttpOnly`, `Secure`, `SameSite`) |
| **Bearer token** | `Authorization: Bearer <token>` sent by your code |
| **API key** | header or query (keep out of URLs when possible) |
| **OAuth 2 / OIDC** | delegated login, short-lived access tokens |
| **Basic auth** | base64 `user:pass`, only over HTTPS |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `GET` for actions with side effects | Prefetchers/crawlers trigger them, caches repeat them | `POST`/`PUT`/`DELETE` |
| Retrying non-idempotent requests blindly | Duplicate orders/charges | Idempotency keys |
| Treating `401` and `403` the same | Wrong recovery | `401`: authenticate, `403`: not allowed |
| Putting secrets in URLs | Logged, cached, leaked via `Referer` | Headers or bodies |
| Building query strings by hand | Encoding bugs | `URL` / `URLSearchParams` |
| Ignoring caching headers | Slow and costly | Set `Cache-Control` and validators |
| Plain HTTP | Interception, blocked features | HTTPS + HSTS |
| Returning `200` for errors with an error body | Breaks tooling/clients | Proper status codes |

## Key takeaways

- HTTP is stateless request/response: method, URL, headers, optional body, status code
- Know safe vs idempotent methods and the status code classes
- Use HTTPS, and let caching headers (`Cache-Control`, `ETag`) do heavy lifting
- Build URLs with `URL`; design APIs with resources, methods and proper status codes

**Next:** [Fetch](./02_fetch.md)
