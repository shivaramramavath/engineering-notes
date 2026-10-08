# Deployment

A built Vite SPA is just **static files**: HTML, JavaScript, CSS, and assets. Deploying means putting those files somewhere that serves them over HTTPS, with the right routing and caching rules. That's simpler than deploying a server, and it's why SPAs are cheap and fast to host. The details that matter are the rules *around* the files.

> Hosting platforms change their config formats often. The **requirements** below are stable. For exact syntax, check your host's current docs.

## What a static host must do

Any host serving your SPA needs to:

1. **Serve `index.html` for unknown paths** (the SPA fallback), so deep links and refreshes work.
2. **Cache hashed assets for a long time** and **never cache `index.html`** for long.
3. **Serve over HTTPS**, with compression (Brotli/gzip).
4. **Send security headers** ([security](./05-security.md#security-headers)).
5. **Route `/api`** to your backend, or allow cross-origin calls ([below](#routing-the-api)).
6. Let you **roll back** to a previous version.

Everything else (CDN, previews, custom domains) is a bonus that most modern hosts provide.

## Hosting options

| Option | Examples | Good for |
|---|---|---|
| **Static hosting / JAMstack platforms** | Netlify, Vercel, Cloudflare Pages, Firebase Hosting | Most SPAs: Git integration, preview deploys, CDN, HTTPS by default |
| **Object storage + CDN** | S3 + CloudFront, GCS + Cloud CDN, Azure Blob + CDN | Full control, cheap at scale, infrastructure-as-code |
| **GitHub Pages** | | Free hosting for docs, demos, and small projects (limited routing and header control) |
| **Your own server** | Nginx/Caddy on a VM | Total control; you maintain it |
| **Containers** | Docker on ECS/Cloud Run/Kubernetes | When you already run containers, or need a custom server ([Docker](./03-docker.md)) |

For a typical SPA, a static platform with Git integration is the lowest-effort, lowest-risk path.

## SPA fallback routing

Client-side routing means `/projects/42` isn't a file on disk. On a **refresh** or when someone opens a shared link, the server receives a request for `/projects/42`, finds nothing, and returns a 404, unless you tell it to serve `index.html` instead ([React Router hosting note](../10-routing/00-react-router.md#hosting-the-spa-fallback)).

The rule: **if no static file matches, return `index.html` with status 200.** Real files (`/assets/…`, `/favicon.ico`) must still be served normally.

Examples (verify against your host's current docs):

```text
# Netlify: public/_redirects
/*    /index.html   200
```

```json
// Vercel: vercel.json
{ "rewrites": [{ "source": "/(.*)", "destination": "/index.html" }] }
```

```nginx
# Nginx
location / {
  try_files $uri /index.html;
}
```

- **Cloudflare Pages** treats a project with no `404.html` as an SPA and falls back to `index.html` automatically.
- **S3 + CloudFront**: S3 website hosting can serve an error document, but with CloudFront the usual setup is custom error responses mapping `403`/`404` to `/index.html` with status `200`.
- **GitHub Pages** has no rewrite rules. The usual workaround is a `404.html` that redirects, or using a hash router (`/#/projects`).

**Test it**: open a deep link directly, and refresh on a nested route. Do it on the *deployed* site, not just locally.

A subtle consequence: with a blanket fallback, a request for a **missing asset** (a stale chunk URL) returns `index.html` (HTML) with a 200, and the browser then fails parsing it as JavaScript. That's the origin of confusing "Unexpected token `<`" and chunk-load errors after deploys ([chunk errors](../14-performance/03-code-splitting-and-lazy-loading.md#chunk-load-errors-and-deployments)). Configure the fallback to **exclude `/assets/`** (returning a true 404 there) where your host allows it.

## Caching headers

The standard SPA split ([network performance](../14-performance/06-network-performance.md#http-caching)):

| Files | `Cache-Control` | Why |
|---|---|---|
| `/assets/*` (hashed JS/CSS/images) | `public, max-age=31536000, immutable` | Content hash changes whenever the content changes, so cache forever |
| `index.html` | `no-cache` (or `max-age=0, must-revalidate`) | It references the current hashed files; users must always get the latest |
| `config.js` / `config.json` (runtime config) | `no-cache` | Must reflect current configuration |
| Un-hashed files in `public/` (favicon, fonts, `robots.txt`) | Moderate (hours to a day) | Not content-addressed, so can't be immutable |

Most static platforms apply this pattern **by default** for hashed assets. Verify with the Network panel (look at `cache-control` on `index.html` and a JS file). Wrong caching causes the most infamous deployment bug: **users stuck on the old version** (HTML cached too long) or **broken pages** (HTML new, assets missing).

## Environments

A typical setup:

| Environment | Purpose | Deploys from |
|---|---|---|
| **Preview** (per pull request) | Review a change in isolation | Every PR branch |
| **Staging** | Production-like testing, QA, demos | `main` (automatic) |
| **Production** | Real users | A release tag, or `main` after approval/checks |

- Each environment has its own **API URL, keys, and config** ([env vars](./00-environment-variables.md)) and its own backend data. Never point staging at the production database.
- **Preview deployments** (one URL per PR) are one of the best features of modern hosts: designers and reviewers click a link instead of checking out a branch. Make sure previews can't write to production data and aren't indexed by search engines (`X-Robots-Tag: noindex`).
- Keep environments **as similar as possible**. Differences are where "works on staging" bugs hide.

## The deploy flow

```text
merge to main
     │
     ▼
CI: lint → typecheck → test → build  ──► artifact (dist/)
     │
     ▼
deploy artifact to staging  ──► smoke tests / e2e
     │
     ▼  (manual approval or automatic)
deploy the SAME artifact to production ──► post-deploy checks ──► monitor
```

Automate every step ([CI/CD](./04-ci-cd.md)). Manual file copying is how production incidents start.

### Atomic deploys and old assets

A good host switches to the new version **atomically** (all files at once), so no visitor sees a half-updated site. Also, **keep the previous version's assets available** for a while. Users with an open tab still have the *old* `index.html` and will request old hashed chunks when they navigate. If those files vanish, they get chunk errors. Platforms with immutable deploys (Netlify, Vercel, Cloudflare) handle this; with S3 or your own server, **don't delete old assets immediately**. Sweep ones older than N days.

### Rollbacks

Plan for failure **before** you need it:

- Static platforms keep every deploy and let you **promote a previous one with one click or command**. That's usually the fastest rollback.
- With your own pipeline, **redeploy the last known-good artifact** (don't "revert and rebuild" under pressure).
- Keep **database/API compatibility**: a frontend rollback only works if the backend still supports the old frontend (and vice versa). Make API changes **backwards-compatible** and deploy in order (backend first for additive changes).
- Rehearse rollbacks. A procedure you've never run is a guess.

## Routing the API

Your SPA calls an API. Two ways to arrange it:

**Same origin (recommended):** serve the API under the same domain, such as `https://app.example.com/api/*`, by rewriting/proxying `/api` to the backend at the host or CDN:

```text
# Netlify
/api/*  https://api.internal.example.com/:splat  200
```

Same-origin means **no CORS**, simple cookies (`SameSite=Lax`), and one TLS certificate. The frontend uses relative URLs (`/api/projects`), identical across environments ([API client](../11-api-integration/02-api-client.md), [dev proxy](./00-environment-variables.md#dev-proxy-avoiding-env-juggling-and-cors)).

**Cross-origin:** the SPA at `app.example.com`, the API at `api.example.com`. This works, but requires **CORS** configuration on the server (specific allowed origins, credentials headers) and `credentials: "include"` for cookies ([fetch and CORS](../11-api-integration/00-fetch.md#headers-credentials-and-cors)). Cookies across *sites* (not just subdomains) need `SameSite=None; Secure`.

## HTTPS, domains, and DNS

- **HTTPS is mandatory** (service workers, many browser APIs, and user trust require it). Modern hosts provision certificates automatically.
- Redirect HTTP → HTTPS, and add **HSTS** once you're sure ([security headers](./05-security.md#security-headers)).
- Pick a **canonical domain** (`www` or apex) and redirect the other.
- **DNS TTL**: lower it before a migration so changes propagate quickly, and raise it afterwards.
- Set up **custom domain** certificates and renewals (automatic on most platforms, but monitor expiry).

## Headers beyond caching

Static hosts let you attach response headers (config file such as `_headers`, or platform settings):

```text
/*
  Content-Security-Policy: default-src 'self'; …
  X-Content-Type-Options: nosniff
  Referrer-Policy: strict-origin-when-cross-origin
  Strict-Transport-Security: max-age=31536000
```

Details and rationale in [05 — Security](./05-security.md#security-headers). Check the *deployed* responses (`curl -I https://yoursite.com`, or the Network panel), because misconfigured header files silently do nothing.

## SEO and previews of a client-rendered app

A pure SPA sends an empty HTML shell. Search engines often execute JavaScript, but social link previews (Slack, X, Facebook) and some crawlers **don't**. If SEO or link previews matter for public pages, consider **prerendering/SSG** those routes or SSR ([SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md)). For authenticated apps, it rarely matters. Set sensible `<title>` and meta tags in `index.html` regardless, and a `robots.txt` (and `noindex` on staging and previews).

## After deploying

- **Smoke test**: a handful of automated checks against the live URL (the app loads, login page renders, a key API call succeeds), via a small Playwright suite ([E2E](../18-testing-and-debugging/06-e2e-testing-playwright.md)) run as the last pipeline step.
- **Mark the release** in your monitoring tool (release/version tag) so errors and performance changes can be tied to it ([06](./06-error-monitoring-and-logging.md#context-that-makes-errors-actionable), [07](./07-performance-monitoring.md)).
- **Watch dashboards** for error rate and vitals for the first minutes/hours.
- **Announce** deploys where your team will see them.

## Reducing deploy risk

- **Small, frequent releases** beat big-bang launches. Smaller changes are easier to diagnose and roll back.
- **Feature flags**: ship code dark and enable it gradually or per user ([env vars](./00-environment-variables.md#feature-flags)).
- **Canary or gradual rollouts** where the platform supports it (a percentage of traffic gets the new version).
- **Deploy outside peak hours** if rollback isn't instant, and **not before a weekend** unless you like pages.
- **Backwards-compatible API changes** so frontend and backend can deploy independently.

## Common mistakes

- **No SPA fallback**, so refresh and deep links return 404.
- **Caching `index.html` for long**, so users never see the new version (or the opposite: no caching on hashed assets, so everything re-downloads).
- **Deleting old assets right after deploy**, breaking open tabs with chunk errors.
- **A blanket fallback that also serves HTML for missing JS/CSS**, producing confusing "Unexpected token `<`" errors.
- **Wrong `base`/`basename`** for subpath deploys.
- **Pointing staging or previews at production data.**
- **Manual deploys** (drag-and-drop, FTP, `scp`) with no record or rollback.
- **Different artifacts per environment** with no way to know what you tested is what you shipped.
- **No rollback plan**, or one that fails because the backend isn't backwards-compatible.
- **Cross-origin APIs without proper CORS/cookie settings**, found only after launch.
- **Forgetting to `noindex` staging and preview sites.**
- **Not checking the deployed headers**, assuming the config file worked.
- **No post-deploy verification or release tagging.**

## Quick summary

- A Vite SPA is static files, so deploy to a static host (Netlify/Vercel/Cloudflare/S3+CDN) or your own server.
- Requirements: **SPA fallback to `index.html`**, **immutable caching for hashed assets / `no-cache` for `index.html`**, HTTPS + compression, security headers, API routing, rollback ability.
- Prefer **same-origin** `/api` (proxy/rewrite) to avoid CORS and cookie headaches.
- Use **preview → staging → production** environments with separate config and data; automate the flow and promote the same artifact where possible.
- Deploy **atomically** and **keep old assets** briefly; plan and rehearse **rollbacks** (promote a previous deploy).
- Smoke-test after deploy, tag the release in monitoring, and reduce risk with small releases and feature flags.

## Next

[03 — Docker](./03-docker.md)
