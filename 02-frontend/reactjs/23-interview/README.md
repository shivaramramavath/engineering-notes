# Module 23: Technical Interview Preparation & Deep Dives (`23-interview`)

> Comprehensive, staff-level technical interview preparation covering React 19 internals, concurrent rendering, state management paradigms, performance diagnosis, testing, architecture, and live machine coding challenges.

---

## 1. Interview Prep Index

| Note | Focus Area | Key Architectural Concepts Covered |
| :--- | :--- | :--- |
| **[00-react-fundamentals.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/00-react-fundamentals.md)** | Core Fundamentals | Modern JSX runtime, `$$typeof` XSS security, $O(n)$ diffing heuristics, index-as-key bugs, `forwardRef` removal |
| **[01-hooks.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/01-hooks.md)** | Hooks & Closures | Fiber hook linked list, call order rules, stale closures, `useEffectEvent`, React 19 `use()` & `useActionState` |
| **[02-rendering-and-internals.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/02-rendering-and-internals.md)** | Reconciler Internals | Fiber tree vs Stack reconciler, double buffering (current/WIP), 31-bit Lanes bitmask, React Compiler HIR/SSA |
| **[03-state-management.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/03-state-management.md)** | State Paradigms | Context API re-render cascading, Zustand external slice selectors, RTK enterprise auditability, Jotai atomic graphs |
| **[04-performance.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/04-performance.md)** | Profiling & INP | Diagnosing Interaction to Next Paint (INP), long task slicing, memory leak heap snapshots, Web Workers |
| **[05-typescript-react.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/05-typescript-react.md)** | Type Safety | Radix `asChild` Slot polymorphic pattern, discriminated union interfaces, generic components, typing React 19 |
| **[06-react-router.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/06-react-router.md)** | Routing Architecture | Library mode vs Framework mode (Remix), parallel route loaders, eliminating fetch waterfalls |
| **[07-tanstack-query.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/07-tanstack-query.md)** | Server State | `staleTime` vs `gcTime` lifecycle, query key factories, optimistic mutations with rollback context |
| **[08-testing.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/08-testing.md)** | Testing Strategies | Behavioral query priorities, avoiding implementation details, Vitest 3 Browser Mode vs jsdom |
| **[09-accessibility.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/09-accessibility.md)** | Accessibility (a11y) | Roving TabIndex keyboard traversal, ARIA live polite vs assertive, focus trapping, screen reader compliance |
| **[10-architecture.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/10-architecture.md)** | Enterprise Scaling | Feature-Sliced Design (FSD) unidirectional boundaries, Module Federation trade-offs, monorepo governance |
| **[11-frontend-system-design.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/11-frontend-system-design.md)** | System Design | Collaborative document editor (Yjs CRDT vs OT), real-time presence, sub-50ms typing performance |
| **[12-coding-challenges.md](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/12-coding-challenges.md)** | Live Machine Coding | Accessible Combobox from scratch, `useUndoRedo` history stack, custom hooks |

---

## 2. Senior Interview Preparation Strategy

### For Architectural System Design Rounds
1. Anchor your answers around **unidirectional data flow** and **clear state ownership** (Client UI vs Server Cache vs URL SearchParams).
2. Quantify performance: Discuss **Interaction to Next Paint (INP)** budgets (under 200ms) and explain how React 19 Concurrent Features (`startTransition`, `useOptimistic`) maintain main-thread responsiveness.

### For Live Machine Coding Rounds
1. **Accessibility First**: Always use semantic HTML, correct roles (`role="combobox"`), and keyboard navigation before polishing visual aesthetics.
2. **Handle Edge Cases**: Explicitly manage empty states, network loading spinners, error boundaries, and unmount cleanups.
