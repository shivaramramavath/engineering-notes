# Helmet

Helmet sets a collection of HTTP **response headers** that tell browsers to turn on built-in protections — against XSS, clickjacking, MIME sniffing, and downgrade attacks.

## Why headers matter

Browsers ship with powerful defenses, but many are **opt-in**: the server has to ask for them through headers. Helmet is a small Express middleware that sets sensible defaults in one line, so you don't have to remember a dozen header names and values.

```bash
npm install helmet
```

```js
import express from "express";
import helmet from "helmet";

const app = express();

app.use(helmet());      // register early, before your routes
```

Put it near the top of your middleware stack so that **every** response, including error responses and static files, carries the headers. Middleware ordering is explained in `06-express/02-middleware.md`.

---

## What `helmet()` does by default

Calling `helmet()` with no options enables these headers (exact defaults can change slightly between major versions, so check the docs for the version you install):

| Header | Purpose |
|---|---|
| `Content-Security-Policy` | Restricts where scripts, styles, images, etc. may load from — the main XSS defense |
| `Strict-Transport-Security` (HSTS) | Tells browsers to use HTTPS only for your domain |
| `X-Content-Type-Options: nosniff` | Stops the browser from guessing a file's type (MIME sniffing) |
| `X-Frame-Options` | Prevents your pages being embedded in iframes (clickjacking) |
| `Referrer-Policy` | Controls how much URL information is sent in the `Referer` header |
| `Cross-Origin-Opener-Policy` | Isolates your window from cross-origin pop-ups |
| `Cross-Origin-Resource-Policy` | Controls which origins may load your resources |
| `Origin-Agent-Cluster` | Requests origin-keyed process isolation |
| `X-DNS-Prefetch-Control` | Controls DNS prefetching |
| `X-Download-Options` | Older IE download-handling protection |
| `X-Permitted-Cross-Domain-Policies` | Restricts Adobe Flash/PDF cross-domain policies |
| `X-XSS-Protection: 0` | Deliberately **disables** the old, buggy browser XSS filter (CSP replaces it) |
| *(removes)* `X-Powered-By` | Stops advertising that you run Express |

> `X-XSS-Protection: 0` surprises people. The legacy XSS auditor in old browsers could itself introduce vulnerabilities, so modern guidance is to turn it off and rely on CSP.

---

## See the headers for yourself

```bash
curl -I http://localhost:3000
```

```
HTTP/1.1 200 OK
Content-Security-Policy: default-src 'self';base-uri 'self';...
Strict-Transport-Security: max-age=15552000; includeSubDomains
X-Content-Type-Options: nosniff
X-Frame-Options: SAMEORIGIN
Referrer-Policy: no-referrer
...
```

Online scanners (securityheaders.com, Mozilla Observatory) grade your deployed site and explain what's missing. Run one before each launch; it's part of `08-security-checklist.md`.

---

## Content Security Policy (CSP) in depth

CSP is the most powerful — and most likely to break things — header Helmet sets. It's an allow-list telling the browser which sources are trusted for each kind of content. If an attacker injects `<script>` into a page, the browser refuses to run it because it doesn't come from an allowed source.

### Default policy

```
default-src 'self';
base-uri 'self';
font-src 'self' https: data:;
form-action 'self';
frame-ancestors 'self';
img-src 'self' data:;
object-src 'none';
script-src 'self';
script-src-attr 'none';
style-src 'self' https: 'unsafe-inline';
upgrade-insecure-requests
```

Translation: load everything from your own origin only; no plugins; no inline scripts; forms may only submit to yourself.

### Customizing CSP

The moment you use a CDN, Google Fonts, analytics, or an image host, the defaults block them. Extend the policy with `directives`:

```js
app.use(
  helmet({
    contentSecurityPolicy: {
      directives: {
        defaultSrc: ["'self'"],
        scriptSrc: ["'self'", "https://cdn.jsdelivr.net"],
        styleSrc: ["'self'", "https://fonts.googleapis.com"],
        fontSrc: ["'self'", "https://fonts.gstatic.com"],
        imgSrc: ["'self'", "data:", "https://images.example.com"],
        connectSrc: ["'self'", "https://api.example.com"],
        frameAncestors: ["'none'"],
        objectSrc: ["'none'"],
      },
    },
  })
);
```

Start from the defaults and add only what you need. Every extra source widens the attack surface.

### Avoid `'unsafe-inline'` for scripts — use nonces

Inline `<script>` blocks are blocked by default. Adding `'unsafe-inline'` would re-enable the very thing CSP exists to stop. The safe alternative is a **nonce**: a random value generated per request that only your legitimate inline scripts carry.

```js
import crypto from "node:crypto";

app.use((req, res, next) => {
  res.locals.cspNonce = crypto.randomBytes(16).toString("base64");
  next();
});

app.use(
  helmet({
    contentSecurityPolicy: {
      directives: {
        scriptSrc: ["'self'", (req, res) => `'nonce-${res.locals.cspNonce}'`],
      },
    },
  })
);
```

```html
<!-- in your template -->
<script nonce="<%= cspNonce %>">
  console.log("allowed: the nonce matches");
</script>
```

An attacker injecting a script can't guess a fresh random nonce, so the browser blocks it.

### Report-only mode: roll out safely

A strict CSP can silently break a live site. Test first without enforcing:

```js
app.use(
  helmet({
    contentSecurityPolicy: {
      reportOnly: true,
      directives: {
        defaultSrc: ["'self'"],
        reportUri: ["/csp-report"],
      },
    },
  })
);

app.post("/csp-report", express.json({ type: "application/csp-report" }), (req, res) => {
  console.warn("CSP violation:", req.body);
  res.sendStatus(204);
});
```

