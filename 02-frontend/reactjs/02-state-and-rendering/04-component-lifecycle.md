# Component Lifecycle

Every component instance has a life: it's **mounted** (added to the screen), **updated** many times as props and state change, and finally **unmounted** (removed). Class components exposed this life through named methods. Function components don't have those methods, and that's deliberate: you describe **what to synchronize**, not **when something happened**.

## Prerequisites

[`03-rendering.md`](./03-rendering.md)

---

## The three phases

| Phase | What happens |
|-------|--------------|
| **Mount** | React creates the component instance, renders it for the first time, and adds its output to the DOM |
| **Update** | State, props, or consumed context change; React re-renders and updates the DOM where output differs |
| **Unmount** | The component is removed from the tree (its parent stopped rendering it, or its position/type changed); React discards its state and DOM |

Mount and unmount happen once per instance. Updates happen any number of times. State lives from mount until unmount ([`05-state-preservation-and-reset.md`](./05-state-preservation-and-reset.md)).

---

## Where lifecycle logic goes in function components

You don't write "on mount" code directly in the body — the body runs on **every** render and must stay pure ([`03-rendering.md`](./03-rendering.md)). Instead, use `useEffect`, which runs **after** the commit:

```jsx
useEffect(() => {
  // setup: runs after the component is committed
  const id = setInterval(tick, 1000);

  return () => {
    // cleanup: runs before the next effect run, and on unmount
    clearInterval(id);
  };
}, []); // dependency array controls when it re-runs
```

The dependency array decides which phase the effect is tied to:

| Dependency array | Runs |
|------------------|------|
| `[]` | After mount; cleanup on unmount |
| `[a, b]` | After mount, and after any render where `a` or `b` changed; cleanup before each re-run and on unmount |
| *(omitted)* | After **every** render; cleanup before each re-run |

Full API in [`../03-hooks/02-useEffect.md`](../03-hooks/02-useEffect.md).

---

## Mapping from class lifecycle methods

If you read older code or interview questions, here's the rough equivalence:

| Class method | Function component equivalent |
|--------------|-------------------------------|
| `constructor` | `useState` initializer / top of the function |
| `render` | The function body (returning JSX) |
| `componentDidMount` | `useEffect(() => { ... }, [])` |
| `componentDidUpdate` | `useEffect(() => { ... }, [deps])` |
| `componentWillUnmount` | The cleanup function returned from an effect |
| `shouldComponentUpdate` | `React.memo` |
| `getDerivedStateFromProps` | Compute during render (see [`02-state-structure-and-lifting.md`](./02-state-structure-and-lifting.md)) |
| `componentDidCatch` | Error boundaries (still classes; see [`../15-concurrent-and-modern-react/04-error-boundaries.md`](../15-concurrent-and-modern-react/04-error-boundaries.md)) |

These are **approximate**. The mapping helps you read legacy code, but don't *think* in lifecycle terms when writing new code.

---

## Think in synchronization, not lifecycle

Lifecycle thinking asks: "what should happen when this mounts, updates, and unmounts?" This leads to scattering related logic across several places.

Effect thinking asks: "what external thing should this component stay **synchronized** with, and what does it need to do to start and stop?" You write one effect for one concern; React handles when to start, stop, and restart it.

```jsx
function ChatRoom({ roomId }) {
  useEffect(() => {
    const connection = createConnection(roomId);
    connection.connect();
    return () => connection.disconnect();
  }, [roomId]);
}
```

This single effect covers mounting (connect), changing rooms (disconnect from the old, connect to the new), and unmounting (disconnect) — without any "mount" or "update" branching.

Not everything needs an effect. Derived values, event-driven logic, and data transformations belong elsewhere; see [`../03-hooks/03-you-might-not-need-an-effect.md`](../03-hooks/03-you-might-not-need-an-effect.md).

---

## StrictMode and the mount–unmount–mount check

In development, StrictMode (see [`03-rendering.md`](./03-rendering.md)) **mounts, unmounts, then mounts again** every component once, to confirm that your cleanup truly undoes your setup:

```
setup → cleanup → setup
```

If an effect works correctly through that sequence, it will also survive real-world cases such as navigating away and back, or a component being remounted. If you see a subscription or request happen twice in development, it usually means your cleanup is missing or incomplete — not that React is buggy. This only happens in development.

---

## What unmounting does

When a component unmounts:

- Its **state is destroyed** (not preserved anywhere).
- Its **effects' cleanup functions run**.
- Its DOM nodes are removed.

A component unmounts when the parent stops rendering it (`{show && <Panel />}` becomes `false`), when it moves to a different position or type in the tree, or when its `key` changes.

---

## Common mistakes

- **Putting setup code in the component body** — it runs on every render and breaks purity.
- **Missing cleanup** — leaked timers, listeners, and subscriptions after unmount.
- **Wrong dependency array** — `[]` with values used inside leads to stale data; the lint rule `exhaustive-deps` exists to catch this ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)).
- **Treating effects as lifecycle hooks** — logic ends up split and out of sync; model each synchronization separately.
- **Reading StrictMode double-effects as a bug** — it's a development-only test for missing cleanup.
- **Using effects for things that belong in event handlers** — user-triggered actions (submitting a form) should run in the handler, not an effect.

## Quick summary

- Components mount once, update many times, and unmount once
- Use `useEffect` for work tied to those phases; the body must remain pure
- `[]` runs after mount, `[deps]` after changes, and cleanup runs before re-runs and on unmount
- Class lifecycle methods map only roughly onto effects; prefer synchronization thinking
- StrictMode runs setup → cleanup → setup in development to verify your cleanup
- Unmounting destroys state and runs cleanup

## Next

**[`05-state-preservation-and-reset.md`](./05-state-preservation-and-reset.md)** explains exactly when React keeps a component's state and when it throws it away.
