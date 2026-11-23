# React Hooks Typing in TypeScript: useState, useReducer, useEffect, and React 19 Hooks

> Comprehensive typing for React Hooks: useState initial state inference, useReducer discriminated action unions, useEffect cleanup closures, useMemo, useCallback, and React 19 hooks (`use()`, `useActionState`, `useOptimistic`).

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [06-generics/00-generic-functions.md](../06-generics/00-generic-functions.md), [19-react-and-frontend/00-component-props-and-children.md](./00-component-props-and-children.md) |
| **TypeScript Baseline** | TS 5.0+, React 19 / @types/react 19 |
| **Tested tsconfig** | `"strict": true, "jsx": "react-jsx", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

React Hooks manage local component state, side effects, refs, memoization, and server interactions. In TypeScript, hooks rely heavily on generic type parameter inference:
- `useState`: Inferring state types from initial values, or explicitly specifying union types when state starts as `null` or `undefined`.
- `useReducer`: Enforcing exhaustive type safety across action types and payloads using discriminated unions.
- `useMemo` & `useCallback`: Preserving function signatures and return types without accidental widening to `any`.

With **React 19**, React introduced revolutionary new hooks:
1. **`use(Promise | Context)`**: Reading promises or context conditionally inside components or hooks.
2. **`useActionState`**: Handling asynchronous form actions, pending states, and returned state.
3. **`useOptimistic`**: Typing optimistic UI updates that roll back if the server action fails.

```
+-----------------------------------------------------------+
|                   React 19 Hook Typing                    |
|                                                           |
|  const [state, formAction, isPending] =                   |
|    useActionState<FormState, FormData>(action, initial);  |
|                                                           |
|  const [optimisticList, setOptimisticList] =              |
|    useOptimistic<Todo[], Todo>(todos, updateFn);          |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. `useState` Inference and Explicit Generics
When an initial value is provided, TypeScript infers the exact type:
```typescript
const [count, setCount] = useState(0); // count: number
```
When state is nullable or starts empty:
```typescript
const [user, setUser] = useState<User | null>(null); // user: User | null
```

### 2. `useReducer` with Discriminated Actions
Reducers model state machines where every action is typed via a tagged union:
```typescript
type Action =
  | { type: 'INCREMENT'; step: number }
  | { type: 'RESET' };

function reducer(state: number, action: Action): number {
  switch (action.type) {
    case 'INCREMENT': return state + action.step;
    case 'RESET': return 0;
  }
}
```

### 3. React 19 `useActionState`
Replaces legacy `useFormState`:
```typescript
export type ActionFn<State, Payload> = (prevState: State, payload: Payload) => Promise<State>;
```

---

## 3. How It Works Internally

In `@types/react`, `useState` has two overloaded signatures:
```typescript
function useState<S>(initialState: S | (() => S)): [S, Dispatch<SetStateAction<S>>];
function useState<S = undefined>(): [S | undefined, Dispatch<SetStateAction<S | undefined>>];
```
When `useState(initialValue)` is called without explicit generic parameters, `S` is inferred from `typeof initialValue`. When called with no arguments `useState()`, `S` defaults to `undefined`.

---

## 4. Syntax & API Reference

### React 19 Modern Hooks Signature Overview
```typescript
import { use, useActionState, useOptimistic } from 'react';

// Reading a Promise or Context conditionally
const data = use(fetchUserPromise);

// Form Actions
const [state, submitAction, isPending] = useActionState(
  async (prevState: State, formData: FormData) => { return newState; },
  initialState
);

// Optimistic updates
const [optimisticState, addOptimistic] = useOptimistic(
  realState,
  (current: State, optimisticValue: Update) => nextState
);
```

---

## 5. Practical Examples

### Example 1: Basic `useState` and `useCallback`
```typescript
import { useState, useCallback } from 'react';

export function Counter() {
  const [count, setCount] = useState<number>(0);

  const increment = useCallback(() => {
    setCount((prev) => prev + 1);
  }, []);

  return <button onClick={increment}>Count: {count}</button>;
}
```

