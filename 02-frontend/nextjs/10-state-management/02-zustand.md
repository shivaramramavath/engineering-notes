# Zustand

Zustand is a small state library: a store is a hook, components subscribe to just the slices they select, and there is no provider required by default. It fits **shared, frequently updated client state** (a cart drawer, an editor, a multi-step wizard) where context would re-render too much.

> Next.js-specific guidance follows the Zustand docs for Next.js. API shapes are for Zustand v5; check the docs of your installed version.

## When to use it

| Use Zustand for | Do not use it for |
|---|---|
| UI state shared across distant Client Components | Server data (fetch in Server Components, or use a query cache) |
| Frequently changing values with narrow subscribers | State used by one component (use `useState`) |
| Client workflows: wizard steps, editor selection, filters not worth a URL | Anything that must survive refresh and be shareable (use the URL) |
| State read outside React (event listeners, utilities) | Per-user data that the server renders |

## Install

```bash
npm install zustand
```

## A basic store (client-only UI state)

```ts
// stores/cart-ui-store.ts
import { create } from "zustand";

type CartUiState = {
  isOpen: boolean;
  open: () => void;
  close: () => void;
  toggle: () => void;
};

export const useCartUi = create<CartUiState>()((set) => ({
  isOpen: false,
  open: () => set({ isOpen: true }),
  close: () => set({ isOpen: false }),
  toggle: () => set((s) => ({ isOpen: !s.isOpen })),
}));
```

```tsx
"use client";

import { useCartUi } from "@/stores/cart-ui-store";

export function CartButton() {
  const toggle = useCartUi((s) => s.toggle);        // select only what you need
  return <button onClick={toggle}>Cart</button>;
}

export function CartDrawer() {
  const isOpen = useCartUi((s) => s.isOpen);
  const close = useCartUi((s) => s.close);
  if (!isOpen) return null;
  return <aside><button onClick={close}>Close</button></aside>;
}
```

Details:

- In TypeScript use the curried form **`create<State>()(...)`** (note the extra `()`); it is required for correct inference and middleware.
- `set` merges shallowly: `set({ isOpen: true })` keeps other fields.
- `set((state) => ...)` when the new value depends on the old one.
- Components that call the hook must be Client Components (`"use client"`).

## Selectors: subscribe narrowly

A component re-renders when **the value its selector returns** changes (by `Object.is`).

```tsx
const count = useStore((s) => s.count);              // re-renders only when count changes
const state = useStore();                            // re-renders on ANY change (avoid)
```

Selecting a **new object or array** each time returns a new reference and causes a re-render on every store update; in Zustand v5 it can also trigger an infinite-loop error. Use `useShallow` to compare shallowly:

```tsx
import { useShallow } from "zustand/react/shallow";

const { name, email } = useStore(useShallow((s) => ({ name: s.name, email: s.email })));
const [a, b] = useStore(useShallow((s) => [s.a, s.b]));
```

Or select primitives separately with several hooks.

## The Next.js caveat: the server shares module state

A normal `create()` store is a **module-level singleton**. In Next.js:

- The server runs your module once and serves many requests with it.
- Client Components are **also rendered on the server** for the initial HTML, so the same store module executes there.

If a store is **written to** on the server, or **seeded with request-specific data**, one user's data can end up visible to another. The Zustand docs therefore recommend not using a global store on the server and instead creating **one store per request**.

A practical rule:

| Situation | Approach |
|---|---|
| Pure client UI state, only mutated by user events in the browser, no server-side initialization (the cart drawer above) | A simple `create()` store is fine; the server only ever sees its defaults |
| The store is **initialized from server data** (props, cookies, a user) | **Per-request store** via a provider (below) |
| Server code writes to it during render | Never; use per-request instances or avoid the store |

## Per-request store (the safe pattern)

Use the vanilla `createStore` to build a **factory**, and give each provider instance its own store, created once and kept in a ref.

