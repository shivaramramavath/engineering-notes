# Next.js Security Vulnerabilities & Advisories (`server-actions-and-rsc-security-advisories`)

> Comprehensive security analysis of Next.js: In-depth audit of CVE-2025-29927 (Middleware Bypass), CVE-2025-55184 (RSC Protocol DoS), Content Security Policy (CSP) with nonces, React Taint APIs, and OWASP Top 10 mitigations.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Security** | CVE Mitigations, CSP, Nonces, Taint APIs, OWASP |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Shift in Next.js Security Architecture
Next.js applications combine client, edge, and server execution environments. Security vulnerabilities often emerge at the seams between these layers:
1. **Network Boundary Exploits (CVE-2025-29927)**: Trusting internal routing headers from external callers.
2. **Protocol Deserialization Bugs (CVE-2025-55184 / CVE-2025-67779)**: Infinite loops or memory exhaustion during React Flight protocol deserialization.
3. **Data Leakage across Boundaries**: Sensitive server records or cryptographic keys inadvertently serialized into client bundles.

```
[ External Untrusted Request ]
             │
             ├── 1. Reverse Proxy WAF (Strips x-middleware-subrequest)
             ├── 2. proxy.ts (CSP Nonce Injection & Edge Routing)
             ├── 3. Data Access Layer (verifySession() + 'server-only')
             ├── 4. React Taint API (Blocks serialization of secret references)
             └── 5. Zod Schema Validation (Enforces input boundaries)
```

---

## 2. In-Depth CVE Breakdown

### CVE-2025-29927: Middleware Authorization Bypass
- **CVSS Score**: 9.1 (Critical)
- **Root Cause**: Next.js tracked internal sub-requests using `x-middleware-subrequest`. External attackers could spoof this header (`curl -H "x-middleware-subrequest: 1" https://app/admin`), causing the server to skip the middleware layer entirely.
- **Fixed Versions**: Next.js 15.2.3+, 14.2.25+, 13.5.9+.
- **Architectural Lesson**: **Defence-in-Depth**. Never rely on middleware as the sole gatekeeper. Authorization must occur at the Data Access Layer and Server Action level.

### CVE-2025-55184 & CVE-2025-67779: RSC Protocol Deserialization Denial of Service
- **Root Cause**: Malformed payloads sent to App Router endpoints could trigger infinite recursion in the server-side React Flight parser, exhausting CPU cycles and crashing the server process.
- **Fixed Versions**: Upstream React 19.0.0+ / Next.js 15.2.3+.
- **Mitigation**: Keep dependencies patched and configure WAF body size limits (<1MB for standard JSON endpoints).

---

## 3. Five Progressive Examples

### Example 1: Implementing Dynamic CSP with Cryptographic Nonces in `proxy.ts`
```ts
// proxy.ts (or middleware.ts)
import { NextResponse } from 'next/server';
import type { NextRequest } from 'next/server';

export function proxy(request: NextRequest) {
  // Generate random 128-bit cryptographic nonce
  const nonce = Buffer.from(crypto.randomUUID()).toString('base64');

  // Strip vulnerable internal headers to mitigate CVE-2025-29927
  const requestHeaders = new Headers(request.headers);
  requestHeaders.delete('x-middleware-subrequest');
  requestHeaders.set('x-nonce', nonce);

  const cspHeader = `
    default-src 'self';
    script-src 'self' 'nonce-${nonce}' 'strict-dynamic';
    style-src 'self' 'nonce-${nonce}';
    img-src 'self' blob: data: https://images.unsplash.com;
    font-src 'self';
    object-src 'none';
    base-uri 'self';
    form-action 'self';
    frame-ancestors 'none';
    upgrade-insecure-requests;
  `.replace(/\s{2,}/g, ' ').trim();

  requestHeaders.set('Content-Security-Policy', cspHeader);

  const response = NextResponse.next({
    request: { headers: requestHeaders },
  });

  response.headers.set('Content-Security-Policy', cspHeader);
  return response;
}
```

