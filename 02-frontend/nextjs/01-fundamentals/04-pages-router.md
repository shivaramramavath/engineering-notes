# Pages Router

The Pages Router is Next.js's original routing system, based on a `pages/` directory. It is still supported, and a huge amount of existing code, tutorials and Stack Overflow answers use it. You need to be able to read it, know how it differs from the App Router, and migrate away from it when appropriate. For new projects, use the App Router.

## How it works

Each file in `pages/` is a route. Components are Client Components (they also pre-render on the server), and data is loaded through special exported functions.

```text
pages/
├── _app.tsx              # wraps every page
├── _document.tsx         # customizes the HTML document
├── index.tsx             # /
├── about.tsx             # /about
├── blog/
│   ├── index.tsx         # /blog
│   └── [slug].tsx        # /blog/hello
└── api/
    └── hello.ts          # /api/hello  (API route)
```

### Data fetching functions

```tsx
// pages/blog/[slug].tsx
import type { GetStaticPaths, GetStaticProps } from "next";

type Post = { slug: string; title: string };

export const getStaticPaths: GetStaticPaths = async () => {
  return {
    paths: [{ params: { slug: "hello" } }],
    fallback: "blocking",
  };
};

export const getStaticProps: GetStaticProps<{ post: Post }> = async ({ params }) => {
  const post: Post = { slug: String(params?.slug), title: "Hello" };
  return { props: { post }, revalidate: 60 };
};

export default function PostPage({ post }: { post: Post }) {
  return <h1>{post.title}</h1>;
}
```

| Function | Runs | Used for |
|---|---|---|
| `getStaticProps` | Build time (and on revalidation) | Static pages |
| `getStaticPaths` | Build time | Which dynamic routes to pre-generate |
| `getServerSideProps` | Every request | Per-request data |
| `getInitialProps` | Server and client | Legacy; avoid |

### Other concepts

- **`_app.tsx`**: persistent wrapper; global CSS, providers.
- **`_document.tsx`**: edit `<html>` / `<body>` (server only).
- **`next/head`**: set `<title>` and meta tags per page.
- **`next/router`**: `useRouter()` for navigation and query params.
- **`pages/api/*`**: API routes.
- Custom `404.tsx` and `500.tsx` pages; `_error.tsx` for other errors.

## App Router vs Pages Router

| Concern | Pages Router | App Router |
|---|---|---|
| Directory | `pages/` | `app/` |
| Default component type | Client (pre-rendered) | Server Component |
| Shared UI | `_app.tsx`, per-page layout patterns | Nested `layout.tsx` (persistent) |
| Data fetching | `getStaticProps`, `getServerSideProps` | `async` components, `fetch`, direct DB access |
| Static paths | `getStaticPaths` | `generateStaticParams` |
| Loading UI | Manual | `loading.tsx` + Suspense streaming |
| Error UI | `_error.tsx`, `404.tsx` | `error.tsx`, `not-found.tsx` |
| Head tags | `next/head` | `metadata` / `generateMetadata` |
| Router hooks | `next/router` | `next/navigation` |
| API endpoints | `pages/api/*.ts` | `route.ts` Route Handlers |
| Mutations | API route + `fetch` | Server Actions |
| React features | Client-side React | Server Components, Suspense, Server Functions |

## Both can coexist

A project can have `app/` and `pages/` at the same time, which enables incremental migration. Constraints:

- The same URL cannot be defined in both; the build reports a conflict.
- Each router has its own navigation hooks. `next/router` works only in `pages/`, `next/navigation` only in `app/`.
- Shared components that use router hooks need care, since they must work with whichever router renders them.

## Migrating to the App Router

Migrate route by route instead of all at once.

**1. Create `app/layout.tsx`** to replace `_app.tsx` and `_document.tsx`:

```tsx
// app/layout.tsx
import "./globals.css";

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
```

Move providers into a `"use client"` component rendered in this layout.

**2. Move one page at a time**, deleting its `pages/` version when its `app/` version works.

**3. Translate the pieces:**

| Pages Router | App Router |
|---|---|
| `<Head><title>…</title></Head>` | `export const metadata` or `generateMetadata` |
| `getServerSideProps` | `async` Server Component (dynamic: reads request data, or uncached fetch) |
| `getStaticProps` | `async` Server Component; static by default when nothing is dynamic |
| `getStaticProps` + `revalidate` | Time-based revalidation on `fetch`, or Cache Components |
| `getStaticPaths` | `generateStaticParams` |
| `pages/api/x.ts` | `app/api/x/route.ts` |
| `useRouter` from `next/router` | `useRouter`, `usePathname`, `useSearchParams` from `next/navigation` |
| `router.query` | `params` (page prop or `useParams`) and `useSearchParams` |
| `404.tsx` | `not-found.tsx` |
| Client logic in a page | Move into a `"use client"` component |

**Example translation:**

```tsx
// Before: pages/blog/[slug].tsx
export const getServerSideProps = async ({ params }) => {
  const post = await getPost(params.slug);
  return { props: { post } };
};
export default function Page({ post }) {
  return <h1>{post.title}</h1>;
}
```

```tsx
// After: app/blog/[slug]/page.tsx
export default async function Page({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const post = await getPost(slug);
  return <h1>{post.title}</h1>;
}
```

**4. Watch for these:**

- Anything using state, effects or browser APIs must be in a `"use client"` file.
- Libraries that use React context or browser APIs at import time may need a client wrapper.
- Built-in internationalized routing (`i18n` in `next.config`) is a Pages Router feature; the App Router needs a route-segment approach instead. See [i18n](../15-advanced-features/00-i18n.md).
- Caching defaults differ. Do not assume `getStaticProps` semantics carry over; review [Caching Overview](../06-caching/00-caching-overview.md).

## Reading old tutorials

If a tutorial or answer mentions any of these, it is Pages Router material:

- `getServerSideProps`, `getStaticProps`, `getStaticPaths`
- `pages/_app.tsx`, `pages/_document.tsx`
- `import { useRouter } from "next/router"`
- `next/head`
- `pages/api`

The ideas (static vs dynamic rendering, dynamic routes) still apply; the APIs differ.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Importing `next/router` in `app/` | Error that the router was not mounted | Use `next/navigation` |
| Defining `/about` in both `app/` and `pages/` | Build conflict error | Remove one |
| Using `getServerSideProps` in `app/` | Ignored / unsupported | Fetch in the Server Component |
| Copying `_app.tsx` logic into the root layout unchanged | Client-only code breaks in a Server Component | Move it into a Client Component provider |
| Assuming `fetch` caches by default (Next 14 behavior) | Stale or uncached surprises | Check the caching rules for your version |

## Quick Summary

- `pages/` is the legacy router: file = route, data via `getStaticProps` / `getServerSideProps`, pages are client-oriented.
- `app/` is the current router: Server Components, nested layouts, streaming.
- They can coexist, but not on the same URL; they use different navigation hooks.
- Migrate incrementally: root layout first, then page by page, translating data fetching, head tags, API routes and router hooks.

## Next

- [02 · Routing](../02-routing/README.md)
- [Server Components](../03-components/00-server-components.md)
- [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md)