```ts
// stores/counter-store.ts
import { createStore } from "zustand/vanilla";

export type CounterState = { count: number };
export type CounterActions = { increment: () => void; decrement: () => void };
export type CounterStore = CounterState & CounterActions;

export const defaultInitState: CounterState = { count: 0 };

export const createCounterStore = (initState: CounterState = defaultInitState) =>
  createStore<CounterStore>()((set) => ({
    ...initState,
    increment: () => set((s) => ({ count: s.count + 1 })),
    decrement: () => set((s) => ({ count: s.count - 1 })),
  }));
```

```tsx
// providers/counter-store-provider.tsx
"use client";

import { createContext, useContext, useRef, type ReactNode } from "react";
import { useStore } from "zustand";
import { createCounterStore, type CounterStore } from "@/stores/counter-store";

type CounterStoreApi = ReturnType<typeof createCounterStore>;

const CounterStoreContext = createContext<CounterStoreApi | undefined>(undefined);

export function CounterStoreProvider({ children, initialCount }: { children: ReactNode; initialCount?: number }) {
  const storeRef = useRef<CounterStoreApi | null>(null);
  if (storeRef.current === null) {
    storeRef.current = createCounterStore(initialCount === undefined ? undefined : { count: initialCount });
  }
  return <CounterStoreContext.Provider value={storeRef.current}>{children}</CounterStoreContext.Provider>;
}

export function useCounterStore<T>(selector: (store: CounterStore) => T): T {
  const ctx = useContext(CounterStoreContext);
  if (!ctx) throw new Error("useCounterStore must be used within CounterStoreProvider");
  return useStore(ctx, selector);
}
```

```tsx
// app/layout.tsx  (Server Component)
import { CounterStoreProvider } from "@/providers/counter-store-provider";

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  const initialCount = await getInitialCountForThisUser();   // server-derived value
  return (
    <html lang="en">
      <body>
        <CounterStoreProvider initialCount={initialCount}>{children}</CounterStoreProvider>
      </body>
    </html>
  );
}
```

```tsx
"use client";
import { useCounterStore } from "@/providers/counter-store-provider";

export function Counter() {
  const count = useCounterStore((s) => s.count);
  const increment = useCounterStore((s) => s.increment);
  return <button onClick={increment}>{count}</button>;
}
```

How it works:

- `createStore` makes a store **without** a React hook; `useStore(api, selector)` subscribes a component to it.
- The `useRef` check creates the store **once per provider instance** (per request on the server, once per tab in the browser), and survives re-renders.
- The server and client both start from the same `initialCount`, so hydration matches.
- Place the provider as deep as it is needed (see [Context](./01-context.md)). Server Components cannot read the store; they pass data into the provider through props.

Per-route stores are the same idea with the provider on the page or segment layout.

## Persisting to `localStorage` (and hydration)

```ts
import { create } from "zustand";
import { createJSONStorage, persist } from "zustand/middleware";

type PrefsState = { compact: boolean; setCompact: (v: boolean) => void };

export const usePrefs = create<PrefsState>()(
  persist(
    (set) => ({ compact: false, setCompact: (compact) => set({ compact }) }),
    {
      name: "prefs",                                  // localStorage key
      storage: createJSONStorage(() => localStorage),
      skipHydration: true,                            // do not read storage during the first render
      partialize: (s) => ({ compact: s.compact }),    // persist only data, not functions
      version: 1,                                     // bump with a migrate() when the shape changes
    },
  ),
);
```

The server cannot read `localStorage`, so reading it during the first client render produces different HTML from the server and a hydration mismatch. Rehydrate **after** mount:

```tsx
"use client";

import { useEffect } from "react";
import { usePrefs } from "@/stores/prefs";

export function PrefsHydrator() {
  useEffect(() => { void usePrefs.persist.rehydrate(); }, []);
  return null;
}
```

Render `<PrefsHydrator />` once (in a layout). Until it runs, components see the defaults. If you need the value on the server's first paint, use a **cookie** instead.

## Slices for larger stores

Keep one store per domain when possible. If you want one store, split it into slice creators:

