# Senior & Staff Interview Guide: Next.js Enterprise Architecture and System Boundaries

> Comprehensive technical interview mastery guide covering Clean Architecture in Next.js, the Data Access Layer (DAL) pattern, Feature-Sliced Design, Multi-Tenant Routing, and Multi-Zones micro-frontends.

| Metadata | Details |
| :--- | :--- |
| **Category** | Technical Interview Preparation |
| **Target Level** | Senior / Staff Software Engineer |
| **Prerequisites** | [23-architecture/README.md](../23-architecture/README.md), [02-routing/README.md](../02-routing/README.md) |
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

Interviewers evaluating senior candidates on Next.js enterprise architecture focus on:
- **Clean Architecture & Separation of Concerns**: Isolating the presentation layer (RSC/Client components) from business logic (services/use cases) and persistence (DAL/ORMs).
- **The Data Access Layer (DAL)**: Establishing a centralized, server-only data access boundary that enforces authentication, tenant filtering, and authorization for every query.
- **Feature-Based Folder Structure (Colocation)**: Organizing code by business domain (`features/billing/`, `features/analytics/`) rather than technical type (`components/`, `hooks/`).
- **Multi-Tenant Routing (Dynamic Hostname Routing)**: Handling wildcards (`*.platform.com`) and custom domains (`custom.com`) via middleware rewrites into `app/[tenant]/`.
- **Multi-Zones Architecture**: Splitting monolithic applications into independently deployed Next.js apps unified under a single user-facing domain.

---

## 2. Deep-Dive Interview Questions & Answers

<details>
<summary>Q1: How do you structure a Data Access Layer (DAL) in Next.js to ensure zero business logic or queries leak into components?</summary>

**Answer**:
A proper DAL isolates all database operations and authorization checks behind strongly-typed, memoized functions:

```
src/
├── app/                  # Pure routing & presentation orchestration
├── features/
│   └── billing/
│       ├── components/   # UI components (RSC and Client)
│       ├── actions.ts    # Server Actions (entry points with Zod validation)
│       └── dal/
│           ├── queries.ts # Reads (server-only, verifySession, React.cache)
│           └── mutations.ts # Writes (server-only, authorization, transactions)
└── lib/
    ├── dal.ts            # Central session verification & tenant scoping
    └── db.ts             # Database client connection pool
```

**DAL Rules**:
1. Every file in `dal/` includes `import 'server-only'`.
2. Reads are wrapped in `React.cache()` for per-request memoization.
3. Every query enforces tenant isolation (`where(eq(table.tenantId, session.tenantId))`).
4. Components never import the database driver (`db`) directly; they only consume DAL query functions.
</details>

<details>
<summary>Q2: What is Multi-Zones and when should an enterprise choose it over a single Next.js monolith?</summary>

**Answer**:
**Multi-Zones** is a feature in Next.js that merges multiple independently built and deployed Next.js applications under a single routing domain.
- **How it works**: A primary gateway app defines `rewrites()` in `next.config.ts`:
```typescript
async rewrites() {
  return [
    {
      source: '/blog/:path*',
      destination: 'https://blog-zone.internal.company.com/blog/:path*',
    },
  ];
}
```
- **When to Choose Multi-Zones**:
  - Independent engineering teams need to deploy features at different tempos without coordination.
  - Reducing CI build times for massive codebases (e.g. 10,000+ pages).
  - Isolating failure domains: a crash in the marketing blog will not bring down checkout or account management.
</details>

<details>
<summary>Q3: Explain how dynamic multi-tenancy (subdomains + custom domains) is implemented in Next.js App Router.</summary>

**Answer**:
1. **Host Extraction**: In `middleware.ts` (or `proxy.ts`), inspect the incoming `Host` header:
```typescript
export function middleware(request: NextRequest) {
  const host = request.headers.get('host') || '';
  const subdomain = host.split('.')[0]; // e.g., 'acme' from 'acme.platform.com'

  // Rewrite internal URL to match app/[tenant] directory
  return NextResponse.rewrite(
    new URL(`/${subdomain}${request.nextUrl.pathname}`, request.url)
  );
}
```
2. **Directory Structure**:
   `app/[tenant]/page.tsx`, `app/[tenant]/dashboard/page.tsx`
3. **Tenant Parameter Resolution**: The Server Component page unwraps `const { tenant } = await params;` and queries tenant-specific theme, settings, and data from the DAL.
</details>

<details>
<summary>Q4: How do you handle shared global state (like a shopping cart or user preferences) across Server and Client Components without causing hydration mismatch?</summary>

**Answer**:
1. **Persistent State in Cookies/URL**: Store authoritative state in cookies or URL search parameters.
2. **Server-Side Initialization**: Read the cookie/URL in a Server Component (e.g. root layout or page) and pass the initial state as a prop to a Client Context Provider (`CartProvider`).
3. **Client Hydration Sync**: The Client Provider initializes its React state using the server-provided initial value, guaranteeing that server-rendered HTML and initial client render match identically.
4. **Optimistic Updates**: Client interactions update the local client state optimistically while dispatching a Server Action or Route Handler to sync the cookie/database asynchronously.
</details>

---

## 3. Output Prediction & Code Analysis Questions

### Question 1: Circular Routing Loop in Middleware
Identify the fatal bug in this multi-tenant middleware:

```typescript
// middleware.ts
import { NextResponse, type NextRequest } from 'next/server';

export function middleware(request: NextRequest) {
  const url = request.nextUrl;
  const hostname = request.headers.get('host')!;

  // Infinite Rewrite Loop!
  return NextResponse.rewrite(new URL(`/tenant-portal${url.pathname}`, request.url));
}
```

**Evaluation**:
- **Infinite Internal Rewrite Loop**: Every rewritten request will re-trigger the middleware matching `/tenant-portal...`, causing Next.js to rewrite again until reaching maximum recursion limit.
- **Fix**: Check if the pathname already starts with `/tenant-portal` or matches static asset paths before rewriting:
```typescript
if (url.pathname.startsWith('/tenant-portal') || url.pathname.startsWith('/_next')) {
  return NextResponse.next();
}
```

---

## 4. Architecture & System Design Scenario

**Scenario**: Designing an enterprise B2B SaaS platform that supports:
- 1,000+ business tenants with custom domains (`app.client.com`).
- High-velocity feature development across 4 autonomous teams.
- Rigorous security compliance (SOC 2, ISO 27001).

**System Architecture**:
1. **Ingress**: Cloudflare for edge SSL termination and DNS routing.
2. **Application Layer**: Next.js App Router deployed with standalone output on Kubernetes, using Multi-Zones for the Marketing Site, Docs, and Core App.
3. **Security Architecture**:
   - `proxy.ts` performs host header parsing and tenant slug rewriting.
   - Centralized DAL enforces tenant isolation on every SQL query via Row-Level Security (RLS) or Drizzle tenant predicates.
   - OpenTelemetry instrumentation for audit tracing.

---

## 5. Senior Technical Evaluation Rubric

| Level | Candidate Signals |
| :--- | :--- |
| **Mid-Level** | Organizes code by folders (`components`, `pages`). Uses context for global state. |
| **Senior** | Implements DAL patterns with `server-only`, designs multi-tenant middleware rewrites, and prevents hydration mismatches. |
| **Staff / Principal** | Architects enterprise multi-zones routing topologies, designs zero-downtime micro-frontend deployments, and enforces formal domain boundaries. |