### Example 2: Consuming the Nonce in Root Layout
```tsx
// app/layout.tsx
import { headers } from 'next/headers';
import Script from 'next/script';

export default async function RootLayout({ children }: { children: React.ReactNode }) {
  const headerList = await headers();
  const nonce = headerList.get('x-nonce') ?? undefined;

  return (
    <html lang="en">
      <head>
        <Script
          src="https://trusted.analytics.com/script.js"
          strategy="afterInteractive"
          nonce={nonce}
        />
      </head>
      <body>{children}</body>
    </html>
  );
}
```

### Example 3: Enabling React Taint Protection
```ts
// next.config.ts
import type { NextConfig } from 'next';

const nextConfig: NextConfig = {
  experimental: {
    taint: true, // Enables React 19 Taint APIs
  },
};

export default nextConfig;
```
```ts
// dal/apiKeyService.ts
import 'server-only';
import { experimental_taintUniqueValue, experimental_taintObjectReference } from 'react';
import { db } from '@/db';

export async function getStripeKey() {
  const secretKey = process.env.STRIPE_SECRET_KEY!;
  // Throws runtime error if this string is ever serialized to client!
  experimental_taintUniqueValue('Never leak Stripe Secret Key to client', secretKey);
  return secretKey;
}
```

### Example 4: Enforcing Server-Only Boundary
```ts
// dal/userAdmin.ts
import 'server-only'; // Throws compile error if imported by any Client Component

export async function purgeUserData(userId: string) {
  return db.user.delete({ where: { id: userId } });
}
```

### Example 5: Strict Security Headers Configuration
```ts
// next.config.ts
const securityHeaders = [
  { key: 'X-DNS-Prefetch-Control', value: 'on' },
  { key: 'Strict-Transport-Security', value: 'max-age=63072000; includeSubDomains; preload' },
  { key: 'X-Frame-Options', value: 'DENY' },
  { key: 'X-Content-Type-Options', value: 'nosniff' },
  { key: 'Referrer-Policy', value: 'origin-when-cross-origin' },
];

export default {
  async headers() {
    return [{ source: '/(.*)', headers: securityHeaders }];
  },
};
```

---

## 4. Under the Hood & Internals
The React Taint API operates directly within the React Server Components serialization engine.
- When an object reference or primitive string is registered via `experimental_taintObjectReference` or `experimental_taintUniqueValue`, React stores the reference or hash in an internal Set on the active Fiber dispatcher.
- During the React Flight serialization pass, every value crossing the server-to-client boundary is checked against this Set. If a match is detected, React immediately aborts serialization and throws an exception, preventing the secret from ever leaving the server process.

---

## 5. OWASP Top 10 for Next.js

| OWASP Vulnerability | Next.js Attack Vector | Authoritative Mitigation |
| :--- | :--- | :--- |
| **A01: Broken Access Control** | CVE-2025-29927 middleware bypass | Verify session in DAL and inside every Server Action |
| **A02: Cryptographic Failures** | Storing JWTs in `localStorage` | Use `httpOnly`, `secure`, `SameSite=Lax` cookies |
| **A03: Injection** | SQL injection via raw query strings | Parameterized queries via Prisma/Drizzle ORM |
| **A04: Insecure Design** | Unprotected Server Action RPC hashes | Input validation via Zod; rate limiting via Redis |
| **A05: Security Misconfiguration** | Permissive remote image domains | Declare explicit `remotePatterns` in `next.config.ts` |
| **A07: Identification & Auth** | Missing CSRF check in Actions | Next.js built-in Origin vs Host validation |
| **A08: Software & Data Integrity**| Supply chain vulnerabilities | Automated lockfile audits (`npm audit`) and patching |

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| `dangerouslySetInnerHTML` | Raw HTML injection | Stored Cross-Site Scripting (XSS) | Sanitize with DOMPurify or avoid raw HTML |
| Wildcard CSP `script-src *` | Permissive policy | XSS scripts execute unimpeded | Use strict nonces (`'nonce-${nonce}'`) |
| Unpatched Next.js version | Ignoring security advisories | Vulnerable to CVE-2025-29927 | Upgrade to Next.js >= 15.2.3 |
| Un-tainted API keys | Accidental prop passing | Secrets visible in client `.rsc` payload | Mark keys with `experimental_taintUniqueValue` |

