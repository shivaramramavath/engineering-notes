# Zustand v5 Cheatsheet

> Quick-reference guide for creating stores, writing slice selectors, middleware (persist, devtools, immer), and actions.

---

## 1. Creating a Basic Store

```ts
import { create } from 'zustand';

interface CounterState {
  count: number;
  increment: () => void;
  decrement: () => void;
  reset: () => void;
}

export const useCounterStore = create<CounterState>()((set) => ({
  count: 0,
  increment: () => set((state) => ({ count: state.count + 1 })),
  decrement: () => set((state) => ({ count: state.count - 1 })),
  reset: () => set({ count: 0 }),
}));
```

---

## 2. Consuming Store with Slice Selectors

```tsx
// Fine-grained subscription: Only re-renders when count changes!
const count = useCounterStore((state) => state.count);

// Action selector: NEVER causes a re-render because functions are stable!
const increment = useCounterStore((state) => state.increment);

// Shallow equality for multi-property extraction
import { useShallow } from 'zustand/react/shallow';

const { count, reset } = useCounterStore(
  useShallow((state) => ({ count: state.count, reset: state.reset }))
);
```

---

## 3. Middleware: Persistent Storage

```ts
import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';

interface AuthState {
  token: string | null;
  setToken: (token: string | null) => void;
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set) => ({
      token: null,
      setToken: (token) => set({ token }),
    }),
    {
      name: 'auth-session-storage',
      storage: createJSONStorage(() => sessionStorage),
    }
  )
);
```
