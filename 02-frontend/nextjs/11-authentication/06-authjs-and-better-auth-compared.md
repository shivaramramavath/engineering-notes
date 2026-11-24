# Auth Architecture: Auth.js v5, Better Auth & Clerk (`authjs-and-better-auth-compared`)

> In-depth architectural comparison of modern Next.js authentication solutions: Auth.js v5 (NextAuth), Better Auth (TypeScript-first), and Clerk (Hosted enterprise identity), session strategies, and defense-in-depth authorization.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Auth Libraries** | Auth.js v5 / Better Auth / Clerk |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Modern Auth Landscape
Authentication in Next.js has matured beyond client-side JWT storage. Modern systems operate on standard Web `Request`/`Response` objects, supporting both Edge and Node.js runtimes.
- **Auth.js v5 (NextAuth)**: Open-source standard, database session adapters (Prisma, Drizzle), universal `auth()` helper working across RSC, Actions, and Route Handlers.
- **Better Auth**: Modern TypeScript-first auth framework with built-in Passkeys, 2FA, multi-session management, and native plugin architecture.
- **Clerk**: Turnkey hosted authentication with enterprise SSO, pre-built accessible UI components, and managed security compliance.

```
+-----------------------------------------------------------------------+
|                    Next.js Authentication Architecture                |
|                                                                       |
|  [ User Login Form ] ──(Credentials / OAuth / Passkey)                |
|           │                                                           |
|           ▼                                                           |
|  [ Auth Service / Route Handler ] ──> [ Database Session Table ]      |
|           │                                                           |
|           ▼                                                           |
|  [ Set-Cookie: session_token (httpOnly, secure, SameSite=Lax) ]       |
|                                                                       |
|  [ Inbound Requests ] ──> [ proxy.ts (Redirect Check) ]               |
|                                   │                                   |
|                                   ▼                                   |
|               [ DAL: verifySession() in server-only ]                 |
+-----------------------------------------------------------------------+
```

---

## 2. Comparison Matrix: Choosing Your Auth Stack

| Feature | Auth.js (v5) | Better Auth | Clerk |
| :--- | :--- | :--- | :--- |
| **Hosting Model** | Self-hosted (In your DB) | Self-hosted (In your DB) | Fully hosted cloud service |
| **Passkeys / WebAuthn**| Requires external config | Built-in native support | Native turnkey support |
| **TypeScript Ergonomics**| Good | Exceptional (strict inferred schemas) | Excellent |
| **Multi-Tenancy / Orgs**| Custom schema modeling | Built-in organization plugin | Enterprise Organizations built-in |
| **Edge Compatibility** | Full (Node & Edge) | Full (Node & Edge) | Full |
| **Cost** | 100% Free / Open Source | 100% Free / Open Source | Free tier + per-MAU pricing |

---

## 3. Five Progressive Examples

### Example 1: Auth.js v5 Universal `auth()` Helper
```ts
// auth.ts
import NextAuth from 'next-auth';
import GitHub from 'next-auth/providers/github';
import { PrismaAdapter } from '@auth/prisma-adapter';
import { db } from '@/db';

export const { handlers, signIn, signOut, auth } = NextAuth({
  adapter: PrismaAdapter(db),
  providers: [GitHub],
  session: { strategy: 'database' },
});
```
```tsx
// app/dashboard/page.tsx (RSC consuming auth)
import { auth } from '@/auth';
import { unauthorized } from 'next/navigation';

export default async function DashboardPage() {
  const session = await auth();
  if (!session) unauthorized();

  return <h1>Welcome, {session.user?.name}</h1>;
}
```

### Example 2: Better Auth Setup with Passkeys
```ts
// lib/auth.ts
import { betterAuth } from 'better-auth';
import { prismaAdapter } from 'better-auth/adapters/prisma';
import { passkey } from 'better-auth/plugins/passkey';
import { db } from '@/db';

export const auth = betterAuth({
  database: prismaAdapter(db, { provider: 'postgresql' }),
  plugins: [passkey()],
  emailAndPassword: { enabled: true },
});
```

### Example 3: Defense-in-Depth DAL Session Verification
```ts
// dal/session.ts
import 'server-only';
import { cache } from 'react';
import { cookies } from 'next/headers';
import { db } from '@/db';

export const verifySession = cache(async () => {
  const cookieJar = await cookies();
  const sessionToken = cookieJar.get('session_token')?.value;

  if (!sessionToken) return null;

  const session = await db.session.findUnique({
    where: { token: sessionToken },
    include: { user: true },
  });

  if (!session || session.expiresAt < new Date()) {
    return null;
  }

  return session;
});
```

### Example 4: Role-Based Authorization Guard in Server Action
```ts
// actions/adminActions.ts
'use server';

import { verifySession } from '@/dal/session';
import { unauthorized, forbidden } from 'next/navigation';
import { db } from '@/db';

export async function deleteTenant(tenantId: string) {
  const session = await verifySession();
  if (!session) unauthorized();
  if (session.user.role !== 'SUPER_ADMIN') forbidden();

  await db.tenant.delete({ where: { id: tenantId } });
}
```

