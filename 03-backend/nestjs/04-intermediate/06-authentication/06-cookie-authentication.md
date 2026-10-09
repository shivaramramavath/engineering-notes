# Cookie Authentication

Cookies are a **transport** for credentials, not an authentication scheme by themselves. You can carry a session id ([session authentication](./07-session-authentication.md)), a JWT, or a refresh token in a cookie. Their big advantage in browsers: with the right flags, **JavaScript can't read them**, which blunts XSS token theft. Their big cost: the browser attaches them to requests **automatically**, which opens the door to **CSRF**.

Prerequisites: [Authentication architecture](./01-authentication-architecture.md), [JWT](./04-jwt.md), [refresh tokens](./05-refresh-tokens-and-logout.md).

## Cookie attributes that matter

```text
Set-Cookie: access_token=...; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=900
```

| Attribute | Purpose | Guidance |
|-----------|---------|----------|
| `HttpOnly` | Hides the cookie from `document.cookie` | **Always** for credentials |
| `Secure` | Sent only over HTTPS | **Always** in production (behind a TLS-terminating proxy, configure `trust proxy`) |
| `SameSite` | Controls cross-site sending | `Lax` or `Strict` by default; `None` only when you truly need cross-site (and then `Secure` is mandatory) |
| `Path` | URL prefix the cookie is sent to | Narrow it (refresh token: `Path=/auth`) |
| `Domain` | Which hosts receive it | **Omit** (host-only) unless you need subdomain sharing; a parent `Domain` widens exposure to every subdomain |
| `Max-Age` / `Expires` | Lifetime | Match the credential's lifetime; omit for a session cookie |
| Name prefix `__Host-` | Browser enforces `Secure`, `Path=/`, no `Domain` | Cheap hardening: `__Host-sid` |

`SameSite` values:

| Value | Cookie sent on... |
|-------|-------------------|
| `Strict` | Only same-site requests (links from other sites arrive without it, so the first navigation looks logged-out) |
| `Lax` | Same-site requests **and** top-level navigations using safe methods (GET) from other sites |
| `None` | All requests, including cross-site (requires `Secure`) |

## Setting cookies in Nest

```bash
npm i cookie-parser
npm i -D @types/cookie-parser
```

```ts
// main.ts
import cookieParser from 'cookie-parser';

app.use(cookieParser(process.env.COOKIE_SECRET));     // secret is optional; needed for signed cookies
```

(Under Fastify use `@fastify/cookie`; the Express-style `res.cookie` calls below are then replaced by its API.)

```ts
// auth.controller.ts
@Public()
@UseGuards(LocalAuthGuard)
@Post('login')
@HttpCode(200)
async login(@Request() req, @Res({ passthrough: true }) res: Response) {
  const { accessToken, refreshToken } = await this.auth.login(req.user);

  res.cookie('access_token', accessToken, {
    httpOnly: true,
    secure: process.env.NODE_ENV === 'production',
    sameSite: 'lax',
    maxAge: 15 * 60 * 1000,
    path: '/',
  });
  res.cookie('refresh_token', refreshToken, {
    httpOnly: true,
    secure: process.env.NODE_ENV === 'production',
    sameSite: 'strict',
    maxAge: 30 * 24 * 3600 * 1000,
    path: '/auth',                                      // only sent to /auth/*
  });
  return { ok: true };                                  // don't also put tokens in the body
}
```

`@Res({ passthrough: true })` lets you set cookies while still returning a value normally, so interceptors and serialization keep working. Without `passthrough`, injecting `@Res()` puts you in manual response mode ([pipeline note](../../03-core-concepts/01-request-pipeline/01-pipeline-overview-and-execution-order.md)). Express's `maxAge` is in **milliseconds** (unlike the `Max-Age` header, which is seconds).

### Logout: clear with the same attributes

```ts
@Post('logout')
@HttpCode(204)
async logout(@Req() req: Request, @Res({ passthrough: true }) res: Response) {
  await this.auth.logout(req.cookies?.refresh_token);
  res.clearCookie('access_token', { path: '/' });
  res.clearCookie('refresh_token', { path: '/auth' });
}
```

A cookie is only removed if `path` (and `domain`, if set) **match** how it was set. A mismatch silently leaves the cookie in place. Also revoke server-side; clearing the cookie only helps the cooperating client ([refresh tokens](./05-refresh-tokens-and-logout.md)).

