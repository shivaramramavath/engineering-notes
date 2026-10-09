# Production Checklist

The previous notes explain *how* each production concern works. This one is the **gate**: a checklist to run before a launch, a major release, or a new environment, and to revisit periodically. It doesn't replace understanding, it makes sure nothing you already understand gets forgotten at 5 p.m. on release day.

How to use it:

- Items marked **(blocker)** should not ship unfixed. Everything else is "fix soon, or consciously accept the risk and write down why".
- Copy it into your launch ticket or PR template, and tick items off with evidence (a link, a screenshot, a command output), not memory.
- **Not every item applies to every app.** Delete what's irrelevant, but delete it on purpose.
- Run it against the **deployed environment** (staging, then production), not just your laptop.

## 1. Build and configuration

- [ ] **(blocker)** Production build succeeds from a clean checkout with `npm ci` ([reproducible builds](./01-build.md#reproducible-builds)).
- [ ] **(blocker)** Type check, lint, and tests all pass in CI ([CI](./04-ci-cd.md)).
- [ ] **(blocker)** No secrets in the bundle, `VITE_*` variables, committed `.env` files, or `public/` ([public vs secret config](./00-environment-variables.md#secret-vs-public-configuration)). Searched `dist/` for keys and tokens.
- [ ] Environment variables validated at startup, with a committed `.env.example` ([validation](./00-environment-variables.md#validate-at-startup)).
- [ ] Correct API URL and environment values baked into (or injected at runtime for) this environment.
- [ ] `base`/`basename` correct for where the app is served ([build](./01-build.md#base-deploying-under-a-subpath)).
- [ ] Node version pinned and the same in local, CI, and Docker.
- [ ] Production build **previewed and clicked through** ([verifying a build](./01-build.md#verifying-a-build)): lazy routes load, deep link + refresh works, no console errors.
- [ ] Release is **version-stamped** (commit SHA) and visible in the app and in monitoring ([version stamping](./01-build.md#version-stamping)).
- [ ] No dev-only code, devtools, debug logging, or test data in the production bundle.

## 2. Security

- [ ] **(blocker)** HTTPS everywhere, with HTTP redirecting to HTTPS and a valid, auto-renewing certificate.
- [ ] **(blocker)** The server enforces **authentication, authorization, and input validation** on every endpoint. The client is not trusted ([security](./05-security.md)).
- [ ] **(blocker)** No `dangerouslySetInnerHTML` with unsanitized input; user-supplied URLs checked against an allow-list of protocols ([XSS](./05-security.md#xss-the-biggest-one)).
- [ ] **Security headers** set and verified on the deployed site with `curl -I` ([headers](./05-security.md#security-headers)): CSP (at least report-only), HSTS, `X-Content-Type-Options`, `Referrer-Policy`, `frame-ancestors`.
- [ ] **CSP** tested against the production build and third-party scripts ([CSP](./05-security.md#content-security-policy-csp)).
- [ ] Token storage and cookie flags follow the [auth guidance](../11-api-integration/03-authentication.md#where-to-store-a-token) (`HttpOnly`, `Secure`, `SameSite`); logout clears tokens and the query cache.
- [ ] CORS configured narrowly (no `*` with credentials), or API served same-origin ([routing the API](./02-deployment.md#routing-the-api)).
- [ ] Third-party API keys restricted (allowed domains, quotas); CDN scripts use SRI where possible.
- [ ] Source maps are **not publicly served** (`hidden` + uploaded to monitoring, then removed from `dist/`) ([source maps](./06-error-monitoring-and-logging.md#source-maps)).
- [ ] Redirect targets (`next`, `returnTo`) validated as internal paths.
- [ ] Dependencies audited; lockfile committed; new packages reviewed ([supply chain](./05-security.md#supply-chain)).
- [ ] CI uses least-privilege permissions, scoped secrets, and pinned actions ([pipeline security](./04-ci-cd.md#pipeline-security)).
- [ ] No sensitive data in URLs, `localStorage`, logs, analytics, or error reports.
- [ ] Containers (if used) run as non-root, with no secrets in image layers, and are scanned ([Docker hardening](./03-docker.md#security-hardening)).

## 3. Performance

- [ ] Bundle analyzed; no surprise giant dependency; chunk sizes within budget ([bundle optimization](../14-performance/05-bundle-optimization.md)).
- [ ] Routes and heavy features are **code-split** ([code splitting](../14-performance/03-code-splitting-and-lazy-loading.md)); critical path is lean.
- [ ] Hashed assets served with **long-lived immutable caching**; `index.html` and runtime config `no-cache` ([caching headers](./02-deployment.md#caching-headers)).
- [ ] Brotli/gzip compression and HTTP/2 or 3 enabled ([network performance](../14-performance/06-network-performance.md)).
- [ ] Images: modern formats, responsive sizes, explicit dimensions, lazy loading below the fold, and the **LCP image not lazy-loaded** with `fetchpriority="high"`.
- [ ] Fonts subset, `font-display: swap`, minimal preloads.
- [ ] Core Web Vitals checked on a **throttled mobile profile** for key pages ([profiling](../14-performance/00-profiling-and-measuring.md#test-under-realistic-conditions)); no major long tasks on critical interactions.
- [ ] No request waterfalls on key screens; server-state caching (`staleTime`) tuned ([caching](../12-server-state/04-caching-and-synchronization.md)).
- [ ] Long lists paginated or virtualized ([virtualization](../14-performance/04-virtualization.md)).
- [ ] Third-party scripts audited and loaded `async`/`defer`.

## 4. Accessibility and user experience

- [ ] Keyboard navigation works for all critical flows, with visible focus and no traps ([focus management](../08-accessibility/02-keyboard-and-focus-management.md)).
- [ ] Semantic HTML, labelled form controls, and meaningful alt text; icon-only buttons have accessible names.
- [ ] Color contrast meets WCAG AA in light **and** dark themes; information isn't conveyed by color alone.
- [ ] Route changes announce/move focus and update `document.title` ([navigation](../10-routing/03-navigation.md#scroll-and-focus)).
- [ ] Tested with a screen reader on at least the main flow, plus an automated scan ([accessibility checklist](../08-accessibility/04-accessibility-checklist.md)).
- [ ] `prefers-reduced-motion` respected; text scales to 200% without breaking.
- [ ] **Every screen has designed loading, empty, and error states** with a next step ([loading and error states](../12-server-state/02-loading-and-error-states.md)).
- [ ] Responsive layouts verified on real small screens; touch targets are usable.
- [ ] `lang` (and `dir` for RTL) set on `<html>`; user-facing strings externalized if you localize ([i18n](../16-advanced-react/03-internationalization.md)).
- [ ] Browser support matches your stated targets (build `target`, polyfills only if needed).

## 5. Metadata, SEO, and sharing

- [ ] Meaningful `<title>`, description, favicon, and `theme-color`.
- [ ] Open Graph / social preview tags, if public pages get shared (link previews don't run your JavaScript; consider prerendering) ([SEO note](./02-deployment.md#seo-and-previews-of-a-client-rendered-app)).
- [ ] `robots.txt` correct: crawlable in production, **`noindex` on staging and previews**.
- [ ] Sitemap and canonical URLs, if search traffic matters.
- [ ] Custom 404 page for unknown routes (the `*` route) ([routing](../10-routing/00-react-router.md#how-matching-works)).

## 6. Reliability and error handling

- [ ] **(blocker)** A **root error boundary** exists, so a render error never produces a blank page ([error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md)); route-level boundaries/`errorElement`s on key routes.
- [ ] API failures handled by type (network, 4xx, 5xx) with retry only where safe ([API errors](../11-api-integration/05-api-error-handling.md)).
- [ ] **Session expiry and token refresh** tested: expired token → refresh → retry; refresh failure → clean redirect to login ([refresh flow](../11-api-integration/04-refresh-token-flow.md)).
- [ ] **Stale-deployment chunk errors** handled (reload once, error boundary with a Reload button) ([chunk errors](../14-performance/03-code-splitting-and-lazy-loading.md#chunk-load-errors-and-deployments)).
- [ ] Offline/slow-network behavior tested (DevTools throttling and offline).
- [ ] Forms prevent double submission; destructive actions confirmed or undoable.
- [ ] Graceful behavior when the backend is down (clear message, not an infinite spinner).
- [ ] Feature flags/kill switches for risky new features, so you can disable without a deploy.

## 7. Testing and CI

- [ ] **(blocker)** CI is green on the exact commit being released, and `main` is protected with required checks ([branch protection](./04-ci-cd.md#branch-protection-and-required-checks)).
- [ ] Unit/integration tests cover the critical flows and the known past bugs ([integration testing](../18-testing-and-debugging/05-integration-testing.md)).
- [ ] **E2E smoke tests** pass against the deployed build of the critical journeys (sign up/login, core create-edit-delete, checkout) ([Playwright](../18-testing-and-debugging/06-e2e-testing-playwright.md)).
- [ ] Manual exploratory pass on staging, in at least two browsers and one real mobile device.
- [ ] No skipped/`only` tests left behind; flaky tests fixed or quarantined with an owner.
- [ ] The **same artifact** tested on staging is the one promoted to production ([build once, deploy many](./04-ci-cd.md#build-once-deploy-many)).

## 8. Deployment and infrastructure

- [ ] **(blocker)** **SPA fallback** configured and tested on the deployed site (deep link + refresh) ([fallback](./02-deployment.md#spa-fallback-routing)); missing assets return real 404s.
- [ ] **(blocker)** **Rollback plan** exists and has been rehearsed: you know exactly how to restore the previous version, and how long it takes ([rollbacks](./02-deployment.md#rollbacks)).
- [ ] Previous release's assets still available so open tabs don't hit chunk errors.
- [ ] Backend/API changes are **backwards-compatible** with both the old and new frontend during rollout.
- [ ] Environments separated (preview/staging/production) with separate data and keys; staging never touches production data.
- [ ] Deploys are automated and traceable (who, what commit, when); no manual file copying.
- [ ] Domain, DNS, certificate expiry, and redirects (`www`/apex, HTTP→HTTPS) verified; certificate renewal monitored.
- [ ] Health endpoint and basic uptime monitoring for the site and its API ([Docker health checks](./03-docker.md#health-checks)).
- [ ] Capacity/rate limits and CDN settings reviewed for expected launch traffic.

## 9. Observability

- [ ] **(blocker)** **Error monitoring is live** in production with source maps uploaded and a verified test error ([monitoring](./06-error-monitoring-and-logging.md#monitoring-the-monitor)).
- [ ] Errors tagged with **release and environment**; user context uses opaque IDs ([context](./06-error-monitoring-and-logging.md#context-that-makes-errors-actionable)).
- [ ] **Alerts** configured for new issues, regressions, and error spikes, routed to someone who will see them ([alerting](./06-error-monitoring-and-logging.md#alerting-and-triage)).
- [ ] **Real-user performance metrics** (LCP, INP, CLS) collected, segmented by route and release ([performance monitoring](./07-performance-monitoring.md)).
- [ ] API latency and error rate tracked.
- [ ] A dashboard exists that you can open in 10 seconds during an incident.
- [ ] Release markers sent to monitoring on each deploy.
- [ ] Product analytics (if used) respect consent and don't collect personal data unnecessarily.

## 10. Privacy and legal

- [ ] Privacy policy and terms published and linked; cookie/analytics **consent** implemented where required.
- [ ] Personal data inventory: you know what you collect, where it goes (including third parties like analytics, error tracking, session replay), and for how long.
- [ ] Session replay and error reports **mask sensitive data** ([scrubbing](./06-error-monitoring-and-logging.md#privacy-and-data-scrubbing)).
- [ ] Data deletion/export processes exist if required by regulation.
- [ ] Third-party licenses reviewed; attribution included where needed.

## 11. Operations and people

- [ ] **Owner** named for the app and for the launch; on-call or escalation path defined.
- [ ] **Runbook** for common problems: site down, bad deploy, login broken, third-party outage, expired certificate.
- [ ] Communication plan: where status is posted, who tells users, who tells stakeholders.
- [ ] Support team knows what's changing and how to ask users for version/browser/steps; the app shows its version.
- [ ] Credentials and access reviewed: who can deploy, who has production access, no shared logins, former team members removed.
- [ ] Backups and recovery verified for any data you own (and the backend team's are confirmed, not assumed).

## Launch day

**Before**

- [ ] Freeze unrelated changes. Re-run the checklist against **staging**.
- [ ] Confirm rollback owner and procedure; confirm who's watching dashboards.
- [ ] Deploy at a time when people are available to respond (not Friday evening).

**Deploy**

- [ ] Ship through the pipeline. Confirm the correct commit/artifact and environment config.
- [ ] Run the **post-deploy smoke test** on production ([after deploying](./02-deployment.md#after-deploying)).
- [ ] Verify headers, deep links, login, one real transaction (with a test account).

**First hour / first day**

- [ ] Watch error rate, new issues, API errors, and Web Vitals against the previous release.
- [ ] Check logs for CSP violation reports, failed asset loads, and chunk errors.
- [ ] Be ready to **roll back first, investigate second** if key metrics degrade.
- [ ] Triage incoming feedback and error reports; fix or document quickly.

**First week**

- [ ] Review field performance data (p75), error trends, and user feedback.
- [ ] Tighten what you loosened (CSP report-only → enforce, extra logging, temporary flags).
- [ ] Hold a short **retrospective**: what went wrong, what almost did, what to automate next time.

## Ongoing (calendar these)

| Cadence | Task |
|---|---|
| Every deploy | CI gates, smoke test, release marker, watch dashboards |
| Weekly | Triage new errors; review dependency update PRs |
| Monthly | Dependency audit, performance trend review, access review |
| Quarterly | Rollback drill, restore test, security header/CSP review, bundle size review |
| Before expiry | Certificates and domains, API keys and tokens rotation |
| After every incident | Write it up, add a test or alert so it can't silently recur |

## Common mistakes

- **Treating the checklist as a formality** (ticking boxes without evidence).
- **Checking only locally**, not the **deployed** environment and its headers.
- **No rehearsed rollback**, so the first attempt happens during an outage.
- **Launching without monitoring** (or with monitoring never verified end to end).
- **Shipping on Friday afternoon** with nobody watching.
- **Forgetting backwards compatibility** between the new frontend and the running backend.
- **Skipping accessibility, error, and empty states** because the happy path demos well.
- **A one-time hardening effort** that decays: no dependency updates, expired certificates, stale access.
- **Leaving launch-time shortcuts permanent** (report-only CSP, debug flags, broad CORS).
- **No owner**, so everything is "someone's" job.

## Quick summary

- Run this list against **staging, then production**, with **evidence** for each item. Blockers don't ship.
- The big ones: **no secrets in the client, server-side authorization, HTTPS and security headers, SPA fallback and correct caching, a root error boundary, live and verified error monitoring, CI green, and a rehearsed rollback**.
- Check **performance** on throttled mobile, **accessibility** with a keyboard and screen reader, and **error/empty/offline states** on purpose.
- Launch with people watching, a smoke test, release markers, and a **roll-back-first** mindset.
- Production readiness isn't a moment, it's a **routine**: recurring audits, rotations, drills, and post-incident follow-ups.

## Next

Continue to [20 — Frontend architecture](../20-frontend-architecture/README.md).