# Cookies

A small piece of data a server asks a browser to store and automatically resend with future requests to the same site. Cookies are how a stateless protocol (HTTP) supports stateful things like login sessions.

## Setting a cookie

```http
HTTP/1.1 200 OK
Set-Cookie: session=abc123; HttpOnly; Secure; SameSite=Lax; Max-Age=86400; Path=/
```

```js
// Express
res.cookie("session", "abc123", {
  httpOnly: true,
  secure: true,
  sameSite: "lax",
  maxAge: 24 * 60 * 60 * 1000, // ms
  path: "/",
});
```

Once set, the browser automatically includes it on every subsequent request to the matching domain/path — you never manually resend it from the client.

```http
GET /dashboard HTTP/1.1
Cookie: session=abc123
```

---

## Cookie attributes, one at a time

| Attribute          | Purpose                                                                                          |
| ------------------ | ------------------------------------------------------------------------------------------------ |
| `httpOnly`         | Blocks JavaScript (`document.cookie`) from reading this cookie — mitigates XSS-based token theft |
| `secure`           | Only sent over HTTPS                                                                             |
| `sameSite`         | Controls whether the cookie is sent on cross-site requests                                       |
| `maxAge`/`expires` | How long the cookie persists; omitted means a session cookie (cleared when the browser closes)   |
| `domain`           | Which host(s) receive the cookie                                                                 |
| `path`             | Which URL paths receive the cookie                                                               |

### `httpOnly` — almost always `true` for auth cookies

```js
res.cookie("session", token, { httpOnly: true });
```

Without `httpOnly`, any JavaScript running on the page — including injected script from an XSS vulnerability (`08-authentication-security/05-common-vulnerabilities.md`) — can read the cookie directly via `document.cookie`. Setting it prevents that entirely; the browser still sends the cookie with requests, but no script on the page can read its value.

### `secure` — required for `sameSite: "none"`, recommended everywhere in production

```js
res.cookie("session", token, { secure: true });
```

Restricts the cookie to HTTPS connections only. Note that `secure: true` over plain HTTP (e.g. `localhost` in local dev) means the cookie **won't be sent at all** — a common source of "why isn't my cookie showing up" confusion locally.

### `sameSite` — the three values

```js
res.cookie("session", token, { sameSite: "strict" }); // never sent cross-site
res.cookie("session", token, { sameSite: "lax" }); // sent on top-level navigation, not on cross-site fetch/XHR
res.cookie("session", token, { sameSite: "none" }); // sent on all cross-site requests — REQUIRES secure: true
```

| Value                              | Cross-site behavior                                                                          | Typical use                                                                                     |
| ---------------------------------- | -------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| `strict`                           | Never sent cross-site, even clicking a link from another site                                | Highest security, can break legitimate cross-site navigation flows                              |
| `lax` (default in modern browsers) | Sent on top-level navigation (clicking a link), not on background requests from another site | A reasonable default for most same-site apps                                                    |
| `none`                             | Sent on every cross-site request                                                             | Needed when your frontend and API are on different domains — must be paired with `secure: true` |

**The rule that trips people up constantly:** `SameSite=None` is rejected by browsers unless `Secure` is also set — you cannot have a cross-site cookie over plain HTTP.

---

## Local development vs production

This is where cookie configuration most often breaks in practice, since local dev is usually HTTP and same-origin, while production is usually HTTPS and sometimes cross-origin (a separate frontend and API domain).

```js
const isProduction = process.env.NODE_ENV === "production";

res.cookie("session", token, {
  httpOnly: true,
  secure: isProduction, // false on local HTTP, true in production HTTPS
  sameSite: isProduction ? "none" : "lax", // "none" only needed if frontend/API are cross-origin in prod
  maxAge: 7 * 24 * 60 * 60 * 1000,
});
```

|            | Local dev                              | Production                                 |
| ---------- | -------------------------------------- | ------------------------------------------ |
| Protocol   | Usually HTTP                           | HTTPS                                      |
| `secure`   | `false`                                | `true`                                     |
| `sameSite` | `lax` (same-origin is typical locally) | `none` if cross-origin, `lax` if same-site |

If you're behind a reverse proxy (see the Nginx docs' `01-reverse-proxy-basics.md`), remember Express needs `app.set("trust proxy", 1)` to correctly recognize the connection as secure — without it, `req.secure` can be wrong even when the original client connection genuinely was HTTPS.

---

## Cross-origin requests need `credentials: true` on both sides

Cookies aren't sent cross-origin by default, even with the right `sameSite` value, unless the request explicitly opts in:

```js
// client
fetch("https://api.example.com/data", { credentials: "include" });
```

```js
// server (CORS — see 04-cors.md)
app.use(
  cors({
    origin: "https://app.example.com", // must be an exact origin, not "*"
    credentials: true,
  }),
);
```

`credentials: true` on the server cannot be combined with a wildcard `origin: "*"` — CORS requires naming the exact allowed origin whenever credentials (cookies) are involved.

---

## Reading cookies in Express

```bash
npm install cookie-parser
```

```js
import cookieParser from "cookie-parser";
app.use(cookieParser());

app.get("/dashboard", (req, res) => {
  console.log(req.cookies.session);
});
```

Without `cookie-parser` (or equivalent), `req.headers.cookie` is just a raw, unparsed string — the middleware parses it into the convenient `req.cookies` object.

---

## Deleting a cookie

```js
res.clearCookie("session", { path: "/" });
```

Under the hood, this sets the same cookie with an expiration date in the past, telling the browser to remove it — the attributes you pass (particularly `path`) must match how the cookie was originally set, or the browser won't recognize it as the same cookie to clear.

## Common mistakes

- **`secure: true` locally over HTTP** — the cookie silently isn't sent at all; either use `secure: isProduction` or test locally over HTTPS.
- **`sameSite: "none"` without `secure: true`** — rejected outright by modern browsers.
- **Forgetting `credentials: true`/`credentials: "include"` on both client and server** for a cross-origin setup — cookies simply won't be included, even with correct `sameSite`/`secure` values.
- **Forgetting `app.set("trust proxy", 1)`** behind a reverse proxy — breaks `secure`-dependent cookie behavior in production.
- **Storing sensitive data directly in a cookie's value, unsigned** — a cookie's value is visible to the user (though not readable by JS if `httpOnly`); store a session ID or signed/encrypted token, not raw sensitive data.

## Quick summary

- `httpOnly` blocks JS from reading a cookie (XSS mitigation); `secure` restricts it to HTTPS; `sameSite` controls cross-site sending
- `SameSite=None` always requires `Secure` — no exceptions, enforced by the browser
- Local dev (HTTP, usually same-origin) and production (HTTPS, sometimes cross-origin) typically need different `secure`/`sameSite` values — branch on `NODE_ENV`
- A cross-origin cookie setup needs `credentials: true`/`"include"` on **both** the server's CORS config and the client's fetch call
- `app.set("trust proxy", 1)` is required behind a reverse proxy for secure-cookie behavior to work correctly

## Next

**`04-cors.md`** covers the browser mechanism that decides whether a cross-origin request — cookie-carrying or not — is allowed at all.