```ts
import { create, type StateCreator } from "zustand";

type CartSlice = { items: string[]; addItem: (id: string) => void };
type UiSlice = { isOpen: boolean; toggle: () => void };

const createCartSlice: StateCreator<CartSlice & UiSlice, [], [], CartSlice> = (set) => ({
  items: [],
  addItem: (id) => set((s) => ({ items: [...s.items, id] })),
});

const createUiSlice: StateCreator<CartSlice & UiSlice, [], [], UiSlice> = (set) => ({
  isOpen: false,
  toggle: () => set((s) => ({ isOpen: !s.isOpen })),
});

export const useAppStore = create<CartSlice & UiSlice>()((...a) => ({
  ...createCartSlice(...a),
  ...createUiSlice(...a),
}));
```

## Useful features

| Feature | How |
|---|---|
| Read or write outside React | `useStore.getState()`, `useStore.setState({...})`, `useStore.subscribe(listener)` |
| Redux DevTools | `devtools` middleware from `zustand/middleware` |
| Mutating-style updates | `immer` middleware (needs the `immer` package) |
| Derived values | Compute in the selector or in a plain function; avoid storing derived state |
| Reset | Keep an `initialState` and add a `reset: () => set(initialState)` action |

## Testing

A module-level store keeps state between tests. Reset it in `beforeEach`:

```ts
beforeEach(() => {
  useCartUi.setState({ isOpen: false });
});
```

Per-request stores are easier to test: create a fresh store per test with the factory.

## Zustand and server data

Do not mirror Server Component data into a store with an effect:

```tsx
// Avoid: two sources of truth
useEffect(() => { setPosts(posts); }, [posts]);
```

Instead:

- Show the server data directly from props.
- Keep only **client-owned** state in the store (selection, drafts, open panels), storing **IDs** rather than copies of the objects where you can.
- For server data that needs browser-side revalidation, use [TanStack Query](./03-tanstack-query.md).
- After a mutation, revalidate on the server ([Mutations](../07-server-actions/03-mutations-and-optimistic-ui.md)) so props update.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| Hydration mismatch with `persist` | Store reads `localStorage` on first render | `skipHydration: true`, then `rehydrate()` in an effect |
| "Maximum update depth exceeded" / infinite loop | Selector returns a new object/array each time (v5) | `useShallow`, or select primitives |
| Component re-renders constantly | `useStore()` with no selector, or selecting large objects | Select only what you use |
| Store works in dev, shares state between users | Server-seeded global store | Per-request store with a provider |
| `Cannot read properties of undefined` from the hook | Hook used outside the provider (per-request pattern) | Wrap the tree in the provider |
| State resets on navigation | Provider placed in a page or template | Put the provider in a layout |
| State resets on refresh | Not persisted | Use `persist`, a cookie, or the URL |
| Using the store in a Server Component | Hooks are not available there | Pass data via props to a Client Component |
| Type errors with middleware | Missing the curried `create<T>()(...)` form | Add the extra `()` |

## Common mistakes

| Mistake | Fix |
|---|---|
| Putting server-fetched data in the store | Use props or a seeded query cache |
| `create()` store seeded from request data | Per-request store |
| `useStore()` without a selector | Always pass a selector |
| Selecting new objects without `useShallow` | `useShallow` or primitive selectors |
| Persisting functions or huge objects | `partialize` the data |
| Reading `localStorage` in the initial state | `skipHydration` + `rehydrate` |
| One giant store for everything | Several small stores or slices |
| Global store for state one component uses | `useState` |
| Forgetting `"use client"` on components that use the hook | Add the directive |

## Quick Summary

- Zustand stores are hooks; components subscribe to selected slices, so updates are cheap.
- Use `create<T>()(...)` in TypeScript, and always pass a selector; use `useShallow` for object or array selections.
- A global store is acceptable for client-only UI state that the server never writes or seeds; otherwise use a **per-request store** (`createStore` + provider + `useRef`).
- Persist with `persist`, but use `skipHydration` and rehydrate after mount to avoid hydration mismatches.
- Do not duplicate server data in a store; keep the store for client-owned state.

## Next

- [TanStack Query](./03-tanstack-query.md)
- [Context](./01-context.md)
- [Client Components](../03-components/01-client-components.md)
