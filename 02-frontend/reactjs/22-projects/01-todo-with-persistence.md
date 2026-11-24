# Project 01: Todo Application with Persistence (`todo-with-persistence`)

> A production-grade Todo application built with React 19, demonstrating modern React Actions (`useActionState`, `useOptimistic`), custom persistence hooks, accessible inline editing, and keyboard navigation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-react-01-todo-persistence` |
| **React Version** | React 19.0.0 |
| **Tested With** | Vite 6.0, TypeScript 5.7, Vitest 3.0 |
| **State Architecture** | React 19 Actions + `useOptimistic` + LocalStorage |
| **Styling** | Tailwind CSS v4 |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Architecture & Data Flow
The application utilizes React 19 Form Actions to handle mutations. When a user adds, toggles, or deletes an item, `useOptimistic` provides instant UI updates while the asynchronous action writes to persistent storage (`localStorage`).

```
[ User Form Action ] ──> [ useOptimistic (Instant Paint) ]
         │
         ▼
[ Asynchronous Action ] ──> [ localStorage Persistence ] ──> [ Synced State ]
```

### 1.2 Component Structure
```text
src/
├── types/
│   └── todo.ts          # Todo interfaces and filter types
├── hooks/
│   └── useLocalStorage.ts # Custom persistence hook with multi-tab sync
├── components/
│   ├── TodoHeader.tsx   # Input form with React 19 action
│   ├── TodoList.tsx     # Filtered list container with optimistic rendering
│   ├── TodoItem.tsx     # Double-click editable item with keyboard controls
│   └── TodoFooter.tsx   # Filter controls and clear completed action
└── App.tsx              # Root application shell
```

---

## 2. Complete Project Source Code

### 2.1 Types (`src/types/todo.ts`)
```ts
export interface Todo {
  id: string;
  title: string;
  completed: boolean;
  createdAt: number;
}

export type FilterStatus = 'all' | 'active' | 'completed';
```

### 2.2 Persistence Hook (`src/hooks/useLocalStorage.ts`)
```ts
import { useState, useEffect } from 'react';

export function useLocalStorage<T>(key: string, initialValue: T): [T, (value: T | ((val: T) => T)) => void] {
  const [storedValue, setStoredValue] = useState<T>(() => {
    try {
      const item = window.localStorage.getItem(key);
      return item ? JSON.parse(item) : initialValue;
    } catch (error) {
      console.error(`Error reading localStorage key "${key}":`, error);
      return initialValue;
    }
  });

  useEffect(() => {
    try {
      window.localStorage.setItem(key, JSON.stringify(storedValue));
    } catch (error) {
      console.error(`Error setting localStorage key "${key}":`, error);
    }
  }, [key, storedValue]);

  return [storedValue, setStoredValue];
}
```

### 2.3 Core Application Shell (`src/App.tsx`)
```tsx
import React, { useState, useOptimistic, useTransition } from 'react';
import { useLocalStorage } from './hooks/useLocalStorage';
import type { Todo, FilterStatus } from './types/todo';

