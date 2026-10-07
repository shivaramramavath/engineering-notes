# 05 · Data Fetching

How data gets into your components. In the App Router the default answer is simple: **fetch on the server, inside the component that needs the data**. This chapter covers that default, the cases where fetching must happen in the browser instead, and how to avoid slow request waterfalls.

> Written for Next.js 16. Caching rules for `fetch` changed in Next.js 15 (no longer cached by default), and Next.js 16 adds Cache Components. Caching itself is covered in [06 · Caching](../06-caching/README.md); this chapter focuses on how and where to fetch.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [Server Fetching](./00-server-fetching.md) | `async` components, `fetch`, direct database access, deduplication, error handling |
| 01 | [Client Fetching](./01-client-fetching.md) | When the browser should fetch, `useEffect` pitfalls, SWR / TanStack Query |
| 02 | [Parallel and Sequential](./02-parallel-and-sequential.md) | Waterfalls, `Promise.all`, preloading, Suspense boundaries |

## Use it as a reference

- "Where should this fetch live?" → [Server Fetching](./00-server-fetching.md), then [Client Fetching](./01-client-fetching.md) if it depends on browser state
- "My page is slow and requests happen one after another" → [Parallel and Sequential](./02-parallel-and-sequential.md)
- "Do I call my own API route from a Server Component?" → [Server Fetching](./00-server-fetching.md), common mistakes

## Next

[06 · Caching](../06-caching/README.md): what Next.js stores, for how long, and how to refresh it.
