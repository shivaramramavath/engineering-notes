# CORS (Cross-Origin Resource Sharing)

The browser mechanism that decides whether JavaScript running on one origin is allowed to make a request to a different origin. This file covers why it exists and how to configure it correctly, rather than by trial-and-error header copying.

## The same-origin policy: the default is "no"

Browsers block a page's JavaScript from reading responses from a different **origin** by default — where "origin" means the exact combination of scheme, domain, and port:

```
https://app.example.com:443
   scheme  domain          port
```

```
https://app.example.com   vs   https://api.example.com    → different origins (different subdomain)
https://example.com        vs   http://example.com           → different origins (different scheme)
https://example.com:443     vs   https://example.com:8080       → different origins (different port)
```

This exists to prevent a malicious site from silently making authenticated requests to another site on a visitor's behalf (e.g. reading their bank account data), using cookies the browser would otherwise attach automatically.

**Important nuance:** the same-origin policy blocks the _browser's JavaScript from reading the response_ — it does **not** stop the request from being sent, and does **not** protect a server that has no other defenses. CORS is a browser-enforced client-side protection, not a server-side security mechanism; a request from `curl` or a mobile app entirely ignores CORS.

---

## What CORS actually does

CORS is the mechanism by which a **server** can explicitly opt in to allowing specific cross-origin requests, by sending the right response headers — the browser checks these headers and decides whether to let the calling JavaScript see the response.

```http
Access-Control-Allow-Origin: https://app.example.com
```

Without this header (or with an origin that doesn't match), the browser blocks the frontend JavaScript from reading the response, even if the request itself succeeded and the server returned real data — you'll see this show up as a network error in the browser console, not a normal error response.

---

## Simple requests vs preflighted requests

Not every cross-origin request behaves the same way — the browser distinguishes "simple" requests from ones that need a preflight check first.

### Simple requests

A `GET`/`HEAD`/`POST` request using only a small set of allowed headers (and a few allowed `Content-Type` values, like `application/x-www-form-urlencoded` or `text/plain`) is sent directly — the browser includes the `Origin` header, and checks the response's `Access-Control-Allow-Origin` afterward.

### Preflighted requests

Anything else — `PUT`/`PATCH`/`DELETE`, a custom header, or `Content-Type: application/json` — triggers a **preflight**: the browser first sends an `OPTIONS` request asking permission, before sending the actual request at all.

```http
OPTIONS /api/users HTTP/1.1
Origin: https://app.example.com
Access-Control-Request-Method: POST
Access-Control-Request-Headers: Content-Type, Authorization
```

```http
HTTP/1.1 204 No Content
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 86400
```

Only if the server's preflight response permits the actual method/headers does the browser send the real request. `Access-Control-Max-Age` lets the browser cache this preflight result, avoiding a repeated `OPTIONS` round-trip on every single request.

This is exactly why a JSON-based API (`Content-Type: application/json`, which is nearly universal) triggers a preflight on almost every non-`GET` request — worth knowing when debugging why every `POST` seems to fire two network requests.

---

## Configuring CORS in Express

```bash
npm install cors
```

```js
import cors from "cors";

app.use(
  cors({
    origin: "https://app.example.com",
    methods: ["GET", "POST", "PUT", "DELETE"],
    allowedHeaders: ["Content-Type", "Authorization"],
    credentials: true,
  }),
);
```

### Multiple allowed origins

```js
const allowedOrigins = ["https://app.example.com", "https://admin.example.com"];

app.use(
  cors({
    origin: (origin, callback) => {
      if (!origin || allowedOrigins.includes(origin)) {
        callback(null, true);
      } else {
        callback(new Error("Not allowed by CORS"));
      }
    },
    credentials: true,
  }),
);
```

A function for `origin` lets you validate against a list (or a dynamic rule, like matching a subdomain pattern) rather than a single hardcoded string.

### `credentials: true` and cookies

```js
app.use(
  cors({
    origin: "https://app.example.com", // must be an EXACT origin — cannot be "*"
    credentials: true,
  }),
);
```

```js
// client
fetch("https://api.example.com/data", { credentials: "include" });
```

As covered in `03-cookies.md`, allowing credentialed (cookie-carrying) cross-origin requests requires an exact origin on the server side — the wildcard `origin: "*"` is explicitly disallowed by the CORS spec whenever `credentials: true` is set, since allowing "any site, with cookies" would defeat the entire point of the same-origin policy.

---

## The wildcard `*`: when it's actually fine

```js
app.use(cors({ origin: "*" }));
```

Fine for a **public, non-authenticated** API — one with no cookies, no user-specific data, nothing that would be harmful for any website to fetch on a visitor's behalf. Not appropriate for anything involving login, cookies, or per-user data.

---

## Debugging a CORS error

```
Access to fetch at 'https://api.example.com/data' from origin 'https://app.example.com'
has been blocked by CORS policy: No 'Access-Control-Allow-Origin' header is present
on the requested resource.
```

This error appears in the **browser console**, on the **client** side — the server, from its own logs, usually looks like it handled the request completely normally (or didn't receive it at all, if it was blocked at the preflight stage). Common causes, roughly in order of likelihood:

1. The server isn't sending `Access-Control-Allow-Origin` at all — CORS middleware isn't configured, or isn't running before the route handler
2. The allowed origin doesn't exactly match the requesting origin (a trailing slash, `http` vs `https`, or a missing subdomain all count as different)
3. `credentials: true` on the client without a matching `credentials: true` + exact origin on the server
4. The preflight `OPTIONS` request itself is being blocked or mishandled — e.g. by an auth middleware that runs before CORS and rejects the unauthenticated `OPTIONS` request before it ever gets a chance to succeed

### Middleware order matters

```js
app.use(
  cors({
    /* ... */
  }),
); // CORS first
app.use(authMiddleware); // then auth
```

If auth middleware runs before CORS, an unauthenticated preflight `OPTIONS` request can get rejected by the auth check before the CORS headers are ever added — breaking every cross-origin request that needed a preflight, for reasons that look, from the error message alone, like a CORS misconfiguration rather than an ordering bug.

## Common mistakes

- **Treating a CORS error as a server-side bug** without checking the actual response headers — it's almost always a header configuration or origin-matching issue, not a broken server.
- **Using `origin: "*"` alongside `credentials: true`** — explicitly disallowed by browsers; always use an exact origin (or a validating function) when cookies/credentials are involved.
- **Putting auth middleware before CORS middleware** — blocks the preflight `OPTIONS` request before CORS headers get attached.
- **Assuming CORS is a security feature protecting your server** — it's a browser-side protection for the _user_; a server with no other auth/validation is just as exposed to non-browser clients regardless of CORS configuration.

## Quick summary

- CORS lets a server explicitly permit specific cross-origin requests; without it, the same-origin policy blocks the browser from reading the response (not from sending the request)
- Non-`GET`/JSON requests typically trigger a preflight `OPTIONS` check before the real request is sent
- `credentials: true` requires an exact origin on the server — never combine it with a wildcard
- CORS errors show up client-side in the browser console; the actual fix is almost always a server-side header/origin/middleware-order issue
- CORS is not a substitute for real authentication/authorization — it only affects browser-based JavaScript callers

## Next

**`05-caching-and-compression.md`** covers the headers that control how responses are cached and compressed over the network.
