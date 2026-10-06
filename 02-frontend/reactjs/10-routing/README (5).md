# 10 — Routing

In a single-page app the browser loads one HTML file and JavaScript decides what to show for each URL. **Routing** is the layer that maps URLs to UI, keeps the address bar in sync, and (in modern routers) loads the data each screen needs.

This folder uses **React Router v7** in its *library* form (`react-router` package), with the **data router** API (`createBrowserRouter`). That's the setup that gives you loaders, actions, and error handling per route, and it's what most Vite + React apps use.

```text
URL ──► route matching ──► layouts (nested) ──► page component
              │                                     ▲
              └──► loaders (data) ──────────────────┘
```

## Prerequisites

- [Components and props](../01-fundamentals/02-components-and-props.md), [children and composition](../01-fundamentals/03-children-and-composition.md)
- [Hooks](../03-hooks/README.md), especially `useEffect` and context

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [React Router](./00-react-router.md) | Setup, route objects, matching, 404s, SPA hosting |
| 01 | [Nested routes and layouts](./01-nested-routes-and-layouts.md) | `Outlet`, index and pathless layout routes |
| 02 | [Dynamic routes and params](./02-dynamic-routes-and-params.md) | `:id`, optional and splat segments, validating params |
| 03 | [Navigation](./03-navigation.md) | `Link`, `NavLink`, `useNavigate`, redirects, scroll |
| 04 | [Route protection](./04-route-protection.md) | Auth guards, redirect-back, roles |
| 05 | [Route data loading](./05-route-data-loading.md) | Loaders, actions, fetchers, errors, TanStack Query |
| 06 | [Search, filter, and URL state](./06-search-filter-and-url-state.md) | `useSearchParams`, URL as state, debounced search |

## Which "React Router" is this?

React Router v7 has three ways to use it:

| Mode | Entry | Notes |
|---|---|---|
| **Declarative** | `<BrowserRouter>` + `<Routes>` | Matching and navigation only. No loaders/actions |
| **Data** | `createBrowserRouter` + `<RouterProvider>` | Adds loaders, actions, `errorElement`, pending states |
| **Framework** | The React Router Vite plugin | Full-stack: SSR, type generation, file conventions |

These notes cover **data mode**. Framework mode builds on the same concepts, with server rendering added; see [server components and SSR](../15-concurrent-and-modern-react/07-server-components-and-ssr.md).

> Older code imports from `react-router-dom` (v6). In v7 everything lives in `react-router`; `react-router-dom` remains only as a re-export for compatibility.

## Conventions

- Examples are TypeScript with React 19.
- Route components are named for the screen (`ProjectPage`), layouts end in `Layout`.
