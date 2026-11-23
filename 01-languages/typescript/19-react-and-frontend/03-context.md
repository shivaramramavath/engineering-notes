# React Context API Typing and Custom Provider Patterns in TypeScript

> Typing React Context in TypeScript: createContext defaults, nullable contexts vs custom hook guards, Context as a Provider in React 19, modular providers, and selector optimization.

| Metadata | Details |
| :--- | :--- |
| **Category** | React and Frontend Typing |
| **Difficulty** | Intermediate |
| **Prerequisites** | [04-objects-and-interfaces/00-interfaces.md](../04-objects-and-interfaces/00-interfaces.md), [19-react-and-frontend/02-hooks.md](./02-hooks.md) |
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

The React **Context API** provides a way to pass data through the component tree without having to pass props down manually at every level ("prop drilling"). Common use cases include current authenticated user, theme settings, preferred language, and UI notification queues.

When typing React Context in TypeScript, developers face a core dilemma regarding **Initial Default Values**:
```typescript
const AuthContext = createContext<AuthContextValue>(???);
```
What default value should be passed to `createContext`?
1. **Passing a dummy mock object**: Faking dummy methods (`login: () => {}`) masks the real error if a consumer accidentally renders outside the Provider.
2. **Passing `null` or `undefined`**: Forces every consuming component to defensively check `if (!auth)` or write `auth?.user?.name` across dozens of files.

The industry-standard solution is the **Guarded Custom Hook Pattern**: initialize context with `null`, but encapsulate consumption inside a custom hook `useAuth()` that checks for `null` and throws a descriptive runtime error if outside the Provider. This narrows the returned type to `AuthContextValue` (non-null) automatically!

```
+-----------------------------------------------------------+
|              Guarded Context Architecture                 |
|                                                           |
|  createContext<AuthValue | null>(null)                    |
|                         ^                                 |
|                         |                                 |
|  export function useAuth(): AuthValue {                   |
|    const ctx = useContext(AuthContext);                  |
|    if (!ctx) throw new Error("useAuth outside Provider"); |
|    return ctx; // Inferred as non-null AuthValue!         |
|  }                                                        |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. React 19: `<Context>` Instead of `<Context.Provider>`
In React 19, you can render `<Context value={...}>` directly instead of `<Context.Provider value={...}>`:
```typescript
const ThemeContext = createContext<Theme>('light');

export function ThemeProvider({ children }: { children: ReactNode }) {
  return <ThemeContext value="dark">{children}</ThemeContext>;
}
```

### 2. Context Separation (State vs Dispatch)
When state changes in a context, every component calling `useContext` re-renders. To prevent unnecessary re-renders, separate frequently changing state from stable dispatch functions into two distinct contexts:
```typescript
const StateContext = createContext<State | null>(null);
const DispatchContext = createContext<Dispatch | null>(null);
```

---

## 3. How It Works Internally

TypeScript's control flow analysis handles the guard:
1. `useContext(MyContext)` returns `T | null`.
2. The `if (!context)` branch throws an exception.
3. Flow analysis eliminates `null` from the return type in subsequent lines, yielding pure `T`.

---

## 4. Syntax & API Reference

### Idiomatic Type-Safe Context Creation Template
```typescript
import { createContext, useContext, type ReactNode } from 'react';

export function createSafeContext<T>(name: string) {
  const Context = createContext<T | null>(null);

  function useSafeContext(): T {
    const value = useContext(Context);
    if (value === null) {
      throw new Error(`use${name} must be used within a ${name}Provider`);
    }
    return value;
  }

  return [Context, useSafeContext] as const;
}
```

---

## 5. Practical Examples

### Example 1: Basic Theme Context with Safe Hook
```typescript
import { createContext, useContext, useState, useMemo, type ReactNode } from 'react';

type Theme = 'light' | 'dark';

interface ThemeContextValue {
  theme: Theme;
  toggleTheme: () => void;
}

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [theme, setTheme] = useState<Theme>('light');

  const value = useMemo(
    () => ({
      theme,
      toggleTheme: () => setTheme((t) => (t === 'light' ? 'dark' : 'light'))
    }),
    [theme]
  );

  return <ThemeContext value={value}>{children}</ThemeContext>;
}

