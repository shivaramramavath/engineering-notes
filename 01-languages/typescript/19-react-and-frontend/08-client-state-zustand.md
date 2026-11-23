# Client State Management with Zustand in TypeScript

> Typing Zustand state stores in TypeScript: create with interface definitions, curried create pattern, sliced pattern, middleware typing (devtools, persist, immer), and atomic selectors.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [19-react-and-frontend/02-hooks.md](./02-hooks.md) |
| **TypeScript Baseline** | TS 5.0+, Zustand v4/v5 |
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

While server state (APIs, databases) belongs in TanStack Query, client-only interactive state (sidebar collapsed state, audio playback position, current drawing tool, active tabs) requires a fast, lightweight, and type-safe local state manager.

Redux historically required hundreds of lines of boilerplate: action types, action creators, reducers, and provider wrappers. React Context triggers re-renders on all consumers whenever any slice changes.

**Zustand** is a minimalist, un-opinionated state management solution based on external store subscribers. In TypeScript, Zustand provides:
- **Curried `create<T>()` Pattern**: Perfect type inference for actions and state without type casting.
- **Slice Pattern**: Dividing large stores into modular, independently typed sub-slices.
- **Atomic Selectors**: Components subscribe only to the exact slice of state they consume (`useStore(state => state.isOpen)`), preventing re-renders when unrelated properties change.

```
+-----------------------------------------------------------+
|                   Zustand Store Architecture              |
|                                                           |
|  create<UserState>()((set) => ({                          |
|    username: 'Alice',                                     |
|    setUsername: (name) => set({ username: name })        |
|  }))                                                      |
|                                                           |
|  In Component:                                            |
|  const username = useStore((s) => s.username);            |
|  // Component ONLY re-renders when username changes!      |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. The Curried `create` Syntax
In TypeScript, you should always use the **curried version** of `create`:
```typescript
import { create } from 'zustand';

// Notice the double parentheses ()()!
export const useStore = create<MyState>()((set) => ({
  count: 0,
  inc: () => set((state) => ({ count: state.count + 1 }))
}));
```
Why? Calling `create<MyState>()` with empty arguments first allows TypeScript to bind the state interface before evaluating the initializer function, allowing `set` and `get` parameters to be typed properly.

### 2. Slices Pattern for Large Stores
Breaking a mega-store into modular feature slices:
```typescript
import type { StateCreator } from 'zustand';

export interface BearSlice { bears: number; addBear: () => void; }
export interface FishSlice { fishes: number; addFish: () => void; }

export type AppStore = BearSlice & FishSlice;

export const createBearSlice: StateCreator<AppStore, [], [], BearSlice> = (set) => ({
  bears: 0,
  addBear: () => set((state) => ({ bears: state.bears + 1 }))
});
```

### 3. Middleware Typing (Persist, Devtools, Immer)
When applying middleware, TypeScript middleware types must be passed to `StateCreator`:
```typescript
import { create } from 'zustand';
import { devtools, persist } from 'zustand/middleware';

export const useStore = create<AppState>()(
  devtools(
    persist(
      (set) => ({ ... }),
      { name: 'app-storage' }
    )
  )
);
```

---

## 3. How It Works Internally

Zustand maintains an external store outside React's virtual DOM via `useSyncExternalStore`:
1. The store holds a plain JavaScript object.
2. When `set()` is called, Zustand shallow-merges the state update.
3. It iterates over all listener subscriptions created by `useSyncExternalStore`.
4. If the selected value (`selector(currentState) !== selector(prevState)`) changes according to `Object.is`, React re-renders that component.

---

## 4. Syntax & API Reference

### Full Slices Creator Helper
```typescript
import { create, type StateCreator } from 'zustand';

export type Slice<TState, TSlice> = StateCreator<TState, [], [], TSlice>;
```

---

## 5. Practical Examples

### Example 1: Basic Curried Store with Actions
```typescript
import { create } from 'zustand';

interface UIState {
  isSidebarOpen: boolean;
  theme: 'light' | 'dark';
  toggleSidebar: () => void;
  setTheme: (theme: 'light' | 'dark') => void;
}

export const useUIStore = create<UIState>()((set) => ({
  isSidebarOpen: false,
  theme: 'light',
  toggleSidebar: () => set((s) => ({ isSidebarOpen: !s.isSidebarOpen })),
  setTheme: (theme) => set({ theme })
}));

