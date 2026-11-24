# Module 26: App Router Routing Cheatsheet

> **Quick Reference**: Routing conventions, special files, route groups, parallel and intercepting routes, and async request APIs in Next.js 15+.

---

## 1. App Router Special File Conventions

| File | Purpose | Execution |
| :--- | :--- | :--- |
| `page.tsx` | Unique UI for a route segment | Server Component (default) |
| `layout.tsx` | Shared UI across children; preserves state | Server Component (default) |
| `template.tsx` | Re-instantiated wrapper on navigation | Server Component (default) |
| `loading.tsx` | Instant loading state (`<Suspense fallback>`) | Server Component |
| `error.tsx` | Catches runtime errors in subtree | **Must be Client Component** (`"use client"`) |
| `global-error.tsx` | Catches errors in root layout | **Must be Client Component** (`"use client"`) |
| `not-found.tsx` | 404 UI invoked by `notFound()` | Server Component |
| `route.ts` | Backend HTTP API endpoint (Route Handler)| Server / Edge runtime |

---

## 2. Dynamic Route Segment Syntax

| Route Pattern | Folder Name | Matching URL Example | `await params` Result |
| :--- | :--- | :--- | :--- |
| **Exact** | `app/blog/` | `/blog` | `{}` |
| **Dynamic** | `app/blog/[slug]/` | `/blog/hello-world` | `{ slug: 'hello-world' }` |
| **Catch-All** | `app/shop/[...items]/` | `/shop/shoes/nike/air` | `{ items: ['shoes', 'nike', 'air'] }` |
| **Optional Catch-All**| `app/docs/[[...slug]]/`| `/docs` or `/docs/a/b` | `{ slug: undefined }` or `{ slug: ['a', 'b'] }` |

---

## 3. Async Request APIs in Next.js 15+

In Next.js 15+, dynamic parameters and request headers are asynchronous promises:

```tsx
// app/dashboard/[id]/page.tsx
interface PageProps {
  params: Promise<{ id: string }>;
  searchParams: Promise<{ tab?: string; page?: string }>;
}

export default async function Page({ params, searchParams }: PageProps) {
  // MUST AWAIT in Next.js 15+
  const { id } = await params;
  const { tab = 'overview', page = '1' } = await searchParams;

  return <div>Item {id} | Tab: {tab} | Page: {page}</div>;
}
```

### Reading Cookies and Headers
```tsx
import { cookies, headers } from 'next/headers';

export default async function ProfilePage() {
  const cookieStore = await cookies();
  const token = cookieStore.get('session')?.value;

  const headerList = await headers();
  const userAgent = headerList.get('user-agent');

  return <div>Token: {token}</div>;
}
```

---

## 4. Route Organization Conventions

### Route Groups `(group)`
- Does not affect URL path.
- Used to partition layouts:
  - `app/(marketing)/about/page.tsx` $\rightarrow$ `/about`
  - `app/(dashboard)/settings/page.tsx` $\rightarrow$ `/settings`

### Parallel Routes `@slot`
- Simultaneous conditionally rendered slots in a layout:
  ```tsx
  // app/dashboard/layout.tsx
  export default function Layout({
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
        {children}
        <div className="grid grid-cols-2">{analytics}{team}</div>
      </div>
    );
  }
  ```

### Intercepting Routes `(.)`, `(..)`, `(...)`
- Intercepts a route to display in a modal while keeping the underlying page intact:
  - `(.)` matches segments on the **same level**.
  - `(..)` matches segments **one level above**.
  - `(...)` matches segments from the **root app directory**.
