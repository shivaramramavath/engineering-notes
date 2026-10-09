# Security

Frontend security starts from one uncomfortable fact: **the user controls the browser.** They can read your bundle, edit your JavaScript, replay and modify any request, and disable every check you wrote. So frontend security is really two jobs:

1. **Don't let attackers run *their* code in *your users'* browsers** (XSS and its relatives).
2. **Don't hand attackers anything valuable** (secrets, tokens, over-exposed data), and never rely on the client to enforce rules. The **server** is the only place access control can live.

> Client-side checks (hiding a button, a route guard, form validation) are **user experience**. They are not security. Every rule needs to be enforced again on the server ([route protection](../10-routing/04-route-protection.md), [authentication](../11-api-integration/03-authentication.md)).

## The threat map

| Threat | What it is | Main defense |
|---|---|---|
| **XSS** (cross-site scripting) | Attacker's script runs on your page, with your users' session | Escape output, sanitize HTML, **CSP** |
| **CSRF** | Another site makes your user's browser send an authenticated request | `SameSite` cookies, CSRF tokens, custom headers |
| **Clickjacking** | Your site is framed invisibly and clicks are hijacked | `frame-ancestors` / `X-Frame-Options` |
| **Secret leakage** | Keys or tokens end up in the bundle, source maps, logs, or URLs | Keep secrets server-side |
| **Supply chain** | A malicious or compromised dependency runs in your build or your users' browsers | Lockfiles, review, audit, least privilege |
| **Insecure transport** | Traffic read or modified in transit | HTTPS everywhere, HSTS |
| **Open redirects, injection into URLs** | Your app sends users to attacker-controlled places | Validate redirect targets and URL protocols |
| **Data over-exposure** | API returns more than the UI needs | Server-side field filtering and authorization |

## XSS: the biggest one

If an attacker can run JavaScript on your page, they can read anything the page can (including tokens in `localStorage`), call your API as the user, and rewrite the UI. It's the most important frontend vulnerability.

### React's built-in protection

React **escapes** values embedded in JSX by default:

```tsx
const comment = '<img src=x onerror="steal()">'
<p>{comment}</p>            // rendered as literal text, not HTML
```

So ordinary React code is far safer than string-concatenated HTML. XSS in React apps comes from the **escape hatches**:

### 1. `dangerouslySetInnerHTML`

```tsx
<div dangerouslySetInnerHTML={{ __html: userBio }} />      // ✗ executes any HTML/handlers in userBio
```

The name is the warning. If you must render user- or third-party-supplied HTML (rich text, Markdown output, CMS content), **sanitize it first** with a maintained library:

```tsx
import DOMPurify from "dompurify"

const clean = DOMPurify.sanitize(userBio)
<div dangerouslySetInnerHTML={{ __html: clean }} />
```

