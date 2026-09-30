# Headers & Content Negotiation

Headers carry metadata alongside a request or response — everything from content type to authentication to caching instructions. Content negotiation is the specific mechanism by which a client and server agree on the _format_ of what gets exchanged.

## Common request headers

```http
GET /api/users HTTP/1.1
Host: example.com
Authorization: Bearer eyJhbGciOi...
Content-Type: application/json
Accept: application/json
User-Agent: Mozilla/5.0...
Cookie: session=abc123
```

| Header          | Purpose                                                                            |
| --------------- | ---------------------------------------------------------------------------------- |
| `Host`          | Which domain the request is for (one server can host multiple domains)             |
| `Authorization` | Credentials — a bearer token, basic auth, etc. (see `08-authentication-security/`) |
| `Content-Type`  | The format of the request **body** the client is sending                           |
| `Accept`        | The format(s) the client would like the **response** in                            |
| `User-Agent`    | Identifies the client software (browser, curl, a mobile app)                       |
| `Cookie`        | Cookies previously set by the server (`03-cookies.md`)                             |

## Common response headers

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 348
Cache-Control: no-cache
Set-Cookie: session=abc123; HttpOnly; Secure
```

| Header           | Purpose                                                                   |
| ---------------- | ------------------------------------------------------------------------- |
| `Content-Type`   | The format of the response body                                           |
| `Content-Length` | The size of the response body, in bytes                                   |
| `Cache-Control`  | Caching instructions for the client — see `05-caching-and-compression.md` |
| `Set-Cookie`     | Sets a cookie in the client's browser (`03-cookies.md`)                   |

---

## `Content-Type` vs `Accept`: two different directions

```http
POST /api/users
Content-Type: application/json     ← "the body I'm SENDING you is JSON"
Accept: application/json             ← "please send the response back as JSON"
```

`Content-Type` describes what's in **this** request/response's body; `Accept` is the client stating a preference for what it wants **back**. A server ignoring `Accept` and always returning JSON regardless is extremely common and usually fine — full content negotiation matters more for APIs genuinely supporting multiple response formats.

---

## Content negotiation

The general mechanism by which a client and server agree on a response's format, language, or encoding — each has a request header expressing a preference, and a matching response header confirming what was actually chosen.

### Format: `Accept` / `Content-Type`

```http
Accept: application/json
```

```http
Accept: text/html, application/xhtml+xml, application/xml;q=0.9, */*;q=0.8
```

Multiple acceptable types can be listed with a `q` (quality) value from 0 to 1, indicating relative preference — a server capable of returning multiple formats picks the highest-preference one it actually supports.

```js
// Express example
app.get("/users/:id", (req, res) => {
  const user = getUser(req.params.id);
  res.format({
    "application/json": () => res.json(user),
    "text/html": () => res.send(`<h1>${user.name}</h1>`),
    default: () => res.status(406).send("Not Acceptable"),
  });
});
```

### Language: `Accept-Language`

```http
Accept-Language: en-US,en;q=0.9,fr;q=0.8
```

A server supporting multiple languages picks the best match and typically confirms it with a `Content-Language` response header.

### Encoding: `Accept-Encoding`

```http
Accept-Encoding: gzip, br
```

The client states which compression formats it can decompress — directly relevant to `05-caching-and-compression.md` and the Nginx compression setup, since a server should never compress a response with a method the client doesn't support.

```http
Content-Encoding: gzip
```

The response header confirms which encoding was actually applied.

---

## Custom headers

```http
X-Request-Id: 8f14e45f-ceea-4c3a-b7c1-3c8e7a1e1234
X-RateLimit-Remaining: 42
```

Historically, non-standard headers were prefixed with `X-` by convention — modern practice has largely dropped the requirement for the prefix, but you'll still see it constantly in existing APIs and libraries. Custom headers are commonly used for request tracing (`14-logging-observability/02-correlation-id.md`), rate limit status, and API versioning (`09-api-development/02-versioning-and-pagination.md`).

---

## Reading headers in Express

```js
app.get("/api/data", (req, res) => {
  console.log(req.headers["user-agent"]);
  console.log(req.get("Authorization")); // case-insensitive convenience method

  res.set("X-Custom-Header", "value");
  res.json({ ok: true });
});
```

`req.headers` is a plain object with lowercased keys (HTTP headers are case-insensitive, but Node normalizes them to lowercase); `req.get(name)` is a convenience method that handles the case-insensitivity for you.

## Common mistakes

- **Confusing `Content-Type` and `Accept`** — one describes the current body, the other is a request for a future response format.
- **Ignoring `Accept-Encoding` and compressing anyway** — sending a client an encoding it can't decompress breaks the response entirely; always check support, or let a tool (Nginx, `compression` middleware) handle this correctly for you.
- **Assuming header names are case-sensitive** — HTTP headers are case-insensitive by spec; don't write brittle code that checks for one exact casing.
- **Manually parsing the `Accept` header's `q` values yourself** — error-prone; use a library (or Express's `res.format()`) rather than hand-rolling the parsing logic.

## Quick summary

- `Content-Type` describes the current body's format; `Accept` is the client's preference for the response's format
- Content negotiation extends to language (`Accept-Language`) and compression (`Accept-Encoding`) as well as data format
- `q` values let a client express relative preference among several acceptable options
- Custom headers (`X-Request-Id`, etc.) are commonly used for tracing, rate limiting, and versioning
- Header names are case-insensitive; Node/Express normalize them to lowercase

## Next

**`03-cookies.md`** covers `Set-Cookie`/`Cookie` in depth — one of the response/request headers introduced here, important enough to warrant its own file.
