# Redux Toolkit

Redux is a pattern: **one store, state changed only by dispatching plain-object actions, handled by pure reducers.** That gives you a predictable, inspectable event log of everything that happened in the app. **Redux Toolkit (RTK)** is the official, opinionated way to write it today. It removes the boilerplate that made classic Redux famous for being tedious.

> If you've seen Redux code with hand-written action types, `switch` reducers, and `connect()`, that's the legacy style. Modern Redux is RTK with hooks, and it looks nothing like that.

```bash
npm install @reduxjs/toolkit react-redux
```

This note covers **Redux Toolkit 2** with **React-Redux 9**.

## Should you use it?

Be honest about the trade-off. Compared with [Zustand](./03-zustand.md), Redux asks for more structure up front.

| Redux Toolkit earns its place when… | Probably overkill when… |
|---|---|
| Large app / large team that benefits from enforced conventions | Small or medium app |
| Many features reacting to the **same events** (cross-cutting logic) | State is simple toggles and lists |
| You want a **replayable action log** and time-travel debugging | You mainly need a cart, a sidebar, and a theme |
| Complex async/side-effect orchestration (listener middleware) | Most async work is server data (use a [query cache](../12-server-state/README.md)) |
| You're joining or maintaining an existing Redux codebase | Greenfield with a small team |

Most apps don't *need* Redux. Plenty of teams still choose it for the consistency and tooling, and that's a valid reason.

## The flow

```text
 UI event ──► dispatch(action) ──► reducer (via slice) ──► new state ──► store
    ▲                                                                       │
    └─────────────── useSelector re-renders subscribed components ◄─────────┘
```

Three ideas: **state is read-only**, changed only by **actions** (descriptions of what happened), processed by **pure reducers**.

## Slices

A **slice** bundles the reducer logic and actions for one feature:

```ts
// features/cart/cart-slice.ts
import { createSlice, type PayloadAction } from "@reduxjs/toolkit"

export type CartItem = { id: string; name: string; price: number; qty: number }
type CartState = { items: CartItem[] }

const initialState: CartState = { items: [] }

const cartSlice = createSlice({
  name: "cart",
  initialState,
  reducers: {
    itemAdded(state, action: PayloadAction<Omit<CartItem, "qty">>) {
      const existing = state.items.find((i) => i.id === action.payload.id)
      if (existing) existing.qty += 1                 // looks like mutation…
      else state.items.push({ ...action.payload, qty: 1 })
    },
    itemRemoved(state, action: PayloadAction<string>) {
      state.items = state.items.filter((i) => i.id !== action.payload)
    },
    quantityChanged(state, action: PayloadAction<{ id: string; qty: number }>) {
      const item = state.items.find((i) => i.id === action.payload.id)
      if (item) item.qty = action.payload.qty
    },
    cleared: () => initialState,
  },
})

export const { itemAdded, itemRemoved, quantityChanged, cleared } = cartSlice.actions
export default cartSlice.reducer
```

What `createSlice` does for you:

- **Generates action creators**: `itemAdded(payload)` returns `{ type: "cart/itemAdded", payload }`.
- **Uses Immer inside reducers**: the "mutating" code above is safe because it operates on a draft that Immer turns into an immutable update. **This applies only inside `createSlice`/`createReducer`**, not in ordinary functions.
- **Types the payload** with `PayloadAction<T>`.

Immer rule: in a case reducer, either **mutate the draft** *or* **return a new value**, never both. Returning `initialState` (as `cleared` does) is fine when you haven't mutated.

Name actions as **events** (`itemAdded`), like in [02](./02-reducer-and-context-pattern.md): many slices can respond to one event.

## The store

```ts
// app/store.ts
import { configureStore } from "@reduxjs/toolkit"
import cartReducer from "@/features/cart/cart-slice"
import uiReducer from "@/features/ui/ui-slice"

export const store = configureStore({
  reducer: {
    cart: cartReducer,
    ui: uiReducer,
  },
})

export type RootState = ReturnType<typeof store.getState>
export type AppDispatch = typeof store.dispatch
```

