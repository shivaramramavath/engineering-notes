# Performance Monitoring

[Profiling](../14-performance/00-profiling-and-measuring.md) tells you how fast the app is **on your machine, today**. **Performance monitoring** tells you how fast it is **for your real users, continuously**: across their phones, networks, and locations, and after every release. Without it, performance quietly degrades one dependency at a time and you find out from a support ticket.

## Two kinds of data

| | Lab (synthetic) | Field (real-user monitoring, RUM) |
|---|---|---|
| Source | Lighthouse, DevTools, scheduled test runs | Real visitors' browsers |
| Conditions | Controlled and repeatable | Wildly varied devices, networks, data |
| Good for | **Debugging**, catching regressions before release, comparing changes | **Knowing the truth**, finding who is affected and where |
| Weakness | Doesn't reflect real diversity | Noisy; you can't reproduce each visit |

You need both: **lab data to find and fix, field data to confirm it matters.** Google's own ranking and Core Web Vitals reports use field data.

## What to measure

The Core Web Vitals ([definitions and thresholds](../14-performance/00-profiling-and-measuring.md#what-to-measure-core-web-vitals)) are the standard starting set:

| Metric | Measures | Points at |
|---|---|---|
| **LCP** (Largest Contentful Paint) | Loading speed of main content | Bundle size, slow API, hero image, render-blocking resources |
| **INP** (Interaction to Next Paint) | Responsiveness to clicks, taps, and keys | Long tasks, slow re-renders, heavy event handlers |
| **CLS** (Cumulative Layout Shift) | Visual stability | Images without dimensions, late-loading content, font swaps |

Supporting metrics: **TTFB** (server and network latency), **FCP** (first content), and **long tasks** (main-thread blocking).

Then add **app-specific metrics** for what matters in *your* product:

- Time until the **first chart/list/dashboard** is usable
- **Search latency**, **checkout step durations**, **time to first message** in chat
- **API latency and error rate** per endpoint, from the client's point of view
- **Route transition time** (click → new screen ready)
- **Bundle/chunk load failures**

## Collecting field data with `web-vitals`

Google's small [`web-vitals`](https://github.com/GoogleChrome/web-vitals) library measures the Core Web Vitals the same way Chrome does, handling the tricky details (back/forward cache, tab visibility, when to report).

```bash
npm install web-vitals
```

```ts
// src/lib/vitals.ts
import { onCLS, onINP, onLCP, onFCP, onTTFB, type Metric } from "web-vitals"

function report(metric: Metric) {
  const body = JSON.stringify({
    name: metric.name,                     // "LCP" | "INP" | "CLS" | ...
    value: metric.value,
    rating: metric.rating,                 // "good" | "needs-improvement" | "poor"
    id: metric.id,                         // unique per page load
    navigationType: metric.navigationType,
    route: window.location.pathname,       // consider normalizing: /projects/:id
    release: __APP_VERSION__,
    connection: (navigator as any).connection?.effectiveType,   // not available in all browsers
  })

  // sendBeacon survives page unload; fall back to fetch with keepalive
  if (!(navigator.sendBeacon && navigator.sendBeacon("/api/vitals", body))) {
    fetch("/api/vitals", { method: "POST", body, keepalive: true })
  }
}

export function initVitals() {
  onCLS(report); onINP(report); onLCP(report); onFCP(report); onTTFB(report)
}
```

Call `initVitals()` once at startup. Send to your own endpoint, your analytics, or your monitoring vendor (many have this built in).

Practical notes:

- **Report once per page load** (the library does, at the right moment, usually when the page is hidden) and use `sendBeacon`/`keepalive` so data isn't lost when users close the tab.
- **Normalize routes** (`/projects/42` → `/projects/:id`), or you'll create thousands of unique "pages". With React Router you can read the matched route pattern.
- **Include the release**, device class, and connection type so you can slice data.
- **Sample** at high traffic (for example 10–50% of sessions) if volume or cost matters.
- Respect **privacy and consent**: don't attach personal data, and follow your analytics consent rules ([privacy](./06-error-monitoring-and-logging.md#privacy-and-data-scrubbing)).

### Attribution: *why* is it slow?

A number alone ("INP is 450 ms") doesn't tell you what to fix. The attribution build adds diagnostic detail:

```ts
import { onINP, onLCP, onCLS } from "web-vitals/attribution"

onINP((metric) => {
  const a = metric.attribution
  report({ ...metric, extra: {
    target: a.interactionTarget,           // which element was interacted with
    type: a.interactionType,               // pointer / keyboard
    inputDelay: a.inputDelay,              // main thread busy before the handler ran
    processingDuration: a.processingDuration,   // your event handlers + React render
    presentationDelay: a.presentationDelay,     // work before the next paint
  }})
})

onLCP((metric) => report({ ...metric, extra: {
  element: metric.attribution.target,
  ttfb: metric.attribution.timeToFirstByte,
  loadDelay: metric.attribution.resourceLoadDelay,
  loadDuration: metric.attribution.resourceLoadDuration,
  renderDelay: metric.attribution.elementRenderDelay,
}}))
```

Breaking LCP into **TTFB → resource load delay → load duration → render delay** immediately tells you whether the problem is the server, a late-discovered image, a heavy file, or JavaScript blocking render. INP's phases tell you whether it's a busy main thread, slow handlers, or slow rendering/painting. (Field names evolve between `web-vitals` versions, so check the library's docs for your version.)

## Reading the data

- **Use percentiles, not averages.** Averages hide the slow tail where users actually suffer. Core Web Vitals are assessed at the **75th percentile** (p75). Look at p75 and p95.
- **Segment**: by route, device class (mobile vs desktop), connection, country, browser, **release**, and logged-in vs anonymous. A "fine" overall number often hides a terrible segment (mobile on slow networks).
- **Look at trends and distributions**, not single numbers. Is the "poor" bucket growing?
- **Compare across releases.** Performance regressions usually have a deploy behind them. A release tag on every metric makes the culprit obvious ([deployment](./02-deployment.md#after-deploying)).
- **Mind sample size.** A route with 20 visits doesn't have a meaningful p75.
- **Know the bias**: some metrics aren't reported by every browser (for example, certain APIs are Chromium-only), and users who bounce early may never report.

### Public data for public sites

For publicly accessible sites, Google provides real-user Chrome data without any instrumentation: **PageSpeed Insights** and the **Chrome UX Report (CrUX)**. They show field metrics (at the 75th percentile) per URL or origin. Useful as a benchmark and an SEO signal, but they only cover sites with enough traffic and don't give your per-release or per-route detail.

## Tracing and API performance

Slow screens are often slow **APIs**, or slow chains of them.

- **Measure request durations in your API client** and report them: endpoint (normalized), status, duration, payload size ([API client](../11-api-integration/02-api-client.md)). A p95 latency per endpoint tells you which backend call hurts users most.
- **Distributed tracing** (Sentry tracing, OpenTelemetry for web) connects a frontend action to the backend requests it triggered, using trace headers. It answers "the click took 3 s: where did the time go?" across the stack. It needs backend cooperation and care with CORS headers for trace propagation, so check the vendor docs.
- **Browser timing APIs** give raw data: `performance.getEntriesByType("navigation")` (page load phases), `"resource"` (each request's timing), `"longtask"`, and Chromium's *Long Animation Frames* entries for diagnosing slow frames (browser support varies, so feature-detect).
- **Custom measures** for your own flows:

```ts
performance.mark("search-start")
// …user searches, results render…
performance.mark("search-done")
const m = performance.measure("search", "search-start", "search-done")
report({ name: "search_latency", value: m.duration })
```

Mark at meaningful moments (route change start, "data ready", "first meaningful content visible") and report the durations. They show up in the DevTools Performance panel too.

## Synthetic monitoring and CI

Lab tests catch problems **before users do**.

- **Lighthouse CI** runs Lighthouse on key pages in your pipeline and can fail the build when scores or metrics regress past thresholds:

```json
// lighthouserc.json
{
  "ci": {
    "collect": { "url": ["http://localhost:4173/", "http://localhost:4173/login"], "numberOfRuns": 3 },
    "assert": {
      "assertions": {
        "largest-contentful-paint": ["error", { "maxNumericValue": 2500 }],
        "cumulative-layout-shift": ["error", { "maxNumericValue": 0.1 }],
        "total-byte-weight": ["warn", { "maxNumericValue": 1500000 }]
      }
    }
  }
}
```

- **Bundle size budgets** (a tool like `size-limit`, or a script checking build output) fail PRs that bloat the bundle ([bundle optimization](../14-performance/05-bundle-optimization.md#set-a-budget-and-watch-it)).
- **Scheduled synthetic checks** against production (a Lighthouse or Playwright script run every hour from a few regions) catch outages and regressions that field data would take days to reveal, and give a consistent baseline.
- Lab results vary run to run, so use several runs, a stable environment, and generous thresholds to avoid flaky gates ([CI quality gates](./04-ci-cd.md#quality-gates-beyond-tests)).

## Dashboards and alerts

- A **dashboard** with p75 LCP/INP/CLS over time, split by route and device, plus API latency and error rate, and **release markers**.
- **Alerts** on meaningful regressions: p75 LCP up more than X% week over week, INP crossing the "poor" threshold on a key route, or a spike in slow API responses after a deploy. Alert on **sustained** change with enough sample size, not single noisy points.
- **Budgets by route**: your checkout flow may deserve a stricter target than an admin settings page.
- Review performance as part of **release and sprint rituals**, not just during crises.

## From measurement to action

```text
Dashboard shows p75 INP regressed on /reports after release 1.8.2
   │
   ▼ segment: mostly mobile, mostly "filter change" interactions
   ▼ attribution: long processingDuration on the filter button
   ▼ reproduce in the lab with CPU throttling ([profiling](../14-performance/00-profiling-and-measuring.md))
   ▼ Profiler: a table re-rendering 5,000 rows on each keystroke
   ▼ fix: virtualization / transition ([virtualization](../14-performance/04-virtualization.md), [transitions](../15-concurrent-and-modern-react/01-transitions.md))
   ▼ ship, watch p75 INP return to normal in the next release's data
```

The loop: **detect (field) → locate (attribution, segments) → reproduce (lab) → fix → verify (field)**.

## Cost and privacy

- Metrics are cheap, but **session replay and full tracing are not**. Sample them.
- **Don't collect personal data** in performance events (URLs with IDs or tokens, user emails). Normalize routes and scrub query strings.
- Respect **consent requirements**. Some regions require consent for analytics-type collection, even when anonymous.
- Make sure the monitoring script itself isn't a **performance problem**. Load it efficiently (small, async, deferred where possible) and keep the SDK's footprint in your [bundle budget](../14-performance/05-bundle-optimization.md).

## Common mistakes

- **Only measuring in the lab** (or only on a fast laptop), and being surprised by field results.
- **Looking at averages** instead of p75/p95 and segments.
- **No release tagging**, so you can't tie a regression to a deploy.
- **Too many unique routes in metrics** (un-normalized URLs with IDs).
- **Reporting data you can't act on**: a number with no attribution or segmentation.
- **Losing data on page unload** (using plain `fetch` instead of `sendBeacon`/`keepalive`).
- **Flaky performance gates in CI** (single runs, tight thresholds), so people ignore or disable them.
- **Alerting on noise** (small samples, single spikes).
- **Ignoring API latency** and blaming React for slow backend calls.
- **Collecting personal data or full URLs** in performance events.
- **Never checking after the fix**, so you don't know whether it worked in the field.
- **Treating performance as a one-time project** rather than something that regresses constantly.

## Quick summary

- **Lab data finds and fixes; field data (RUM) confirms real impact.** You need both.
- Track **LCP, INP, CLS** (plus TTFB/FCP) and **app-specific metrics** and **API latency** that reflect your product.
- Use **`web-vitals`** to report to your own endpoint or vendor with `sendBeacon`; include **release, normalized route, device and connection**; use the **attribution** build to see *why* a metric is poor.
- Read **percentiles (p75/p95) and segments**, compare **across releases**, and watch sample sizes.
- Add **tracing/timing** for APIs and custom `performance.mark/measure` for key user flows.
- Guard regressions in CI (**Lighthouse CI**, **bundle budgets**) and with **scheduled synthetic checks**.
- Set **dashboards and alerts**, and close the loop: detect → locate → reproduce → fix → verify.

## Next

[08 — Production checklist](./08-production-checklist.md)