## Reading a JWT from a cookie

With Passport:

```ts
super({
  jwtFromRequest: ExtractJwt.fromExtractors([
    (req: Request) => req?.cookies?.access_token ?? null,
    ExtractJwt.fromAuthHeaderAsBearerToken(),             // fallback for non-browser clients
  ]),
  secretOrKey: config.getOrThrow('JWT_ACCESS_SECRET'),
  algorithms: ['HS256'],
});
```

With a custom guard, read `req.cookies.access_token` instead of the `Authorization` header. If you accept **both**, understand that cookie-borne credentials need CSRF protection while header-borne ones don't, and an attacker can only exploit the cookie path.

## CSRF: the price of automatic sending

**Cross-Site Request Forgery**: a malicious page makes the victim's browser send a request to your API; the browser helpfully attaches your cookie, and the server treats it as the user's intent.

```text
evil.com page:  <form action="https://api.bank.com/transfer" method="POST"> ... auto-submit
browser:        POST https://api.bank.com/transfer  + Cookie: session=...   ← sent automatically
```

Defenses (layer them):

1. **`SameSite=Lax` or `Strict`** blocks the cookie on most cross-site subrequests and POSTs. It's a strong baseline but not complete: it doesn't protect against attacks from **same-site** origins (other subdomains, XSS on a sibling), and `Lax` still sends cookies on top-level `GET` navigations, so **never perform state changes on GET**.
2. **CSRF tokens**: the server issues an unpredictable token the client must echo in a header (`X-CSRF-Token`); the attacker's page can't read it. Variants: synchronizer token (stored server-side/in session) or **double-submit cookie** (token in a cookie and in a header, signed/bound to the session).
3. **Require a custom header** (`X-Requested-With`, or a JSON `Content-Type`) on state-changing requests: cross-site forms can't set them without a CORS preflight, which your CORS policy rejects. Useful for pure-API backends.
4. **Check `Origin`/`Referer`** on state-changing requests and reject unexpected origins.
5. **Strict CORS** (below): don't allow arbitrary origins with credentials.

Token/header-based auth (Bearer in the `Authorization` header) isn't automatically attached by browsers, so it **isn't CSRF-prone**, which is why people choose it. Details and Nest tooling choices: [CSRF](../../07-production/01-security/03-csrf.md). Note that the `csurf` package is deprecated; consult current guidance for which maintained approach to use.

## CORS with cookies

When your frontend and API are on different origins, cookies require explicit configuration on **both** sides:

```ts
// server
app.enableCors({
  origin: ['https://app.example.com'],     // explicit allow-list; NOT '*' and NOT "reflect any origin"
  credentials: true,                       // sends Access-Control-Allow-Credentials: true
});
```

```ts
// browser client
fetch('https://api.example.com/me', { credentials: 'include' });
// axios: { withCredentials: true }
```

Rules:

- With `credentials: true`, the browser **rejects** `Access-Control-Allow-Origin: *`. You must name origins.
- Reflecting any `Origin` back with credentials enabled is a vulnerability (any site could make credentialed requests and read responses).
- **Same-site vs same-origin:** `app.example.com` and `api.example.com` are *same-site* (so `SameSite=Lax` cookies work) but *cross-origin* (so CORS still applies). Different registrable domains (`myapp.com` + `myapi.io`) are *cross-site*: you'd need `SameSite=None; Secure`, which is also affected by browsers' third-party cookie restrictions and is fragile. Prefer putting the API under the same site (a subdomain or a reverse-proxied `/api` path) when using cookies ([security headers and CORS](../../07-production/01-security/02-http-security-headers-and-cors.md)).

## Other cookie details

