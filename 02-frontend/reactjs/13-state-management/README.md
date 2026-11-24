# 13 — State Management

Once server data lives in a [query cache](../12-server-state/README.md), URL state lives in the [URL](../10-routing/06-search-filter-and-url-state.md), and form state lives in a [form library](../06-forms/02-react-hook-form.md), what remains is **shared client state**: the theme, a collapsed sidebar, a shopping cart, a multi-step wizard, the currently selected items, or complex local workflows.

```text
  Is it server data?                  → TanStack Query              (12)
  Does it belong in a URL?            → search params / route       (10)
  Is it a form's draft?               → React Hook Form / local     (06)
  Used by one component?              → useState
  Shared by a few nearby?             → lift state up
  Shared widely, rarely changes?      → Context
  Shared widely, complex transitions? → useReducer + Context
  Shared widely, changes often?       → Zustand / Jotai
  Complex states / impossible states? → XState v5
  Extreme high-frequency telemetry?   → Signals / Ref buffers
```

---

## What you'll be able to do after this module

- **Select the Right Tool**: Apply the decision matrix to avoid over-engineering state management while preventing prop drilling and context performance issues.
- **Master Built-in Primitives**: Optimize React Context with provider splitting, memoization, and the `useReducer` + Context pattern.
- **Engineer Global Stores with Zustand v5**: Build minimal, selector-optimized global stores with middleware (devtools, persist) and out-of-React access.
- **Architect Enterprise Redux Toolkit (RTK 2)**: Structure feature slices, typed hooks, entity adapters, and listener middleware.
- **Implement Atomic State with Jotai**: Build bottom-up reactive graphs with primitive, derived, and async atoms without root providers.
- **Model Impossible States with XState v5**: Eliminate impossible states and race conditions using statecharts, guards, and the Actor Model.
- **Understand Signals vs React State**: Evaluate fine-grained reactivity (Preact Signals, TC39 proposal) vs React 19's VDOM reconciliation and compiler memoization.

---

## Module Syllabus

| # | Note | Description | Key Paradigm / Tool |
|---|------|-------------|:---:|
| 00 | [00-choosing-state-management.md](./00-choosing-state-management.md) | Decision framework, state classification, comparison matrix | Decision Tree |
| 01 | [01-context-patterns-and-performance.md](./01-context-patterns-and-performance.md) | Context propagation, re-render boundaries, provider splitting | React Context |
| 02 | [02-reducer-and-context-pattern.md](./02-reducer-and-context-pattern.md) | `useReducer` combined with Context for complex local modules | Built-in React |
| 03 | [03-zustand.md](./03-zustand.md) | Lightweight monolithic stores, selector optimizations, middleware | Zustand v5 |
| 04 | [04-redux-toolkit.md](./04-redux-toolkit.md) | Slices, typed hooks, `createEntityAdapter`, listener middleware | Redux Toolkit 2 |
| 05 | [05-jotai-atomic-state.md](./05-jotai-atomic-state.md) | Decentralized atomic state, derived atoms, async atoms, Suspense | Jotai v2 |
| 06 | [06-xstate-and-state-machines.md](./06-xstate-and-state-machines.md) | Finite state machines, statecharts, actor model, eliminating invalid states | XState v5 |
| 07 | [07-signals-vs-react-state.md](./07-signals-vs-react-state.md) | Fine-grained reactivity vs VDOM reconciliation, Preact Signals, React Compiler | Signals / React 19 |

---

## Architectural Comparison Matrix

| Paradigm | Library | Core Mental Model | Best Used For |
| :--- | :--- | :--- | :--- |
| **Monolithic Store** | Zustand v5 | Single store object + actions | Global app state, shopping carts, player state |
| **Atomic Graph** | Jotai v2 | Decentralized graph of atomic cells | Highly interconnected UI state, dynamic entities |
| **Action / Reducer** | Redux Toolkit | Predictable unidirection actions + slices | Large enterprise teams, strict audit requirements |
| **Statechart / FSM** | XState v5 | Finite states + guarded transitions | Multi-step wizards, checkout, auth orchestrations |
| **Fine-Grained Signal** | Preact Signals | Direct reactive DOM binding | High-frequency 60Hz telemetry, games, stock tickers |

---

## Boundary with Other Modules

- **Server State Caching**: Read [`12-server-state/`](../12-server-state/README.md) for remote data caching with TanStack Query.
- **Routing State**: Read [`10-routing/06-search-filter-and-url-state.md`](../10-routing/06-search-filter-and-url-state.md) for URL search params as state.
- **Forms**: Read [`06-forms/`](../06-forms/README.md) for form validation with React Hook Form and Zod.
- **Performance**: Read [`14-performance/`](../14-performance/README.md) for profiling re-renders.

---
