# Context

React context lets a value (the current user, a theme, an API client) be read by any component below a provider, without passing props through every level. Typing it well is mostly about one question: **what is the context's value when no provider exists?** The pattern in this note makes that question disappear, so consumers always get a defined, correctly typed value, and misuse fails loudly instead of silently.

> **Version note.** React 19 lets you render the context object directly as a provider (`<ThemeContext value={...}>`) and read it with `use(ThemeContext)`. React 18 uses `<ThemeContext.Provider value={...}>` and `useContext(ThemeContext)`. Both are shown where they differ.

**Prerequisites:**
- [Hooks](./02-hooks.md)
- [Component props and children](./00-component-props-and-children.md)
- [Dependency injection](../17-design-patterns/05-dependency-injection.md) (context is DI for components)

---

## The problem: the default value

`createContext` requires a default value, used only when there is **no provider above** the consumer:

```tsx
interface Theme { mode: "light" | "dark"; toggle: () => void }

const ThemeContext = createContext<Theme>({
  mode: "light",
  toggle: () => {},          // a do-nothing fake, just to satisfy the type
});
```

This compiles, but it hides bugs: a component rendered outside the provider silently gets a fake theme whose `toggle` does nothing. A realistic context value (an authenticated user, an API client) often has no sensible default at all.

## The standard pattern: `undefined` default plus a checking hook

Make the context value possibly `undefined`, and expose a hook that **throws** if it is missing:

```tsx
import { createContext, useContext, useMemo, useState, type ReactNode } from "react";

interface ThemeContextValue {
  mode: "light" | "dark";
  toggle: () => void;
}

const ThemeContext = createContext<ThemeContextValue | undefined>(undefined);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [mode, setMode] = useState<"light" | "dark">("light");

  const value = useMemo<ThemeContextValue>(
    () => ({ mode, toggle: () => setMode((m) => (m === "light" ? "dark" : "light")) }),
    [mode],
  );

  return <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>;
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (ctx === undefined) {
    throw new Error("useTheme must be used within a ThemeProvider");
  }
  return ctx;                // narrowed: ThemeContextValue, never undefined
}
```

Why this works well:

- The **hook's return type is non-nullable**. Consumers never write `ctx?.mode` or `ctx!`.
- A missing provider is a **clear error at the point of misuse**, not a mysterious wrong behavior later.
- The context object itself is **not exported**, so everyone goes through the hook and the checking cannot be bypassed.

Usage:

```tsx
function ThemeButton() {
  const { mode, toggle } = useTheme();         // fully typed
  return <button onClick={toggle}>Current: {mode}</button>;
}
```

In React 19, the provider and hook can be written as:

```tsx
return <ThemeContext value={value}>{children}</ThemeContext>;

const ctx = use(ThemeContext);       // also usable inside conditionals, unlike useContext
```

## A reusable strict-context factory

If you have several contexts, the pattern is mechanical, so write it once:

```tsx
export function createStrictContext<T>(name: string) {
  const Context = createContext<T | undefined>(undefined);

  function useStrictContext(): T {
    const value = useContext(Context);
    if (value === undefined) {
      throw new Error(`${name} provider is missing`);
    }
    return value;
  }

  return [Context.Provider, useStrictContext] as const;
}

const [AuthProvider, useAuth] = createStrictContext<AuthState>("Auth");
```

`as const` keeps the tuple's element types. One limitation: if a **valid** context value can itself be `undefined`, this cannot tell "no provider" from "provider with `undefined`". Wrap such values in an object, or use a unique sentinel instead of `undefined` as the default.

## Memoize the provider value

Every consumer re-renders when the provider's `value` changes **by reference**. An object literal created inline changes every render:

```tsx
// re-renders all consumers on every render of Provider's parent
<ThemeContext.Provider value={{ mode, toggle }}>

// stable between renders unless `mode` changes
const value = useMemo(() => ({ mode, toggle }), [mode, toggle]);
<ThemeContext.Provider value={value}>
```

Functions inside the value should be stable too (`useCallback`, or state setters, which are already stable).

## Split state from actions

When some consumers only dispatch actions and never read state, putting both in one context re-renders them needlessly on every state change. Two contexts avoid that:

```tsx
const StateContext = createContext<State | undefined>(undefined);
const DispatchContext = createContext<Dispatch<Action> | undefined>(undefined);

function CartProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(cartReducer, initialState);
  return (
    <DispatchContext.Provider value={dispatch}>
      <StateContext.Provider value={state}>{children}</StateContext.Provider>
    </DispatchContext.Provider>
  );
}
```

