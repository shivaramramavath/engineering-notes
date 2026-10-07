# Rendering Performance

React re-renders components often, and that's normally fine: rendering is cheap, and React only touches the DOM for what actually changed. Rendering becomes a *problem* when a re-render is **slow** (it does heavy work) or **wasted and frequent** (large trees re-rendering on every keystroke).

This note is about **fixing renders in order of preference**. The cheapest, most robust fixes come first. [Memoization](./02-memoization.md) is deliberately near the end.

(Background: [rendering](../02-state-and-rendering/03-rendering.md), [reconciliation](../17-react-internals/01-reconciliation.md).)

## What causes a re-render

A component re-renders when:

1. **Its own state changes** (`useState`, `useReducer`).
2. **Its parent re-renders**, and by default **all children re-render too**, whether or not their props changed.
3. **A context it reads changes** ([context performance](../13-state-management/01-context-patterns-and-performance.md)).

That second rule is the surprising one. Props changing is *not* required for a child to re-render.

## Diagnose first

Use the [Profiler](./00-profiling-and-measuring.md#react-devtools-profiler) with "record why each component rendered". You're looking for:

- A **slow** component (ranked chart), meaning each render costs real time.
- A **fast component rendering constantly** (typing → 500 rows re-render).
- The *reason*: "parent rendered" suggests restructuring; "props changed" with identical values suggests unstable references; "context changed" suggests splitting context.

## Fix 1: move state down (colocate)

The most effective and least clever fix. State should live **as close as possible** to where it's used.

```tsx
// ✗ `query` lives in the page, so every keystroke re-renders the whole page
function Page() {
  const [query, setQuery] = useState("")
  return (
    <>
      <input value={query} onChange={(e) => setQuery(e.target.value)} />
      <ExpensiveChart />            {/* re-renders on every keystroke */}
      <HugeTable />
    </>
  )
}

// ✓ state moved into the component that needs it
function Page() {
  return (
    <>
      <SearchBox />                 {/* owns `query`; only it re-renders */}
      <ExpensiveChart />
      <HugeTable />
    </>
  )
}

function SearchBox() {
  const [query, setQuery] = useState("")
  return <input value={query} onChange={(e) => setQuery(e.target.value)} />
}
```

No `memo`, no hooks, no cleverness, and it's easier to read. Always try this first.

## Fix 2: lift content up (pass `children`)

If the state must wrap the expensive part, pass the expensive part **as `children`** (or any prop). Elements created by the *parent* keep the same identity, so React skips re-rendering them when the wrapper's state changes:

```tsx
function ScrollTracker({ children }: { children: React.ReactNode }) {
  const [scrollY, setScrollY] = useState(0)
  useEffect(() => {
    const onScroll = () => setScrollY(window.scrollY)
    window.addEventListener("scroll", onScroll, { passive: true })
    return () => window.removeEventListener("scroll", onScroll)
  }, [])

  return (
    <div>
      <Indicator value={scrollY} />
      {children}                    {/* same element reference each render → not re-rendered */}
    </div>
  )
}

<ScrollTracker>
  <ExpensiveTree />
</ScrollTracker>
```

This is why [composition](../01-fundamentals/03-children-and-composition.md) is also a performance tool. (For scroll- or pointer-driven visuals, also consider avoiding React state entirely: CSS, `IntersectionObserver`, or a ref plus direct style updates.)

## Fix 3: don't store what you can compute

State that mirrors other state forces extra renders and bugs:

```tsx
// ✗ extra state + effect = an additional render pass every time
const [items, setItems] = useState<Item[]>([])
const [filtered, setFiltered] = useState<Item[]>([])
useEffect(() => { setFiltered(items.filter(matches)) }, [items, query])

// ✓ derive during render
const filtered = items.filter(matches)           // wrap in useMemo only if it's measurably slow
```

Effects that only call `setState` in response to other state are a smell: [you might not need an effect](../03-hooks/03-you-might-not-need-an-effect.md).

## Fix 4: keep component identity stable

Two bugs that make React throw away and rebuild DOM (far costlier than a re-render):

**Defining components inside components.**

```tsx
function Page() {
  // ✗ a NEW component type on every render → React unmounts and remounts it each time
  const Row = ({ item }: { item: Item }) => <li>{item.name}</li>
  return <ul>{items.map((i) => <Row key={i.id} item={i} />)}</ul>
}
```

Declare components at module level (or pass render output, not a component).

**Unstable or index keys.**

```tsx
{items.map((item, i) => <Row key={i} item={item} />)}            // ✗ breaks on reorder/insert
{items.map((item) => <Row key={Math.random()} item={item} />)}   // ✗✗ remount every render
{items.map((item) => <Row key={item.id} item={item} />)}         // ✓ stable identity
```

Remounting loses state, resets inputs and focus, and re-runs effects and DOM creation. See [lists and keys](../01-fundamentals/05-lists-and-keys.md).

Conversely, a deliberate `key` change is the correct way to reset a component ([state preservation and reset](../02-state-and-rendering/05-state-preservation-and-reset.md)).

## Fix 5: do less work per render

If a component is genuinely slow, find the work:

- **Heavy computation in the render body** (sorting/filtering 10,000 items, regex over large text, building big structures). Move it to the server, a Web Worker, or cache it with `useMemo` ([02](./02-memoization.md)).
- **Creating large objects/arrays each render** that you then deep-compare.
- **Rendering too many elements.** Thousands of rows are slow no matter how efficient each is. Paginate or [virtualize](./04-virtualization.md).
- **Expensive children** rendered but not visible (hidden tabs, collapsed panels, off-screen sections). Don't render what isn't shown; conditionally render, or lazy-mount.
- **Layout-heavy DOM**: deeply nested, thousands of nodes. Flatten.

For off-screen sections that must stay in the DOM, CSS can skip their layout and paint work:

```css
.card-list > li { content-visibility: auto; contain-intrinsic-size: auto 120px; }
```

`content-visibility: auto` lets the browser skip rendering off-screen content until needed. Provide `contain-intrinsic-size` so the scrollbar and layout stay stable. It has accessibility and find-in-page implications, so test it.

## Fix 6: keep urgent updates urgent

Some updates are **urgent** (typing, clicking, pressing) and some can lag a moment (filtering a big list as you type). React's concurrent features let you mark the second kind as non-urgent so input stays responsive:

```tsx
const [query, setQuery] = useState("")
const deferredQuery = useDeferredValue(query)       // lags behind `query` during heavy renders

<input value={query} onChange={(e) => setQuery(e.target.value)} />   {/* instant */}
<BigResults query={deferredQuery} />                                 {/* catches up when React is idle */}
```

```tsx
const [isPending, startTransition] = useTransition()
startTransition(() => setFilter(next))              // this update can be interrupted
```

This doesn't make the rendering *faster*; it keeps the UI responsive while expensive work happens. Details: [transitions](../15-concurrent-and-modern-react/01-transitions.md), [useDeferredValue](../15-concurrent-and-modern-react/02-useDeferredValue.md). It pairs best with fixing the underlying cost where you can.

## Fix 7: batch and reduce updates

- React **batches** state updates inside event handlers, effects, timeouts, and promises (React 18+), so several `setState` calls produce one render.
- Avoid **high-frequency `setState`** (every `mousemove`, every WebSocket message). Throttle, batch to once per animation frame, or keep the value in a ref and update the DOM directly for purely visual effects.
- Don't `setState` in an effect to react to props/state ([above](#fix-3-dont-store-what-you-can-compute)).

## Fix 8: only then, memoize

If a component is slow, re-renders unnecessarily, and restructuring isn't possible, [`memo`, `useMemo`, and `useCallback`](./02-memoization.md) apply. They're a **targeted tool**, not a default. The [React Compiler](../15-concurrent-and-modern-react/06-react-compiler.md) can apply much of this automatically, which shifts the guidance further toward "write straightforward code and fix measured problems".

## Things that are *not* usually the problem

- **Inline functions and objects as props** (`onClick={() => …}`, `style={{…}}`). They create new references each render, which only matters if the child is memoized or the value is in a dependency array. Otherwise the cost is negligible.
- **Re-rendering small, cheap components.** Fast renders are fine.
- **Using context at all.** The issue is *how often* the value changes and *how many* components consume it.
- **StrictMode double rendering.** It's development-only.
- **The virtual DOM itself.** Diffing is rarely the bottleneck; your component's own work or DOM size usually is.

## DOM and layout performance

Even with perfect React code, the browser's work can dominate:

- **Layout thrashing**: interleaving DOM reads (`offsetHeight`, `getBoundingClientRect`) and writes in a loop forces repeated synchronous layouts. Batch reads, then writes.
- Animate **`transform` and `opacity`** (compositor-friendly) rather than `top`, `left`, `width`, `height` (layout).
- Avoid giant DOM trees (thousands of nodes) and deep nesting.
- `useLayoutEffect` blocks painting; keep it tiny ([useLayoutEffect](../03-hooks/08-useLayoutEffect.md)).

## Common mistakes

- **Reaching for `memo` before restructuring.**
- **State placed too high**, so typing re-renders the page.
- **Components defined inside components** (remount every render).
- **Index or random keys**, causing remounts and state bugs.
- **Mirrored/derived state kept with `useEffect` + `setState`.**
- **Rendering thousands of DOM nodes** instead of paginating or virtualizing.
- **Rendering hidden content** (inactive tabs/panels) eagerly.
- **High-frequency `setState`** for scroll or pointer effects.
- **Optimizing fast renders** while a slow one sits at the top of the Profiler's ranked chart.
- **Measuring in dev mode** and over-trusting absolute numbers.

## Quick summary

- Re-renders happen from state, **parent re-renders**, or context; they're only a problem when slow or constant on big trees.
- Fix in order: **move state down → lift content up (`children`) → derive instead of store → stable identity (keys, no inner components) → do less work (virtualize, don't render hidden) → defer non-urgent updates → memoize**.
- Keep high-frequency updates out of React state when only visuals change.
- Inline functions and small re-renders are rarely the culprit; measure to find the real one.

## Next

[02 — Memoization](./02-memoization.md)
