# Error Monitoring and Logging

In development you see errors in your own console. In production you see **nothing**. Users hit errors on devices, browsers, networks, and data you've never tested, and most of them won't report anything. They'll just leave. **Error monitoring** makes those failures visible to you, with enough context to fix them.

The goal isn't "capture everything". It's: **find out about real problems before users complain, and have enough information to fix them quickly.**

## What to capture

| Source | How it surfaces |
|---|---|
| **Uncaught JavaScript errors** | `window.onerror` / `error` event |
| **Unhandled promise rejections** | `unhandledrejection` event |
| **React render errors** | [Error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md) and React 19's root error hooks |
| **Failed API calls that signal bugs** | 5xx responses, unexpected response shapes, contract mismatches ([API errors](../11-api-integration/05-api-error-handling.md)) |
| **Handled-but-important failures** | A payment step that fell back, a retry that gave up (`captureException` explicitly) |
| **Performance problems** | Slow interactions, long tasks ([07](./07-performance-monitoring.md)) |

What *not* to capture: expected, user-caused conditions (validation errors, 401s handled by refresh, 404s for missing records the UI already explains), request cancellations (`AbortError`), and noise from things you don't control (below).

### The low-level hooks

Everything a monitoring SDK does starts from these browser events:

```ts
window.addEventListener("error", (event) => report(event.error ?? event.message))
window.addEventListener("unhandledrejection", (event) => report(event.reason))
```

You rarely write this yourself. An SDK installs them, adds breadcrumbs and context, deduplicates, batches, and retries. Writing your own is a good way to understand it, and a poor way to run production.

## Using a monitoring service

Options include **Sentry**, Datadog RUM, Bugsnag, Rollbar, LogRocket, and others. They overlap in capability, so pick on cost, privacy requirements, integrations, and team familiarity. The examples use Sentry because it's widely used, but the concepts transfer. SDK option names change between versions, so confirm against the current docs.

```bash
npm install @sentry/react
```

```tsx
// src/monitoring.ts: initialize BEFORE rendering the app
import * as Sentry from "@sentry/react"

Sentry.init({
  dsn: import.meta.env.VITE_SENTRY_DSN,          // public by design (see env vars note)
  environment: import.meta.env.MODE,             // "production" | "staging"
  release: __APP_VERSION__,                      // commit SHA (see build note: version stamping)
  enabled: import.meta.env.PROD,                 // no noise from local development
  integrations: [Sentry.browserTracingIntegration()],
  tracesSampleRate: 0.1,                         // sample 10% of transactions
  ignoreErrors: [/ResizeObserver loop/, "Non-Error promise rejection captured"],
  denyUrls: [/extensions\//i, /^chrome:\/\//i, /^moz-extension:\/\//i],
  beforeSend(event) {
    return scrub(event)                          // see privacy below
  },
})
```

```tsx
// main.tsx: React 19 root hooks report everything React catches or surfaces
createRoot(rootElement, {
  onUncaughtError: Sentry.reactErrorHandler(),
  onCaughtError: Sentry.reactErrorHandler(),
  onRecoverableError: Sentry.reactErrorHandler(),
}).render(<App />)
```

