# Next.js Facts & Authoritative Architecture Baseline (Phase 0)

> **Document Purpose**: Source of truth for Next.js curriculum engineering, facts, verified APIs, CVEs, configuration flags, and breaking changes from Next.js 13 through modern Next.js 15/16.

---

## 1. Release Timeline, LTS & Node Versions

| Framework / Tool | Verified Version | Support / LTS Status | Node Minimum | Key Significance |
| :--- | :--- | :--- | :--- | :--- |
| **Next.js** | 15.2.3+ / 16.0 canary/stable line | Current Stable Generation | Node.js 18.18+ (20+ LTS recommended) | Async request APIs, `"use cache"`, `after()`, `proxy.ts`, `next/form` |
| **React** | React 19.0.0 (GA) | Modern Baseline | N/A | Actions, `useActionState`, `useOptimistic`, `use()`, React Compiler |
| **TypeScript** | 5.7+ | Current Stable | N/A | Strict null checks, typed routes |
| **Tailwind CSS** | v4.0+ | CSS-First Engine | N/A | `@theme`, `@import "tailwindcss"`, no `tailwind.config.js` |
| **Turbopack** | Default bundler in `next dev` | Stable in dev | N/A | Fast HMR, `--turbopack` by default in dev |

---

## 2. Critical Breaking Changes & Architectural Evolutions

### 2.1 Asynchronous Request APIs (Next.js 15+)
In Next.js 15+, request-bound APIs are **asynchronous Promises**:
- `const cookieStore = await cookies()`
- `const headerList = await headers()`
- `const { id } = await params` (in Page, Layout, and Route Handler props)
- `const { q } = await searchParams` (in Page props)
- In Client Components, `params` and `searchParams` passed as props are unwrapped using React 19 `use(params)`.
- **Codemod**: `npx @next/codemod@canary next-async-request-api`

### 2.2 Next-Gen Caching Model: `"use cache"` & Cache Components
- **Dynamic by Default**: The older implicit caching of `fetch()` has been inverted: `fetch()` requests default to `no-store` (uncached).
- **`"use cache"` Directive**: File, component, or function-level directive opting into caching.
- **`cacheLife(profile)`**: Defines freshness profiles (`seconds`, `minutes`, `hours`, `days`, `max`).
- **`cacheTag(tag)`**: Attaches invalidation tags.
- **`updateTag(tag)`**: Server Action function for immediate cache expiry and instant UI update (read-your-own-writes).
- **`revalidateTag(tag, profile)`**: Two-argument signature for background SWR revalidation (e.g. `revalidateTag('posts', 'max')`).

### 2.3 Middleware Deprecation & `proxy.ts` (Next.js 16)
- The legacy `middleware.ts` file convention is deprecated/renamed to **`proxy.ts`**.
- Export function name changes from `export function middleware()` to `export function proxy()`.
- Clarifies its architectural purpose as a lightweight edge network interceptor rather than an Express-style business logic layer.
- **Codemod**: `npx @next/codemod@canary middleware-to-proxy`

### 2.4 Removal of `next lint`
- `next lint` is removed in Next.js 16 (deprecated in 15.5).
- `next build` no longer executes ESLint automatically.
- Developers use standard ESLint CLI (`eslint .`) with ESLint 9 Flat Config (`eslint.config.mjs`) using `eslint-config-next/core-web-vitals` or modern Biome.

### 2.5 New Built-in Navigation & Form Components
- **`<Form>` (`next/form`)**: Extends HTML `<form>`.
  - If `action="/search"` (string URL): executes GET with query params, automatic prefetching of layout/loading state, and client-side navigation.
  - If `action={serverAction}` (function): executes Server Action mutation.
- **`after()` (`next/server`)**: Schedules non-blocking background tasks (logging, analytics, queue dispatches) after the response stream has completed.
- **`unauthorized()` & `forbidden()` (`next/navigation`)**: Interrupt functions throwing 401 and 403 HTTP statuses with dedicated UI files (`unauthorized.tsx`, `forbidden.tsx`). Requires `experimental.authInterrupts = true` in config.

---

## 3. Verified Security Advisories & CVEs

### 3.1 CVE-2025-29927: Middleware Authorization Bypass
- **CVSS Score**: 9.1 (Critical)
- **Affected Versions**: Next.js 11.1.4 through 13.5.6, 14.x < 14.2.25, 15.x < 15.2.3.
- **Mechanism**: The framework mistakenly trusted the internal header `x-middleware-subrequest` sent by external HTTP clients, tricking the server into treating the request as an internal subrequest and skipping the entire middleware layer.
- **Mitigation**: Upgrade to `14.2.25` / `15.2.3+`, or strip `x-middleware-subrequest` at external reverse proxies/WAFs.
- **Architectural Lesson**: **Defence-in-Depth**. Never rely solely on middleware/proxy for authorization; always authorize at the Data Access Layer (DAL), Server Action body, and Route Handler!

### 3.2 React Server Components Protocol & Deserialization Issues
- **CVE-2025-55184 / CVE-2025-67779**: Denial of Service via malformed deserialization payload causing infinite loops in server parser.
- **CVE-2025-55183**: Source code/secrets leakage via crafted RSC requests.
- **Mitigation**: Patch to React 19.0.0+ / Next.js 15.2.3+. Use `"server-only"` and React Taint APIs (`experimental_taintObjectReference`, `experimental_taintUniqueValue`).

---

## 4. Ecosystem & Integration Baselines

- **Auth.js (v5) / Better Auth / Clerk**:
  - Auth.js v5: uses standard Web Request/Response, works with edge and node runtimes.
  - Better Auth: TypeScript-first, native Passkey, 2FA, session-in-database support.
  - Clerk: Hosted enterprise identity with App Router components (`<ClerkProvider>`, `auth()`).
- **Databases**: Prisma (v6) with client acceleration, Drizzle ORM (v0.38+) with serverless connection pooling, Postgres (Neon/Supabase), Redis (Upstash/ioredis).
- **Background Jobs**: BullMQ on Redis for asynchronous queues triggered from Server Actions (`after()` or direct queue dispatch).
- **AI**: Vercel AI SDK Core (`ai`) + UI (`@ai-sdk/react`), streaming UI with RSC, tool calling with Zod.
