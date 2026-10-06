# Intercepting Routes

An intercepting route lets you load a route from another part of the app **inside the current page**, without leaving it. The classic case is a modal: clicking a photo in a gallery opens the photo in a modal over the gallery, but the URL becomes `/photo/42`, so it is shareable. If someone opens that URL directly (or refreshes), they get the full photo page instead.

## The idea

```text
Soft navigation  (click a <Link>)   → intercepted: show the route in a modal over the current page
Hard navigation  (reload / paste URL) → not intercepted: show the full page
```

Same URL, two presentations. That is what makes it useful for modals, quick previews and login dialogs.

## The convention

An intercepting folder name starts with a marker that says **how many route segments up** to find the route being intercepted:

| Marker | Intercepts a route... |
|---|---|
| `(.)name` | at the **same** level |
| `(..)name` | **one** level up |
| `(..)(..)name` | **two** levels up |
| `(...)name` | from the **root** `app/` |

Important: the levels count **route segments**, not file system folders. Slots (`@modal`) and route groups (`(group)`) are not segments, so they do not count.

## Example: photo modal

Goal: `/` shows a gallery. Clicking a photo opens `/photo/[id]` in a modal. Direct visits to `/photo/[id]` show a full page.

```text
app/
├── layout.tsx                      renders {children} and {modal}
├── page.tsx                        gallery
├── photo/
│   └── [id]/
│       └── page.tsx                full photo page (direct visit / refresh)
└── @modal/
    ├── default.tsx                 returns null (no modal by default)
    └── (.)photo/
        └── [id]/
            └── page.tsx            modal version (intercepts /photo/[id])
```

`(.)photo` sits in `@modal`, which is next to `photo` at the same segment level, so `(.)` is correct.

### 1. Layout renders the slot

```tsx
// app/layout.tsx
export default function RootLayout({
  children,
  modal,
}: {
  children: React.ReactNode;
  modal: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body>
        {children}
        {modal}
      </body>
    </html>
  );
}
```

### 2. Default slot renders nothing

```tsx
// app/@modal/default.tsx
export default function Default() {
  return null;
}
```

### 3. The gallery links normally

```tsx
// app/page.tsx
import Link from "next/link";

export default function Gallery() {
  return (
    <ul>
      {[1, 2, 3].map((id) => (
        <li key={id}>
          <Link href={`/photo/${id}`}>Photo {id}</Link>
        </li>
      ))}
    </ul>
  );
}
```

### 4. The intercepted (modal) version

```tsx
// app/@modal/(.)photo/[id]/page.tsx
import { Modal } from "@/components/modal";

export default async function PhotoModal({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  return (
    <Modal>
      <h2>Photo {id}</h2>
    </Modal>
  );
}
```

```tsx
// components/modal.tsx
"use client";

import { useRouter } from "next/navigation";

export function Modal({ children }: { children: React.ReactNode }) {
  const router = useRouter();
  return (
    <div role="dialog" aria-modal="true" className="backdrop" onClick={() => router.back()}>
      <div className="panel" onClick={(e) => e.stopPropagation()}>
        {children}
        <button onClick={() => router.back()}>Close</button>
      </div>
    </div>
  );
}
```

### 5. The full page for direct visits

```tsx
// app/photo/[id]/page.tsx
export default async function PhotoPage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  return (
    <main>
      <h1>Photo {id}</h1>
    </main>
  );
}
```

## How closing works

The modal is just a route, so closing means navigating away. `router.back()` returns to the gallery and the slot goes back to `default.tsx` (`null`). Because the history entry is real, the browser back button closes it too.

If the modal lingers when you navigate with a `<Link>` to an unrelated route, add a catch-all in the slot that renders nothing:

```text
app/@modal/[...catchAll]/page.tsx    → export default function CatchAll() { return null; }
```

## Other use cases

- **Login or sign-up modal** with a real `/login` page for direct visits
- **Quick view** of a product from a listing, with a full product page behind the same URL
- **Side panels** for item details in a list

## Accessibility

A modal built from a route still needs modal behavior: a dialog role, focus moved into it and restored on close, Escape to close, and background content inert. Use the native `<dialog>` element or an accessible dialog library, not a plain `div`.

## Rules and limits

- Interception applies to **soft navigations** only. Reload or open in a new tab shows the real route.
- You must provide the real (non-intercepted) route too; without it, direct URLs 404.
- The marker is relative to route segments, ignoring `@slots` and `(groups)`.
- Pair it with a parallel-route slot and a `default.tsx` that returns `null`.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Missing real route (`photo/[id]/page.tsx`) | Refresh gives 404 | Add the full page |
| Wrong marker level | Intercept never triggers | Count route segments, ignoring slots/groups |
| No `default.tsx` in `@modal` | Build error or 404 on hard navigation | Add `default.tsx` returning `null` |
| Modal stays open after navigating elsewhere | Slot keeps its last state on soft navigation | Add a `[...catchAll]` page returning `null` |
| Using `router.push("/")` to close | Adds history entries | Use `router.back()` |
| Div-only modal | Keyboard and screen-reader problems | Use `<dialog>` or an accessible library |

## Quick Summary

- Intercepting routes show another route's content within the current page on soft navigation, keeping a shareable URL.
- Markers `(.)`, `(..)`, `(..)(..)`, `(...)` count route segments, not folders.
- Use with a `@modal` slot, a `default.tsx` returning `null`, and the real route for direct visits.
- Close with `router.back()`; make the dialog accessible.

## Next

- [03 · Components](../03-components/README.md)
- [Parallel Routes](./06-parallel-routes.md)
- [Navigation](./03-navigation.md)