// Usage in component with atomic selector:
export function SidebarToggle() {
  const isOpen = useUIStore((s) => s.isSidebarOpen);
  const toggle = useUIStore((s) => s.toggleSidebar);

  return <button onClick={toggle}>{isOpen ? 'Close' : 'Open'}</button>;
}
```

### Example 2: Real-World Persisted Auth Token Store with Immer
```typescript
import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import { immer } from 'zustand/middleware/immer';

interface UserProfile { name: string; email: string; }

interface AuthStore {
  user: UserProfile | null;
  token: string | null;
  setAuth: (user: UserProfile, token: string) => void;
  updateEmail: (email: string) => void;
  logout: () => void;
}

export const useAuthStore = create<AuthStore>()(
  persist(
    immer((set) => ({
      user: null,
      token: null,
      setAuth: (user, token) =>
        set((state) => {
          state.user = user;
          state.token = token;
        }),
      updateEmail: (email) =>
        set((state) => {
          if (state.user) state.user.email = email;
        }),
      logout: () =>
        set((state) => {
          state.user = null;
          state.token = null;
        })
    })),
    {
      name: 'auth-storage',
      storage: createJSONStorage(() => localStorage)
    }
  )
);
```

### Example 3: Edge Case - Asynchronous Action inside Zustand
```typescript
interface AsyncDataStore {
  items: string[];
  isLoading: boolean;
  error: string | null;
  fetchItems: () => Promise<void>;
}

export const useAsyncStore = create<AsyncDataStore>()((set) => ({
  items: [],
  isLoading: false,
  error: null,
  fetchItems: async () => {
    set({ isLoading: true, error: null });
    try {
      const res = await fetch('/api/items');
      const items = await res.json();
      set({ items, isLoading: false });
    } catch (err) {
      set({ error: (err as Error).message, isLoading: false });
    }
  }
}));
```

### Example 4: Failure Case - Destructuring Without Selector Causing Whole-Store Re-renders
```typescript
// ANTI-PATTERN:
// const { isSidebarOpen } = useUIStore(); // Subscribes to the WHOLE store! Rerenders on ANY state change!

// CORRECT PATTERN:
// const isSidebarOpen = useUIStore(s => s.isSidebarOpen); // Only rerenders when isSidebarOpen changes!
```

### Example 5: Idiomatic Composing Slices
```typescript
import { create, type StateCreator } from 'zustand';

interface UserSlice { username: string; setUsername: (name: string) => void; }
interface CartSlice { cartCount: number; addCart: () => void; }

type RootStore = UserSlice & CartSlice;

const createUserSlice: StateCreator<RootStore, [], [], UserSlice> = (set) => ({
  username: 'Guest',
  setUsername: (name) => set({ username: name })
});

const createCartSlice: StateCreator<RootStore, [], [], CartSlice> = (set) => ({
  cartCount: 0,
  addCart: () => set((s) => ({ cartCount: s.cartCount + 1 }))
});

export const useRootStore = create<RootStore>()((...a) => ({
  ...createUserSlice(...a),
  ...createCartSlice(...a)
}));
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Destructuring Without Selectors**:
   Writing `const { a, b } = useStore()` subscribes the component to the entire store object reference. Whenever *any* state in the store updates (even `c`), the component re-renders! Always use atomic selectors: `const a = useStore(s => s.a)`.
2. **Returning Object Literals in Selectors**:
   Writing `const { a, b } = useStore(s => ({ a: s.a, b: s.b }))` creates a new object literal on every state update, triggering a re-render every time. Use shallow equality: `useStore(s => ({ a: s.a, b: s.b }), useShallow)`.
3. **Immer Middleware Mutability**:
   When using `immer`, `set` allows direct draft mutations (`state.count++`). If not using `immer`, direct mutations will fail to notify subscribers because Zustand relies on shallow object equality.
4. **Hydration Mismatch with `persist` in Next.js**:
   When persisting to `localStorage`, the server renders the initial default state while the client hydrates with stored state. Use a custom `useHydration()` hook or `skipHydration: true` to avoid React hydration errors.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Non-Curried `create`** | `create<State>((set) => ...)` | `create<State>()((set) => ...)` | `set` parameters lose accurate typing |
| **Destructuring Store Root** | `const { count } = useStore()` | `const count = useStore(s => s.count)` | Component re-renders on every store change |
| **New Object in Selector** | `useStore(s => ({ a: s.a }))` | Use `useShallow` comparator | Endless re-render loops |
| **Direct Mutation Without Immer** | `state.items.push(x)` without immer | Return `{ items: [...state.items, x] }` | State changes but UI fails to update |
| **Storing Server Cache in Zustand** | Storing raw API responses | TanStack Query for server state | Duplicates cache, retries, and invalidations |
| **Calling Store Outside React** | Writing React hooks in pure functions | `useStore.getState()` / `.setState()` | Allows reading/writing store outside components |

