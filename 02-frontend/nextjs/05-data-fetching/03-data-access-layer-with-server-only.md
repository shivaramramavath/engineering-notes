# Data Access Layer & Preload Pattern (`data-access-layer-with-server-only`)

> Designing a secure Data Access Layer (DAL) in Next.js using `server-only`, request memoization with React `cache()`, eager preload patterns, and DTO boundaries.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Server-Only DAL |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Need for a Data Access Layer (DAL)
In naive Next.js tutorials, components make direct database queries (`prisma.user.findMany()`) directly inside Page or Component files. This leads to three severe issues:
1. **Security Vulnerabilities**: Authorization checks are scattered or forgotten across different components.
2. **Component Waterfalls**: Deeply nested components make sequential queries, multiplying database round-trips.
3. **Over-fetching**: Database records with sensitive columns (password hashes, secret keys) are passed down directly to client components.

A **Data Access Layer (DAL)** centralizes:
- **Authentication & Authorization**: Verify sessions and roles before querying.
- **Data Transfer Objects (DTOs)**: Sanitize records before returning.
- **Request Memoization**: Deduplicate identical calls across different components in the same render pass using React `cache()`.

```
[ Page / Layout Component ] ──> [ Data Access Layer (server-only) ]
                                             │
                                             ├── 1. Session & Role Verification
                                             ├── 2. React cache() Request Deduplication
                                             ├── 3. Database Execution (Prisma / Drizzle)
                                             └── 4. DTO Sanitization & Return
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Direct Database Queries in UI
```tsx
// app/users/page.tsx (Naive - Scattered Auth & Raw Records)
import { db } from '@/db';

export default async function UsersPage() {
  // Vulnerability: No role check! Raw database records with passwordHash exposed
  const users = await db.user.findMany();
  return <ClientUserList users={users} />;
}
```

### Idiomatic: Centralized DAL with `server-only`
```tsx
// dal/users.ts (Idiomatic DAL)
import 'server-only';
import { cache } from 'react';
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from '@/dal/session';
import { db } from '@/db';

export const getUsers = cache(async () => {
  const session = await verifySession();
  if (!session) unauthorized();
  if (session.user.role !== 'admin') forbidden();

  const rawUsers = await db.user.findMany();
  return rawUsers.map((u) => ({
    id: u.id,
    name: u.name,
    email: u.email,
  }));
});
```

---

## 3. Five Progressive Examples

### Example 1: Implementing the Preload Pattern
```tsx
// dal/users.ts
import 'server-only';
import { cache } from 'react';
import { db } from '@/db';

export const getUser = cache(async (id: string) => {
  return db.user.findUnique({ where: { id } });
});

// Eager preload pattern: Kicks off fetch immediately before component renders
export const preloadUser = (id: string) => {
  void getUser(id);
};
```
```tsx
// app/users/[id]/page.tsx
import { preloadUser, getUser } from '@/dal/users';
import { UserDetails } from './UserDetails';

export default async function Page({ params }: { params: Promise<{ id: string }> }) {
  const { id } = await params;
  preloadUser(id); // Eagerly trigger fetch while child components compile

  return <UserDetails id={id} />;
}
```

### Example 2: React `cache()` Request Deduplication
```tsx
// dal/settings.ts
import 'server-only';
import { cache } from 'react';

// Even if called by 5 different components during a single page render,
// the underlying fetch/query runs exactly ONCE!
export const getOrgSettings = cache(async (orgId: string) => {
  console.log('Querying DB for org settings:', orgId);
  return db.orgSettings.findUnique({ where: { orgId } });
});
```

### Example 3: URL State Management with `nuqs`
```tsx
// app/catalog/ProductFilter.tsx
'use client';

import { useQueryState, parseAsString, parseAsInteger } from 'nuqs';

export function ProductFilter() {
  const [category, setCategory] = useQueryState('category', parseAsString.withDefault('all'));
  const [minPrice, setMinPrice] = useQueryState('minPrice', parseAsInteger.withDefault(0));

  return (
    <div className="flex gap-4">
      <select value={category} onChange={(e) => setCategory(e.target.value)}>
        <option value="all">All</option>
        <option value="electronics">Electronics</option>
      </select>
      <input
        type="number"
        value={minPrice}
        onChange={(e) => setMinPrice(Number(e.target.value))}
      />
    </div>
  );
}
```

### Example 4: External API Resiliency with Abort Controller & Retries
```ts
// dal/externalApi.ts
import 'server-only';

export async function fetchWithTimeout(url: string, timeoutMs: number = 3000) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const res = await fetch(url, { signal: controller.signal });
    if (!res.ok) throw new Error(`HTTP error: ${res.status}`);
    return await res.json();
  } finally {
    clearTimeout(id);
  }
}
```

### Example 5: Clean DTO Separation
```ts
// dal/dto/invoice.ts
import 'server-only';

