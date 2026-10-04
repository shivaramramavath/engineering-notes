# State and Snapshots

Components need to remember things: what's typed in a box, which tab is open, whether a menu is expanded. That memory is **state**. This file explains why ordinary variables can't do the job, what `useState` adds, and the single most useful mental model in React: **state is a snapshot**.

## Prerequisites

[`../01-fundamentals/06-events.md`](../01-fundamentals/06-events.md) and closures from [`../00-setup/00-javascript-for-react.md`](../00-setup/00-javascript-for-react.md).

---

## Why a regular variable isn't enough

```jsx
function Counter() {
  let count = 0;

  function handleClick() {
    count = count + 1;   // changes the variable...
  }

  return <button onClick={handleClick}>Clicked {count}</button>;
}
```

Clicking does nothing visible. Two reasons:

1. **Local variables don't persist.** Every render calls the function again, so `count` starts at `0` again.
2. **Changing a local variable doesn't trigger a render.** React has no way to know something changed.

State fixes both:

```jsx
import { useState } from "react";

function Counter() {
  const [count, setCount] = useState(0);

  return <button onClick={() => setCount(count + 1)}>Clicked {count}</button>;
}
```

`useState` gives you:

- **A state variable** (`count`) that React **retains between renders**.
- **A setter** (`setCount`) that updates it **and tells React to re-render**.

The setter's name is up to you, but `[thing, setThing]` is the convention.

---

## State is private to a component instance

State belongs to **one instance** of a component in the tree. Render `<Counter />` twice and each has its own count:

```jsx
<Counter />   {/* count: 0 */}
<Counter />   {/* count: 0, independent */}
```

State isn't tied to the function — it's tied to a **position in the tree** (more in [`05-state-preservation-and-reset.md`](./05-state-preservation-and-reset.md)). It isn't visible to the parent unless passed down as props.

---

## State is a snapshot

When React renders a component, it hands that render a **snapshot** of props and state. Everything inside that render — the JSX and every handler it creates — sees the values from that snapshot, and **those values never change during that render**.

Setting state doesn't modify the variable you already have. It asks React for a **new render** with the new value.

```jsx
function Counter() {
  const [count, setCount] = useState(0);

  return (
    <button
      onClick={() => {
        setCount(count + 1);
        console.log(count);   // still logs the OLD value
      }}
    >
      {count}
    </button>
  );
}
```

On the first click, this logs `0`, not `1`. In this render, `count` **is** `0` — a constant. The new value `1` exists only in the *next* render.

### A famous example: the delayed alert

```jsx
function Counter() {
  const [number, setNumber] = useState(0);

  return (
    <button
      onClick={() => {
        setNumber(number + 5);
        setTimeout(() => alert(number), 3000);
      }}
    >
      {number}
    </button>
  );
}
```

Click the button, then click it again within the three seconds. What do the alerts show? **`0`**, both times — not `5` or `10`. The handler closed over the `number` from the render where it was created, and that render's `number` was `0`. The alert shows what the state *was* when you clicked.

This isn't a bug; it's what makes behavior predictable. A handler always reflects the UI the user was looking at when they triggered it.

---

## The mental model

1. React calls your component, passing the current props and state as a **snapshot**.
2. Your component returns JSX describing the UI **for that snapshot**, with handlers that close over those values.
3. The user interacts; a handler calls a setter.
4. React **queues** the update and renders again with a **new snapshot**.

Think of each render as a photograph. The photo never changes; asking for a new state means taking another one.

---

## Practical consequences

- **Don't read state right after setting it** and expect the new value. Compute the next value in a variable if you need it immediately:

  ```jsx
  const next = count + 1;
  setCount(next);
  save(next);   // uses the value you just computed
  ```

- **Multiple updates in one handler based on the same snapshot** won't stack unless you use the updater form — see [`01-state-updates-and-batching.md`](./01-state-updates-and-batching.md).
- **Async code sees old values.** Timers and awaited promises use the snapshot from when they started. Use the updater form or a ref when you genuinely need the latest value (see [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md)).
- **State isn't reactive like a variable** — React re-runs the whole component function; nothing "watches" `count`.

---

## What counts as state

State is data that **changes over time** and that **the UI depends on**. Not everything belongs in it:

- Derived from other state or props? Compute it during render. See [`02-state-structure-and-lifting.md`](./02-state-structure-and-lifting.md).
- Needed across renders but not for display (timer ids, DOM nodes)? Use a ref.
- Constant? Use a normal variable outside the component.

---

## Common mistakes

- **Using a plain variable for UI data** — it resets every render and never triggers one.
- **Expecting the state variable to change right after calling the setter** — it's a snapshot; the new value arrives next render.
- **Mutating state directly** (`count++`, `user.name = "x"`) — React isn't notified. Always use the setter.
- **Expecting an async callback to see the latest state** — it sees the render it was created in.
- **Storing derived values in state** — they drift out of sync; compute them instead.

## Quick summary

- Regular variables reset every render and don't trigger updates; state does both jobs
- `useState` returns the current value and a setter that schedules a re-render
- Each component instance has its own state
- State is a snapshot: within one render its value is fixed, and handlers see that value
- Setting state requests a new render with a new snapshot

## Next

**[`01-state-updates-and-batching.md`](./01-state-updates-and-batching.md)** covers updating state correctly, including batching and immutable updates.
