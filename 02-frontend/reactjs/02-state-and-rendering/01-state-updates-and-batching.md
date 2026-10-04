# State Updates and Batching

Knowing that state is a snapshot ([`00-state-and-snapshots.md`](./00-state-and-snapshots.md)), the next questions are practical: how do you update state based on its previous value, how does React handle several updates at once, and how do you update objects and arrays correctly? This file answers all three.

## Prerequisites

[`00-state-and-snapshots.md`](./00-state-and-snapshots.md), and spread syntax from [`../00-setup/00-javascript-for-react.md`](../00-setup/00-javascript-for-react.md).

---

## Batching

React doesn't re-render after every setter call. It **waits until all the code in an event handler has finished**, processes the queued updates together, and renders **once**.

```jsx
function handleClick() {
  setFirstName("Ada");
  setLastName("Lovelace");
  setCount(count + 1);
  // one re-render after the handler finishes, not three
}
```

This is **batching**. It keeps the UI consistent (you never see a half-updated state) and avoids wasted renders.

Since React 18, batching is **automatic everywhere** — inside event handlers, timeouts, promises, and native event handlers. You no longer need to restructure code to get it.

React also guarantees that **one event is fully handled before the next**. Two quick clicks are processed separately, never merged.

---

## Updating based on the previous value

Because state is a snapshot, this doesn't do what it looks like:

```jsx
function handleClick() {
  setCount(count + 1);
  setCount(count + 1);
  setCount(count + 1);
}
// count goes from 0 to 1, not 3
```

All three calls use the same snapshot, where `count` is `0`. Each says "set it to `1`".

### The updater function

Pass a **function** to compute the next state from the latest queued state:

```jsx
function handleClick() {
  setCount((prev) => prev + 1);
  setCount((prev) => prev + 1);
  setCount((prev) => prev + 1);
}
// count goes from 0 to 3
```

How React processes the queue:

| Queued update | `prev` | Result |
|---------------|--------|--------|
| `prev => prev + 1` | 0 | 1 |
| `prev => prev + 1` | 1 | 2 |
| `prev => prev + 1` | 2 | 3 |

During the next render, React hands each updater the result of the previous one.

### Mixing values and updaters

A plain value **replaces** the state; an updater **transforms** it. They're processed in order:

```jsx
setCount(count + 5);          // "replace with 5" (when count was 0)
setCount((prev) => prev + 1); // 5 → 6
setCount(42);                 // "replace with 42"
// final value: 42
```

### When to use which

- Next state depends on previous state → **use the updater form**: `setCount(prev => prev + 1)`.
- Next state is independent (e.g., from an input's `e.target.value`) → pass the value directly.
- Updates from async code (timers, promises, event listeners) → prefer the updater form so you don't use a stale snapshot.

**Updater functions must be pure.** They run during rendering (and twice in StrictMode), so don't mutate or cause side effects inside them.

---

## State updates replace, they don't merge

Unlike class components' `setState`, a `useState` setter **replaces** the value:

```jsx
const [user, setUser] = useState({ name: "Ada", age: 36 });

setUser({ name: "Grace" });   // user is now { name: "Grace" } — age is gone
```

Copy the existing fields yourself, as shown next. (Alternatively, use separate state variables for unrelated values.)

---

## Treat state as immutable

State can hold any JavaScript value, including objects and arrays. But you must **never mutate** them — create a new one and pass it to the setter.

```jsx
// ❌ Mutation: same object reference, React sees no change
user.name = "Grace";
setUser(user);

// ✅ Replacement: a new object
setUser({ ...user, name: "Grace" });
```

Why:

- React decides whether to re-render by comparing the **reference**. Mutating keeps the same reference, so React may skip the update.
- Predictable snapshots: old renders keep their old data.
- Memoization and effects depend on reference changes (see [`../14-performance/02-memoization.md`](../14-performance/02-memoization.md)).

### Updating objects

```jsx
setUser((prev) => ({ ...prev, name: "Grace" }));       // change one field
setUser((prev) => ({ ...prev, address: { ...prev.address, city: "Paris" } })); // nested
```

Spread is **shallow**, so nested objects need their own spread at every level you change. Deep nesting gets awkward — flatten your state (see [`02-state-structure-and-lifting.md`](./02-state-structure-and-lifting.md)) or use Immer (below).

### Updating arrays

| Goal | ❌ Avoid (mutates) | ✅ Prefer (returns new array) |
|------|-------------------|------------------------------|
| Add | `push`, `unshift` | `[...arr, item]`, `[item, ...arr]` |
| Remove | `pop`, `shift`, `splice` | `filter(x => x.id !== id)` |
| Replace / update | `arr[i] = x` | `map(x => x.id === id ? updated : x)` |
| Insert | `splice` | `[...arr.slice(0, i), item, ...arr.slice(i)]` |
| Sort / reverse | `sort`, `reverse` on the original | `[...arr].sort(...)`, `[...arr].reverse()`, or `toSorted()` / `toReversed()` |

```jsx
// Add
setTodos((prev) => [...prev, { id: nextId++, text, done: false }]);

// Remove
setTodos((prev) => prev.filter((t) => t.id !== id));

// Toggle
setTodos((prev) =>
  prev.map((t) => (t.id === id ? { ...t, done: !t.done } : t))
);
```

Note the object inside `map`: `{ ...t, done: !t.done }` copies the changed item too — don't mutate `t.done` in place.

---

## Immer (optional)

For deeply nested state, the `use-immer` library lets you write mutation-looking code that is safely converted to immutable updates:

```jsx
import { useImmer } from "use-immer";

const [person, updatePerson] = useImmer({ address: { city: "Rome" } });

updatePerson((draft) => {
  draft.address.city = "Paris";   // draft is a safe proxy, not your real state
});
```

Learn plain immutable updates first; Immer is a convenience, not a replacement for understanding them. Redux Toolkit uses the same approach internally (see [`../13-state-management/04-redux-toolkit.md`](../13-state-management/04-redux-toolkit.md)).

---

## Forcing a synchronous update (rare)

Occasionally you need the DOM updated *immediately* after a state change — for example, to scroll to an element you just added. `flushSync` from `react-dom` does this, but it opts out of batching and hurts performance. Reach for it only after trying alternatives such as effects or refs.

---

## Common mistakes

- **Calling `setX(x + 1)` repeatedly** and expecting a stack — use `setX(prev => prev + 1)`.
- **Mutating objects or arrays in state** (`push`, `arr[i] = …`, `obj.key = …`) — the UI may not update, and bugs appear far from the cause.
- **Forgetting that setters replace** — `setUser({ name })` drops other fields.
- **Shallow-copying nested data** — updating `address.city` without copying `address` mutates the old state.
- **`sort`/`reverse` on state arrays** — they mutate; copy first.
- **Side effects inside updater functions** — updaters must be pure.
- **Using the updater form inside a closure but reading state elsewhere** — mixing both in one handler leads to confusing results; decide on one approach per update.

## Quick summary

- React batches all updates in a handler (and, since React 18, everywhere) into one render
- Use `setX(prev => ...)` whenever the new value depends on the old
- Setters replace state; copy existing fields when updating objects
- Never mutate state: use spread, `map`, `filter`, and copy-then-sort
- Flatten state or use Immer if nested updates get painful

## Next

**[`02-state-structure-and-lifting.md`](./02-state-structure-and-lifting.md)** covers how to shape state so updates stay simple, and where to put it.
