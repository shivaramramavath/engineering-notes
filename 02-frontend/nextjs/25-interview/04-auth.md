# Senior & Staff Interview Guide: Authentication and Session Security

> Comprehensive technical interview mastery guide covering Next.js authentication architectures, session cookies, JWT vs Database sessions, Middleware / `proxy.ts`, Data Access Layer (DAL) security, and preventing CVE-2025-29927.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [11-authentication/README.md](../11-authentication/README.md), [08-route-handlers-and-proxy/05-proxy-patterns-and-cve-2025-29927.md](../08-route-handlers-and-proxy/05-proxy-patterns-and-cve-2025-29927.md) |
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

Interviewers evaluating senior candidates on authentication in Next.js test:
- **Defense-in-Depth Authentication**: Why edge middleware / `proxy.ts` should only be used for optimistic routing/redirection, NOT as the sole security boundary.
- **The Data Access Layer (DAL)**: Implementing `verifySession()` and memoized session resolution using `React.cache()` inside server components and server actions.
- **Session Strategies**: Stateless encrypted JWTs (jose) vs Stateful database sessions (Redis/Postgres).
- **Security Vulnerabilities**: CVE-2025-29927 middleware bypass mechanics and mitigation strategies.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: Why is relying solely on `middleware.ts` (or `proxy.ts`) for route protection considered an anti-pattern in modern Next.js?</summary>

**Answer**:
1. **Network Boundary Bypass**: If middleware is misconfigured (e.g., regex matcher errors or subrequest spoofing like CVE-2025-29927), requests can bypass middleware entirely and land directly on Server Components or Server Actions.
2. **Server Actions Bypass**: Server Actions can be invoked directly via POST requests with arbitrary paths, bypassing path-based middleware matchers.
3. **Data Access Decoupling**: If page rendering assumes the user is authenticated because "middleware let the request pass", unauthenticated data queries can execute if the component is ever imported elsewhere.
4. **Architectural Best Practice**: **Defense in Depth**. Middleware performs an initial optimistic check (redirecting unauthenticated users to `/login`), while the **Data Access Layer (DAL)** strictly validates the session token before querying private database records.
</details>

<details>
<summary>Q2: What was CVE-2025-29927, and what fundamental architectural flaw did it expose?</summary>

**Answer**:
- **The Vulnerability**: CVE-2025-29927 (CVSS 9.1 Critical) allowed external attackers to bypass `middleware.ts` entirely by injecting internal Next.js rewrite headers (such as `x-middleware-subrequest: 1`).
- Next.js treated the incoming external request as if it were an internal subrequest that had already been processed by middleware, routing the request straight to the page handlers without executing `middleware.ts`.
- **The Flaw**: Relying on edge proxy headers as trusted authentication assertions without validating the session inside the actual server execution context.
- **Resolution**: Next.js 15.2.3+ stripped internal middleware headers from external ingress, and engineering teams adopted mandatory DAL verification.
</details>

<details>
<summary>Q3: How do you prevent redundant database queries when verifying the session across multiple nested Server Components in a single render?</summary>

**Answer**:
Use React's per-request memoization function: `React.cache()` inside the DAL.

```typescript
// lib/dal.ts
import 'server-only';
import { cache } from 'react';
import { cookies } from 'next/headers';
import { decryptSessionToken } from '@/lib/session';

export const verifySession = cache(async () => {
  const cookieStore = await cookies();
  const token = cookieStore.get('session')?.value;
  if (!token) return { isAuth: false, userId: null };

  const session = await decryptSessionToken(token);
  if (!session?.userId) return { isAuth: false, userId: null };

  return { isAuth: true, userId: session.userId, role: session.role };
});
```

Because `verifySession` is wrapped in `cache()`, calling it in `layout.tsx`, `page.tsx`, and multiple nested Server Components executes the decryption/verification query **exactly once per incoming HTTP request**.
</details>

<details>
<summary>Q4: Compare Auth.js (NextAuth) with Better-Auth for a modern Next.js 15+ greenfield application.</summary>

**Answer**:
- **Auth.js (v5)**:
  - Deep Vercel/Next.js lineage.
  - Strengths: Broad OAuth provider ecosystem (50+ providers), native adapter support for Prisma/Drizzle.
  - Challenges: Historical API churn, configuration complexity across Edge vs Node runtimes.
- **Better-Auth**:
  - Modern TypeScript-first architecture designed specifically for Server Components and Server Actions.
  - Strengths: Built-in two-factor authentication (2FA), organization/team multi-tenancy, passkeys (WebAuthn), and direct CLI schema generation for Drizzle/Prisma.
  - Architecture: Modular plugin system, strict separation of client vs server APIs, and fully typed hooks.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Cookie Mutation in Server Component
What will happen when attempting to set a cookie inside a Server Component?

```tsx
// app/dashboard/page.tsx
import { cookies } from 'next/headers';

export default async function DashboardPage() {
  const cookieStore = await cookies();
  // Attempting to refresh the session token
  cookieStore.set('session', 'newToken123'); // What happens?

  return <div>Welcome to Dashboard</div>;
}
```

**Evaluation**:
- **Throws Runtime Error**: Server Components are read-only regarding HTTP response headers because rendering can be streamed or prerendered before headers are finalized.
- Calling `cookieStore.set()` or `cookieStore.delete()` inside a Server Component throws:
  `Error: Cookies can only be modified in a Server Action or Route Handler.`
- **Remedy**: Refresh cookies in Middleware / `proxy.ts`, Route Handlers, or Server Actions.

---

## 4. Architecture & System Design Scenario

**Scenario**: Designing a multi-tenant enterprise RBAC system where users belong to organizations with roles (`Admin`, `Editor`, `Viewer`).
**Requirements**:
1. Zero permission leaks across tenants.
2. Fast page loads with cached layouts.
3. Instant revocation of compromised session tokens.

**Design**:
1. **Cookie Structure**: `session_token` contains a cryptographically signed UUID pointing to a stateful Redis session record with an invalidation epoch.
2. **Revocation**: Password changes or admin revocations increment the user's invalidation epoch in Redis; all active tokens fail validation within $< 1$s.
3. **Tenant Scoping in DAL**: Every database query in the DAL must require `orgId` as an explicit parameter derived directly from the verified session:
   `db.select().from(documents).where(and(eq(documents.orgId, session.orgId), eq(documents.id, docId)))`.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Protects routes using `middleware.ts` matchers and redirects. |
| **Senior** | Implements DAL pattern with `React.cache()`, explains why middleware alone is insufficient, and secures Server Actions against IDOR. |
| **Staff / Principal** | Analyzes subrequest header vulnerabilities (CVE-2025-29927), designs cross-runtime token refresh rotation, and establishes zero-trust multi-tenant isolation. |
