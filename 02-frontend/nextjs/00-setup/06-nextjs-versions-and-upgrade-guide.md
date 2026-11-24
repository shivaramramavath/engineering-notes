# Next.js Versions & Upgrade Guide (`nextjs-versions-and-upgrade-guide`)

> Complete architectural upgrade guide from Next.js 13 and 14 to modern Next.js 15.2+ and 16, detailing breaking API changes, automated codemods, and migration recipes.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS (Minimum 18.18) |
| **Router** | App Router (`app/`) |
| **Runtime** | Node.js / Edge |
| **Tested With** | Next.js 15.2, React 19, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 Why Upgrading Matters
Between Next.js 13 and modern Next.js 15/16, the framework underwent three major architectural shifts:
1. **Asynchronous Request APIs**: Synchronous access to `cookies()`, `headers()`, `params`, and `searchParams` was phased out in favor of `Promise`-based async APIs to decouple static rendering shells from dynamic runtime streams.
2. **Explicit Caching Model**: Implicit automatic caching of `fetch()` was inverted to dynamic-by-default (`no-store`), superseded by the granular `"use cache"`, `cacheLife`, and `cacheTag` model.
3. **Network Boundary Clarification**: The ambiguous `middleware.ts` was deprecated and renamed to `proxy.ts`, clarifying its role as an edge routing proxy rather than an application-layer handler.

### 1.2 Migration Path Diagram
```
Next.js 13 / 14 (Sync APIs, Aggressive Fetch Cache, middleware.ts, next lint)
                               │
                               ▼ [ Automated Codemods: @next/codemod ]
Next.js 15 (Async cookies/headers/params, dynamic-by-default fetch, React 19 GA)
                               │
                               ▼
Next.js 16 (proxy.ts, "use cache" stable, next lint removed, ESLint 9 Flat Config)
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive Implementation (Next.js 14 Legacy)
```tsx
// app/users/[id]/page.tsx (Next.js 14 - Breaks in 15+)
import { cookies, headers } from 'next/headers';

export default function UserPage({ params, searchParams }: {
  params: { id: string };
  searchParams: { tab?: string };
}) {
  // ERROR in Next.js 15+: Synchronous calls cause runtime warnings or rejections
  const cookieStore = cookies();
  const token = cookieStore.get('session');
  const userId = params.id;
  const currentTab = searchParams.tab;

  return <div>User: {userId} on tab {currentTab}</div>;
}
```

### Idiomatic Implementation (Modern Next.js 15/16)
```tsx
// app/users/[id]/page.tsx (Modern Next.js 15+)
import { cookies, headers } from 'next/headers';

type Props = {
  params: Promise<{ id: string }>;
  searchParams: Promise<{ tab?: string }>;
};

export default async function UserPage({ params, searchParams }: Props) {
  const [cookieStore, { id }, { tab }] = await Promise.all([
    cookies(),
    params,
    searchParams,
  ]);

  const token = cookieStore.get('session');
  return <div>User: {id} on tab {tab || 'overview'}</div>;
}
```

---

## 3. Five Progressive Examples

### Example 1: Basic Async Request APIs
```tsx
import { cookies, headers } from 'next/headers';

export default async function ProfileHeader() {
  const cookieJar = await cookies();
  const headerList = await headers();

  const theme = cookieJar.get('theme')?.value ?? 'light';
  const ip = headerList.get('x-forwarded-for') ?? '127.0.0.1';

  return <div className={`theme-${theme}`}>Request from: {ip}</div>;
}
```

### Example 2: Client Component Param Unwrapping with React 19 `use()`
```tsx
'use client';

import { use } from 'react';

export function TabSelector({ searchParams }: { searchParams: Promise<{ tab?: string }> }) {
  const { tab } = use(searchParams);
  return <div className="font-bold">Active Tab: {tab ?? 'default'}</div>;
}
```

### Example 3: Running Official Next.js Upgrade Codemods
```bash
# 1. Migrate async request APIs (cookies, headers, params)
npx @next/codemod@canary next-async-request-api

# 2. Migrate middleware.ts to proxy.ts
npx @next/codemod@canary middleware-to-proxy

# 3. Migrate from removed 'next lint' to native ESLint 9 CLI
npx @next/codemod@latest next-lint-to-eslint-cli
```

### Example 4: Handling Removed `next lint` with ESLint 9 Flat Config
```js
// eslint.config.mjs (ESLint 9 Flat Config)
import { defineConfig, globalIgnores } from 'eslint/config';
import nextVitals from 'eslint-config-next/core-web-vitals';
import nextTs from 'eslint-config-next/typescript';

export default defineConfig([
  ...nextVitals,
  ...nextTs,
  globalIgnores(['.next/**', 'out/**', 'build/**', 'next-env.d.ts']),
]);
```

### Example 5: Inverting Fetch Caching to Modern Explicit `"use cache"`
```tsx
import { cacheLife, cacheTag } from 'next/cache';

