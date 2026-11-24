# Senior & Staff Interview Guide: Rendering Models and Caching Architecture

> Comprehensive technical interview mastery guide covering Static vs Dynamic rendering, Partial Prerendering (PPR), the `"use cache"` directive, cache invalidation protocols, and Next.js 14 vs 15 caching paradigms.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [04-rendering/README.md](../04-rendering/README.md), [06-caching/README.md](../06-caching/README.md) |
| **Next.js Baseline** | Next.js 15.2+ / 16.0, React 19.0.0, Node.js 20+ LTS |

---

## Table of Contents
1. [Core Architectural Concepts](#1-core-architectural-concepts)
2. [Deep-Dive Interview Questions & Answers](#2-deep-dive-interview-questions--answers)
3. [Output Prediction & Code Analysis Questions](#3-output-prediction--code-analysis-questions)
4. [Architecture & System Design Scenario](#4-architecture--system-design-scenario)
5. [Senior Technical Evaluation Rubric](#5-senior-technical-evaluation-rubric)

---

## 1. Core Architectural Concepts

Interviewers evaluating senior candidates on rendering and caching focus on:
- **Rendering Modes**: Static Site Generation (SSG), Incremental Static Regeneration (ISR), Dynamic Server-Side Rendering (SSR), and Partial Prerendering (PPR `◐`).
- **The Next.js 15 Caching Shift**: Why `fetch` requests and `GET` Route Handlers are no longer cached by default (`cache: 'no-store'`).
- **Cache Components Model**: The `"use cache"` directive, `cacheLife()`, `cacheTag()`, and fine-grained sub-component memoization.
- **Cache Invalidation Semantics**: Immediate synchronous write invalidation (`updateTag` in Server Actions) vs Stale-While-Revalidate (`revalidateTag` in Route Handlers).

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: Why did Next.js 15 invert the default caching behavior of `fetch` from aggressive caching to uncached (`no-store`)?</summary>

**Answer**:
In Next.js 13 and 14, standard `fetch('https://api.example.com/data')` calls were cached indefinitely by default (`force-cache`).
While this improved out-of-the-box performance benchmarks, it caused severe architectural friction:
1. **Developer Mental Model**: Developers expected standard JavaScript `fetch()` to fetch fresh data, not behave like an aggressive proxy cache.
2. **Stale Data Bugs**: Authenticated dashboards, user profiles, and dynamic feeds frequently served stale data unless developers remembered to pass `{ cache: 'no-store' }`.
3. **Introduction of `"use cache"`**: Next.js 15 decoupled caching from `fetch()` entirely, shifting to an explicit, composable caching model where developers opt into caching using `"use cache"` at the function, file, or component level.
</details>

<details>
<summary>Q2: What is Partial Prerendering (PPR) and how does it solve the classic static-vs-dynamic trade-off?</summary>

**Answer**:
Historically, a page had to choose:
- **Static (Fast TTFB)**: Instant edge CDN delivery, but cannot personalize or show real-time user data.
- **Dynamic (Personalized)**: Slow TTFB because the server waits for database queries before emitting the first byte of HTML.

**PPR combines both in a single HTTP response (`◐`)**:
1. At build time, Next.js prerenders the static shell of the page up to the boundaries of React `<Suspense>` components that contain dynamic calls (e.g. `cookies()`, unmemoized database queries).
2. On request, the edge CDN **immediately serves the prerendered static shell** within ~10ms (instant FCP and TTFB).
3. Concurrently, the server executes the suspended dynamic components and **streams the dynamic HTML chunks into the same open HTTP stream**, replacing fallbacks seamlessly.
</details>

<details>
<summary>Q3: Compare `updateTag(tag)` with `revalidateTag(tag, profile)`. When should each be used?</summary>

**Answer**:
- `updateTag(tag)`:
  - Designed for **Server Actions (Read-Your-Own-Writes)**.
  - Purges the cache synchronously and guarantees that the client's subsequent render within the same transition sees the newly updated state immediately, preventing UI flicker.
- `revalidateTag(tag, profile)`:
  - Designed for **Route Handlers / Webhooks (Background Invalidation)**.
  - Marks cached entries as stale using Stale-While-Revalidate (SWR) semantics. The current request serves the existing stale cache entry while triggering background recomputation.
</details>

<details>
<summary>Q4: How does `"use cache"` interact with React Server Component props and arguments?</summary>

**Answer**:
When `"use cache"` is applied to a function or component:
- Next.js automatically serializes the function's arguments / props to construct the internal cache key.
- All arguments must be **serializable primitives or plain objects**.
- If a function takes non-serializable arguments (like a database connection handle or a function), the cache key generation fails.
- Inside the cached block, dynamic functions (`cookies()`, `headers()`) are prohibited and will throw a build/runtime error unless accessed outside the cached scope and passed in as serializable values.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Unintended Dynamic De-Optimization
Will the following page be statically prerendered or dynamically rendered per request?

```tsx
// app/products/page.tsx
import { cookies } from 'next/headers';
import { db } from '@/lib/db';

export default async function ProductsPage() {
  const products = await db.products.findMany();

  return (
    <div>
      <h1>Product Catalog</h1>
      <ul>{products.map(p => <li key={p.id}>{p.name}</li>)}</ul>
    </div>
  );
}
```

**Evaluation**:
- **Static Prerendering (`○`)**: The component does not read `cookies()`, `headers()`, or `searchParams`, nor does it use uncached dynamic functions.
- Next.js automatically detects that all operations are static at build time and generates a pre-computed HTML/RSC payload.
- If we add `const cookieStore = await cookies();` inside `ProductsPage`, the route immediately de-optimizes to **Dynamic (`ƒ`)** unless wrapped in a Suspense boundary under PPR.

---

## 4. Architecture & System Design Scenario

**Scenario**: Global news publication with 50M monthly active readers.
**Requirements**:
1. Global article page TTFB $< 30$ms.
2. Breaking news banner must update within 10 seconds of editorial publish.
3. Personalized user subscription banner in the navigation bar.

**Senior Architectural Blueprint**:
- **Rendering Engine**: Enable Partial Prerendering (`experimental: { ppr: true }`).
- **Static Shell**: The layout, article body, typography, and images are static, prerendered at build time and cached on global CDN edges.
- **Dynamic Island 1 (User Session)**: Wrapped in `<Suspense fallback={<NavSkeleton />}>`. Reads `await cookies()` to check subscription status and streams instantly.
- **Cached Dynamic Island 2 (Breaking News Banner)**: Uses `"use cache"` with `cacheLife({ expire: 10, stale: 5 })` and tagged with `cacheTag('breaking-news')`. Invalidation webhook calls `revalidateTag('breaking-news')` upon CMS publish.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Understands difference between static and dynamic pages. Uses `revalidatePath`. |
| **Senior** | Explains Partial Prerendering architecture, RSC streaming over open HTTP streams, and difference between `updateTag` and `revalidateTag`. |
| **Staff / Principal** | Details cache key serialization algorithms, edge CDN cache-control header coordination, and zero-downtime cache stampede mitigation under high concurrency. |
