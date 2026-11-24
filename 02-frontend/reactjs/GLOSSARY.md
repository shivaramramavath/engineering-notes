# ReactJS Technical Glossary (GLOSSARY.md)

> Exhaustive index of core definitions, architectural acronyms, and internal runtime terms in React 19+.

---

## A
- **Actions (React 19)**: Async functions passed directly to form or event handlers that run inside transitions, managing pending states and form resets automatically.
- **`asChild` (Slot Pattern)**: A pattern popularized by Radix UI where a component delegates rendering and prop merging to its direct child element without injecting an extra DOM wrapper.
- **Automatic Batching**: React grouping multiple state updates into a single re-render pass to prevent intermediate flickering (expanded to promises, timeouts, and native events in React 18+).

## C
- **Commit Phase**: The synchronous second phase of React's rendering pipeline where DOM mutations, layout effects (`useLayoutEffect`), and passive effects (`useEffect`) are applied.
- **Compound Components**: A component design pattern where a parent component manages shared state via Context, coordinating multiple child components (e.g. `<Select>`, `<Select.Option>`).
- **Concurrent React**: The internal architecture allowing React to interrupt, pause, or abandon in-progress renders to prioritize urgent user interactions (e.g. typing or clicking).

## F
- **Fiber**: CPython-like internal execution unit representing a component instance, holding its DOM node, memoized state, hooks linked list, and child/sibling pointers.
- **Flight Protocol (RSC)**: The streaming binary/JSON data format used by React Server Components to serialize virtual DOM trees and server functions over the network.
- **`forwardRef`**: Legacy higher-order component previously required to pass refs to functional components; deprecated in React 19 in favor of passing `ref` as a normal prop.

## H
- **Hook Dispatcher**: The internal C-like function pointer table swapped by React during render (`HooksDispatcherOnMount` vs `HooksDispatcherOnUpdate`) enforcing the Rules of Hooks.
- **Hydration**: The client-side process where React attaches event listeners and initializes state onto server-rendered HTML markup sent from the server.

## L
- **Lanes (Scheduler)**: Bitmask integers used by the React Reconciler to prioritize fiber work (e.g. `SyncLane`, `InputContinuousLane`, `DefaultLane`, `TransitionLane`).

## M
- **Micro-Frontends**: Architectural pattern decomposing a monolithic frontend application into independently deployable feature apps, often connected via Module Federation.

## O
- **Optimistic Updates**: Immediately updating the UI with expected mutation results before the network response completes, rolling back automatically if the request fails (powered by `useOptimistic`).

## R
- **React Compiler (formerly React Forget)**: Build-time compiler that analyzes React code and automatically injects memoization caches, eliminating manual `useMemo` and `useCallback`.
- **Reconciliation**: The $O(n)$ heuristic diffing algorithm React uses to compare work-in-progress Fiber trees with current trees to determine minimal DOM mutations.
- **Render Phase**: The pure, side-effect-free first phase of React rendering where components are invoked to generate virtual DOM elements. Can be paused or restarted by React.

## S
- **StrictMode**: A development-only wrapper that intentionally double-invokes components, initial state functions, and effects to detect impure side-effects and cleanup bugs.
- **Suspense**: A boundary component allowing children to suspend rendering while asynchronous operations (promises, lazy imports, server state) resolve.

## T
- **Transition (`useTransition`, `startTransition`)**: Marking a state update as non-urgent, allowing the browser to remain responsive to higher-priority user events.
- **Traversable**: Abstract file-system protocol used in package resources.

## U
- **`use` Hook (React 19)**: Built-in function to read Promises or Context values directly in render, uniquely callable conditionally inside `if` statements or loops.
- **`useActionState`**: React 19 hook managing form state, pending status, and action dispatchers.
- **`useFormStatus`**: Hook reading parent `<form>` submission status (`pending`, `data`, `method`) without prop drilling.
- **`useOptimistic`**: Hook for creating optimistic state representations during async transitions.
- **`useSyncExternalStore`**: Recommended hook for subscribing to non-React external stores without encountering concurrent tearing.

## V
- **View Transitions API**: Browser native API enabling smooth animated transitions between DOM states, integrated natively with modern React routing.
- **Virtual DOM**: In-memory representation of real DOM elements used by React to compute diffs prior to committing changes to the real browser DOM.