Watch the violation reports for a few days, fix the allow-list, then switch `reportOnly` off.

### Is CSP relevant for a pure JSON API?

Mostly less so: an API that returns only JSON has no scripts to restrict. A very tight policy is still harmless, and Helmet's defaults cost nothing. The benefit is largest for server-rendered pages and for anything that serves HTML (including error pages and API docs like Swagger UI, which usually need relaxed directives).

---

## HSTS: force HTTPS

`Strict-Transport-Security` tells the browser: *"For the next N seconds, never talk to this domain over plain HTTP — upgrade automatically."* It blocks SSL-stripping attacks on hostile networks.

```js
app.use(
  helmet({
    strictTransportSecurity: {
      maxAge: 31536000,         // 1 year, in seconds
      includeSubDomains: true,
      preload: false,           // see warning below
    },
  })
);
```

Cautions:

- HSTS only works over **HTTPS**; browsers ignore it on plain HTTP. Terminate TLS properly first (`16-production/04-nginx.md`).
- Start with a short `maxAge` (e.g. a day), confirm everything works, then raise it. A long HSTS period on a site whose certificate later breaks locks users out.
- `preload: true` submits you to a list baked into browsers — removal takes months. Only enable when you're certain every subdomain supports HTTPS permanently.
- Don't enable HSTS on `localhost` development in a way that affects other local projects.

---

## Disabling or tuning individual headers

Each header is an option. Pass `false` to turn one off, or an object to configure it:

```js
app.use(
  helmet({
    contentSecurityPolicy: false,                 // e.g. you set CSP at the proxy instead
    crossOriginEmbedderPolicy: false,
    frameguard: { action: "deny" },               // X-Frame-Options: DENY
    referrerPolicy: { policy: "strict-origin-when-cross-origin" },
    hsts: { maxAge: 31536000, includeSubDomains: true },
  })
);
```

You can also use the individual middlewares directly:

```js
app.use(helmet.frameguard({ action: "deny" }));
app.use(helmet.noSniff());
app.use(helmet.hidePoweredBy());     // equivalent to app.disable("x-powered-by")
```

---

## Headers worth knowing beyond the defaults

### `Permissions-Policy`

Controls access to browser features (camera, microphone, geolocation). Helmet doesn't set it by default; add it yourself:

```js
app.use((req, res, next) => {
  res.setHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
  next();
});
```

### `Cache-Control` for sensitive responses

Don't let browsers or proxies cache personal data:

```js
app.get("/account", requireAuth, (req, res) => {
  res.set("Cache-Control", "no-store");
  res.json(req.user);
});
```

Caching headers are covered in `05-http-web/05-caching-and-compression.md`.

### Cross-origin isolation

`Cross-Origin-Embedder-Policy` (COEP) is off by default in current Helmet versions because enabling it breaks loading of cross-origin images and iframes that don't opt in. Turn it on only if you need features that require cross-origin isolation (e.g. `SharedArrayBuffer`).

---

## Helmet and CORS are different things

| | Helmet | CORS |
|---|---|---|
| Purpose | Hardens how browsers treat *your* responses | Relaxes the browser's same-origin rule so *other origins* can call your API |
| Direction | Restrictive | Permissive |
| Library | `helmet` | `cors` |

They're complementary. Use both, and don't mistake one for the other — particularly, CORS is **not** a security control that stops non-browser clients or CSRF (see `05-http-web/04-cors.md` and `05-common-vulnerabilities.md`).

```js
app.use(helmet());
app.use(cors({ origin: "https://app.example.com", credentials: true }));
```

---

## Recommended baseline

```js
import express from "express";
import helmet from "helmet";

const app = express();

app.disable("x-powered-by");     // belt and braces (helmet also removes it)
app.set("trust proxy", 1);       // when behind a reverse proxy

app.use(helmet());               // sensible defaults
// app.use(cors(...));           // if browsers from other origins call you
// app.use(rateLimiter);         // 06-rate-limiting.md
app.use(express.json({ limit: "100kb" }));   // cap body size
```

The `limit` on the JSON parser is a free security win: it stops someone from posting a 500 MB body to exhaust memory.

---

## Common problems

| Symptom | Likely cause | Fix |
|---|---|---|
| Inline scripts/styles stop working | Default CSP blocks inline code | Move code to files, or use nonces |
| CDN scripts, fonts, or images blocked | Source not in CSP allow-list | Add the origin to the matching directive |
| Swagger UI / API docs page is blank | CSP blocks its inline scripts | Relax CSP for that route only |
| Browser console: "Refused to load…" | CSP violation | Read the message; it names the directive |
| Cross-origin images fail after enabling extra headers | COEP or CORP too strict | Adjust `crossOriginEmbedderPolicy` / `crossOriginResourcePolicy` |
| Site unreachable after enabling HSTS | HTTPS not configured or cert expired | Fix TLS; use a short `maxAge` while rolling out |

**Tip:** scope a relaxed policy to only the routes that need it instead of weakening it globally:

```js
app.use("/docs", helmet({ contentSecurityPolicy: false }), swaggerRouter);
app.use(helmet());   // everything else keeps the strict defaults
```

(Register the route-specific middleware **before** the global one if you want it to take effect first for those paths, and check the resulting headers with `curl -I`.)

## Next

**`08-security-checklist.md`** pulls every topic in this section into a single pre-launch checklist you can run through before each deployment.
