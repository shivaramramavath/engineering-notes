# useEffect

`useEffect` lets a component **synchronize with a system outside React**: a server, a browser API, a subscription, a third-party widget. It runs after React updates the screen. It is also the most misused hook, so this file covers both how it works and when *not* to reach for it.

## Prerequisites

[`01-useState.md`](./01-useState.md), [`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md), and [`../02-state-and-rendering/04-component-lifecycle.md`](../02-state-and-rendering/04-component-lifecycle.md)

---

## What an effect is

During rendering, a component must be pure ([`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)). Anything that reaches outside React — starting a timer, opening a connection, changing `document.title`, calling an API — is a **side effect** and doesn't belong in the render body.

- **Events cause side effects** → put them in **event handlers** (a click sends a request).
- **Rendering itself causes side effects** → put them in an **effect** (the component appearing on screen opens a connection).

---

## Syntax

```jsx
useEffect(() => {
  // setup: runs after the component is committed to the DOM
  return () => {
    // cleanup (optional): runs before the next setup, and on unmount
  };
}, [dependencies]);
```

```jsx
function PageTitle({ title }) {
  useEffect(() => {
    document.title = title;
  }, [title]);

  return <h1>{title}</h1>;
}
```

---

## The dependency array

Dependencies tell React when to re-run the effect.

| Array | Behavior |
|-------|----------|
| `[a, b]` | Runs after the first render, then after any render where `a` or `b` changed (compared with `Object.is`) |
| `[]` | Runs once after mount (setup), cleanup on unmount |
| *omitted* | Runs after **every** render |

**Dependencies aren't a choice.** They're every reactive value (props, state, and variables or functions derived from them) that the effect reads. You don't pick them; you *declare* them. The `react-hooks/exhaustive-deps` rule ([`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)) checks this for you.

```jsx
useEffect(() => {
  const connection = createConnection(serverUrl, roomId);
  connection.connect();
  return () => connection.disconnect();
}, [serverUrl, roomId]);   // both are used inside, so both are listed
```

Don't silence the lint rule to "fix" an effect. If the dependency array feels wrong, the effect's *design* is usually wrong.

---

## Cleanup

Cleanup undoes what setup did, and runs:

1. **Before** the effect re-runs with new dependencies (using the *old* values).
2. When the component **unmounts**.

```jsx
useEffect(() => {
  function handleResize() { setWidth(window.innerWidth); }
  window.addEventListener("resize", handleResize);
  return () => window.removeEventListener("resize", handleResize);
}, []);
```

Every setup should have a matching cleanup: subscriptions, timers, event listeners, connections. In development, StrictMode runs `setup → cleanup → setup` once to verify yours works ([`../02-state-and-rendering/04-component-lifecycle.md`](../02-state-and-rendering/04-component-lifecycle.md)).

---

## Common uses

### Subscribing to a browser API or event

The resize example above. For subscribing to external data stores, prefer `useSyncExternalStore` ([`../16-advanced-react/02-external-stores.md`](../16-advanced-react/02-external-stores.md)).

### Controlling non-React widgets

```jsx
useEffect(() => {
  const map = new MapLibrary(ref.current);
  return () => map.destroy();
}, []);
```

### Fetching data

An effect *can* fetch, but you must handle **race conditions**: if `query` changes quickly, an older response can arrive after a newer one and overwrite it.

```jsx
useEffect(() => {
  let ignore = false;

  async function load() {
    const res = await fetch(`/api/search?q=${query}`);
    const data = await res.json();
    if (!ignore) setResults(data);
  }
  load();

  return () => { ignore = true; };   // cleanup marks stale responses
}, [query]);
```

Or cancel the request with `AbortController`:

```jsx
useEffect(() => {
  const controller = new AbortController();
  fetch(url, { signal: controller.signal })
    .then((r) => r.json())
    .then(setData)
    .catch((e) => { if (e.name !== "AbortError") setError(e); });
  return () => controller.abort();
}, [url]);
```

Hand-written fetching in effects also lacks caching, deduplication, retries, and loading/error handling. For real apps, prefer a data library ([`../12-server-state/03-tanstack-query.md`](../12-server-state/03-tanstack-query.md)); learn the effect version in [`../12-server-state/01-fetching-data.md`](../12-server-state/01-fetching-data.md).

---

## Objects and functions as dependencies

Objects and functions created inside the component body are **new on every render**, so an effect that depends on them re-runs every time:

```jsx
function Room({ roomId }) {
  const options = { serverUrl, roomId };   // new object each render

  useEffect(() => {
    const c = createConnection(options);
    c.connect();
    return () => c.disconnect();
  }, [options]);   // ❌ reconnects on every render
}
```

Fixes, in order of preference:

1. **Move the object/function inside the effect**, so it isn't a dependency.
2. **Depend on primitives** (`roomId`, `serverUrl`) instead of the object.
3. **Move it outside the component** if it doesn't use props or state.
4. As a last resort, memoize with `useMemo` / `useCallback` ([`07-useMemo-and-useCallback.md`](./07-useMemo-and-useCallback.md)).

---

## Reading the latest value without re-running

Sometimes an effect needs the latest props or state but shouldn't re-run when they change (for example, logging a visit with the current theme). Solutions: restructure so the value is a real dependency, store it in a ref ([`04-useRef.md`](./04-useRef.md)), or use `useEffectEvent`, which recent React versions provide for exactly this "event inside an effect" case — check the React docs for its current status in your version.

---

## When effects run in the timeline

```
Render → Commit (DOM updated) → Browser paints → Effects run
```

`useEffect` runs **after paint**, so it doesn't block the screen from updating. If you must measure or change layout **before** the browser paints (to avoid flicker), use `useLayoutEffect` ([`08-useLayoutEffect.md`](./08-useLayoutEffect.md)).

---

## Updating state inside effects

Effects can set state, but doing so triggers another render, so ask whether you need the effect at all. Setting state in an effect to mirror props or compute derived values is the most common mistake — see [`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md).

---

## Debugging

- Add a `console.log` in setup and cleanup to see when they run; expect doubles in dev because of StrictMode.
- If an effect runs too often, log the dependencies and compare them across renders (an object or function changing identity is the usual culprit).
- If it uses stale values, a dependency is missing.

---

## Common mistakes

- **Missing or lying dependency array** — leads to stale values; follow `exhaustive-deps`.
- **No cleanup** — leaked listeners, timers, and connections.
- **Race conditions when fetching** — use `ignore` flags or `AbortController`.
- **Object/function dependencies recreated each render** — move them inside or depend on primitives.
- **Using an effect to derive state or respond to events** — compute during render or use handlers ([`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md)).
- **An `async` function passed directly to `useEffect`** — effects must return nothing or a cleanup function, not a promise; define an inner async function and call it.
- **Infinite loops** — an effect that sets state it also depends on, with no guard.
- **Treating the double run in development as a bug** — it's StrictMode checking your cleanup.

## Quick summary

- Effects synchronize a component with external systems; they run after the commit
- Events → handlers; rendering-caused sync → effects
- Declare every reactive value you use in the dependency array; don't suppress the lint rule
- Always clean up what setup started
- Handle race conditions when fetching, or use a data library
- Avoid objects and functions as dependencies unless stable
- Many things people put in effects don't belong there

## Next

**[`03-you-might-not-need-an-effect.md`](./03-you-might-not-need-an-effect.md)** shows the common cases where an effect is the wrong tool, and what to do instead.
