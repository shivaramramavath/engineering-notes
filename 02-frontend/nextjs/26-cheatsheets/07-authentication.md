# Module 26: Authentication & Authorization Cheatsheet

> **Quick Reference**: Session cookies, Data Access Layer (DAL) pattern, Better-Auth / Auth.js comparison, Edge middleware proxy routing, and CVE-2025-29927 defense.

---

## 1. Defense-in-Depth Authentication Architecture

```
Incoming Request
      │
      ▼
[ Edge Middleware / proxy.ts ]  <--- Optimistic redirect to /login if cookie missing
      │
      ▼
[ Next.js Server Component / Action ]
      │
      ▼
[ Data Access Layer (DAL) ]      <--- Cryptographic token verification & DB tenant check
      │
      ▼
[ PostgreSQL / Redis Database ]  <--- Tenant-scoped query execution
```

---

## 2. Minimal Production Data Access Layer (`lib/dal.ts`)

```typescript
import 'server-only';
import { cache } from 'react';
import { cookies } from 'next/headers';
import { redirect } from 'next/navigation';
import { jwtVerify } from 'jose';

const JWT_SECRET = new TextEncoder().encode(process.env.SESSION_SECRET);

export interface SessionData {
  userId: string;
  role: 'admin' | 'user';
  orgId: string;
}

// React.cache guarantees single verification per request
export const verifySession = cache(async (): Promise<SessionData> => {
  const cookieStore = await cookies();
  const token = cookieStore.get('session_token')?.value;

  if (!token) {
    redirect('/login');
  }

  try {
    const { payload } = await jwtVerify(token, JWT_SECRET);
    return payload as unknown as SessionData;
  } catch {
    redirect('/login');
  }
});

// Role-Based Authorization Guard
export async function requireRole(allowedRole: 'admin') {
  const session = await verifySession();
  if (session.role !== allowedRole) {
    redirect('/unauthorized');
  }
  return session;
}
```

---

## 3. Session Cookie Best Practices

```typescript
// app/actions/login.ts
'use server';
import { cookies } from 'next/headers';

export async function setSessionCookie(token: string) {
  const cookieStore = await cookies();
  cookieStore.set('session_token', token, {
    httpOnly: true,                               // Prevents XSS theft
    secure: process.env.NODE_ENV === 'production',// HTTPS only
    sameSite: 'lax',                              // CSRF protection
    path: '/',
    maxAge: 60 * 60 * 24 * 7,                     // 7 days
  });
}
```

---

## 4. Auth.js (v5) vs Better-Auth Quick Comparison

| Feature | Auth.js (v5) | Better-Auth |
| :--- | :--- | :--- |
| **Philosophy** | Mature, provider-heavy ecosystem | TypeScript-first, Server Component native |
| **Built-in 2FA** | Requires custom configuration | ✅ Out-of-the-box plugin |
| **Passkeys / WebAuthn** | Experimental adapter | ✅ First-class plugin |
| **Multi-Tenancy** | Manual DB scoping | ✅ Teams & Organization plugin |
| **Client Type-Safety** | Good | Excellent (inferred client types) |
