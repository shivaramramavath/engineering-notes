# Redux Toolkit (RTK) 2 — Basic to Advanced

A comprehensive reference card for `@reduxjs/toolkit` v2 and `react-redux` v9 in React 19.

> **Target Version:** Redux Toolkit v2.5+ / React-Redux v9.2+  
> **Philosophy:** Opinionated, batteries-included standard for Redux  
> **Key Innovations:** Built-in Immer, `createSlice`, `createEntityAdapter`, listener middleware  

---

## Level 0 — Concepts

- **Predictable Unidirectional Data Flow**: State is read-only. The only way to mutate state is by dispatching an `action` object to a pure `reducer` function.
- **Built-in Immer**: Redux Toolkit wraps reducers in Immer's `produce()`, allowing you to write intuitive "mutating" syntax (`state.todos.push(item)`) that compiles safely to immutable updates.
- **Core Primitives Table**:

| API | Type | Purpose |
| :--- | :--- | :--- |
| `configureStore()` | Function | Creates Redux store with default thunk and devtools middleware |
| `createSlice()` | Function | Generates action creators and reducer in a unified declaration |
| `createEntityAdapter()`| Function | Manages normalized collections (`ids` array and `entities` dictionary) |
| `useAppDispatch()` | Hook | Strongly typed `useDispatch` hook |
| `useAppSelector()` | Hook | Strongly typed `useSelector` hook |
| `createListenerMiddleware()`| Function | Lightweight side-effect listener responding to actions |

---

## Level 1 — Basic Setup & Usage

### 1.1 Installation
```bash
npm install @reduxjs/toolkit react-redux
```

### 1.2 Defining a Slice and Configuring Store
```ts
// src/store/counterSlice.ts
import { createSlice, type PayloadAction } from '@reduxjs/toolkit';

interface CounterState {
  value: number;
}

const initialState: CounterState = { value: 0 };

export const counterSlice = createSlice({
  name: 'counter',
  initialState,
  reducers: {
    increment: (state) => {
      state.value += 1; // Immer allows "mutating" logic!
    },
    incrementByAmount: (state, action: PayloadAction<number>) => {
      state.value += action.payload;
    },
  },
});

export const { increment, incrementByAmount } = counterSlice.actions;
```

```ts
// src/store/store.ts
import { configureStore } from '@reduxjs/toolkit';
import { counterSlice } from './counterSlice';

export const store = configureStore({
  reducer: {
    counter: counterSlice.reducer,
  },
});

export type RootState = ReturnType<typeof store.getState>;
export type AppDispatch = typeof store.dispatch;
```

### 1.3 Typed Hooks
```ts
// src/store/hooks.ts
import { useDispatch, useSelector, type TypedUseSelectorHook } from 'react-redux';
import type { RootState, AppDispatch } from './store';

export const useAppDispatch: () => AppDispatch = useDispatch;
export const useAppSelector: TypedUseSelectorHook<RootState> = useSelector;
```

---

## Level 2 — Intermediate: Normalized Collections with `createEntityAdapter`

```ts
import { createEntityAdapter, createSlice } from '@reduxjs/toolkit';

interface Book {
  id: string;
  title: string;
}

const booksAdapter = createEntityAdapter<Book>();

export const booksSlice = createSlice({
  name: 'books',
  initialState: booksAdapter.getInitialState(),
  reducers: {
    bookAdded: booksAdapter.addOne,
    booksReceived: booksAdapter.setAll,
    bookRemoved: booksAdapter.removeOne,
  },
});

// Generates memoized selectors automatically!
export const { selectAll: selectAllBooks, selectById: selectBookById } =
  booksAdapter.getSelectors((state: { books: any }) => state.books);
```

---

## Level 3 — Advanced: Listener Middleware

```ts
import { createListenerMiddleware } from '@reduxjs/toolkit';
import { counterSlice } from './counterSlice';

export const listenerMiddleware = createListenerMiddleware();

listenerMiddleware.startListening({
  actionCreator: counterSlice.actions.increment,
  effect: async (action, listenerApi) => {
    console.log('Counter incremented! Action:', action);
    // Fetch background analytics or dispatch secondary actions
  },
});
```

---

## Level 4 — Production Checklist & Anti-Patterns

| Anti-Pattern | Why It Breaks | Recommended Practice |
| :--- | :--- | :--- |
| **Storing Non-Serializable Values in Redux** | Storing Promises, Class Instances, or DOM nodes breaks DevTools time-travel and serialization. | Store strictly plain JSON objects, arrays, and primitives. |
| **Using Untyped `useSelector` and `useDispatch`** | Loses autocompletion and type-safety across actions and state trees. | Always use pre-typed `useAppSelector` and `useAppDispatch`. |
| **Mutating State Outside `createSlice` Reducers** | Bypasses Immer, silently corrupting state history. | Only use "mutating" syntax directly inside RTK slice reducers. |
| **Deeply Nested State Objects** | Complicates update logic and causes unnecessary component re-renders. | Normalize state into flat dictionaries using `createEntityAdapter`. |