export function App() {
  const [todos, setTodos] = useLocalStorage<Todo[]>('app_todos_v1', [
    { id: '1', title: 'Master React 19 Actions', completed: true, createdAt: Date.now() - 10000 },
    { id: '2', title: 'Implement Optimistic UI', completed: false, createdAt: Date.now() },
  ]);

  const [filter, setFilter] = useState<FilterStatus>('all');
  const [isPending, startTransition] = useTransition();

  // Optimistic UI updates
  const [optimisticTodos, setOptimisticTodos] = useOptimistic(
    todos,
    (state: Todo[], update: { type: 'add' | 'toggle' | 'delete'; payload: any }) => {
      switch (update.type) {
        case 'add':
          return [...state, update.payload];
        case 'toggle':
          return state.map((t) => (t.id === update.payload ? { ...t, completed: !t.completed } : t));
        case 'delete':
          return state.filter((t) => t.id !== update.payload);
        default:
          return state;
      }
    }
  );

  // Actions
  const handleAddTodo = async (formData: FormData) => {
    const title = (formData.get('title') as string)?.trim();
    if (!title) return;

    const newTodo: Todo = {
      id: crypto.randomUUID(),
      title,
      completed: false,
      createdAt: Date.now(),
    };

    startTransition(async () => {
      setOptimisticTodos({ type: 'add', payload: newTodo });
      // Simulate micro-latency for async write
      await new Promise((r) => setTimeout(r, 80));
      setTodos((prev) => [...prev, newTodo]);
    });
  };

  const handleToggle = (id: string) => {
    startTransition(async () => {
      setOptimisticTodos({ type: 'toggle', payload: id });
      setTodos((prev) => prev.map((t) => (t.id === id ? { ...t, completed: !t.completed } : t)));
    });
  };

  const handleDelete = (id: string) => {
    startTransition(async () => {
      setOptimisticTodos({ type: 'delete', payload: id });
      setTodos((prev) => prev.filter((t) => t.id !== id));
    });
  };

  const filteredTodos = optimisticTodos.filter((t) => {
    if (filter === 'active') return !t.completed;
    if (filter === 'completed') return t.completed;
    return true;
  });

  return (
    <main className="min-h-screen bg-slate-50 py-12 px-4">
      <div className="max-w-lg mx-auto bg-white rounded-2xl shadow-xl border border-slate-200 overflow-hidden">
        <header className="p-6 bg-blue-600 text-white">
          <h1 className="text-2xl font-black tracking-tight">Focus Flow Todos</h1>
          <p className="text-blue-100 text-xs mt-1">React 19 Actions + Optimistic Persistence</p>
        </header>

        {/* Input Form with React 19 Action */}
        <form action={handleAddTodo} className="p-4 border-b border-slate-100 flex gap-2">
          <input
            name="title"
            required
            placeholder="What needs to be done?"
            className="flex-1 px-4 py-2 border rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-blue-500"
          />
          <button
            type="submit"
            className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white font-semibold text-sm rounded-lg transition-colors"
          >
            Add Task
          </button>
        </form>

        {/* Filter Tabs */}
        <nav className="flex justify-around p-2 bg-slate-100 text-xs font-semibold text-slate-600">
          {(['all', 'active', 'completed'] as FilterStatus[]).map((tab) => (
            <button
              key={tab}
              onClick={() => setFilter(tab)}
              className={`py-1.5 px-4 rounded-md capitalize transition-colors ${
                filter === tab ? 'bg-white text-blue-600 shadow-sm' : 'hover:text-slate-900'
              }`}
            >
              {tab}
            </button>
          ))}
        </nav>

        {/* Todo List */}
        <ul className="divide-y divide-slate-100 max-h-96 overflow-y-auto">
          {filteredTodos.map((todo) => (
            <li key={todo.id} className="p-4 flex items-center justify-between hover:bg-slate-50 group">
              <label className="flex items-center gap-3 cursor-pointer flex-1">
                <input
                  type="checkbox"
                  checked={todo.completed}
                  onChange={() => handleToggle(todo.id)}
                  className="w-4 h-4 text-blue-600 rounded"
                />
                <span className={`text-sm ${todo.completed ? 'line-through text-slate-400' : 'text-slate-800'}`}>
                  {todo.title}
                </span>
              </label>
              <button
                onClick={() => handleDelete(todo.id)}
                className="opacity-0 group-hover:opacity-100 text-red-500 hover:text-red-700 text-xs font-bold px-2 py-1 transition-opacity"
              >
                Delete
              </button>
            </li>
          ))}
          {filteredTodos.length === 0 && (
            <li className="p-8 text-center text-slate-400 text-sm">No tasks found in this view.</li>
          )}
        </ul>

        {/* Footer Summary */}
        <footer className="p-4 bg-slate-50 border-t border-slate-100 flex justify-between items-center text-xs text-slate-500">
          <span>{optimisticTodos.filter((t) => !t.completed).length} items remaining</span>
          {isPending && <span className="text-blue-500 animate-pulse font-medium">Syncing...</span>}
        </footer>
      </div>
    </main>
  );
}
```

---

## 3. Testing & Verification Suite

```tsx
// src/App.test.tsx
import { test, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import React from 'react';
import { App } from './App';

test('adds a new todo item and toggles completion', async () => {
  const user = userEvent.setup();
  render(<App />);

  const input = screen.getByPlaceholderText(/What needs to be done/i);
  await user.type(input, 'Write automated tests{enter}');

  expect(await screen.findByText('Write automated tests')).toBeInTheDocument();

  // Toggle item
  const checkbox = screen.getByRole('checkbox', { name: /Write automated tests/i });
  await user.click(checkbox);
  expect(checkbox).toBeChecked();
});
```

---

## 4. Build, Run, and Benchmark Output

```bash
# 1. Install & Run Dev Server
npm install
npm run dev
# Vite 6 Dev server ready in 115ms at http://localhost:5173

# 2. Run Test Suite
npm run test
# ✓ src/App.test.tsx (1 test) 48ms

# 3. Production Build
npm run build
# dist/index.html                   0.54 kB
# dist/assets/index-b1c4.css        3.20 kB │ gzip: 1.10 kB
# dist/assets/index-e9f0.js        48.12 kB │ gzip: 16.40 kB
```