`configureStore` sets up the Redux DevTools connection, thunk support, and **development-only checks** that warn when you mutate state outside a reducer or put non-serializable values (functions, class instances, promises) into state or actions.

## Provider and typed hooks

```tsx
// main.tsx
import { Provider } from "react-redux"

createRoot(document.getElementById("root")!).render(
  <Provider store={store}>
    <App />
  </Provider>
)
```

```ts
// app/hooks.ts
import { useDispatch, useSelector } from "react-redux"
import type { AppDispatch, RootState } from "./store"

export const useAppDispatch = useDispatch.withTypes<AppDispatch>()
export const useAppSelector = useSelector.withTypes<RootState>()
```

Use these **pre-typed hooks** everywhere instead of the raw ones, so selectors know your `RootState` and dispatch knows about thunks.

```tsx
function AddToCartButton({ product }: { product: Product }) {
  const dispatch = useAppDispatch()
  return <Button onClick={() => dispatch(itemAdded(product))}>Add to cart</Button>
}

function CartBadge() {
  const count = useAppSelector((state) => state.cart.items.reduce((n, i) => n + i.qty, 0))
  return <span>{count}</span>
}
```

## Selectors

`useAppSelector` re-renders the component when the selected value changes by **reference** (`===`). Same selective-subscription idea as Zustand:

- **Select the smallest thing you need.** `state.cart.items.length` beats selecting the whole `cart`.
- **Don't return new objects/arrays from a selector** (`state.items.filter(...)`) without memoizing. It's a new reference every call, so the component re-renders on every store update. React-Redux 9 warns about this in development.

Memoize derived data with **`createSelector`** (reselect, re-exported by RTK):

```ts
import { createSelector } from "@reduxjs/toolkit"

export const selectCartItems = (state: RootState) => state.cart.items

export const selectCartTotal = createSelector([selectCartItems], (items) =>
  items.reduce((sum, i) => sum + i.price * i.qty, 0)
)
```

The result is recalculated only when `items` changes, and returns the same reference otherwise. Slices can also define their own `selectors` field, which keeps them next to the state shape. Define selectors **with the slice** so components don't depend on where state lives in the tree.

## Async logic

Reducers are pure, so async code lives elsewhere. The classic tool is a **thunk** (a function dispatched like an action):

```ts
import { createAsyncThunk } from "@reduxjs/toolkit"

export const fetchTodos = createAsyncThunk("todos/fetch", async (_, { signal }) => {
  return todosApi.list(signal)
})

// in the slice
extraReducers: (builder) => {
  builder
    .addCase(fetchTodos.pending, (state) => { state.status = "loading" })
    .addCase(fetchTodos.fulfilled, (state, action) => { state.status = "idle"; state.items = action.payload })
    .addCase(fetchTodos.rejected, (state) => { state.status = "failed" })
},
```

(`extraReducers` uses the **builder callback** in RTK 2. The old object syntax was removed.)

But for **server data**, don't hand-write this pending/fulfilled/rejected ceremony. You'd be rebuilding a cache. Use one of:

- **TanStack Query** alongside Redux, with Redux holding only client state ([12](../12-server-state/README.md)). This is what many teams do.
- **RTK Query**, the data-fetching layer included in Redux Toolkit: it defines endpoints, caches by arguments, and generates hooks, with a similar model to TanStack Query. A good choice when your app is already Redux-centric and you want one tool.

Reserve `createAsyncThunk` for non-cache workflows (multi-step client orchestration) and use **listener middleware** (`createListenerMiddleware`) to run side effects in response to dispatched actions (such as "when `loggedOut` fires, clear persisted data").

## Normalized state

For collections keyed by ID, `createEntityAdapter` stores `{ ids: [], entities: {} }` and gives you ready-made reducers and selectors (`addOne`, `upsertMany`, `selectById`, `selectAll`):

