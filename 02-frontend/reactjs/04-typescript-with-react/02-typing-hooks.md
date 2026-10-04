# Typing Hooks

Most built-in hooks infer their types from the arguments you pass. You only add annotations where inference can't know the answer — usually when the initial value doesn't represent every value the state will hold. This file covers each core hook, then typing your own.

## Prerequisites

[`00-typing-components-and-props.md`](./00-typing-components-and-props.md) and [`../03-hooks/`](../03-hooks/README.md)

---

## `useState`

TypeScript infers the type from the initial value:

```tsx
const [count, setCount] = useState(0);          // number
const [name, setName] = useState("");           // string
const [open, setOpen] = useState(false);        // boolean
```

Annotate with a **generic** when the initial value is narrower than what you'll store:

```tsx
type User = { id: string; name: string };

const [user, setUser] = useState<User | null>(null);   // starts null, later a User
const [tags, setTags] = useState<string[]>([]);        // [] alone is inferred as never[]
const [selectedId, setSelectedId] = useState<string | undefined>();
```

Then narrow before use:

```tsx
if (user === null) return <p>No user</p>;
return <p>{user.name}</p>;      // user: User here
```

### Mutually exclusive states

Use a union, so impossible combinations don't type-check ([`../02-state-and-rendering/02-state-structure-and-lifting.md`](../02-state-and-rendering/02-state-structure-and-lifting.md)):

```tsx
type Status = "idle" | "loading" | "success" | "error";
const [status, setStatus] = useState<Status>("idle");

type FetchState =
  | { status: "idle" }
  | { status: "loading" }
  | { status: "success"; data: User[] }
  | { status: "error"; error: Error };

const [state, setState] = useState<FetchState>({ status: "idle" });

if (state.status === "success") {
  state.data;   // User[] — only available when status is "success"
}
```

---

## `useReducer`

Type the state and the **actions as a discriminated union**. TypeScript then checks every `case` and narrows `action` automatically:

```tsx
type Todo = { id: number; text: string; done: boolean };

type Action =
  | { type: "added"; id: number; text: string }
  | { type: "toggled"; id: number }
  | { type: "deleted"; id: number };

function todosReducer(todos: Todo[], action: Action): Todo[] {
  switch (action.type) {
    case "added":
      return [...todos, { id: action.id, text: action.text, done: false }];
    case "toggled":
      return todos.map((t) => (t.id === action.id ? { ...t, done: !t.done } : t));
    case "deleted":
      return todos.filter((t) => t.id !== action.id);
  }
}

const [todos, dispatch] = useReducer(todosReducer, []);

dispatch({ type: "added", id: 1, text: "Buy milk" });   // ✅
dispatch({ type: "added", id: 1 });                     // ❌ text is missing
dispatch({ type: "renamed" });                          // ❌ unknown action
```

With a return type annotation and a union that covers every case, TypeScript flags a missing `case`. For an explicit guarantee, see the exhaustiveness check in [`05-advanced-typing-patterns.md`](./05-advanced-typing-patterns.md).

---

## `useRef`

`useRef` has two typing modes, depending on what the ref holds.

### DOM refs

Pass the element type and `null`. In React 19's types the result is `RefObject<HTMLInputElement | null>`, so `current` can be `null` and needs a check:

```tsx
const inputRef = useRef<HTMLInputElement>(null);

function focusInput() {
  inputRef.current?.focus();      // optional chaining handles null
}

return <input ref={inputRef} />;
```

Use the **most specific element type** (`HTMLInputElement`, `HTMLDivElement`, `HTMLCanvasElement`) so you get the right methods and properties. Not sure which? Hover the element's `ref` prop.

### Mutable value refs

For values you manage yourself (timer ids, previous values), pass the value type and an initial value:

```tsx
const timerRef = useRef<number | undefined>(undefined);
const countRef = useRef(0);                                // inferred number

timerRef.current = window.setInterval(tick, 1000);
```

React 19's types require an initial argument (`useRef(undefined)` rather than `useRef()`). Details on when to use refs: [`../03-hooks/04-useRef.md`](../03-hooks/04-useRef.md).

---

## `useContext`

Contexts need a default value of the right type. A common pattern is `null` as the default, then a custom hook that **narrows it away** so consumers never handle `null`:

```tsx
import { createContext, useContext, useState, type ReactNode } from "react";

type Theme = "light" | "dark";

type ThemeContextValue = {
  theme: Theme;
  toggleTheme: () => void;
};

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [theme, setTheme] = useState<Theme>("light");
  const toggleTheme = () => setTheme((t) => (t === "light" ? "dark" : "light"));

  return (
    <ThemeContext value={{ theme, toggleTheme }}>{children}</ThemeContext>
  );
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (ctx === null) {
    throw new Error("useTheme must be used within a ThemeProvider");
  }
  return ctx;   // narrowed to ThemeContextValue
}
```

