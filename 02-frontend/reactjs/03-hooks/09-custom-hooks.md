# Custom Hooks

A custom hook is a JavaScript function whose name starts with `use` and that calls other hooks. It lets you **extract and reuse stateful logic** between components without changing the component hierarchy. This file covers when and how to build one; ready-made examples are in [`10-hook-recipes.md`](./10-hook-recipes.md).

## Prerequisites

[`00-hook-rules.md`](./00-hook-rules.md), [`01-useState.md`](./01-useState.md), and [`02-useEffect.md`](./02-useEffect.md)

---

## What a custom hook is

Components often repeat the same combination of hooks: subscribe to something, keep it in state, clean up. A custom hook packages that combination in one function.

```jsx
// Before: duplicated in every component that needs it
function StatusBar() {
  const [isOnline, setIsOnline] = useState(true);
  useEffect(() => {
    const on = () => setIsOnline(true);
    const off = () => setIsOnline(false);
    window.addEventListener("online", on);
    window.addEventListener("offline", off);
    return () => {
      window.removeEventListener("online", on);
      window.removeEventListener("offline", off);
    };
  }, []);
  return <h1>{isOnline ? "Online" : "Disconnected"}</h1>;
}
```

```jsx
// After: extracted
function useOnlineStatus() {
  const [isOnline, setIsOnline] = useState(true);
  useEffect(() => {
    const on = () => setIsOnline(true);
    const off = () => setIsOnline(false);
    window.addEventListener("online", on);
    window.addEventListener("offline", off);
    return () => {
      window.removeEventListener("online", on);
      window.removeEventListener("offline", off);
    };
  }, []);
  return isOnline;
}

function StatusBar() {
  const isOnline = useOnlineStatus();
  return <h1>{isOnline ? "Online" : "Disconnected"}</h1>;
}

function SaveButton() {
  const isOnline = useOnlineStatus();
  return <button disabled={!isOnline}>Save</button>;
}
```

The components now say **what** they need, not **how** it works.

---

## Rules

1. **Name starts with `use` followed by a capital letter** (`useOnlineStatus`) — the linter relies on this to enforce the Rules of Hooks ([`00-hook-rules.md`](./00-hook-rules.md)).
2. **It may call other hooks** — and must follow the same rules: top level only, never conditionally.
3. **A function that doesn't call any hooks shouldn't be named `use…`** — it's a normal utility function.

---

## Custom hooks share logic, not state

Every call to a hook gets its **own independent state**:

```jsx
function Form() {
  const firstName = useFormInput("Ada");
  const lastName = useFormInput("Lovelace");   // independent from firstName
}
```

Two components using `useOnlineStatus()` each subscribe separately. To share *state* between components, lift it up, use context, or use an external store ([`05-useContext.md`](./05-useContext.md), [`../13-state-management/README.md`](../13-state-management/README.md)).

State changes inside a hook re-render **the component that called it**, like any state in that component.

---

## Passing values between hooks

Hooks can take arguments and return values, so you can compose them. Because the component re-renders, the latest values flow through every hook on each render:

```jsx
function useChatRoom({ serverUrl, roomId }) {
  useEffect(() => {
    const connection = createConnection({ serverUrl, roomId });
    connection.connect();
    return () => connection.disconnect();
  }, [serverUrl, roomId]);
}

function ChatRoom({ roomId }) {
  const [serverUrl, setServerUrl] = useState("https://localhost:1234");
  useChatRoom({ serverUrl, roomId });   // re-subscribes when either changes
}
```

---

## Designing a good hook

### Return value

- **One value** → return it directly (`const isOnline = useOnlineStatus()`).
- **A value and an updater** → an array, like `useState` (`const [value, setValue] = useLocalStorage(...)`), so callers can rename freely.
- **Several related values** → an object (`const { data, error, isLoading } = useFetch(url)`), so order doesn't matter and fields can be added without breaking callers.

### Parameters

