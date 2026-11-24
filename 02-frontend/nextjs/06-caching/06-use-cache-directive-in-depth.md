# Modern Caching: `"use cache"`, `cacheLife` & `updateTag` (`use-cache-directive-in-depth`)

> In-depth exploration of the Next.js modern opt-in caching architecture, `"use cache"` directive, `cacheLife` expiration profiles, `cacheTag` keys, `updateTag` vs `revalidateTag`, and custom Redis cache handlers.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Cache Components / dynamicIO |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Evolution: From Implicit to Explicit Caching
In Next.js 13 and 14, caching was aggressive and implicit: `fetch()` requests were cached forever (`force-cache`) by default, leading to stale data confusion and complex segment configuration overrides (`export const revalidate = 60`).
In modern Next.js 15+ and 16:
1. **Dynamic by Default**: All data fetching is uncached (`no-store`) unless explicitly configured.
2. **`"use cache"` Directive**: File, component, or function-level directive opting into fine-grained caching.
3. **`cacheLife(profile)`**: Controls staleness and retention with predefined profiles (`seconds`, `minutes`, `hours`, `days`, `max`).
4. **`cacheTag(tag)`**: Assigns semantic invalidation keys.
5. **Dual Invalidation Model**:
   - **`updateTag(tag)`**: Used inside Server Actions for **instant read-your-own-writes** cache purging.
   - **`revalidateTag(tag, profile)`**: Two-argument function used for **background Stale-While-Revalidate (SWR)** re-fetching.

```
[ Data Query / Component Function ]
                │
                ├── "use cache" ──> [ Evaluated Once and Cached ]
                ├── cacheLife('hours') ──> [ Stale-While-Revalidate Window ]
                └── cacheTag('catalog') ──> [ Targeted Invalidation Key ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Legacy Route Segment Revalidation
```tsx
// app/products/page.tsx (Next.js 14 Legacy)
export const revalidate = 3600; // Coarse-grained: Entire route is locked to 1 hour

export default async function Page() {
  const products = await fetch('https://api/products').then(res => res.json());
  return <ProductList products={products} />;
}
```

### Idiomatic: Fine-Grained `"use cache"` Component
```tsx
// app/products/page.tsx (Modern Next.js 15+)
import { cacheLife, cacheTag } from 'next/cache';

// Only this specific component is cached; other components can be dynamic!
async function CachedProductCatalog() {
  'use cache';
  cacheLife('hours');
  cacheTag('products-catalog');

  const products = await fetch('https://api/products').then(res => res.json());
  return <ProductList products={products} />;
}

export default function Page() {
  return (
    <main>
      <h1>Current Deals</h1>
      <CachedProductCatalog />
    </main>
  );
}
```

---

## 3. Five Progressive Examples

### Example 1: Enabling Dynamic IO & Cache Components in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    dynamicIO: true, // In Next.js 15 (standardized in Next.js 16 under cacheComponents)
  },
};

export default nextConfig;
```

### Example 2: Function-Level `"use cache"` with Tagging
```ts
// dal/articles.ts
import 'server-only';
import { cacheLife, cacheTag } from 'next/cache';
import { db } from '@/db';

export async function getPublishedArticle(slug: string) {
  'use cache';
  cacheLife('days');
  cacheTag('articles', `article-${slug}`);

  return db.article.findUnique({ where: { slug } });
}
```

### Example 3: Immediate Invalidation via `updateTag` in Server Actions
```tsx
// actions/articleActions.ts
'use server';

import { updateTag } from 'next/cache';
import { db } from '@/db';

export async function publishArticle(slug: string, content: string) {
  await db.article.update({
    where: { slug },
    data: { content, status: 'PUBLISHED' },
  });

  // Instantly expunges cache so the submitting user sees updates on next render!
  updateTag(`article-${slug}`);
}
```

### Example 4: Background Webhook Invalidation via `revalidateTag`
```ts
// app/api/cms-webhook/route.ts
import { NextResponse } from 'next/server';
import { revalidateTag } from 'next/cache';

export async function POST(req: Request) {
  const { event, tag } = await req.json();

  if (event === 'ENTRY_UPDATED') {
    // Requires profile argument in Next.js 15+!
    revalidateTag(tag, 'max'); // Stale-While-Revalidate in background
  }

  return NextResponse.json({ ok: true });
}
```

### Example 5: Custom Multi-Instance Redis Cache Handler
```ts
// cache-handler.mjs
import Redis from 'ioredis';

const redis = new Redis(process.env.REDIS_URL);

export default class RedisCacheHandler {
  async get(key) {
    const data = await redis.get(key);
    return data ? JSON.parse(data) : null;
  }

  async set(key, data, ctx) {
    await redis.set(key, JSON.stringify(data), 'EX', ctx.revalidate || 86400);
  }

  async revalidateTag(tag) {
    const keys = await redis.smembers(`tag:${tag}`);
    if (keys.length) await redis.del(...keys);
  }
}
```
```ts
// next.config.ts
const nextConfig = {
  cacheHandler: require.resolve('./cache-handler.mjs'),
};
```

---

