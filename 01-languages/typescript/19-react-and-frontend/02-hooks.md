# Hooks

Hooks let function components hold state, run effects, and share logic. Most hooks are typed well by inference, so the skill is knowing the few places where you **do** need to help the compiler: empty initial values, `null` states, refs, reducers, and your own custom hooks. This note covers the built-in hooks with TypeScript in mind, and how to write generic, well-typed custom hooks.

**Prerequisites:**
- [Component props and children](./00-component-props-and-children.md)
- [Generic functions](../06-generics/00-generic-functions.md)
- [Discriminated unions](../03-unions-and-narrowing/04-discriminated-unions.md) (for reducers)

---

## `useState`

TypeScript infers the state type from the initial value:

```tsx
const [count, setCount] = useState(0);           // number
const [name, setName] = useState("");            // string
const [open, setOpen] = useState(false);         // boolean
```

Provide the type argument when the initial value does not tell the whole story:

```tsx
const [user, setUser] = useState<User | null>(null);        // null now, a User later
const [ids, setIds] = useState<string[]>([]);               // not never[]
const [selected, setSelected] = useState<Item>();           // Item | undefined
```

The classic trap: `useState([])` infers `never[]` under `strictNullChecks`, and then you cannot add anything to it. Annotate empty arrays and objects.

Setters accept a value or an updater function. Use the updater when the new state depends on the old one:

```tsx
setCount((c) => c + 1);                                  // c: number
setItems((prev) => [...prev, newItem]);                  // never mutate prev
setUser((u) => (u ? { ...u, name: "Asha" } : u));        // handle the null case
```

**Lazy initialization** runs an expensive initializer only once:

```tsx
const [data, setData] = useState(() => parseLargeJson(raw));
```

### State shapes: prefer a union over flags

```tsx
// avoid: allows impossible combinations
const [loading, setLoading] = useState(false);
const [error, setError] = useState<Error | null>(null);
const [data, setData] = useState<User | null>(null);

// prefer: one state, impossible combinations are unrepresentable
type FetchState =
  | { status: "idle" }
  | { status: "loading" }
  | { status: "success"; data: User }
  | { status: "error"; error: Error };

const [state, setState] = useState<FetchState>({ status: "idle" });
```

See [state machines](../17-design-patterns/06-state-machines.md). For server data, a data-fetching library removes most of this ([server state with TanStack Query](./07-server-state-tanstack-query.md)).

## `useReducer`

Use a reducer when state transitions are many or interrelated. Type the state and a **discriminated union of actions**, and the reducer's `switch` narrows each action:

```tsx
type State = { items: Item[]; status: "idle" | "saving" };

type Action =
  | { type: "added"; item: Item }
  | { type: "removed"; id: string }
  | { type: "saveStarted" }
  | { type: "saveFinished" };

function reducer(state: State, action: Action): State {
  switch (action.type) {
    case "added":        return { ...state, items: [...state.items, action.item] };
    case "removed":      return { ...state, items: state.items.filter((i) => i.id !== action.id) };
    case "saveStarted":  return { ...state, status: "saving" };
    case "saveFinished": return { ...state, status: "idle" };
  }
}

const [state, dispatch] = useReducer(reducer, { items: [], status: "idle" });

dispatch({ type: "added", item });       // checked: payload must match the action
dispatch({ type: "removed" });           // error: id is missing
```

State and action types are inferred from the reducer's signature, so you usually do not pass type arguments to `useReducer`. With a return type of `State` and every case covered, TypeScript also verifies exhaustiveness ([exhaustiveness checking](../03-unions-and-narrowing/06-exhaustiveness-checking.md)).

## `useRef`

Two different jobs, with different types. Full details in [refs](./04-refs.md):

```tsx
const inputRef = useRef<HTMLInputElement>(null);     // DOM element: starts null
const timerRef = useRef<ReturnType<typeof setTimeout>>();   // a mutable box for any value
```

Remember that a ref created with `null` for a DOM element is `null` until the element mounts, so access it with optional chaining (`inputRef.current?.focus()`). Newer `@types/react` versions require an initial argument to `useRef`, so write `useRef<T>(null)` or `useRef<T | undefined>(undefined)`.

## `useEffect` and `useLayoutEffect`

Effects have no type arguments. What matters is the cleanup function and the dependency array.

```tsx
useEffect(() => {
  const id = setInterval(() => setTick((t) => t + 1), 1000);
  return () => clearInterval(id);                 // cleanup: must return void or a function
}, []);
```

**The effect callback cannot be `async`,** because it must return nothing or a cleanup function, not a promise. Define an async function inside:

```tsx
useEffect(() => {
  let ignore = false;                             // guards against out-of-order responses

  async function load() {
    const res = await fetch(`/api/users/${id}`);
    const data: unknown = await res.json();
    if (!ignore) setUser(UserSchema.parse(data));   // validate at the boundary
  }

  void load();                                    // `void` marks the promise as intentionally unawaited
  return () => { ignore = true; };
}, [id]);
```

Better still, use `AbortController` to cancel the request itself ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)), or avoid fetching in effects altogether and use a data-fetching library.

Dependencies: list every reactive value the effect reads. The lint rule `react-hooks/exhaustive-deps` finds omissions. Objects and functions created during render change every render, so they make effects re-run, so move them inside the effect, outside the component, or memoize them.

## `useMemo` and `useCallback`

Both infer their types from the function:

```tsx
const total = useMemo(() => items.reduce((sum, i) => sum + i.price, 0), [items]);   // number

const handleSelect = useCallback((id: string) => {
  setSelectedId(id);
}, []);                                           // (id: string) => void
```

Annotate callback **parameters** (nothing contextual types them here), and let the return type infer. Use these for expensive calculations or to keep stable references for memoized children and effect dependencies. They are not free, so do not wrap everything by default.

