# Reducer and Context Pattern

`useState` is great for a value and a setter. When one piece of state has **several named ways of changing** (add, remove, change quantity, clear) and rules about how those changes combine, scattering `setState` calls across components gets messy. A **reducer** centralizes those rules in one pure function. Pair it with context and you get shared, structured state with no library.

(Reducer basics and syntax: [useReducer](../03-hooks/06-useReducer.md). This note is about the *pattern* for sharing it.)

## The shape of the pattern

```text
            ┌──────────────── Provider ────────────────┐
 events ──► │ dispatch(action) ──► reducer(state, action) │ ──► new state
            └───────┬──────────────────────┬────────────┘
                    │                      │
         CartDispatchContext         CartStateContext
          (stable, never changes)     (changes with state)
                    │                      │
              useCartDispatch()          useCart()
```

Components **describe what happened** (`dispatch({ type: "added", … })`). The reducer decides **how state changes**.

## A complete example: shopping cart

### 1. State and actions as types

```ts
// features/cart/cart-reducer.ts
export type CartItem = { id: string; name: string; price: number; qty: number }
export type CartState = { items: CartItem[] }

export type CartAction =
  | { type: "added"; item: Omit<CartItem, "qty"> }
  | { type: "removed"; id: string }
  | { type: "quantity_changed"; id: string; qty: number }
  | { type: "cleared" }
```

Actions are a **discriminated union**: TypeScript narrows `action` inside each `case`, so payload fields are typed correctly.

Name actions for **what happened** (`added`, `removed`), not the setter you'd call (`SET_ITEMS`). One user event can then trigger several state changes in the reducer without components knowing.

### 2. The reducer: pure and total

```ts
export function cartReducer(state: CartState, action: CartAction): CartState {
  switch (action.type) {
    case "added": {
      const existing = state.items.find((i) => i.id === action.item.id)
      return {
        items: existing
          ? state.items.map((i) => (i.id === existing.id ? { ...i, qty: i.qty + 1 } : i))
          : [...state.items, { ...action.item, qty: 1 }],
      }
    }
    case "removed":
      return { items: state.items.filter((i) => i.id !== action.id) }

    case "quantity_changed":
      return {
        items: action.qty <= 0
          ? state.items.filter((i) => i.id !== action.id)
          : state.items.map((i) => (i.id === action.id ? { ...i, qty: action.qty } : i)),
      }

    case "cleared":
      return { items: [] }

    default: {
      const _exhaustive: never = action          // compile error if a case is missing
      return state
    }
  }
}
```

Rules for reducers:

