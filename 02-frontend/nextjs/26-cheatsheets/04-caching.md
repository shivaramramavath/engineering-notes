# Module 26: Caching & Cache Components Cheatsheet

> **Quick Reference**: Modern Next.js 15+ caching architecture, `"use cache"` directive, `cacheLife()`, `cacheTag()`, and invalidation semantics.

---

## 1. Next.js 14 vs Next.js 15 Caching Paradigm Shift

| Feature | Next.js 14 | Next.js 15+ |
| :--- | :--- | :--- |
| **`fetch()` requests** | Cached by default (`force-cache`) | **Uncached by default** (`no-store`) |
| **GET Route Handlers** | Cached if static | **Uncached by default** |
| **Client Router Cache** | Page segments cached 30s | **0s (no page cache by default)** |
| **Fine-Grained Cache** | Route-level `revalidate = N` | **`"use cache"` directive** |

---

## 2. The `"use cache"` Directive

Applied to a file, component, or async function to memoize output in the Next.js Data Cache:

```tsx
// 1. Function-level caching
export async function getLeaderboard() {
  'use cache';
  const data = await db.query.leaderboard.findMany();
  return data;
}

// 2. Component-level caching
import { cacheLife, cacheTag } from 'next/cache';

export async function TrendingProducts() {
  'use cache';
  cacheLife('hours'); // Profile duration
  cacheTag('trending-products'); // Tag for targeted invalidation

  const products = await db.query.products.findMany({ limit: 10 });
  return <div>{products.map(p => <span key={p.id}>{p.name}</span>)}</div>;
}
```

---

## 3. Built-in `cacheLife` Profiles

| Profile | Stale Window | Revalidate After | Expire Duration | Best Used For |
| :--- | :--- | :--- | :--- | :--- |
| `'default'` | 5 minutes | 15 minutes | 1 hour | General balanced content |
| `'seconds'` | 0 seconds | 10 seconds | 1 minute | Fast-changing metrics |
| `'minutes'` | 1 minute | 5 minutes | 1 hour | Activity feeds, blogs |
| `'hours'` | 5 minutes | 1 hour | 1 day | Catalog items, documentation |
| `'days'` | 1 hour | 1 day | 1 week | Legal terms, static policies |
| `'weeks'` | 1 day | 1 week | 1 month | Archives, historical stats |
| `'max'` | 1 week | 1 month | 1 year | Immutable assets, releases |

### Custom Profile Definition (`next.config.ts`)
```typescript
const nextConfig: NextConfig = {
  experimental: {
    cacheLife: {
      flashSale: {
        stale: 10,      // 10s
        revalidate: 30, // 30s
        expire: 60,     // 1m
      },
    },
  },
};
```

---

## 4. Cache Invalidation Matrix

| Invalidation API | Execution Context | Target | Behavior |
| :--- | :--- | :--- | :--- |
| **`updateTag(tag)`** | Server Actions | `cacheTag(tag)` | **Synchronous write**: Guarantees caller sees fresh data immediately (Read-Your-Own-Writes). |
| **`revalidateTag(tag)`** | Route Handlers / Webhooks | `cacheTag(tag)` | **Stale-While-Revalidate**: Serves stale cache once while recomputing in background. |
| **`revalidatePath(path)`** | Server Actions / Webhooks | Route URL path | Purges all cached segments and queries for the specific URL. |

### Usage Example
```tsx
// app/actions/updateProduct.ts
'use server';
import { updateTag } from 'next/cache';

export async function updatePriceAction(id: string, price: number) {
  await db.update(products).set({ price }).where(eq(products.id, id));
  // Invalidate product cache immediately
  updateTag(`product-${id}`);
}
```