- **Signed cookies** (`cookieParser(secret)` + `res.cookie(name, value, { signed: true })`, read from `req.signedCookies`) add **integrity**: tampering is detected. They don't hide the value (not encrypted). A random session id doesn't need signing; a JWT is already signed.
- **Size:** browsers cap cookies at roughly 4 KB each and limit counts per domain; cookies travel on every matching request, so keep them small (session id or compact JWT).
- **Secure behind proxies:** if TLS terminates at a load balancer, Express sees plain HTTP and may refuse to set `Secure` cookies. Set `app.set('trust proxy', 1)` (or the right hop count) so `req.secure` reflects `X-Forwarded-Proto` ([reverse proxy](../../07-production/04-deployment/04-reverse-proxy-and-nginx.md)).
- **Local development:** `Secure` cookies work on `localhost` in modern browsers, but a non-HTTPS custom host won't; toggle by environment, as in the example.
- **Domain sharing:** setting `Domain=example.com` shares the cookie with every subdomain, including untrusted or compromised ones. Prefer host-only cookies.
- **Privacy/consent:** authentication cookies are typically "strictly necessary", but check your jurisdiction's rules for any non-essential cookies.

## Cookies vs Authorization header

| | `httpOnly` cookie | `Authorization: Bearer` (token in JS memory/storage) |
|-|-------------------|------------------------------------------------------|
| XSS can steal the credential | **No** (but XSS can still make authenticated requests) | Yes, if stored in JS-accessible storage |
| CSRF | **Yes: must defend** | No |
| Works for non-browser clients | Awkward | Natural |
| Cross-origin setups | CORS credentials + SameSite complexity | Simple |
| Typical fit | Browser apps on the same site | Mobile, public APIs, microservices |

Note that XSS defeats both models in practice (an attacker's script can call your API as the user); `httpOnly` limits **exfiltration** of the credential, not abuse during the session. Fix XSS with output encoding and a Content Security Policy.

## Testing

- Assert cookie attributes on login: `Set-Cookie` contains `HttpOnly`, `SameSite`, correct `Path`, and `Secure` in production mode.
- Logout clears cookies with matching `Path`.
- Authenticated requests succeed using `request.agent(app.getHttpServer())`, which persists cookies across calls ([E2E testing](../01-testing/06-e2e-testing.md)).
- State-changing request without the CSRF token/header is rejected; cross-origin request from a non-allowed origin gets no CORS approval.

```ts
const agent = request.agent(app.getHttpServer());
await agent.post('/auth/login').send({ email, password }).expect(200);
await agent.get('/me').expect(200);
```

## Common mistakes

- **Cookie auth without CSRF defenses.**
- **Missing `HttpOnly`/`Secure`**, or leaving `SameSite=None` unnecessarily.
- **`Access-Control-Allow-Origin: *` with credentials** (browser blocks it) or **reflecting any origin** (vulnerable).
- **State changes on GET.**
- **Clearing cookies with different `path`/`domain`**, leaving them set.
- **Widening `Domain`** to share across subdomains without need.
- **Forgetting `trust proxy`**, so `Secure` cookies aren't set behind a load balancer.
- **Returning tokens in the body *and* cookies**, then storing the body copy in `localStorage`.
- **Putting large payloads in cookies.**
- **Using `@Res()` without `passthrough`** and breaking interceptors/serialization.

## Debugging

- Cookie not set: `Secure` over plain HTTP, missing `credentials: 'include'` on the client, or the browser rejected `SameSite=None` without `Secure`.
- Cookie set but not sent: `Path` mismatch, cross-site request with `Lax`/`Strict`, `Domain` mismatch, or `credentials` not included.
- CORS error with credentials: allowed origin must be explicit and `credentials: true` set; check the preflight response.
- Works in Postman, fails in the browser: SameSite/CORS rules apply only to browsers.
- `req.cookies` undefined: `cookie-parser` isn't registered (or registered after the route in a way that skips it).
- Use the browser dev tools **Application → Cookies** and the network panel's response headers to see attributes and rejection reasons.

## Quick Summary

- Cookies are a transport: `HttpOnly` + `Secure` + `SameSite` (+ narrow `Path`, no `Domain`) make them safe against token theft, but automatic sending requires **CSRF defense**.
- Set them with `res.cookie` using `@Res({ passthrough: true })`; clear them with identical `path`/`domain`.
- CSRF defenses: SameSite, CSRF tokens (or custom-header requirement), Origin checks, no state changes on GET.
- CORS with credentials needs explicit origins on the server and `credentials: 'include'` on the client; prefer same-site deployment.
- Configure `trust proxy` behind load balancers; keep cookies small; `httpOnly` limits theft but doesn't stop XSS abuse.

## Next

[Session authentication →](./07-session-authentication.md)
