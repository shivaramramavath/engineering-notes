# Rendering

"Rendering" in React means **React calling your components** to find out what should be on screen. It doesn't mean painting pixels. Understanding the three steps — trigger, render, commit — explains why components re-render, why rendering must be pure, and where performance problems come from.

## Prerequisites

[`00-state-and-snapshots.md`](./00-state-and-snapshots.md)

---

## The three steps

Think of React as a restaurant: a **trigger** places the order, **rendering** is the kitchen preparing it, and **committing** is serving it to the table.

```
Trigger  →  Render  →  Commit  →  (browser paints)
```

### 1. Trigger

A render is triggered in two ways:

- **Initial render** — when the app starts, via `createRoot(...).render(<App />)` (see [`../00-setup/03-project-structure.md`](../00-setup/03-project-structure.md)).
- **A state update** — calling a setter on a component (or one of its ancestors' state that changes its props) queues a re-render.

Updating state **queues** a render; it doesn't run it immediately (see [`01-state-updates-and-batching.md`](./01-state-updates-and-batching.md)).

### 2. Render

React **calls your component functions** to compute the JSX:

- On the initial render, it calls the root component, then the components it returns, recursively, down to the leaves.
- On a re-render, it calls the component whose state changed, and then **every component it renders**, recursively.

The result is a description of the UI — plain objects, not DOM changes. Rendering does **no** DOM work.

### 3. Commit

React applies the result to the DOM:

- **Initial render:** creates all DOM nodes (with `appendChild`).
- **Re-render:** applies the **minimum set of changes** needed to make the DOM match the latest output. It compares the new output to the previous one and only touches what differs.

```jsx
function Clock({ time }) {
  return (
    <>
      <h1>Clock</h1>
      <p>{time}</p>
    </>
  );
}
```

Each second this component re-renders. Commit changes only the text of the `<p>`; the `<h1>` DOM node is left alone.

After the commit, **the browser paints** the screen. Rendering can happen without any visible change if the output is identical — React skips DOM work entirely in that case.

---

## Rendering is recursive

When a component re-renders, **all of its children re-render too**, regardless of whether their props changed:

```jsx
function Parent() {
  const [count, setCount] = useState(0);
  return (
    <>
      <button onClick={() => setCount(count + 1)}>{count}</button>
      <ExpensiveChild />   {/* re-renders on every click */}
    </>
  );
}
```

This is the default, and usually fine — rendering is fast. When it isn't, you have tools:

- Move state **down** into the component that uses it.
- Pass the expensive part as `children` (it's created by the parent's parent and not re-created here).
- `React.memo` to skip re-rendering when props are unchanged.

See [`../14-performance/01-rendering-performance.md`](../14-performance/01-rendering-performance.md) and [`../14-performance/02-memoization.md`](../14-performance/02-memoization.md). Measure before optimizing ([`../14-performance/00-profiling-and-measuring.md`](../14-performance/00-profiling-and-measuring.md)).

---

## Rendering must be pure

React may render a component **more than once**, **in any order**, or **throw a render away**. That only works if rendering is a pure calculation:

- **Same inputs, same output.** Given the same props, state, and context, return the same JSX.
- **No side effects while rendering.** Don't mutate variables declared outside the component, call APIs, modify the DOM, or start timers in the body.
- **Don't mutate props or state.**

```jsx
// ❌ Mutates an outside variable during render
let total = 0;
function Item({ price }) {
  total += price;
  return <li>{price}</li>;
}

// ✅ Compute from props
function Cart({ items }) {
  const total = items.reduce((sum, i) => sum + i.price, 0);
  return <p>{total}</p>;
}
```

Creating and mutating variables **local to the render** is fine — they're thrown away afterward.

Where do side effects go?

- **Event handlers** — for things caused by a user action.
- **Effects** (`useEffect`) — for things caused by rendering itself, like syncing with an external system. See [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md).

---

## StrictMode

In development, `<StrictMode>` (which Vite's template wraps your app in) helps you find impurity by:

- **Rendering each component twice** and discarding one result. If your component is pure, you won't notice. If it mutates outside state or generates random values, the doubled output exposes the bug.
- **Re-running effects** once extra on mount to check that cleanup works (see [`04-component-lifecycle.md`](./04-component-lifecycle.md)).

StrictMode has **no effect in production**. Seeing a `console.log` print twice in dev is expected, not a bug.

---

## Why a component re-renders

A component re-renders when:

1. **Its own state changes.**
2. **Its parent re-renders** (the default cascade).
3. **A context it consumes changes** (see [`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md)).

Changing a plain variable or a ref does **not** trigger a render. The React DevTools Profiler can tell you exactly *why* each component rendered ([`../00-setup/04-react-devtools.md`](../00-setup/04-react-devtools.md)).

---

## Render vs commit vs paint

| Phase | What happens | Can you do side effects? |
|-------|--------------|--------------------------|
| Render | Components are called; JSX is produced | **No** — must be pure |
| Commit | React updates the DOM | Effects run **after** this |
| Paint | The browser draws pixels | — |

Because commit happens after render, in a handler or effect you can read the updated DOM. Inside render itself, you can't — the DOM hasn't been updated yet.

For the underlying machinery (Fiber, work loop, interruptible rendering) see [`../17-react-internals/00-fiber-architecture.md`](../17-react-internals/00-fiber-architecture.md) and [`../15-concurrent-and-modern-react/00-concurrent-rendering.md`](../15-concurrent-and-modern-react/00-concurrent-rendering.md).

---

## Common mistakes

- **Treating "render" as "update the DOM"** — rendering just calls your functions; commit updates the DOM.
- **Side effects in the component body** — fetches, subscriptions, or mutations during render break purity and run unpredictably (twice in StrictMode).
- **Mutating outside variables during render** — produces order-dependent bugs.
- **Panicking about re-renders** — re-rendering is normal and cheap; optimize only after measuring.
- **Being surprised by double logs** — that's StrictMode in development.
- **Calling `Math.random()` or `Date.now()` in render** — results differ each pass; use state, refs, or effects.

## Quick summary

- Rendering = React calling your components; it has three steps: trigger, render, commit
- Triggers: the initial render and state updates
- Re-rendering a component re-renders its children by default
- Commit only changes the DOM where the output differs
- Rendering must be pure; side effects go in handlers or effects
- StrictMode double-renders in development to expose impurity

## Next

**[`04-component-lifecycle.md`](./04-component-lifecycle.md)** looks at a component's life: mounting, updating, and unmounting.
