# 14 — Performance

Most React apps are fast enough by default. When they aren't, the cause is almost never what people guess first. Performance work is **detective work**: measure, find the actual bottleneck, fix that, measure again. Sprinkling `useMemo` everywhere is not a strategy.

Performance problems fall into three buckets, and the fixes are different for each:

```text
 LOAD time        "the page takes forever to appear"
   └─ bundle size, code splitting, network, images, fonts        (03, 05, 06)

 RUNTIME work     "the UI janks, typing lags, scrolling stutters"
   └─ re-renders, expensive renders, huge DOM, long tasks        (01, 02, 04)

 PERCEIVED speed  "it feels slow even if it isn't"
   └─ skeletons, optimistic UI, prefetching, transitions         (06, + 12, 15)
```

Always start with [00 — Profiling and measuring](./00-profiling-and-measuring.md) to know which bucket you're in.

## Prerequisites

- [Rendering](../02-state-and-rendering/03-rendering.md): you can't optimize renders you don't understand
- [useMemo and useCallback](../03-hooks/07-useMemo-and-useCallback.md)
- [Server state](../12-server-state/README.md): caching is the biggest network optimization you have

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Profiling and measuring](./00-profiling-and-measuring.md) | Web Vitals, React Profiler, DevTools, a repeatable process |
| 01 | [Rendering performance](./01-rendering-performance.md) | Why renders get slow and the fixes in order of preference |
| 02 | [Memoization](./02-memoization.md) | `memo`, `useMemo`, `useCallback`: when they help, when they hurt |
| 03 | [Code splitting and lazy loading](./03-code-splitting-and-lazy-loading.md) | `import()`, `React.lazy`, route-level splitting, chunk errors |
| 04 | [Virtualization](./04-virtualization.md) | Rendering only what's visible; TanStack Virtual |
| 05 | [Bundle optimization](./05-bundle-optimization.md) | Analyzing and shrinking what you ship |
| 06 | [Network performance](./06-network-performance.md) | Caching, compression, resource hints, images, fonts |

## Suggested order

00 first, always. Then follow your symptom: **slow to load** → 05, 03, 06. **Slow to interact** → 01, 02, 04.

## The rules this folder keeps repeating

1. **Measure before and after.** An optimization you didn't measure is a guess.
2. **Fix the biggest thing first.** One 800 KB dependency matters more than a hundred memoized components.
3. **Cheapest fix first:** restructure → avoid the work → do less work → memoize last.
4. **Optimize what users feel.** Real devices, real networks, production builds.
