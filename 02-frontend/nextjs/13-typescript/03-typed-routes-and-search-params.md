# Typed Routes, Async SearchParams & Zod Validation (`typed-routes-and-search-params`)

> Deep type safety in Next.js 15+ and 16: Enabling typed routes, typing asynchronous route params and searchParams, and end-to-end Zod boundary validation.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **TypeScript** | 5.7+ Strict |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 Type Safety in Modern Next.js
Historically, URLs in web frameworks were plain strings: typos like `<Link href="/user/dashbord">` went unnoticed until a user encountered a 404 in production.
Modern Next.js delivers end-to-end type safety across the entire application stack:
1. **Typed Routes**: Statically validates every internal link against the actual file system route tree.
2. **Asynchronous Props**: Enforces typing on `params` and `searchParams` as `Promise` objects.
3. **Zod Boundary Validation**: Validates external inputs (query parameters, form submissions, webhook payloads) into strongly typed models.

```
[ Developer Types <Link href="/invalid"> ] ──> [ TypeScript Compiler (Typed Routes) ]
                                                              │
                                                              ▼
                                                   [ Compile Error: Route Not Found ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Untyped Strings & Sync Props
```tsx
// Naive (Next.js 14)
import Link from 'next/link';

export default function Page({ searchParams }: { searchParams: { query?: string } }) {
  // Vulnerability: No input sanitization, sync prop access breaks in 15+
  return <Link href="/catalog?category=shoes">Shop</Link>; // Typo risk!
}
```

### Idiomatic: Typed Routes & Async Zod Validation
```tsx
// Idiomatic (Next.js 15+)
import Link from 'next/link';
import type { Route } from 'next';
import { z } from 'zod';

const searchSchema = z.object({
  query: z.string().default(''),
  page: z.coerce.number().min(1).default(1),
});

interface PageProps {
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}

export default async function Page({ searchParams }: PageProps) {
  const rawParams = await searchParams;
  const { query, page } = searchSchema.parse(rawParams);

  // Type safe: TypeScript verifies that /catalog exists!
  return <Link href={'/catalog' as Route}>Shop Shoes (Page {page})</Link>;
}
```

---

## 3. Five Progressive Examples

### Example 1: Enabling Typed Routes in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    typedRoutes: true, // Generates .next/types/link.d.ts automatically
  },
};

export default nextConfig;
```

### Example 2: Typing Dynamic Route Parameters
```tsx
// app/organizations/[orgId]/members/[memberId]/page.tsx
interface PageProps {
  params: Promise<{
    orgId: string;
    memberId: string;
  }>;
}

export default async function MemberPage({ params }: PageProps) {
  const { orgId, memberId } = await params;
  return <h1>Org: {orgId} | Member: {memberId}</h1>;
}
```

### Example 3: Validating Server Action Payloads with Zod
```ts
// actions/userActions.ts
'use server';

import { z } from 'zod';

const createUserSchema = z.object({
  name: z.string().min(2, 'Name must be at least 2 chars'),
  email: z.string().email('Invalid email address'),
  role: z.enum(['MEMBER', 'ADMIN']).default('MEMBER'),
});

export type CreateUserInput = z.infer<typeof createUserSchema>;

export async function createUserAction(rawInput: unknown) {
  const result = createUserSchema.safeParse(rawInput);
  if (!result.success) {
    return { success: false, errors: result.error.flatten().fieldErrors };
  }

  // Strongly typed as { name: string, email: string, role: "MEMBER" | "ADMIN" }
  const validData = result.data;
  return { success: true, data: validData };
}
```

### Example 4: Type-Safe URL Navigation with `Route`
```tsx
import Link from 'next/link';
import type { Route } from 'next';

export function NavigationBar() {
  // TypeScript will fail compilation if '/dashboard' or '/settings' do not exist in app/!
  const dashboardRoute: Route = '/dashboard';
  return (
    <nav className="flex gap-4">
      <Link href={dashboardRoute}>Dashboard</Link>
      <Link href="/settings">Settings</Link>
    </nav>
  );
}
```

### Example 5: Type-Safe Environment Variables via `@t3-oss/env-nextjs`
```ts
// env.mjs
import { createEnv } from '@t3-oss/env-nextjs';
import { z } from 'zod';

export const env = createEnv({
  server: {
    DATABASE_URL: z.string().url(),
    REDIS_URL: z.string().url(),
  },
  client: {
    NEXT_PUBLIC_APP_URL: z.string().url(),
  },
  runtimeEnv: {
    DATABASE_URL: process.env.DATABASE_URL,
    REDIS_URL: process.env.REDIS_URL,
    NEXT_PUBLIC_APP_URL: process.env.NEXT_PUBLIC_APP_URL,
  },
});
```