This keeps the runtime safety from [`../03-hooks/05-useContext.md`](../03-hooks/05-useContext.md) and gives consumers a clean, non-nullable type. (Use `<ThemeContext.Provider value=…>` on React versions before 19.)

---

## `useMemo` and `useCallback`

Both infer from what you return; annotate only when you need to:

```tsx
const total = useMemo(() => items.reduce((s, i) => s + i.price, 0), [items]);  // number

const handleSelect = useCallback((id: string) => {
  setSelectedId(id);
}, []);
// (id: string) => void — parameter types are NOT inferred, so annotate them
```

Parameters of a callback you write inside `useCallback` need explicit types, because there's no contextual type to infer from.

---

## `useEffect` and `useLayoutEffect`

Effects need no type arguments. The setup function returns `void` or a cleanup function; TypeScript checks that you don't return anything else (a common error is passing an `async` function directly):

```tsx
useEffect(() => {
  const id = setTimeout(done, 1000);
  return () => clearTimeout(id);
}, []);
```

---

## Custom hooks

Type parameters explicitly on **public** hooks, and let the return type be inferred or declared deliberately.

### Return a tuple

Array returns are inferred as arrays, not tuples, unless you say otherwise:

```tsx
// ❌ Inferred as (boolean | (() => void))[] — loses positional types
function useToggle(initial = false) {
  const [value, setValue] = useState(initial);
  const toggle = () => setValue((v) => !v);
  return [value, toggle];
}

// ✅ Use `as const` to get a readonly tuple
function useToggle(initial = false) {
  const [value, setValue] = useState(initial);
  const toggle = () => setValue((v) => !v);
  return [value, toggle] as const;     // readonly [boolean, () => void]
}

// ✅ Or declare the return type
function useToggle(initial = false): [boolean, () => void] { /* ... */ }
```

### Return an object

Objects need no special handling and scale better as the hook grows:

```tsx
function useOnlineStatus() {
  const [isOnline, setIsOnline] = useState(true);
  /* effect omitted */
  return { isOnline };
}
```

### Generic hooks

A hook that stores or returns arbitrary data takes a type parameter — see [`03-generics.md`](./03-generics.md):

```tsx
function useLocalStorage<T>(key: string, initial: T) { /* ... */ }
const [theme, setTheme] = useLocalStorage<Theme>("theme", "light");
```

Examples to type: [`../03-hooks/10-hook-recipes.md`](../03-hooks/10-hook-recipes.md).

---

## `useImperativeHandle`

Type the exposed handle with an interface, and the ref as an ordinary prop (React 19) or via `forwardRef`:

```tsx
type FancyInputHandle = { focus: () => void };

function FancyInput({ ref }: { ref: React.Ref<FancyInputHandle> }) { /* ... */ }
```

Full coverage in [`../16-advanced-react/01-refs-and-imperative-handles.md`](../16-advanced-react/01-refs-and-imperative-handles.md).

---

## Common mistakes

- **Letting `useState([])` infer `never[]`** — add a generic: `useState<Item[]>([])`.
- **`useState(null)` without a union** — infers `null` only, so setting an object errors; use `useState<User | null>(null)`.
- **Forgetting `null` handling on DOM refs** — `ref.current` is `null` until mounted; use `?.`.
- **Typing a context default with a fake object** (`{} as Value`) — hides missing providers; use `null` plus a throwing hook.
- **Returning arrays from custom hooks without `as const`** — callers lose tuple types.
- **Leaving `useCallback` parameters untyped** — they're implicitly `any` under no inference; annotate them.
- **Using `any` for reducer actions** — use a discriminated union so TypeScript validates each `dispatch`.
- **Over-annotating** hooks that already infer correctly — noise with no benefit.

## Quick summary

- Rely on inference; add `useState<T>` when the initial value doesn't cover every value
- Model status and actions as discriminated unions
- DOM refs: `useRef<HTMLElementType>(null)` and handle `null`; value refs: `useRef<T>(initial)`
- Contexts: default `null` plus a custom hook that narrows and throws outside the provider
- Annotate `useCallback` parameters; effects need no type arguments
- Custom hooks: `as const` or an explicit tuple for array returns, objects for many values, generics for arbitrary data

## Next

**[`03-generics.md`](./03-generics.md)** shows how to write components and hooks that work with any data type.
