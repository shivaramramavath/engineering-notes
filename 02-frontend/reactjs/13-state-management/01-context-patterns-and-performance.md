# Context Patterns and Performance

Context is the most misunderstood tool in React state management. It is **not a state management library**. It's a way to **deliver a value to any descendant without passing props**. Whatever state you put in it still comes from `useState`/`useReducer`. Knowing exactly how it re-renders explains when it's perfect and when it quietly slows your app.

(Basic `createContext`/`useContext` syntax: [useContext](../03-hooks/05-useContext.md).)

## How it works

```tsx
const ThemeContext = createContext<"light" | "dark">("light")

function App() {
  const [theme, setTheme] = useState<"light" | "dark">("light")
  return (
    <ThemeContext value={theme}>        {/* React 19: no .Provider needed */}
      <Page />
    </ThemeContext>
  )
}

function Button() {
  const theme = useContext(ThemeContext)   // or use(ThemeContext)
  return <button className={theme}>…</button>
}
```

On React 18, write `<ThemeContext.Provider value={theme}>`. In React 19, `<ThemeContext value>` works directly, and `use(ThemeContext)` reads context (and, unlike `useContext`, can be called conditionally).

The default value passed to `createContext` is used **only when there's no provider above**. It's not the initial state.

## The performance rule

> When a provider's `value` changes (compared with `Object.is`), **every component that reads that context re-renders**, even if it only uses part of the value, and even if it's wrapped in `React.memo`.

```tsx
const AppContext = createContext<{ user: User; theme: Theme; cartCount: number } | null>(null)

function Provider({ children }: { children: React.ReactNode }) {
  const [user, setUser] = useState(…)
  const [theme, setTheme] = useState(…)
  const [cartCount, setCartCount] = useState(0)

  return <AppContext value={{ user, theme, cartCount }}>{children}</AppContext>
}
```

Two problems here:

1. `{ user, theme, cartCount }` is a **new object every render**, so the value "changes" on every provider render, even when nothing in it did.
2. Even with a stable object, **any** field changing re-renders **every** consumer. A component that only reads `theme` re-renders when `cartCount` changes.

There is no built-in selector for context. You fix this with structure, not a hook option.

## Fix 1: stabilize the value

```tsx
const value = useMemo(() => ({ user, theme, cartCount }), [user, theme, cartCount])
return <AppContext value={value}>{children}</AppContext>
```

Now the value only changes when one of its parts does. For functions inside, use `useCallback` (or define them with stable identity) so they don't break the memo ([useMemo and useCallback](../03-hooks/07-useMemo-and-useCallback.md)). The [React Compiler](../15-concurrent-and-modern-react/06-react-compiler.md) can add this memoization automatically; without it, it's your job.

## Fix 2: split contexts

Separate things that change at different rates or are used by different components:

```tsx
const ThemeContext = createContext<Theme>("light")
const UserContext = createContext<User | null>(null)
const CartContext = createContext<CartState | null>(null)
```

A cart change now re-renders only cart consumers. Split by **domain** and by **rate of change**.

### State and dispatch/actions

A very effective split: the **state** changes; the **functions that change it** don't.

```tsx
const CartStateContext = createContext<CartState | null>(null)
const CartActionsContext = createContext<CartActions | null>(null)

function CartProvider({ children }: { children: React.ReactNode }) {
  const [items, setItems] = useState<CartItem[]>([])

  const actions = useMemo<CartActions>(() => ({
    add: (item) => setItems((prev) => [...prev, item]),
    remove: (id) => setItems((prev) => prev.filter((i) => i.id !== id)),
  }), [])                                  // stable forever: setState functional updates need no deps

  return (
    <CartActionsContext value={actions}>
      <CartStateContext value={items}>{children}</CartStateContext>
    </CartActionsContext>
  )
}
```

An "Add to cart" button that only reads `CartActionsContext` **never re-renders** when the cart changes. This is the same pattern as `useReducer`'s stable `dispatch` ([02](./02-reducer-and-context-pattern.md)).

## Fix 3: the `children` trick

When a provider holds state, components passed as `children` are created by the *parent*, so they're the same element references on every provider re-render, and React skips them (unless they consume the context):

