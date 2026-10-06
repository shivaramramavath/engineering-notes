# Project Structure

A Next.js project follows conventions: some folders and file names have special meaning to the framework, and everything else is yours to organize. This note is the map: what lives at the top level, what each reserved file does, and how to organize a growing codebase.

## Top level

```text
my-app/
├── app/                    # App Router: routes and UI
├── public/                 # static files, served from "/"
├── src/                    # optional: wraps app/ and other source folders
├── next.config.ts          # framework config
├── proxy.ts                # optional: request interception (formerly middleware.ts)
├── instrumentation.ts      # optional: observability hooks
├── .env.local              # local environment variables
├── tsconfig.json
├── eslint.config.mjs
└── package.json
```

| Entry | Purpose |
|---|---|
| `app/` | Routes, layouts, route handlers (App Router) |
| `pages/` | Legacy Pages Router; see [Pages Router](./04-pages-router.md) |
| `public/` | Static assets, e.g. `public/logo.svg` is served at `/logo.svg` |
| `src/` | Optional. Puts `app/` at `src/app/` to separate code from root config |
| `next.config.ts` | See [next.config](../00-setup/04-next-config.md) |
| `proxy.ts` | Runs before routes: redirects, rewrites, header changes. Next.js 16 name; older versions call it `middleware.ts`. Place it next to `app/` (inside `src/` if you use it) |
| `instrumentation.ts` | Runs once on server start; used for monitoring/tracing setup |

If you use `src/`, config files (`next.config.ts`, `tsconfig.json`, env files) stay at the project root.

## Special files in a route segment

Inside `app/`, these file names are reserved. Extensions can be `.js`, `.jsx` or `.tsx`.

| File | What it does |
|---|---|
| `layout` | Shared UI for a segment and its children; preserved across navigation |
| `page` | Unique UI for a route; makes it publicly accessible |
| `template` | Like layout, but remounts on every navigation |
| `loading` | Loading UI shown while the segment streams (wraps the page in Suspense) |
| `error` | Error UI for the segment (React error boundary; must be a Client Component) |
| `global-error` | Error UI that replaces the root layout when the root itself fails |
| `not-found` | UI for `notFound()` and unmatched URLs |
| `route` | HTTP endpoint (GET, POST, ...) instead of UI |
| `default` | Fallback for a parallel-route slot after a hard navigation |

### Metadata files

Files that generate head tags or well-known URLs:

| File | Result |
|---|---|
| `favicon.ico`, `icon`, `apple-icon` | Site icons |
| `opengraph-image`, `twitter-image` | Social share images |
| `sitemap.xml` / `sitemap.ts` | Sitemap |
| `robots.txt` / `robots.ts` | Crawler rules |
| `manifest.json` / `manifest.ts` | Web app manifest |

See [SEO and Metadata](../14-seo-and-metadata/00-metadata.md).

## How the special files nest

When a segment renders, the files wrap each other in a fixed order:

```text
layout
 └─ template
     └─ error boundary        (error.tsx)
         └─ Suspense boundary  (loading.tsx)
             └─ error boundary (not-found.tsx)
                 └─ page
```

This explains common behavior:

- `error.tsx` catches errors in the page **and** in its children, but **not** errors in the `layout` of the same segment (put an `error.tsx` in the parent segment for that).
- `loading.tsx` is a Suspense fallback around the page, so it only covers content below the layout; the layout renders immediately.

## Dynamic and organizational folder names

Folder names can also carry meaning:

| Name | Meaning | Example |
|---|---|---|
| `[slug]` | Dynamic segment | `blog/[slug]` → `/blog/hello` |
| `[...slug]` | Catch-all segment | `docs/[...slug]` → `/docs/a/b/c` |
| `[[...slug]]` | Optional catch-all | also matches `/docs` |
| `(group)` | Route group: organizes without changing the URL | `(marketing)/about` → `/about` |
| `_folder` | Private folder: opted out of routing | `_components` |
| `@slot` | Named slot for parallel routes | `@modal` |
| `(.)folder`, `(..)folder` | Intercepting routes | modal-over-page patterns |

See [File System Routing](../02-routing/00-file-system-routing.md), [Route Groups](../02-routing/02-route-groups.md), [Parallel Routes](../02-routing/06-parallel-routes.md).

## Colocation is safe

Putting non-route files inside `app/` is fine. A segment is not public until it has `page` or `route`, so these are never reachable by URL:

```text
app/
└── dashboard/
    ├── page.tsx          # public: /dashboard
    ├── chart.tsx         # not a route
    ├── actions.ts        # not a route
    └── dashboard.test.tsx
```

This lets you keep a component next to the one page that uses it.

## Organizing a project

There is no single right layout. Three common strategies:

**1. Everything outside `app/`**: `app/` holds only routing; shared code lives elsewhere.

```text
app/                    # routes only
components/
lib/
```

**2. Shared folders inside `app/`**: same idea, kept under `app/`.

```text
app/
├── _components/
├── _lib/
└── dashboard/
```

**3. Split by feature or route**: shared code at the top, route-specific code colocated.

```text
app/
├── (marketing)/
│   └── pricing/page.tsx
└── dashboard/
    ├── page.tsx
    └── _components/
components/             # truly shared
lib/
```

Pick one and apply it consistently. Scaling choices (feature-based structure, layers) are covered in [Feature-Based Architecture](../23-architecture/00-feature-based-architecture.md).

A solid default for a typical app:

```text
src/
├── app/                # routes
├── components/         # shared UI
├── lib/                # utilities, db client, helpers
└── features/           # optional: feature modules when the app grows
```

## Common mistakes

| Mistake | What happens | Fix |
|---|---|---|
| `Page.tsx` / `Layout.tsx` capitalized | Not recognized as special files | Use lowercase `page.tsx`, `layout.tsx` |
| Expecting a folder without `page.tsx` to work | 404 | Add `page.tsx` |
| `error.tsx` not catching layout errors | Errors in same-segment layout bubble up | Add `error.tsx` in the parent segment |
| `error.tsx` without `"use client"` | Build error | Error components must be Client Components |
| `proxy.ts` in the wrong place | Never runs | Put it beside `app/` (or in `src/`) |
| Using `middleware.ts` on Next.js 16 | Deprecated | Rename to `proxy.ts` and update the exported function name |
| Config files inside `src/` | Ignored | Keep them at the project root |

## Quick Summary

- `app/` for routes, `public/` for static files, `src/` optional, config files at the root.
- Reserved files: `layout`, `page`, `template`, `loading`, `error`, `global-error`, `not-found`, `route`, `default`.
- They nest in a fixed order: layout → template → error boundary → Suspense → not-found → page.
- Only `page` and `route` make a segment public, so colocation is safe.
- Use `[x]`, `(x)`, `_x`, `@x` folder names for dynamic segments, grouping, privacy and slots.

## Next

- [Pages and Layouts](./03-pages-and-layouts.md)
- [File System Routing](../02-routing/00-file-system-routing.md)
- [Error and Not Found](../02-routing/05-error-and-not-found.md)