# 10 — Client-Side & Type-Safe Routing

This module covers client-side routing in single-page applications and modern web applications. From traditional client-side route mapping in React Router to data routers (loaders, actions), protected routes, URL state synchronization, the React Router v7 architectural merger with Remix, and 100% end-to-end type-safe routing with TanStack Router.

---

## What you'll be able to do after this module

- **Master Client-Side Routing Fundamentals**: Understand the browser History API (`pushState`, `popstate`), client-side link interception, and declarative route trees.
- **Implement Nested Layouts & Outlets**: Build persistent shell layouts, nested sidebars, and hierarchical dashboards without redundant component re-mounting.
- **Architect Protected Routes & Auth Guards**: Protect authenticated sections, handle multi-tenant role-based access control (RBAC), and preserve redirect locations for post-login redirection.
- **Leverage Data Routers**: Eliminate "fetch-on-render" waterfalls by using route loaders and actions to parallelize code fetching and data fetching.
- **Manage URL as State**: Use `useSearchParams` and URL serialization as the single source of truth for table pagination, search filters, modal states, and deep linking.
- **Navigate React Router v7 Architecture**: Distinguish between Library Mode (pure SPA) and Framework Mode (SSR, SSG, server bundles powered by the Vite plugin), understanding the unified Remix evolution.
- **Engineer End-to-End Type Safety with TanStack Router**: Enforce compile-time type validation on path parameters, search parameters validated via Zod, and typed navigation links that prevent 404 runtime errors.

---

## Module Syllabus

| # | Note | Description | Key Ecosystem Tool |
|---|------|-------------|:---:|
| 00 | [00-react-router.md](./00-react-router.md) | Declarative client-side routing, route trees, Route matching | React Router |
| 01 | [01-nested-routes-and-layouts.md](./01-nested-routes-and-layouts.md) | Hierarchical UI composition, persistent shell layouts, `<Outlet />` | React Router |
| 02 | [02-dynamic-routes-and-params.md](./02-dynamic-routes-and-params.md) | Route parameter extraction (`useParams`), wildcard splats, path scoring | React Router |
| 03 | [03-navigation.md](./03-navigation.md) | Declarative `<Link>`, `<NavLink>`, imperative `useNavigate`, history navigation | React Router |
| 04 | [04-route-protection.md](./04-route-protection.md) | Auth guards, RBAC authorization, redirect state retention | React Router / Context |
| 05 | [05-route-data-loading.md](./05-route-data-loading.md) | Data routers (`createBrowserRouter`), parallel loaders, actions, `useFetcher` | React Router Data APIs |
| 06 | [06-search-filter-and-url-state.md](./06-search-filter-and-url-state.md) | Search params as state, deep linking, debounced URL sync, pagination | URLSearchParams / Nuqs |
| 07 | [07-react-router-v7-library-vs-framework.md](./07-react-router-v7-library-vs-framework.md) | React Router v7 architectural shift: Library Mode vs Framework Mode, Remix merger | React Router v7 / Vite |
| 08 | [08-tanstack-router-type-safe.md](./08-tanstack-router-type-safe.md) | 100% type-safe routing, typed search params with Zod, loader prefetching | TanStack Router v1 |

---

## Architectural Comparison Matrix

| Feature / Dimension | React Router v7 (Library Mode) | React Router v7 (Framework Mode) | TanStack Router v1 |
| :--- | :--- | :--- | :--- |
| **Primary Focus** | Traditional client SPA | Fullstack React app (formerly Remix) | 100% Type-safe SPA / Fullstack |
| **Type Safety** | Typegen via Vite plugin | Typegen via Vite plugin | Native TypeScript inference + code-gen |
| **Search Param Validation**| Manual (`URLSearchParams`) | Manual or Zod in loaders | Built-in Zod/Valibot schema adapters |
| **Data Loading** | `loader` / `action` on routes | `loader` / `action` (Server or Client) | `loader` with built-in TanStack Query tie-in |
| **Prefetching** | Built-in via `<Link prefetch>` | Full bundle + data prefetch | Built-in hover/render prefetch engine |
| **File-Based Routing** | Optional (flat-routes convention) | Built-in file-system routing | Built-in file-system route generator |
| **Bundle Footprint** | Moderate (~12KB gzipped) | Full framework runtime | Lightweight core (~14KB gzipped) |

---

## Boundary with Other Modules

- **Server State & Caching**: Read [`12-server-state/`](../12-server-state/README.md) for caching route data with TanStack Query v5.
- **API Integration & Auth**: Read [`11-api-integration/09-oauth-oidc-spa-flows.md`](../11-api-integration/09-oauth-oidc-spa-flows.md) for PKCE OAuth token handling in route guards.
- **Framework Routing (Next.js)**: For file-system App Router, React Server Components routing, and server middleware, cross-link to `02-frontend/nextjs`.

---