---

## 7. Performance & Optimization Guide
Generating a cryptographic nonce in `proxy.ts` takes less than **0.2ms** using native Web Crypto `crypto.randomUUID()`. A strict Content Security Policy adds negligible network overhead (<1 KB) while neutralizing 99% of reflected and stored XSS vectors.

---

## 8. TypeScript & Strict Typing Recipes
```ts
export interface SecureSessionPayload {
  userId: string;
  role: 'ADMIN' | 'USER';
  exp: number;
}
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect } from 'vitest';
import { proxy } from './proxy';
import { NextRequest } from 'next/server';

describe('Security Proxy CSP', () => {
  it('generates nonce and injects CSP headers', () => {
    const req = new NextRequest('http://localhost:3000/');
    const res = proxy(req);
    expect(res.headers.get('Content-Security-Policy')).toContain('nonce-');
  });
});
```

---

## 10. When NOT to Use This Pattern
- Do not apply strict script nonces to static export SPAs (`output: 'export'`) that lack an active server runtime, as nonces must be uniquely generated per request.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. How was CVE-2025-29927 exploited in Next.js?</summary>
Attackers injected the internal `x-middleware-subrequest` header into HTTP requests, tricking Next.js into believing the request was an internal sub-request, which caused it to skip the entire middleware layer.
</details>

<details>
<summary>2. What is the React Taint API?</summary>
An experimental React 19 security feature (`experimental_taintObjectReference` and `experimental_taintUniqueValue`) that throws an error if sensitive objects or secret values are accidentally passed across the server-to-client Flight boundary.
</details>

<details>
<summary>3. Why is CSP with cryptographic nonces superior to `'unsafe-inline'`?</summary>
Nonces ensure that only scripts possessing the exact random cryptographic token generated for that specific HTTP response can execute, rendering injected attacker scripts harmless.
</details>

<details>
<summary>4. How does `import 'server-only'` enhance security?</summary>
It throws a compilation error at build time if a server module containing database connections or private keys is ever imported by a Client Component.
</details>

<details>
<summary>5. How does Next.js protect Server Actions against CSRF attacks?</summary>
Next.js compares the incoming request's `Origin` header with the `Host` header. If they do not match, the request is rejected with a 403 Forbidden.
</details>

<details>
<summary>6. Why is trusting middleware alone an architectural anti-pattern?</summary>
Because middleware can be bypassed via header injection, routing matcher exclusions, or misconfigured rewrites. Defence-in-depth requires checking authorization in the Data Access Layer and Server Actions.
</details>

<details>
<summary>7. What is CVE-2025-55184?</summary>
A denial-of-service vulnerability in the React Server Components Flight protocol parser where malformed payloads triggered infinite loops during server deserialization.
</details>

<details>
<summary>8. How do you prevent SSRF attacks in Next.js image optimization?</summary>
Declare strict `remotePatterns` in `next.config.ts` specifying allowed protocols, hostnames, and ports, rather than allowing arbitrary remote domains.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Inject security headers</summary>
Configure `X-Frame-Options` and `Strict-Transport-Security` in `next.config.ts`.
</details>

<details>
<summary>Exercise 2 (Medium): Implement dynamic CSP with nonces</summary>
Create a `proxy.ts` file that generates a cryptographic nonce and injects it into CSP headers and `app/layout.tsx`.
</details>

<details>
<summary>Exercise 3 (Hard): Configure React Taint protection</summary>
Enable `experimental.taint = true` and protect database credentials from client serialization.
</details>

---

## 13. Cross-References & Further Reading
- [Proxy Architecture & CVE-2025-29927](file:///E:/Learning/engineering-notes/02-frontend/nextjs/08-route-handlers-and-proxy/05-proxy-patterns-and-cve-2025-29927.md)
- [Official Next.js Security Best Practices](https://nextjs.org/docs/app/building-your-application/configuring/content-security-policy)
