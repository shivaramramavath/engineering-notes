# 06 · Caching

What Next.js stores, where, for how long, and how to refresh it. Caching is the most version-sensitive part of Next.js, so this chapter starts by helping you work out **which model your project uses**, then covers both.

> Verified against the Next.js 16.4 documentation. Per those docs, new projects from `create-next-app` with the recommended defaults have **Cache Components** and **Partial Prefetching** enabled, and both are planned to become the only behavior in the next major version. Older projects and many tutorials use the **previous model**.

## Which model am I on?

Open `next.config.ts` and look for `cacheComponents`:

```ts
const nextConfig: NextConfig = {
  cacheComponents: true, // ← Cache Components model
};
```

| Setting | Model | Mental model |
|---|---|---|
| `cacheComponents: true` | **Cache Components** | Dynamic by default. You opt in to caching with `use cache`. |
| Not set | **Previous model** | Static by default. `fetch` caching is opt-in; route-level config (`dynamic`, `revalidate`) controls behavior. |

## Reading order

| # | Note | Covers |
|---|---|---|
| 00 | [Caching Overview](./00-caching-overview.md) | The layers, the two models, what to cache, debugging |
| 01 | [Data Cache](./01-data-cache.md) | `fetch` caching and `unstable_cache` (previous model), and how `use cache` differs |
| 02 | [Full Route Cache](./02-full-route-cache.md) | Prerendered output: full route cache (previous) and static shell / ISR (Cache Components) |
| 03 | [Router Cache](./03-router-cache.md) | The browser-side client cache, prefetching, stale times |
| 04 | [Revalidation](./04-revalidation.md) | `cacheLife`, tags, `revalidateTag`, `updateTag`, `revalidatePath`, `refresh` |
| 05 | [Cache Components](./05-cache-components.md) | `use cache`, Suspense, static shell, migration |

## If you only read two notes

New project (Cache Components on): [05 · Cache Components](./05-cache-components.md) and [04 · Revalidation](./04-revalidation.md).

Existing project (previous model): [01 · Data Cache](./01-data-cache.md) and [04 · Revalidation](./04-revalidation.md), then [05](./05-cache-components.md) when you migrate.

## Next

[07 · Server Actions](../07-server-actions/README.md): mutations, forms, and the revalidation calls that follow them.