export function useTheme(): ThemeContextValue {
  const ctx = useContext(ThemeContext);
  if (!ctx) throw new Error('useTheme must be used within ThemeProvider');
  return ctx;
}
```

### Example 2: Real-World Authenticated User Context with Loading States
```typescript
interface User { id: string; email: string; role: 'admin' | 'member'; }

interface AuthContextValue {
  user: User | null;
  isLoading: boolean;
  login: (token: string) => Promise<void>;
  logout: () => void;
}

const AuthContext = createContext<AuthContextValue | null>(null);

export function useAuth(): AuthContextValue {
  const ctx = useContext(AuthContext);
  if (!ctx) throw new Error('useAuth must be used within AuthProvider');
  return ctx;
}
```

### Example 3: Edge Case - Split State and Dispatch Contexts
```typescript
import { createContext, useContext, useReducer, type Dispatch, type ReactNode } from 'react';

type Todo = { id: string; text: string };
type Action = { type: 'ADD'; text: string } | { type: 'REMOVE'; id: string };

const TodoStateContext = createContext<Todo[] | null>(null);
const TodoDispatchContext = createContext<Dispatch<Action> | null>(null);

function todoReducer(state: Todo[], action: Action): Todo[] {
  switch (action.type) {
    case 'ADD': return [...state, { id: crypto.randomUUID(), text: action.text }];
    case 'REMOVE': return state.filter((t) => t.id !== action.id);
  }
}

export function TodoProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(todoReducer, []);
  return (
    <TodoStateContext value={state}>
      <TodoDispatchContext value={dispatch}>{children}</TodoDispatchContext>
    </TodoStateContext>
  );
}

export function useTodoState(): Todo[] {
  const ctx = useContext(TodoStateContext);
  if (!ctx) throw new Error('useTodoState outside TodoProvider');
  return ctx;
}

export function useTodoDispatch(): Dispatch<Action> {
  const ctx = useContext(TodoDispatchContext);
  if (!ctx) throw new Error('useTodoDispatch outside TodoProvider');
  return ctx;
}
```

### Example 4: Failure Case - Using Context Outside Provider at Runtime
```typescript
function StrayButton() {
  // Throws informative Error: "useTheme must be used within ThemeProvider"!
  const { theme } = useTheme();
  return <button className={theme}>Click</button>;
}
```

### Example 5: Idiomatic Reading Context Conditionally with React 19 `use()`
```typescript
import { use } from 'react';

