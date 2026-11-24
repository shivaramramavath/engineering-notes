# React Engineering Curriculum

> **A Comprehensive, Production-Grade Engineering Track for Modern React 19+**  
> Covers client-side React internals, Concurrent Mode, React 19 Actions, Fiber reconciliation, TypeScript, state architectures, performance optimization, accessibility, and production systems design.

---

## Track Overview

This track provides complete, foundational-to-expert coverage of modern React application engineering. It emphasizes production architectures, rigorous TypeScript types, real browser/Vitest verification, and elimination of outdated legacy patterns (such as `forwardRef`, Create React App, manual `useMemo` when compiler is enabled, and class-first components).

### Key Architectural Standards
- **Baseline Version**: **React 19.0.0+** (tested with Vite 6.0, TypeScript 5.7, Vitest 3.0, and Tailwind CSS v4).
- **Modern Idioms**: Built-in Actions, `useActionState`, `useFormStatus`, `useOptimistic`, `use(Promise | Context)`, native `<title>/<meta>` metadata hoisting, and `ref` as a prop.
- **Routing**: React Router v7 (Library Mode vs Framework Mode) and TanStack Router v1.
- **Data Fetching**: TanStack Query v5 dedicated suspense hooks (`useSuspenseQuery`), optimistic mutations, and SSR hydration.
- **Accessibility**: 100% WCAG 2.2 compliant patterns using Radix UI primitives and React Aria.

---

## Curriculum Structure