### Example 5: Clerk Authentication Integration
```tsx
// app/layout.tsx
import { ClerkProvider, SignInButton, SignedIn, SignedOut, UserButton } from '@clerk/nextjs';

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <ClerkProvider>
      <html lang="en">
        <body>
          <header className="flex justify-between p-4">
            <SignedOut>
              <SignInButton />
            </SignedOut>
            <SignedIn>
              <UserButton />
            </SignedIn>
          </header>
          {children}
        </body>
      </html>
    </ClerkProvider>
  );
}
```

---

## 4. Under the Hood & Internals
Database sessions store an opaque cryptographic token (e.g. 256-bit random string) in a database table mapping to the user record.
- **Revocation**: Unlike stateless JWTs (which cannot be revoked until expiry without blacklists), database sessions can be invalidated instantly by deleting the row from the `sessions` table.
- **Cookie Security**: Set with `httpOnly: true` (inaccessible to JavaScript `document.cookie` to prevent XSS theft), `secure: true` (transmitted only over HTTPS), and `sameSite: 'lax'` (protecting against CSRF).

---

## 5. Security & Defence-in-Depth
- **Mitigating CVE-2025-29927**: Never use edge proxy as your only authentication gate! Always invoke `verifySession()` in your Data Access Layer and inside every Server Action.

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| Storing JWT in `localStorage` | Client-side mindset | Vulnerable to XSS token exfiltration | Use `httpOnly` secure cookies |
| Stateless JWTs without revoking | Cannot log out compromised users | Stolen token valid until expiry | Use database sessions or short-lived tokens |
| Missing auth inside Server Actions | Assuming actions are protected by UI | Malicious POST deletes arbitrary data | Check `verifySession()` in action body |
| Missing `server-only` in DAL | Accidental client bundle leakage | Secret keys visible in browser JS | Add `import 'server-only'` to session files |

---

## 7. Performance & Optimization Guide
Use React `cache()` on `verifySession()`. When multiple Server Components in the same page layout call `verifySession()`, the database session lookup executes exactly once, eliminating duplicate queries.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export interface AuthenticatedUser {
  id: string;
  email: string;
  role: 'ADMIN' | 'MEMBER';
}

export interface SessionContext {
  user: AuthenticatedUser;
  expiresAt: Date;
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { verifySession } from './session';

describe('verifySession DAL', () => {
  it('returns null when cookie is absent', async () => {
    const session = await verifySession();
    expect(session).toBeNull();
  });
});
```

---

## 10. When NOT to Use This Pattern
- If building a purely static public blog with no user accounts, do not add authentication overhead.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. What are the trade-offs between Database Sessions vs JWTs in Next.js?</summary>
Database sessions allow instantaneous revocation and avoid token size limits, but require a database lookup per request. JWTs are stateless and fast, but cannot be easily revoked before expiration and increase HTTP header payload sizes.
</details>

<details>
<summary>2. Why is storing authentication tokens in `localStorage` an anti-pattern?</summary>
Because any cross-site scripting (XSS) vulnerability can access `window.localStorage` and exfiltrate user credentials. `httpOnly` cookies cannot be accessed by client-side JavaScript.
</details>

<details>
<summary>3. How does Auth.js v5 differ from NextAuth v4?</summary>
Auth.js v5 uses standard Web Request/Response objects, works seamlessly across Edge and Node runtimes, and introduces a single universal `auth()` function.
</details>

<details>
<summary>4. What is Better Auth?</summary>
Better Auth is a TypeScript-first authentication framework with built-in plugins for Passkeys, 2FA, organization multi-tenancy, and strict schema inference.
</details>

<details>
<summary>5. Why must Server Actions verify authentication independently of middleware?</summary>
Because middleware can be bypassed (as in CVE-2025-29927) or misconfigured with matcher exclusions. Server Actions are public HTTP POST endpoints that must validate session identity internally.
</details>

<details>
<summary>6. How does React `cache()` optimize session verification?</summary>
It memoizes the `verifySession()` promise per request, ensuring that multiple layout and page components share a single database lookup.
</details>

<details>
<summary>7. What cookie flags are essential for security?</summary>
`httpOnly: true`, `secure: true`, and `sameSite: 'lax'`.
</details>

<details>
<summary>8. How do you implement Passkeys in Next.js?</summary>
Using Better Auth's `@better-auth/passkey` plugin or SimpleWebAuthn with WebAuthn browser APIs.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a session cookie check</summary>
Write a `verifySession` function that reads cookies via `await cookies()`.
</details>

<details>
<summary>Exercise 2 (Medium): Build an admin route guard</summary>
Implement a Server Component that checks role membership and triggers `forbidden()`.
</details>

<details>
<summary>Exercise 3 (Hard): Configure Auth.js v5 with Prisma</summary>
Set up Auth.js v5 with a PostgreSQL database adapter and GitHub OAuth provider.
</details>

---

## 13. Cross-References & Further Reading
- [Security: CVE Analysis & Taint APIs](file:///E:/Learning/engineering-notes/02-frontend/nextjs/21-security/README.md)
- [Official Auth.js Documentation](https://authjs.dev)
