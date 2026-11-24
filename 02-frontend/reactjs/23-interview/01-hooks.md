# React Hooks — Senior Interview Questions & Deep Dives

> Deep architectural exploration of hook linked lists, stale closures, dependency array pitfalls, React 19 hooks (`use`, `useActionState`, `useOptimistic`), and custom hook design.

---

## Question 1: How does React internally store and track Hooks without argument keys? Why does call order strictly matter?

### The Core Answer
Inside the React Fiber node, hooks are stored as a singly linked list via the `memoizedState` property. When a component executes:
1. `currentlyRenderingFiber` points to the active component's Fiber.
2. A pointer `workInProgressHook` advances down the linked list node-by-node:
   ```
   Fiber.memoizedState ──> [Hook 1: useState] ──> [Hook 2: useEffect] ──> [Hook 3: useMemo] ──> null
   ```
3. Each call (`useState`, `useEffect`) accesses the hook at `workInProgressHook.next`.

### Why Conditional Calls Crash React
If a hook call is wrapped in an `if (condition)` statement, the number or sequence of hook invocations changes between renders. The pointer reads the state of a completely different hook:
- A `useState` counter might read the cleanup function of a `useEffect`.
- React detects a mismatch between the number of hooks on the current fiber and the work-in-progress fiber, throwing the error: *"Rendered fewer hooks than expected. This may be caused by an accidental early return statement."*

---

## Question 2: What is the "Stale Closure" problem in `useEffect` and `useCallback`, and how does `useEffectEvent` resolve it?

### The Stale Closure Mechanism
JavaScript closures capture variables by reference to the lexical environment in which the function was instantiated. If an effect or callback has an empty `[]` dependency array, it captures the initial value of the variables from the first render:
```tsx
function Timer() {
  const [count, setCount] = useState(0);

  useEffect(() => {
    const id = setInterval(() => {
      // Stale closure! `count` is forever locked at 0 from render 1
      console.log(count);
    }, 1000);
    return () => clearInterval(id);
  }, []); // Missing count dependency
}
```

### The Solution: Functional Updates or `useEffectEvent`
1. For state mutations: Use the functional updater form `setCount(prev => prev + 1)` which reads current state from React's internal fiber queue without needing `count` in the dependency array.
2. For reactive logic: `useEffectEvent` (experimental / React 19 pattern) separates reactive triggers from non-reactive reading:
```tsx
const onTick = useEffectEvent(() => {
  logEvent(count); // Reads current count without re-subscribing the interval!
});

useEffect(() => {
  const id = setInterval(() => onTick(), 1000);
  return () => clearInterval(id);
}, []); // No need to re-trigger interval on count change
```

---

## Question 3: How does React 19's `use()` hook differ fundamentally from all previous hooks?

### The Core Answer
The `use()` hook is the first React hook that **breaks the classical Rules of Hooks**:
1. **Can be called conditionally**: `use()` can be placed inside `if` statements and loops.
2. **Handles Promises directly**: Calling `use(fetchPromise)` suspends the component until the promise resolves, integrating seamlessly with `<Suspense>`.
3. **Reads Context conditionally**: `use(ThemeContext)` can be invoked inside nested branch conditions, whereas `useContext(ThemeContext)` must always run unconditionally at the component top level.

```tsx
function UserProfile({ userPromise, fetchDetails }: { userPromise: Promise<User>, fetchDetails: boolean }) {
  // Suspend conditionally!
  if (fetchDetails) {
    const user = use(userPromise);
    return <div>{user.name} - Detailed View</div>;
  }
  return <div>Summary View</div>;
}
```

---

## Question 4: How does `useActionState` manage pending state, errors, and optimistic transitions in React 19?

### The Core Answer
`useActionState` simplifies asynchronous form actions and server mutations:
```tsx
const [state, formAction, isPending] = useActionState(async (prevState, formData) => {
  const result = await updateProfile(formData);
  if (!result.ok) return { error: result.message, data: prevState.data };
  return { error: null, data: result.data };
}, { error: null, data: initialData });
```

### Key Architectural Behaviors
- **Integrated Transition**: The action is automatically executed inside a React Transition (`startTransition`), ensuring `isPending` is `true` while the background work proceeds without freezing the UI.
- **Form Association**: `formAction` can be passed directly to `<form action={formAction}>`, automatically intercepting submissions and managing submission locks.
- **Rollback Resilience**: When paired with `useOptimistic`, failed action states automatically revert optimistic mutations to the last confirmed state.
