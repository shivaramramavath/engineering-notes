# App Router

The App Router is Next.js's routing and rendering system, based on an `app/` directory. Folders define URLs, special files define what each route shows, and components are Server Components unless you say otherwise. It is built on React features: Server Components, Suspense, and Server Functions.

## The core rule: folders are routes

```text
app/
├── page.tsx              →  /
├── about/
│   └── page.tsx          →  /about
└── blog/
    ├── page.tsx          →  /blog
    └── [slug]/
        └── page.tsx      →  /blog/hello-world
```

A folder is a URL **segment**. A segment is only publicly accessible once it contains a `page.tsx` (or `route.ts`). Folders without one are just organization.

## Your first routes

```tsx
// app/layout.tsx  (required root layout)
export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
```

```tsx
// app/page.tsx
export default function HomePage() {
  return <h1>Home</h1>;
}
```

```tsx
// app/about/page.tsx
import Link from "next/link";

export default function AboutPage() {
  return (
    <>
      <h1>About</h1>
      <Link href="/">Back home</Link>
    </>
  );
}
```

Visit `/about`. No registration, no config.

## Server Components by default

Every component in `app/` is a Server Component unless it (or a parent import chain) is marked `"use client"`. That has practical consequences:

```tsx
// app/posts/page.tsx  (a Server Component)
export default async function PostsPage() {
  const res = await fetch("https://api.example.com/posts");
  const posts: { id: number; title: string }[] = await res.json();

  return (
    <ul>
      {posts.map((p) => (
        <li key={p.id}>{p.title}</li>
      ))}
    </ul>
  );
}
```

The component is `async`, awaits data directly, and ships no JavaScript to the browser. You can also query a database here or read secrets safely.

To use state, effects, or browser APIs, move that part into a Client Component:

```tsx
// app/components/like-button.tsx
"use client";

import { useState } from "react";

export function LikeButton() {
  const [likes, setLikes] = useState(0);
  return <button onClick={() => setLikes(likes + 1)}>Likes: {likes}</button>;
}
```

```tsx
// app/posts/page.tsx
import { LikeButton } from "../components/like-button";

export default function PostsPage() {
  return (
    <>
      <h1>Posts</h1>
      <LikeButton />
    </>
  );
}
```

Keep `"use client"` as low in the tree as practical so most of the page stays on the server. Full treatment: [Server vs Client](../03-components/02-server-vs-client.md).

## Special files

Each route segment can contain files with reserved names that define behavior:

| File | Purpose |
|---|---|
| `page.tsx` | The route's UI; makes the segment public |
| `layout.tsx` | Shared UI wrapping the segment and its children |
| `loading.tsx` | Instant loading UI while the segment streams |
| `error.tsx` | Error UI for the segment |
| `not-found.tsx` | UI for `notFound()` / unmatched content |
| `route.ts` | An HTTP endpoint instead of UI |

The complete list and how they nest is in [Project Structure](./02-project-structure.md).

## How navigation works

```tsx
import Link from "next/link";

<Link href="/blog">Blog</Link>
```

`<Link>` navigates on the client: it prefetches, and on click it requests only the data for segments that changed. **Shared layouts are not re-rendered or remounted**, so their state survives. Details in [Navigation](../02-routing/03-navigation.md).

## Route parameters

Dynamic segments use brackets. In Next.js 15 and later, `params` is a **Promise**:

```tsx
// app/blog/[slug]/page.tsx
export default async function PostPage({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  return <h1>Post: {slug}</h1>;
}
```

More in [Dynamic Routes](../02-routing/01-dynamic-routes.md).

## Rules worth remembering

- Only `page.tsx` and `route.ts` make a segment reachable. Other files in a folder (components, utilities, tests) are safe to colocate.
- A segment cannot contain both `page.tsx` and `route.ts` for the same HTTP path.
- Hooks (`useState`, `useEffect`) and event handlers only work in Client Components.
- A root `app/layout.tsx` with `<html>` and `<body>` is required.
- A route cannot exist in both `app/` and `pages/` at the same URL; the build reports a conflict.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Using `useState` in a Server Component | Build error mentioning `"use client"` | Move interactivity to a Client Component |
| Putting `"use client"` on the page for convenience | Whole page ships as JS | Extract only the interactive leaf |
| Folder without `page.tsx` | 404 on that URL | Add `page.tsx` |
| Reading `params.slug` without `await` (Next 15+) | Type error / warning | `const { slug } = await params` |
| Importing a server-only module into a Client Component | Build error or leaked server code | Keep server logic out of client imports |
| Using `next/router` | Error in `app/` | Use hooks from `next/navigation` |

## Quick Summary

- `app/` folders map to URLs; `page.tsx` makes a route public.
- Components are Server Components by default; add `"use client"` only where you need interactivity.
- Special files (`layout`, `loading`, `error`, `not-found`, `route`) add behavior per segment.
- `<Link>` does client-side navigation and preserves shared layouts.
- `params` and `searchParams` are Promises in 15+.

## Next

- [Project Structure](./02-project-structure.md)
- [Pages and Layouts](./03-pages-and-layouts.md)
- [File System Routing](../02-routing/00-file-system-routing.md)