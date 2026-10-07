# Server Fetching

The default way to load data in the App Router is inside a Server Component: make it `async`, `await` the data, and render. No `useEffect`, no loading state plumbing, no separate API layer. The data stays on the server, along with any secrets used to get it.

## The basics

```tsx
// app/posts/page.tsx
type Post = { id: number; title: string };

export default async function PostsPage() {
  const res = await fetch("https://api.example.com/posts");
  if (!res.ok) throw new Error(`Failed to load posts (${res.status})`);
  const posts: Post[] = await res.json();

  return (
    <ul>
      {posts.map((p) => (
        <li key={p.id}>{p.title}</li>
      ))}
    </ul>
  );
}
```

Anything you can do in Node works here: `fetch`, an ORM, a file read, an SDK call.

## Querying a database directly

No API endpoint is needed between your component and your data:

```tsx
// app/users/page.tsx
import { db } from "@/lib/db";

export default async function UsersPage() {
  const users = await db.user.findMany({
    select: { id: true, name: true }, // only what the UI needs
    orderBy: { name: "asc" },
  });

  return (
    <ul>
      {users.map((u) => (
        <li key={u.id}>{u.name}</li>
      ))}
    </ul>
  );
}
```

Select only the fields you render. Anything you pass to a Client Component as props is sent to the browser. Database setup is covered in [Database Architecture](../12-database/00-database-architecture.md).

## `fetch` in Next.js

Next.js extends the standard `fetch` with options for caching and revalidation:

```ts
// cache for an hour, tag it for on-demand invalidation
await fetch(url, { next: { revalidate: 3600, tags: ["posts"] } });

// opt in to caching explicitly
await fetch(url, { cache: "force-cache" });

// never reuse a stored result
await fetch(url, { cache: "no-store" });
```

Since Next.js 15, `fetch` responses are **not cached by default**; you opt in. Whether and how a result is stored affects whether the route is static or dynamic, and what refreshes it. That is the subject of [Caching Overview](../06-caching/00-caching-overview.md) and [Revalidation](../06-caching/04-revalidation.md). With Cache Components enabled (the default in new projects per the Next.js 16.4 docs), caching is controlled with the `use cache` directive instead of `fetch` options: fetches inside a `use cache` scope are cached, and uncached fetches run at request time and must sit inside `<Suspense>` (or be cached) so the rest of the page can prerender. See [Cache Components](../06-caching/05-cache-components.md).

## Request deduplication

If several components in one render request the same data, you do not need to hoist the fetch to a common parent or pass props down.

- Identical `fetch` calls (same URL and options, GET requests) during one render are **memoized** and executed once.
- For anything that is not `fetch` (ORM calls, SDKs), wrap the function with React's `cache()`:

```ts
// lib/data/user.ts
import "server-only";
import { cache } from "react";
import { db } from "@/lib/db";

export const getUser = cache(async (id: string) => {
  return db.user.findUnique({ where: { id } });
});
```

```tsx
// Both can call getUser(id); the query runs once per request.
<Header userId={id} />
<Profile userId={id} />
```

This memoization lasts for a single request and render pass. It is not a cross-request cache.

## Organize data access in one place

Scattering raw queries through components makes security and changes hard. A common pattern: a small data layer of server-only functions that components import.

```text
lib/data/
├── posts.ts       getPost, getPosts, getPostsByAuthor
├── users.ts       getUser, getCurrentUser
└── orders.ts
```

```ts
// lib/data/posts.ts
import "server-only";
import { cache } from "react";
import { db } from "@/lib/db";

export const getPost = cache(async (slug: string) => {
  return db.post.findUnique({
    where: { slug },
    select: { id: true, title: true, body: true },
  });
});
```

Benefits: one place to apply authorization checks, DTO shaping (return only safe fields), and `server-only` protection. See [Auth Architecture](../11-authentication/00-auth-architecture.md).

## Dynamic data and request context

Use request-specific data (cookies, headers) to personalize the fetch. This makes the route dynamic:

```tsx
import { cookies } from "next/headers";

export default async function Account() {
  const token = (await cookies()).get("session")?.value;
  const res = await fetch("https://api.example.com/me", {
    headers: { Authorization: `Bearer ${token}` },
    cache: "no-store",
  });
  const me = await res.json();
  return <h1>{me.name}</h1>;
}
```

See [Dynamic Rendering](../04-rendering/02-dynamic-rendering.md).

## Loading and error states

You do not write `isLoading` flags. Instead:

- **Loading:** wrap the slow component in `<Suspense>` or add a `loading.tsx` ([Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md)).
- **Errors:** throw, and the nearest `error.tsx` handles it ([Error and Not Found](../02-routing/05-error-and-not-found.md)).
- **Missing data:** call `notFound()`.

```tsx
import { notFound } from "next/navigation";

const post = await getPost(slug);
if (!post) notFound();
```

Always check `res.ok` for `fetch`; a 404 or 500 does not throw by itself.

## Validate what you fetch

`await res.json()` is typed as `any`. Casting with `as Post[]` is a promise, not a check. For external APIs, validate:

```ts
import { z } from "zod";

const PostSchema = z.object({ id: z.number(), title: z.string() });
const posts = z.array(PostSchema).parse(await res.json());
```

## Where in the tree to fetch

Fetch **in the component that uses the data**, not at the top "just in case". Benefits: less prop drilling, each section can stream independently, and deduplication makes it cheap. Layouts and pages both can fetch, but a layout cannot pass data to its page; each fetches what it needs.

## Do not call your own Route Handlers from Server Components

```tsx
// Avoid
const res = await fetch("http://localhost:3000/api/posts");

// Prefer: call the same function the handler uses
const posts = await getPosts();
```

Calling your own endpoint adds a network hop and needs an absolute URL, and it fails at build time when the server is not running. Share the underlying function between the Server Component and the Route Handler instead. See [Route Handlers](../08-route-handlers-and-proxy/00-route-handlers.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Fetching with `useEffect` in a client component what the server could load | Extra round trip, loading flash | Fetch in a Server Component and pass props |
| Not checking `res.ok` | JSON parse errors or silent bad data | Check status; throw on failure |
| Assuming `fetch` is cached (Next 14 habits) | Unexpected extra requests | Opt in with `cache`/`next.revalidate` or `use cache` |
| Hoisting every fetch to the page and drilling props | Hard to maintain, blocks streaming | Fetch where used; rely on deduplication |
| Passing whole DB records to Client Components | Private fields exposed | Select fields; return DTOs |
| Fetching `localhost` API routes at build time | Build failure | Call functions directly |
| Using `cache()` and expecting cross-request caching | Data refetched each request | `cache()` is per-request only; see [Caching](../06-caching/00-caching-overview.md) |
| Secret in a shared module | Leaks to the client bundle | `import "server-only"` |

## Quick Summary

- Fetch in `async` Server Components; query databases directly when appropriate.
- `fetch` caching is opt-in in Next.js 15+; revalidation and tags are options on `fetch`.
- Same-render duplicates are deduplicated for `fetch`; use `cache()` for other functions.
- Centralize data access in server-only functions that return only safe fields.
- Handle loading with Suspense, errors with `error.tsx`, missing data with `notFound()`.
- Do not fetch your own Route Handlers from Server Components.

## Next

- [Client Fetching](./01-client-fetching.md)
- [Parallel and Sequential](./02-parallel-and-sequential.md)
- [Caching Overview](../06-caching/00-caching-overview.md)