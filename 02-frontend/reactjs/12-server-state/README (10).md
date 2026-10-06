# 12 — Server State

Most of the data in a typical React app doesn't belong to React. Projects, users, orders, messages: they live on a server, and the UI only holds a **temporary copy**. That copy can go stale, be changed by someone else, fail to load, or need refreshing. Managing it with `useState` and `useEffect` works for a demo and falls apart in a real app.

**Server state** is the name for that category of data, and it needs different tools than UI state. This folder covers the mental model and the standard tool for it: **[TanStack Query](https://tanstack.com/query)** (v5).

```text
Server (source of truth)
   │  fetch / mutate        ← 11-api-integration (the client)
   ▼
Query cache  ◄── keys, staleness, invalidation   ← this folder
   │  useQuery / useMutation
   ▼
Components (render whatever the cache says)
```

## Prerequisites

- [API integration](../11-api-integration/README.md): you need a working client that throws on HTTP errors
- [Hooks](../03-hooks/README.md): especially `useEffect`, and why [you might not need one](../03-hooks/03-you-might-not-need-an-effect.md)
- [TypeScript with React](../04-typescript-with-react/README.md)

## Contents

| # | File | What you'll learn |
|---|------|-------------------|
| 00 | [Server vs client state](./00-server-vs-client-state.md) | The distinction, what lives where, the "don't copy it" rule |
| 01 | [Fetching data](./01-fetching-data.md) | Hand-rolled fetching, the states to model, waterfalls, parallel and dependent fetches |
| 02 | [Loading and error states](./02-loading-and-error-states.md) | Skeletons, background refetch, errors with stale data, Suspense |
| 03 | [TanStack Query](./03-tanstack-query.md) | Setup, `useQuery`, query keys, `queryOptions`, v5 changes |
| 04 | [Caching and synchronization](./04-caching-and-synchronization.md) | `staleTime` vs `gcTime`, refetch triggers, invalidation |
| 05 | [Mutations](./05-mutations.md) | `useMutation`, cache updates, invalidation, forms |
| 06 | [Optimistic updates](./06-optimistic-updates.md) | Instant UI with rollback, and when not to |
| 07 | [Pagination and infinite queries](./07-pagination-and-infinite-queries.md) | Page-based, cursor-based, infinite scroll |

## Suggested order

00 → 01 gives you the problem. 03 is the core API, and 02 and 04–07 build on it. Files 02 and 04 use TanStack Query's flags in their examples because that's the clearest way to describe the states, so if you want the setup first, read 03 right after 01.

## Conventions

- TanStack Query **v5**, `@tanstack/react-query`.
- Examples call the `api` client and resource modules from [02 — API client](../11-api-integration/02-api-client.md) (`projectsApi.list`, `projectsApi.get`, …).
- Query functions must **throw** on failure (the client does).
