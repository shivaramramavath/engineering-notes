# Authorization Interrupts: `unauthorized()` & `forbidden()` (`not-found-forbidden-unauthorized-conventions`)

> Declarative HTTP error interruption conventions in Next.js App Router, custom error UI templates (`unauthorized.tsx`, `forbidden.tsx`), robots meta tag injection, and security architecture.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Runtime** | Node.js Server |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Need for Granular Auth Interrupts
Historically, handling unauthenticated or unauthorized users in Next.js required imperative redirects (e.g. `redirect('/login')`). This approach suffered from three major architectural flaws:
1. **Ambiguous Status Codes**: A redirect returns an HTTP 307/308, masking genuine 401 Unauthorized or 403 Forbidden HTTP semantics from search engines and API clients.
2. **SEO Indexing Pollution**: Protected or restricted error pages were frequently indexed because servers returned successful 200/307 responses.
3. **Clunky UI Composition**: Redirecting users away from their intended URL destroys page context and requires state round-trips (`?redirect_to=...`).

Modern Next.js introduces two first-class navigation interrupt functions:
- **`unauthorized()`** (`next/navigation`): Halts rendering, emits an HTTP **401 Unauthorized** status, serves `app/unauthorized.tsx`, and automatically injects `<meta name="robots" content="noindex" />`.
- **`forbidden()`** (`next/navigation`): Halts rendering, emits an HTTP **403 Forbidden** status, serves `app/forbidden.tsx`, and automatically injects `<meta name="robots" content="noindex" />`.

```
[ Incoming Request ] ──> [ Page / Layout Server Component ]
                                       │
                                       ├── User Not Logged In ──> unauthorized() ──> [ 401 Status + unauthorized.tsx ]
                                       │
                                       └── Insufficient Role  ──> forbidden()    ──> [ 403 Status + forbidden.tsx ]
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Imperative Redirects
```tsx
// app/admin/page.tsx (Naive)
import { redirect } from 'next/navigation';
import { getSession } from '@/lib/auth';

export default async function AdminPage() {
  const session = await getSession();

  if (!session) {
    redirect('/login'); // Returns 307; loses semantic HTTP status
  }

  if (session.user.role !== 'admin') {
    redirect('/dashboard?error=unauthorized'); // Masking 403 Forbidden
  }

  return <h1>Admin Controls</h1>;
}
```

### Idiomatic: Declarative Auth Interrupts
```tsx
// app/admin/page.tsx (Idiomatic Next.js 15+)
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from '@/dal/session';

export default async function AdminPage() {
  const session = await verifySession();

  if (!session) {
    unauthorized(); // Serves app/unauthorized.tsx with 401 status & noindex
  }

  if (session.user.role !== 'admin') {
    forbidden(); // Serves app/forbidden.tsx with 403 status & noindex
  }

  return <h1>Admin Controls</h1>;
}
```

---

## 3. Five Progressive Examples

### Example 1: Enabling Auth Interrupts in `next.config.ts`
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    authInterrupts: true, // Enables unauthorized() and forbidden()
  },
};

export default nextConfig;
```

### Example 2: Creating Custom `app/unauthorized.tsx`
```tsx
// app/unauthorized.tsx
import Link from 'next/link';

export default function UnauthorizedPage() {
  return (
    <main className="flex min-h-screen flex-col items-center justify-center p-8 bg-slate-950 text-white">
      <div className="rounded-2xl border border-slate-800 bg-slate-900 p-8 max-w-md text-center">
        <h1 className="text-3xl font-extrabold text-amber-400">401 - Unauthorized</h1>
        <p className="mt-3 text-sm text-slate-400">
          You must be authenticated to access this resource. Please sign in to continue.
        </p>
        <Link
          href="/login"
          className="mt-6 inline-block rounded-xl bg-blue-600 px-6 py-2.5 text-sm font-semibold hover:bg-blue-700 transition"
        >
          Sign In
        </Link>
      </div>
    </main>
  );
}
```

