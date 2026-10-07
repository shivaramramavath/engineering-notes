# 04 · Rendering

How and **when** Next.js turns your components into HTML: ahead of time (static), per request (dynamic), or in pieces as data arrives (streaming). Rendering strategy is chosen per route, mostly implicitly from what your code does, so the key skill is predicting what Next.js will do and checking it.

> Written for Next.js 16. Rendering and caching are tightly linked; this chapter explains the rendering side, and [06 · Caching](../06-caching/README.md) covers storage and revalidation.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [Rendering Overview](./00-rendering-overview.md) | Where and when rendering happens, SSR/SSG/ISR/CSR mapped to the App Router, how Next.js decides |
| 01 | [Static Rendering](./01-static-rendering.md) | Build-time HTML, what keeps a route static, revalidation, static export |
| 02 | [Dynamic Rendering](./02-dynamic-rendering.md) | Request-time rendering, what triggers it, how to control it |
| 03 | [Streaming and Suspense](./03-streaming-and-suspense.md) | `loading.tsx`, `<Suspense>`, static shells, common mistakes |

## Use it as a reference

- "Why is my page dynamic?" → [Dynamic Rendering](./02-dynamic-rendering.md), triggers section
- "How do I pre-build pages?" → [Static Rendering](./01-static-rendering.md)
- "The whole page waits for one slow query" → [Streaming and Suspense](./03-streaming-and-suspense.md)
- "What do ○ and ƒ in the build output mean?" → [Rendering Overview](./00-rendering-overview.md)

## Next

[05 · Data Fetching](../05-data-fetching/README.md): getting data into Server and Client Components.
