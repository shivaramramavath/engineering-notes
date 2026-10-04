# Rules of Hooks

Hooks are ordinary JavaScript functions with two restrictions. Break them and React can attach state to the wrong hook or crash with confusing errors. This file states the rules, explains why they exist, and shows how to restructure code that seems to need a "conditional hook".

## Prerequisites

[`../02-state-and-rendering/03-rendering.md`](../02-state-and-rendering/03-rendering.md)

---

## What a hook is

A hook is a function whose name starts with `use` — `useState`, `useEffect`, `useContext`, or your own `useOnlineStatus`. Hooks let a function component "hook into" React features: state, lifecycle-like effects, context, refs, and more.

---

## The two rules

### Rule 1: Only call hooks at the top level

Call hooks **unconditionally, in the same order, on every render**. Never call them inside:

- `if` / `else` blocks
- loops
- nested functions (including event handlers)
- after an early `return`
- `try` / `catch` / `finally`
- callbacks passed to `useMemo`, `useReducer`, or `useEffect`

```jsx
function Profile({ user }) {
  // ❌ Conditional hook
  if (user) {
    const [name, setName] = useState(user.name);
  }

  // ❌ Hook after an early return
  if (!user) return null;
  const [age, setAge] = useState(0);

  // ❌ Hook in a loop
  for (const item of items) {
    useEffect(() => {}, []);
  }
}
```

✅ Corrected:

```jsx
function Profile({ user }) {
  const [name, setName] = useState(user?.name ?? "");
  const [age, setAge] = useState(0);

  if (!user) return null;   // early return AFTER all hooks

  return <p>{name}, {age}</p>;
}
```

### Rule 2: Only call hooks from React functions

Call hooks from:

- **function components**, or
- **other custom hooks**.

Never from regular JavaScript functions, class components, or event handlers:

```jsx
// ❌ Plain function
function getTheme() {
  return useContext(ThemeContext);
}

// ✅ Custom hook (name starts with "use")
function useTheme() {
  return useContext(ThemeContext);
}
```

---

## Why the rules exist

React doesn't know your state variables' names. It identifies each hook by **the order in which it was called** during a render, keeping a list per component instance:

```jsx
function Form() {
  const [name, setName] = useState("");      // slot 0
  const [email, setEmail] = useState("");    // slot 1
  useEffect(() => {}, []);                   // slot 2
}
```

On every render, React walks through the calls and matches them to slots 0, 1, 2. If a hook is skipped on one render (a conditional), every hook after it shifts by one slot and receives the **wrong state**. Calling hooks at the top level guarantees the same sequence every time.

The mechanics are covered in [`../17-react-internals/02-how-hooks-work.md`](../17-react-internals/02-how-hooks-work.md).

---

## Patterns for "conditional" hooks

You can't call a hook conditionally, but you can usually restructure.

### Make the condition live inside the hook

Always call the hook; condition on what it *does*:

```jsx
// ❌
if (isOnline) {
  useEffect(() => { connect(); }, []);
}

// ✅
useEffect(() => {
  if (!isOnline) return;
  connect();
  return () => disconnect();
}, [isOnline]);
```

### Split into separate components

If a hook is only needed in one branch, move that branch into its own component:

```jsx
function Page({ user }) {
  if (!user) return <LoginPrompt />;
  return <UserDashboard user={user} />;   // hooks live here
}
```

`UserDashboard` can call hooks freely, because it only exists when `user` is set.

### Hooks in loops

Move each iteration into a child component:

```jsx
// ❌ useState inside .map
// ✅ Each item is its own component with its own hooks
{items.map((item) => <ItemRow key={item.id} item={item} />)}
```

---

## Enforcement with ESLint

The `react-hooks/rules-of-hooks` rule catches violations automatically (see [`../00-setup/05-typescript-and-linting-setup.md`](../00-setup/05-typescript-and-linting-setup.md)). Keep it set to `error`. Its companion, `exhaustive-deps`, checks dependency arrays — see [`02-useEffect.md`](./02-useEffect.md).

---

## Naming conventions

- Hooks **must** start with `use` followed by a capital letter (`useFetch`). The lint rule relies on this naming to recognize them.
- Functions that don't call hooks shouldn't be named `useSomething`.
- Components start with a capital letter; hooks with lowercase `use`.

---

## Hooks are not magic singletons

Each call to a hook creates **independent** state per component instance. Two components that both call `useCounter()` do **not** share a count; they each get their own. To share data, lift state up, use context, or an external store ([`05-useContext.md`](./05-useContext.md), [`../13-state-management/README.md`](../13-state-management/README.md)).

---

## Common mistakes

- **Calling a hook after an early `return`** — the most common violation. Put all hooks above any early return.
- **Calling hooks in event handlers** — handlers run outside render; call the hook at the top and use its result in the handler.
- **Calling hooks inside `useEffect` or `useMemo` callbacks** — hooks can't nest.
- **Calling `useState` in a loop** — extract a child component per item.
- **Naming a hook-using function without the `use` prefix** — the linter can't verify it.
- **Disabling `rules-of-hooks`** to "make it work" — this hides a real bug that will surface as corrupted state.

## Quick summary

- Call hooks at the top level of components or custom hooks, never conditionally or in loops
- Only call hooks from components or other hooks, and name custom hooks `useSomething`
- React matches hooks by call order, so the order must never change between renders
- Restructure with conditions inside the hook, or split into child components
- Keep `rules-of-hooks` as an ESLint error

## Next

**[`01-useState.md`](./01-useState.md)** covers the most-used hook in detail.
