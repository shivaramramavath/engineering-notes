# React & Ecosystem Facts & Baseline Versions (REACT_FACTS.md)

> Canonical facts, APIs, and versions discovered during Phase 0 research.
> Source of truth for all curriculum notes, benchmarks, and version tags.

---

## 1. Core React Release Landscape

### React 19 Stable
- **Released**: December 2024 (General Availability).
- **Key Features Shipped**:
  - **Actions**: Async transitions integrated directly into elements (`<form action={fn}>`, `<button formAction={fn}>`). Automatic pending states, automatic form reset on success, optimistic rollbacks.
  - **`useActionState`**: Replaced older `useFormState`. Takes `(action, initialState)` and returns `[state, formAction, isPending]`.
  - **`useFormStatus`**: Hook from `react-dom` reading parent `<form>` status (`{ pending, data, method, action }`) without prop drilling. Must be invoked inside a component nested in `<form>`.
  - **`useOptimistic`**: Manages optimistic UI updates before async actions complete. Takes `(currentState, updateFn)` and returns `[optimisticState, setOptimisticState]`. Rolls back automatically if action throws.
  - **`use(Promise | Context)`**: Reads values of Promises or Context directly in render. Unlike traditional hooks, `use()` can be called **conditionally** (inside `if`, loops, after early returns). Automatically integrates with `<Suspense>`.
  - **`ref` as a Prop**: Functions now accept `ref` directly as a regular prop (`function MyInput({ ref, ...props })`). `React.forwardRef()` is deprecated/no longer required.
  - **Native Document Metadata**: Built-in support for `<title>`, `<meta>`, `<link>` inside component trees. React automatically hoists them to `<head>`.
  - **Stylesheet Precedence & Preloading**: Native support for `<link rel="stylesheet" precedence="default" />`, `preload()`, `preinit()`.
  - **Direct `<Context value={...}>`**: No longer requires `<Context.Provider value={...}>`.
  - **Deprecations/Removals**: `propTypes`, `defaultProps` on function components removed, `forwardRef` deprecated, string refs removed, legacy context removed.

### React Compiler
- **Status**: Stable / Release Candidate.
- **Linting**: The standalone `eslint-plugin-react-compiler` package is deprecated and merged directly into **`eslint-plugin-react-hooks`** (v6+).
- **Behavior**: Auto-memoizes JSX, values, and functions at build time using static analysis. Eliminates manual `useMemo` and `useCallback` boilerplate for performance.

---

## 2. Routing Ecosystem

### React Router v7
- **Released**: Late 2024.
- **Architectural Shift**: Remix and React Router merged into a single package: `react-router`.
- **Modes**:
  1. **Library Mode**: Standard client-side SPA routing (`createBrowserRouter`, `<RouterProvider>`).
     - *Declarative Mode*: `<BrowserRouter>`, `<Routes>`, `<Route>`.
     - *Data Mode*: Route `loader`, `action`, `shouldRevalidate`.
  2. **Framework Mode**: Formerly Remix. Full-stack meta-framework with file-system routing (`app/routes/*`), SSR, streaming, Vite plugin, and server actions.

### TanStack Router v1
- **Status**: v1.0+ Stable.
- **Features**: 100% type-safe routing, TypeScript-first path autocomplete, built-in search params validation via Zod, route loaders with TanStack Query integration (`ensureQueryData`), code-splitting.

---

## 3. State Management & Data Fetching

### TanStack Query (React Query) v5
- **Latest Version**: v5.x.
- **Major Changes from v4**:
  - Removed `{ suspense: true }` in `useQuery`.
  - Dedicated Suspense hooks: `useSuspenseQuery`, `useSuspenseInfiniteQuery`, `useSuspenseQueries`.
  - Guaranteed `data: T` (never `undefined`) in `useSuspenseQuery`.
  - Removed `placeholderData` and `enabled` in suspense hooks.
  - Simplified object syntax: `useQuery({ queryKey, queryFn })`.

### State Management Libraries
- **Zustand**: v5.x (lightweight, unopinionated store, `useSyncExternalStore` based, middleware support).
- **Jotai**: v2.x (atomic state management, bottom-up composition).
- **Redux Toolkit (RTK)**: v2.x (`createSlice`, listener middleware, RTK Query).

---

## 4. UI, Styling & Tooling

### Tailwind CSS v4
- **Engine**: Ground-up rewrite powered by **Rust / Lightning CSS**.
- **Vite Integration**: Dedicated first-party `@tailwindcss/vite` plugin.
- **Zero-Config Content**: Auto-scans files without manual `content: []` arrays in JS.
- **CSS-First Theme**: Configured in CSS via `@import "tailwindcss";` and `@theme`, eliminating `tailwind.config.js`.

### Radix UI & shadcn/ui
- **React 19 Compatibility**: Radix UI primitives (`@radix-ui/*`) and `shadcn/ui` are fully compatible with React 19.
- **Patterns**: Headless accessible primitives, `asChild` composition pattern via Slot.

### Forms & Validation
- **React Hook Form**: v7.x (`useForm`, controller, integration with React 19).
- **TanStack Form**: v0.x / v1 (framework-agnostic type-safe form state).
- **Zod**: Zod 3 vs Zod 4 (Zod 4 features up to 14x faster parsing, `zod/mini` functional tree-shaking, unified error parameter).

### Bundlers & Test Tools
- **Vite**: v6.x (default bundler for React client-side apps).
- **Vitest**: v2.x / v3.x (fast unit testing with JSDOM / Happy-DOM and Vitest Browser Mode).
- **Testing Library**: `@testing-library/react` v16+ supporting React 19 async act and transition lifecycles.
