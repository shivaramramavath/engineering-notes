# 02 · Routing

How URLs map to UI in the App Router, how users move between routes, and how routes handle redirects and failure. The first four notes cover everything most apps need; the last two (parallel and intercepting routes) are advanced patterns you can come back to.

> Written for Next.js 16. Concepts introduced in [01 · Fundamentals](../01-fundamentals/README.md) (segments, `page.tsx`, layouts) are assumed.

## Reading order

| # | Note | You will learn |
|---|---|---|
| 00 | [File System Routing](./00-file-system-routing.md) | Folders as segments, nesting, route precedence, colocation |
| 01 | [Dynamic Routes](./01-dynamic-routes.md) | `[slug]`, catch-all, optional catch-all, `generateStaticParams` |
| 02 | [Route Groups](./02-route-groups.md) | `(group)` folders, per-section layouts, multiple root layouts |
| 03 | [Navigation](./03-navigation.md) | `<Link>`, prefetching, `useRouter`, URL hooks |
| 04 | [Redirects and Rewrites](./04-redirects-and-rewrites.md) | Four ways to redirect, rewrites, when to use which |
| 05 | [Error and Not Found](./05-error-and-not-found.md) | `error.tsx`, `global-error.tsx`, `not-found.tsx`, `notFound()` |
| 06 | [Parallel Routes](./06-parallel-routes.md) | `@slot` folders, independent regions, `default.tsx` |
| 07 | [Intercepting Routes](./07-intercepting-routes.md) | `(.)` conventions, modal-over-page pattern |

## Use it as a reference

- 404 on a route that should exist → [File System Routing](./00-file-system-routing.md)
- Need a `/blog/:slug` style route → [Dynamic Routes](./01-dynamic-routes.md)
- Different layouts for marketing vs app → [Route Groups](./02-route-groups.md)
- Which redirect API do I use? → [Redirects and Rewrites](./04-redirects-and-rewrites.md)
- Photo-gallery or login modal with a shareable URL → [Intercepting Routes](./07-intercepting-routes.md)

## Next

[03 · Components](../03-components/README.md): Server Components, Client Components and how to combine them.
