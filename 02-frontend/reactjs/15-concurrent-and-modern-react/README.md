# 15 — Concurrent and Modern React

React 18 and 19 fundamentally changed what React *can do*, not just what it looks like. The foundational capability: **rendering can be interrupted.** That one capability powers transitions, deferred values, Suspense for data, streaming server rendering, React 19 Actions, and hardware-accelerated View Transitions.

```text
Interruptible rendering                      (00 — the foundation)
   ├─ Transitions / useTransition            (01 — mark updates as non-urgent)
   ├─ useDeferredValue                       (02 — lag a value behind)
   ├─ Suspense                               (03 — declarative loading)
   └─ Error boundaries                       (04 — declarative failure)

React 19 additions                           (05 — Actions, use, ref as prop, …)
React Compiler                               (06 — automatic memoization)
Server rendering and Server Components       (07 — work moved to the server)
View Transitions API                         (08 — GPU-accelerated page morphs)
```

---

## What you'll be able to do after this module

- **Master the Concurrent Mental Model**: Understand cooperative multitasking, time slicing, urgent vs non-urgent update queues, and why render purity is mandatory.
- **Implement Non-Blocking Transitions**: Use `startTransition` and `useTransition` to keep inputs and animations responsive while heavy background views render.
- **Defer Expensive Values**: Use `useDeferredValue` to lag expensive computations behind user typing without manual debouncing timers.
- **Coordinate Suspense Hierarchies**: Nest `<Suspense>` boundaries to eliminate loading waterfalls and orchestrate graceful reveal sequences.
- **Architect Resilient Error Boundaries**: Use `react-error-boundary` and `QueryErrorResetBoundary` to catch render exceptions and offer scoped user retries.
- **Adopt React 19 Paradigms**: Leverage Actions, `useActionState`, `useOptimistic`, native `ref` props, and document metadata hoisting.
- **Integrate Native View Transitions**: Implement `document.startViewTransition` with `flushSync` and React Router v7 for smooth GPU cross-fades and shared element morphing.

---

## Module Syllabus

| # | Note | Description | Key Concept / API |
|---|------|-------------|:---:|
| 00 | [00-concurrent-rendering.md](./00-concurrent-rendering.md) | Cooperative time-slicing, interruptible render work loop | Fiber Engine |
| 01 | [01-transitions.md](./01-transitions.md) | `useTransition`, `startTransition`, non-blocking state updates | Transitions |
| 02 | [02-useDeferredValue.md](./02-useDeferredValue.md) | Deferring values, stale render lag, comparison with debounce | Deferred Values |
| 03 | [03-suspense.md](./03-suspense.md) | Declarative loading boundaries, `React.lazy`, waterfall prevention | Suspense |
| 04 | [04-error-boundaries.md](./04-error-boundaries.md) | Catching render errors, fallback UI, error recovery hooks | Error Boundaries |
| 05 | [05-react-19-features.md](./05-react-19-features.md) | Actions, `useActionState`, `useOptimistic`, `ref` prop, metadata | React 19 APIs |
| 06 | [06-react-compiler.md](./06-react-compiler.md) | High-level compiler overview, automatic memoization | React Compiler |
| 07 | [07-server-components-and-ssr.md](./07-server-components-and-ssr.md) | SSR, hydration, streaming, RSC architecture, `"use client"` | RSC & SSR |
| 08 | [08-view-transitions-api.md](./08-view-transitions-api.md) | W3C View Transitions API, `flushSync`, shared element morphing | View Transitions |

---

## Boundary with Other Modules

- **React 19 Hooks**: Read [`03-hooks/`](../03-hooks/README.md) for individual deep dives into `useOptimistic` (15), `useActionState` (16), `useFormStatus` (17), and `use` (18).
- **Performance Profiling**: Read [`14-performance/00-profiling-and-measuring.md`](../14-performance/00-profiling-and-measuring.md) for profiling transitions in Chrome DevTools.
- **Routing Transitions**: Read [`10-routing/07-react-router-v7-library-vs-framework.md`](../10-routing/07-react-router-v7-library-vs-framework.md) for React Router v7 View Transitions.

---