```ts
const todosAdapter = createEntityAdapter<Todo>()
const slice = createSlice({
  name: "todos",
  initialState: todosAdapter.getInitialState(),
  reducers: { todoAdded: todosAdapter.addOne, todoUpdated: todosAdapter.updateOne },
})
```

Normalization avoids duplicated data and O(n) lookups, but it's another concept to learn. Use it when many places reference the same entities.

## Structure

```text
src/
├── app/
│   ├── store.ts
│   └── hooks.ts
└── features/
    ├── cart/
    │   ├── cart-slice.ts          # state + reducers + actions + selectors
    │   ├── cart-slice.test.ts
    │   └── CartPage.tsx
    └── ui/
        └── ui-slice.ts
```

The "ducks" convention (one file per feature slice) is the Redux team's recommendation. It matches a [feature-based architecture](../20-frontend-architecture/00-feature-based-architecture.md).

## Debugging and testing

- **Redux DevTools** (browser extension) is included with `configureStore`: every action, the state diff, time-travel, and action replay. This is a major reason to pick Redux.
- **Slice reducers are pure**, so test them directly:

```ts
import reducer, { itemAdded } from "./cart-slice"

test("adding the same item twice increments qty", () => {
  let state = reducer(undefined, itemAdded({ id: "1", name: "Pen", price: 2 }))
  state = reducer(state, itemAdded({ id: "1", name: "Pen", price: 2 }))
  expect(state.items).toEqual([{ id: "1", name: "Pen", price: 2, qty: 2 }])
})
```

- For component tests, create a **fresh store per test** (export a `setupStore(preloadedState)` factory) rather than sharing the singleton, which leaks state between tests.

## Redux Toolkit vs Zustand

| | Redux Toolkit | Zustand |
|---|---|---|
| Setup | Store, slices, provider, typed hooks | One `create` call |
| Structure | Enforced (actions, reducers, slices) | Your choice |
| DevTools | Built in, action log, time travel | Via `devtools` middleware |
| Side-effect tooling | Thunks, listeners, RTK Query | Plain async functions |
| Outside React | `store.getState()`/`dispatch` | `useStore.getState()` |
| Best for | Large teams, shared conventions, complex cross-cutting logic | Most apps, quick iteration |

Neither is wrong. The deciding factor is usually team scale and how much you value enforced structure and the event log.

## Common mistakes

- **Mutating state outside `createSlice`/`createReducer`.** Immer only wraps those.
- **Mutating *and* returning** a value in a case reducer.
- **Non-serializable values in state or actions** (functions, `Date`, class instances, `Promise`s). Store ISO strings or timestamps. This breaks DevTools and persistence.
- **Using raw `useSelector`/`useDispatch`** and losing types; use the pre-typed hooks.
- **Selectors that return fresh arrays/objects**, re-rendering on every store change. Use `createSelector`.
- **Selecting too much** (`useAppSelector((s) => s.cart)`) when you need one field.
- **Storing server data by hand** with thunks instead of using RTK Query/TanStack Query.
- **Putting everything in Redux**: form drafts, hover state, URL state.
- **Using the legacy object form of `extraReducers`** (removed in RTK 2).
- **Side effects in reducers** (API calls, random IDs, `Date.now()`). Generate IDs in the action via a `prepare` callback or before dispatching.
- **Sharing a single store between tests.**
- **Copy-pasting classic Redux tutorials** (hand-written action constants, `connect`, `switch` reducers).

## Quick summary

- Redux = single store, actions describe events, pure reducers compute the next state. **RTK is the modern, official way** to use it.
- `createSlice` generates actions and lets you write "mutating" code safely (Immer); `configureStore` wires DevTools, thunks, and dev checks.
- Use pre-typed hooks (`useAppDispatch`, `useAppSelector`), select minimal data, and memoize derived values with `createSelector`.
- Keep state serializable; keep reducers pure; handle async in thunks/listeners, and **server data in RTK Query or TanStack Query**.
- Pick Redux for structure, tooling, and large-team consistency, not by default.

## Next

Continue to [14 — Performance](../14-performance/README.md).