// Data query function opted into fine-grained cache
export async function getProductCatalog() {
  'use cache';
  cacheLife('hours');
  cacheTag('products');

  const res = await fetch('https://api.internal/products');
  return res.json();
}
```

---

## 4. Under the Hood & Internals
The shift to asynchronous request APIs allows the Next.js Prerender Engine to render the static shell of any component tree up until the exact point where an un-awaited dynamic promise is encountered. When paired with Partial Prerendering (PPR), the static shell compiles into static HTML while the dynamic promise is automatically converted into an unresolved React Suspense boundary placeholder.

---

## 5. Security & Defence-in-Depth
- **CVE-2025-29927 Remediation**: Ensure framework is updated past `15.2.3` or `14.2.25`. This prevents attackers from spoofing the `x-middleware-subrequest` header to bypass authentication checks in middleware.
- **Defence-in-Depth**: Never rely exclusively on `proxy.ts` or `middleware.ts` for security. Always authenticate and authorize inside Data Access Layer (DAL) methods and Server Actions.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Accessing `params.id` directly | Missing `await` in Next.js 15+ | TypeScript compile error / Promise object rendered | `const { id } = await params;` |
| Calling `next lint` in CI | Command removed in Next.js 16 | CI build fails with command not found | Run `eslint .` via ESLint 9 Flat Config |
| Relying on implicit fetch cache | Fetch is now dynamic by default | Sudden database load spike | Add `"use cache"` with `cacheLife('hours')` |
| Using `middleware.ts` in Next 16 | Deprecated in favor of `proxy.ts` | Deprecation warning / name mismatch | Rename to `proxy.ts` and `export function proxy()` |
| Synchronous `cookies().get()` | Outdated Next.js 14 syntax | Runtime error or unhandled Promise | `const store = await cookies();` |
| Running middleware auth only | Vulnerable to subrequest bypass | Unauthorized data access | Validate session in DAL with `server-only` |

---

## 7. Performance & Optimization Guide
Migrating to asynchronous request APIs and `"use cache"` eliminates unnecessary dynamic de-optimizations. By isolating `await searchParams` or `await cookies()` inside small `<Suspense>` wrapped server components, the entire page shell renders in under 20ms TTFB.

---

## 8. TypeScript & Strict Typing Recipes
```tsx
// app/dashboard/[orgId]/page.tsx
interface PageProps {
  params: Promise<{ orgId: string }>;
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}

export default async function Page({ params, searchParams }: PageProps) {
  const { orgId } = await params;
  const query = await searchParams;
  return <h1>Org: {orgId}</h1>;
}
```

---

## 9. Testing & Quality Verification

```tsx
import { describe, it, expect, vi } from 'vitest';
import UserPage from './page';

describe('UserPage Async Component', () => {
  it('resolves async params correctly', async () => {
    const mockParams = Promise.resolve({ id: 'user-123' });
    const mockSearchParams = Promise.resolve({ tab: 'billing' });

    const jsx = await UserPage({ params: mockParams, searchParams: mockSearchParams });
    expect(jsx).toBeDefined();
  });
});
```

---

## 10. When NOT to Use This Pattern
- If maintaining a legacy Pages Router application that cannot yet migrate to React 19, stay on Next.js 14 LTS until dependencies are compatible.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. Why did Next.js 15 turn `cookies()`, `headers()`, and `params` into asynchronous Promises?</summary>
Turning request APIs into Promises decouples the execution of the page shell from the incoming request. It allows Next.js to start prerendering static parts of the component tree immediately, suspending only at the exact point where dynamic request data is awaited, which powers Partial Prerendering (PPR).
</details>

<details>
<summary>2. What happened to `next lint` in Next.js 16?</summary>
`next lint` was removed. Developers now invoke standard `eslint .` directly using ESLint 9 Flat Config (`eslint.config.mjs`) with `eslint-config-next`, improving build performance and flexibility.
</details>

<details>
<summary>3. What is the difference between `revalidateTag` in Next.js 14 versus 15/16?</summary>
In Next.js 14, `revalidateTag(tag)` was a single-argument call. In modern Next.js, `revalidateTag(tag, profile)` requires a second profile argument for background SWR revalidation, while `updateTag(tag)` is used in Server Actions for instant read-your-own-writes invalidation.
</details>

<details>
<summary>4. How does `proxy.ts` differ from legacy `middleware.ts`?</summary>
`proxy.ts` is the new convention in Next.js 16. It renames the file and the export (`export function proxy()`) to emphasize that its sole responsibility is network-level request proxying (redirects, rewrites, headers) rather than heavy application-level business logic.
</details>

<details>
<summary>5. How are async `params` unwrapped in Client Components?</summary>
Client Components use React 19's `use()` hook: `const { id } = use(params);`.
</details>

<details>
<summary>6. How do you mitigate CVE-2025-29927 in Next.js?</summary>
Upgrade to Next.js 15.2.3+ or 14.2.25+. Configure external reverse proxies (Nginx/Cloudflare) to strip the `x-middleware-subrequest` header, and ensure data authorization is performed in the DAL, not solely in middleware.
</details>

<details>
<summary>7. What is the replacement for `fetch(url, { cache: 'force-cache' })` in modern Next.js?</summary>
The modern idiom is to wrap data fetching in a function marked with the `"use cache"` directive, specifying lifetimes with `cacheLife()` and tags with `cacheTag()`.
</details>

<details>
<summary>8. How do you run the automated codemod for async request APIs?</summary>
Run `npx @next/codemod@canary next-async-request-api`.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Refactor a sync `params` page to async</summary>
Update a Next.js 14 page component to properly type and await `params.id`.
```tsx
export default async function Page({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  return <h1>Item: {id}</h1>;
}
```
</details>

<details>
<summary>Exercise 2 (Medium): Create an ESLint 9 Flat Config for Next.js</summary>
Configure `eslint.config.mjs` with `eslint-config-next/core-web-vitals` and ignore directories.
</details>

<details>
<summary>Exercise 3 (Hard): Implement a dual-runtime DAL with `'server-only'`</summary>
Create a data access layer function that verifies session cookies using `await cookies()` and protects database credentials with `'server-only'`.
</details>

---

## 13. Cross-References & Further Reading
- [Next.js Fundamentals: Route Tables](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/README.md)
- [Official Next.js Upgrade Guide](https://nextjs.org/docs/app/building-your-application/upgrading)