Sanitize **at render time** (or both at write and read), keep the sanitizer updated, and prefer rendering Markdown through a safe renderer (one that doesn't allow raw HTML by default) over `innerHTML`.

### 2. User-controlled URLs

```tsx
<a href={user.website}>Website</a>        // ✗ href="javascript:alert(document.cookie)" executes on click
<img src={user.avatar} />                  // less dangerous, but can leak requests and track users
```

Validate the **protocol** before using user-supplied URLs:

```ts
export function safeUrl(input: string): string | null {
  try {
    const url = new URL(input, window.location.origin)
    return ["http:", "https:", "mailto:"].includes(url.protocol) ? url.href : null
  } catch {
    return null
  }
}
```

Use an allow-list (`http`, `https`, `mailto`), not a block-list of `javascript:`, which attackers bypass with encodings and casing.

### 3. Other injection points

- **`eval`, `new Function`, `setTimeout("string")`**: never with user data.
- **Setting `innerHTML`/`outerHTML`/`document.write`** via refs or effects.
- **`<script>` or `<style>` content** built from user input; dynamic `href`/`src`/`action` attributes.
- **Third-party widgets and scripts** run with full page privileges. A compromised analytics script is an XSS.
- **`postMessage`**: always check `event.origin` and validate the message shape before acting.
- **Server-rendered HTML** ([SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)): when embedding data in a `<script>` (initial state), serialize it safely. A raw `JSON.stringify` can break out with `</script>`. Use a serializer that escapes `<`.
- **`target="_blank"`**: add `rel="noopener noreferrer"` for external links (modern browsers imply `noopener`, but being explicit costs nothing).

### Defense in depth: assume you'll miss one

Escaping and sanitizing prevent XSS. **Content Security Policy** limits the damage when something slips through.

## Content Security Policy (CSP)

CSP is a response header that tells the browser **which sources of scripts, styles, images, and connections are allowed**. An injected `<script>` or inline handler from an untrusted source is simply blocked.

```http
Content-Security-Policy:
  default-src 'self';
  script-src 'self';
  style-src 'self' 'unsafe-inline';
  img-src 'self' data: https:;
  font-src 'self';
  connect-src 'self' https://api.example.com https://*.sentry.io;
  frame-ancestors 'none';
  base-uri 'self';
  object-src 'none'
```

| Directive | Controls |
|---|---|
| `default-src` | Fallback for anything not listed |
| `script-src` | Where JavaScript can load from. **The most important.** Avoid `'unsafe-inline'` and `'unsafe-eval'` here |
| `style-src` | CSS sources (many setups need `'unsafe-inline'` for style attributes/CSS-in-JS, or nonces) |
| `connect-src` | `fetch`/XHR/WebSocket destinations. **List your API and monitoring endpoints** |
| `img-src`, `font-src`, `media-src` | Asset sources |
| `frame-ancestors` | Who may embed your site (clickjacking defense) |
| `base-uri`, `object-src` | Close off less common injection routes |

Rolling it out (CSP is easy to break your own app with):

1. Start with **`Content-Security-Policy-Report-Only`**: it logs violations without blocking.
2. Collect reports (`report-to`/`report-uri`, or the browser console), fix legitimate violations (a missing API origin, an inline script).
3. Switch to **enforcing** once the reports are quiet.
4. **Tighten over time.** Prefer `script-src 'self'` with no inline scripts; where inline is unavoidable, use **nonces** or **hashes** instead of `'unsafe-inline'`.

Things that commonly trip CSP in a Vite SPA: analytics and chat snippets (inline scripts and third-party hosts), Google Fonts (`style-src`/`font-src`), `connect-src` missing the API or Sentry, inline styles from UI libraries, and dev-only tooling. Test the *production build* behind the real headers. A strict policy with a nonce-based approach is also an option for server-rendered setups. For a purely static SPA, hash- or `'self'`-based policies are the practical route.

## Security headers

Set these on **every response** from your host or server ([deployment](./02-deployment.md#headers-beyond-caching), [nginx](./03-docker.md#the-nginx-config)):

| Header | Purpose | Typical value |
|---|---|---|
| **`Content-Security-Policy`** | Limit script/resource sources (above) | See the example |
| **`Strict-Transport-Security`** (HSTS) | Force HTTPS for future visits | `max-age=31536000; includeSubDomains` (add `preload` only when you're certain) |
| **`X-Content-Type-Options`** | Stop MIME-type sniffing | `nosniff` |
| **`Referrer-Policy`** | Control what URL info leaks to other sites | `strict-origin-when-cross-origin` |
| **`Permissions-Policy`** | Disable browser features you don't use | `camera=(), microphone=(), geolocation=()` |
| **`frame-ancestors`** (CSP) or `X-Frame-Options` | Prevent clickjacking | `frame-ancestors 'none'` (or `DENY`) |
| **`Cross-Origin-Opener-Policy`** | Isolate your window from cross-origin popups | `same-origin` where compatible |

Notes:

- **HSTS is sticky.** Once a browser has seen it, it refuses plain HTTP for that duration, so make sure HTTPS works on all subdomains before enabling `includeSubDomains` or `preload`. Start with a short `max-age`.
- Verify what's actually served: `curl -I https://yoursite.com`, browser DevTools, or an online header scanner. A misnamed config file silently does nothing.
- On nginx, remember [`add_header` inheritance](./03-docker.md#the-nginx-config): a `location` that sets its own `add_header` drops the server-level ones.
- **CORS is not a security header you add for protection.** It *relaxes* the same-origin policy for specific origins. Configure it narrowly on the **server** (no `*` with credentials), and remember it only restrains browsers, not attackers calling your API directly.

## Authentication, tokens, and cookies

The full discussion is in [authentication](../11-api-integration/03-authentication.md). The short version:

- **XSS can steal anything JavaScript can read**, including tokens in `localStorage`/`sessionStorage`. `HttpOnly` cookies can't be read by script (though XSS can still *use* the session while the page is open).
- A strong SPA pattern: **short-lived access token in memory**, **refresh token in an `HttpOnly; Secure; SameSite` cookie**.
- **Cookies need CSRF thought**: use `SameSite=Lax` or `Strict`, plus CSRF tokens or required custom headers for state-changing requests when relevant.
- **Never put tokens in URLs** (history, logs, referrers) or error reports.
- **Log out properly**: clear tokens, clear the [query cache](../12-server-state/04-caching-and-synchronization.md#cache-and-identity), and revoke server-side.

## Secrets and sensitive data

- **Nothing secret in the bundle.** `VITE_*` values, source files, and `public/` are all public ([env vars](./00-environment-variables.md#secret-vs-public-configuration)). Check built output: `grep -r "sk_live\|secret\|password" dist/`.
- **Source maps** can expose your original source and comments. Use `sourcemap: "hidden"`, upload to monitoring, and don't serve them ([build](./01-build.md#sourcemap)).
- **Keep sensitive data out of** `localStorage`, URLs, analytics events, console logs, error reports, and the DOM (hidden inputs and `data-` attributes are readable).
- **Restrict third-party keys** at the provider (allowed domains, quotas, scoped permissions). A publishable key that works from anywhere is an abuse vector.
- **Minimize what the API returns.** If the UI doesn't need a field (email, role, internal IDs), the server shouldn't send it. "The frontend just doesn't display it" is not protection.

## Input handling

- **Validate on the server**, always. Client validation ([forms](../06-forms/01-form-validation.md)) improves UX; attackers bypass it.
- **File uploads**: client checks of type and size are convenience only. The server must validate content type, size, and scan/store files safely. Serve user uploads from a separate origin or with `Content-Disposition`/`nosniff`, never as executable HTML from your main domain. SVGs can contain scripts.
- **Redirects**: after login, `?next=` targets must be validated as **internal paths** (start with a single `/`, not `//` or a full URL) to avoid open redirects ([route protection](../10-routing/04-route-protection.md#redirect-back-after-login)).
- **Rendering user content**: treat names, bios, comments, filenames, and error messages from APIs as untrusted strings.

## Supply chain

Your app is mostly other people's code. A malicious or hijacked package runs on your build machine and in your users' browsers.

- **Commit the lockfile** and install with **`npm ci`** so you get exactly the audited versions ([build](./01-build.md#reproducible-builds)).
- **Review new dependencies** before adding them: maintainer, download history, size, recent activity, transitive dependencies. Prefer fewer, well-maintained packages; the best dependency is the one you don't add ([bundle optimization](../14-performance/05-bundle-optimization.md#ship-less-code)).
- Beware **typosquatting** (`reactt`, `lodahs`) and **dependency confusion** for private package names (scope them and configure your registry).
- **Run `npm audit`** (and automated update tools like Dependabot or Renovate) regularly. Treat results with judgment: many advisories affect build-only or unreachable code, but critical issues in runtime dependencies deserve prompt action.
- Be cautious with **install scripts** (`postinstall`). Consider disabling lifecycle scripts for installs where you don't need them (`--ignore-scripts`), and evaluate what packages run on install.
- **Pin and review GitHub Actions** and other CI dependencies. They run with your secrets ([CI/CD security](./04-ci-cd.md#pipeline-security)).
- **Third-party scripts** (CDN-loaded libraries, tag managers) are the same risk with less visibility. Self-host where you can; if you load from a CDN, use **Subresource Integrity** (`integrity="sha384-…" crossorigin="anonymous"`) so a tampered file is rejected. SRI doesn't work for scripts that change (like most analytics snippets), so weigh whether you need them.
- Keep **React, your router, and your build tools updated**, and watch advisories for your main dependencies.

## Security in the pipeline and process

- **Secrets in CI** as encrypted, environment-scoped secrets, ideally short-lived (OIDC), never printed or committed ([CI/CD](./04-ci-cd.md#secrets-and-environments)).
- **Scan**: dependency audit, container image scan ([Docker](./03-docker.md#security-hardening)), secret scanning (to catch committed keys), and optionally static analysis (CodeQL/Semgrep).
- **Least privilege** everywhere: CI tokens, cloud deploy roles, API keys.
- **If a secret leaks, rotate it immediately.** Removing it from git history is not enough, because it's been exposed.
- Write down who to contact and how to respond to a security report. A `security.txt` or a documented process helps researchers reach you.

## Privacy and compliance (briefly)

- Collect the **minimum** personal data, tell users what you collect, and honor consent requirements for analytics and cookies where laws like GDPR apply.
- **Don't send personal data to third parties** (analytics, error tracking, session replay) without understanding it. Scrub and mask ([error monitoring privacy](./06-error-monitoring-and-logging.md#privacy-and-data-scrubbing)).
- This isn't legal advice, and regulations vary. Involve whoever owns compliance for your product.

## A practical checklist

1. No `dangerouslySetInnerHTML` with unsanitized input; user URLs validated by protocol.
2. A **CSP** (report-only → enforcing) and the standard **security headers**, verified on the deployed site.
3. HTTPS everywhere; HSTS once stable.
4. No secrets in the bundle, source maps hidden, third-party keys restricted.
5. Tokens handled per the [auth guidance](../11-api-integration/03-authentication.md); cache cleared on logout.
6. Server enforces **authorization and validation** for everything.
7. Lockfile, `npm ci`, dependency audit and updates, reviewed new packages, SRI for CDN scripts.
8. CI hardened: least-privilege permissions, pinned actions, scoped secrets.
9. Monitoring in place to notice anomalies ([06](./06-error-monitoring-and-logging.md)).

## Common mistakes

- **Treating client-side checks as security** (hidden buttons, route guards, disabled inputs).
- **`dangerouslySetInnerHTML` with unsanitized content**, or sanitizing once and trusting forever.
- **Rendering user-supplied URLs unchecked** (`javascript:` links).
- **Secrets in `VITE_*` variables, committed `.env` files, or public source maps.**
- **Tokens in `localStorage` with no XSS defenses** (no CSP, no sanitization).
- **No CSP**, or an "enforced" CSP full of `'unsafe-inline'`/`'unsafe-eval'` that protects nothing.
- **Turning on a strict CSP without report-only first**, breaking production.
- **Security headers set but never verified**, or dropped by server config inheritance.
- **CORS `*` with credentials**, or treating CORS as access control.
- **Sensitive data in URLs, logs, analytics, or error reports.**
- **Unreviewed dependencies, no lockfile, `npm install` in CI.**
- **Over-permissive CI** (broad token permissions, unpinned third-party actions).
- **Open redirects** via unvalidated `next`/`returnTo` parameters.
- **Relying on the frontend not displaying a field** instead of the API not sending it.
- **Rotating nothing after a leak.**

## Quick summary

- The browser is untrusted: **the server must enforce authorization and validation**; frontend checks are UX.
- **XSS** is the main risk. React escapes by default, so danger lives in `dangerouslySetInnerHTML` (sanitize with DOMPurify), user-controlled URLs (allow-list protocols), `eval`, `innerHTML`, third-party scripts, and `postMessage`.
- **CSP** (rolled out report-only first) plus **security headers** (HSTS, `nosniff`, `Referrer-Policy`, `Permissions-Policy`, `frame-ancestors`) limit damage; verify them on the deployed site.
- Keep **secrets out of the client** (bundle, `VITE_*`, public source maps, URLs, logs); restrict third-party keys.
- Choose token storage deliberately (memory + `HttpOnly` refresh cookie), and think about CSRF when using cookies.
- Defend the **supply chain**: lockfile, `npm ci`, review and audit dependencies, SRI for CDN scripts, hardened CI.
- Minimize data collected and exposed; scrub what you send to third parties.

## Next

[06 — Error monitoring and logging](./06-error-monitoring-and-logging.md)