---

## 8. Performance & Type-Checker Complexity

- **Atomic Selectors**: Zustand uses `Object.is` to compare selector results. Selecting primitives (`number`, `string`, `boolean`) means components re-render ONLY when that exact primitive changes.
- **Accessing Store Outside React**: `useStore.getState()` and `useStore.setState()` can be called in non-React code (e.g. Axios interceptors or WebSocket event listeners) with zero render overhead.

---

## 9. Compiler Internals

When calling `create<T>()`:
The outer function returns a curried factory `(initializer: StateCreator<T>) => UseBoundStore<StoreApi<T>>`.
Because `T` is already locked in during the first call, TypeScript does not attempt to infer `T` from the return value of `initializer`, preventing circular type inference errors when actions reference `set`.

---

## 10. Best Practices & Design Guidelines

1. **Always Use Curried `create<T>()(...)`**: Guarantees accurate types for `set`, `get`, and middleware.
2. **Use Atomic Selectors with `useShallow`**: Prevent unnecessary component re-renders.
3. **Use Slice Pattern for Scalability**: Modularize stores as the application expands.
4. **Reserve Zustand for Client State**: Let TanStack Query manage network requests.

---

## 11. Real-World Case Study

### Interactive Drawing Canvas State Store
Managing drawing tool selection, active colors, stroke widths, and undo/redo history:

```typescript
import { create } from 'zustand';

export type ToolType = 'brush' | 'eraser' | 'rectangle';

export interface CanvasState {
  currentTool: ToolType;
  strokeColor: string;
  strokeWidth: number;
  history: string[]; // Base64 snapshots
  setTool: (tool: ToolType) => void;
  setColor: (color: string) => void;
  setWidth: (width: number) => void;
  pushHistory: (snapshot: string) => void;
  undo: () => string | null;
}

export const useCanvasStore = create<CanvasState>()((set, get) => ({
  currentTool: 'brush',
  strokeColor: '#000000',
  strokeWidth: 4,
  history: [],
  setTool: (currentTool) => set({ currentTool }),
  setColor: (strokeColor) => set({ strokeColor }),
  setWidth: (strokeWidth) => set({ strokeWidth }),
  pushHistory: (snapshot) =>
    set((s) => ({ history: [...s.history.slice(-20), snapshot] })),
  undo: () => {
    const { history } = get();
    if (history.length <= 1) return null;
    const nextHistory = history.slice(0, -1);
    set({ history: nextHistory });
    return nextHistory[nextHistory.length - 1];
  }
}));
```

---

## 12. Interview Questions

<details>
<summary>1. Why does Zustand recommend the curried `create<T>()(...)` syntax in TypeScript?</summary>

In TypeScript, generic type arguments cannot be partially inferred. If you write `create<T>((set) => ...)`, the compiler tries to type `set` based on `T`, but TypeScript's parser struggles to infer function arguments when generics are supplied simultaneously on the outer call. Currying `create<T>()` locks in `T` first, allowing the returned function to type `set`, `get`, and middleware with 100% precision.
</details>

<details>
<summary>2. How do atomic selectors in Zustand prevent unnecessary re-renders?</summary>

When a component calls `useStore((state) => state.username)`, Zustand registers a listener using `useSyncExternalStore`. When any store update occurs, Zustand executes the selector function. If the selected value (`state.username`) is identical (`Object.is`) to its previous value, React skips re-rendering the component.
</details>

<details>
<summary>3. What is the `useShallow` hook in Zustand v5?</summary>

When a selector returns an object or array (e.g. `useStore(s => [s.a, s.b])`), a new reference is created on every store update, causing a re-render. `useShallow` performs a shallow equality check on the returned object/array properties, preventing re-renders if the individual elements did not change.
</details>

<details>
<summary>4. How can you read or update a Zustand store outside of React components?</summary>

Using `useStore.getState()` and `useStore.setState()`. Because Zustand stores exist as vanilla JavaScript objects independent of React, they can be called in pure functions, Axios interceptors, or background workers.
</details>

<details>
<summary>5. How does the Slice pattern work in Zustand?</summary>

The Slice pattern breaks a monolithic store into smaller functions of type `StateCreator<RootStore, [], [], SubSlice>`. Each slice creator receives `set`, `get`, and returns its slice properties. The root store merges all slices together in a single `create<RootStore>()` definition.
</details>

