# Profiling and Measuring

The first rule of performance: **you can't fix what you haven't measured.** Intuition about what's slow is wrong surprisingly often. The component you suspect renders in 0.3ms; the 600 KB charting library nobody remembered is the real cost.

## A repeatable process

```text
1. Define the problem    "typing in search lags", "dashboard takes 6s to show content"
2. Measure a baseline    numbers, on a realistic device/network, production-like build
3. Find the bottleneck   profiler / network panel / bundle analyzer
4. Change ONE thing
5. Measure again         did the number improve? by how much?
6. Keep it or revert it
```

Skip step 5 and you accumulate complexity (memo wrappers, lazy boundaries) that buys nothing.

## Test under realistic conditions

Your dev machine is a poor benchmark.

- **Use a production build.** Development builds include extra checks, warnings, and StrictMode's double rendering, so React is much slower in dev. Measure real timings with `vite build && vite preview`.
- **Throttle the CPU.** In Chrome DevTools → Performance → gear icon → *CPU: 4× or 6× slowdown*. That approximates a mid-range phone.
- **Throttle the network.** Network panel → *Fast 4G* / *Slow 4G*.
- **Disable cache** (Network panel) to see first-visit behavior, and test a warm cache too.
- **Disable extensions** (or use an incognito window); they distort timings.
- If you can, test on an actual low-end phone.

## What to measure: Core Web Vitals

Google's field metrics for user experience:

| Metric | Measures | "Good" threshold |
|---|---|---|
| **LCP**, Largest Contentful Paint | How fast the main content appears | ≤ 2.5 s |
| **INP**, Interaction to Next Paint | Responsiveness: delay between an interaction and the next frame | ≤ 200 ms |
| **CLS**, Cumulative Layout Shift | Visual stability: how much content jumps | ≤ 0.1 |

