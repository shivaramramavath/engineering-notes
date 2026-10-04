# useContext

Context lets a parent component make data available to **any component below it** in the tree, without passing props through every level. `useContext` is how a component reads it. This file covers the **API and basic usage**; organizing providers and avoiding unnecessary re-renders is in [`../13-state-management/01-context-patterns-and-performance.md`](../13-state-management/01-context-patterns-and-performance.md).

## Prerequisites

[`01-useState.md`](./01-useState.md) and [`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md)

---

## The problem: prop drilling

Passing data through components that don't use it, just to reach a deep descendant:

```jsx
<Page theme={theme}>
  <Layout theme={theme}>
    <Sidebar theme={theme}>
      <Button theme={theme} />   {/* only this one needs it */}
    </Sidebar>
  </Layout>
</Page>
```

Before using context, try **composition** (passing elements as `children` or props — see [`../01-fundamentals/03-children-and-composition.md`](../01-fundamentals/03-children-and-composition.md)), which often removes the drilling without any new concept.

---

## Three steps

### 1. Create the context

```jsx
// ThemeContext.js
import { createContext } from "react";

export const ThemeContext = createContext("light");   // "light" is the default value
```

The default value is used only when a component reads the context **without any provider above it**.

### 2. Provide a value

```jsx
function App() {
  const [theme, setTheme] = useState("dark");

  return (
    <ThemeContext value={theme}>
      <Page />
    </ThemeContext>
  );
}
```

In React 19 you can render the context object directly as a provider. In earlier versions, use `<ThemeContext.Provider value={theme}>`; that form still works.

### 3. Consume it

```jsx
import { useContext } from "react";
import { ThemeContext } from "./ThemeContext";

function Button({ children }) {
  const theme = useContext(ThemeContext);
  return <button className={`btn-${theme}`}>{children}</button>;
}
```

`useContext` returns the value from the **closest provider above** the calling component. React 19 also offers `use(ThemeContext)`, which can be called conditionally (see [`../15-concurrent-and-modern-react/05-react-19-features.md`](../15-concurrent-and-modern-react/05-react-19-features.md)).

---

## Updating context

Context itself is just a delivery mechanism. To make values change, **put state in the provider** and pass both the value and a way to update it:

```jsx
const ThemeContext = createContext(null);

function ThemeProvider({ children }) {
  const [theme, setTheme] = useState("light");

  function toggleTheme() {
    setTheme((t) => (t === "light" ? "dark" : "light"));
  }

  return (
    <ThemeContext value={{ theme, toggleTheme }}>
      {children}
    </ThemeContext>
  );
}
```

```jsx
function ThemeToggle() {
  const { theme, toggleTheme } = useContext(ThemeContext);
  return <button onClick={toggleTheme}>Theme: {theme}</button>;
}
```

When the provider's state changes, it re-renders and all consumers re-render with the new value.

### Nested providers

A nested provider **overrides** the value for its subtree:

```jsx
<ThemeContext value="dark">
  <Page />
  <ThemeContext value="light">
    <Footer />   {/* sees "light" */}
  </ThemeContext>
</ThemeContext>
```

---

## Wrap it in a custom hook

Export a hook instead of the raw context so consumers get a clear error when they forget the provider:

```jsx
const ThemeContext = createContext(null);

export function useTheme() {
  const ctx = useContext(ThemeContext);
  if (ctx === null) {
    throw new Error("useTheme must be used within a ThemeProvider");
  }
  return ctx;
}
```

This pattern gives you a single import, one place to change the shape later, and a failure with a clear message instead of `undefined is not an object`. TypeScript version: [`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md).

---

## When to use context

Good fits — **data many components at different depths need**, which changes infrequently:

- Theme (light/dark)
- Current user / auth status
- Locale / language
- Router state, feature flags
- Dependency injection (an API client instance)

Not great for:

- **Rapidly changing values** (mouse position, form input on each keystroke) — every consumer re-renders on each change.
- **Large, complex state** with many consumers needing different slices — consider a state library ([`../13-state-management/00-choosing-state-management.md`](../13-state-management/00-choosing-state-management.md)).
- **Data used by just one or two components** — pass props.

---

## Performance in one paragraph

**Every component that calls `useContext(X)` re-renders when the value passed to `X`'s provider changes** (compared with `Object.is`) — even if it only uses part of the value. Passing a new object literal (`value={{ theme, toggleTheme }}`) every render makes that happen on every provider render. Mitigations include splitting contexts, memoizing the value, and keeping state in the lowest provider possible; all are covered in [`../13-state-management/01-context-patterns-and-performance.md`](../13-state-management/01-context-patterns-and-performance.md).

---

## Common mistakes

- **Reaching for context before trying composition** — passing elements as props or `children` can eliminate drilling.
- **Forgetting the provider** — consumers silently get the default value (or `null`); wrap in a custom hook that throws.
- **Passing a new object each render** as the value — re-renders all consumers; memoize or split.
- **Putting fast-changing state in context** — causes widespread re-renders.
- **Expecting context to include setters automatically** — you must pass them through the provider yourself.
- **Reading context in a component outside the provider's subtree** — it gets the default value.
- **Creating the context inside a component** — define it at module level so identity is stable.

## Quick summary

- Context passes data deeply without prop drilling: create → provide → consume with `useContext`
- The consumer reads the value from the closest provider above it
- Keep state in the provider and pass the value and updater down
- Wrap `useContext` in a custom hook that throws when no provider exists
- Use for low-frequency, widely needed data (theme, user, locale); beware re-renders of all consumers

## Next

**[`06-useReducer.md`](./06-useReducer.md)** covers reducer-based state, which pairs well with context for larger state logic.
