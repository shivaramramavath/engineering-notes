# External Stores

Not all data lives in React state. Browser APIs (`navigator.onLine`, `matchMedia`, `localStorage`), third-party libraries, and your own module-level stores all hold values **outside** React that components need to display. **`useSyncExternalStore`** is the hook for subscribing to them correctly.

```tsx
const snapshot = useSyncExternalStore(subscribe, getSnapshot, getServerSnapshot?)
```

You'll rarely write it directly for app state ([Zustand](../13-state-management/03-zustand.md) and Redux do it for you), but you'll use it to wrap browser APIs, build small stores, and it explains how those libraries stay correct.

## Why a special hook?

The tempting version:

```tsx
function useOnlineStatus() {
  const [online, setOnline] = useState(navigator.onLine)
  useEffect(() => {
    const update = () => setOnline(navigator.onLine)
    window.addEventListener("online", update)
    window.addEventListener("offline", update)
    return () => { /* remove listeners */ }
  }, [])
  return online
}
```

It works in simple cases, but has problems:

- **Missed updates**: the value can change between the first render and the effect that subscribes, and you never notice.
- **Tearing under [concurrent rendering](../15-concurrent-and-modern-react/00-concurrent-rendering.md#tearing-and-usesyncexternalstore)**: React may pause a render halfway. If the external value changes mid-render, some components render with the old value and some with the new, so the same screen shows **inconsistent data**.
- **Server rendering**: `navigator` doesn't exist on the server, and the client's first render must match the server's HTML.

`useSyncExternalStore` handles all three: it reads the value, subscribes, and makes React **re-check the value before committing**. If it changed mid-render, React re-renders synchronously so the UI is consistent.

## The contract

```tsx
useSyncExternalStore(
  subscribe,         // (callback) => unsubscribe
  getSnapshot,       // () => current value (must be cached/stable)
  getServerSnapshot  // () => value for server render and hydration (optional)
)
```

1. **`subscribe(callback)`**: register `callback` to be called when the store changes; **return a function that unsubscribes**.
2. **`getSnapshot()`**: return the store's **current value**. React calls it often (every render, and to check for changes).
3. **`getServerSnapshot()`**: the value to use when rendering on the server and during hydration. Required for SSR; must return the same value on the client's first render as on the server.

The hook returns the snapshot, and re-renders the component when `getSnapshot()` returns something different (compared with `Object.is`).

## Example: online status

```tsx
function subscribe(callback: () => void) {
  window.addEventListener("online", callback)
  window.addEventListener("offline", callback)
  return () => {
    window.removeEventListener("online", callback)
    window.removeEventListener("offline", callback)
  }
}

export function useOnlineStatus() {
  return useSyncExternalStore(
    subscribe,
    () => navigator.onLine,        // client snapshot
    () => true                     // server snapshot: assume online
  )
}
```

Cleaner than the effect version, with no state to keep in sync, and it's tear-free.

## Example: media queries

```tsx
export function useMediaQuery(query: string) {
  const subscribe = useCallback(
    (callback: () => void) => {
      const mql = window.matchMedia(query)
      mql.addEventListener("change", callback)
      return () => mql.removeEventListener("change", callback)
    },
    [query]
  )

  return useSyncExternalStore(
    subscribe,
    () => window.matchMedia(query).matches,
    () => false                    // no window on the server
  )
}

const isDesktop = useMediaQuery("(min-width: 768px)")
const prefersReducedMotion = useMediaQuery("(prefers-reduced-motion: reduce)")
```

Here `subscribe` depends on `query`, so it's wrapped in `useCallback`. See the next section for why identity matters. Prefer CSS media queries for pure styling ([responsive design](../07-styling/03-responsive-design.md)); use this hook when JavaScript logic needs the answer (rendering different components, disabling animations).

## Snapshot rules (where bugs come from)

### 1. `getSnapshot` must return a stable value

React compares successive snapshots with `Object.is`. If `getSnapshot` returns a **new object or array every call**, React thinks the store changed on every check, and loops:

```tsx
// ✗ new object each call → infinite re-render ("getSnapshot should be cached")
useSyncExternalStore(subscribe, () => ({ width: window.innerWidth, height: window.innerHeight }))

// ✓ return a primitive…
useSyncExternalStore(subscribe, () => window.innerWidth)

// ✓ …or cache the object and only replace it when the data actually changes
let cached = { width: window.innerWidth, height: window.innerHeight }
function getSnapshot() {
  if (cached.width !== window.innerWidth || cached.height !== window.innerHeight) {
    cached = { width: window.innerWidth, height: window.innerHeight }
  }
  return cached
}
```

In development React warns "The result of getSnapshot should be cached to avoid an infinite loop". Treat it as a bug.

### 2. Treat store data as immutable

If your store **mutates** an object in place and returns the same reference, `Object.is` says "unchanged" and React doesn't re-render. Replace the value with a **new** one when something changes.

### 3. Keep `subscribe` stable

If you define `subscribe` **inline in the component**, it's a new function every render, so React unsubscribes and resubscribes each time. Wasteful, and can miss updates in the gap. Define it at module level (like `useOnlineStatus`), or `useCallback` it when it depends on props (like `useMediaQuery`).

### 4. Keep `getSnapshot` cheap

It's called on every render and frequently during commits. No heavy computation, and no side effects.

## Building a tiny store

This is essentially Zustand's core:

```ts
// store.ts
export function createStore<T>(initial: T) {
  let state = initial
  const listeners = new Set<() => void>()

  return {
    getState: () => state,
    setState(next: T | ((prev: T) => T)) {
      const value = typeof next === "function" ? (next as (prev: T) => T)(state) : next
      if (Object.is(value, state)) return          // no change, no notification
      state = value
      listeners.forEach((listener) => listener())
    },
    subscribe(listener: () => void) {
      listeners.add(listener)
      return () => { listeners.delete(listener) }
    },
  }
}
```

```tsx
// use-store.ts
import { useSyncExternalStore } from "react"

export function useStore<T, S>(store: Store<T>, selector: (state: T) => S): S {
  return useSyncExternalStore(
    store.subscribe,
    () => selector(store.getState()),
    () => selector(store.getState())     // server snapshot: same initial state
  )
}

// usage
const counter = createStore({ count: 0 })

function Count() {
  const count = useStore(counter, (s) => s.count)     // re-renders only when `count` changes
  return <button onClick={() => counter.setState((s) => ({ count: s.count + 1 }))}>{count}</button>
}
```

Selectors work because `getSnapshot` returns `selector(state)`. A component re-renders only when the **selected value** changes. Selectors must return a **stable** value (a primitive, or an existing sub-object from state), never a freshly built object, for the same reason as rule 1. That's exactly the "selecting new objects causes loops" caveat in [Zustand v5](../13-state-management/03-zustand.md#selecting-several-values).

The store can be read and written **outside React** (`counter.getState()`, `counter.setState(...)`), which is why this pattern suits API clients, event handlers, and non-React code.

## Example: `localStorage` with cross-tab sync

```tsx
function subscribeToStorage(callback: () => void) {
  window.addEventListener("storage", callback)       // fires in OTHER tabs when storage changes
  return () => window.removeEventListener("storage", callback)
}

export function useLocalStorageValue(key: string) {
  return useSyncExternalStore(
    subscribeToStorage,
    () => localStorage.getItem(key),                 // string | null: a stable primitive
    () => null
  )
}

function setLocalStorageValue(key: string, value: string) {
  localStorage.setItem(key, value)
  window.dispatchEvent(new StorageEvent("storage", { key }))   // `storage` doesn't fire in the SAME tab, so notify manually
}
```

Two real-world details: the `storage` event doesn't fire in the tab that made the change (hence the manual dispatch), and storage can throw (private mode, quota). Wrap access in `try/catch` in production code. Don't store sensitive data here ([authentication](../11-api-integration/03-authentication.md#where-to-store-a-token)).

## Server rendering and hydration

`getServerSnapshot` is used **on the server and during hydration** so the client's first render matches the server HTML:

```tsx
useSyncExternalStore(subscribe, () => window.matchMedia(q).matches, () => false)
```

Server and first-client render both see `false`. Right after hydration React re-checks `getSnapshot`, and if the real value differs (the user is on a wide screen), it re-renders with it. Omitting `getServerSnapshot` makes server rendering throw. A snapshot that differs between server and first client render causes a [hydration mismatch](../15-concurrent-and-modern-react/07-server-components-and-ssr.md#hydration-mismatches). In a client-only SPA, you can omit it.

## When to use it

| Situation | Use |
|---|---|
| Browser APIs that change over time (online status, media queries, window size, visibility, storage) | `useSyncExternalStore` |
| A small module-level store shared by components | `useSyncExternalStore` (or just use Zustand) |
| Integrating a third-party non-React state source | `useSyncExternalStore` |
| State that belongs to React components | `useState` / `useReducer` / context |
| Server data | A [query cache](../12-server-state/README.md) (TanStack Query itself subscribes via this hook internally) |
| One-off "run something when X happens" with no value to render | `useEffect` |

Libraries built on it include Zustand (v5), React-Redux, and TanStack Query's React bindings, which is why you rarely call it for app state.

## Common mistakes

- **`getSnapshot` returning a new object/array each call**, causing an infinite loop and a "should be cached" warning.
- **Mutating the store's state in place** and returning the same reference, so no re-render.
- **Inline `subscribe`**, resubscribing every render.
- **Missing `getServerSnapshot`** with SSR (server error), or one that differs from the client's first render (hydration mismatch).
- **Doing heavy work or side effects in `getSnapshot`.**
- **Forgetting to return an unsubscribe function**, leaking listeners.
- **Using `useState` + `useEffect` to mirror an external store** and accepting tearing and missed updates.
- **Using it for ordinary component state**, where `useState` is simpler.
- **Selectors that build new objects**, re-rendering every time.
- **Ignoring `localStorage` quirks**: no same-tab `storage` event, and access can throw.

## Quick summary

- `useSyncExternalStore(subscribe, getSnapshot, getServerSnapshot)` safely reads data React doesn't own, tear-free under concurrent rendering.
- `subscribe` registers a callback and **returns an unsubscribe**; `getSnapshot` returns the **current, stable** value; `getServerSnapshot` supports SSR/hydration.
- Snapshots are compared with `Object.is`: return primitives or cached/immutable values, never fresh objects.
- Keep `subscribe` stable (module-level or `useCallback`) and `getSnapshot` cheap.
- A tiny store is ~20 lines (state + listeners + `subscribe`), and selectors give per-slice re-renders. This is the heart of Zustand.
- Use it for browser APIs and non-React state; use `useState`/context for React-owned state and a query cache for server data.

## Next

[03 — Internationalization](./03-internationalization.md)
