# State Overview

"State management" in a Next.js App Router app is mostly about **not** reaching for a state library. Much of what older React apps kept in a global store now lives somewhere better: the server, the URL, the form, or a single component. This note is the decision guide; the next three cover the tools.

> Verified against the Next.js 16.4 docs (Server and Client Components, SPA patterns, client-side data fetching).

## The first question: what kind of state is it?

| Kind of state | Example | Where it should live |
|---|---|---|
| **Server data** | Posts, products, the current user | **Server Components** (fetch directly), cached with [Caching](../06-caching/README.md) |
| **URL state** | Search text, filters, sort, page, selected tab | **The URL** (`searchParams`, path segments) |
| **Form state** | Field values, validation errors, pending | The form: `FormData`, `useActionState`, `useFormStatus` |
| **Local UI state** | Open/closed, hover, a draft in one component | `useState` / `useReducer` in that component |
| **Shared UI state** | Sidebar open, cart drawer, theme, multi-step wizard | Context or a small store ([Context](./01-context.md), [Zustand](./02-zustand.md)) |
| **Server data that needs live browser behavior** | Polling, infinite scroll, focus refetch, optimistic lists | A client cache ([TanStack Query](./03-tanstack-query.md)) |
| **Session / preferences the server must know** | Auth session, locale, theme | **Cookies** (readable by the server) |
| **Persistent client-only data** | A draft saved across reloads | `localStorage` (client only) behind a hydration-safe pattern |

Most apps need little beyond the first five rows.

## Why the App Router changes things

In a Server Component there is **no state at all**: no `useState`, no context, no effects. A Server Component renders once per request from props and data. That pushes you toward:

1. **Fetch where you render.** Data comes from the server and flows down as props. No store to fill.
2. **Mutate with Server Actions, then revalidate.** The server re-renders with fresh data; you do not patch a client copy. See [Mutations and Optimistic UI](../07-server-actions/03-mutations-and-optimistic-ui.md).
3. **Keep interactivity in small Client Components** ("leaves"), so state stays local and the JavaScript stays small.

```text
Server Component (fetches, no state)
   │ props
   ▼
Client Component (useState for UI details)
```

## The ladder: reach for the lowest rung that works

```text
1. Props from a Server Component
2. The URL
3. useState / useReducer in one component
4. Lift state to the nearest common parent
5. Context (rarely changing, app-wide values)
6. A small store like Zustand (frequently changing client state shared widely)
7. A client data cache like TanStack Query (server data with browser-side behavior)
```

Move up a rung only when the one below genuinely hurts.

## URL state

Anything a user might want to **share, bookmark or reload** belongs in the URL. It also survives refresh for free and works with the back button.

```tsx
// app/products/page.tsx  (Server Component reads the URL)
export default async function Page({
  searchParams,
}: {
  searchParams: Promise<{ q?: string; sort?: string }>;
}) {
  const { q = "", sort = "new" } = await searchParams;
  const products = await getProducts({ q, sort });
  return (
    <>
      <SearchBox />            {/* Client Component writes to the URL */}
      <ProductList products={products} />
    </>
  );
}
```

```tsx
// app/products/search-box.tsx  (Client Component updates the URL)
"use client";

import { usePathname, useRouter, useSearchParams } from "next/navigation";
import { useTransition } from "react";

export function SearchBox() {
  const router = useRouter();
  const pathname = usePathname();
  const searchParams = useSearchParams();
  const [isPending, startTransition] = useTransition();

  function onChange(value: string) {
    const params = new URLSearchParams(searchParams.toString());
    if (value) params.set("q", value);
    else params.delete("q");
    startTransition(() => router.replace(`${pathname}?${params.toString()}`));
  }

  return (
    <input
      defaultValue={searchParams.get("q") ?? ""}
      onChange={(e) => onChange(e.target.value)}
      aria-busy={isPending}
      placeholder="Search…"
    />
  );
}
```

Notes:

- `router.replace` avoids adding a history entry per keystroke; use `push` for discrete changes like switching a tab.
- Debounce typing before updating the URL, since each change triggers a server render.
- `useSearchParams` in a statically prerendered route needs a `<Suspense>` boundary around the component that reads it.
- Parse and validate URL values like any other input; they are user-controlled strings.
- The docs also describe updating the URL without a navigation using `window.history.pushState` / `replaceState`, which Next.js syncs with `useSearchParams`.

## Cookies for state the server needs

If the **server** must know the value to render correctly on the first paint (theme, locale, a feature flag), store it in a cookie, not `localStorage` (the server cannot read that). Set it from a Server Action or Route Handler and read it with `cookies()`. Reading it makes that part of the page request-time. See [Theming](../09-styling-and-assets/03-theming.md) and [Dynamic Rendering](../04-rendering/02-dynamic-rendering.md).

## Local state done well

- **Keep state minimal.** Store the smallest set of facts and **derive** the rest during render. Do not store `filteredItems` next to `items` and `filter`; compute it.
- **Colocate.** Put state in the component that uses it. Lift it only as far as needed.
- **Use `useReducer`** when several fields change together or transitions have rules.
- **Reset with `key`.** Changing a component's `key` remounts it with fresh state: `<Editor key={postId} />`.
- **Do not copy props into state** unless you intentionally want an editable draft that starts from the prop.

