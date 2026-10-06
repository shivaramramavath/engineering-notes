# Error and Not Found

Things fail: a database is down, a record does not exist, a component throws. The App Router gives each route segment its own failure UI through `error.tsx`, `global-error.tsx` and `not-found.tsx`, so one broken part of a page does not take down the whole app.

## Two kinds of problems

| Kind | Examples | How to handle |
|---|---|---|
| **Expected** | Validation failed, resource missing, "email already used" | Handle in code: return values, `notFound()` |
| **Unexpected** | Bug, network failure, thrown exception | Let it throw; an error boundary (`error.tsx`) catches it |

Do not use exceptions for expected cases. For forms and Server Actions, return the error as data; see [Action Errors](../07-server-actions/04-action-errors.md).

## `error.tsx`: unexpected errors

`error.tsx` wraps a segment in a React error boundary. It **must be a Client Component**.

```tsx
// app/dashboard/error.tsx
"use client";

import { useEffect } from "react";

export default function Error({
  error,
  reset,
}: {
  error: Error & { digest?: string };
  reset: () => void;
}) {
  useEffect(() => {
    console.error(error);
  }, [error]);

  return (
    <div>
      <h2>Something went wrong</h2>
      <button onClick={() => reset()}>Try again</button>
    </div>
  );
}
```

- `error`: the thrown error. In production, errors thrown in **Server Components** are sanitized to avoid leaking details; you get a generic message and a `digest` hash. Match the `digest` against your server logs to find the real error.
- `reset()`: re-renders the segment to try again. It only helps if the failure was transient.

### Where it applies

```text
app/
├── layout.tsx
└── dashboard/
    ├── layout.tsx          ← errors here are NOT caught by dashboard/error.tsx
    ├── error.tsx           ← catches errors in page and children
    └── page.tsx
```

- The boundary catches errors in the segment's `page` and in nested segments.
- It does **not** catch errors in the `layout` of the same segment, because the boundary sits inside the layout. Put an `error.tsx` in the parent segment to cover that layout.
- Errors bubble up to the nearest `error.tsx` above the failing segment.
- The segment's layout stays visible, so navigation still works around the error UI.

### What error boundaries do not catch

- Errors in **event handlers** (`onClick`): handle those with `try/catch`.
- Errors in asynchronous code that is not part of rendering (timers, promises you do not await).
- Errors thrown during `redirect()` / `notFound()`: those are control flow, not failures.

## `global-error.tsx`: errors in the root layout

If the root layout itself fails, there is nothing above it to show UI. `global-error.tsx` replaces the entire root layout, so it must define its own `<html>` and `<body>`:

```tsx
// app/global-error.tsx
"use client";

export default function GlobalError({
  error,
  reset,
}: {
  error: Error & { digest?: string };
  reset: () => void;
}) {
  return (
    <html>
      <body>
        <h2>Something went seriously wrong</h2>
        <button onClick={() => reset()}>Try again</button>
      </body>
    </html>
  );
}
```

It is a last resort, and it is only active in production. Keep it simple and dependency-free.

## `not-found.tsx` and `notFound()`

Two triggers render the closest `not-found.tsx`:

1. Calling `notFound()` in your code (resource does not exist).
2. A URL that matches no route, which uses the **root** `app/not-found.tsx`.

```tsx
// app/blog/[slug]/page.tsx
import { notFound } from "next/navigation";

export default async function Post({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const post = await getPost(slug);
  if (!post) notFound();
  return <h1>{post.title}</h1>;
}
```

```tsx
// app/blog/not-found.tsx
import Link from "next/link";

export default function BlogNotFound() {
  return (
    <div>
      <h2>Post not found</h2>
      <Link href="/blog">Back to all posts</Link>
    </div>
  );
}
```

```tsx
// app/not-found.tsx  (global 404)
export default function NotFound() {
  return <h1>404: Page not found</h1>;
}
```

- `not-found.tsx` is a Server Component by default and wraps with the layouts above it.
- `notFound()` throws; like `redirect()`, do not swallow it in `try/catch`.
- A segment-level `not-found.tsx` only applies to `notFound()` calls inside that segment. Unmatched URLs always use the root one.
- Status code: `notFound()` yields **404** when the response has not started streaming. If streaming already began, the status is already 200 and Next.js adds a `noindex` meta tag instead.

## Loading and failure together

```text
layout
 └─ error boundary   (error.tsx)
     └─ Suspense      (loading.tsx)
         └─ not-found boundary
             └─ page
```

This order explains why a `loading.tsx` shows while data streams and `error.tsx` appears if that streaming work throws. See [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md).

## Reporting errors

`console.error` in `error.tsx` only reaches the user's browser. To know about production errors, send them to a monitoring service from the server (and from the client boundary if needed). See [Monitoring](../22-production/07-monitoring.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `error.tsx` without `"use client"` | Build error | Add the directive |
| Expecting `error.tsx` to catch layout errors | Error bubbles past it | Add `error.tsx` in the parent segment |
| Wrapping `notFound()` or `redirect()` in `try/catch` | 404 or redirect never happens | Call outside the `try` or rethrow |
| Seeing a generic message in production | Server error text is sanitized | Use the `digest` to find it in server logs |
| Throwing for validation errors | Whole segment replaced by error UI | Return the error as data |
| Expecting event-handler errors to hit `error.tsx` | Nothing happens | `try/catch` inside the handler |
| `global-error.tsx` missing `<html>`/`<body>` | Blank page on a root failure | Include both |
| Custom 404 not shown for bad URLs | Created in a nested folder | Put it at `app/not-found.tsx` |

## Quick Summary

- Handle expected problems in code; let unexpected ones throw to an error boundary.
- `error.tsx` (Client Component) catches errors in its segment's page and children, not its own layout.
- `global-error.tsx` replaces the root layout and needs its own `<html>`/`<body>`.
- `notFound()` plus `not-found.tsx` handle missing resources; the root `not-found.tsx` handles unmatched URLs.
- In production, server error messages are hidden; correlate with the `digest`.

## Next

- [Parallel Routes](./06-parallel-routes.md)
- [Action Errors](../07-server-actions/04-action-errors.md)
- [Common Errors](../19-debugging/02-common-errors.md)
