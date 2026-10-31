# CORS and Security Headers

Two related browser-enforced layers. **CORS** controls which *other websites'* JavaScript may call your API from a browser. **Security headers** tell the browser how to treat your pages and responses (no framing, no sniffing, restricted script sources, limited referrers). Both are misconfigured constantly, and both matter most for cookie-based flows like the refresh token.

**Prerequisites:** [00-project-setup.md](../00-setup/00-project-setup.md), [05-csrf.md](../04-sessions/05-csrf.md)

---

## CORS in one page

### Same-origin policy

An **origin** is `scheme + host + port`. By default a page at `https://app.example.com` can send requests to `https://api.example.com`, but the browser **blocks its JavaScript from reading the response** unless the API opts in. The block is on *reading*, not on *sending*. For "simple" requests the request still reaches your server.

```text
https://app.example.com  →  https://api.example.com   different origin (different host)
http://localhost:5173    →  http://localhost:4000      different origin (different port)
```

(Don't confuse origin with **site**: same-site is broader. CORS uses origin; SameSite cookies use site: see [02-cookie-sessions.md](../04-sessions/02-cookie-sessions.md).)

### The opt-in headers

```http
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Credentials: true
Access-Control-Allow-Methods: GET, POST, PATCH, DELETE
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 600
Vary: Origin
```

### Preflight

Requests that aren't "simple" (a method other than GET/HEAD/POST, a custom header like `Authorization`, or `Content-Type: application/json`) are preceded by an automatic `OPTIONS` request:

```text
Browser                                  API
  │ OPTIONS /api/orders                   │
  │ Origin: https://app.example.com       │
  │ Access-Control-Request-Method: POST   │
  │ Access-Control-Request-Headers: authorization, content-type
  │ ────────────────────────────────────► │
  │ ◄──── 204 + Allow-Origin/Methods/Headers
  │ POST /api/orders (real request)       │
  │ ────────────────────────────────────► │
```

If the preflight fails, the real request is **never sent**. That is why requiring JSON/`Authorization` is a useful extra CSRF layer ([05-csrf.md](../04-sessions/05-csrf.md)).

---

## Configuring it correctly

```js
import cors from "cors";

const allowed = new Set([process.env.FRONTEND_URL]);              // exact origins, from config

app.use(
  cors({
    origin(origin, cb) {
      if (!origin) return cb(null, false);                        // non-browser or same-origin: no CORS headers needed
      cb(null, allowed.has(origin));                              // exact match against the allowlist
    },
    credentials: true,                                            // allow cookies (the refresh cookie)
    methods: ["GET", "POST", "PUT", "PATCH", "DELETE"],
    allowedHeaders: ["Content-Type", "Authorization"],
    maxAge: 600,                                                  // cache preflight for 10 min
  })
);
```

Rules:

| Rule | Why |
|---|---|
| **Exact allowlist** of origins | Wildcards and "contains" checks let attacker origins in |
| **Never `*` together with credentials** | Browsers refuse it, and "fixing" it by echoing the request's `Origin` is the same as `*` |
| **Never reflect `Origin` blindly** | Any site can then read authenticated responses |
| Don't allow the `null` origin | Sandboxed iframes and local files send it; attackers can produce it |
| Compare exactly: `origin === allowed`, not `endsWith`/`includes`/regex without anchors | `https://app.example.com.evil.com` passes sloppy checks |
| Send `Vary: Origin` when the response varies by origin | Caches must not serve one origin's headers to another (the `cors` package handles this) |
| Allow only needed methods/headers | Smaller attack surface |
| Keep it before your routes | Preflights must be answered before auth middleware (an `OPTIONS` has no token) |

```js
// ❌ All three are vulnerable
cors({ origin: "*", credentials: true });
cors({ origin: true, credentials: true });                        // reflects any Origin
cors({ origin: /example\.com/, credentials: true });              // matches evil-example.com.attacker.io
```

### What CORS is *not*

- **Not authentication or authorization.** `curl`, Postman and servers ignore CORS entirely. Attackers don't use browsers.
- **Not CSRF protection by itself.** A forged cross-site `POST` form still reaches the server; CORS only stops the attacker's script reading the reply.
- **Not a way to hide an API.** It only restricts other sites' scripts in the user's browser.

Common setup by environment:

| Environment | Origin | Note |
|---|---|---|
| Development | `http://localhost:5173` | Or use the Vite dev proxy so there's no cross-origin at all |
| Production, same site | `https://app.example.com` | `SameSite=Strict` refresh cookie works |
| Same origin (SPA served by the API) | None needed | Simplest: no CORS |

---

## Security headers with Helmet

```bash
npm i helmet
```

```js
import helmet from "helmet";
app.disable("x-powered-by");           // Helmet also removes it
app.use(helmet());
```

Helmet sets a sensible baseline. Roughly:

| Header | Purpose |
|---|---|
| `Strict-Transport-Security` | Force HTTPS for a long time after the first HTTPS visit |
| `X-Content-Type-Options: nosniff` | Don't guess MIME types (blocks some script-injection tricks) |
| `Content-Security-Policy` | Restrict where scripts/styles/frames/etc. may load from |
| `Referrer-Policy` | Limit what URL information leaks in the `Referer` header |
| `X-Frame-Options` / CSP `frame-ancestors` | Prevent clickjacking via framing |
| `Cross-Origin-Opener-Policy`, `Cross-Origin-Resource-Policy` | Isolate browsing contexts / restrict who can embed your resources |
| `X-XSS-Protection: 0` | Disables the buggy legacy XSS auditor |

Exact defaults vary by Helmet version, so read its docs for the version you use.

### Where headers should be set

```text
SPA (React) static files   → set at the host that serves HTML: Nginx, Vercel/Netlify config, Cloudflare, S3+CloudFront
JSON API                   → Helmet in Express (CSP matters little for JSON, but nosniff, HSTS, no-store, Referrer-Policy do)
```

A **CSP protects HTML pages**, so it belongs where your SPA's `index.html` is served. Setting a strict CSP only on the API does nothing for the frontend.

---

## HSTS

```http
Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
```

- After one HTTPS visit the browser refuses plain HTTP to your domain for `max-age` seconds, defeating SSL-stripping downgrade attacks.
- **Only send over HTTPS**, and make sure every subdomain supports HTTPS before adding `includeSubDomains`.
- `preload` submits the domain to browsers' built-in lists. It is hard to undo, so start with a short `max-age` (like a day), increase gradually, and only preload when you're sure.

---

## Content Security Policy (CSP)

CSP is the strongest defense-in-depth layer against XSS: even if an attacker injects markup, the browser refuses to run scripts from unexpected places.

A reasonable starting policy for a React SPA with a separate API:

```http
Content-Security-Policy:
  default-src 'self';
  script-src 'self';
  style-src 'self' 'unsafe-inline';
  img-src 'self' data: https:;
  font-src 'self';
  connect-src 'self' https://api.example.com;
  frame-ancestors 'none';
  base-uri 'self';
  form-action 'self';
  object-src 'none'
```

| Directive | Effect |
|---|---|
| `default-src 'self'` | Fallback: only your own origin |
| `script-src 'self'` | **No inline scripts, no `eval`, no third-party scripts**; the key XSS control |
| `connect-src` | Where `fetch`/XHR/WebSocket may connect: list your API (and the OAuth provider's origin if the browser calls it) |
| `frame-ancestors 'none'` | Nobody may frame the app (clickjacking); replaces `X-Frame-Options` |
| `form-action 'self'` | Forms can only post to you |
| `base-uri 'self'` | Blocks `<base>` injection |
| `object-src 'none'` | No plugins |

Rollout strategy:

1. Start with **`Content-Security-Policy-Report-Only`** plus a `report-to` / `report-uri` endpoint to see what would break.
2. Fix violations (move inline scripts to files, or use **nonces/hashes** for the few you need).
3. Switch to enforcing.

Avoid `'unsafe-inline'` and `'unsafe-eval'` for `script-src`; they remove most of CSP's value. Inline `style` is a smaller risk and often tolerated initially. Tell Helmet your values via `contentSecurityPolicy: { directives: { ... } }` when you serve HTML from Express.

CSP does not replace output encoding; it limits the damage when encoding fails. Also see [02-threat-model.md](../01-fundamentals/02-threat-model.md).

---

## Referrer-Policy

URLs often contain secrets (reset/verification tokens, OAuth `code`/`state`). The `Referer` header would send the full URL to any third-party resource on the page.

```http
Referrer-Policy: no-referrer
```

- Use `no-referrer` (or at least `strict-origin-when-cross-origin`, the modern browser default) globally.
- Auth landing pages (reset, verify, OAuth callback) should be strictly `no-referrer` and free of third-party scripts ([09-reset-tokens.md](../03-tokens/09-reset-tokens.md), [05-oauth-security.md](../06-oauth/05-oauth-security.md)).

---

## Cache headers on auth responses

Tokens and personal data must not be stored by browsers or intermediaries:

```js
app.use("/api/auth", (req, res, next) => {
  res.set("Cache-Control", "no-store");
  next();
});
```

Apply `no-store` to login, refresh, MFA, session-list and any response that contains tokens or secrets (a TOTP enrollment QR, recovery codes).

---

## Other headers worth knowing

| Header | Use |
|---|---|
| `Permissions-Policy` | Disable unused browser features (camera, microphone, geolocation); keep what WebAuthn needs |
| `Cross-Origin-Opener-Policy: same-origin` | Isolates your window from cross-origin popups (helps against some cross-window attacks; may break OAuth popup flows, so test) |
| `X-Content-Type-Options: nosniff` | Always |
| `Content-Type` correct on every response | `application/json` for APIs; avoid HTML in error bodies |
| `Set-Cookie` flags | `HttpOnly; Secure; SameSite` ([02-cookie-sessions.md](../04-sessions/02-cookie-sessions.md)) |
| `Clear-Site-Data` | Optionally on logout to wipe cached data |

---

## Verify what you ship

```bash
curl -sI https://api.example.com/api/health | grep -iE "strict-transport|content-security|x-content-type|referrer|cache-control|x-powered-by"
```

Browser DevTools → Network → Response Headers; the Console reports CSP violations and CORS errors. Online scanners (securityheaders.com, Mozilla Observatory) grade a deployed site.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| `origin: true` / `*` with credentials | Any website can read authenticated responses |
| Regex/`includes` origin check | Lookalike domains accepted |
| CORS placed after auth middleware | Preflight gets `401`, browser blocks the app |
| Treating CORS as an access-control mechanism | Server-to-server callers bypass it entirely |
| Strict CSP on the API but none on the HTML host | Frontend unprotected |
| `script-src 'unsafe-inline'` | CSP no longer stops XSS |
| HSTS `includeSubDomains` before all subdomains support HTTPS | Subdomains become unreachable |
| Forgetting `Vary: Origin` behind a CDN | Wrong CORS headers served from cache |
| No `no-store` on token responses | Tokens cached by proxies or the back/forward cache |
| Allowing `Authorization`/credentials from dev origins in production | Unnecessary exposure |

## Debugging

| Browser message | Cause |
|---|---|
| `No 'Access-Control-Allow-Origin' header is present` | Origin not allowed, or the error response (5xx/401) bypassed CORS middleware |
| `The value of the 'Access-Control-Allow-Origin' header must not be the wildcard '*' when the request's credentials mode is 'include'` | Wildcard + credentials; use an explicit origin |
| `Request header field authorization is not allowed by Access-Control-Allow-Headers` | Missing in `allowedHeaders` |
| `Method PATCH is not allowed` | Missing in `methods` |
| `Response to preflight request doesn't pass access control check: It does not have HTTP ok status` | Preflight rejected by auth/other middleware; answer `OPTIONS` first |
| Cookie not sent/stored with CORS request | `credentials: "include"` on the client **and** `Allow-Credentials: true` plus exact origin on the server; check SameSite/Secure |
| `Refused to load ... because it violates the following Content Security Policy directive` | Add the specific source, or fix the violation; start with Report-Only |

---

## Quick summary

- CORS restricts which sites' scripts can *read* your API's responses; use an exact origin allowlist, never `*` or reflection with credentials. It isn't authentication or CSRF protection.
- Helmet gives a baseline; set the CSP where the HTML is served, start in Report-Only, avoid `unsafe-inline` for scripts.
- HSTS forces HTTPS (ramp up carefully); `Referrer-Policy: no-referrer` protects URL tokens; `Cache-Control: no-store` on auth responses.
- Test with `curl -I`, DevTools and header scanners.

**Next:** [04-input-validation.md](./04-input-validation.md)
