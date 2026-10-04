# useMemo and useCallback

`useMemo` caches the **result of a calculation** between renders; `useCallback` caches a **function definition**. Both are performance and stability tools, not correctness tools — your app should work identically without them. This file is the **API reference and the rules for when they help**. The wider strategy (`React.memo`, profiling, the React Compiler) lives in [`../14-performance/02-memoization.md`](../14-performance/02-memoization.md).

## Prerequisites

[`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md) (especially "rendering is recursive") and [`02-useEffect.md`](./02-useEffect.md)

---

## `useMemo`

```jsx
const cached = useMemo(calculateValue, dependencies);
```

```jsx
import { useMemo } from "react";

function TodoList({ todos, filter }) {
  const visibleTodos = useMemo(
    () => filterTodos(todos, filter),
    [todos, filter]
  );

  return <List items={visibleTodos} />;
}
```

- React calls `calculateValue` on the first render and caches the result.
- On later renders, it **returns the cached value if no dependency changed** (compared with `Object.is`); otherwise it recalculates.
- `calculateValue` must be **pure** and takes no arguments.

## `useCallback`

```jsx
const cachedFn = useCallback(fn, dependencies);
```

```jsx
const handleSubmit = useCallback(
  (orderDetails) => {
    post(`/product/${productId}/buy`, { referrer, orderDetails });
  },
  [productId, referrer]
);
```

- Returns the **same function object** between renders as long as dependencies don't change.
- `useCallback(fn, deps)` is equivalent to `useMemo(() => fn, deps)`.

Without it, every render creates a **new** function (and object, and array), even if its contents are identical. That only matters when something **compares by reference**.

---

## When they actually help

### 1. Skipping an expensive calculation

If a calculation is genuinely slow, memoize it. **Measure first** — most calculations are fast enough:

```jsx
console.time("filter");
const visible = filterTodos(todos, filter);
console.timeEnd("filter");
```

If it consistently takes ~1 ms or more with realistic data (test with CPU throttling), consider `useMemo`. Below that, don't bother.

### 2. Skipping re-renders of a memoized child

Components re-render whenever their parent does ([`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)). `React.memo` skips re-rendering when **props are shallowly equal**:

```jsx
const ShippingForm = memo(function ShippingForm({ onSubmit }) { /* ... */ });
```

But if the parent passes a **new function or object every render**, the props are never equal and `memo` does nothing. `useCallback`/`useMemo` give the child stable references:

```jsx
function ProductPage({ productId, referrer }) {
  const handleSubmit = useCallback(
    (details) => post(`/product/${productId}/buy`, { referrer, details }),
    [productId, referrer]
  );
  return <ShippingForm onSubmit={handleSubmit} />;   // memo now works
}
```

**`useCallback` without `memo` on the child (or another reference-comparing consumer) has no benefit.**

### 3. Stabilizing a dependency of another hook

If an effect or memo depends on a function or object created in the body, it re-runs every render. Stabilizing it avoids that — but first try restructuring so you don't need the dependency at all ([`02-useEffect.md`](./02-useEffect.md)):

```jsx
const options = useMemo(() => ({ serverUrl, roomId }), [serverUrl, roomId]);

useEffect(() => {
  const c = createConnection(options);
  c.connect();
  return () => c.disconnect();
}, [options]);
```

(Simpler: create `options` *inside* the effect and depend on `serverUrl` and `roomId` directly.)

### 4. Stabilizing a context value

An object passed to a context provider that changes identity each render re-renders all consumers; memoizing it helps ([`../13-state-management/01-context-patterns-and-performance.md`](../13-state-management/01-context-patterns-and-performance.md)).

---

## When they don't help

- **Cheap calculations** — memoization has its own cost (dependency comparison and storage).
- **Child isn't memoized** — a new function passed to a non-`memo` child changes nothing; the child re-renders with its parent anyway.
- **Values that change on nearly every render** — the cache rarely hits.
- **As a way to "fix" correctness** — if behavior depends on memoization, you have a bug elsewhere; React may discard caches.

**React may throw away the cache** (for example, during development or if a component suspends), so memoization is only an optimization.

---

## Alternatives to try first

Often there's a better fix than memoizing:

1. **Move state down** — keep frequently changing state in the smallest component that needs it, so the rest of the tree doesn't re-render.
2. **Pass JSX as `children`** — content created above the stateful component isn't re-created when that component re-renders ([`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md)).
3. **Avoid unnecessary effects** that set state ([`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md)).
4. **Reduce rendering work** (smaller components, virtualize long lists — [`../14-performance/04-virtualization.md`](../14-performance/04-virtualization.md)).

Then, if profiling shows a real problem ([`../14-performance/00-profiling-and-measuring.md`](../14-performance/00-profiling-and-measuring.md)), memoize.

---

## The React Compiler

The **React Compiler** (a build-time tool) automatically memoizes components and hooks, deciding for you what to cache. With it enabled, hand-written `useMemo`/`useCallback` become mostly unnecessary, except as an escape hatch for precise control (for example, a stable effect dependency). Code that follows the Rules of React works with it. See [`../15-concurrent-and-modern-react/06-react-compiler.md`](../15-concurrent-and-modern-react/06-react-compiler.md). Until you adopt it, the guidance in this file applies.

---

## Gotchas

- **Dependencies are compared with `Object.is`.** Objects and arrays created inline are always "new", so they defeat the cache.
- **Memoizing inside loops isn't possible** — hooks can't be called in loops. Extract a child component and memoize inside it.
- **Don't call hooks inside the memo callback** ([`00-hook-rules.md`](./00-hook-rules.md)).
- **Returning a function from `useMemo`** works but is exactly what `useCallback` is for.
- **Forgetting dependencies** — the cached value or function holds stale data; follow the `exhaustive-deps` lint rule.

---

## Common mistakes

- **Sprinkling `useMemo`/`useCallback` everywhere** — adds noise and cost without measurable benefit.
- **`useCallback` for a function passed to a non-memoized child** — no effect.
- **Skipping measurement** — memoize based on profiling, not intuition.
- **Stale closures from missing dependencies** — the function sees old state or props.
- **Using memoization to prevent an effect from re-running** — usually the effect's design needs changing.
- **Memoizing values that depend on objects recreated every render** — the cache always misses.

## Quick summary

- `useMemo` caches a computed value; `useCallback` caches a function; both depend on a dependency array
- They help in three cases: skipping genuinely expensive work, giving stable props to `memo`'d children, and stabilizing hook dependencies
- Memoize only after measuring; first try moving state down or passing `children`
- Memoization is an optimization, never a correctness mechanism
- The React Compiler can automate most of this

## Next

**[`08-useLayoutEffect.md`](./08-useLayoutEffect.md)** covers the rare effect that must run before the browser paints.