### Example 2: Real-World Type-Safe `useReducer` for Shopping Cart
```typescript
import { useReducer } from 'react';

interface CartItem { id: string; name: string; price: number; }
interface CartState { items: CartItem[]; total: number; }

type CartAction =
  | { type: 'ADD_ITEM'; item: CartItem }
  | { type: 'REMOVE_ITEM'; id: string }
  | { type: 'CLEAR' };

function cartReducer(state: CartState, action: CartAction): CartState {
  switch (action.type) {
    case 'ADD_ITEM':
      return {
        items: [...state.items, action.item],
        total: state.total + action.item.price
      };
    case 'REMOVE_ITEM': {
      const filtered = state.items.filter((i) => i.id !== action.id);
      return {
        items: filtered,
        total: filtered.reduce((acc, i) => acc + i.price, 0)
      };
    }
    case 'CLEAR':
      return { items: [], total: 0 };
  }
}

export function useCart() {
  return useReducer(cartReducer, { items: [], total: 0 });
}
```

### Example 3: Edge Case - `useEffect` Cleanup Closure Typing
```typescript
import { useEffect } from 'react';

// useEffect callbacks must return either void or a cleanup function () => void.
// Returning anything else (like a Promise) is a compiler error!
export function useWindowResize(handler: () => void) {
  useEffect(() => {
    window.addEventListener('resize', handler);
    // Cleanup must be synchronous () => void:
    return () => {
      window.removeEventListener('resize', handler);
    };
  }, [handler]);
}
```

### Example 4: Failure Case - Passing Async Callback to `useEffect`
```typescript
import { useEffect } from 'react';

// @ts-expect-error TS2345: Argument of type '() => Promise<void>' is not assignable to parameter of type 'EffectCallback'. Type 'Promise<void>' is not assignable to type 'void | Destructor'.
useEffect(async () => {
  await fetch('/api/data');
});
```

### Example 5: Idiomatic React 19 `useActionState` Form Handling
```typescript
import { useActionState } from 'react';

interface FormState {
  success: boolean;
  error?: string;
}

async function updateProfile(prevState: FormState, formData: FormData): Promise<FormState> {
  const username = formData.get('username') as string;
  if (!username) return { success: false, error: 'Username required' };
  return { success: true };
}

export function ProfileForm() {
  const [state, formAction, isPending] = useActionState(updateProfile, { success: false });

  return (
    <form action={formAction}>
      <input name="username" type="text" />
      <button type="submit" disabled={isPending}>Save</button>
      {state.error && <p className="error">{state.error}</p>}
    </form>
  );
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Async in `useEffect`**:
   `useEffect` does NOT allow `async () => {}` because an async function returns a `Promise`, whereas React expects either `undefined` or a synchronous cleanup function (`() => void`). To call async functions in effects, define an internal async function and invoke it immediately:
   ```typescript
   useEffect(() => {
     let mounted = true;
     async function run() {
       const res = await fetchData();
       if (mounted) setData(res);
     }
     run();
     return () => { mounted = false; };
   }, []);
   ```
2. **`useCallback` without Generic Type Annotation**:
   Writing `useCallback((e) => ..., [])` usually infers `e` as `any` unless the callback is passed directly inline to a JSX prop whose type is known.
3. **Lazy Initializer Typing**:
   When passing a lazy initializer to `useState(() => expensiveCalculation())`, make sure the initializer is a pure function returning the expected state type, not calling it immediately.
4. **`use(Promise)` in React 19**:
   Unlike other hooks, `use()` can be called conditionally inside `if` statements and loops. However, the promise passed to `use()` must be cached (e.g. created outside render or wrapped in a cache layer), otherwise every render triggers a new promise, causing an infinite suspense loop.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Async Effect Callback** | `useEffect(async () => ...)` | Inner async function `run()` | React warning; broken cleanup function |
| **`useState()` Without Union** | `const [u, setU] = useState()` | `useState<User \| null>(null)` | Type inferred as `undefined`, cannot assign `User` |
| **Un-discriminated Reducer** | `action: { type: string, payload?: any }` | Tagged union with exact payloads | Loss of compiler verification on action payloads |
| **Omitting Deps in `useCallback`** | Stale closures from missing deps | Include all referenced variables in deps | Subtle runtime state bugs |
| **Returning Non-Void from Cleanups** | `return window.removeEventListener(...)` | `return () => window.removeEventListener(...)` | Runtime crash when React executes non-function |
| **Un-memoized Context Values** | `value={{ count }}` inside Provider | `useMemo(() => ({ count }), [count])` | Triggers rerender on all consumers on every render |

---

## 8. Performance & Type-Checker Complexity

- **Generic Depth in Reducers**: Complex state reducers with 30+ actions check quickly if action unions are discriminated by a string literal `type`.
- **Hook Return Tuples**: Returning `[state, dispatch] as const` prevents TypeScript from widening the returned tuple to an array `(State | Dispatch)[]`.

---

## 9. Compiler Internals

When evaluating custom hooks returning tuples:
```typescript
export function useToggle(init = false) {
  const [val, setVal] = useState(init);
  const toggle = () => setVal(!val);
  return [val, toggle] as const;
}
```
Without `as const`, TypeScript infers `(boolean | (() => void))[]`. With `as const`, TypeScript creates a `readonly [boolean, () => void]` tuple with strict element position typing.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `as const` on Custom Hook Return Tuples**: Preserve positional type accuracy.
2. **Type Actions as Discriminated Unions**: Eliminate runtime action payload mismatches.
3. **Use React 19 `useActionState` for Forms**: Simplifies pending flags and server validation.
4. **Always Define Synchronous Cleanup Functions in Effects**: Prevent promise leaks.

---

## 11. Real-World Case Study

### Optimistic Likes Hook in React 19
An interactive social media post that updates like counts instantly, rolling back if the server mutation fails:

```typescript
import { useOptimistic, useTransition } from 'react';

