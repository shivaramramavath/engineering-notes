# Composition Patterns

Real pages mix Server and Client Components. These patterns show how to combine them without turning everything into client code. Each one solves a specific, recurring situation.

## 1. Push the client boundary to the leaves

Extract only the interactive part into its own file.

```tsx
// app/blog/[slug]/page.tsx  (Server)
import { LikeButton } from "./like-button";

export default async function Post({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const post = await getPost(slug);

  return (
    <article>
      <h1>{post.title}</h1>
      <div dangerouslySetInnerHTML={{ __html: post.html }} />
      <LikeButton postId={post.id} />
    </article>
  );
}
```

```tsx
// app/blog/[slug]/like-button.tsx
"use client";

import { useState } from "react";

export function LikeButton({ postId }: { postId: string }) {
  const [liked, setLiked] = useState(false);
  return <button onClick={() => setLiked(!liked)}>{liked ? "Liked" : "Like"}</button>;
}
```

The article body and the Markdown/HTML processing stay on the server.

## 2. Server Components as children of Client Components

A Client Component can **render** Server Components if they are passed in as `children` (or any JSX prop). The server renders them first and hands the result to the client component as a slot.

```tsx
// components/collapsible.tsx
"use client";

import { useState } from "react";

export function Collapsible({ title, children }: { title: string; children: React.ReactNode }) {
  const [open, setOpen] = useState(false);
  return (
    <section>
      <button onClick={() => setOpen(!open)}>{title}</button>
      {open && children}
    </section>
  );
}
```

```tsx
// app/page.tsx  (Server)
import { Collapsible } from "@/components/collapsible";
import { SlowServerData } from "./slow-server-data"; // Server Component

export default function Page() {
  return (
    <Collapsible title="Details">
      <SlowServerData />
    </Collapsible>
  );
}
```

`SlowServerData` stays a Server Component, even though it appears inside a Client Component's output. This works because the parent (a Server Component) decides what `children` is. The Client Component never imports it.

What does **not** work:

```tsx
"use client";
import { SlowServerData } from "./slow-server-data"; // now client code
```

This is the same technique used for modals, tabs, accordions, sidebars and any wrapper with interactive behavior.

## 3. Context providers without losing server rendering

Context needs a Client Component. Wrap it once and place it in the layout. The page content passed as `children` stays server-rendered:

```tsx
// app/providers.tsx
"use client";

import { ThemeProvider } from "next-themes";

export function Providers({ children }: { children: React.ReactNode }) {
  return <ThemeProvider attribute="class">{children}</ThemeProvider>;
}
```

```tsx
// app/layout.tsx  (Server)
import { Providers } from "./providers";

export default function Layout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body>
        <Providers>{children}</Providers>
      </body>
    </html>
  );
}
```

Render providers as deep in the tree as they are needed; wrapping the whole `<html>` is rarely necessary. Multiple providers can be composed inside the single `Providers` component. See [Context](../10-state-management/01-context.md).

## 4. Third-party components that need the client

Many libraries use state or effects but do not include `"use client"`. Importing them directly into a Server Component fails. Create a thin wrapper:

```tsx
// components/carousel.tsx
"use client";

import { Carousel as ThirdPartyCarousel } from "acme-carousel";

export const Carousel = ThirdPartyCarousel;
```

Now Server Components can import `Carousel` from your wrapper:

```tsx
import { Carousel } from "@/components/carousel";
```

Libraries that already include the directive need no wrapper.

## 5. Passing server data to the client

Fetch on the server and pass the minimum the client needs:

```tsx
// Server
const user = await getUser();
return <ProfileForm initialName={user.name} initialBio={user.bio} />;
```

Pass specific fields instead of the full record to avoid shipping private data and to keep the payload small.

### Streaming data to a Client Component with `use`

You can pass an **unawaited Promise** as a prop and read it in the client with React's `use` hook. The page renders immediately and the client component suspends until the data arrives:

```tsx
// app/posts/page.tsx  (Server)
import { Suspense } from "react";
import { Posts } from "./posts";

export default function Page() {
  const postsPromise = getPosts(); // do NOT await
  return (
    <Suspense fallback={<p>Loading…</p>}>
      <Posts postsPromise={postsPromise} />
    </Suspense>
  );
}
```

```tsx
// app/posts/posts.tsx
"use client";

import { use } from "react";

export function Posts({ postsPromise }: { postsPromise: Promise<{ id: string; title: string }[]> }) {
  const posts = use(postsPromise);
  return (
    <ul>
      {posts.map((p) => (
        <li key={p.id}>{p.title}</li>
      ))}
    </ul>
  );
}
```

Useful when the client needs the data for interactivity (filtering, sorting) but you still want server-side fetching. See [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md).

## 6. Sharing data between Server Components

Server Components cannot share state or context. Instead, call the same data function wherever it is needed:

- Identical `fetch` calls in one render pass are deduplicated automatically.
- For database calls and other non-`fetch` work, wrap the function in React's `cache()`:

```ts
// lib/user.ts
import { cache } from "react";

export const getCurrentUser = cache(async () => {
  return db.user.findUnique({ where: { id: await getSessionUserId() } });
});
```

Layout, page and nested components can all call `getCurrentUser()` and the query runs once per request. Do not try to pass data from a layout to its page through props; neither can pass props to the other.

## 7. Server-only and client-only modules

```ts
// lib/db.ts
import "server-only";
```

Add this to modules that touch secrets or databases so an accidental client import fails the build. See [Server vs Client](./02-server-vs-client.md).

## 8. Browser-only widgets

For a component that cannot render on the server (touches `window` at import time), load it client-side only, from within a Client Component:

```tsx
"use client";

import dynamic from "next/dynamic";

const Chart = dynamic(() => import("./chart"), { ssr: false });

export function ChartSection({ data }: { data: number[] }) {
  return <Chart data={data} />;
}
```

## 9. Interleaving server and client within a page

A typical page composes all of the above:

```text
RootLayout (Server)
└── Providers (Client)                 context only
    └── Page (Server)                  fetches data
        ├── Header (Server)
        ├── Sidebar (Client)           open/close state
        │   └── {children}  ← NavLinks (Server) passed as children
        ├── ArticleBody (Server)
        └── CommentForm (Client)       controlled inputs, Server Action on submit
```

## Anti-patterns

| Anti-pattern | Why it hurts | Instead |
|---|---|---|
| `"use client"` on a page | Whole page + deps shipped to the browser | Extract leaf Client Components |
| Importing a Server Component into a Client file | It becomes client code | Pass it as `children` |
| Fetching in `useEffect` data a Server Component could fetch | Extra round trip and loading flashes | Fetch on the server, pass props |
| One giant Client Component "App" | Loses the whole server model | Split by interactivity |
| Passing entire records as props | Leaks fields, bigger payload | Pass only needed fields |
| Using context to pass server data around | Server Components have no context | Call a cached data function |

## Quick Summary

- Keep interactivity in small client leaves; keep pages and data on the server.
- Pass Server Components into Client Components through `children` or JSX props.
- Wrap providers in one Client Component rendered in the layout.
- Wrap third-party client-only components in a `"use client"` re-export.
- Pass minimal serializable data down; stream with an unawaited Promise and `use` when needed.
- Share server-side data via `fetch` deduplication or `cache()`, not props or context.
- Use `server-only` to guard private modules.

## Next

- [04 · Rendering](../04-rendering/README.md)
- [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md)
- [Context](../10-state-management/01-context.md)
