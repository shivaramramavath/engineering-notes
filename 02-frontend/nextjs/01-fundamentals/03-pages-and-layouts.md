# Pages and Layouts

A **page** is the unique UI of a route. A **layout** is UI shared by a segment and everything below it. Together they let you build nested interfaces where shared chrome (navigation, sidebars) stays put while only the page content changes.

## Pages

A page is a file named `page.tsx` that default-exports a component. It is what makes a route publicly accessible.

```tsx
// app/dashboard/page.tsx  →  /dashboard
export default function DashboardPage() {
  return <h1>Dashboard</h1>;
}
```

A page receives `params` (dynamic segments) and `searchParams` (query string). Both are Promises in Next.js 15+:

```tsx
// app/blog/[slug]/page.tsx  →  /blog/hello?draft=true
export default async function Page({
  params,
  searchParams,
}: {
  params: Promise<{ slug: string }>;
  searchParams: Promise<{ draft?: string }>;
}) {
  const { slug } = await params;
  const { draft } = await searchParams;
  return (
    <h1>
      {slug} {draft ? "(draft)" : ""}
    </h1>
  );
}
```

Reading `searchParams` makes the page dynamically rendered (it depends on the request). Typing these is covered in [Routes and Params](../13-typescript/01-routes-and-params.md).

## Layouts

A layout wraps its segment's pages and nested layouts. It receives `children`:

```tsx
// app/dashboard/layout.tsx
import Link from "next/link";

export default function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div className="flex">
      <nav>
        <Link href="/dashboard">Overview</Link>
        <Link href="/dashboard/settings">Settings</Link>
      </nav>
      <main>{children}</main>
    </div>
  );
}
```

This layout applies to `/dashboard`, `/dashboard/settings`, and every deeper route in that folder.

### The root layout

Every app needs a root layout at `app/layout.tsx`. It must contain `<html>` and `<body>`:

```tsx
// app/layout.tsx
import type { Metadata } from "next";

export const metadata: Metadata = {
  title: { default: "My App", template: "%s | My App" },
};

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

The root layout is a Server Component and cannot be a Client Component with state. Put providers (theme, query client) in a separate Client Component and render it inside the layout:

```tsx
// app/providers.tsx
"use client";

export function Providers({ children }: { children: React.ReactNode }) {
  // context providers go here
  return <>{children}</>;
}
```

```tsx
<body>
  <Providers>{children}</Providers>
</body>
```

## Nesting

Layouts nest automatically:

```text
app/
├── layout.tsx                  RootLayout
└── dashboard/
    ├── layout.tsx              DashboardLayout
    └── settings/
        └── page.tsx            /dashboard/settings
```

```text
RootLayout
 └─ DashboardLayout
     └─ settings/page.tsx
```

Visiting `/dashboard/settings` renders all three, outermost first.

## What makes layouts special: they persist

When you navigate between pages that share a layout, **the layout is not re-rendered and its state is preserved**. Only the changed segment updates. This is partial rendering.

```text
/dashboard/settings  →  /dashboard/profile

RootLayout        (kept)
DashboardLayout   (kept, state preserved)
page              (replaced)
```

That is what you want for navigation bars, sidebars and audio players. It also explains the rules below.

## Rules and limitations

| Rule | Why |
|---|---|
| Layouts do not receive `searchParams` | They do not re-render on navigation, so they cannot stay in sync with the query string |
| Layouts cannot read the current pathname or child segments directly | Same reason; use `usePathname()` / `useSelectedLayoutSegment()` in a Client Component |
| A layout cannot pass data to its children | Use shared fetching (requests with the same input are deduplicated) or context |
| Navigating between two different root layouts causes a full page load | They are separate HTML documents |
| A layout and page in the same segment both render, but are independent | Do not depend on layout rendering order for data |

Highlighting an active link therefore needs a small Client Component:

```tsx
"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";

export function NavLink({ href, children }: { href: string; children: React.ReactNode }) {
  const pathname = usePathname();
  return (
    <Link href={href} aria-current={pathname === href ? "page" : undefined}>
      {children}
    </Link>
  );
}
```

## Templates: layouts that reset

`template.tsx` looks like a layout but creates a **new instance on every navigation**: state resets, effects re-run, and DOM is recreated.

```tsx
// app/dashboard/template.tsx
export default function Template({ children }: { children: React.ReactNode }) {
  return <div>{children}</div>;
}
```

Use a template when you need that reset: enter animations, per-page analytics logging in an effect, or resetting a feedback form. If it is not needed, use a layout. If both exist, the template renders inside the layout.

## Multiple root layouts

Route groups let different sections have different root layouts, each with its own `<html>`:

```text
app/
├── (marketing)/
│   ├── layout.tsx      root layout for marketing
│   └── page.tsx        /
└── (app)/
    ├── layout.tsx      root layout for the app
    └── dashboard/page.tsx
```

When using this pattern, there is no top-level `app/layout.tsx`. Remember that moving between these sections is a full page load. See [Route Groups](../02-routing/02-route-groups.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Missing `<html>` / `<body>` in the root layout | Error at startup | Add both |
| Expecting a layout to update when the query string changes | Layout shows stale values | Read `searchParams` in the page, or use `useSearchParams()` in a Client Component |
| Putting `"use client"` on the root layout | Everything becomes a client boundary | Wrap only providers in a Client Component |
| Trying to pass props from layout to page | No mechanism exists | Fetch in both (deduplicated), or use context |
| Expecting state reset between sibling pages | State persists in layouts | Use `template.tsx` |
| Heavy data fetching in a high-level layout | Slows every route under it | Fetch where the data is used |

## Quick Summary

- `page.tsx` is a route's unique UI; `layout.tsx` is shared UI that wraps it.
- The root layout must define `<html>` and `<body>`; keep it a Server Component.
- Layouts nest and **persist** across navigation; they do not re-render or receive `searchParams`.
- Use `template.tsx` when you need a fresh instance on each navigation.
- Active-link or pathname-based UI belongs in a small Client Component.

## Next

- [Pages Router](./04-pages-router.md)
- [Navigation](../02-routing/03-navigation.md)
- [Server vs Client](../03-components/02-server-vs-client.md)