- **Pure**: same inputs, same output. No `fetch`, no `Date.now()`, no `Math.random()`, no `localStorage`, no dispatching. React may call a reducer twice in development (StrictMode) to catch impurities.
- **Never mutate** `state`; return new objects/arrays. (If that's tedious, see [Immer](#immer-for-nested-updates).)
- **Return the same `state` reference** when nothing changes, so React can skip a re-render.
- The `never` check in `default` turns "I added an action and forgot to handle it" into a compile-time error.

### 3. Provider with split contexts

```tsx
// features/cart/cart-provider.tsx
const CartStateContext = createContext<CartState | null>(null)
const CartDispatchContext = createContext<React.Dispatch<CartAction> | null>(null)

export function CartProvider({ children }: { children: React.ReactNode }) {
  const [state, dispatch] = useReducer(cartReducer, undefined, loadInitialCart)

  useEffect(() => {
    try { localStorage.setItem("cart", JSON.stringify(state)) } catch { /* storage full/blocked */ }
  }, [state])

  return (
    <CartDispatchContext value={dispatch}>
      <CartStateContext value={state}>{children}</CartStateContext>
    </CartDispatchContext>
  )
}

function loadInitialCart(): CartState {
  try {
    const raw = localStorage.getItem("cart")
    return raw ? (JSON.parse(raw) as CartState) : { items: [] }
  } catch {
    return { items: [] }
  }
}
```

- **`dispatch` is stable**: React guarantees its identity never changes, so it can go in a context without `useMemo`, and consumers of it never re-render because of state changes. (This is the "split state and actions" idea from [01](./01-context-patterns-and-performance.md#state-and-dispatchactions).)
- The third argument to `useReducer` is a **lazy initializer**, run once, so you don't parse `localStorage` on every render.
- Persistence is a **side effect**, so it lives in an effect, never in the reducer.
- Validate persisted data in real apps (a schema, a version key). Old or hand-edited storage will otherwise crash your app on load.

### 4. Hooks that hide the plumbing

```tsx
export function useCart() {
  const state = useContext(CartStateContext)
  if (!state) throw new Error("useCart must be used within <CartProvider>")
  return state
}

export function useCartDispatch() {
  const dispatch = useContext(CartDispatchContext)
  if (!dispatch) throw new Error("useCartDispatch must be used within <CartProvider>")
  return dispatch
}
```

### 5. Use it

```tsx
function AddToCartButton({ product }: { product: Product }) {
  const dispatch = useCartDispatch()                 // does NOT re-render when the cart changes
  return <Button onClick={() => dispatch({ type: "added", item: product })}>Add to cart</Button>
}

function CartBadge() {
  const { items } = useCart()
  return <span>{items.reduce((n, i) => n + i.qty, 0)}</span>
}
```

Wrap only the part of the tree that needs the cart:

```tsx
<CartProvider>
  <Shop />
</CartProvider>
```

## Derived values: selectors as plain functions

Don't store totals in state. Compute them, and keep the functions next to the reducer so they're testable and reusable:

```ts
export const selectCount = (s: CartState) => s.items.reduce((n, i) => n + i.qty, 0)
export const selectTotal = (s: CartState) => s.items.reduce((sum, i) => sum + i.price * i.qty, 0)
```

```tsx
const total = selectTotal(useCart())
```

If a computation is expensive, wrap it in `useMemo` at the call site. (Context has no built-in selector subscription, so a consumer re-renders on *any* state change regardless of which selector it uses. See the limits below.)

## Testing is the payoff

A reducer is a pure function, so tests need no React, no providers, and no mocks:

```ts
import { cartReducer } from "./cart-reducer"

test("adding an existing item increments quantity", () => {
  const state = { items: [{ id: "1", name: "Pen", price: 2, qty: 1 }] }
  const next = cartReducer(state, { type: "added", item: { id: "1", name: "Pen", price: 2 } })
  expect(next.items[0].qty).toBe(2)
})

test("quantity of 0 removes the item", () => {
  const state = { items: [{ id: "1", name: "Pen", price: 2, qty: 1 }] }
  expect(cartReducer(state, { type: "quantity_changed", id: "1", qty: 0 }).items).toEqual([])
})
```

This is the strongest reason to pick a reducer over scattered `setState`: **business rules become unit tests.** See [testing fundamentals](../18-testing-and-debugging/00-testing-fundamentals.md).

## Async work

Reducers are synchronous and pure, so async belongs **outside** them. Do the work in a handler (or a mutation) and dispatch the **results**:

```tsx
async function checkout() {
  dispatch({ type: "checkout_started" })
  try {
    const order = await ordersApi.create(items)
    dispatch({ type: "checkout_succeeded", orderId: order.id })
  } catch (error) {
    dispatch({ type: "checkout_failed", message: getErrorMessage(error) })
  }
}
```

For anything involving the server, prefer a [mutation](../12-server-state/05-mutations.md) and keep the reducer for *client* state (the cart contents), not the request's lifecycle.

## Immer for nested updates

Deeply nested immutable updates get verbose. [Immer](https://immerjs.github.io/immer/) lets you write "mutating" code that produces immutable results:

```bash
npm install immer use-immer
```

```ts
import { useImmerReducer } from "use-immer"

function reducer(draft: CartState, action: CartAction) {
  switch (action.type) {
    case "added": {
      const existing = draft.items.find((i) => i.id === action.item.id)
      if (existing) existing.qty++
      else draft.items.push({ ...action.item, qty: 1 })
      return
    }
  }
}

const [state, dispatch] = useImmerReducer(reducer, initialState)
```

With Immer you either mutate the draft **or** return a new value, not both. It's optional: for flat state like the cart, plain spreads are fine.

## When it's a good fit

- A **cohesive feature state** with multiple actions: cart, wizard, editor/canvas state, undo/redo, a complex filter panel, a game board.
- State scoped to **one part of the tree** (provide it around that feature only).
- You want logic **testable without React**.

Also a good option: use `useReducer` **without context** for complex *local* state in one component.

## Limits (and when to move on)

- **No selective subscription.** Every consumer of `CartStateContext` re-renders on every state change. Splitting dispatch from state helps *action-only* components, but not components that read slices of state.
- **No middleware or devtools** beyond React DevTools: no action log, no time travel.
- **Only exists inside the tree**, so non-React code (an API interceptor) can't read it.
- **High-frequency updates** (drag, typing, live data) will re-render consumers constantly.
- **Cross-feature coordination** gets awkward with many providers nested at the root.

If you hit these, the reducer you wrote ports almost directly to Zustand ([03](./03-zustand.md)) or a Redux slice ([04](./04-redux-toolkit.md)). Pure reducers with typed actions make that migration mechanical, which is another reason to write them this way.

## Common mistakes

- **Mutating state** in a reducer (`state.items.push(...)`). It breaks change detection and StrictMode checks.
- **Side effects in the reducer** (API calls, storage, random IDs, `Date.now()`).
- **Generating IDs inside the reducer.** Create them in the handler and pass them in the action.
- **Action names that mirror setters** (`SET_QTY`) rather than events (`quantity_changed`).
- **Forgetting the exhaustive `never` check**, so unhandled actions silently do nothing.
- **A single context for state and dispatch**, so button-only components re-render needlessly.
- **Storing derived data** (totals, counts) in state.
- **Providing the reducer at the app root** when only one feature needs it.
- **Unvalidated persisted state** crashing the app on load.
- **Putting server data in the reducer** instead of the query cache.

## Quick summary

- Reducer = pure `(state, action) => newState`. Context = delivery. Together: structured shared state with no dependency.
- Type actions as a discriminated union, name them after events, and add an exhaustive `never` check.
- **Split state and dispatch contexts**; `dispatch` is stable, so action-only components stay quiet.
- Wrap in a provider + hooks that throw outside the provider; derive values with plain selector functions.
- Reducers are trivially unit-testable; async and side effects stay outside.
- Move to a store when you need selective updates, devtools, middleware, or access outside React.

## Next

[03 — Zustand](./03-zustand.md)