interface Post { id: string; likes: number; }

export function PostLikeButton({ post, onLikeAction }: { post: Post; onLikeAction: () => Promise<void> }) {
  const [isPending, startTransition] = useTransition();

  const [optimisticLikes, addOptimisticLike] = useOptimistic(
    post.likes,
    (current, update: number) => current + update
  );

  const handleClick = () => {
    startTransition(async () => {
      addOptimisticLike(1); // Optimistic increment
      await onLikeAction();  // Server action
    });
  };

  return (
    <button onClick={handleClick} disabled={isPending}>
      ♥ {optimisticLikes} {isPending ? '(Updating...)' : ''}
    </button>
  );
}
```

---

## 12. Interview Questions

<details>
<summary>1. Why must custom hooks returning tuples use `as const`?</summary>

In TypeScript, arrays are inferred by default as homogeneous unions: `[a, b]` becomes `(TypeA | TypeB)[]`. When destructuring `const [val, setVal] = useCustomHook()`, both variables are typed as `TypeA | TypeB`. Adding `as const` forces TypeScript to infer a fixed-length tuple `readonly [TypeA, TypeB]`.
</details>

<details>
<summary>2. Why does `useEffect` forbid returning a `Promise<void>`?</summary>

`useEffect` expects either nothing (`void | undefined`) or a cleanup destructor function (`() => void`). An `async` function returns a `Promise`. If React executed `await effect()`, it could not run cleanup functions synchronously when components unmount or before the next effect run, causing race conditions and memory leaks.
</details>

<details>
<summary>3. What is the new `use()` hook in React 19?</summary>

`use()` is a React 19 hook that unwraps the value of a Promise or a React Context. Unlike all other React hooks, `use()` can be called conditionally inside `if` statements and loops. When called with a Promise, it integrates directly with React Suspense.
</details>

<details>
<summary>4. How does `useActionState` simplify form handling in React 19?</summary>

`useActionState` manages the lifecycle of an asynchronous action function. It returns `[state, formAction, isPending]`. It handles tracking the pending transition state and updates component state with the action's returned value without needing manual `useState` and `try/catch` boilerplate.
</details>

<details>
<summary>5. How do you type an initial state callback for `useState`?</summary>

Pass a function that returns the state type: `useState<ExpensiveData>(() => computeExpensiveData())`. TypeScript ensures `computeExpensiveData` returns `ExpensiveData`.
</details>

<details>
<summary>6. How do you ensure a `useReducer` action is exhaustively handled in the reducer?</summary>

By routing the `default:` branch in the reducer's `switch (action.type)` to a variable or function expecting `never`:
```typescript
default: {
  const _unhandled: never = action;
  throw new Error(`Unhandled action: ${_unhandled}`);
}
```
</details>

<details>
<summary>7. What is the difference between `useMemo` and `useCallback`?</summary>

`useMemo(() => compute(), deps)` caches the **result** of invoking a function. `useCallback(fn, deps)` caches the **function instance itself**. In fact, `useCallback(fn, deps)` is equivalent to `useMemo(() => fn, deps)`.
</details>

<details>
<summary>8. How do you type `useOptimistic` in React 19?</summary>

`useOptimistic<TState, TUpdate>(initialState, (current: TState, update: TUpdate) => nextState)`. It enforces that the optimistic reducer maps current state and update payload to the same state shape.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Typed Boolean Toggle Hook</summary>

**Problem:** Implement a custom `useToggle(initial?: boolean)` hook returning `[boolean, () => void]` with `as const`.

**Solution:**
```typescript
import { useState, useCallback } from 'react';