For React 18 and earlier, wrap parts of the tree in Sentry's `ErrorBoundary` (or report from `componentDidCatch`/`react-error-boundary`'s `onError`). The root hooks are the cleanest central place in React 19 ([error boundaries](../15-concurrent-and-modern-react/04-error-boundaries.md#react-19-root-level-error-hooks)).

### Reporting handled errors

```ts
try {
  await submitPayment(order)
} catch (error) {
  Sentry.captureException(error, {
    tags: { feature: "checkout" },
    extra: { orderId: order.id },        // IDs, not personal data
  })
  showPaymentFailed()                    // the user still gets a graceful UI
}
```

Report errors you **handle but need to know about**: those that indicate a bug or an outage, not routine user mistakes.

## Source maps

Production code is minified, so a stack trace reads `at a (index-a1b2c3.js:1:48271)`. **Source maps** translate that back to `ProjectList.tsx:42`. Without them, monitoring is nearly useless.

The recommended setup ([build](./01-build.md#sourcemap)):

1. Build with **`sourcemap: "hidden"`**: maps are generated but not referenced by the JS.
2. **Upload the maps** to your monitoring service during the build, tagged with the **same release** you pass to `Sentry.init`.
3. **Delete the `.map` files** from `dist/` before deploying, so they aren't public.

```ts
// vite.config.ts
import { sentryVitePlugin } from "@sentry/vite-plugin"

export default defineConfig({
  build: { sourcemap: "hidden" },
  plugins: [
    react(),
    sentryVitePlugin({
      org: process.env.SENTRY_ORG,
      project: process.env.SENTRY_PROJECT,
      authToken: process.env.SENTRY_AUTH_TOKEN,        // a BUILD secret: no VITE_ prefix (see CI/CD note)
      release: { name: process.env.GITHUB_SHA },
      sourcemaps: { filesToDeleteAfterUpload: ["./dist/**/*.map"] },
    }),
  ],
})
```

Details differ per vendor and plugin version. The essential rules don't:

- **Release names must match** between the uploaded maps and the SDK, or traces stay minified.
- **Upload before (or as) you deploy.** Errors arriving before the maps exist can't be symbolicated retroactively in some setups.
- Keep maps for as long as old releases may still be running in users' browsers.
- **Never leave public `.map` files** unless you're comfortable publishing your source.

## Context that makes errors actionable

A stack trace says *where*. Context says *who, when, and what led there*. Attach:

| Context | Why |
|---|---|
| **Release** (commit SHA) | Which deploy introduced it? Is it fixed in a newer one? |
| **Environment** | Don't mix staging noise with production |
| **User ID** (opaque, not email/name) | How many users? Same user repeatedly? Contact them for repro |
| **Route/URL** (without sensitive query params) | Which screen |
| **Breadcrumbs** | The trail before the error: clicks, navigation, console messages, network calls |
| **Tags** | Feature, tenant/plan, flag variants, browser, device |
| **App state hints** | The relevant IDs and feature flags, not whole stores |

```ts
Sentry.setUser({ id: user.id })                   // on login; Sentry.setUser(null) on logout
Sentry.setTag("plan", user.plan)
Sentry.addBreadcrumb({ category: "checkout", message: "payment submitted", level: "info" })
```

SDKs add many breadcrumbs automatically (clicks, `fetch`/XHR calls, console output, navigation). Add custom ones for **business-significant steps**. They turn "TypeError in `Cart`" into "the user applied a coupon, then removed an item, then crashed".

## Reducing noise

An alerting system people ignore is worse than none. A flood of irrelevant errors buries real ones. Common sources:

- **Browser extensions** injecting scripts that throw. Filter with `denyUrls` and `allowUrls` (only count errors from your own bundle's URLs).
- **`ResizeObserver loop` warnings**: benign browser noise.
- **Network failures and `Failed to fetch`** from users going offline or ad blockers blocking requests.
- **Bots and old/unsupported browsers.**
- **Cancelled requests** (`AbortError`) and navigation-triggered aborts.
- **Third-party script errors** (often just "Script error." with no detail).
- **The same error repeating thousands of times.** Grouping and rate limits help, but fix the cause.

Tactics: `ignoreErrors`, `denyUrls`, `beforeSend` filtering, **sampling**, using **fingerprints** to group related errors correctly (or split ones lumped together), and **regularly triaging**: assign, fix, or ignore-with-reason. Noise you consciously decide to ignore is fine. Noise you never look at is the problem.

## Alerting and triage

Monitoring is only valuable if someone acts on it.

- **Alert on what matters**: a **new** issue in production, a **regression** (a resolved issue reappearing), a **spike** in error rate after a deploy, and errors on **critical flows** (login, checkout).
- **Don't alert on every error.** Route alerts to a channel with an owner. Use thresholds (more than N events or M users in X minutes).
- **Tie errors to releases**, so "error rate jumped after v1.8.2" leads to an immediate rollback decision ([deployment rollbacks](./02-deployment.md#rollbacks)).
- **Triage regularly**: new issues get an owner and a priority; stale ones get closed. Track a simple measure like "crash-free sessions" or error rate per release.
- Write a **runbook** for common incidents: how to check status, roll back, and who to contact.

## Logging

**Logging** is recording what the app is doing. In a browser app, logs are far less useful than in a server, because they live on the user's device. So in production, "logging" mostly means **sending meaningful events to your monitoring tool** as breadcrumbs or messages, not writing to the console.

### A thin logger

Route all logging through one small module so you can change behavior in one place:

```ts
// src/lib/logger.ts
import * as Sentry from "@sentry/react"

type Context = Record<string, unknown>

export const logger = {
  debug(message: string, context?: Context) {
    if (import.meta.env.DEV) console.debug(message, context)
  },
  info(message: string, context?: Context) {
    if (import.meta.env.DEV) console.info(message, context)
    Sentry.addBreadcrumb({ level: "info", message, data: context })
  },
  warn(message: string, context?: Context) {
    if (import.meta.env.DEV) console.warn(message, context)
    Sentry.addBreadcrumb({ level: "warning", message, data: context })
  },
  error(message: string, error?: unknown, context?: Context) {
    if (import.meta.env.DEV) console.error(message, error, context)
    Sentry.captureException(error ?? new Error(message), { extra: { message, ...context } })
  },
}
```

- **Levels** matter: `debug` (dev only), `info` and `warn` as breadcrumbs, `error` as reported events.
- **Structured context** (an object) is searchable. Free-form string concatenation isn't.
- An ESLint `no-console` rule keeps raw `console.log` out of production code.
- Never log **secrets or personal data** ([below](#privacy-and-data-scrubbing)).

## Privacy and data scrubbing

Error reports and logs can accidentally collect **passwords, tokens, personal data, and payment details**: in URLs, request bodies, breadcrumbs, DOM snapshots, form values, and user objects.

- **Don't attach personal data** to events. Use opaque IDs, not emails or names.
- **Scrub before sending** with `beforeSend`/`beforeBreadcrumb`:

```ts
function scrub(event: Sentry.ErrorEvent): Sentry.ErrorEvent {
  if (event.request?.url) event.request.url = event.request.url.replace(/([?&](token|key|code)=)[^&]+/gi, "$1[redacted]")
  if (event.request?.headers) delete event.request.headers["Authorization"]
  if (event.user) event.user = { id: event.user.id }       // drop email/IP/username
  return event
}
```

- **Session replay and DOM-capturing tools** record what users see. Mask all inputs and sensitive text by default, and block entire sensitive screens (payment, health, admin).
- **Check what your SDK collects by default** (IP addresses, cookies, request bodies) and disable what you don't need.
- Where regulations such as GDPR apply, understand **consent, data residency, retention, and your vendor's data processing terms** ([security](./05-security.md#privacy-and-compliance-briefly)). Involve whoever owns compliance.
- Set **retention** limits so old data doesn't pile up.
- Never log or report **tokens, passwords, or full request/response bodies** from authenticated endpoints.

## Monitoring the monitor

- **Verify it works**: after setup, trigger a test error in a deployed non-production build and confirm it arrives with a readable stack and the right release. Do the same after build or tooling changes. Silent breakage (a changed release name, failed map upload) is common.
- **Make the pipeline fail if the source map upload fails** ([CI/CD](./04-ci-cd.md#deploy-jobs)), or at least alert.
- **Don't let monitoring break the app**: SDK initialization failures shouldn't prevent rendering. Wrap init defensively, and keep `tracesSampleRate`/replay modest for performance.
- **Ad blockers** block many monitoring domains. Some teams proxy the SDK through their own domain ("tunnel") to avoid losing reports. Weigh the privacy and complexity trade-offs.
- Keep an eye on **cost**: event volume and replay can be expensive. Use sampling and filters.

## Common mistakes

- **No monitoring in production**, learning about bugs from angry users.
- **No source maps**, or public ones, so traces are unreadable or source is exposed.
- **Release mismatch** between uploaded maps and the SDK's `release`.
- **No release/environment tagging**, so you can't tell staging from production or which deploy broke things.
- **Capturing everything**, drowning real issues in extension noise, aborts, and expected errors.
- **Alerting on every event**, so alerts get muted and ignored.
- **Collecting personal data or secrets** in events, breadcrumbs, replays, or URLs.
- **Swallowing errors** (`catch {}`) so nothing is reported, or catching and re-throwing without context.
- **Raw `console.log`** as the "logging strategy".
- **Never testing that reporting works**, and discovering during an incident that it silently broke.
- **No owner or triage process**, so issues pile up unread.
- **Treating monitoring as a substitute for tests**. It tells you what broke *after* users hit it.

## Quick summary

- Production errors are invisible unless you capture them: **uncaught errors, unhandled rejections, React render errors** (via error boundaries and React 19's root hooks), and important handled failures.
- Use an SDK (Sentry or equivalent) initialized **before** render, with `environment`, **`release`** (commit SHA), sampling, and filters.
- **Hidden source maps**, uploaded with a matching release and removed from `dist/`, make traces readable without publishing source.
- Add **context**: release, environment, opaque user ID, route, tags, and breadcrumbs.
- **Cut noise** (extensions, `ResizeObserver`, aborts, expected errors) and **alert sparingly** on new issues, regressions, spikes, and critical flows; triage regularly.
- Route logging through a **thin logger**; send meaningful events to monitoring instead of relying on the console.
- **Scrub personal data and secrets**, mask replays, and verify the whole pipeline works end to end.

## Next

[07 — Performance monitoring](./07-performance-monitoring.md)