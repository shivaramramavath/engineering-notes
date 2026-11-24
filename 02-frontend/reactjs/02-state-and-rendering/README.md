# Module 02: State and Rendering

## Syllabus & Topic Overview

Module 02 dissects the core runtime engine of React: state transitions, snapshots, reconciliation, and the two-phase rendering pipeline. From the fundamental snapshot mental model to deep dive inspections of the Render and Commit phases, immutable updates with Immer, state colocation, and StrictMode double-rendering behaviors, this module provides the rigorous conceptual foundations necessary to build bug-free, high-performance UI state systems.

### Concept Notes Index

1. [00-state-and-snapshots.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/00-state-and-snapshots.md) - State as a snapshot across time, closures capturing render variables, and the asynchronous queue mental model.
2. [01-state-updates-and-batching.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/01-state-updates-and-batching.md) - Updater functions `setCount(c => c + 1)`, automatic batching across microtasks, and `flushSync`.
3. [02-state-structure-and-lifting.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/02-state-structure-and-lifting.md) - Structuring state cleanly: avoiding redundancy, lifting state to common parents, and state normalization.
4. [03-rendering.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/03-rendering.md) - Triggers, render tree execution, and how React avoids unnecessary work.
5. [04-component-lifecycle.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/04-component-lifecycle.md) - The functional lifecycle: mounting, updating, and unmounting mapped to effects and reconciliation.
6. [05-state-preservation-and-reset.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/05-state-preservation-and-reset.md) - How component position in the UI tree controls state identity, and resetting state via the `key` prop.
7. [06-state-colocation-and-derived-state.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/06-state-colocation-and-derived-state.md) - Keeping state local, avoiding state duplication, and calculating pure derived values during render.
8. [07-immutable-updates-with-immer.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/07-immutable-updates-with-immer.md) - ES6 Proxy drafts, structural sharing, `useImmer`, curried reducers, and eliminating spread operator clutter.
9. [08-render-phases-render-vs-commit.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/08-render-phases-render-vs-commit.md) - Render phase (pure, concurrent, interruptible) vs Commit phase (synchronous, DOM mutations, layout effects).
10. [09-strict-mode-double-rendering.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/09-strict-mode-double-rendering.md) - Why React renders twice in development, effect remounting stress tests, and fixing cleanup bugs.

---

## Learning Outcomes & Core Takeaways

- **The Snapshot Mental Model**: Internalize that state variables never mutate within an already-running render pass; they are immutable constants captured by function closures.
- **Render vs Commit**: Understand why side effects are fatal in the Render phase and how `useLayoutEffect` vs `useEffect` hook into the Commit phase lifecycle.
- **Derived State Mastery**: Eliminate redundant state variables and sync effects by calculating derived values directly in the component body.
- **Immer Fluency**: Update deep nested objects with natural mutable syntax while retaining structural sharing and referential equality.
- **StrictMode Resilient**: Write pure render functions and robust effect cleanup logic that pass StrictMode double-rendering checks effortlessly.