<details>
<summary>6. How do you persist Zustand state to `localStorage` safely in Next.js?</summary>

Wrap the store in the `persist` middleware. Because `localStorage` is not available during Next.js server-side rendering, wrap component reads in a custom `useHydrated()` hook or use `skipHydration: true` with manual hydration in `useEffect`.
</details>

<details>
<summary>7. What is the difference between Zustand and React Context?</summary>

React Context re-renders all consuming components whenever the context value reference changes. Zustand supports fine-grained selector subscriptions so components re-render only when their specific selected property changes. In addition, Zustand stores can be accessed outside the React component tree.
</details>

<details>
<summary>8. How do you type the `immer` middleware in Zustand?</summary>

Wrap the store initializer with `immer(...)`. Inside the updater, `set` provides a mutable Draft object where properties can be mutated directly without manual object spreading.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Theme Store</summary>

**Problem:** Create a simple theme store with `theme: 'light' | 'dark'` and `toggleTheme: () => void`.

**Solution:**
```typescript
import { create } from 'zustand';

interface ThemeState {
  theme: 'light' | 'dark';
  toggleTheme: () => void;
}

export const useThemeStore = create<ThemeState>()((set) => ({
  theme: 'light',
  toggleTheme: () => set((s) => ({ theme: s.theme === 'light' ? 'dark' : 'light' }))
}));
```
</details>

<details>
<summary>Exercise 2 (Easy): Store Reader Outside React</summary>

**Problem:** Write a function `getAuthToken()` that retrieves the current token from an auth store without using a hook.

**Solution:**
```typescript
interface AuthStore { token: string | null; }
declare const useAuthStore: { getState: () => AuthStore };

export function getAuthToken(): string | null {
  return useAuthStore.getState().token;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Sliced State Creator</summary>

**Problem:** Type an independent `CounterSlice` containing `count: number` and `increment: () => void` intended for a larger `RootStore`.

**Solution:**
```typescript
import type { StateCreator } from 'zustand';

export interface CounterSlice {
  count: number;
  increment: () => void;
}

export const createCounterSlice: StateCreator<CounterSlice, [], [], CounterSlice> = (set) => ({
  count: 0,
  increment: () => set((s) => ({ count: s.count + 1 }))
});
```
</details>

<details>
<summary>Exercise 4 (Medium): Shallow Selector Hook</summary>

**Problem:** Use `useShallow` from `zustand/react/shallow` to select multiple properties `{ x, y }` without infinite re-renders.

**Solution:**
```typescript
import { useShallow } from 'zustand/react/shallow';

declare const useCoordinateStore: <T>(sel: (s: any) => T, eq?: any) => T;

export function useCoordinates() {
  return useCoordinateStore(
    useShallow((state: { x: number; y: number }) => ({ x: state.x, y: state.y }))
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Reset All Stores Utility Pattern</summary>

**Problem:** Implement a store registry pattern that allows resetting all registered Zustand stores to their initial states simultaneously (e.g. on user logout).

**Solution:**
```typescript
const storeResetters = new Set<() => void>();

export function createResettableStore<T extends object>(
  initializer: (set: any, get: any, api: any) => T
) {
  let initial: T;
  const store = create<T>()((set, get, api) => {
    initial = initializer(set, get, api);
    return initial;
  });

  storeResetters.add(() => store.setState(initial, true));
  return store;
}

export function resetAllStores() {
  storeResetters.forEach((reset) => reset());
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Persistent Typed Action History Stack</summary>

**Problem:** Create a Zustand store that maintains an undo/redo stack of actions with full TypeScript typing.

**Solution:**
```typescript
import { create } from 'zustand';

interface HistoryStore<T> {
  present: T;
  past: T[];
  future: T[];
  setPresent: (val: T) => void;
  undo: () => void;
  redo: () => void;
}

export function makeHistoryStore<T>(initial: T) {
  return create<HistoryStore<T>>()((set, get) => ({
    present: initial,
    past: [],
    future: [],
    setPresent: (val) => set((s) => ({ past: [...s.past, s.present], present: val, future: [] })),
    undo: () => {
      const { past, present, future } = get();
      if (past.length === 0) return;
      const prev = past[past.length - 1];
      set({ past: past.slice(0, -1), present: prev, future: [present, ...future] });
    },
    redo: () => {
      const { past, present, future } = get();
      if (future.length === 0) return;
      const next = future[0];
      set({ past: [...past, present], present: next, future: future.slice(1) });
    }
  }));
}
```
</details>
