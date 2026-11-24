# Module 26: Rendering Models & PPR Cheatsheet

> **Quick Reference**: Static, Dynamic, Streaming SSR, Suspense boundaries, and Partial Prerendering (PPR `◐`) in Next.js 15+.

---

## 1. Next.js Build Output Route Symbols

When running `next build`, routes are classified in the terminal output:

| Symbol | Rendering Strategy | Description |
| :--- | :--- | :--- |
| `○` | **Static** | Prerendered as static HTML/RSC at build time. Zero runtime compute. |
| `ƒ` | **Dynamic** | Rendered on-demand per request at runtime (uses cookies, headers, or query params). |
| `◐` | **Partial Prerender (PPR)**| Prerendered static shell + dynamic content streamed concurrently in one stream. |

---

## 2. Rendering Strategy Decision Matrix

```
                      Does page read cookies(), headers(),
                         or searchParams at runtime?
                                  │
                  ┌───────────────┴───────────────┐
                 YES                              NO
                  │                               │
       Does it use Suspense?          Is dynamic data needed?
         │             │                      │             │
        YES            NO                    YES            NO
         │             │                      │             │
   Partial Prerender  Dynamic SSR          Dynamic SSR    Static (SSG)
     (PPR `◐`)          (`ƒ`)                 (`ƒ`)          (`○`)
```

---

## 3. Streaming SSR with React Suspense

Stream heavy components without delaying the rest of the page:

```tsx
// app/dashboard/page.tsx
import { Suspense } from 'react';
import { MetricCards, MetricSkeleton } from './MetricCards';
import { ActivityFeed, FeedSkeleton } from './ActivityFeed';

export default function Dashboard() {
  return (
    <main className="p-8 space-y-6">
      <h1 className="text-2xl font-bold">Analytics Dashboard</h1>

      {/* Static header renders immediately; metrics stream when ready */}
      <Suspense fallback={<MetricSkeleton />}>
        <MetricCards />
      </Suspense>

      {/* Independent stream; does not block MetricCards */}
      <Suspense fallback={<FeedSkeleton />}>
        <ActivityFeed />
      </Suspense>
    </main>
  );
}
```

---

## 4. Partial Prerendering (PPR) Configuration

### Step 1: Enable in `next.config.ts`
```typescript
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    ppr: 'incremental', // Enable per route segment
  },
};

export default nextConfig;
```

### Step 2: Opt In at Route Level
```tsx
// app/products/[id]/page.tsx
import { Suspense } from 'react';
import { ProductDetails } from '@/components/ProductDetails';
import { UserCartStatus } from '@/components/UserCartStatus';

// Enable PPR for this segment:
export const experimental_ppr = true;

export default async function ProductPage({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;

  return (
    <div>
      {/* 1. Static Shell (prerendered at build time, served in ~10ms from CDN) */}
      <ProductDetails productId={id} />

      {/* 2. Dynamic Hole (streamed into open HTTP stream on request) */}
      <Suspense fallback={<div className="h-8 w-24 bg-gray-200 animate-pulse" />}>
        <UserCartStatus />
      </Suspense>
    </div>
  );
}
```

---

## 5. Segment Configuration Options (Reference)

```typescript
// Force dynamic rendering (equivalent to getServerSideProps)
export const dynamic = 'force-dynamic';

// Force static rendering (errors if dynamic functions are called)
export const dynamic = 'force-static';

// Revalidate interval in seconds (legacy ISR mode)
export const revalidate = 60;

// Dynamic parameters allowed for generateStaticParams
export const dynamicParams = true;
```
