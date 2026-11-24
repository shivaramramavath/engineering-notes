# Next.js Engineering Curriculum

> **A Comprehensive, Production-Grade Engineering Track for Modern Next.js 15+ and 16.0**  
> Covers App Router architecture, React Server Components (RSC), Turbopack compilation engine, React 19 Actions, Partial Prerendering (PPR `◐`), the `"use cache"` directive, Data Access Layer (DAL) security, and enterprise fullstack system design.

---

## Track Overview

This track provides complete, foundational-to-expert coverage of modern Next.js application engineering. It emphasizes production architectures, rigorous TypeScript types, real browser/Vitest/Playwright verification, and elimination of outdated legacy patterns (such as Pages Router `getServerSideProps`, `next lint`, aggressive `fetch` caching assumptions, synchronous request parameter access, and reliance on edge middleware as the sole security boundary).

### Key Architectural Standards
- **Baseline Version**: **Next.js 15.2+ / Next.js 16.0 (Current Generation)**, React 19.0.0 (GA), Node.js 20+ LTS (minimum 18.18.0), TypeScript 5.7+, Tailwind CSS v4.
- **Modern Idioms**:
  - **Async Request APIs**: `await cookies()`, `await headers()`, `await params`, `await searchParams`.
  - **Cache Components Model**: The `"use cache"` directive, `cacheLife()`, `cacheTag()`, `updateTag(tag)` in Server Actions (read-your-own-writes), and `revalidateTag(tag, profile)` in Route Handlers (SWR).
  - **Partial Prerendering (PPR `◐`)**: Instant edge static shell delivery with concurrent dynamic Suspense streaming over a single HTTP stream.
  - **Security & DAL**: Zero-trust Data Access Layer (`import 'server-only'`) enforcing session and tenant isolation on every database query, mitigating CVE-2025-29927 (middleware subrequest bypass).
  - **React 19 Synergy**: Server Actions, `useActionState`, `useOptimistic`, and React Taint APIs (`taintObjectReference`, `taintUniqueValue`).
  - **Tooling**: Turbopack incremental computation engine, ESLint 9 Flat Config (`eslint.config.mjs`) / Biome, and `output: 'standalone'` Docker containerization.

---

## Complete Curriculum Structure (Modules 00–26)

| Module | Title | Focus & Core Capabilities |
| :---: | :--- | :--- |
| **[00-setup](./00-setup/README.md)** | Setup & Tooling | Turbopack engine, Next.js 15/16 upgrade paths, ESLint 9 Flat Config, tsconfig bundler resolution. |
| **[01-fundamentals](./01-fundamentals/README.md)** | Fundamentals & Runtime | App Router architecture, Node.js vs Edge runtimes, build output anatomy (`○`, `ƒ`, `◐`). |
| **[02-routing](./02-routing/README.md)** | Routing & Navigation | Dynamic segments, catch-all, route groups, parallel (`@slot`), intercepting routes, async params. |
| **[03-components](./03-components/README.md)** | Components & Boundaries | RSC wire protocol, `"use client"` boundaries, component composition via `children`, `server-only`. |
| **[04-rendering](./04-rendering/README.md)** | Rendering & PPR | Static, Dynamic, Suspense streaming, Partial Prerendering (PPR `◐`), segment configs. |
| **[05-data-fetching](./05-data-fetching/README.md)** | Data Fetching & DAL | Component-level fetching, Data Access Layer pattern, deduplication with `React.cache()`. |
| **[06-caching](./06-caching/README.md)** | Caching & Cache Components | Next.js 15 uncached `fetch` default, `"use cache"`, `cacheLife`, `cacheTag`, `updateTag`. |
| **[07-server-actions](./07-server-actions/README.md)** | Server Actions & Mutations | Form actions, `useActionState`, `useOptimistic`, cryptographic action manifests, CSRF security. |
| **[08-route-handlers-and-proxy](./08-route-handlers-and-proxy/README.md)** | Route Handlers & Proxy | `route.ts` REST APIs, SSE streaming, Webhook verification, `proxy.ts` / `middleware.ts`, CVE-2025-29927. |
| **[09-styling-and-assets](./09-styling-and-assets/README.md)** | Styling & Assets | Tailwind CSS v4, `next/font` zero-layout-shift, `next/image` optimization, static assets. |
| **[10-state-management](./10-state-management/README.md)** | State Management | URL state (`nuqs`, `useSearchParams`), Server $\leftrightarrow$ Client state hydration, Zustand v5 in App Router. |
| **[11-authentication](./11-authentication/README.md)** | Authentication & RBAC | Defense-in-depth, encrypted JWTs, stateful Redis sessions, Better-Auth vs Auth.js (v5). |
| **[12-database](./12-database/README.md)** | Database & ORMs | Drizzle ORM, Prisma v6, serverless connection pooling (PgBouncer, Neon), migrations. |
| **[13-typescript](./13-typescript/README.md)** | TypeScript Integration | Statically typed routes, typing async `params`/`searchParams`, typing Server Actions. |
| **[14-seo-and-metadata](./14-seo-and-metadata/README.md)** | SEO & Metadata | `generateMetadata`, OpenGraph, dynamic Twitter cards, JSON-LD schemas, sitemaps. |
| **[15-advanced-features](./15-advanced-features/README.md)** | Advanced Features | OpenTelemetry distributed tracing, `instrumentation.ts`, Draft Mode, `after()` background tasks. |
| **[16-integrations](./16-integrations/README.md)** | Enterprise Integrations | Stripe subscriptions & webhooks, AWS S3 pre-signed uploads, Resend transactional emails. |
| **[17-ai](./17-ai/README.md)** | AI Engineering | Vercel AI SDK Core & UI, streaming chat, generative UI, vector search (pgvector). |
| **[18-testing](./18-testing/README.md)** | Testing & Quality | Testing Server Components with Vitest 3, mocking DAL, Playwright 1.49 E2E testing. |
| **[19-debugging](./19-debugging/README.md)** | Debugging & Diagnostics | Troubleshooting hydration mismatches, debugging caching staleness, profiling streaming TTFB. |
| **[20-performance](./20-performance/README.md)** | Performance & Profiling | Core Web Vitals (LCP, INP, CLS), bundle analysis, RSC flight payload trimming. |
| **[21-security](./21-security/README.md)** | Security Architecture | Threat modeling, React Taint APIs, CSP nonces, CVE-2025-29927 and CVE-2025-55184 defense. |
| **[22-production](./22-production/README.md)** | Production Engineering | Standalone Docker output, self-hosting on Kubernetes, OpenNext on AWS, healthchecks. |
| **[23-architecture](./23-architecture/README.md)** | Enterprise Architecture | Clean Architecture, Feature-Sliced Design, multi-tenancy (custom domains), Multi-Zones. |
| **[24-projects](./24-projects/README.md)** | Reference Projects | 14 complete production reference projects (SaaS, E-commerce, RAG, BullMQ, Offline Sync). |
| **[25-interview](./25-interview/README.md)** | Interview Preparation | Senior & Staff interview guides, system design scenarios, tricky output predictions. |
| **[26-cheatsheets](./26-cheatsheets/README.md)** | Cheatsheets & Reference | 11 high-density syntax and architecture cheat sheets. |

---

## Authoring & Quality Constraints
- **Markdown-Only**: All notes, code examples, and architectural diagrams are self-contained pure Markdown (`.md`).
- **Production-Grade**: Every snippet compiles against Next.js 15.2+ and React 19 without placeholder ellipses or broken imports.
- **Defense in Depth**: Zero tolerance for relying on client-side security or edge proxy checks alone.
