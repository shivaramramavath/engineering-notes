# Context

React context lets a value reach many components without passing props through every level. In the App Router it is useful, but it comes with two constraints: **context only works in Client Components**, and **every consumer re-renders when the value changes**.

> Verified against the Next.js 16.4 docs (Server and Client Components, SPA patterns).

## What it is, and when to use it

Context fits **app-wide values that change rarely**:

| Good fit | Poor fit |
|---|---|
| Theme, locale/direction | A counter that updates many times a second |
| Current user (a small object) | Large lists or server data you could pass as props |
| Feature flags | Form field values |
| A dialog/toast manager | State used by one or two nearby components (just lift state) |
| Dependency injection (a client, a config) | Anything where you need fine-grained subscriptions |

If a value changes often and many components read different pieces of it, context is the wrong tool; see [Zustand](./02-zustand.md).

## The App Router rules

1. **`createContext`, providers and `useContext` are Client Component features.** A Server Component cannot create or consume context.
2. A provider is a **Client Component that accepts `children`** and renders `<Context.Provider>`.
3. A Server Component (usually the layout) renders that provider. Everything beneath it, including Server Components passed as `children`, stays server-rendered; only Client Components below can read the context.

```tsx
// app/theme-provider.tsx
"use client";

import { createContext, useContext, useState } from "react";

type Theme = "light" | "dark";
type ThemeContextValue = { theme: Theme; setTheme: (t: Theme) => void };

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children, initialTheme = "light" }: { children: React.ReactNode; initialTheme?: Theme }) {
  const [theme, setTheme] = useState<Theme>(initialTheme);
  return <ThemeContext.Provider value={{ theme, setTheme }}>{children}</ThemeContext.Provider>;
}

export function useTheme() {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error("useTheme must be used within a ThemeProvider");
  return ctx;
}
```

```tsx
// app/layout.tsx  (a Server Component)
import { ThemeProvider } from "./theme-provider";

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>
        <ThemeProvider>{children}</ThemeProvider>
      </body>
    </html>
  );
}
```

```tsx
// any Client Component below
"use client";
import { useTheme } from "@/app/theme-provider";

export function ThemeButton() {
  const { theme, setTheme } = useTheme();
  return <button onClick={() => setTheme(theme === "dark" ? "light" : "dark")}>{theme}</button>;
}
```

### Render providers as deep as is practical

The docs advise wrapping only `{children}`, not the whole `<html>` document, and placing providers as low in the tree as they are needed. This lets Next.js optimize the static parts of your Server Components. If only the `/dashboard` area needs a provider, put it in `app/dashboard/layout.tsx`.

### Always export a typed hook that throws

A `useX()` hook that throws when the provider is missing turns a confusing `undefined` error into a clear message and gives you a non-null type. Do not export the raw context object.

### React 19 notes

In React 19 you can render the context itself as the provider (`<ThemeContext value={...}>`) and read it with `use(ThemeContext)`, which, unlike `useContext`, can be called conditionally. Older code with `.Provider` and `useContext` still works.

## Giving the provider initial data from the server

The layout can fetch something and pass it down as a prop. A small, serializable value is simple:

```tsx
// app/layout.tsx
import { cookies } from "next/headers";
import { ThemeProvider } from "./theme-provider";

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  const theme = (await cookies()).get("theme")?.value === "dark" ? "dark" : "light";
  return (
    <html lang="en">
      <body>
        <ThemeProvider initialTheme={theme}>{children}</ThemeProvider>
      </body>
    </html>
  );
}
```

Reading `cookies()` in the layout makes routes under it request-time; see [Dynamic Rendering](../04-rendering/02-dynamic-rendering.md).

### Streaming data through context with `use()`

For data fetched on the server that Client Components must read, pass an **un-awaited Promise** to the provider and unwrap it with `use()`. The request starts on the server immediately and streams, instead of waiting for client JavaScript:

```tsx
// app/layout.tsx
import { UserProvider } from "./user-provider";
import { getUser } from "./user";            // server-side function

export default function RootLayout({ children }: { children: React.ReactNode }) {
  const userPromise = getUser();               // do NOT await

  return (
    <html lang="en">
      <body>
        <UserProvider userPromise={userPromise}>{children}</UserProvider>
      </body>
    </html>
  );
}
```

```tsx
// app/user-provider.tsx
"use client";

import { createContext, useContext } from "react";

type User = { id: string; name: string };
const UserContext = createContext<Promise<User> | null>(null);

export function useUser() {
  const userPromise = useContext(UserContext);
  if (!userPromise) throw new Error("useUser must be used within a UserProvider");
  return userPromise;
}

export function UserProvider({ children, userPromise }: { children: React.ReactNode; userPromise: Promise<User> }) {
  return <UserContext.Provider value={userPromise}>{children}</UserContext.Provider>;
}
```

```tsx
// app/profile.tsx
"use client";

import { use } from "react";
import { useUser } from "./user-provider";

export function Profile() {
  const user = use(useUser());     // suspends until the promise resolves
  return <p>{user.name}</p>;
}
```

```tsx
// app/page.tsx
import { Suspense } from "react";
import { Profile } from "./profile";

export default function Page() {
  return (
    <Suspense fallback={<p>Loading…</p>}>
      <Profile />
    </Suspense>
  );
}
```

Cautions from the docs:

- Wrap consumers in `<Suspense>`; they suspend while the promise resolves.
- Re-fetching a promise set high in the tree re-runs the Server Component that created it. For data only part of the app needs, put the provider on that subtree, not in the root layout.
- If several components read the same data in one request, wrap the fetcher in React's `cache()` so it runs once.
- If a Client Component needs polling, focus revalidation or mutations on that data, use a client data library instead ([TanStack Query](./03-tanstack-query.md)).
- Only **serializable** values can cross from a Server Component to the provider. You cannot pass functions (other than Server Actions), class instances or most live objects.

