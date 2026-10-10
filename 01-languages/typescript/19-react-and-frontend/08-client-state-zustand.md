# Client State with Zustand

**Client state** is state that exists only in the browser: which panel is open, a shopping cart before checkout, theme preference, a multi-step wizard's progress. When several distant components need it, passing props gets tedious and React context re-renders too much. Zustand is a small store library with a hook-based API, selectors for fine-grained subscriptions, and good TypeScript support. This note covers typing a store, selecting state without extra re-renders, middleware, and when a store is the wrong tool.

> **Version note.** The examples assume Zustand v4 or v5. Both use the curried `create<State>()(...)` form in TypeScript. v5 changed how selectors that return new objects behave (see [selectors](#selectors-and-re-renders)), and some import paths differ. Check your installed version's documentation.

**Prerequisites:**
- [Hooks](./02-hooks.md)
- [Context](./03-context.md) (the alternative this note compares against)
- [Server state with TanStack Query](./07-server-state-tanstack-query.md)
- [Generic types](../06-generics/01-generic-types.md)

---

## Server state vs client state

Before reaching for a store, sort your state:

| Kind | Examples | Tool |
|---|---|---|
| **Server state** | users, products, search results | a query library ([TanStack Query](./07-server-state-tanstack-query.md)) |
| **Local UI state** | an input draft, whether a dropdown is open | `useState` in the component |
| **Shared client state** | cart, theme, sidebar, wizard step, auth session | a store (Zustand, context, or similar) |
| **URL state** | filters, pagination, selected tab | the router and query string |

A large share of what people put in global stores is really server state or URL state. Move those out, and the store shrinks to a few genuinely client-side concerns.

## A typed store

```ts
import { create } from "zustand";

interface CartItem { sku: string; name: string; priceCents: number; qty: number }

interface CartState {
  items: CartItem[];
  add: (item: CartItem) => void;
  remove: (sku: string) => void;
  clear: () => void;
}

export const useCart = create<CartState>()((set) => ({
  items: [],
  add: (item) =>
    set((state) => ({ items: [...state.items, item] })),
  remove: (sku) =>
    set((state) => ({ items: state.items.filter((i) => i.sku !== sku) })),
  clear: () => set({ items: [] }),
}));
```

Key points:

- Define **one interface** containing both the data and the actions. `create<CartState>()` uses it for `set`, `get`, and the hook's return type.
- The **double call** `create<CartState>()(...)` is the TypeScript form. It lets you provide the state type explicitly while still inferring the middleware types. Calling `create<CartState>(...)` directly without the extra `()` works in the simplest case but breaks once middleware is involved.
- `set` takes either a partial state object (shallow merged into the state) or a function from the current state to a partial state.
- Always produce **new** objects and arrays. Mutating `state.items.push(...)` would not trigger updates.

Use it in components with a **selector**:

```tsx
function CartCount() {
  const count = useCart((state) => state.items.length);     // number
  return <span>{count}</span>;
}

function AddButton({ item }: { item: CartItem }) {
  const add = useCart((state) => state.add);                // (item: CartItem) => void
  return <button onClick={() => add(item)}>Add to cart</button>;
}
```

The selector's return type is the hook's return type, so everything is inferred.

## Selectors and re-renders

A component re-renders only when its **selected value changes** (compared with `Object.is` by default). So select the smallest piece you need:

```tsx
const count = useCart((s) => s.items.length);   // re-renders only when the count changes
const all = useCart();                          // subscribes to the whole store: re-renders on any change
```

Avoid calling the hook with no selector in components, as it makes them re-render on every update.

### Selecting several values

Selecting an **object or array built in the selector** creates a new reference every time, so Zustand sees a change on every update:

```tsx
// new object each time: re-renders on every store update
const { items, clear } = useCart((s) => ({ items: s.items, clear: s.clear }));
```

In v5 this can even cause an infinite render loop ("Maximum update depth exceeded"). Two good options:

```tsx
// 1. separate selectors (simple, and each is cheap)
const items = useCart((s) => s.items);
const clear = useCart((s) => s.clear);

// 2. useShallow: compares the selected object's fields shallowly
import { useShallow } from "zustand/react/shallow";

const { items, clear } = useCart(useShallow((s) => ({ items: s.items, clear: s.clear })));
```

Actions (`add`, `clear`) are stable references, so selecting them never causes re-renders.

### Derived state

Compute derived values in the selector or with a helper rather than storing them:

```tsx
const totalCents = useCart((s) => s.items.reduce((sum, i) => sum + i.priceCents * i.qty, 0));
```

Storing derived data creates a second source of truth that can drift. If the computation is heavy, memoize in the component, or select the inputs and compute with `useMemo`.

## Using the store outside React

A store is a plain object with methods, so non-React code can use it:

```ts
const items = useCart.getState().items;              // read the current state
useCart.getState().clear();                          // call an action
const unsubscribe = useCart.subscribe((state) => {   // listen to changes
  localStorage.setItem("cart", JSON.stringify(state.items));
});
```

This is handy in event handlers, services, and tests ([testing](#testing)). Avoid reading state with `getState()` inside render, where the hook with a selector keeps components in sync.

## Middleware

Middleware wraps the store creator. In TypeScript, use the curried `create<T>()(...)` form so the types compose.

### `devtools`

```ts
import { devtools } from "zustand/middleware";

export const useCart = create<CartState>()(
  devtools((set) => ({
    items: [],
    add: (item) => set((s) => ({ items: [...s.items, item] }), false, "cart/add"),   // action name for DevTools
    // ...
  })),
);
```

This connects the store to the Redux DevTools browser extension, which shows state history and action names.

### `persist`

Save state to `localStorage` (or other storage) and restore it on load:

```ts
import { persist } from "zustand/middleware";

interface SettingsState {
  theme: "light" | "dark";
  fontSize: number;
  setTheme: (t: SettingsState["theme"]) => void;
}

export const useSettings = create<SettingsState>()(
  persist(
    (set) => ({
      theme: "light",
      fontSize: 14,
      setTheme: (theme) => set({ theme }),
    }),
    {
      name: "settings",                                        // storage key
      partialize: (s) => ({ theme: s.theme, fontSize: s.fontSize }),   // persist data only, not functions
      version: 1,
      migrate: (persisted, version) => { /* upgrade old shapes */ return persisted as SettingsState; },
    },
  ),
);
```

Points on persistence:

- **`partialize`** chooses which fields are saved, so actions and transient state are excluded.
- **Stored data is untrusted:** it may come from an older app version, be corrupted, or be edited by the user. `migrate` receives `unknown`, so validate it with a schema instead of casting ([validation recipes](../15-runtime-validation/04-validation-recipes.md)).
- **Bump `version`** when the stored shape changes, and write a `migrate` function.
- **Server rendering:** the server has no `localStorage`, so the first client render may differ from the server's. Frameworks need care here (hydration), see [Next.js](./09-nextjs.md).

### Combining and `immer`

Middleware nest, with the order mattering for types and behavior (typically `devtools(persist(...))`). The `immer` middleware lets you write "mutating" updates inside `set` that produce immutable results, which is convenient for deeply nested state:

```ts
import { immer } from "zustand/middleware/immer";

create<State>()(immer((set) => ({
  todos: [],
  toggle: (id) => set((s) => { const t = s.todos.find((t) => t.id === id); if (t) t.done = !t.done; }),
})));
```

It adds a dependency, so use it when nesting makes spread updates painful.

## Splitting a large store into slices

For large stores, define each piece as a **slice** and combine them. The typing uses `StateCreator`:

```ts
import { create, type StateCreator } from "zustand";

interface AuthSlice { user: User | null; login: (u: User) => void; logout: () => void }
interface UiSlice { sidebarOpen: boolean; toggleSidebar: () => void }

type AppState = AuthSlice & UiSlice;

const createAuthSlice: StateCreator<AppState, [], [], AuthSlice> = (set) => ({
  user: null,
  login: (user) => set({ user }),
  logout: () => set({ user: null }),
});

const createUiSlice: StateCreator<AppState, [], [], UiSlice> = (set) => ({
  sidebarOpen: true,
  toggleSidebar: () => set((s) => ({ sidebarOpen: !s.sidebarOpen })),
});

export const useAppStore = create<AppState>()((...a) => ({
  ...createAuthSlice(...a),
  ...createUiSlice(...a),
}));
```

`StateCreator<FullState, Middlewares-in, Middlewares-out, SliceState>` lets each slice see the whole state while returning only its own part. The generics become more intricate when middleware are combined. If that is too heavy, **several small stores** (one per concern) are often simpler than one sliced store.

## Stores and server rendering

A module-level store is a **singleton**, shared by all requests on a server. For apps that render per request (Next.js and other SSR frameworks), a global store can leak state between users. The documented approach is to create the store **per request or per provider** and supply it through context:

```tsx
const StoreContext = createContext<StoreApi<CartState> | null>(null);
```

Client-only stores that are only used in client components are fine as singletons. See [Next.js](./09-nextjs.md) and [context](./03-context.md).

## Zustand vs context vs alternatives

| | Context + `useState`/`useReducer` | Zustand |
|---|---|---|
| Setup | built in | small dependency |
| Selective subscription | none: every consumer re-renders when the value changes | built in via selectors |
| Use outside React | no | yes (`getState`, `subscribe`) |
| Middleware (persist, devtools) | write your own | provided |
| Best for | rarely changing values, DI, scoped state | frequently changing shared state |

Other options: **Redux Toolkit** (structured, larger ecosystem, more ceremony), **Jotai** or **Recoil-style atoms** (fine-grained derived state), **Valtio** (proxy-based). The concepts of typed state, actions, and selectors carry across them. For rarely changing, widely needed values, plain [context](./03-context.md) is enough.

## Testing

Because the store is plain JavaScript, test it without rendering:

```ts
import { beforeEach, it, expect } from "vitest";

const initial = useCart.getState();

beforeEach(() => {
  useCart.setState(initial, true);       // replace state to reset between tests
});

it("adds an item", () => {
  useCart.getState().add({ sku: "A", name: "Thing", priceCents: 500, qty: 1 });
  expect(useCart.getState().items).toHaveLength(1);
});
```

Reset the store between tests, or state leaks across them ([unit testing](../18-testing-and-debugging/00-unit-testing.md)).

## Important rules and misconceptions

- **Immutable updates only.** `set` merges at the top level only, so nested objects must be replaced, not mutated.
- **`set` merges shallowly.** `set({ items: [] })` leaves other fields alone, while `set(newState, true)` replaces the whole state.
- **A store is a global.** Anything you put there lives as long as the module unless you reset it.
- **Selecting is subscribing.** A component subscribes to exactly what its selector returns.
- **Persisted state must be treated as untrusted input.**
- **Do not mirror server data in a store.** Let the query cache own it.

## Common mistakes

- Calling `useStore()` with no selector and re-rendering on every change.
- Returning a new object or array from a selector without `useShallow` or separate selectors.
- Mutating state directly (`state.items.push(x)`).
- Calling `create<State>(...)` without the extra `()` and then fighting middleware types.
- Storing server data, derived data, or form drafts in the store.
- Persisting everything, including functions and transient flags, with no `partialize`.
- Casting persisted data in `migrate` instead of validating it.
- Using a module-level store for per-request state in server rendering.
- One giant store for the whole app with no clear boundaries.
- Forgetting to reset store state between tests.

## Debugging

- Use the Redux DevTools extension (with the `devtools` middleware) to see every state change and its action name.
- If a component re-renders too often, check its selector: is it returning a new reference, or the whole store?
- If you see "Maximum update depth exceeded" after upgrading, look for selectors that return new objects, and use `useShallow`.
- If persisted state looks wrong, inspect the storage entry in the browser's Application panel, and check `version` and `migrate`.
- If types fail with middleware, confirm the curried `create<T>()(...)` form and the middleware order.
- Log with `useStore.subscribe(console.log)` to watch all changes.

## Quick summary

- Use a store for genuinely **shared client state**. Keep server data in a query cache, URL state in the router, and drafts in local state.
- Type the store with one interface holding data and actions, and create it with `create<State>()((set) => ...)`.
- Subscribe with **selectors**, select the smallest value you need, and use `useShallow` or separate selectors when selecting several values.
- Middleware: `devtools` for inspection, `persist` for storage (with `partialize`, `version`, `migrate`, and validated restores), `immer` for nested updates.
- Slices (`StateCreator`) or several small stores keep large state manageable. In server rendering, avoid shared singleton stores.
- Context suits rarely changing values. Stores suit frequently changing shared state.

**Next:** [Next.js](./09-nextjs.md)
