# 03 — Hooks

Hooks are functions that let components use React features: state, side effects, refs, context, and more. This module covers the complete landscape of React hooks—from foundational primitives (`useState`, `useEffect`) and performance tools (`useMemo`, `useCallback`) to advanced concurrent and React 19 primitives (`useId`, `useSyncExternalStore`, `useImperativeHandle`, `useInsertionEffect`, `useOptimistic`, `useActionState`, `useFormStatus`, `use`, and `useEffectEvent`).

---

## What you'll be able to do after this module

- **Follow and enforce the Rules of Hooks**: Explain linked-list fiber mechanics and why hooks cannot run conditionally (and why `use()` is the exception).
- **Manage Local & Complex State**: Master `useState` and `useReducer` with lazy initialization and state machine patterns.
- **Synchronize with External Systems**: Master `useEffect`, effect cleanups, race-condition guards, and know when you do *not* need an effect.
- **Handle Refs & Imperative Boundaries**: Use `useRef` for mutable values and DOM access; use `useImperativeHandle` with React 19 native `ref` props to expose custom handles.
- **Context & State Sharing**: Avoid prop drilling using `useContext` and React 19 conditional `use(Context)`.
- **Memoization & Compiler Awareness**: Know when manual `useMemo` and `useCallback` are necessary vs when the React Compiler optimizes automatically.
- **Prevent Concurrent Tearing**: Subscribe safely to non-React mutable stores with `useSyncExternalStore`.
- **Implement React 19 Form Actions & Optimistic UI**: Use `useActionState`, `useFormStatus`, and `useOptimistic` for instant feedback with automatic error rollbacks.
- **Unwrap Async Resources**: Use React 19's `use()` API to suspend on Promises and read context conditionally.
- **Decouple Reactive Logic**: Extract non-reactive side-effects cleanly with `useEffectEvent`.
- **Author Reusable Custom Hooks**: Build composable, typed hook primitives following production patterns.

---

## Module Syllabus

| # | Note | Description | React Version |
|---|------|-------------|:---:|
| 00 | [00-hook-rules.md](./00-hook-rules.md) | Hook rules, fiber call-order linked lists, linter invariants | React 16.8+ |
| 01 | [01-useState.md](./01-useState.md) | State API, functional updates, lazy state initializers | React 16.8+ |
| 02 | [02-useEffect.md](./02-useEffect.md) | External synchronization, cleanup phase, race-condition handling | React 16.8+ |
| 03 | [03-you-might-not-need-an-effect.md](./03-you-might-not-need-an-effect.md) | Eliminating redundant effects, computing during render | React 16.8+ |
| 04 | [04-useRef.md](./04-useRef.md) | Mutable references, DOM node bindings, interval handles | React 16.8+ |
| 05 | [05-useContext.md](./05-useContext.md) | Context subscriptions, provider patterns, consumption | React 16.8+ |
| 06 | [06-useReducer.md](./06-useReducer.md) | Reducer pattern, action dispatchers, complex state machines | React 16.8+ |
| 07 | [07-useMemo-and-useCallback.md](./07-useMemo-and-useCallback.md) | Value & callback memoization, React Compiler implications | React 16.8+ |
| 08 | [08-useLayoutEffect.md](./08-useLayoutEffect.md) | Synchronous pre-paint DOM measurements, layout shifts | React 16.8+ |
| 09 | [09-custom-hooks.md](./09-custom-hooks.md) | Extracting reusable logic, composition, return signatures | React 16.8+ |
| 10 | [10-hook-recipes.md](./10-hook-recipes.md) | Production recipes: debounce, throttle, media queries, storage | React 16.8+ |
| 11 | [11-useId.md](./11-useId.md) | Deterministic SSR ID generation, accessibility ARIA linking | React 18+ |
| 12 | [12-useSyncExternalStore.md](./12-useSyncExternalStore.md) | Subscribing to external non-React stores without concurrent tearing | React 18+ |
| 13 | [13-useImperativeHandle.md](./13-useImperativeHandle.md) | Custom ref handles, React 19 native ref props, encapsulation | React 19 |
| 14 | [14-useInsertionEffect.md](./14-useInsertionEffect.md) | Pre-mutation CSS-in-JS style injection, reflow avoidance | React 18+ |
| 15 | [15-useOptimistic.md](./15-useOptimistic.md) | Optimistic UI updates, transition bindings, automatic error rollbacks | React 19 |
| 16 | [16-useActionState.md](./16-useActionState.md) | React 19 Form Actions, async pending transitions, returned state | React 19 |
| 17 | [17-useFormStatus.md](./17-useFormStatus.md) | Contextual form status from parent forms without prop drilling | React 19 |
| 18 | [18-use-hook.md](./18-use-hook.md) | Reading Promises & Context conditionally inside `if` statements and loops | React 19 |
| 19 | [19-useEffectEvent.md](./19-useEffectEvent.md) | Extracting non-reactive event logic from effects, stable callbacks | React 19 Exp |

