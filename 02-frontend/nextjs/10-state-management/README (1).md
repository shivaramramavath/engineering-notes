# 10 · State Management

Where state should live in an App Router app, and the three tools for the cases where plain props, the URL and `useState` are not enough: Context, Zustand and TanStack Query.

> Verified against the Next.js 16.4 documentation. Zustand and TanStack Query details follow their own docs (Zustand v5, TanStack Query v5) and can change between versions.

## Start here: what kind of state is it?

```text
Server data ─────────────► Server Components (props). Cache: chapter 06
Shareable view state ────► the URL (searchParams)
Form state ──────────────► the form + useActionState
One component's UI ──────► useState / useReducer
Shared, slow-changing ───► Context
Shared, fast-changing ───► Zustand
Server data + browser
behavior (poll, infinite
scroll, optimistic) ─────► TanStack Query
Server needs it at first
paint (theme, locale) ───► cookie
```

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [State Overview](./00-state-overview.md) | The decision guide, URL state, cookies, hydration safety, state across navigations |
| 01 | [Context](./01-context.md) | Providers in the App Router, `use()` with promises, avoiding re-render storms |
| 02 | [Zustand](./02-zustand.md) | Stores, selectors, per-request stores for SSR, persistence |
| 03 | [TanStack Query](./03-tanstack-query.md) | Provider setup, server prefetch and hydration, mutations, cache coordination |

## The rules to remember

1. **Do not start with a store.** Props, the URL and local state cover most apps.
2. **Never copy server data into a client store.** Pass it as props, or use a query cache seeded from the server.
3. **No shared mutable state on the server.** Per-user data in a module-level store can leak between requests; use per-request instances.
4. **Mutations end in revalidation,** and every cache layer holding the data must be invalidated.
5. **First renders must match.** Use stable initial values, effects, or cookies for anything from `localStorage` or `window`.
6. **Keep stateful components small** and near the leaves; pass Server Components in as `children`.

## Next

[11 · Authentication](../11-authentication/README.md): sessions, protecting routes and data, and the Data Access Layer.
