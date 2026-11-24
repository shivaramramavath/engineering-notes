# Module 22: Reference Projects (`22-projects`)

> Complete, production-grade reference applications designed to bridge theoretical concepts with real-world architectural mastery. Every project features comprehensive architecture diagrams, TypeScript domain models, modular service hooks, full source implementations, automated Vitest suites, and reproduction steps.

---

## 1. Project Catalog & Architectural Matrix

| # | Project Name | Slug | State Architecture | UI / Primitives | Key Concepts Demonstrated |
| :---: | :--- | :--- | :--- | :--- | :--- |
| **01** | [Todo with Persistence](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/01-todo-with-persistence.md) | `todo-with-persistence` | React 19 Actions + `useOptimistic` | Tailwind CSS v4 | Form actions, multi-tab local storage sync, inline editing, keyboard a11y |
| **02** | [Kanban Board DnD](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/02-kanban-board-dnd.md) | `kanban-board-dnd` | Zustand v5 + `@dnd-kit` | Tailwind CSS v4 | Multi-column drag and drop, collision detection, keyboard navigation, state history |
| **03** | [Expense Tracker Dashboard](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/03-expense-tracker-dashboard.md) | `expense-tracker-dashboard` | Context API + Recharts | Recharts + Zod | Data visualization, category breakdowns, financial ledger calculations, schema validation |
| **04** | [Movie Explorer TMDB](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/04-movie-explorer.md) | `movie-explorer` | TanStack Query v5 (`useInfiniteQuery`) | Tailwind + TMDB | Infinite scrolling sentinel, debounced search, genre filters, backdrop image portals |
| **05** | [E-Commerce Storefront](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/05-e-commerce-storefront.md) | `e-commerce-storefront` | Zustand v5 (Persist) | Radix UI Sheet | Sliding shopping cart drawer, voucher discounts, inventory quantity steppers |
| **06** | [Markdown Blog](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/06-blog-with-markdown.md) | `blog-with-markdown` | Scroll Spy + Unified AST | Tailwind Typography | AST markdown compilation, code block copy triggers, dynamic Table of Contents |
| **07** | [Enterprise Admin Dashboard](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/07-admin-dashboard.md) | `admin-dashboard` | TanStack Table v8 | Radix UI + Tailwind | High-throughput headless data grid, multi-column sorting, RBAC permission guards |
| **08** | [Realtime Chat Workspace](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/08-realtime-chat.md) | `realtime-chat` | WebSocket + `useOptimistic` | Tailwind CSS v4 | Reconnecting sockets, presence tracking, live typing bubbles, pinned auto-scroll |
| **09** | [Offline Notes PWA](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/09-notes-pwa-offline.md) | `notes-pwa-offline` | Dexie.js (IndexedDB) | PWA Manifest | Zero-latency local storage, `useSyncExternalStore` online detection, background sync queue |
| **10** | [Dynamic Form Builder](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/10-form-builder.md) | `form-builder` | React Hook Form + Dynamic Zod | `@dnd-kit` | Visual drag-and-drop form canvas, runtime Zod schema generation, live interactive preview |
| **11** | [AI Chat Interface](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/11-ai-chat-ui-streaming.md) | `ai-chat-ui-streaming` | Native Streams / Vercel AI | Tailwind + Lucide | Server-Sent Events token streaming, Generative UI tool call widgets, abort controllers |
| **12** | [Design System Primitives](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/12-design-system-storybook.md) | `design-system-storybook` | Radix Primitives + CVA | Storybook 8.4 + Tailwind | Polymorphic `asChild` Slot composition, class-variance-authority variants, token styling |
| **13** | [Interactive Node Graph](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/13-flow-editor-react-flow.md) | `flow-editor-react-flow` | `@xyflow/react` | React Flow Styles | Workflow graph editor, custom input/output nodes, cycle detection, export JSON |
| **14** | [Virtualized Social Feed](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/14-social-feed-infinite-scroll.md) | `social-feed-infinite-scroll` | `@tanstack/react-virtual` | Tailwind CSS v4 | DOM virtualization for 10k items, dynamic height measurement, optimistic reactions |
| **15** | [Tech Job Search Portal](file:///E:/Learning/engineering-notes/02-frontend/reactjs/22-projects/15-job-board-search-filters.md) | `job-board-search-filters` | URL `useSearchParams` | React Router v7 | Deep-linked multi-facet filtering, URL as single source of truth, zero-shift skeletons |

---

## 2. Progressive Learning Pathways

### Pathway A: Modern UI Architecture & Enterprise Systems
- Start with **01-todo-with-persistence** (React 19 Actions) ➔ **05-e-commerce-storefront** (Zustand & Radix) ➔ **07-admin-dashboard** (TanStack Table & RBAC) ➔ **12-design-system-storybook** (Radix Primitives & CVA).

### Pathway B: Server State, Async & Realtime Data
- Start with **04-movie-explorer** (TanStack Query Infinite) ➔ **08-realtime-chat** (WebSockets & Optimistic updates) ➔ **11-ai-chat-ui-streaming** (ReadableStream & Generative UI) ➔ **09-notes-pwa-offline** (Dexie.js & Offline Sync).

### Pathway C: Complex Canvas, Drag-and-Drop & Interactions
- Start with **02-kanban-board-dnd** (@dnd-kit) ➔ **10-form-builder** (Dynamic schema generation) ➔ **13-flow-editor-react-flow** (Node graph pipelines) ➔ **14-social-feed-infinite-scroll** (TanStack Virtual).

---

## 3. Engineering Quality Guidelines

Every reference project in this module complies with the following production criteria:
1. **Zero External CSS Overhead**: Modern utility styling using Tailwind CSS v4.
2. **Robust Type Safety**: Full TypeScript strict typing without `any` escapes.
3. **Automated Testing Suite**: Unit and integration tests written in Vitest and React Testing Library.
4. **Accessible Foundations**: Full ARIA markup, keyboard navigation support, and semantic HTML elements.