export function useToggle(initial = false) {
  const [value, setValue] = useState(initial);
  const toggle = useCallback(() => setValue((v) => !v), []);
  return [value, toggle] as const;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Typed Previous Value Hook</summary>

**Problem:** Create a `usePrevious<T>(value: T): T | undefined` hook that stores the value from the previous render.

**Solution:**
```typescript
import { useRef, useEffect } from 'react';

export function usePrevious<T>(value: T): T | undefined {
  const ref = useRef<T | undefined>(undefined);
  useEffect(() => {
    ref.current = value;
  }, [value]);
  return ref.current;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Strongly-Typed LocalStorage State Hook</summary>

**Problem:** Write a `useLocalStorage<T>(key: string, initial: T)` hook that serializes and deserializes JSON with type safety.

**Solution:**
```typescript
import { useState, useEffect } from 'react';

export function useLocalStorage<T>(key: string, initial: T) {
  const [stored, setStored] = useState<T>(() => {
    try {
      const item = window.localStorage.getItem(key);
      return item ? JSON.parse(item) : initial;
    } catch {
      return initial;
    }
  });

  useEffect(() => {
    try {
      window.localStorage.setItem(key, JSON.stringify(stored));
    } catch (e) {
      console.error(e);
    }
  }, [key, stored]);

  return [stored, setStored] as const;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Debounced Value Hook with Cleanup</summary>

**Problem:** Write a `useDebounce<T>(value: T, delayMs: number): T` hook that debounces updates to a value.

**Solution:**
```typescript
import { useState, useEffect } from 'react';

export function useDebounce<T>(value: T, delayMs: number): T {
  const [debounced, setDebounced] = useState<T>(value);

  useEffect(() => {
    const handler = setTimeout(() => setDebounced(value), delayMs);
    return () => clearTimeout(handler);
  }, [value, delayMs]);

  return debounced;
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Async Data Fetching Hook with Cancellation</summary>

**Problem:** Implement `useAsync<T>(fn: (signal: AbortSignal) => Promise<T>)` that tracks `{ data, error, isLoading }` and cancels in-flight requests on dependency change.

**Solution:**
```typescript
import { useState, useEffect } from 'react';

export function useAsync<T>(fetcher: (signal: AbortSignal) => Promise<T>) {
  const [data, setData] = useState<T | null>(null);
  const [error, setError] = useState<Error | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const controller = new AbortController();
    setIsLoading(true);

    fetcher(controller.signal)
      .then((res) => { setData(res); setError(null); })
      .catch((err) => {
        if (err.name !== 'AbortError') setError(err);
      })
      .finally(() => setIsLoading(false));

    return () => controller.abort();
  }, [fetcher]);

  return { data, error, isLoading };
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe State Machine Hook</summary>

**Problem:** Write a `useStateMachine<S extends string, E extends string>` hook that accepts a transition table and enforces valid transitions at runtime and compile-time.

**Solution:**
```typescript
import { useState, useCallback } from 'react';

export function useStateMachine<S extends string, E extends string>(
  initial: S,
  transitions: Record<S, Partial<Record<E, S>>>
) {
  const [state, setState] = useState<S>(initial);

  const send = useCallback((event: E) => {
    setState((curr) => {
      const next = transitions[curr]?.[event];
      return next ?? curr;
    });
  }, [transitions]);

  return [state, send] as const;
}
```
</details>
