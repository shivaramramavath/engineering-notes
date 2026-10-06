# Navigation

Navigation in the App Router is client-side: clicking a link requests only the parts of the page that changed and updates in place, while shared layouts keep their state. You will use four tools: `<Link>`, the `useRouter` hook, the URL hooks, and `redirect()` on the server.

## `<Link>`

```tsx
import Link from "next/link";

export function Nav() {
  return (
    <nav>
      <Link href="/">Home</Link>
      <Link href="/blog/hello">Post</Link>
      <Link href={`/products/${id}?tab=reviews`}>Reviews</Link>
    </nav>
  );
}
```

`<Link>` renders an `<a>`, so it is crawlable, keyboard-accessible, and supports open-in-new-tab. Use it for every internal navigation. For external sites, use a plain `<a>`.

Useful props:

| Prop | Effect |
|---|---|
| `href` | Destination (string or URL object) |
| `replace` | Replace the history entry instead of pushing |
| `scroll={false}` | Do not scroll to the top after navigating |
| `prefetch` | Control prefetching (see below) |

### Prefetching

When a `<Link>` enters the viewport, Next.js preloads its route in the background so the click feels instant. This is **production-only**; you will not see it in `next dev`.

- **Static routes** can be prefetched in full.
- **Dynamic routes** are prefetched only up to the nearest `loading.tsx`, so the skeleton shows immediately while data streams.

Disable it for a link if it would trigger heavy or unwanted requests: `<Link prefetch={false} href="...">`. How prefetched data is stored and expires is in [Router Cache](../06-caching/03-router-cache.md).

## `useRouter`: imperative navigation

For navigation triggered by code (after a form, a timer, an event) use the hook from `next/navigation`:

```tsx
"use client";

import { useRouter } from "next/navigation";

export function CreateButton() {
  const router = useRouter();

  async function handleClick() {
    const res = await fetch("/api/items", { method: "POST" });
    const { id } = await res.json();
    router.push(`/items/${id}`);
  }

  return <button onClick={handleClick}>Create</button>;
}
```

| Method | Behavior |
|---|---|
| `push(href)` | Navigate, adding a history entry |
| `replace(href)` | Navigate without adding a history entry |
| `back()` / `forward()` | History navigation |
| `refresh()` | Re-fetch the current route's Server Components and update in place, keeping client state |
| `prefetch(href)` | Preload a route manually |

Prefer `<Link>` whenever the action is just "go there". It is more accessible and prefetches automatically.

> Import from `next/navigation`. The old `next/router` hook belongs to the Pages Router and fails in `app/`.

## Reading the current URL

All of these are Client Component hooks:

```tsx
"use client";

import { usePathname, useSearchParams, useParams } from "next/navigation";

export function Debug() {
  const pathname = usePathname(); // "/blog/hello"
  const searchParams = useSearchParams(); // URLSearchParams
  const params = useParams<{ slug: string }>(); // { slug: "hello" }

  const tab = searchParams.get("tab");
  return <pre>{pathname} {params.slug} {tab}</pre>;
}
```

In Server Components you do not use hooks: `params` and `searchParams` arrive as page props.

### `useSearchParams` and Suspense

In a statically rendered route, `useSearchParams` opts the client part of the page out of static rendering. Wrap the component in a Suspense boundary, otherwise the build can fail with a message about needing a Suspense boundary:

```tsx
import { Suspense } from "react";
import { SearchBox } from "./search-box"; // uses useSearchParams

export default function Page() {
  return (
    <Suspense fallback={null}>
      <SearchBox />
    </Suspense>
  );
}
```

### Updating the query string

```tsx
"use client";

import { usePathname, useRouter, useSearchParams } from "next/navigation";

export function Filter() {
  const router = useRouter();
  const pathname = usePathname();
  const searchParams = useSearchParams();

  function setTab(tab: string) {
    const next = new URLSearchParams(searchParams.toString());
    next.set("tab", tab);
    router.push(`${pathname}?${next.toString()}`);
  }

  return <button onClick={() => setTab("reviews")}>Reviews</button>;
}
```

Keeping state in the URL is covered in [State Overview](../10-state-management/00-state-overview.md).

### Highlighting the active link

A layout does not re-render on navigation, so active state must come from a hook:

```tsx
"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

export function NavLink({ href, children }: { href: string; children: React.ReactNode }) {
  const pathname = usePathname();
  const active = pathname === href;
  return (
    <Link href={href} aria-current={active ? "page" : undefined}>
      {children}
    </Link>
  );
}
```

`useSelectedLayoutSegment()` gives the active child segment of the layout it is called in, useful for tabs.

## Server-side navigation

On the server (Server Components, Server Actions, Route Handlers) navigate with `redirect()`; see [Redirects and Rewrites](./04-redirects-and-rewrites.md).

## Scroll and history behavior

- After navigation, Next.js scrolls to the top of the new page, unless the target is already visible or you pass `scroll={false}`.
- Browser back/forward restores scroll position.
- Layout state, such as an open sidebar or a playing video in a layout, survives navigation.
- A hash link (`/docs#install`) scrolls to the element with that id.

## Type-safe links (optional)

With `typedRoutes: true` in `next.config.ts`, `<Link href>` is checked against your real routes, so typos become type errors. See [next.config](../00-setup/04-next-config.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `<a href="/about">` for internal links | Full page reload, no prefetch | Use `<Link>` |
| `useRouter` from `next/router` | "NextRouter was not mounted" | `next/navigation` |
| Calling `router.push` during render | React warning, loops | Call it in an event handler or effect |
| `useSearchParams` without Suspense | Build error | Wrap in `<Suspense>` |
| Using hooks in a Server Component | Error about client hooks | Add `"use client"` or use page props |
| Prefetch never visible in dev | Prefetching is production-only | Test with `build` + `start` |
| Layout "doesn't update" on navigation | Layouts persist by design | Use a hook in a client child |
| Navigation after a mutation shows stale data | Router cache holds the old page | `router.refresh()` or revalidate on the server |

## Quick Summary

- Use `<Link>` for navigation; it prefetches in production and preserves shared layouts.
- Use `useRouter` from `next/navigation` for code-driven navigation; `refresh()` re-fetches server data.
- Read the URL with `usePathname`, `useSearchParams`, `useParams` (client) or page props (server).
- Wrap `useSearchParams` consumers in Suspense.
- Navigate on the server with `redirect()`.

## Next

- [Redirects and Rewrites](./04-redirects-and-rewrites.md)
- [Router Cache](../06-caching/03-router-cache.md)
- [State Overview](../10-state-management/00-state-overview.md)