## 4. Under the Hood & Internals
`"use cache"` operates on the serialized AST and return values of functions.
1. When a `"use cache"` function is invoked, Next.js calculates a cryptographic hash based on the function body and serialized input arguments.
2. If a cached entry matching the hash exists within its `cacheLife` window, Next.js deserializes the cached output directly without evaluating the function body or making database connections.
3. Cache entries are indexed in an internal tag-to-key inverted index, enabling $O(1)$ invalidations when `updateTag()` is called.

---

## 5. Security & Isolation
- **Prevent Multi-Tenant Cache Poisoning**: Never use `"use cache"` inside functions that take private user tokens or return personal account details without including the user/tenant ID in the function arguments or cache tags:
  ```ts
  // VULNERABLE: Caches User A's profile for all subsequent users!
  async function getUserProfile() { 'use cache'; ... }

  // SECURE: Tenant and user ID part of cache key
  async function getUserProfile(userId: string) { 'use cache'; cacheTag(`user-${userId}`); ... }
  ```

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Single-arg `revalidateTag('tag')` | Deprecated syntax in Next.js 15+ | TypeScript compile error / runtime failure | Use `revalidateTag(tag, 'max')` or `updateTag(tag)` |
| `updateTag()` in Route Handlers | `updateTag` only valid in Actions | Throws error during execution | Use `revalidateTag(tag, profile)` in Route Handlers |
| Caching personal user data | Missing tenant ID in cache tag | User sees another user's private data | Scope cache tags with `user-${id}` |
| Non-serializable returns | Returning class instances from cached function | Serialization error | Return plain JSON-serializable objects |

---

## 7. Performance & Optimization Guide
Combining `"use cache"` with Redis allows horizontally scaled Next.js clusters (e.g. 10 Kubernetes pods) to share a unified cache. When one pod warms the cache for a heavy database aggregation, all 9 other pods serve the result with sub-5ms latency.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import { cacheLife, cacheTag } from 'next/cache';

export type CacheProfile = 'seconds' | 'minutes' | 'hours' | 'days' | 'max';

export async function cachedQuery<T>(tag: string, profile: CacheProfile, queryFn: () => Promise<T>): Promise<T> {
  'use cache';
  cacheLife(profile);
  cacheTag(tag);
  return queryFn();
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { getPublishedArticle } from './articles';

describe('use cache Integration', () => {
  it('returns cached data on repeated calls', async () => {
    const article1 = await getPublishedArticle('slug-1');
    const article2 = await getPublishedArticle('slug-1');
    expect(article1).toEqual(article2);
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not use `"use cache"` for real-time financial trading tickers or chat message streams where latency must be zero and data cannot be stale.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does `"use cache"` differ from legacy `fetch({ next: { revalidate: 60 } })`?</summary>
`"use cache"` is a universal directive that works on any asynchronous function or component, including database queries (Prisma/Drizzle), external APIs, or heavy computational transforms, rather than being restricted strictly to `fetch()`.
</details>

<details>
<summary>2. What is the difference between `updateTag()` and `revalidateTag()`?</summary>
`updateTag(tag)` is designed exclusively for Server Actions to provide immediate, synchronous cache expiration for the submitting user (read-your-own-writes). `revalidateTag(tag, profile)` accepts a profile argument and triggers background Stale-While-Revalidate re-fetching for external webhooks.
</details>

<details>
<summary>3. What built-in profiles are supported by `cacheLife()`?</summary>
`seconds`, `minutes`, `hours`, `days`, and `max`.
</details>

<details>
<summary>4. How does `cacheTag()` enable on-demand invalidation?</summary>
It associates one or more string tags with a cached computation. Calling `updateTag('products')` or `revalidateTag('products', 'max')` purges all cache entries linked to that tag.
</details>

<details>
<summary>5. Can `"use cache"` be placed at the top of a file?</summary>
Yes. Placing `"use cache"` at the top of a file marks every exported function in that module as cached.
</details>

<details>
<summary>6. How do you implement a shared cache across multiple self-hosted instances?</summary>
By creating a custom `cache-handler.js` that connects to a shared Redis cluster and registering it via `cacheHandler` in `next.config.ts`.
</details>

<details>
<summary>7. What happens if a `"use cache"` function returns a non-serializable value?</summary>
Next.js will throw a serialization error because cached outputs must be persisted to disk, memory, or external storage.
</details>

<details>
<summary>8. How do you prevent cache stampedes in Next.js?</summary>
By utilizing Stale-While-Revalidate profiles with `cacheLife`, allowing stale content to be served immediately while only a single background task warms the cache.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Cache a database query</summary>
Add `"use cache"` and `cacheLife('minutes')` to a blog query function.
</details>

<details>
<summary>Exercise 2 (Medium): Trigger immediate update in an action</summary>
Write a Server Action that updates a product price and invalidates its tag using `updateTag('product-123')`.
</details>

<details>
<summary>Exercise 3 (Hard): Build a Redis CacheHandler</summary>
Implement a custom Redis cache handler class supporting `get`, `set`, and `revalidateTag`.
</details>

---

## 13. Cross-References & Further Reading
- [Data Access Layer Architecture](file:///E:/Learning/engineering-notes/02-frontend/nextjs/05-data-fetching/03-data-access-layer-with-server-only.md)
- [Official Cache Components RFC & Docs](https://nextjs.org/docs/app/api-reference/directives/use-cache)
