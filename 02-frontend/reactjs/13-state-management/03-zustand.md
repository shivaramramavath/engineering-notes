# Zustand

Zustand is a small state library built around one idea: **a store is a hook.** You define state and the functions that change it in one place, and components subscribe to exactly the slices they read. No providers, no reducers required, and the store is reachable from non-React code too.

```bash
npm install zustand
```

This note covers **v5** (named `create` export, built on React's `useSyncExternalStore`; see [external stores](../16-advanced-react/02-external-stores.md) for how that works).

## A first store

```ts
// stores/counter-store.ts
import { create } from "zustand"

type CounterState = {
  count: number
  increment: () => void
  reset: () => void
}

export const useCounterStore = create<CounterState>()((set) => ({
  count: 0,
  increment: () => set((state) => ({ count: state.count + 1 })),
  reset: () => set({ count: 0 }),
}))
```

```tsx
function Counter() {
  const count = useCounterStore((state) => state.count)
  const increment = useCounterStore((state) => state.increment)
  return <button onClick={increment}>Clicked {count} times</button>
}
```

- **State and actions live together** in the object you return.
- `create<CounterState>()(...)`: note the **double call**. It's the TypeScript-friendly curried form, so inference works with middleware.
- `set(partial)` **shallow-merges** into the state (like `setState` in class components), so you don't spread the whole state. To replace entirely, pass `true` as the second argument.
- `set` also accepts a function of the current state, used when the new value depends on the old one.

## Selectors: the key to performance

```tsx
const count = useCounterStore((state) => state.count)   // re-renders only when `count` changes
```

The function you pass is a **selector**. Zustand compares the selected value to its previous result (`Object.is`) and re-renders **only if it changed**. A component reading `count` ignores changes to other fields. That's the selective subscription Context can't give you ([01](./01-context-patterns-and-performance.md#the-performance-rule)).

> **Don't write `const store = useCounterStore()`** (no selector). It subscribes to the *entire* state, so the component re-renders on every change anywhere in the store.

### Selecting several values

In v5, a selector that returns a **new object or array each call** is treated as a changed value every time, which causes an infinite render loop ("Maximum update depth exceeded"):

```tsx
// ✗ new object every time
const { count, increment } = useCounterStore((s) => ({ count: s.count, increment: s.increment }))
```

Use **`useShallow`** to compare the contents instead:

```tsx
import { useShallow } from "zustand/react/shallow"

const { count, increment } = useCounterStore(
  useShallow((s) => ({ count: s.count, increment: s.increment }))
)
```

Or simply call the hook twice with primitive selectors (cheap and clear). Also select **derived** values in the selector when they're primitives:

```tsx
const total = useCartStore((s) => s.items.reduce((sum, i) => sum + i.price * i.qty, 0))
const isEmpty = useCartStore((s) => s.items.length === 0)
```

Actions are stable references (defined once), so selecting them never causes re-renders.

### Wrap the store in domain hooks

```ts
export const useCartCount = () => useCartStore((s) => s.items.reduce((n, i) => n + i.qty, 0))
export const useCartActions = () => useCartStore((s) => s.actions)   // see "actions namespace" below
```

Components get small, named hooks and never import the store shape directly. That's the [seam](./00-choosing-state-management.md#keep-a-seam) that makes a later change painless.

## A realistic store

```ts
// stores/cart-store.ts
import { create } from "zustand"

type CartItem = { id: string; name: string; price: number; qty: number }

type CartState = {
  items: CartItem[]
  actions: {
    add: (item: Omit<CartItem, "qty">) => void
    remove: (id: string) => void
    setQty: (id: string, qty: number) => void
    clear: () => void
  }
}

export const useCartStore = create<CartState>()((set) => ({
  items: [],
  actions: {
    add: (item) =>
      set((s) => {
        const existing = s.items.find((i) => i.id === item.id)
        return {
          items: existing
            ? s.items.map((i) => (i.id === item.id ? { ...i, qty: i.qty + 1 } : i))
            : [...s.items, { ...item, qty: 1 }],
        }
      }),
    remove: (id) => set((s) => ({ items: s.items.filter((i) => i.id !== id) })),
    setQty: (id, qty) =>
      set((s) => ({
        items: qty <= 0 ? s.items.filter((i) => i.id !== id) : s.items.map((i) => (i.id === id ? { ...i, qty } : i)),
      })),
    clear: () => set({ items: [] }),
  },
}))
```

Grouping functions under an **`actions` namespace** keeps them separate from data, makes `useCartStore((s) => s.actions)` a single stable selection, and makes it obvious what's state versus behavior.

Nested state still needs immutable updates: `set` only merges **one level**. For deep updates either spread carefully or use the Immer middleware.

## Reading state in actions: `get`

```ts
create<State>()((set, get) => ({
  items: [],
  checkoutTotal: () => get().items.reduce((s, i) => s + i.price * i.qty, 0),
}))
```

`get()` returns the current state, handy for logic that doesn't need to trigger a re-render.

## Async actions

Actions can be `async`; call `set` whenever you have a result. No thunks or middleware needed:

```ts
signIn: async (credentials) => {
  set({ status: "loading" })
  try {
    const { user } = await authApi.login(credentials)
    set({ status: "authenticated", user })
  } catch (error) {
    set({ status: "error" })
    throw error
  }
},
```

(For *server data*, still use a query cache; keep stores for client state. See [00](./00-choosing-state-management.md#anti-patterns).)

## Middleware

Wrap the creator function. The three you'll use:

```ts
import { create } from "zustand"
import { devtools, persist, createJSONStorage } from "zustand/middleware"
import { immer } from "zustand/middleware/immer"

export const useSettingsStore = create<SettingsState>()(
  devtools(
    persist(
      (set) => ({
        sidebarOpen: true,
        theme: "system",
        setTheme: (theme) => set({ theme }, false, "settings/setTheme"),   // 3rd arg = action name in devtools
        toggleSidebar: () => set((s) => ({ sidebarOpen: !s.sidebarOpen }), false, "settings/toggleSidebar"),
      }),
      {
        name: "settings",                                  // storage key
        storage: createJSONStorage(() => localStorage),    // default; or sessionStorage
        partialize: (s) => ({ sidebarOpen: s.sidebarOpen, theme: s.theme }),   // persist data only
        version: 1,
      }
    ),
    { name: "SettingsStore" }
  )
)
```

### `devtools`

Connects to the **Redux DevTools** browser extension: inspect state, see an action log, and time-travel. Naming actions via the third argument of `set` makes the log readable.

### `persist`

Saves state to storage and rehydrates on load.

- **`partialize`** persists only what you choose. Never persist functions or secrets.
- **`version` + `migrate`** let you change the state shape without crashing returning users: bump `version` and provide `migrate(oldState, oldVersion)`.
- **Hydration timing.** With synchronous storage like `localStorage` it hydrates immediately on the client; with async storage, or when the server-rendered HTML differs from the client, you can see a flash or a hydration mismatch. Check `useSettingsStore.persist.hasHydrated()` or `onFinishHydration` before rendering persisted-dependent UI.
- Treat persisted data as **untrusted input** (users can edit storage).
- **Don't persist tokens or sensitive data** in `localStorage` ([authentication](../11-api-integration/03-authentication.md#where-to-store-a-token)).

### `immer`

Write mutations in actions:

```ts
create<State>()(immer((set) => ({
  todos: [],
  toggle: (id) => set((draft) => {
    const todo = draft.todos.find((t) => t.id === id)
    if (todo) todo.done = !todo.done
  }),
})))
```

Compose order matters when stacking; the common order is `devtools(persist(immer(...)))`. Check the docs for the middleware combination you use (TypeScript signatures differ slightly).

## Use outside React

The hook returned by `create` also carries the store API:

```ts
useAuthStore.getState().token                       // read now
useAuthStore.setState({ token: null })              // write
const unsub = useAuthStore.subscribe((state) => { /* react to changes */ })
```

This is Zustand's superpower for integration code. An API client or interceptor can read the token, a WebSocket handler can push to the store, and a route loader can check auth. All without hooks:

```ts
// lib/api/client.ts
const token = useAuthStore.getState().token
```

Use this sparingly. Reading with `getState()` in components bypasses subscriptions, so the component won't update.

## Organizing larger stores

- **Several small stores** (one per domain: `useCartStore`, `useUiStore`, `useAuthStore`) beat one mega-store. Selectors already limit re-renders, so there's no performance reason to merge them.
- **Slices pattern**: split one store's creator into functions (`createCartSlice`, `createUiSlice`) and combine them. It's useful when slices need to see each other; see the Zustand docs for the typed `StateCreator` approach.
- Put each store next to its feature ([feature-based architecture](../20-frontend-architecture/00-feature-based-architecture.md)).

## Testing

Stores are global singletons, so state **leaks between tests** unless you reset it:

```ts
const initial = useCartStore.getState()
beforeEach(() => useCartStore.setState(initial, true))   // `true` replaces instead of merging
```

Then test actions directly (`useCartStore.getState().actions.add(...)`) and assert on `getState()`. No rendering needed for logic tests. See [testing fundamentals](../18-testing-and-debugging/00-testing-fundamentals.md).

## Server rendering note

A module-level store is shared across requests on a server, which can leak one user's state to another. For SSR frameworks, create a **store per request** (a factory with `createStore` from `zustand/vanilla`, provided through context). In a client-only SPA this doesn't apply.

## Zustand vs the alternatives

- **vs Context**: selective re-renders, usable outside React, middleware. Costs one dependency.
- **vs Redux Toolkit** ([04](./04-redux-toolkit.md)): far less ceremony and no provider; Redux offers stricter structure, a richer ecosystem, and built-in tooling. Zustand's flexibility means conventions are yours to enforce.
- Zustand is a good default for **shared client state** in most apps.

## Common mistakes

- **Calling the hook without a selector**, subscribing to the whole store.
- **Selectors returning new objects/arrays** (infinite loop in v5; unnecessary re-renders before). Use `useShallow` or primitive selectors.
- **Mutating state directly** (`state.items.push(...)`) without Immer.
- **Assuming `set` deep-merges.** It merges one level.
- **Putting server data in a store.** Use a query cache.
- **Persisting sensitive or non-serializable data** (tokens, functions, class instances).
- **Ignoring hydration** and rendering persisted-dependent UI before it's loaded.
- **One giant store for everything**, with unrelated concerns.
- **Forgetting to reset stores between tests.**
- **Using `getState()` inside components** and expecting re-renders.
- **A global store on the server shared between requests.**
- **v4 habits in v5**: default `import create from "zustand"`, or `createWithEqualityFn`-style equality arguments (those moved to `zustand/traditional`).

## Quick summary

- A Zustand store is a hook: `create<State>()((set, get) => ({ state, actions }))`.
- **Always pass a selector**; only the selected value triggers re-renders. Use `useShallow` for multi-value selectors.
- `set` shallow-merges; use functional `set` for updates based on current state; keep actions in the store (an `actions` namespace helps).
- Middleware: `devtools` (Redux DevTools), `persist` (storage, with `partialize`/`version`), `immer` (mutable-style updates).
- Reach the store outside React with `getState` / `setState` / `subscribe`.
- Prefer several small stores, wrap them in domain hooks, reset them in tests, and keep server data out.

## Next

[04 — Redux Toolkit](./04-redux-toolkit.md)