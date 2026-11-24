# Zustand v5 — Basic to Advanced

A comprehensive reference card for `zustand` v5 in React 19.

> **Target Version:** Zustand v5.0+  
> **Philosophy:** Minimal, unopinionated, hooks-based global store without providers  
> **Reactivity:** Uses `useSyncExternalStore` for 100% Concurrent React safety  

---

## Level 0 — Concepts

- **Provider-less Architecture**: A Zustand store is created via `create()`. It can be consumed directly anywhere in the component tree without wrapping the app in context providers.
- **Selector-Based Subscriptions**: Components subscribe to specific slices of state (`useStore(state => state.count)`). The component re-renders **only** when the selected slice changes by reference.
- **Out-of-React Access**: Zustand stores can be read and mutated outside React (e.g. in network fetch interceptors, utility files, or WebSocket callbacks) via `store.getState()` and `store.setState()`.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `create()` | Function | Factory creating a reactive hook bound to an internal store |
| `set()` | Function | Merges updates into store state |
| `get()` | Function | Reads current state synchronously inside actions |
| `useStore(selector)` | Hook | Subscribes a React component to a slice of state |
| `store.getState()` | Method | Reads store state imperatively outside React |
| `store.subscribe()` | Method | Listens for store changes outside React |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install zustand
```

### 1.2 Creating and Consuming a Store
```tsx
import { create } from 'zustand';

interface CounterState {
  count: number;
  increment: () => void;
  reset: () => void;
}

export const useCounterStore = create<CounterState>((set) => ({
  count: 0,
  increment: () => set((state) => ({ count: state.count + 1 })),
  reset: () => set({ count: 0 }),
}));

export function CounterView() {
  // Select only the count value!
  const count = useCounterStore((state) => state.count);
  const increment = useCounterStore((state) => state.increment);

  return (
    <div className="p-4 border rounded">
      <p>Count: {count}</p>
      <button onClick={increment} className="bg-blue-600 text-white px-3 py-1 rounded">
        +1
      </button>
    </div>
  );
}
```

---

## Level 2 — Intermediate: Middleware (DevTools & Persist)

```ts
import { create } from 'zustand';
import { devtools, persist } from 'zustand/middleware';

interface AuthState {
  token: string | null;
  setToken: (token: string | null) => void;
}

export const useAuthStore = create<AuthState>()(
  devtools(
    persist(
      (set) => ({
        token: null,
        setToken: (token) => set({ token }),
      }),
      { name: 'auth-storage' } // Persists automatically to localStorage!
    )
  )
);
```

---

## Level 3 — Advanced: Out-of-React Axios Interceptor Access

```ts
import axios from 'axios';
import { useAuthStore } from './authStore';

export const apiClient = axios.create();

apiClient.interceptors.request.use((config) => {
  // Read token imperatively from Zustand store without React hooks!
  const token = useAuthStore.getState().token;
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Omitting Selectors (`const state = useStore()`)**| Subscribes to the entire store; component re-renders on ANY state change anywhere in the store. | Always use granular selectors: `useStore((s) => s.specificSlice)`. |
| **Mutating State In-Place (`state.items.push()`)** | Violates immutability; Zustand relies on shallow reference equality, skipping updates. | Use `immer` middleware or return new copies: `{ items: [...state.items, x] }`. |
| **Using Zustand for Server-Side Data Fetching** | Lacks built-in background refetching, request deduplication, and retry logic. | Use TanStack Query for server state; use Zustand strictly for client UI state. |
| **Destructuring Action Functions from Whole Store** | Triggers unnecessary re-renders. | Select individual actions or define them outside the reactive slice. |