---

## 4. Under the Hood & Internals
When `typedRoutes: true` is enabled, Next.js scans the `app/` file hierarchy and compiles a `.next/types/link.d.ts` declaration file. It builds a union of literal string paths (e.g. `type Route = '/' | '/dashboard' | '/products/${string}'`). Any `<Link href="...">` or `router.push(...)` call is type-checked against this union at compile time.

---

## 5. Security & Boundary Hardening
- **Never Trust Client Params**: URL search parameters and dynamic slugs originate directly from external user requests. Always sanitize inputs with Zod schemas before passing them into SQL queries or filesystem operations to prevent injection vulnerabilities.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Missing `await` on `params` | Next.js 15+ breaking change | `[object Promise]` rendered or runtime error | `const { id } = await params;` |
| Using string casts `as any` on links | Disabling typed routes | Broken links reach production unnoticed | Use typed routes without casts |
| Omitting Zod coercion | `searchParams` are strings | `page: "1"` treated as string instead of number | Use `z.coerce.number()` in schema |
| Accessing `process.env` directly | Typo in environment variable | Silent undefined error at runtime | Validate with `@t3-oss/env-nextjs` |

---

## 7. Performance & Optimization Guide
TypeScript compile-time checks have **zero runtime overhead**. Validating with Zod at network boundaries ensures downstream components and database drivers receive sanitized, predictable shapes, eliminating defensive runtime assertions throughout the codebase.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export type AsyncPageProps<TParams = {}, TSearchParams = {}> = {
  params: Promise<TParams>;
  searchParams: Promise<TSearchParams>;
};
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { createUserAction } from './userActions';

describe('Zod Validation in Actions', () => {
  it('rejects invalid email address', async () => {
    const res = await createUserAction({ name: 'Alice', email: 'invalid-email' });
    expect(res.success).toBe(false);
  });
});
```

---

## 10. When NOT to Use This Pattern
- When generating dynamic links to external third-party websites (e.g. `https://stripe.com`), use standard anchor tags `<a>` or cast external URLs properly.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How do typed routes work in Next.js?</summary>
Next.js scans the `app/` directory and generates a TypeScript declaration file (`.next/types/link.d.ts`) containing a union of all valid routes. `<Link>` and `router.push` enforce that URLs match a valid route literal.
</details>

<details>
<summary>2. Why are `params` and `searchParams` typed as Promises in Next.js 15+?</summary>
To support Partial Prerendering (PPR) and asynchronous request handling, decoupling static prerender compilation from dynamic runtime parameter resolution.
</details>

<details>
<summary>3. What is `z.coerce.number()` in Zod?</summary>
It converts string values (such as query parameter `"42"`) into numbers before validation, which is necessary because URL search parameters are always parsed as strings.
</details>

<details>
<summary>4. How does `@t3-oss/env-nextjs` protect environment variables?</summary>
It parses `process.env` using Zod schemas at build and runtime, halting the application immediately if any required database string or API key is missing or malformed.
</details>

<details>
<summary>5. Can typed routes validate dynamic slugs like `/products/123`?</summary>
Yes. Next.js generates template literal types like `/products/${string}`, allowing valid dynamic segments while preventing completely invalid base paths.
</details>

<details>
<summary>6. How do you type Server Action return values?</summary>
Use a discriminated union: `type ActionResponse<T> = { success: true, data: T } | { success: false, errors: Record<string, string[]> }`.
</details>

<details>
<summary>7. How do you unwrap async params in Client Components?</summary>
Using React 19's `use()` hook: `const { id } = use(params);`.
</details>

<details>
<summary>8. How do you enable typed routes in your project?</summary>
Set `experimental: { typedRoutes: true }` in `next.config.ts`.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Enable typed routes</summary>
Turn on `typedRoutes: true` in `next.config.ts` and verify `.next/types/link.d.ts` is generated.
</details>

<details>
<summary>Exercise 2 (Medium): Validate search parameters</summary>
Create a Zod schema validating `page`, `sort`, and `filter` query parameters with defaults.
</details>

<details>
<summary>Exercise 3 (Hard): Build type-safe Server Action wrapper</summary>
Implement a generic `createSafeAction(schema, handler)` utility that validates inputs and infers TypeScript types automatically.
</details>

---

## 13. Cross-References & Further Reading
- [Setup: TypeScript & Environment Configuration](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/01-typescript.md)
- [Official Next.js Typed Routes Documentation](https://nextjs.org/docs/app/building-your-application/configuring/typescript#statically-typed-links)
