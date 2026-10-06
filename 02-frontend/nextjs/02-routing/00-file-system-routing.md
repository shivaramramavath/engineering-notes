# File System Routing

In the App Router, **the folder structure under `app/` is your route table**. There is no router config to maintain: create a folder, add a `page.tsx`, and the URL exists. This note covers how folders become URLs, how nesting works, how Next.js decides between competing matches, and how to debug a route that returns 404.

## Segments and routes

Each folder is a URL **segment**. A route is the chain of segments from `app/` down to a folder containing a `page.tsx` (or `route.ts`).

```text
app/                                  (root segment)
├── page.tsx                          →  /
├── pricing/
│   └── page.tsx                      →  /pricing
└── dashboard/
    ├── page.tsx                      →  /dashboard
    └── settings/
        └── page.tsx                  →  /dashboard/settings
```

```tsx
// app/pricing/page.tsx
export default function PricingPage() {
  return <h1>Pricing</h1>;
}
```

A folder with no `page.tsx` is not a route by itself:

```text
app/dashboard/            ← no page.tsx here: /dashboard is a 404
└── settings/page.tsx     ← /dashboard/settings works
```

## Nested routes and layouts

Nesting folders nests URLs and layouts. Each segment may add its own `layout.tsx`, wrapping everything below it:

```text
app/
├── layout.tsx                 RootLayout
└── dashboard/
    ├── layout.tsx             DashboardLayout
    ├── page.tsx               /dashboard
    └── settings/
        └── page.tsx           /dashboard/settings
```

`/dashboard/settings` renders `RootLayout → DashboardLayout → settings page`. Layout behavior is in [Pages and Layouts](../01-fundamentals/03-pages-and-layouts.md).

## What turns a segment into an endpoint

Only two files make a segment publicly reachable:

| File | Responds with |
|---|---|
| `page.tsx` | UI (HTML) for GET navigation |
| `route.ts` | An HTTP response from your handler |

A segment cannot have both a `page` and a `route` for the same path. Everything else in the folder is private to your code.

## Colocation is safe

You can keep components, tests and helpers next to the route that uses them:

```text
app/blog/
├── page.tsx              ← public: /blog
├── post-card.tsx         ← not a route
├── blog.test.tsx         ← not a route
└── queries.ts            ← not a route
```

To make the intent explicit (and to avoid clashes with future special file names), prefix with an underscore. The folder is then excluded from routing entirely:

```text
app/
├── _components/          ← never a route, even if it contained page.tsx
└── blog/page.tsx
```

If you actually need a literal underscore in a URL, write `%5F` in the folder name (`%5Fabout` → `/_about`).

## Which route wins?

When more than one folder could match a URL, Next.js uses specificity:

```text
app/blog/new/page.tsx            static segment
app/blog/[slug]/page.tsx         dynamic segment
app/blog/[...rest]/page.tsx      catch-all
```

| URL | Matches |
|---|---|
| `/blog/new` | the static `new` folder |
| `/blog/hello` | `[slug]` |
| `/blog/2026/hello` | `[...rest]` |

Static beats dynamic, dynamic beats catch-all. See [Dynamic Routes](./01-dynamic-routes.md).

## Folder names with special meaning

| Name | Effect | Note |
|---|---|---|
| `[slug]`, `[...slug]`, `[[...slug]]` | Dynamic segments | [Dynamic Routes](./01-dynamic-routes.md) |
| `(group)` | Organizes routes without affecting the URL | [Route Groups](./02-route-groups.md) |
| `_folder` | Private, opted out of routing | above |
| `@slot` | Parallel route slot (not a URL segment) | [Parallel Routes](./06-parallel-routes.md) |
| `(.)x`, `(..)x`, `(...)x` | Intercepting routes | [Intercepting Routes](./07-intercepting-routes.md) |

## URL behavior you control from config

- **Trailing slash:** by default `/about/` redirects to `/about`. Set `trailingSlash: true` in `next.config.ts` to invert it.
- **Base path:** `basePath: "/docs"` serves the whole app under a prefix.
- **Case:** treat URLs as case-sensitive when planning folder names; use lowercase, hyphen-separated names (`terms-of-service`).

See [next.config](../00-setup/04-next-config.md).

## Seeing your routes

`next build` prints the route table. Use it to check that a route exists and whether it is static or dynamic:

```text
Route (app)
┌ ○ /
├ ○ /pricing
├ ƒ /dashboard
└ ƒ /blog/[slug]
```

## Common mistakes and debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| 404 on a folder that exists | No `page.tsx` in it | Add one |
| 404 despite `page.tsx` | File misnamed (`Page.tsx`, `page.ts` without a default export, wrong extension) | Use exactly `page.tsx` with a default export |
| Route resolves to the wrong page | A static or dynamic sibling matched first | Check specificity order |
| "You cannot have two parallel pages that resolve to the same path" | Two route groups define the same URL | Rename or remove one |
| `page` and `route` in the same folder error | Both match the same path | Move the handler to a different segment |
| Folder with brackets treated literally in a shell | `[slug]` is a glob pattern | Quote the path: `mkdir "app/blog/[slug]"` |
| Edited route not showing | Dev cache | Restart `next dev`; if needed delete `.next` |

Debugging order for a 404: check the exact folder path → check `page.tsx` spelling and default export → check route groups and private folders → check `basePath`/`trailingSlash` → check any `proxy.ts` rewrite or redirect.

## Quick Summary

- Folders under `app/` are URL segments; `page.tsx` or `route.ts` makes one public.
- Nested folders mean nested URLs and nested layouts.
- Colocate freely; use `_folder` to be explicit.
- Specificity decides conflicts: static > dynamic > catch-all.
- The build's route table is the quickest way to verify what exists.

## Next

- [Dynamic Routes](./01-dynamic-routes.md)
- [Route Groups](./02-route-groups.md)
- [Navigation](./03-navigation.md)