export interface InvoiceDTO {
  id: string;
  totalFormatted: string;
  isPaid: boolean;
}

export function toInvoiceDTO(raw: any): InvoiceDTO {
  return {
    id: raw.id,
    totalFormatted: `$${(raw.amountCents / 100).toFixed(2)}`,
    isPaid: raw.status === 'PAID',
  };
}
```

---

## 4. Under the Hood & Internals
React's `cache()` function uses a WeakMap bound to the active request's Fiber reconciler dispatcher. When called with primitive arguments (`id: string`), it performs a strict equality check (`Object.is`). If the key matches an existing entry in the current request phase, it returns the memoized promise immediately, bypassing execution.

---

## 5. Security & Defence-in-Depth
- **CVE-2025-29927 Mitigation**: Always verify authentication and role permissions inside the DAL functions, rather than assuming the request passed through middleware security checks.
- **Zero Raw Object Exposure**: Never return raw ORM entities directly to Page components. Convert them through explicit DTO sanitizers.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Querying DB in Client Component | Missing server boundary | Build crashes on missing `fs` / `net` | Move query to DAL and call from Server Component |
| Missing React `cache()` | Re-fetching same entity in header & page | Multiple duplicate SQL queries per request | Wrap DAL function in `cache()` |
| Passing objects to `cache()` | Reference inequality defeats memoization | Cache misses on every call | Pass primitive IDs (`string`, `number`) to `cache()` |
| Omitting `server-only` in DAL | Accidental client import | Build does not catch leaking credentials | Add `import 'server-only'` to all DAL files |

---

## 7. Performance & Optimization Guide
The preload pattern (`preloadUser(id)`) combined with React `cache()` eliminates request waterfalls. By triggering the async query at the top of the route segment, the network request runs in parallel while downstream React components are initialized.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import 'server-only';
import { cache } from 'react';

export const getCachedItem = cache(async <T>(id: string, fetcher: (id: string) => Promise<T>): Promise<T> => {
  return fetcher(id);
});
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { getUsers } from './users';

describe('Data Access Layer', () => {
  it('deduplicates sequential calls via cache()', async () => {
    const res1 = await getUsers();
    const res2 = await getUsers();
    expect(res1).toBe(res2); // Same memoized promise result
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not use React `cache()` for mutations. Mutations must execute fresh with new side-effects.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What is the difference between React `cache()` and Next.js `"use cache"`?</summary>
React `cache()` provides per-request memoization to deduplicate duplicate database queries or calculations within a single render pass. Next.js `"use cache"` provides persistent cross-request caching across users and requests with configurable lifetimes (`cacheLife`) and invalidation tags (`cacheTag`).
</details>

<details>
<summary>2. Why does the preload pattern work in React Server Components?</summary>
Because invoking a cached function without awaiting it (`preloadUser(id)`) begins executing the async promise immediately. When a child component later calls `await getUser(id)`, the promise is already in-flight or resolved.
</details>

<details>
<summary>3. Why must arguments to React `cache()` be primitives?</summary>
Because React `cache()` compares arguments using `Object.is()`. Passing newly created object literals `{ id }` results in reference inequality, creating a cache miss on every invocation.
</details>

<details>
<summary>4. What role does `server-only` play in a Data Access Layer?</summary>
It guarantees at compile time that database queries and private business logic inside the DAL can never be imported into Client Components.
</details>

<details>
<summary>5. How does a DAL solve CVE-2025-29927?</summary>
By ensuring that authorization checks happen at the point of data access, rather than solely at the network middleware boundary.
</details>

<details>
<summary>6. How does `nuqs` improve URL state management in Next.js?</summary>
`nuqs` provides type-safe, debounced, and synchronous-feeling client state synchronized directly with the browser URL query string without manual `useSearchParams` boilerplate.
</details>

<details>
<summary>7. What is a DTO in Next.js development?</summary>
A Data Transfer Object (DTO) is a sanitized plain object containing only the exact fields required by the UI, stripped of internal database IDs, hashes, and secrets.
</details>

<details>
<summary>8. Does React `cache()` persist data between different users?</summary>
No. React `cache()` is strictly request-scoped and is automatically garbage-collected at the end of each HTTP request.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a cached user DAL function</summary>
Write a `getUserById(id: string)` function wrapped in React `cache()`.
</details>

<details>
<summary>Exercise 2 (Medium): Implement the preload pattern</summary>
Export a `preloadUser(id)` helper and call it at the layout root.
</details>

<details>
<summary>Exercise 3 (Hard): Build a complete DAL service with DTO mapping</summary>
Create a full user DAL with session verification, role check, and sanitized DTO transformation.
</details>

---

## 13. Cross-References & Further Reading
- [Modern Caching & "use cache"](file:///E:/Learning/engineering-notes/02-frontend/nextjs/06-caching/README.md)
- [Official Next.js Data Fetching Patterns](https://nextjs.org/docs/app/building-your-application/data-fetching/patterns)