## `useContext`

Covered in [context](./03-context.md). The key pattern is a custom hook that throws when the provider is missing, so the context value is never `undefined` at the use site.

## Newer hooks (React 18 and 19)

Types are inferred for these, with a few notes:

- **`useId()`** returns a `string` for accessibility ids (`htmlFor`, `aria-describedby`). Do not use it for list keys.
- **`useTransition()`** returns `[isPending, startTransition]`. Wrap non-urgent state updates in `startTransition(() => setX(...))`.
- **`useDeferredValue(value)`** returns a lagging copy of the same type.
- **`useSyncExternalStore(subscribe, getSnapshot)`** subscribes to an external store. `getSnapshot` must return a stable value (the same reference when nothing changed), and its return type is the hook's type.
- **React 19:** `use(promise or context)`, `useActionState`, `useFormStatus` (from `react-dom`), and `useOptimistic` add typed support for async data and form actions. Their exact types depend on your React and `@types/react` versions, so check the documentation for yours ([forms](./05-forms.md)).

## Writing custom hooks

A custom hook is a function whose name starts with `use` and which calls other hooks. Type its **parameters** and let the **return type** infer, or annotate it for public hooks.

### Return a tuple or an object?

```tsx
// Tuple: caller picks the names. Use `as const` so it is not widened to an array.
function useToggle(initial = false) {
  const [on, setOn] = useState(initial);
  const toggle = useCallback(() => setOn((v) => !v), []);
  return [on, toggle] as const;                      // readonly [boolean, () => void]
}

const [isOpen, toggleOpen] = useToggle();

// Object: names are fixed. Better for three or more values.
function useCounter(initial = 0) {
  const [count, setCount] = useState(initial);
  return {
    count,
    increment: () => setCount((c) => c + 1),
    reset: () => setCount(initial),
  };
}
```

Without `as const`, a returned `[on, toggle]` is typed as `(boolean | (() => void))[]`, which makes destructuring useless.

### Generic hooks

```tsx
function useDebouncedValue<T>(value: T, delayMs: number): T {
  const [debounced, setDebounced] = useState(value);

  useEffect(() => {
    const id = setTimeout(() => setDebounced(value), delayMs);
    return () => clearTimeout(id);
  }, [value, delayMs]);

  return debounced;
}

const query = useDebouncedValue(searchText, 300);    // string
```

`T` is inferred from the argument, and the result has the same type. A more involved example, a typed `localStorage` hook that **validates** what it reads, because stored data is untrusted:

```tsx
function useLocalStorage<T>(
  key: string,
  initial: T,
  parse: (raw: unknown) => T,          // e.g. (raw) => Schema.parse(raw)
) {
  const [value, setValue] = useState<T>(() => {
    try {
      const raw = localStorage.getItem(key);
      return raw === null ? initial : parse(JSON.parse(raw));
    } catch {
      return initial;                   // corrupt or outdated data: fall back
    }
  });

  useEffect(() => {
    localStorage.setItem(key, JSON.stringify(value));
  }, [key, value]);

  return [value, setValue] as const;
}
```

The `parse` parameter ties the runtime check to the type, so callers cannot claim a type the data does not have ([validation recipes](../15-runtime-validation/04-validation-recipes.md)). Remember server rendering: `localStorage` does not exist on the server, so guard or defer reading it ([Next.js](./09-nextjs.md)).

### Hooks that take callbacks

```tsx
function useEvent<Args extends unknown[], R>(fn: (...args: Args) => R) {
  const ref = useRef(fn);
  useEffect(() => { ref.current = fn; });
  return useCallback((...args: Args): R => ref.current(...args), []);
}
```

Generic rest parameters preserve the callback's exact signature ([variadic tuple types](../10-advanced-types/06-variadic-tuple-types.md)).

## Rules of hooks

Call hooks only at the top level of components and custom hooks, never conditionally, in loops, or after an early return. The `eslint-plugin-react-hooks` rules enforce this and the dependency rule. TypeScript does not check it.

## Common mistakes

- `useState([])` or `useState({})` without a type argument, giving `never[]` or `{}`.
- `useState<User>(null)`, which fails under `strictNullChecks` because `null` is not a `User`. Use `User | null`.
- Mutating state (`items.push(x); setItems(items)`) instead of creating a new value.
- An `async` effect callback.
- Missing or unstable dependencies, causing stale data or infinite loops.
- Setting state from a stale closure (`setCount(count + 1)` in an interval), instead of an updater.
- Returning a tuple without `as const` from a custom hook.
- Reading `localStorage` or `window` during render on the server.
- Using a boolean flag per async state and getting impossible combinations.
- Wrapping everything in `useMemo`/`useCallback` without a reason.

## Debugging

- Hover a hook call to see the inferred type. `never[]` or `undefined` in state types signals a missing annotation.
- If an effect runs too often, log the dependencies and compare them between renders. A new object or function each render is the usual cause.
- If state seems stale, check closures and use updater functions.
- If TypeScript says a hook callback's parameter is `any`, there is no context. Annotate it.
- Use React DevTools to inspect hook state and the Profiler to see re-renders.

## Quick summary

- Most hooks infer their types. Annotate `useState` for `null`, empty collections, and unions, and annotate callback parameters.
- Model state as a discriminated union, and use `useReducer` with a union of actions for complex transitions.
- Effects cannot be `async`. Clean up, list dependencies, and validate fetched data.
- Return `as const` tuples or objects from custom hooks, and write generic hooks that tie runtime parsing to the type.
- Follow the rules of hooks with the lint plugin, and use data-fetching libraries instead of hand-written fetch effects.

**Next:** [Context](./03-context.md)