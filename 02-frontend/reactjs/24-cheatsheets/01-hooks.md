# React Hooks Cheatsheet

> Quick-reference cheat sheet for standard React hooks, React 19 additions (`use`, `useActionState`, `useOptimistic`), and common idioms.

---

## 1. Core State & Effect Hooks

| Hook | Syntax | When to Use |
| :--- | :--- | :--- |
| `useState` | `const [val, setVal] = useState(init);` | Local mutable state |
| `useReducer` | `const [state, dispatch] = useReducer(reducer, init);` | Complex multi-branch state transitions |
| `useEffect` | `useEffect(() => { subscribe(); return () => cleanup(); }, [deps]);` | External side-effects (DOM, timers, subscriptions) |
| `useLayoutEffect` | `useLayoutEffect(() => { measure(); }, [deps]);` | Synchronous DOM measurements before paint |
| `useRef` | `const ref = useRef<HTMLDivElement>(null);` | Persistent mutable ref / DOM node attachment |

---

## 2. React 19 Modern Hooks

### `use()`
```tsx
import { use } from 'react';

// Read promise directly inside component or condition!
function UserCard({ promise }: { promise: Promise<UserData> }) {
  const user = use(promise); // Suspends until resolved
  return <div>{user.name}</div>;
}

// Read context conditionally!
if (themeRequired) {
  const theme = use(ThemeContext);
}
```

### `useActionState()`
```tsx
import { useActionState } from 'react';

const [state, formAction, isPending] = useActionState(
  async (previousState, formData) => {
    const res = await apiUpdate(formData.get('name'));
    return res;
  },
  initialState
);

<form action={formAction}>
  <input name="name" />
  <button disabled={isPending}>{isPending ? 'Saving...' : 'Submit'}</button>
</form>;
```

### `useOptimistic()`
```tsx
import { useOptimistic } from 'react';

const [optimisticLikes, setOptimisticLikes] = useOptimistic(
  actualLikes,
  (current, next) => next
);

const handleLike = () => {
  startTransition(async () => {
    setOptimisticLikes(optimisticLikes + 1); // Instant UI
    await sendLikeToServer();
  });
};
```

---

## 3. Concurrency & Performance Hooks

### `useTransition`
```tsx
const [isPending, startTransition] = useTransition();

startTransition(() => {
  // Non-blocking, interruptible render
  setFilteredQuery(query);
});
```

### `useDeferredValue`
```tsx
const deferredQuery = useDeferredValue(query);
// Renders immediately with stale query, catches up in background
```
