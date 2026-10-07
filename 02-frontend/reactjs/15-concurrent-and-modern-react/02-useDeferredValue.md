# useDeferredValue

`useDeferredValue` gives you a **lagging copy of a value**. The urgent part of the UI uses the real value and updates instantly; the expensive part uses the deferred copy, which React updates later, at low priority, and **interruptibly**.

It's the right tool when you don't control the `setState` call (so [`startTransition`](./01-transitions.md) isn't available), or when one value drives both a cheap, urgent UI and an expensive one.

## The canonical example

```tsx
import { useDeferredValue, useState } from "react"

function SearchPage() {
  const [query, setQuery] = useState("")
  const deferredQuery = useDeferredValue(query)

  return (
    <>
      <input value={query} onChange={(e) => setQuery(e.target.value)} />   {/* uses the real value: instant */}
      <SlowResults query={deferredQuery} />                                {/* uses the deferred value: may lag */}
    </>
  )
}
```

What happens when you type a character:

```text
1. query = "ab"            ──► urgent render: input shows "ab" immediately.
                                deferredQuery is still "a", so <SlowResults> gets the same props as before.
2. In the background:      ──► React re-renders with deferredQuery = "ab"  (low priority, interruptible)
3. Type "abc" mid-render   ──► React abandons step 2, handles the urgent update, then starts over with "abc"
```

Typing never waits for `SlowResults`. On a fast machine the deferred render finishes so quickly you can't tell it was deferred.

## It needs `memo` to do anything

During step 1, `SlowResults` is re-rendered by its parent but receives the **same** `deferredQuery`. Unless it skips rendering when its props are unchanged, React still does the slow work on the urgent render, and you've gained nothing:

```tsx
const SlowResults = memo(function SlowResults({ query }: { query: string }) {
  const items = filterHugeList(query)       // expensive
  return <ul>{items.map((i) => <Row key={i.id} item={i} />)}</ul>
})
```

This is the **one** place where `memo` is essentially required, not optional. Without it, the deferral is pointless. (With the [React Compiler](./06-react-compiler.md) the component is usually memoized automatically, but verify in the Profiler.)

## Showing that results are stale

While the deferred value lags, the results on screen belong to an *older* query. Tell users:

```tsx
const isStale = query !== deferredQuery

<div className={isStale ? "opacity-60 transition-opacity" : undefined} aria-busy={isStale}>
  <SlowResults query={deferredQuery} />
</div>
```

Comparing the two values is the idiomatic "pending" signal, the equivalent of `isPending` from `useTransition`.

## Initial value (React 19)

```tsx
const deferred = useDeferredValue(query, "")      // second argument: the value to use on the FIRST render
```

On initial mount, React renders with the provided `initialValue` first, then schedules a deferred re-render with the real value. It's useful when the first render of the expensive part should be cheap (an empty result set) and fill in right after. Without it, the first render uses the value as-is, and deferral applies only to updates.

## With data fetching and Suspense

Deferring a value that feeds a Suspense-enabled query keeps the **old results on screen** while the new ones load, instead of showing the fallback:

```tsx
function Search() {
  const [query, setQuery] = useState("")
  const deferredQuery = useDeferredValue(query)
  const isStale = query !== deferredQuery

  return (
    <>
      <input value={query} onChange={(e) => setQuery(e.target.value)} />
      <Suspense fallback={<ResultsSkeleton />}>
        <div className={isStale ? "opacity-60" : undefined}>
          <Results query={deferredQuery} />       {/* suspends via useSuspenseQuery(query) */}
        </div>
      </Suspense>
    </>
  )
}
```

When `query` changes, `Results` still receives the old `deferredQuery`, so it doesn't suspend. React prepares the new render in the background, and the fallback appears **only on the very first load**. See [Suspense](./03-suspense.md).

Note what this does **not** do: it doesn't reduce network requests. Every distinct `deferredQuery` value that React renders still triggers a fetch. If requests are costly, also debounce the value that reaches the data layer ([search state](../10-routing/06-search-filter-and-url-state.md#debounced-search-input)).

## `useDeferredValue` vs `useTransition`

| | `useDeferredValue` | `useTransition` |
|---|---|---|
| You wrap | A **value** | A **state update** |
| Control point | Where the value is *consumed* | Where the state is *set* |
| Works when the value comes from props or a library | **Yes** | No (you can't wrap `setState` you don't own) |
| Pending flag | Compare `value !== deferredValue` | `isPending` |
| Typical | Search text → heavy results; a prop passed down | Tab switches, navigation, event handlers |

Rule of thumb: **if you own the update, prefer a transition; if you only receive the value, defer it.**

## `useDeferredValue` vs debouncing

| | `useDeferredValue` | Debounce |
|---|---|---|
| Waits a fixed time | No: starts immediately | Yes (for example 300 ms) |
| Adapts to the device | **Yes**: fast machines catch up instantly | No: everyone waits the same |
| Interruptible | **Yes** | No: once the timer fires, work runs |
| Reduces network/side-effect calls | **No** | **Yes** |
| Best for | Expensive **rendering** | Expensive **requests and side effects** |

They solve different problems and combine well.

## Rules and gotchas

**Pass primitives, or values with stable identity.** `useDeferredValue` compares with `Object.is`. If you pass a **new object/array every render**, React sees the value as changed every time, so the deferral never settles and can trigger endless background re-renders:

```tsx
const deferred = useDeferredValue({ query })        // ✗ new object every render
const deferred = useDeferredValue(query)            // ✓ primitive
const deferred = useDeferredValue(filters)          // ✓ only if `filters` comes from state/useMemo
```

**Don't defer the input's own value.** The text input must show the real value immediately, and only the *consumer* of the expensive result should get the deferred copy.

**Deferred updates don't make the render faster**, just interruptible. If a single render takes seconds, users still wait for it to finish. Reduce the work too ([rendering performance](../14-performance/01-rendering-performance.md), [virtualization](../14-performance/04-virtualization.md)).

**It's not a timer or throttle.** Don't use it to delay an action, or to "wait for the user to stop typing".

**Effects see the deferred value in the deferred render.** An effect that depends on `deferredQuery` runs when the deferred render commits, not at the same time as one that depends on `query`.

## Choosing: a quick decision

```text
Heavy render caused by my own setState (tab, navigation, filter button)?
   └─► useTransition / startTransition
A value I receive (prop, other hook) drives a heavy render?
   └─► useDeferredValue  (+ memo on the heavy child)
Too many network requests while typing?
   └─► debounce the value that feeds the request
Render is simply too much work?
   └─► reduce the work first (virtualize, paginate, simplify)
```

## Common mistakes

- **Not memoizing the slow child**, so deferring changes nothing.
- **Deferring the input's own value**, so typing lags.
- **Passing a new object/array each render**, causing endless background re-renders.
- **Treating it as a debounce**, and expecting fewer requests or a fixed delay.
- **No stale indicator**, leaving users unsure results are out of date.
- **Using it to fix a render that's simply too heavy** instead of reducing the work.
- **Deferring something cheap**, adding a second render pass for no gain.
- **Mixing up which value feeds what.** Components that should react instantly must use the real value.

## Quick summary

- `useDeferredValue(value)` returns a copy that **lags behind** during urgent updates and catches up in an interruptible, low-priority render.
- Give the **expensive** component the deferred value and **memoize it**; keep the cheap, urgent UI on the real value.
- Detect staleness with `value !== deferredValue` and signal it subtly.
- With Suspense, it keeps old results visible instead of flashing the fallback.
- Use it when you only *receive* the value; use `useTransition` when you own the update.
- It doesn't debounce or reduce requests, and it doesn't make renders faster.

## Next

[03 — Suspense](./03-suspense.md)