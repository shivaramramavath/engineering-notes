# Partial Prerendering (PPR) & Cache Components (`partial-prerendering-and-cache-components-model`)

> Modern rendering architecture in Next.js: Combining static prerendered HTML shells with dynamic streaming React Suspense boundaries, build route symbol `◐`, and cache components.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Node.js / Edge PPR |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Rendering Dilemma: Static vs Dynamic
For years, web architectures forced an all-or-nothing trade-off:
- **Static Site Generation (SSG)**: Fast TTFB from Edge CDN, but cannot show personalized user content without client-side waterfall fetches.
- **Server-Side Rendering (SSR)**: Dynamic and personalized, but blocks the entire page response until the slowest database query finishes.

### 1.2 The Solution: Partial Prerendering (PPR)
**Partial Prerendering (PPR)** delivers the best of both worlds in a single HTTP response:
1. At **build time**, Next.js prerenders the static shell (navbar, layout, product image, sidebars) and serves it instantly from the Edge CDN cache.
2. In the same HTTP response, dynamic holes wrapped in React `<Suspense>` are left open.
3. At **request time**, the server streams dynamic chunks into the open holes as they resolve, without blocking the initial paint!

```
HTTP 200 OK (Single Connection)
├── Instant Chunk 1: Static Prerendered Shell (Header, Layout, Skeleton) [0ms TTFB]
└── Streamed Chunk 2: Personalized User Cart & Recommendations [Streamed at 150ms]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Full SSR Dynamic De-optimization
```tsx
// app/store/page.tsx (Naive - Entire page waits for cart fetch)
import { cookies } from 'next/headers';
import { getCart, getFeaturedProducts } from '@/dal/store';

export default async function StorePage() {
  const cookieJar = await cookies();
  const cart = await getCart(cookieJar.get('session')?.value); // Dynamic
  const products = await getFeaturedProducts(); // Static data

  return (
    <main>
      <StoreHeader cartCount={cart.items.length} />
      <ProductGrid products={products} />
    </main>
  );
}
```

### Idiomatic: Partial Prerendering with Suspense
```tsx
// app/store/page.tsx (Idiomatic PPR)
import { Suspense } from 'react';
import { getFeaturedProducts } from '@/dal/store';
import { CartBadgeContainer } from './CartBadgeContainer';

export const experimental_ppr = true;

export default async function StorePage() {
  const products = await getFeaturedProducts(); // Prerendered into static shell

  return (
    <main>
      <header className="flex justify-between p-4">
        <h1>TechStore</h1>
        <Suspense fallback={<div className="h-8 w-8 animate-pulse rounded-full bg-slate-200" />}>
          <CartBadgeContainer /> {/* Dynamic streaming hole */}
        </Suspense>
      </header>
      <ProductGrid products={products} /> {/* Static */}
    </main>
  );
}
```

---

## 3. Five Progressive Examples

### Example 1: Enabling PPR in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    ppr: true, // Enables Partial Prerendering
  },
};

export default nextConfig;
```

### Example 2: Opting In at Route Segment Level
```tsx
// app/dashboard/page.tsx
export const experimental_ppr = true; // Enables PPR for this route segment
```

### Example 3: Dynamic Hole with Async Request API
```tsx
// app/dashboard/UserGreeting.tsx
import { cookies } from 'next/headers';

export async function UserGreeting() {
  const cookieJar = await cookies();
  const userName = cookieJar.get('userName')?.value ?? 'Guest';
  return <span>Welcome back, {userName}!</span>;
}
```

