# 13 — State Management

Once server data lives in a [query cache](../12-server-state/README.md), URL state lives in the [URL](../10-routing/06-search-filter-and-url-state.md), and form state lives in a [form library](../06-forms/02-react-hook-form.md), what's left is **shared client state**: the theme, a collapsed sidebar, a shopping cart, a multi-step wizard, the currently selected items. That's a much smaller problem than "state management" used to mean, and the answer is usually simpler than people expect.

```text
  Is it server data?        → TanStack Query              (12)
  Does it belong in a URL?  → search params / route
  Is it a form's draft?     → React Hook Form / local
  Used by one component?    → useState
  Shared by a few nearby?   → lift state up
  Shared widely, rarely changes?      → Context
  Shared widely, complex transitions? → useReducer + Context
  Shared widely, changes often, or needs middleware/devtools?
                            → Zustand  /  Redux Toolkit
```

Climb that ladder **only as far as you need to**.

## Prerequisites

- [State and rendering](../02-state-and-rendering/README.md): especially [state structure and lifting](../02-state-and-rendering/02-state-structure-and-lifting.md)
- [useContext](../03-hooks/05-useContext.md) and [useReducer](../03-hooks/06-useReducer.md)
- [Server vs client state](../12-server-state/00-server-vs-client-state.md)

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Choosing state management](./00-choosing-state-management.md) | A decision framework and comparison of the options |
| 01 | [Context patterns and performance](./01-context-patterns-and-performance.md) | What context is (and isn't), re-render behavior, splitting, providers |
| 02 | [Reducer and context pattern](./02-reducer-and-context-pattern.md) | `useReducer` + context for complex state without a library |
| 03 | [Zustand](./03-zustand.md) | Minimal global stores, selectors, middleware, use outside React |
| 04 | [Redux Toolkit](./04-redux-toolkit.md) | Slices, typed hooks, selectors, when Redux still earns its place |

## Suggested order

Read 00 first, since it's the map. 01 and 02 are about built-in React tools and apply even if you end up using a library. 03 and 04 are independent; read the one you'll use (or both, to compare).

## Conventions

- TypeScript, React 19.
- **Zustand v5** and **Redux Toolkit 2 with React-Redux 9**. Older tutorials use older APIs, so check the version.
- Quick reference lives in the [Zustand cheatsheet](../24-cheatsheets/06-zustand.md).