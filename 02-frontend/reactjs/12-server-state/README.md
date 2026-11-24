# 12 — Server State & Asynchronous Data Caching

This module covers the architecture and lifecycle of server state in React applications. Understanding why server state differs fundamentally from client state, eliminating `useEffect` fetching bugs, and mastering the industry standard caching engine: TanStack Query v5 (React Query), including Suspense integration, query key factories, optimistic rollbacks, cursor pagination, and SSR dehydration.

---

## What you'll be able to do after this module

- **Distinguish Server State from Client State**: Understand that server state is remotely persisted, asynchronously retrieved, shared across multiple clients, and potentially stale.
- **Eliminate `useEffect` Fetching Pitfalls**: Replace fragile `useEffect` data fetching patterns with battle-tested asynchronous caching engines.
- **Master TanStack Query v5 Core**: Configure `QueryClient`, understand query lifecycles (`pending`, `success`, `error`), and master `staleTime` vs `gcTime`.
- **Implement Optimistic Mutations**: Update local cache instantly during user actions, snapshot previous data, and execute automatic rollbacks if the network fails.
- **Build Infinite Feeds & Pagination**: Implement cursor-based and offset pagination using `useInfiniteQuery` with virtualized viewports.
- **Adopt Suspense-First Architecture**: Use `useSuspenseQuery` with React 19 `<Suspense>` and `<QueryErrorResetBoundary>` for guaranteed non-null `data` types.
- **Implement SSR Dehydration & Hydration**: Prefetch data on Node servers using request-scoped `QueryClient` instances, eliminating double-fetching during client hydration.

---

## Module Syllabus

| # | Note | Description | Key Concept / Tool |
|---|------|-------------|:---:|
| 00 | [00-server-vs-client-state.md](./00-server-vs-client-state.md) | Differences between server state & client UI state, ownership, latency | Mental Models |
| 01 | [01-fetching-data.md](./01-fetching-data.md) | Primitive `useEffect` fetching, race conditions, cancellation cleanup | Anti-Patterns & Basics |
| 02 | [02-loading-and-error-states.md](./02-loading-and-error-states.md) | Tri-state fetching, skeletons, retry strategies, visual feedback | UX Architecture |
| 03 | [03-tanstack-query.md](./03-tanstack-query.md) | TanStack Query v5 setup, `QueryClientProvider`, `useQuery`, `useMutation` | TanStack Query v5 |
| 04 | [04-caching-and-synchronization.md](./04-caching-and-synchronization.md) | `staleTime` vs `gcTime`, active/inactive queries, automated revalidation | Cache Mechanics |
| 05 | [05-mutations.md](./05-mutations.md) | Server mutations, cache invalidation, post-mutation side effects | Mutations |
| 06 | [06-optimistic-updates.md](./06-optimistic-updates.md) | Instant UI updates, rollback snapshots in `onMutate`, error recovery | Optimistic UI |
| 07 | [07-pagination-and-infinite-queries.md](./07-pagination-and-infinite-queries.md) | Cursor pagination, `useInfiniteQuery`, page params, infinite lists | Pagination |
| 08 | [08-tanstack-query-v5-suspense.md](./08-tanstack-query-v5-suspense.md) | Type-safe `useSuspenseQuery`, query key factories, React 19 transitions | Suspense & Types |
| 09 | [09-ssr-hydration-prefetching.md](./09-ssr-hydration-prefetching.md) | `dehydrate`, `HydrationBoundary`, per-request QueryClients, streaming SSR | SSR & Hydration |

---

## Architectural Comparison: Cache Lifetime Rules

| Property | Default Value | Meaning | What happens when it expires? |
| :--- | :--- | :--- | :--- |
| **`staleTime`** | `0` ms | How long data is considered "fresh". | Query data is marked "stale"; refetched on window focus or remount. |
| **`gcTime`** | `5 * 60 * 1000` (5 mins) | How long inactive query data stays in memory. | Query is garbage collected; next mount must fetch from scratch. |

---

## Boundary with Other Modules

- **Network Layer**: Read [`11-api-integration/`](../11-api-integration/README.md) for configuring HTTP clients, interceptors, and PKCE tokens.
- **Client State**: Read [`13-state-management/`](../13-state-management/README.md) for client-only UI state (Zustand, Jotai).
- **React 19 Actions**: Read [`06-forms/07-react-19-form-actions.md`](../06-forms/07-react-19-form-actions.md) for comparing native React actions with TanStack mutations.

---