export function DynamicBanner({ showTheme }: { showTheme: boolean }) {
  if (!showTheme) {
    return <div>Standard Banner</div>;
  }

  // React 19 'use()' can be called inside conditionals!
  const theme = use(ThemeContext);
  return <div className={`banner-${theme?.theme}`}>Themed Banner</div>;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Unmemoized Value Prop**:
   Passing an unmemoized object literal directly to `value={{ user, login }}` creates a new object reference on every parent render. This triggers re-renders across all consumer components down the tree, even if `user` has not changed! Always wrap context value objects in `useMemo`.
2. **Context vs State Management (Redux/Zustand)**:
   Context is a dependency injection mechanism, not a high-frequency state management tool. Updating a context value re-renders every component subscribed to that context. For values updating multiple times per second (e.g. mouse positions, audio streams, real-time gaming), use Zustand or external state stores.
3. **Circular Dependencies between Context Providers**:
   Provider A needing Provider B while Provider B needs Provider A causes initialization deadlocks. Break dependencies into shared sub-contexts.
4. **Mock Values in Unit Tests**:
   Unit testing a component that uses `useAuth()` requires either wrapping the component in `<AuthProvider>` or overriding the custom hook in tests.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Fake Default Dummy Object** | `createContext({ login: () => {} })` | `createContext(null)` + guarded hook | Silent bugs when rendered outside provider |
| **Unmemoized Context Value** | `<Context.Provider value={{ a, b }}>` | `value={useMemo(() => ({ a, b }), [a, b])}` | Unnecessary re-renders across the entire tree |
| **Exporting Raw Context Object** | `export const AuthContext = ...` | Export only `AuthProvider` and `useAuth()` | Consumers bypass guard and deal with `null` |
| **Mega Global Context** | Single context with 40 unrelated states | Cohesive, focused domain contexts | Any state update re-renders entire application |
| **Missing Generic Type Parameter** | `createContext(null)` | `createContext<ContextValue \| null>(null)` | Inferred as `null`, cannot pass real value |
| **Prop Drilling Instead of Context** | Passing user prop through 8 levels | Context Provider at root | Brittle components tightly coupled to hierarchy |

---

## 8. Performance & Type-Checker Complexity

- **Re-render Propagation**: Any update to a context's `value` reference triggers a render pass for every consumer component. Splitting contexts into State and Dispatch avoids re-rendering buttons that only dispatch actions.
- **Type Checking**: Context generic types evaluate instantly without compiler friction.

---

## 9. Compiler Internals

When calling `createContext<T>(defaultValue: T)`:
The compiler records `T` as the internal type of the `React.Context<T>` object. The JSX checker verifies that `<Context.Provider value={val}>` accepts a `value` assignable to `T`.

---

## 10. Best Practices & Design Guidelines

1. **Keep Raw Context Objects Private**: Export only the `Provider` component and the `useX()` custom hook.
2. **Always Use Guarded Hooks**: Eliminate `null` checks for consumers.
3. **Memoize Context Values**: Use `useMemo` for the object passed to `value`.
4. **Separate State and Dispatch Contexts**: Optimize render performance.

---

## 11. Real-World Case Study

### Notification Toast Manager Context
A notification system providing an imperative `notify(message, type)` trigger to any component in the application:

```typescript
import { createContext, useContext, useState, useCallback, useMemo, type ReactNode } from 'react';

export interface Toast {
  id: string;
  message: string;
  type: 'info' | 'success' | 'error';
}

interface ToastContextValue {
  toasts: readonly Toast[];
  addToast: (message: string, type?: Toast['type']) => void;
  removeToast: (id: string) => void;
}

const ToastContext = createContext<ToastContextValue | null>(null);

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const removeToast = useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  const addToast = useCallback((message: string, type: Toast['type'] = 'info') => {
    const id = crypto.randomUUID();
    setToasts((prev) => [...prev, { id, message, type }]);
    setTimeout(() => removeToast(id), 5000);
  }, [removeToast]);

  const value = useMemo(
    () => ({ toasts, addToast, removeToast }),
    [toasts, addToast, removeToast]
  );

  return (
    <ToastContext value={value}>
      {children}
      <aside className="toast-container">
        {toasts.map((t) => (
          <div key={t.id} className={`toast toast-${t.type}`}>{t.message}</div>
        ))}
      </aside>
    </ToastContext>
  );
}

export function useToast(): ToastContextValue {
  const ctx = useContext(ToastContext);
  if (!ctx) throw new Error('useToast must be used within ToastProvider');
  return ctx;
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the recommended strategy for handling initial default values in TypeScript React Context?</summary>

Initialize the context with `null`: `createContext<T | null>(null)`. Then wrap `useContext` in a custom hook `useMyContext()` that checks if the context value is `null`. If `null`, it throws an error explaining that the component must be wrapped in the Provider. This eliminates `null` from the hook's return type for all consumers.
</details>

<details>
<summary>2. Why does React 19 allow `<Context value={...}>` without `.Provider`?</summary>

In React 19, the context object itself acts as the Provider component (`<MyContext value={value}>`), simplifying JSX and deprecating the redundant `.Provider` property.
</details>

<details>
<summary>3. Why should context value objects always be wrapped in `useMemo`?</summary>

If an object literal is passed to `value` without `useMemo`, a new object reference is created on every render of the provider component. This forces every consuming component that calls `useContext` to re-render, even if individual property values did not change.
</details>

<details>
<summary>4. How does splitting Context into State and Dispatch optimize performance?</summary>

Components that only dispatch actions (like a submit button) only subscribe to `DispatchContext`. Because dispatch functions (like `setCount` or `dispatch` from `useReducer`) have stable references and never change, dispatch-only components never re-render when state changes.
</details>

<details>
<summary>5. How does the React 19 `use()` hook differ from `useContext()`?</summary>

`useContext()` must adhere to the standard Rules of Hooks (must be called unconditionally at the top level of a component). `use(Context)` can be called conditionally inside `if` statements and loops.
</details>

<details>
<summary>6. When should you choose an external state management library (Zustand) over React Context?</summary>

Use React Context for low-frequency global updates (theme, user, locale). Use external state management libraries like Zustand or Redux Toolkit for high-frequency updates, state with fine-grained selector subscriptions, or complex state logic outside React components.
</details>

<details>
<summary>7. How do you test a component that consumes a context?</summary>

Wrap the component under test in the corresponding Provider inside the test:
```typescript
render(<MyComponent />, { wrapper: AuthProvider });
```
</details>

<details>
<summary>8. How do you prevent circular dependencies when two providers need each other?</summary>

Extract the shared state into a third independent provider that is rendered higher up in the component tree.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic Counter Context</summary>

**Problem:** Create a minimal CounterContext with `count: number` and `increment: () => void`.

**Solution:**
```typescript
import { createContext, useContext } from 'react';

interface CounterContextValue { count: number; increment: () => void; }
export const CounterContext = createContext<CounterContextValue | null>(null);

export function useCounter(): CounterContextValue {
  const ctx = useContext(CounterContext);
  if (!ctx) throw new Error('useCounter outside provider');
  return ctx;
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Generic Safe Context Generator</summary>

**Problem:** Implement a generic function `createSafeContext<T>(name: string)` that returns `[Context, useHook]`.

**Solution:**
```typescript
import { createContext, useContext } from 'react';

export function createSafeContext<T>(displayName: string) {
  const ctx = createContext<T | null>(null);
  const useCtx = () => {
    const val = useContext(ctx);
    if (!val) throw new Error(`use${displayName} must be inside ${displayName}Provider`);
    return val;
  };
  return [ctx, useCtx] as const;
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Typed Modal Registry Context</summary>

**Problem:** Create a ModalContext that allows opening a modal by name with typed arguments.

**Solution:**
```typescript
import { createContext, useContext, useState, type ReactNode } from 'react';

type ModalState = { name: string; props?: Record<string, unknown> } | null;

interface ModalContextValue {
  activeModal: ModalState;
  openModal: (name: string, props?: Record<string, unknown>) => void;
  closeModal: () => void;
}

const ModalContext = createContext<ModalContextValue | null>(null);

export function ModalProvider({ children }: { children: ReactNode }) {
  const [activeModal, setActiveModal] = useState<ModalState>(null);
  return (
    <ModalContext value={{
      activeModal,
      openModal: (name, props) => setActiveModal({ name, props }),
      closeModal: () => setActiveModal(null)
    }}>
      {children}
    </ModalContext>
  );
}

export function useModal() {
  const ctx = useContext(ModalContext);
  if (!ctx) throw new Error('useModal outside ModalProvider');
  return ctx;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Compose Providers Helper</summary>

**Problem:** Write a helper component `ComposeProviders` that takes an array of Provider components and nests them without pyramid-of-doom JSX.

**Solution:**
```typescript
import type { ComponentType, ReactNode } from 'react';

interface ComposeProps {
  providers: ComponentType<{ children: ReactNode }>[];
  children: ReactNode;
}

export function ComposeProviders({ providers, children }: ComposeProps) {
  return providers.reduceRight(
    (acc, Provider) => <Provider>{acc}</Provider>,
    children
  );
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Selective Context Consumer</summary>

**Problem:** Create a custom selector hook `useContextSelector<T, S>(ctx: Context<T | null>, selector: (val: T) => S): S` that derives a selected slice.

**Solution:**
```typescript
import { useContext, type Context } from 'react';

export function useContextSelector<T, S>(
  context: Context<T | null>,
  selector: (val: T) => S
): S {
  const val = useContext(context);
  if (!val) throw new Error('useContextSelector outside Provider');
  return selector(val);
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Type-Safe Multi-Theme Switcher Context</summary>

**Problem:** Build a theme context supporting customizable nested design tokens (colors, typography, spacing) with compile-time type safety.

**Solution:**
```typescript
export interface ThemeTokens {
  colors: { primary: string; background: string; text: string };
  spacing: { sm: number; md: number; lg: number };
}

const DefaultThemeContext = createContext<ThemeTokens | null>(null);

export function useThemeTokens(): ThemeTokens {
  const ctx = useContext(DefaultThemeContext);
  if (!ctx) throw new Error('Missing ThemeTokensProvider');
  return ctx;
}
```
</details>
