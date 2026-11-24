# 17 — React Internals & Core Architecture

This module dives beneath the public API surface of React into the low-level data structures, algorithms, and compiler pipelines that power the framework: the Fiber architecture, double buffering, reconciliation diffing heuristics, the internal linked list of hooks, the Scheduler and 31-bit Priority Lanes, the SyntheticEvent delegation model, and the React Compiler's HIR transforms.

---

## What you'll be able to do after this module

- **Understand the Fiber Tree**: Traverse Fiber data structures (`child`, `sibling`, `return`, `memoizedState`, `alternate`) and understand the double-buffering model (`current` vs `workInProgress`).
- **Deconstruct the Reconciliation Algorithm**: Understand the $O(n)$ diffing heuristics, why `key` props prevent unneeded unmounting, and how React processes sibling lists.
- **Trace Hook Dispatchers**: Trace how `useState` and `useEffect` build and traverse circular linked lists on a Fiber's `memoizedState` and why hook order rules exist.
- **Master the Scheduler & Lanes**: Understand cooperative multitasking, `MessageChannel` tick yielding, and how 31-bit binary bitmasks (`Lanes`) prioritize urgent user inputs over background renders.
- **Navigate the SyntheticEvent System**: Understand root container event delegation (attached to `#root` in React 17+), event pooling removal, and cross-browser event normalization.
- **Demystify the React Compiler**: Understand High-level Intermediate Representation (HIR), Static Single Assignment (SSA) dataflow, Alias Graphs, and runtime `_c()` cache slots.

---

## Module Syllabus

| # | Note | Description | Key Internal Concept |
|---|------|-------------|:---:|
| 00 | [00-fiber-architecture.md](./00-fiber-architecture.md) | Fiber data structure, unit of work, double-buffering, work loop | Fiber Node |
| 01 | [01-reconciliation.md](./01-reconciliation.md) | Tree diffing heuristics, element recycling, key prop algorithms | Reconciliation |
| 02 | [02-how-hooks-work.md](./02-how-hooks-work.md) | Hook dispatchers, `memoizedState` linked list, update queues | Hooks Internals |
| 03 | [03-scheduler-and-lanes.md](./03-scheduler-and-lanes.md) | Cooperative time-slicing, 31-bit priority lanes, work interruption | Scheduler & Lanes |
| 04 | [04-event-system.md](./04-event-system.md) | Root event delegation, SyntheticEvent wrapper, event phases | SyntheticEvents |
| 05 | [05-compiler-transforms-internals.md](./05-compiler-transforms-internals.md) | Compiler HIR, SSA dataflow, alias analysis, reactive scopes, `_c()` | Compiler Internals |

---

## Internal Architecture Summary

```text
React Component (JSX + Hooks)
      │
      ▼
React Compiler (HIR / SSA analysis -> injects _c() cache slots)
      │
      ▼
Scheduler (Lanes: Sync, InputContinuous, Default, Idle)
      │
      ▼
Reconciler (Fiber Work Loop: beginWork -> completeWork)
      │
      ├── Current Fiber Tree (displayed in DOM)
      └── WorkInProgress Fiber Tree (speculatively prepared off-screen)
      │
      ▼
Commit Phase (Mutates Host DOM in single synchronous pass)
```

---

## Boundary with Other Modules

- **Rendering Lifecycle**: Read [`02-state-and-rendering/08-render-phases-render-vs-commit.md`](../02-state-and-rendering/08-render-phases-render-vs-commit.md) for practical render vs commit phase boundaries.
- **Concurrent React**: Read [`15-concurrent-and-modern-react/00-concurrent-rendering.md`](../15-concurrent-and-modern-react/00-concurrent-rendering.md) for how the Scheduler interacts with `useTransition`.
- **Custom Renderers**: Read [`16-advanced-react/04-custom-renderers-reconciler.md`](../16-advanced-react/04-custom-renderers-reconciler.md) for how `react-reconciler` interfaces with custom host targets.

---
