# useState

`useState` adds a state variable to a component. This file is the **API reference and practical patterns**. The mental model behind it — state as a snapshot, batching, immutable updates — lives in [`../02-state-and-rendering/`](../02-state-and-rendering/README.md) and is linked rather than repeated here.

## Prerequisites

[`00-hook-rules.md`](./00-hook-rules.md) and [`../02-state-and-rendering/00-state-and-snapshots.md`](../02-state-and-rendering/00-state-and-snapshots.md)

---

## Syntax

```jsx
const [state, setState] = useState(initialState);
```

| Part | Meaning |
|------|---------|
| `initialState` | The value for the **first** render only; ignored afterward |
| `state` | The current value (a snapshot for this render) |
| `setState` | Function that updates state and schedules a re-render |

```jsx
import { useState } from "react";

function Counter() {
  const [count, setCount] = useState(0);
  return <button onClick={() => setCount(count + 1)}>{count}</button>;
}
```

State can hold any value: numbers, strings, booleans, objects, arrays, `null`, even functions (with a caveat below).

---

## Setter forms

```jsx
setCount(5);                    // set directly
setCount((prev) => prev + 1);   // updater: compute from the latest queued value
```

Use the updater form whenever the next value depends on the previous one. Why, and how queued updates are processed: [`../02-state-and-rendering/01-state-updates-and-batching.md`](../02-state-and-rendering/01-state-updates-and-batching.md).

The setter returns nothing and does not change the variable in the current render.

---

## Lazy initialization

The initial value is only *used* on the first render, but an expression like `createTodos()` still **runs every render**:

```jsx
// ❌ createInitialTodos() is called on every render (result discarded after the first)
const [todos, setTodos] = useState(createInitialTodos());

// ✅ Pass the function itself; React calls it once, on the first render
const [todos, setTodos] = useState(createInitialTodos);
```

Use this for expensive work such as parsing `localStorage` or building large arrays:

```jsx
const [settings, setSettings] = useState(() => {
  const saved = localStorage.getItem("settings");
  return saved ? JSON.parse(saved) : defaultSettings;
});
```

Pass the function **reference or an arrow function** — don't call it. The initializer function must be pure and take no arguments.

### Storing a function in state

Because functions are treated as initializers/updaters, wrap a function value in another function:

```jsx
const [callback, setCallback] = useState(() => myFunction);   // store the function
setCallback(() => otherFunction);                               // replace it
```

This is rare; reconsider whether a ref or a different design fits better.

---

## Working with object and array state

The setter **replaces** the value, so you must copy the parts you're keeping, and never mutate. The full table of array operations and nested-object patterns is in [`../02-state-and-rendering/01-state-updates-and-batching.md`](../02-state-and-rendering/01-state-updates-and-batching.md). In brief:

```jsx
const [user, setUser] = useState({ name: "Ada", age: 36 });
setUser((prev) => ({ ...prev, age: prev.age + 1 }));

const [tags, setTags] = useState([]);
setTags((prev) => [...prev, "react"]);
setTags((prev) => prev.filter((t) => t !== "react"));
```

---

## Multiple state variables or one object?

- **Unrelated values** → separate `useState` calls (simpler updates).
- **Values that change together** → one object, or consider `useReducer` ([`06-useReducer.md`](./06-useReducer.md)).
- **Mutually exclusive states** (loading/success/error) → a single `status` value, not several booleans.

Guidelines in [`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md).

---

## Same value: React bails out

If you set state to a value that is `Object.is`-equal to the current one, React skips re-rendering the component's children and effects:

```jsx
setCount(count);   // no change → bails out (may still render this component once more)
```

This is also why **mutating then setting the same object** fails to update the UI — the reference is identical.

---

## Resetting state

To reset a component's state when something identifying it changes, give it a `key`:

```jsx
<Profile key={userId} userId={userId} />
```

Details in [`../02-state-and-rendering/05-state-preservation-and-reset.md`](../02-state-and-rendering/05-state-preservation-and-reset.md). To reset within the component, call the setter with the initial value.

---

## Derived values don't belong in state

If something can be computed from existing state or props, compute it in the render body:

```jsx
const [items, setItems] = useState([]);
const total = items.reduce((sum, i) => sum + i.price, 0);   // not another useState
```

See [`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md).

---

## Server-side and other notes

- State is **not shared** between components, even if they call the same hook ([`00-hook-rules.md`](./00-hook-rules.md)).
- State resets on remount; to persist it across reloads, combine with `localStorage` ([`10-hook-recipes.md`](./10-hook-recipes.md)).
- TypeScript: pass a generic when the initial value can't infer the type, e.g. `useState<User | null>(null)` — see [`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md).

---

## Common mistakes

- **Mutating state** (`user.name = "x"; setUser(user)`) — UI doesn't update; create a new object.
- **Calling an expensive initializer every render** — use `useState(fn)`, not `useState(fn())`.
- **Reading state right after setting it** — you get the old snapshot; use a local variable for the new value.
- **Multiple `setCount(count + 1)` calls** — use `setCount(prev => prev + 1)`.
- **Initializing with `undefined` then switching to a defined value on an input** — causes a controlled/uncontrolled warning; start with `""`.
- **Copying props into state** — it won't follow later prop changes.
- **Setting state during render unconditionally** — causes an infinite loop. (Conditional "adjust during render" for the same component is a documented advanced pattern; see [`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md).)

## Quick summary

- `const [value, setValue] = useState(initial)`; initial is used only on first render
- Pass an updater function when the next value depends on the previous
- Pass a function to `useState` for expensive initialization
- Never mutate objects or arrays in state; copy them
- Prefer one status value over multiple booleans; derive values instead of storing them
- Use `key` to reset a component's state

## Next

**[`02-useEffect.md`](./02-useEffect.md)** covers synchronizing a component with systems outside React.
