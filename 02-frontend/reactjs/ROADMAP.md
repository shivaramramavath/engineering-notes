# ReactJS Learning Roadmaps (ROADMAP.md)

> Structured engineering learning pathways with realistic time commitments, prerequisites, and milestone projects.

---

## 1. Track Pathways Overview

```
[ Track 1: Modern React 19 Core ] ────> [ Track 2: Production State & Routing ]
                 │                                        │
                 ▼                                        ▼
[ Track 3: TypeScript React & Forms ] ─> [ Track 4: Architecture, Internals & Performance ]
                 │                                        │
                 └───────────────────┬────────────────────┘
                                     ▼
                       [ Track 5: Interview Mastery ]
```

---

## Path 1: Modern React 19 Core (Foundational)
- **Goal**: Master modern declarative UI programming with React 19, Vite, and modern JSX transforms.
- **Estimated Time**: 20-30 Hours
- **Modules**:
  1. `00-setup`: Vite 6 setup, TypeScript strict mode, React DevTools, StrictMode behaviors.
  2. `01-fundamentals`: JSX compilation, component purity, reconciliation keys, events, basic forms.
  3. `02-state-and-rendering`: Render vs commit phases, state batching, Immer, StrictMode double-rendering.
  4. `03-hooks`: Core hooks (`useState`, `useEffect`, `useRef`, `useContext`, `useReducer`, `useMemo`, `useCallback`).
- **Milestone Project**: [Todo Application with Persistence](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/01-todo-with-persistence/).

---

## Path 2: Production State, Routing & APIs (Job-Ready Frontend)
- **Goal**: Build scalable, network-resilient frontend applications with modern client/server state separation.
- **Estimated Time**: 35-45 Hours
- **Modules**:
  1. `06-forms`: Controlled vs uncontrolled inputs, React Hook Form, React 19 Form Actions, Zod 4.
  2. `07-styling`: Modern Tailwind CSS v4, CSS Modules, zero-runtime themes.
  3. `08-accessibility`: Semantic HTML, ARIA patterns, focus management, WCAG 2.2.
  4. `10-routing`: React Router v7 (Library vs Framework mode), nested layouts, loaders/actions.
  5. `11-api-integration`: Axios/Fetch clients, JWT refresh token rotation, SSE streaming.
  6. `12-server-state`: TanStack Query v5, `useSuspenseQuery`, query invalidation, optimistic updates.
  7. `13-state-management`: Zustand v5, Redux Toolkit, Jotai atomic state.
- **Milestone Project**: [Movie Explorer with TanStack Query & MSW](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/04-movie-explorer/).

---

## Path 3: TypeScript React & Advanced Component Design
- **Goal**: Design enterprise component libraries and robust, type-safe frontend APIs.
- **Estimated Time**: 25-35 Hours
- **Modules**:
  1. `04-typescript-with-react`: Typing props, generics, polymorphic components, typing refs in React 19.
  2. `05-component-design`: Headless UI, slots/`asChild` pattern, state reducers, compound components.
  3. `09-ui-components`: TanStack Table v8, Dnd-kit drag-and-drop, Floating UI popovers, Storybook.
- **Milestone Project**: [Kanban Board with Drag-and-Drop](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/02-kanban-board-dnd/).

---

## Path 4: Architecture, Internals & Performance Engineering (Senior / Staff)
- **Goal**: Understand Fiber internals, React Compiler optimizations, concurrent scheduling, and enterprise architecture.
- **Estimated Time**: 40-50 Hours
- **Modules**:
  1. `14-performance`: React Compiler in practice, React Profiler, virtualization, Web Workers.
  2. `15-concurrent-and-modern-react`: Actions, Transitions, `useDeferredValue`, View Transitions, RSC concepts.
  3. `16-advanced-react`: Custom renderers, Flight protocol, micro-frontends, module federation.
  4. `17-react-internals`: Fiber node architecture, reconciliation diffing, lane scheduler priorities.
  5. `18-testing-and-debugging`: Vitest browser mode, Testing Library, MSW network mocking, Playwright e2e.
  6. `19-production`: CSP hardening, bundle budgets, Docker containers, Sentry monitoring.
  7. `20-frontend-architecture`: Feature-sliced design, CRA to Vite migration, module boundaries.
- **Milestone Project**: [Enterprise Admin Dashboard with RBAC & TanStack Table](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/07-admin-dashboard/).

---

## Path 5: Frontend Interview Mastery
- **Goal**: Ace technical machine coding rounds, frontend system design, and output questions.
- **Estimated Time**: 20-30 Hours
- **Modules**:
  1. `23-interview`: 13 comprehensive guides covering machine coding (autocomplete, modal, infinite scroll, toast system, undo/redo) and frontend system design.
  2. `24-cheatsheets`: Quick reference cheat sheets for React 19, Router v7, hooks pitfalls, and testing.
