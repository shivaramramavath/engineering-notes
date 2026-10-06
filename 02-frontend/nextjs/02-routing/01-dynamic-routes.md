# Dynamic Routes

A dynamic route handles many URLs with one folder, where part of the path is a variable: `/blog/hello`, `/blog/another-post`, `/products/42`. You wrap the folder name in square brackets, and the matched value arrives as a `params` prop.

## The three forms

| Folder | Matches | `params` |
|---|---|---|
| `[slug]` | one segment: `/blog/a` | `{ slug: "a" }` |
| `[...slug]` | one or more segments: `/docs/a/b/c` | `{ slug: ["a", "b", "c"] }` |
| `[[...slug]]` | zero or more segments (also matches `/docs`) | `{ slug: undefined }` for `/docs`, otherwise an array |

```text
app/
├── blog/[slug]/page.tsx             /blog/hello
├── docs/[...slug]/page.tsx          /docs/a, /docs/a/b   (not /docs)
└── shop/[[...slug]]/page.tsx        /shop, /shop/a, /shop/a/b
```

All values are **strings** (or arrays of strings). Convert numbers yourself: `Number(id)`.

## Reading params

In Next.js 15 and later, `params` is a **Promise**:

```tsx
// app/blog/[slug]/page.tsx
export default async function PostPage({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  return <h1>{slug}</h1>;
}
```

Multiple dynamic segments just add keys:

```tsx
// app/shop/[category]/[id]/page.tsx  →  /shop/shoes/42
export default async function Product({
  params,
}: {
  params: Promise<{ category: string; id: string }>;
}) {
  const { category, id } = await params;
  return <p>{category} #{id}</p>;
}
```

A catch-all gives an array:

```tsx
// app/docs/[...slug]/page.tsx
export default async function Docs({
  params,
}: {
  params: Promise<{ slug: string[] }>;
}) {
  const { slug } = await params; // ["getting-started", "install"]
  return <p>{slug.join(" / ")}</p>;
}
```

For an optional catch-all, type it `slug?: string[]`.

The same `params` object appears in layouts, `generateMetadata`, and Route Handlers (as the second argument's `params`, also a Promise). Typing helpers are in [Routes and Params](../13-typescript/01-routes-and-params.md).

### In Client Components

A Client Component cannot take `params` from the router directly. Use the hook:

```tsx
"use client";

import { useParams } from "next/navigation";

export function PostTitle() {
  const { slug } = useParams<{ slug: string }>();
  return <span>{slug}</span>;
}
```

## Handling "this one does not exist"

Dynamic routes match *any* value, so you must handle values that have no data:

```tsx
import { notFound } from "next/navigation";

export default async function PostPage({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const post = await getPost(slug); // returns null if missing
  if (!post) notFound();
  return <h1>{post.title}</h1>;
}
```

`notFound()` renders the nearest `not-found.tsx` and sets a 404 status. See [Error and Not Found](./05-error-and-not-found.md).

## Pre-generating pages: `generateStaticParams`

By default, a dynamic route is rendered on demand. To build known pages ahead of time, export `generateStaticParams`:

```tsx
// app/blog/[slug]/page.tsx
export async function generateStaticParams() {
  const posts = await getAllPosts();
  return posts.map((post) => ({ slug: post.slug }));
}
```

At build time Next.js renders one static page per returned object. For catch-all routes return arrays:

```tsx
export function generateStaticParams() {
  return [{ slug: ["getting-started"] }, { slug: ["guides", "caching"] }];
}
```

What happens to a slug you did **not** list is controlled by `dynamicParams`:

```tsx
export const dynamicParams = false; // unlisted slugs → 404
// default is true: unlisted slugs render on demand
```

If a parent segment also has dynamic params, children's `generateStaticParams` can read the parent's values. Rendering consequences (static vs dynamic, ISR) are in [Static Rendering](../04-rendering/01-static-rendering.md). Behavior here has shifted between versions, particularly with Cache Components, so verify against the docs for yours.

## Precedence

```text
app/blog/new/page.tsx          static      /blog/new
app/blog/[slug]/page.tsx       dynamic     /blog/anything-else
app/blog/[...rest]/page.tsx    catch-all   /blog/a/b
```

More specific wins. A plain `[slug]` and a `[...slug]` / `[[...slug]]` at the same level conflict, so choose one.

## When to use which

- **`[slug]`**: a known number of variable parts (resource by id or slug).
- **`[...slug]`**: arbitrary-depth paths where there is always at least one segment (docs trees, file paths).
- **`[[...slug]]`**: the same, but the bare path should also render this page (e.g. `/shop` and `/shop/a/b` share one component).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Not awaiting `params` | Type error or runtime warning on Next 15+ | `const { slug } = await params` |
| Assuming `params.id` is a number | `"42" + 1 === "421"` | Convert and validate |
| No data check for the param | Page renders with `undefined`, or throws | Call `notFound()` when data is missing |
| Reading unvalidated params in a query | Injection risk | Validate with a schema before use |
| `[[...slug]]` next to a `page.tsx` for the same path | Two pages resolve to one URL | Remove the explicit page |
| `generateStaticParams` returns strings instead of objects | Build error | Return `[{ slug: "a" }]` |
| Catch-all expects a string | `slug.split` fails | It is an array; `join` it |

## Quick Summary

- `[x]` = one segment, `[...x]` = one or more, `[[...x]]` = zero or more.
- Values are strings (or string arrays) and arrive in a Promise in Next 15+.
- Always handle "no such resource" with `notFound()`.
- `generateStaticParams` pre-renders known values; `dynamicParams` controls the rest.
- Static beats dynamic beats catch-all in matching.

## Next

- [Route Groups](./02-route-groups.md)
- [Static Rendering](../04-rendering/01-static-rendering.md)
- [Routes and Params typing](../13-typescript/01-routes-and-params.md)
