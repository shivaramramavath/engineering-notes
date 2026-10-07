# Server Components

A Server Component is a React component that runs **only on the server**. Its code never ships to the browser, only its rendered output does. In the App Router, every component is a Server Component unless you opt out with `"use client"`.

## Why they exist

Traditional React sends all component code to the browser, which then fetches data and renders. Server Components flip the default:

| Benefit | Why |
|---|---|
| **Less JavaScript** | Component code and its dependencies (a Markdown parser, a date library) stay on the server |
| **Direct data access** | Query a database or call internal services without an API layer |
| **Secrets stay safe** | API keys and tokens never reach the client |
| **Faster first content** | Data is fetched near the data source, and output streams as it is ready |
| **Simpler data flow** | `async/await` in the component instead of `useEffect` + loading state |

## The basic example

```tsx
// app/posts/page.tsx  (no directive → Server Component)
import { db } from "@/lib/db";

export default async function PostsPage() {
  const posts = await db.post.findMany({ orderBy: { createdAt: "desc" } });

  return (
    <ul>
      {posts.map((post) => (
        <li key={post.id}>{post.title}</li>
      ))}
    </ul>
  );
}
```

Notice: the component is `async`, it awaits the database directly, and no `useEffect`, loading state or API route is involved. The browser receives plain rendered output.

## What you can do

- Make the component `async` and `await` data
- Access databases, file system, environment secrets
- Read request data via `cookies()`, `headers()` (both async in Next 15+)
- Import heavy server-only libraries at no client cost
- Render other Server Components and Client Components
- Pass props, including JSX, to Client Components

```tsx
import { cookies } from "next/headers";

export default async function Greeting() {
  const cookieStore = await cookies();
  const theme = cookieStore.get("theme")?.value ?? "light";
  return <p>Theme: {theme}</p>;
}
```

## What you cannot do

Server Components run once per render on the server, with no browser and no persistent state, so:

| Not allowed | Reason | Alternative |
|---|---|---|
| `useState`, `useReducer` | No client-side re-rendering | Move to a Client Component |
| `useEffect`, `useLayoutEffect` | No lifecycle in the browser | Client Component |
| Event handlers (`onClick`, `onChange`) | No interactivity on the server | Client Component |
| Browser APIs (`window`, `localStorage`, `document`) | Not available on the server | Client Component |
| `useContext` / `createContext` | Context needs client-side rendering | Client Component provider |
| Class components with state | Same as hooks | Client Component |

Trying one of these produces a build error telling you to add `"use client"`.

## How rendering works

On the server, React renders your Server Components into a special **RSC payload**: a compact description of the UI tree that includes rendered output, placeholders for Client Components, and the props passed to them.

```text
Initial page load
─────────────────
Server:  render Server Components → RSC payload
         pre-render HTML from payload (+ Client Components' first render)
Browser: receives HTML → shows it immediately (non-interactive)
         receives RSC payload → reconciles the component tree
         loads JS for Client Components → hydrates (becomes interactive)

Later navigations
─────────────────
Browser asks for the RSC payload of the changed segments only
React updates the page in place; no full reload
```

Rendering is split by **route segment** and by **Suspense boundary**, so slow parts can stream later without blocking the rest. See [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md).

Server Components render at **build time** for static routes and **per request** for dynamic ones; what makes a route dynamic is explained in [Rendering Overview](../04-rendering/00-rendering-overview.md).

## Data fetching shape

Fetch where the data is used. Two sibling components can each fetch what they need; identical `fetch` requests in one render pass are deduplicated automatically, and you can wrap other data functions in React's `cache()`:

```tsx
// lib/data.ts
import { cache } from "react";
import { db } from "@/lib/db";

export const getUser = cache(async (id: string) => {
  return db.user.findUnique({ where: { id } });
});
```

Both `<Header />` and `<Profile />` can call `getUser(id)` and the query runs once per request. More in [Server Fetching](../05-data-fetching/00-server-fetching.md).

## Keeping server code on the server

Mark modules that must never reach the client:

```bash
npm install server-only
```

```ts
// lib/secrets.ts
import "server-only";

export const stripeKey = process.env.STRIPE_SECRET_KEY!;
```

If a Client Component (directly or transitively) imports this file, the build fails instead of leaking the code. Use it for database clients, secret access and anything that touches private data.

## Server Components vs SSR

These are different ideas and are often confused:

| | Server Components | SSR (server-side rendering) |
|---|---|---|
| What it is | A kind of component that never runs in the browser | Rendering HTML on the server for any component |
| Hydrated in the browser? | **No**, no client JS for it | Yes, components hydrate |
| Applies to | Server Components | Client Components too (they are pre-rendered) |

A page can be server-rendered HTML that contains both Server Components (no JS) and Client Components (JS, hydrated).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Using `useState` / `onClick` | Build error about `"use client"` | Extract a Client Component |
| Importing a Server Component into a Client Component file | It silently becomes a client component | Pass it as `children` or a prop instead (see [Composition Patterns](./03-composition-patterns.md)) |
| Passing a function or class instance as a prop to a Client Component | Serialization error | Pass plain data; see [Server vs Client](./02-server-vs-client.md) |
| Forgetting `await` on `cookies()`/`headers()`/`params` (Next 15+) | Type error or warning | `await` them |
| Fetching in a Client Component what the server could provide | Extra round trip, more JS | Fetch on the server, pass props |
| Secret read in a shared module | Exposed to client or build error | `import "server-only"` |
| `console.log` not visible in the browser | Server Component logs go to the terminal | Check the server console |

## Quick Summary

- Server Components are the default; they run only on the server and ship no component JS.
- They can be `async`, query data directly and use secrets.
- They cannot use state, effects, event handlers, browser APIs or context.
- Output is an RSC payload plus HTML; navigations fetch only changed segments.
- Protect server-only modules with `import "server-only"`.
- Server Components are not the same thing as SSR.

## Next

- [Client Components](./01-client-components.md)
- [Server vs Client](./02-server-vs-client.md)
- [Server Fetching](../05-data-fetching/00-server-fetching.md)