| Module | Title | Focus & Core Capabilities |
| :---: | :--- | :--- |
| **[00-setup](file:///E:/Learning/engineering-notes/02-frontend/reactjs/00-setup/README.md)** | Setup & Modern Tooling | Vite 6, React 19 upgrade, ESLint with React Hooks/Compiler rules, monorepos. |
| **[01-fundamentals](file:///E:/Learning/engineering-notes/02-frontend/reactjs/01-fundamentals/README.md)** | Core Fundamentals | JSX compilation, component purity, reconciliation keys, events, forms. |
| **[02-state-and-rendering](file:///E:/Learning/engineering-notes/02-frontend/reactjs/02-state-and-rendering/README.md)** | State & Render Mechanics | Render vs commit phases, StrictMode double-rendering, batching, Immer. |
| **[03-hooks](file:///E:/Learning/engineering-notes/02-frontend/reactjs/03-hooks/README.md)** | React Hooks In-Depth | Built-in hooks (`useState` to `useInsertionEffect`), React 19 Action hooks, `use()`. |
| **[04-typescript-with-react](file:///E:/Learning/engineering-notes/02-frontend/reactjs/04-typescript-with-react/README.md)** | TypeScript Integration | Typing props/events/hooks, polymorphic components, typing refs in 19. |
| **[05-component-design](file:///E:/Learning/engineering-notes/02-frontend/reactjs/05-component-design/README.md)** | Component Architecture | Headless components, slots/`asChild`, state reducers, compound components. |
| **[06-forms](file:///E:/Learning/engineering-notes/02-frontend/reactjs/06-forms/README.md)** | Forms & Validation | React 19 Form Actions, React Hook Form, TanStack Form, Zod 4 schemas. |
| **[07-styling](file:///E:/Learning/engineering-notes/02-frontend/reactjs/07-styling/README.md)** | Modern Styling | Tailwind CSS v4 (`@tailwindcss/vite`), CSS Modules, zero-runtime tokens. |
| **[08-accessibility](file:///E:/Learning/engineering-notes/02-frontend/reactjs/08-accessibility/README.md)** | Web Accessibility (a11y) | Radix primitives, React Aria, focus management, WCAG 2.2, Playwright a11y. |
| **[09-ui-components](file:///E:/Learning/engineering-notes/02-frontend/reactjs/09-ui-components/README.md)** | Complex UI Components | TanStack Table, Dnd-kit, Floating UI, command palettes, Storybook. |
| **[10-routing](file:///E:/Learning/engineering-notes/02-frontend/reactjs/10-routing/README.md)** | Routing & Navigation | React Router v7 (Library vs Framework), TanStack Router type-safe routes. |
| **[11-api-integration](file:///E:/Learning/engineering-notes/02-frontend/reactjs/11-api-integration/README.md)** | API & Network Layer | Fetch/Axios clients, OAuth SPA flows, SSE streaming, GraphQL, tRPC. |
| **[12-server-state](file:///E:/Learning/engineering-notes/02-frontend/reactjs/12-server-state/README.md)** | Server State & Cache | TanStack Query v5 `useSuspenseQuery`, query keys, prefetching, offline. |
| **[13-state-management](file:///E:/Learning/engineering-notes/02-frontend/reactjs/13-state-management/README.md)** | Client State Management | Zustand v5, Redux Toolkit, Jotai atomic state, XState state machines. |
| **[14-performance](file:///E:/Learning/engineering-notes/02-frontend/reactjs/14-performance/README.md)** | Performance & Profiling | React Compiler, React Profiler, virtualization, bundle splitting, Web Workers. |
| **[15-concurrent-and-modern-react](file:///E:/Learning/engineering-notes/02-frontend/reactjs/15-concurrent-and-modern-react/README.md)** | Concurrent React | Transitions, `useDeferredValue`, Suspense, View Transitions, RSC concepts. |
| **[16-advanced-react](file:///E:/Learning/engineering-notes/02-frontend/reactjs/16-advanced-react/README.md)** | Advanced Systems | Custom renderers, Flight protocol, Micro-frontends, module federation. |
| **[17-react-internals](file:///E:/Learning/engineering-notes/02-frontend/reactjs/17-react-internals/README.md)** | React Core Internals | Fiber node structure, reconciliation algorithms, lane scheduler priorities. |
| **[18-testing-and-debugging](file:///E:/Learning/engineering-notes/02-frontend/reactjs/18-testing-and-debugging/README.md)** | Testing & Debugging | Vitest, Testing Library, MSW network mocking, Playwright E2E testing. |
| **[19-production](file:///E:/Learning/engineering-notes/02-frontend/reactjs/19-production/README.md)** | Production Readiness | Docker containers, CI/CD pipelines, CSP/XSS security hardening, Sentry. |
| **[20-frontend-architecture](file:///E:/Learning/engineering-notes/02-frontend/reactjs/20-frontend-architecture/README.md)** | Architecture & Design | Feature-sliced design, layered systems, CRA to Vite migration strategies. |
| **[21-specializations](file:///E:/Learning/engineering-notes/02-frontend/reactjs/21-specializations/README.md)** | Specialized Domains | Framer Motion animations, React Flow node editors, Recharts, maps, AI UIs. |
| **[22-projects](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/README.md)** | Production Projects | 15 full production reference applications with complete tests and architecture. |
| **[23-interview](file:///E:/Learning/engineering-notes/02-frontend/reactjs/23-interview/README.md)** | Interview Preparation | Machine coding rounds, staff frontend system design, output prediction. |
| **[24-cheatsheets](file:///E:/Learning/engineering-notes/02-frontend/reactjs/24-cheatsheets/README.md)** | Quick Reference Cards | 10 rapid reference cards for APIs, hooks, types, testing, and debugging. |
| **[_libraries](file:///E:/Learning/engineering-notes/02-frontend/reactjs/_libraries/README.md)** | Ecosystem Libraries | Concise reference cards for TanStack, Router, Redux, Zustand, Tailwind, etc. |

---

## Cross-Repository Navigation
- **TypeScript Language Fundamentals**: [01-languages/typescript](file:///E:/Learning/engineering-notes/01-languages/typescript/)
- **Next.js & Server Components**: [02-frontend/nextjs](file:///E:/Learning/engineering-notes/02-frontend/nextjs/)
- **Backend API Engineering**: [03-backend](file:///E:/Learning/engineering-notes/03-backend/)
- **AI & LLM Client Integrations**: [05-ai](file:///E:/Learning/engineering-notes/05-ai/)
