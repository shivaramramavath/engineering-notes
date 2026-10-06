# Parallel Routes

Parallel routes let one layout render **several independent pages at the same time**, each with its own loading state, error state and navigation. You define them as **slots** using folders that start with `@`. Think of a dashboard where the analytics panel and the team panel load and fail separately.

## Slots

A slot is a folder named `@something`. It is **not** a URL segment (`@analytics` never appears in a path). The parent layout receives each slot as a prop, next to `children`:

```text
app/dashboard/
├── layout.tsx
├── page.tsx              implicit "children" slot
├── @analytics/
│   ├── page.tsx
│   ├── loading.tsx
│   └── default.tsx
└── @team/
    ├── page.tsx
    └── default.tsx
```

```tsx
// app/dashboard/layout.tsx
export default function DashboardLayout({
  children,
  analytics,
  team,
}: {
  children: React.ReactNode;
  analytics: React.ReactNode;
  team: React.ReactNode;
}) {
  return (
    <div>
      <main>{children}</main>
      <section>{analytics}</section>
      <aside>{team}</aside>
    </div>
  );
}
```

`/dashboard` renders `page.tsx`, `@analytics/page.tsx` and `@team/page.tsx` together. `children` is just an implicit slot (`app/dashboard/page.tsx` is equivalent to `app/dashboard/@children/page.tsx`).

Because each slot is its own route tree:

- Each can have its own `loading.tsx` and `error.tsx`, so a slow or failing panel does not block the others.
- Each can have nested routes of its own (`@analytics/views/page.tsx`).
- Slots stream independently.

## Soft vs hard navigation: why `default.tsx` exists

Slots are tracked independently, and that matters when the URL changes.

| Navigation | What happens to a slot that has no match for the new URL |
|---|---|
| **Soft** (client-side `<Link>`) | Next.js keeps the slot's previous active content |
| **Hard** (full page load, refresh, direct URL) | Next.js cannot know the slot's previous state, so it renders `default.tsx` |

Example: you are at `/dashboard`, then click a link to `/dashboard/settings`, which only exists in `children`. On a soft navigation `@analytics` stays as it was. If you reload the page at `/dashboard/settings`, `@analytics` has no match for that URL, so it needs a fallback:

```tsx
// app/dashboard/@analytics/default.tsx
export default function Default() {
  return null; // or a placeholder / the same UI as page.tsx
}
```

Without `default.tsx`, an unmatched slot on a hard navigation produces a 404. In recent Next.js versions, a missing `default.tsx` for a slot fails the build, so always add one (including for the implicit `children` slot when needed).

## Common use cases

### 1. Dashboards with independent regions

The example above. Each panel fetches its own data, suspends independently, and fails independently.

### 2. Conditional rendering by user or state

Choose which slot to show in the layout:

```tsx
export default function Layout({
  admin,
  user,
}: {
  admin: React.ReactNode;
  user: React.ReactNode;
}) {
  const role = getRole(); // from session, cookies, etc.
  return role === "admin" ? admin : user;
}
```

This is for UI selection. It is not an authorization mechanism; protect the underlying data separately. See [Protecting Routes](../11-authentication/04-protecting-routes.md).

### 3. Tabs with their own URLs

Slots can host sub-routes (`@tabs/overview`, `@tabs/billing`) so each tab is linkable while the rest of the page stays mounted.

### 4. Modals

Combined with intercepting routes: [Intercepting Routes](./07-intercepting-routes.md).

## Reading the active slot segment

In a Client Component inside the layout you can find out which segment a slot currently shows:

```tsx
"use client";

import { useSelectedLayoutSegment } from "next/navigation";

export function Tabs() {
  const segment = useSelectedLayoutSegment("analytics"); // slot name without "@"
  return <p>Active: {segment ?? "default"}</p>;
}
```

## Rules

- Slots are named `@name` and passed to the **nearest parent layout** as a prop named `name`.
- Slots do not add to the URL.
- `default.tsx` supplies the fallback for hard navigations (and is required in recent versions).
- Slots can have their own `loading.tsx`, `error.tsx`, `not-found.tsx` and nested routes.
- Navigating with `<Link>` updates only the slots whose routes match; the rest keep their state.

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| No `default.tsx` | 404 or build error on refresh of a deeper URL | Add `default.tsx` to each slot |
| Slot prop name does not match the folder | Slot renders nothing / type mismatch | `@team` → `team` prop |
| Expecting `@slot` in the URL | 404 when visiting `/dashboard/@team` | It is not a segment |
| Treating a slot-based role switch as security | Data still reachable | Authorize at the data layer |
| Works when clicking, breaks on refresh | Soft vs hard navigation difference | Test with a full reload; add defaults |
| Slot not rendering | Not placed in the parent layout | Render `{slot}` in the layout |

## Quick Summary

- `@folder` creates a slot rendered by the parent layout as a prop.
- Slots load, stream and fail independently, and can have their own sub-routes.
- Soft navigation preserves unmatched slots; hard navigation needs `default.tsx`.
- Great for dashboards, role-based UI, tabs, and modals.
- Slots organize UI; they are not access control.

## Next

- [Intercepting Routes](./07-intercepting-routes.md)
- [Streaming and Suspense](../04-rendering/03-streaming-and-suspense.md)
- [03 · Components](../03-components/README.md)
