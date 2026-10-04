# useRef

`useRef` gives you a value that **persists across renders without causing a re-render when it changes**. It has two main uses: holding mutable values that aren't part of what's displayed (timer ids, previous values) and getting a handle on DOM elements (focus, scroll, measure).

## Prerequisites

[`01-useState.md`](./01-useState.md) and [`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)

---

## Syntax

```jsx
import { useRef } from "react";

const ref = useRef(initialValue);
// ref is an object: { current: initialValue }
```

- `ref.current` can be **read and written freely**.
- React keeps the **same object** between renders.
- Changing `ref.current` does **not** trigger a re-render.

---

## Ref vs state

| | `useState` | `useRef` |
|---|-----------|----------|
| Persists between renders | Yes | Yes |
| Changing it re-renders | **Yes** | **No** |
| Mutable directly | No (use the setter) | Yes (`ref.current = x`) |
| Read during render | Yes | **Avoid** (see below) |
| Use for | Data shown on screen | Values the UI doesn't depend on |

Rule of thumb: **if it affects what's rendered, it's state. If it's bookkeeping, it can be a ref.**

---

## Use 1: Storing values outside the render flow

### A timer id

```jsx
function Stopwatch() {
  const [now, setNow] = useState(null);
  const [startTime, setStartTime] = useState(null);
  const intervalRef = useRef(null);

  function handleStart() {
    setStartTime(Date.now());
    setNow(Date.now());
    clearInterval(intervalRef.current);
    intervalRef.current = setInterval(() => setNow(Date.now()), 10);
  }

  function handleStop() {
    clearInterval(intervalRef.current);
  }

  const secondsPassed = startTime != null && now != null ? (now - startTime) / 1000 : 0;

  return (
    <>
      <h1>Time passed: {secondsPassed.toFixed(3)}</h1>
      <button onClick={handleStart}>Start</button>
      <button onClick={handleStop}>Stop</button>
    </>
  );
}
```

The interval id isn't shown on screen, but it must survive re-renders so `handleStop` can clear it. A plain `let` variable would be reset each render; a ref persists.

### Other typical uses

- Previous value of a prop or state ([`10-hook-recipes.md`](./10-hook-recipes.md))
- A flag such as "has this effect already run?"
- The latest value of a callback, for use in async code or intervals
- Instances of non-React objects (a WebSocket, a chart)

---

## Use 2: Accessing the DOM

Pass a ref to an element's `ref` attribute. React sets `ref.current` to the DOM node after it renders:

```jsx
function SearchBox() {
  const inputRef = useRef(null);

  return (
    <>
      <input ref={inputRef} />
      <button onClick={() => inputRef.current.focus()}>Focus the input</button>
    </>
  );
}
```

Common DOM tasks: `focus()`, `scrollIntoView()`, measuring with `getBoundingClientRect()`, playing/pausing media, integrating non-React libraries.

### Focus on mount

```jsx
useEffect(() => {
  inputRef.current?.focus();
}, []);
```

`ref.current` is `null` until the element is mounted, and `null` again after it unmounts, so use it in handlers or effects, not during render.

### Refs for lists

You can't call `useRef` in a loop (see [`00-hook-rules.md`](./00-hook-rules.md)). Use a **ref callback** to build a map:

```jsx
const itemsRef = useRef(new Map());

{items.map((item) => (
  <li
    key={item.id}
    ref={(node) => {
      if (node) itemsRef.current.set(item.id, node);
      else itemsRef.current.delete(item.id);
    }}
  />
))}
```

### Ref callbacks

A function passed to `ref` is called with the node on mount and `null` on unmount. This also works for measuring:

```jsx
const [height, setHeight] = useState(0);
const measuredRef = (node) => {
  if (node !== null) setHeight(node.getBoundingClientRect().height);
};

<div ref={measuredRef}>...</div>
```

---

## Passing refs to your own components

In React 19, `ref` is an ordinary prop for function components, so a component can pass it through to a DOM element:

```jsx
function MyInput({ ref, ...props }) {
  return <input ref={ref} {...props} />;
}

<MyInput ref={inputRef} />
```

In earlier versions you needed `forwardRef`. Exposing a limited API instead of the whole DOM node uses `useImperativeHandle`; both are covered in [`../16-advanced-react/01-refs-and-imperative-handles.md`](../16-advanced-react/01-refs-and-imperative-handles.md).

---

## Rules for using refs

- **Don't read or write `ref.current` during rendering.** Rendering must be pure ([`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)); a ref that changes outside React's knowledge makes output unpredictable. Read and write refs in **event handlers and effects**.
  - One acceptable exception is lazy initialization: `if (ref.current === null) ref.current = new Expensive();`.
- **Don't use a ref for something that should update the UI** — use state.
- **Avoid manipulating the DOM that React manages** (adding/removing children, changing text). Stick to non-destructive operations like focus and scroll, or React's tracked DOM and the real DOM can diverge.

---

## Common mistakes

- **Using a ref for displayed data** — the UI won't update when `ref.current` changes.
- **Reading `ref.current` during render** — can be `null` on first render, and makes rendering impure.
- **Expecting `ref.current` to be set immediately** — it's assigned after the commit, so use it in an effect or handler.
- **Calling `useRef` in a loop or conditionally** — breaks the Rules of Hooks; use a ref callback or a child component.
- **Mutating DOM nodes React owns** — causes React and the DOM to disagree.
- **Forgetting the null check** — `inputRef.current.focus()` throws if the element isn't mounted; use `?.`.

## Quick summary

- `useRef` returns a persistent `{ current }` object that doesn't trigger renders when changed
- Use it for bookkeeping values (timer ids, previous values) and DOM access
- If it affects what's displayed, use state instead
- Read and write refs in handlers and effects, not during render
- Use ref callbacks for lists; in React 19, `ref` is a normal prop on function components

## Next

**[`05-useContext.md`](./05-useContext.md)** covers passing data deeply through the tree without prop drilling.
