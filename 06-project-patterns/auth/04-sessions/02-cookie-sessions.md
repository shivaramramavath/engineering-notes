# Cookie Sessions

The classic server-rendered-app model: the browser holds one opaque cookie, the server holds all session state, and there are no JWTs. It is simpler than the hybrid design and often the right choice for same-site web apps. This note shows it with `express-session`, and doubles as the **reference for cookie attributes** used everywhere else.

**Prerequisites:** [00-session-basics.md](./00-session-basics.md), [01-redis-sessions.md](./01-redis-sessions.md)

---

## How it works

```text
POST /login (email, password)
      │
      ▼
Server: verify password → create session in Redis → Set-Cookie: sid=...
      │
      ▼
Browser sends Cookie: sid=... on every request automatically
      │
      ▼
Server: lookup sid in Redis → req.session.userId → authorized
```

No access token, no refresh flow, no client-side token handling. The cookie *is* the credential, so its attributes and CSRF defenses matter a lot.

---

## express-session + Redis

```bash
npm i express-session connect-redis
```

```js
import session from "express-session";
import { RedisStore } from "connect-redis";   // v8+ named export; older versions use a default export

app.set("trust proxy", 1); // required behind a proxy/load balancer so `secure` cookies work

app.use(
  session({
    name: "sid",                                   // don't leave the default "connect.sid"
    secret: process.env.SESSION_SECRET,            // signs the cookie value
    store: new RedisStore({ client: redis, prefix: "sess:" }),
    resave: false,                                 // don't rewrite unchanged sessions
    saveUninitialized: false,                      // no session/cookie for anonymous visitors
    rolling: false,                                // true = reset cookie expiry on every response
    cookie: {
      httpOnly: true,
      secure: process.env.NODE_ENV === "production",
      sameSite: "lax",
      maxAge: 1000 * 60 * 60 * 24 * 7,             // 7 days
    },
  })
);
```

Login must **regenerate** the session id to prevent fixation:

```js
app.post("/login", async (req, res, next) => {
  const user = await verifyCredentials(req.body);      // from 02-passwords
  if (!user) return res.status(401).json({ message: "Invalid email or password" });

  req.session.regenerate((err) => {                    // new id, old one destroyed
    if (err) return next(err);
    req.session.userId = user._id.toString();
    req.session.role = user.role;
    req.session.save((err) => (err ? next(err) : res.json({ ok: true })));
  });
});

app.post("/logout", (req, res, next) => {
  req.session.destroy((err) => {                       // removes it from Redis
    if (err) return next(err);
    res.clearCookie("sid");                            // match the cookie's options
    res.json({ ok: true });
  });
});
```

Auth middleware becomes trivial:

```js
export const requireAuth = (req, res, next) =>
  req.session.userId ? next() : res.status(401).json({ message: "Authentication required" });
```

How it differs from [01-redis-sessions.md](./01-redis-sessions.md): `express-session` stores a JSON blob under `sess:<id>` and signs the cookie with `SESSION_SECRET` (cookie value looks like `s:<id>.<signature>`). It does not give you the per-user index set, device list or refresh-secret rotation; add those yourself if you need them (store `userId` → ids in a set on login).

---

## Cookie attributes reference

| Attribute | Effect | Recommended |
|---|---|---|
| `HttpOnly` | JS can't read it | Always for credentials |
| `Secure` | Sent only over HTTPS | Always in production |
| `SameSite` | Cross-site sending rules | `Lax` (sessions) / `Strict` (refresh cookie) |
| `Path` | URL prefix where it's sent | Narrow it where possible |
| `Domain` | Which hosts receive it | Omit (host-only) unless you need subdomains |
| `Max-Age` / `Expires` | Lifetime | Match server session TTL |

Omitting `Domain` makes the cookie **host-only**: sent only to the exact host that set it. Setting `Domain=example.com` shares it with every subdomain, including a forgotten or compromised `blog.example.com`.

### Cookie name prefixes

Browsers enforce rules on special name prefixes:

- `__Secure-` requires the `Secure` attribute.
- `__Host-` requires `Secure`, `Path=/`, and **no** `Domain`. It cannot be overwritten by a sibling subdomain, which defends against cookie-injection attacks.

```js
session({
  name: "__Host-sid",                       // the prefix goes in the cookie NAME
  cookie: { secure: true, path: "/", httpOnly: true, sameSite: "lax" },  // no `domain`
  // ...other options as above
});
```

Note: `__Host-` requires `Path=/`, so it **can't** be combined with the path-scoped refresh cookie in the hybrid design. A `__Secure-` prefix works there.

### What "same site" means

*Site* = scheme + registrable domain (eTLD+1). `app.example.com` and `api.example.com` are **same-site** (different origins). `example.com` and `example.org` are cross-site. `SameSite` cares about site, CORS cares about origin. People mix these up constantly.

| SameSite | Cookie sent on... |
|---|---|
| `Strict` | Same-site requests only. Even clicking a link from an email arrives *without* the cookie, so the first page load looks logged out |
| `Lax` | Same-site requests + cross-site top-level **GET** navigations (clicking a link) |
| `None` | Always (requires `Secure`); you own CSRF defense |

Browsers treat a missing `SameSite` as `Lax`. Set it explicitly.

---

## Strengths and weaknesses

| Strengths | Weaknesses |
|---|---|
| Simplest mental model | A Redis lookup on every authenticated request |
| Instant revocation and updates | Cookie auth needs CSRF protection |
| Nothing readable or stealable by JS | Awkward for mobile/native and third-party API clients |
| No token refresh logic in the frontend | Cross-site SPA + API deployments fight `SameSite` |
| Tiny credential (an id) | Shared Redis becomes a hard dependency for all requests |

Compare approaches in [06-jwt-vs-sessions.md](./06-jwt-vs-sessions.md).

---

## Common mistakes

- Leaving `saveUninitialized: true`, which creates a session and cookie for every anonymous visitor (and bots).
- Not calling `regenerate` at login (fixation).
- Default `MemoryStore` in production: leaks memory, loses sessions on restart, doesn't work with multiple instances. It's for development only.
- Forgetting `trust proxy`, so `secure` cookies never get set behind a load balancer.
- Weak or committed `SESSION_SECRET`.
- Logging out by only calling `clearCookie`.
- `SameSite=None` "to make it work" without CSRF defenses.

## Debugging

| Symptom | Cause |
|---|---|
| Cookie not set | `secure: true` over HTTP; missing `trust proxy`; CORS `credentials` missing for cross-origin |
| Session lost on every request | New `MemoryStore` per process, `sameSite`/`secure` mismatch, or cookie dropped by the browser (check DevTools → Application → Cookies and the console warning) |
| Logged out when arriving via an email link | `SameSite=Strict` on the session cookie; use `Lax` |
| Works on localhost, not in prod | `Secure` + proxy, `Domain` mismatch, or different subdomain/site |

---

## Quick summary

- Cookie session = opaque `sid` cookie + server-side state; simplest model for same-site apps.
- Regenerate the id on login; destroy server-side on logout.
- Set `HttpOnly`, `Secure`, explicit `SameSite`, narrow `Path`, no `Domain`; consider `__Host-`.
- Cookies are auto-sent, so CSRF defenses are required.

**Next:** [03-multi-device.md](./03-multi-device.md)