---

## Architectural Relationships & Mental Models

```
┌────────────────────────────────────────────────────────────────────────┐
│                          React Hook Ecosystem                          │
├────────────────────────────────┬───────────────────────────────────────┤
│ State & Lifecycle              │ Performance & Sync                    │
│ • useState                     │ • useMemo & useCallback               │
│ • useReducer                   │ • useSyncExternalStore                │
│ • useEffect                    │ • useLayoutEffect                     │
│ • useRef                       │ • useInsertionEffect                  │
├────────────────────────────────┼───────────────────────────────────────┤
│ React 19 Actions & Modern APIs │ Modularity & Ergonomics               │
│ • useActionState               │ • useContext & use(Context)           │
│ • useFormStatus                │ • useId                               │
│ • useOptimistic                │ • useImperativeHandle                 │
│ • use(Promise)                 │ • useEffectEvent                      │
│                                │ • Custom Hooks                        │
└────────────────────────────────┴───────────────────────────────────────┘
```

---

## Boundary with Other Modules

- **Mental Model & Render Phases**: Read [`02-state-and-rendering/`](../02-state-and-rendering/README.md) for deep dives into render vs commit phases, snapshot execution, and automatic batching.
- **TypeScript Typing for Hooks**: Read [`04-typescript-with-react/`](../04-typescript-with-react/README.md) for typing custom hooks, generics, and React 19 ref types.
- **Concurrent Transitions**: Read [`15-concurrent-and-modern-react/`](../15-concurrent-and-modern-react/README.md) for `useTransition` and `useDeferredValue`.
- **Form Architecture**: Read [`06-forms/`](../06-forms/README.md) for full-scale form schemas, Zod integration, and TanStack Form.
- **Hook Internals**: Read [`17-react-internals/`](../17-react-internals/README.md) for Fiber node structures and Hook dispatchers.

---

## Practical Exercises

1. **Accessible Switch (`useId`)**: Build a toggle switch component bound to an external label with deterministic SSR IDs.
2. **Online Status Indicator (`useSyncExternalStore`)**: Subscribe to `navigator.onLine` and `window.matchMedia` with zero tearing.
3. **Encapsulated Video Player (`useImperativeHandle`)**: Build a player exposing only `play()`, `pause()`, and `seekTo()` using React 19 native `ref` props.
4. **Optimistic Cart Quantity (`useOptimistic`)**: Build an e-commerce quantity stepper with instant UI response and automatic rollback on network failure.
5. **Form Action with Pending State (`useActionState`)**: Build an invitation form showing real-time error messages and auto-disabling submission.
6. **Decoupled Submit Button (`useFormStatus`)**: Create a design system button that reads parent form status without prop drilling.
7. **Conditional Context & Promise Unwrapping (`use()`)**: Lazily fetch regional data only when an admin settings context flag is enabled.
8. **Stable Analytics Logger (`useEffectEvent`)**: Log page navigations with user theme metadata without re-triggering network calls when theme changes.

---

## Next Steps

Advance to **[04-typescript-with-react/README.md](../04-typescript-with-react/README.md)** to master type-safe components, generics, and React 19 type patterns.