INP replaced the older FID metric in 2024. Check the current thresholds at [web.dev/vitals](https://web.dev/vitals), since they're occasionally revised.

Which problem maps to which metric:

- **Poor LCP** → load problem: big bundles, slow API, unoptimized hero image, render-blocking resources ([03](./03-code-splitting-and-lazy-loading.md), [05](./05-bundle-optimization.md), [06](./06-network-performance.md)).
- **Poor INP** → runtime problem: long tasks blocking the main thread, slow re-renders after clicks or typing ([01](./01-rendering-performance.md), [02](./02-memoization.md)).
- **Poor CLS** → images/embeds without dimensions, late-loading content pushing things down, fonts swapping, skeletons that don't match final layout.

### Lab vs field data

| | Lab (Lighthouse, DevTools) | Field (real users) |
|---|---|---|
| Conditions | Controlled, repeatable | Varied devices, networks |
| Good for | Debugging, comparing changes | Knowing what users actually experience |
| Tools | Lighthouse, DevTools Performance | `web-vitals` library, CrUX, RUM tools |

Use lab data to **find and fix**, field data to **confirm it matters**. See [performance monitoring](../19-production/07-performance-monitoring.md).

Collecting field metrics yourself:

```ts
import { onLCP, onINP, onCLS } from "web-vitals"

function send(metric: { name: string; value: number; id: string }) {
  navigator.sendBeacon("/analytics", JSON.stringify(metric))
}

onLCP(send)
onINP(send)
onCLS(send)
```

## Chrome DevTools

### Performance panel: the truth about the main thread

Record an interaction (click *Record*, do the thing, stop). You get a timeline of everything the browser did.

What to look for:

- **Long tasks**: tasks over **50 ms** are flagged with red corners. They block input, and users feel them as jank. Click one to see what ran inside (the flame chart).
- **Scripting** (yellow) vs **Rendering/Layout** (purple) vs **Painting** (green). Mostly yellow means JavaScript is the cost; lots of purple means layout thrash or a huge DOM.
- Your own function names in the flame chart: wide bars are where the time went.
- Frames dropped below 60 fps.

Recent React versions can also add React-specific tracks to this panel in development and profiling builds. Check the React release notes for your version.

### Network panel

- **Waterfall**: a staircase means a request chain ([waterfalls](../12-server-state/01-fetching-data.md#waterfalls)).
- Sort by **Size** and **Time**. What are the biggest and slowest requests?
- Check compression (`content-encoding: br` or `gzip`) and caching headers ([06](./06-network-performance.md)).
- **Coverage** tab (⋮ → More tools → Coverage): shows how much loaded JavaScript and CSS is actually *executed*, a rough measure of dead weight.

### Lighthouse

Quick audit of a page: Performance score, Web Vitals estimates, and a list of opportunities. Useful as a **starting point and regression check**, but don't chase the score itself. Run it in incognito, on a production build, several times (scores vary run to run).

## React DevTools Profiler

The Profiler tab records **commits** (each time React updates the DOM) and shows what rendered and how long it took.

1. Open *Components/Profiler* (React DevTools extension).
2. In Profiler settings (gear), enable **"Record why each component rendered while profiling"**.
3. Click record, perform the slow interaction, stop.
4. Read the results:
   - **Flamegraph**: each bar is a component; width = render time; grey = didn't render in this commit.
   - **Ranked**: components sorted by render time, so start at the top.
   - Hover a component to see **why it rendered** (props changed, state changed, hooks changed, parent rendered, context changed).

What it tells you:

- *Which* components re-render for an interaction (is typing in a search box re-rendering the whole page?).
- *How long* each takes, and which are the real cost.
- *Why*, which determines the fix ([01](./01-rendering-performance.md)).

Notes:

- The Profiler works in **development** (and special profiling builds), not a normal production build. Use it for **relative** information (what renders, why, how many times), and the Chrome Performance panel on a production build for **absolute** timings.
- In development, StrictMode intentionally renders twice. Don't count those as bugs.
- A re-render isn't a problem. A **slow** or **unnecessary-and-slow** re-render is. Focus on commits that take real time (several ms+) or that happen constantly.

### The `<Profiler>` component

For measuring a specific subtree in code (including automated checks):

```tsx
import { Profiler } from "react"

<Profiler
  id="ProjectTable"
  onRender={(id, phase, actualDuration) => {
    console.log(`${id} ${phase}: ${actualDuration.toFixed(1)}ms`)   // phase: "mount" | "update" | "nested-update"
  }}
>
  <ProjectTable />
</Profiler>
```

It adds overhead and is disabled in standard production builds, so treat it as a diagnostic tool.

## Measuring in code

```ts
performance.mark("filter-start")
const result = expensiveFilter(items)
performance.mark("filter-end")
performance.measure("filter", "filter-start", "filter-end")
console.log(performance.getEntriesByName("filter")[0].duration)
```

Marks and measures also appear in the Performance panel timeline ("Timings" track). For quick checks `console.time("x")` / `console.timeEnd("x")` is enough. Do it **with CPU throttling on**, since a 1 ms calculation on your laptop may be 6 ms on a phone.

Detect long tasks in the field:

```ts
new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) console.warn("Long task", entry.duration, entry)
}).observe({ type: "longtask", buffered: true })
```

## Common findings and where to go next

| You see… | Likely cause | Look at |
|---|---|---|
| One huge JS file, slow first load | Big bundle | [05](./05-bundle-optimization.md), [03](./03-code-splitting-and-lazy-loading.md) |
| Staircase of requests before content | Fetch waterfall | [Fetching data](../12-server-state/01-fetching-data.md), [06](./06-network-performance.md) |
| Typing lags; whole page re-renders | State too high in the tree | [01](./01-rendering-performance.md) |
| Same components render over and over | Unstable props/context churn | [02](./02-memoization.md), [context](../13-state-management/01-context-patterns-and-performance.md) |
| Scrolling a long list stutters | Thousands of DOM nodes | [04](./04-virtualization.md) |
| Content jumps as page loads | Missing image dimensions, late content | [06](./06-network-performance.md) |
| One expensive render (100 ms+) | Heavy computation in render | [01](./01-rendering-performance.md), [02](./02-memoization.md) |

## Performance budgets and regression

Performance erodes one dependency at a time. Guard it:

- Set a **bundle size budget** (Vite warns past ~500 kB per chunk by default; tighten it).
- Run Lighthouse in CI on key pages and fail on big regressions.
- Look at the **bundle analyzer** whenever you add a dependency ([05](./05-bundle-optimization.md)).
- Track Web Vitals in production and watch trends after releases.

## Common mistakes

- **Optimizing without a measurement**: guessing at the bottleneck.
- **Profiling a development build** and trusting its absolute numbers.
- **Testing only on a fast machine and fast network.**
- **Chasing a Lighthouse score** instead of the experience it approximates.
- **Fixing re-renders that are fast** while ignoring a huge bundle (or vice versa).
- **Changing five things at once**, so you don't know what helped.
- **Treating every re-render as a bug.**
- **Never re-measuring**, so ineffective "optimizations" stay as complexity.
- **Ignoring field data**, so you optimize a page real users rarely hit.

## Quick summary

- Process: define → baseline → find the bottleneck → change one thing → re-measure.
- Test with **production builds**, **CPU and network throttling**, and a cold cache.
- Core Web Vitals: **LCP** (loading), **INP** (responsiveness), **CLS** (stability), and each points to a different kind of fix.
- Chrome **Performance** panel shows main-thread truth (long tasks > 50 ms); **Network** panel shows waterfalls and payloads; **React Profiler** shows what re-renders and why.
- Use lab data to debug and field data to confirm; set budgets so regressions get caught.

## Next

[01 — Rendering performance](./01-rendering-performance.md)
