# 14 — Performance Engineering

Most React apps are fast enough by default. When they aren't, performance work is **detective work**: measure, find the actual bottleneck, fix that, measure again. Sprinkling `useMemo` everywhere is not a strategy.

Performance problems fall into three buckets, and the fixes are different for each:

```text
 LOAD time        "the page takes forever to appear"
   └─ bundle size, code splitting, network, images, fonts        (03, 05, 06)

 RUNTIME work     "the UI janks, typing lags, scrolling stutters"
   └─ re-renders, expensive renders, huge DOM, long tasks        (01, 02, 04, 07, 08)

 PERCEIVED speed  "it feels slow even if it isn't"
   └─ skeletons, optimistic UI, prefetching, transitions         (06, + 12, 15)
```

Always start with [00 — Profiling and measuring](./00-profiling-and-measuring.md) to identify the true bottleneck.

---

## What you'll be able to do after this module

- **Profile with Scientific Rigor**: Use Chrome DevTools, React DevTools Profiler, and Core Web Vitals (LCP, INP, CLS) to isolate bottlenecks.
- **Eliminate Wasted Renders**: Restructure component state, colocate state, and implement composition patterns before reaching for memoization.
- **Understand Manual vs Auto Memoization**: Master `memo`, `useMemo`, and `useCallback`, understanding when they prevent re-renders versus when they add useless memory overhead.
- **Master the React Compiler**: Configure `babel-plugin-react-compiler` and `eslint-plugin-react-compiler` to auto-memoize components and reactive scopes.
- **Implement Virtualization**: Render 100,000+ items smoothly at 60fps using TanStack Virtual.
- **Optimize Bundles & Code Splitting**: Split routes with `React.lazy`, analyze bundles with visualizers, and eliminate duplicate dependencies.
- **Offload Compute with Web Workers**: Offload heavy CPU tasks (parsing, crypto, image processing) to Web Workers using Comlink to keep INP < 16ms.

---

## Module Syllabus

| # | Note | Description | Key Concept / Tool |
|---|------|-------------|:---:|
| 00 | [00-profiling-and-measuring.md](./00-profiling-and-measuring.md) | Web Vitals, React Profiler, DevTools, diagnostic process | Core Web Vitals / INP |
| 01 | [01-rendering-performance.md](./01-rendering-performance.md) | Why renders get slow, render waterfall avoidance | Render Hierarchy |
| 02 | [02-memoization.md](./02-memoization.md) | `memo`, `useMemo`, `useCallback`: when they help, when they hurt | Manual Memoization |
| 03 | [03-code-splitting-and-lazy-loading.md](./03-code-splitting-and-lazy-loading.md) | `import()`, `React.lazy`, route-level splitting, chunk recovery | Code Splitting |
| 04 | [04-virtualization.md](./04-virtualization.md) | Rendering only what's visible, dynamic measurements | TanStack Virtual |
| 05 | [05-bundle-optimization.md](./05-bundle-optimization.md) | Analyzing and shrinking bundles, tree shaking, lodash-es | Vite / Rollup |
| 06 | [06-network-performance.md](./06-network-performance.md) | Caching, compression, resource hints, modern image formats | Network & Assets |
| 07 | [07-react-compiler-in-practice.md](./07-react-compiler-in-practice.md) | React Compiler, HIR dataflow, auto-memoization, ESLint rules, bailouts | React Compiler |
| 08 | [08-web-workers-with-react.md](./08-web-workers-with-react.md) | Background multithreading, Comlink, zero-copy Transferable ArrayBuffers | Web Workers / Comlink |

---

## The Four Golden Rules of React Performance

1. **Measure before and after**: An optimization you didn't measure with the Profiler is merely a guess.
2. **Fix the biggest thing first**: Removing one 600 KB unused library impacts load time more than a hundred memoized components.
3. **Cheapest fix first**: State restructure → Avoid the work → Do less work → Auto-memoize with Compiler → Web Workers for heavy compute.
4. **Optimize what users feel**: Profile on real mobile devices and simulated 4G networks, not high-end developer workstations.

---

## Boundary with Other Modules

- **State Architecture**: Read [`02-state-and-rendering/`](../02-state-and-rendering/README.md) for state colocation and render phases.
- **Concurrent React**: Read [`15-concurrent-and-modern-react/`](../15-concurrent-and-modern-react/README.md) for `useTransition` and non-blocking updates.
- **Testing Performance**: Read [`18-testing-and-debugging/`](../18-testing-and-debugging/README.md) for automated performance regression testing.

---
