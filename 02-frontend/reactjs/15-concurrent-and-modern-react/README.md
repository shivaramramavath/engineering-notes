# 15 — Concurrent and Modern React

React 18 and 19 changed what React *can do*, not just what it looks like. The headline change: **rendering can be interrupted.** That one capability makes several features possible: transitions, deferred values, Suspense for data, streaming server rendering. React 19 then built on it with Actions, `use`, and a compiler that removes most manual memoization.

This folder explains the model first, then each feature.

```text
Interruptible rendering                      (00 — the foundation)
   ├─ Transitions / useTransition            (01 — mark updates as non-urgent)
   ├─ useDeferredValue                       (02 — lag a value behind)
   ├─ Suspense                               (03 — declarative loading)
   └─ Error boundaries                       (04 — declarative failure)

React 19 additions                           (05 — Actions, use, ref as prop, …)
React Compiler                               (06 — automatic memoization)
Server rendering and Server Components       (07 — work moved to the server)
```

## Prerequisites

- [Rendering](../02-state-and-rendering/03-rendering.md) and [state updates and batching](../02-state-and-rendering/01-state-updates-and-batching.md)
- [Hooks](../03-hooks/README.md), especially `useEffect` and `useMemo`
- [Performance basics](../14-performance/README.md): this folder's features are tools for the *runtime* problems described there

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Concurrent rendering](./00-concurrent-rendering.md) | What "concurrent" means, urgent vs non-urgent updates, why renders must be pure |
| 01 | [Transitions](./01-transitions.md) | `startTransition`, `useTransition`, async transitions |
| 02 | [useDeferredValue](./02-useDeferredValue.md) | Deferring a value, pairing with `memo`, vs debounce |
| 03 | [Suspense](./03-suspense.md) | Boundaries, `use`, `lazy`, avoiding waterfalls |
| 04 | [Error boundaries](./04-error-boundaries.md) | Catching render errors, `react-error-boundary`, root error hooks |
| 05 | [React 19 features](./05-react-19-features.md) | Actions, `useActionState`, `useOptimistic`, `ref` as a prop, metadata, removals |
| 06 | [React Compiler](./06-react-compiler.md) | Automatic memoization: setup, rules, opt-out |
| 07 | [Server Components and SSR](./07-server-components-and-ssr.md) | SSR, hydration, streaming, RSC, `"use client"` |

## Suggested order

00 → 01 → 02 → 03 → 04 is a natural chain. 05, 06, and 07 are independent of each other once you've read 00.

## A note on versions

This folder targets **React 19**. Features land in minor releases too (19.1, 19.2 and later), so where a detail is recent or still evolving, the note says so. When in doubt, check the [React release notes](https://react.dev/blog) for your installed version.