`dispatch` from `useReducer` is stable for the lifetime of the component, so consumers of `DispatchContext` never re-render because of state changes. Type `dispatch` as `Dispatch<Action>` (from `react`), and write two small hooks, one per context. The reducer and its discriminated-union actions are in [hooks](./02-hooks.md).

## Context as dependency injection

Context is a convenient way to give a subtree a service, and to swap it in tests:

```tsx
const [ApiProvider, useApi] = createStrictContext<ApiClient>("Api");

function UserList() {
  const api = useApi();
  const { data } = useQuery({ queryKey: ["users"], queryFn: () => api.listUsers() });
  // ...
}

// production
<ApiProvider value={realClient}><App /></ApiProvider>

// test
<ApiProvider value={fakeClient}><UserList /></ApiProvider>
```

The component depends on the `ApiClient` **interface**, not on a concrete client or a global ([dependency injection](../17-design-patterns/05-dependency-injection.md), [typed fetch and API client](../16-type-safe-apis/05-typed-fetch-and-api-client.md)).

## Testing components that use context

Wrap the component under test in the provider, using a helper:

```tsx
function renderWithTheme(ui: ReactElement, mode: "light" | "dark" = "light") {
  return render(<ThemeContext.Provider value={{ mode, toggle: vi.fn() }}>{ui}</ThemeContext.Provider>);
}
```

This is one reason to export the provider (and, for tests, optionally the raw context) from the module that defines it, while still hiding the context from application code. See [mocking](../18-testing-and-debugging/02-mocking.md).

## When context is the wrong tool

Context is for values that change **rarely** and are needed in **many places**: theme, locale, current user, an API client, feature flags.

It is a poor fit for frequently changing state (form input, mouse position, a large store). Every consumer re-renders when the value changes, and there is no built-in selector to subscribe to part of the value. For that, use a state library with selectors ([client state with Zustand](./08-client-state-zustand.md)), or lift state and pass props. For server data, use a query library rather than hand-rolling fetch state in a provider ([server state](./07-server-state-tanstack-query.md)).

## Server components

In frameworks with React Server Components (such as the Next.js App Router), context only works in **client components**. A provider must be in a file marked `"use client"`, and server components can render it and pass `children` through. See [Next.js](./09-nextjs.md).

## Important rules and misconceptions

- **The default value is used only when no provider exists above.** A provider with value `undefined` overrides it.
- **Context does not "update" selectively.** All consumers of a context re-render when its value changes by reference.
- **Splitting a value into several properties does not help** if they live in one context object. Split the contexts.
- **Context is not global state management.** It is a transport for a value. The state still lives in a component (`useState`, `useReducer`).
- **A provider can appear multiple times,** and the nearest one wins. This is useful for scoped overrides.

## Common mistakes

- Giving a fake default value so the context "works" without a provider, hiding missing-provider bugs.
- Using `useContext(Ctx)!` or `ctx?.` instead of a throwing hook.
- Passing an inline object as `value`, causing re-renders of every consumer.
- Putting fast-changing state in context and wondering why the app is slow.
- Exporting the raw context and having components call `useContext` directly, bypassing the check.
- Making one giant context for everything.
- Providers placed too low in the tree, so some consumers are outside.
- Using context in a server component.

## Debugging

- If a hook throws "must be used within a provider", the consumer is rendered outside it. Check the tree in React DevTools, and portals or separate roots (modals, tooltips) that may sit outside the provider.
- If many components re-render on every change, check whether the `value` is memoized and whether the context mixes frequently changing data with stable data.
- Use React DevTools' "Components" tab to inspect each provider's current value.
- If a consumer sees a stale value, check for a nested provider that overrides it.

## Quick summary

- Create the context with `T | undefined` and a hook that throws when the value is missing, so consumers get a non-nullable `T`.
- Do not export the raw context. Export the provider and the hook. A small factory removes the boilerplate for many contexts.
- Memoize the provider value, and split state and dispatch into separate contexts to limit re-renders.
- Use context for rarely changing, widely needed values and as dependency injection for services. For fast-changing state, use a store with selectors.
- React 19 adds `<Context value>` and `use(Context)`. In server component frameworks, context lives in client components.

**Next:** [Refs](./04-refs.md)
