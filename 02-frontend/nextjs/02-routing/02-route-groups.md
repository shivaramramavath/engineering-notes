# Route Groups

A route group is a folder wrapped in parentheses, like `(marketing)`. It is **ignored in the URL**, so it lets you organize routes and attach different layouts without changing paths.

## The basics

```text
app/
├── (marketing)/
│   ├── about/page.tsx          →  /about
│   └── pricing/page.tsx        →  /pricing
└── (shop)/
    ├── cart/page.tsx           →  /cart
    └── products/page.tsx       →  /products
```

`(marketing)` and `(shop)` do not appear in any URL. They exist to group files for you and for layouts.

## Use 1: Organize without changing URLs

Without groups, a large `app/` folder becomes a flat list of unrelated routes. Groups give it structure for people:

```text
app/
├── (auth)/
│   ├── login/page.tsx
│   └── register/page.tsx
├── (dashboard)/
│   ├── overview/page.tsx
│   └── settings/page.tsx
└── (public)/
    └── page.tsx
```

## Use 2: Different layouts for different sections

Put a `layout.tsx` inside a group and it wraps only that group's routes:

```text
app/
├── layout.tsx                    root layout (html, body)
├── (marketing)/
│   ├── layout.tsx                marketing header + footer
│   └── about/page.tsx
└── (app)/
    ├── layout.tsx                sidebar + auth-aware shell
    └── dashboard/page.tsx
```

```tsx
// app/(app)/layout.tsx
export default function AppLayout({ children }: { children: React.ReactNode }) {
  return (
    <div className="flex">
      <aside>Sidebar</aside>
      <main>{children}</main>
    </div>
  );
}
```

`/about` gets the marketing layout, `/dashboard` the app layout, and both sit inside the root layout.

## Use 3: Opt some routes into a shared layout

When siblings in the same folder need different layouts, group the ones that share:

```text
app/shop/
├── (with-sidebar)/
│   ├── layout.tsx
│   ├── products/page.tsx        /shop/products
│   └── categories/page.tsx      /shop/categories
└── checkout/page.tsx            /shop/checkout   (no sidebar)
```

## Use 4: Multiple root layouts

If each section needs its own `<html>` and `<body>` (different `lang`, fonts, global styles), delete the top-level `app/layout.tsx` and give each group a root layout:

```text
app/
├── (marketing)/
│   ├── layout.tsx             contains <html> and <body>
│   └── page.tsx               /
└── (app)/
    ├── layout.tsx             contains <html> and <body>
    └── dashboard/page.tsx
```

Navigating between two different root layouts triggers a **full page load**, because they are different documents. If the sections are navigated between often, prefer one root layout with nested layouts inside groups.

## Other things groups can hold

Any segment file works inside a group, which scopes it to that group:

- `loading.tsx`: a loading skeleton for only that group's routes
- `error.tsx`, `not-found.tsx`: group-specific failure UI
- `template.tsx`

## Rules

| Rule | Detail |
|---|---|
| Not part of the URL | `(group)/about` → `/about` |
| Names are for you | Any name works; use meaningful ones |
| No duplicate URLs | `(a)/about` and `(b)/about` both resolve to `/about` and conflict |
| Groups can nest | `(site)/(marketing)/about` is valid |
| Root layout rule | With several root layouts, there is no top-level `app/layout.tsx` |
| Top-level `page.tsx` | If you use multiple root layouts, put the home page in a group |

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| Two groups define the same URL | Build error about parallel pages resolving to the same path | Rename or move one |
| Expecting the group name in the URL | `/marketing/about` returns 404 | The URL is `/about` |
| Multiple root layouts plus an `app/layout.tsx` | Confusing nesting, duplicate `<html>` | Pick one approach |
| Navigation between root layouts feels slow | Full reload by design | Use one root layout with nested group layouts |
| Forgetting `<html>`/`<body>` in a group root layout | Runtime error | Add both |
| Using a group to hide a page from users | Page still public | Groups are not access control; see [Protecting Routes](../11-authentication/04-protecting-routes.md) |

## Quick Summary

- `(name)` folders organize routes without altering URLs.
- Use them to scope layouts, loading and error UI to a section.
- Multiple root layouts are possible but cause full reloads between them.
- Two groups cannot define the same URL.
- Groups are structure, not security.

## Next

- [Navigation](./03-navigation.md)
- [Pages and Layouts](../01-fundamentals/03-pages-and-layouts.md)
- [Protecting Routes](../11-authentication/04-protecting-routes.md)