### Example 3: Creating Custom `app/forbidden.tsx`
```tsx
// app/forbidden.tsx
import Link from 'next/link';

export default function ForbiddenPage() {
  return (
    <main className="flex min-h-screen flex-col items-center justify-center p-8 bg-slate-950 text-white">
      <div className="rounded-2xl border border-slate-800 bg-slate-900 p-8 max-w-md text-center">
        <h1 className="text-3xl font-extrabold text-rose-500">403 - Forbidden</h1>
        <p className="mt-3 text-sm text-slate-400">
          Your account does not possess the administrative privileges required to view this page.
        </p>
        <Link
          href="/dashboard"
          className="mt-6 inline-block rounded-xl border border-slate-700 px-6 py-2.5 text-sm font-semibold hover:bg-slate-800 transition"
        >
          Return to Dashboard
        </Link>
      </div>
    </main>
  );
}
```

### Example 4: Data Access Layer Protection with `server-only`
```tsx
// dal/billing.ts
import 'server-only';
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from './session';
import { db } from '@/db';

export async function getEnterpriseInvoices(orgId: string) {
  const session = await verifySession();
  if (!session) unauthorized();

  if (session.user.orgId !== orgId || session.user.role !== 'billing_admin') {
    forbidden();
  }

  return db.invoices.findMany({ where: { orgId } });
}
```

### Example 5: Handling Interrupts in Layouts vs Pages
```tsx
// app/(admin)/layout.tsx
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from '@/dal/session';

export default async function AdminLayout({ children }: { children: React.ReactNode }) {
  const session = await verifySession();
  if (!session) unauthorized();
  if (session.user.role !== 'admin') forbidden();

  return <div className="admin-shell">{children}</div>;
}
```

---

## 4. Under the Hood & Internals
When `unauthorized()` or `forbidden()` is invoked, Next.js throws an internal control-flow exception (similar to `notFound()` and `redirect()`).
1. The exception is caught by the nearest route segment error boundary.
2. Next.js sets the outgoing HTTP response status header to `401` or `403` (unless response streaming has already flushed the headers).
3. The runtime injects `<meta name="robots" content="noindex" />` into the generated HTML head to prevent indexing.
4. The corresponding `unauthorized.tsx` or `forbidden.tsx` component is rendered as the fallback UI.

---

## 5. Security & Defence-in-Depth
- **Mitigating CVE-2025-29927**: Because middleware-level authentication was vulnerable to header bypasses via `x-middleware-subrequest`, placing `unauthorized()` and `forbidden()` directly inside your Server Components and Data Access Layer provides genuine **defence-in-depth**. Even if network proxy rules fail, the component layer halts execution before database access occurs.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Missing `experimental.authInterrupts` | Flag disabled in config | Function throws generic unhandled error | Add `experimental: { authInterrupts: true }` |
| Calling inside `try/catch` | Swallowing internal control-flow error | Interrupt never triggers; page executes | Avoid wrapping `unauthorized()` in catch blocks |
| Using 401 instead of 403 | Confusing auth with authz | Logged-in users prompted to log in again | Use `unauthorized()` for 401, `forbidden()` for 403 |
| Missing `noindex` consideration | Custom error page without robots tag | Private error URLs indexed by Google | Handled automatically by `unauthorized()` |
| Client-side auth checks only | Hiding UI buttons via client state | Server data accessible via direct URL | Enforce `unauthorized()` in Server Components |

---

## 7. Performance & Optimization Guide
Interrupting execution via `unauthorized()` halts further child component rendering immediately. Database queries and nested layout renders downstream from the interrupt point are completely skipped, saving server CPU cycles and database connection pool slots.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import { unauthorized, forbidden } from 'next/navigation';

export type UserRole = 'admin' | 'editor' | 'viewer';