```tsx
function CounterProvider({ children }: { children: React.ReactNode }) {
  const [count, setCount] = useState(0)
  return <CountContext value={count}>{children}</CountContext>
}

<CounterProvider>
  <ExpensiveTree />        {/* doesn't re-render when count changes, unless it reads the context */}
</CounterProvider>
```

Always write providers as components that take `children`, with state **inside** the provider. Putting `useState` in `App` and wrapping the tree inline re-renders the whole tree on every state change.

## A well-formed provider module

Encapsulate the context, so consumers can't misuse it:

```tsx
// features/cart/cart-context.tsx
const CartContext = createContext<CartValue | null>(null)   // null = "no provider"

export function CartProvider({ children }: { children: React.ReactNode }) {
  const [items, setItems] = useState<CartItem[]>([])
  const value = useMemo(
    () => ({ items, add: (i: CartItem) => setItems((p) => [...p, i]) }),
    [items]
  )
  return <CartContext value={value}>{children}</CartContext>
}

export function useCart() {
  const ctx = useContext(CartContext)
  if (!ctx) throw new Error("useCart must be used within <CartProvider>")
  return ctx
}
```

- Export the **provider and the hook**, not the raw context object.
- `null` default + the throwing hook gives a clear error instead of `undefined is not iterable` three files away, and narrows the type for TypeScript.
- Provide **scoped** contexts close to where they're used (a feature's provider around the feature), not everything at the root.

## When context is the right tool

| Good fit | Why |
|---|---|
| Theme, locale/i18n, direction | Rarely changes, read everywhere |
| Current user / auth `status` | Changes at login/logout only |
| Feature flags, config | Basically static |
| A compound component's internal state ([Tabs, Dialog](../05-component-design/02-compound-components.md)) | Scoped to one component tree |
| Dependency injection (a service, a QueryClient) | Stable value, no re-render concern |

## When it's the wrong tool

- **High-frequency updates**: pointer position, scroll offset, drag state, animation values, live cursors. Every tick re-renders every consumer.
- **Large state read in slices by many components.** No selectors means no escape from over-rendering. Use a store with selectors ([Zustand](./03-zustand.md)).
- **Server data.** Use a [query cache](../12-server-state/README.md).
- **State needed outside React** (an API client, event listeners). Context only exists inside the tree.

If you notice yourself memoizing contexts, splitting them into five, and wrapping consumers in `memo` to fight re-renders, you've outgrown context for that state.

## Common misconceptions

- **"Context avoids re-renders."** No. It avoids *prop drilling*. A change re-renders all consumers.
- **"`React.memo` stops context re-renders."** It doesn't; a context update bypasses `memo` for components reading that context.
- **"Putting state in context makes it global state management."** It's just state with delivery. Every limitation of `useState` (and none of a store's features) still applies.
- **"I can select part of the value."** Not natively. `useContext(Ctx).theme` still subscribes to the *entire* value.
- **"Context is slow."** Context itself is cheap. The cost is *how many* components re-render *how often*, which you control by structure.

## Measuring

Don't guess: profile. In React DevTools Profiler, record an interaction and check which components re-rendered and "why did this render?" (context change). Fix only what's measurably slow ([profiling and measuring](../14-performance/00-profiling-and-measuring.md), [memoization](../14-performance/02-memoization.md)). Many apps with a theme and an auth context never need any of these optimizations.

## Common mistakes

- **Inline object as the provider value**, re-rendering all consumers on every provider render.
- **One giant context** for unrelated state.
- **State in a parent without the `children` pattern**, re-rendering the whole subtree.
- **Using context for rapidly changing values.**
- **Exporting the raw context** and calling `useContext` with no provider check.
- **Meaningful default values** that mask a missing provider.
- **Assuming `memo` blocks context updates.**
- **Putting every provider at the root** instead of scoping them to the features that need them.
- **Unstable action functions** in the value (recreated every render) defeating `useMemo`.

## Quick summary

- Context delivers values; state still comes from `useState`/`useReducer`.
- **Any change to the value re-renders every consumer**, with no selectors and no `memo` shield.
- Stabilize the value with `useMemo`, **split contexts** by domain and by state vs actions, and use the `children` pattern for providers.
- Wrap context in a provider component + a hook that throws when missing.
- Ideal for rarely-changing, widely-read values; wrong for high-frequency or selectively-read state.
- Profile before optimizing.

## Next

[02 — Reducer and context pattern](./02-reducer-and-context-pattern.md)