### Example 4: Combining PPR with `"use cache"`
```tsx
// dal/recommendations.ts
import { cacheLife } from 'next/cache';

export async function getPersonalizedRecommendations(userId: string) {
  'use cache';
  cacheLife('minutes');
  return fetch(`https://api.internal/recs/${userId}`).then((res) => res.json());
}
```

### Example 5: Verifying Route Table Symbol `◐`
```bash
# Output in terminal after `next build`:
Route (app)                              Size     First Load JS
├ ◐ /store                               4.2 kB         94.1 kB
```

---

## 4. Under the Hood & Internals
During `next build`, Next.js renders the component tree using React 19's `prerender` API.
1. When React encounters a dynamic API (`cookies()`, `headers()`) inside a `<Suspense>` boundary, the boundary is converted into a postponed promise.
2. The static shell is saved as an HTML and RSC stream payload.
3. At runtime, when an HTTP request arrives, the server instantly sends the static shell, then immediately invokes the suspended promises and streams the dynamic chunks down the existing connection.

---

## 5. Security & Isolation
- **Preventing User Data Leakage**: Because the static shell is prerendered at build time and cached globally on CDN edges, **never put user-specific data outside of a `<Suspense>` boundary** on a PPR-enabled page. Dynamic personalized data must always be isolated inside Suspense holes.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Dynamic API outside Suspense | Awaiting `cookies()` at page root | Entire page flips to dynamic `ƒ` | Move `await cookies()` into Suspense component |
| Prerendering personal data | Leaking user details into static shell | User A sees User B's username | Wrap greeting in Suspense hole |
| Nested waterfalls in dynamic hole | Sequential awaits inside hole | Slow streaming resolution | Fetch dynamic hole data with `Promise.all` |
| Missing fallback UI | Suspense without fallback | Blank space during streaming | Always provide skeleton fallback |

---

## 7. Performance & Optimization Guide
PPR achieves near-zero Time to First Byte (TTFB < 30ms from CDN Edge), while retaining dynamic personalization without triggering client-side layout shifts or double-rendering waterfalls.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export const experimental_ppr = true; // Route config type
```

---

## 9. Testing & Quality Verification

```bash
# Run build and inspect route table output
npx next build
# Verify target routes output ◐ symbol
```

---

## 10. When NOT to Use This Pattern
- If an entire page is purely static with zero personalization (e.g. Terms of Service, public blog post), standard static prerendering (`○`) is simpler and does not require streaming.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How does Partial Prerendering (PPR) differ from traditional ISR?</summary>
ISR revalidates the entire page in the background on a time interval. PPR serves a static shell instantly from the CDN while streaming dynamic user-specific holes at runtime in the very same HTTP request.
</details>

<details>
<summary>2. What role does React `<Suspense>` play in PPR?</summary>
Suspense boundaries define the exact boundary between the static prerendered shell and the dynamic runtime streaming holes.
</details>

<details>
<summary>3. What is the Route Table symbol for PPR?</summary>
The half-filled circle: `◐  (Partial Prerender)`.
</details>

<details>
<summary>4. Can PPR be enabled per-route?</summary>
Yes, by exporting `export const experimental_ppr = true` from a page or layout.
</details>

<details>
<summary>5. What happens if a dynamic request API is called outside of Suspense in a PPR route?</summary>
Next.js will fail the static prerender and fall back to fully dynamic server rendering (`ƒ`) for the entire route.
</details>

<details>
<summary>6. How does PPR improve Largest Contentful Paint (LCP)?</summary>
The hero image and page layout arrive in the first static chunk (<50ms), allowing the browser to paint LCP immediately while dynamic content streams below.
</details>

<details>
<summary>7. Is PPR supported on self-hosted Node.js servers?</summary>
Yes, PPR works on Node.js standalone servers as well as Edge CDN environments.
</details>

<details>
<summary>8. How do you configure fallback skeletons in PPR?</summary>
Pass skeleton components to the `fallback` prop of the `<Suspense>` boundary enclosing the dynamic hole.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Enable PPR for a dashboard route</summary>
Export `experimental_ppr = true` and wrap a user profile widget in Suspense.
</details>

<details>
<summary>Exercise 2 (Medium): Create skeleton fallbacks</summary>
Build a pulse skeleton for an e-commerce cart badge inside a PPR route.
</details>

<details>
<summary>Exercise 3 (Hard): Audit build route table</summary>
Configure `next.config.ts` with `ppr: true`, run `next build`, and verify that `◐` appears next to your target routes.
</details>

---

## 13. Cross-References & Further Reading
- [Build Output Anatomy & Route Table](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/05-build-output-anatomy-and-route-table.md)
- [Official Partial Prerendering Guide](https://nextjs.org/docs/app/building-your-application/rendering/partial-prerendering)