## State and navigation

How long client state survives navigation depends on the file convention:

| What | Behavior across navigations |
|---|---|
| **Layout** | Stays mounted. Its Client Component state persists (a sidebar's open state survives) |
| **Page** | Remounts for a different route |
| **`template.tsx`** | Remounts on every navigation, resetting state |
| **Cache Components** | Recently visited routes can be kept hidden with React's `<Activity>`, so state, form values and scroll persist when you return. Code that relied on unmounting to reset state needs an explicit reset. See [Router Cache](../06-caching/03-router-cache.md) |

Use these deliberately: persistent UI state in layouts, per-page state in pages.

## Where each tool fits

| Tool | Best for | Avoid for |
|---|---|---|
| **Props + Server Components** | All server data | Anything that must change instantly in the browser without a round trip |
| **URL** | Shareable view state | Large or private data |
| **`useState` / `useReducer`** | Local UI | State many distant components share |
| **Context** | Slow-changing app-wide values (theme, current user, feature flags) | Frequently changing values; it re-renders every consumer |
| **Zustand** | Shared, frequently updated client UI state (cart drawer, editor state, wizard) | Server data; state only one component uses |
| **TanStack Query** | Browser-side caching, polling, infinite scroll, optimistic lists | Data that is read once on the server and never revalidated on the client |

## Rules of thumb

1. **Do not copy server data into a client store.** You create two sources of truth that drift. Pass it as props, or use a client data cache that is explicitly seeded from the server.
2. **No module-level mutable state on the server.** A server process serves many users; a shared variable or global store written during a request can leak one user's data to another. See [Zustand](./02-zustand.md) for the per-request pattern.
3. **A mutation should end in revalidation,** not manual patching of several copies.
4. **Prefer fewer Client Components.** Each one that needs state should be small and near the leaves; pass Server Components in as `children`. See [Composition Patterns](../03-components/03-composition-patterns.md).
5. **Hydration must match.** Anything that differs between server and first client render (random values, `localStorage`, `window` size, the current time) causes a hydration mismatch. Render a stable value first, then update in an effect, or load it client-only.

## Hydration-safe client-only values

```tsx
"use client";

import { useEffect, useState } from "react";

export function useLocalStorage(key: string, initial: string) {
  const [value, setValue] = useState(initial);   // same on server and first client render

  useEffect(() => {
    const stored = window.localStorage.getItem(key);
    if (stored !== null) setValue(stored);        // update after hydration
  }, [key]);

  function set(next: string) {
    setValue(next);
    window.localStorage.setItem(key, next);
  }
  return [value, set] as const;
}
```

The first paint uses `initial`, then switches to the stored value. If that flash is unacceptable, move the value into a **cookie** so the server can render it correctly.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| "Hydration failed" warning | Render depends on `window`, `localStorage`, `Date.now()` or `Math.random()` | Stable first render, update in `useEffect`, or render client-only |
| `useState` error in a Server Component | Missing `"use client"` | Move the stateful part into a Client Component |
| State resets when navigating | Component lives in a page or `template` | Move it to a layout, or store it in the URL/cookie |
| State persists when you expected a reset | Layout persistence or `<Activity>` under Cache Components | Change `key`, or reset explicitly |
| Two parts of the UI disagree about data | Server data copied into client state | Single source: props or a seeded query cache |
| Filter state lost on refresh | Held in `useState` | Put it in the URL |
| Another user's data appeared | Module-level store/variable mutated on the server | Per-request instances; avoid server-side globals |
| Whole app re-renders on a small change | Large context value or broad store subscription | Split context, or select narrowly in a store |

## Common mistakes

| Mistake | Fix |
|---|---|
| Installing a global store on day one | Start with props, URL and local state |
| Putting fetched data in Redux/Zustand "so everyone can read it" | Fetch in Server Components, pass props |
| Storing derived values in state | Compute them during render |
| Putting everything in one giant context | Split by concern and update frequency |
| Using `localStorage` for something the server needs | Use a cookie |
| Forgetting that layouts keep state | Use `key`, `template.tsx`, or explicit reset |
| Updating the URL on every keystroke without debounce | Debounce, use `replace` |
| Making large parts of the tree Client Components for one stateful widget | Isolate the widget as a small leaf |

## Quick Summary

- First ask what kind of state it is; most of it belongs on the server, in the URL, in a form, or in one component.
- Server Components have no state; fetch there, mutate with Server Actions, then revalidate.
- Use the lowest rung of the ladder that works: props, URL, local state, lifted state, context, store, client data cache.
- Do not duplicate server data in a client store, and never keep per-user data in server-side module state.
- Layouts keep client state across navigations; pages and templates reset it; Cache Components can preserve it.
- Make first renders hydration-safe; use cookies for values the server needs.

## Next

- [Context](./01-context.md)
- [Zustand](./02-zustand.md)
- [TanStack Query](./03-tanstack-query.md)