export function assertRole(userRole: UserRole, requiredRole: UserRole): asserts userRole is UserRole {
  if (userRole !== requiredRole) {
    forbidden();
  }
}
```

---

## 9. Testing & Quality Verification

```tsx
import { describe, it, expect, vi } from 'vitest';
import AdminPage from './page';

vi.mock('next/navigation', () => ({
  unauthorized: vi.fn(() => { throw new Error('NEXT_UNAUTHORIZED'); }),
  forbidden: vi.fn(() => { throw new Error('NEXT_FORBIDDEN'); }),
}));

describe('AdminPage Auth Interruption', () => {
  it('throws unauthorized when session is null', async () => {
    vi.mocked(verifySession).mockResolvedValueOnce(null);
    await expect(AdminPage()).rejects.toThrow('NEXT_UNAUTHORIZED');
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not use `unauthorized()` in public marketing pages where a missing session is normal behavior. Return anonymous fallback views instead.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What is the fundamental difference between `unauthorized()` and `forbidden()`?</summary>
`unauthorized()` emits an HTTP 401 status, signaling that the user is not authenticated (missing or invalid credentials) and should sign in. `forbidden()` emits an HTTP 403 status, signaling that the user is authenticated but lacks the necessary permissions or roles to access the resource.
</details>

<details>
<summary>2. Why are declarative auth interrupts preferred over `redirect('/login')`?</summary>
Interrupts preserve semantic HTTP status codes (401/403 instead of 307 redirects), automatically inject `noindex` robots meta tags to prevent SEO indexing, and render contextual UI without URL jumping.
</details>

<details>
<summary>3. What configuration flag is required to use `unauthorized()` and `forbidden()`?</summary>
Set `experimental: { authInterrupts: true }` in `next.config.ts`.
</details>

<details>
<summary>4. Why should you avoid wrapping `unauthorized()` in a `try...catch` block?</summary>
Like `redirect()` and `notFound()`, `unauthorized()` works by throwing an internal Next.js control-flow error. Catching the error without re-throwing prevents Next.js from halting the render and serving `unauthorized.tsx`.
</details>

<details>
<summary>5. How do auth interrupts provide defence-in-depth against CVE-2025-29927?</summary>
CVE-2025-29927 involved bypassing middleware via forged headers. Placing auth interrupts inside Server Components and Data Access Layer functions ensures that authorization is validated on the actual rendering server, rendering middleware bypass attacks harmless.
</details>

<details>
<summary>6. What happens if `unauthorized()` is called after streaming has started?</summary>
If HTTP headers have already been flushed to the client, the HTTP status code cannot be altered from 200, but Next.js will stream the `unauthorized.tsx` UI to replace the page content.
</details>

<details>
<summary>7. Where should `unauthorized.tsx` and `forbidden.tsx` files be located?</summary>
They can be placed at the root of `app/` (e.g. `app/unauthorized.tsx`) or colocated inside specific route segments and route groups.
</details>

<details>
<summary>8. Do auth interrupts automatically prevent search engine indexing?</summary>
Yes, Next.js automatically injects `<meta name="robots" content="noindex" />` when rendering `unauthorized.tsx` or `forbidden.tsx`.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create an unauthorized error template</summary>
Create `app/unauthorized.tsx` with a clean login button and helpful prompt.
</details>

<details>
<summary>Exercise 2 (Medium): Protect a route segment with role checks</summary>
Implement a Server Component that checks user membership and calls `forbidden()` for non-admin users.
</details>

<details>
<summary>Exercise 3 (Hard): Build a Data Access Layer guard</summary>
Create a reusable DAL utility function `enforcePermission(permission: string)` that triggers `unauthorized()` or `forbidden()` before returning database records.
</details>

---

## 13. Cross-References & Further Reading
- [Authentication Architecture & Session Strategies](file:///E:/Learning/engineering-notes/02-frontend/nextjs/11-authentication/README.md)
- [Official Next.js Authorization Interrupts](https://nextjs.org/docs/app/api-reference/functions/unauthorized)