## The re-render problem

Every component that calls `useContext(X)` re-renders when the provider's `value` changes, even if it only uses part of it.

```tsx
// New object every render → every consumer re-renders every time the provider re-renders
<AppContext.Provider value={{ user, theme, setTheme }}>
```

Three fixes:

### 1. Split contexts by concern and by update frequency

```tsx
const UserContext = createContext<User | null>(null);          // rarely changes
const ThemeContext = createContext<ThemeContextValue | null>(null);
```

Components that read only the user do not re-render when the theme changes.

### 2. Separate state from dispatch

```tsx
const CartStateContext = createContext<CartState | null>(null);
const CartDispatchContext = createContext<React.Dispatch<CartAction> | null>(null);

export function CartProvider({ children }: { children: React.ReactNode }) {
  const [state, dispatch] = useReducer(cartReducer, initialCart);
  return (
    <CartDispatchContext.Provider value={dispatch}>   {/* dispatch identity is stable */}
      <CartStateContext.Provider value={state}>{children}</CartStateContext.Provider>
    </CartDispatchContext.Provider>
  );
}
```

Components that only trigger actions (an "Add to cart" button) subscribe to the stable dispatch and do not re-render on every state change.

### 3. Memoize the value

```tsx
const value = useMemo(() => ({ theme, setTheme }), [theme]);
return <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>;
```

(If you use the React Compiler, it can do much of this memoization automatically, but splitting contexts is still the clearest design.)

### Also: pass `children`

A provider that receives `children` as a prop does **not** re-render its children when its own state changes, because those elements were created by the parent. That is why providers take `children` rather than rendering the app themselves.

## Context + `useReducer`

For a state machine with several transitions, a reducer keeps rules in one place and works with split contexts:

```tsx
type CartAction = { type: "add"; id: string } | { type: "remove"; id: string } | { type: "clear" };

function cartReducer(state: string[], action: CartAction): string[] {
  switch (action.type) {
    case "add": return state.includes(action.id) ? state : [...state, action.id];
    case "remove": return state.filter((id) => id !== action.id);
    case "clear": return [];
  }
}
```

The reducer is a pure function, which makes it easy to test.

## Context and Server Components together

You cannot read context in a Server Component, but you can still compose them:

```tsx
// app/page.tsx (Server Component)
import { CartProvider } from "./cart-provider";   // Client
import { ProductList } from "./product-list";      // Server: fetches products
import { CartButton } from "./cart-button";        // Client: reads the cart context

export default function Page() {
  return (
    <CartProvider>
      <ProductList />      {/* stays a Server Component, passed through as children */}
      <CartButton />
    </CartProvider>
  );
}
```

Because `<ProductList />` is passed through `children` (not imported by the provider), it remains server-rendered. See [Composition Patterns](../03-components/03-composition-patterns.md).

## Alternatives to context

| Need | Often better |
|---|---|
| Passing a server value a few levels | Props, or fetch again with `cache()` in the component that needs it |
| A value read by deeply nested **Server** Components | `React.cache()` or `server-only` helper functions, not context |
| Frequently changing shared client state | [Zustand](./02-zustand.md) |
| Server data with browser caching | [TanStack Query](./03-tanstack-query.md) |
| Shareable view state | The URL |

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| `createContext is not a function` / context error in a Server Component | Used in a file without `"use client"` | Move to a Client Component file |
| `useX must be used within a Provider` | Component is outside the provider, or in a different tree (a portal is fine; a separate root is not) | Move the provider higher |
| Everything re-renders on any change | One big context with a fresh object value | Split, separate dispatch, memoize |
| Provider re-renders all of the app | Provider renders children itself rather than receiving them | Accept `children` as a prop |
| Hydration mismatch from a provider | Initial state reads `localStorage` or `window` | Start with a stable value, update in an effect, or use a cookie |
| Promise-in-context refetches too often | Provider is high in the tree | Move the provider to the subtree that needs it |
| "Props must be serializable" | Passing a function or class instance to the provider from a Server Component | Pass plain data; define functions in the Client Component |

## Common mistakes

| Mistake | Fix |
|---|---|
| Wrapping `<html>` in the provider | Wrap only `{children}` |
| One mega-context for the whole app | Several small contexts |
| Exporting the raw context | Export a hook that throws when missing |
| Using context for rapidly changing values | Use a store with selectors |
| Mutating the context value instead of setting state | Use `useState`/`useReducer` setters |
| Putting server data in context just to avoid prop drilling | Fetch where used (with `cache()`), or pass props |
| Making the whole layout a Client Component to host a provider | Keep the layout a Server Component; import a client provider |
| Reading context in a Server Component | Not possible; pass props or read data directly |

## Quick Summary

- Context works only in Client Components; create a provider that accepts `children`, and render it from a Server Component layout.
- Place providers as deep as possible; wrap `{children}`, not `<html>`.
- Export a typed `useX()` hook that throws when the provider is absent.
- To stream server data through context, pass an un-awaited Promise and unwrap it with `use()` inside `<Suspense>`.
- Every consumer re-renders on value change: split contexts, separate state from dispatch, memoize values.
- For frequent updates use a store; for server data with client behavior use a query cache.

## Next

- [Zustand](./02-zustand.md)
- [Composition Patterns](../03-components/03-composition-patterns.md)
- [State Overview](./00-state-overview.md)