- Keep the API small and clear. An options object scales better than many positional arguments.
- **Event handlers passed in** (callbacks) can change identity every render; hooks that put them in effect dependencies will re-run. Either document that callers should keep them stable, or store the latest callback in a ref ([`10-hook-recipes.md`](./10-hook-recipes.md) shows `useInterval`).

### Focus on a concrete purpose

Good: `useOnlineStatus`, `useDebounce`, `useMediaQuery`, `useChatRoom`, `useLocalStorage`.

Avoid generic lifecycle wrappers like `useMount`, `useEffectOnce`, or `useUpdateEffect`. They hide the dependency rules the linter would otherwise enforce and tend to encourage thinking in lifecycles instead of synchronization ([`../02-state-and-rendering/04-component-lifecycle.md`](../02-state-and-rendering/04-component-lifecycle.md)).

### Keep effects inside the hook honest

Everything the rules of `useEffect` require still applies: declare dependencies, clean up, handle races. A hook that fetches should handle cancellation; one that subscribes should unsubscribe.

---

## When to extract a hook

Extract when:

- the same stateful logic appears in **two or more** components,
- a component mixes several unrelated concerns and extracting one makes it readable,
- you want to **name** a concept (`useOnlineStatus` explains itself better than 15 lines of effect), or
- you want to test the logic separately ([`../18-testing-and-debugging/03-hook-testing.md`](../18-testing-and-debugging/03-hook-testing.md)).

Don't extract just because code is long, or for one-off logic with no clear name. A hook that doesn't call any hooks is just a function.

### Hook vs utility function vs component

| You have | Use |
|----------|-----|
| Pure logic with no state or effects | A plain function |
| Stateful or effect-based logic, no UI | A custom hook |
| Reusable UI (with or without logic) | A component |
| Reusable logic that wraps UI | A hook plus a component, or a compound component ([`../05-component-design/02-compound-components.md`](../05-component-design/02-compound-components.md)) |

---

## Hooks replace older patterns

Custom hooks replaced most uses of **render props and higher-order components** for sharing logic. Those patterns are covered in [`../05-component-design/03-legacy-component-patterns.md`](../05-component-design/03-legacy-component-patterns.md) mainly for reading older code.

---

## Organizing hooks in a project

- Keep a hook next to the feature that uses it (`features/chat/useChatRoom.js`); move it to a shared folder only once multiple features need it. See [`../20-frontend-architecture/00-feature-based-architecture.md`](../20-frontend-architecture/00-feature-based-architecture.md).
- One hook per file, named after the hook.
- TypeScript: type parameters and return values explicitly for public hooks ([`../04-typescript-with-react/02-typing-hooks.md`](../04-typescript-with-react/02-typing-hooks.md)).

---

## Common mistakes

- **Not starting the name with `use`** — the linter can't check it, so rule violations go unnoticed.
- **Expecting two components to share state** by calling the same hook — they get separate copies.
- **Over-extracting** — a hook for logic used once, with no reasonable name, adds indirection.
- **Generic `useMount` / `useEffectOnce` helpers** — they hide dependencies and create stale-value bugs.
- **Returning a new object or function each render** and then using it as an effect dependency — causes needless re-runs; stabilize or restructure.
- **Forgetting cleanup** inside the hook — leaks happen in every component that uses it.
- **Putting UI in a hook** — hooks return data and functions; components return JSX.

## Quick summary

- A custom hook is a `use…` function that calls other hooks, to reuse stateful logic
- Calls get independent state; hooks share logic, not state
- Return one value, `[value, setter]`, or an object, depending on the shape
- Extract when logic repeats or deserves a name; avoid lifecycle-style wrappers like `useMount`
- All Rules of Hooks and effect rules still apply inside
- Hooks replaced most render-prop and HOC use cases

## Next

**[`10-hook-recipes.md`](./10-hook-recipes.md)** gives ready-to-use custom hooks for common needs.
