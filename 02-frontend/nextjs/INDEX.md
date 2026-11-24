# Next.js Curriculum Index

> Complete directory of all modules, concept notes, reference projects, technical interview guides, and engineering cheatsheets across the `02-frontend/nextjs` track.

---

## Foundation & Setup
- **[00-setup](file:///E:/Learning/engineering-notes/02-frontend/nextjs/00-setup/README.md)**: Toolchain, create-next-app, TypeScript, Turbopack, Biome vs ESLint 9, Next.js DevTools & MCP tooling.
- **[01-fundamentals](file:///E:/Learning/engineering-notes/02-frontend/nextjs/01-fundamentals/README.md)**: Build output anatomy, route table symbols (`○`, `ƒ`, `◐`), request lifecycle end-to-end, Pages-to-App Router migration.

## Routing, Components & Rendering
- **[02-routing](file:///E:/Learning/engineering-notes/02-frontend/nextjs/02-routing/README.md)**: Dynamic routes, route groups, parallel routes `default.js`, intercepting routes, typed routes, `unauthorized()` & `forbidden()` auth interrupts.
- **[03-components](file:///E:/Learning/engineering-notes/02-frontend/nextjs/03-components/README.md)**: Server Components (RSC), Client Components (`'use client'`), Flight protocol serialization boundaries, `"server-only"` DAL.
- **[04-rendering](file:///E:/Learning/engineering-notes/02-frontend/nextjs/04-rendering/README.md)**: Partial Prerendering (PPR), static export SPA mode, Edge vs Node.js runtime, streaming UI patterns.

## Data, Caching & Actions
- **[05-data-fetching](file:///E:/Learning/engineering-notes/02-frontend/nextjs/05-data-fetching/README.md)**: Server-side fetching, request memoization, preload patterns, Data Access Layer with `server-only`, `nuqs` URL state.
- **[06-caching](file:///E:/Learning/engineering-notes/02-frontend/nextjs/06-caching/README.md)**: Modern `"use cache"` directive, `cacheLife`, `cacheTag`, `updateTag` vs `revalidateTag(tag, profile)`, custom Redis cache handlers.
- **[07-server-actions](file:///E:/Learning/engineering-notes/02-frontend/nextjs/07-server-actions/README.md)**: Server Actions security, `useActionState`, file uploads, `after()` deferred work, BullMQ background queues.

## Network, Assets & State
- **[08-route-handlers-and-proxy](file:///E:/Learning/engineering-notes/02-frontend/nextjs/08-route-handlers-and-proxy/README.md)**: Route Handlers, `proxy.ts` rename, CVE-2025-29927 mitigation, streaming SSE, signed webhooks.
- **[09-styling-and-assets](file:///E:/Learning/engineering-notes/02-frontend/nextjs/09-styling-and-assets/README.md)**: Tailwind CSS v4 CSS-first `@theme`, `@vercel/og` dynamic social images, font optimization, layout shifts.
- **[10-state-management](file:///E:/Learning/engineering-notes/02-frontend/nextjs/10-state-management/README.md)**: URL state as single source of truth, SSR-safe Zustand, `useOptimistic`.

## Auth, Databases & TypeScript
- **[11-authentication](file:///E:/Learning/engineering-notes/02-frontend/nextjs/11-authentication/README.md)**: Auth.js v5, Better Auth, Clerk, defense-in-depth authorization, Passkeys.
- **[12-database](file:///E:/Learning/engineering-notes/02-frontend/nextjs/12-database/README.md)**: Serverless connection pooling, Prisma v6 vs Drizzle ORM, Row-Level Security (RLS), Upstash Redis.
- **[13-typescript](file:///E:/Learning/engineering-notes/02-frontend/nextjs/13-typescript/README.md)**: Typed routes, asynchronous params/searchParams, Zod validation, shared monorepo types.

## SEO, Advanced Features & Integrations
- **[14-seo-and-metadata](file:///E:/Learning/engineering-notes/02-frontend/nextjs/14-seo-and-metadata/README.md)**: Dynamic `generateMetadata`, JSON-LD schemas, sitemaps, Core Web Vitals.
- **[15-advanced-features](file:///E:/Learning/engineering-notes/02-frontend/nextjs/15-advanced-features/README.md)**: `next-intl` internationalization, OpenTelemetry instrumentation, Resend + React Email, S3/UploadThing.
- **[16-integrations](file:///E:/Learning/engineering-notes/02-frontend/nextjs/16-integrations/README.md)**: Stripe subscriptions & webhook raw body verification, typed BFF clients.

## AI, Quality & Observability
- **[17-ai](file:///E:/Learning/engineering-notes/02-frontend/nextjs/17-ai/README.md)**: Vercel AI SDK Core & UI, streaming UI with RSC, MCP servers in Next.js, Redis token bucket rate limiting.
- **[18-testing](file:///E:/Learning/engineering-notes/02-frontend/nextjs/18-testing/README.md)**: Testing async Server Components, Server Actions, MSW v2, Playwright E2E.
- **[19-debugging](file:///E:/Learning/engineering-notes/02-frontend/nextjs/19-debugging/README.md)**: Caching debugging, streaming suspense issues, source maps, production OpenTelemetry traces.

## Enterprise Architecture & Production
- **[20-performance](file:///E:/Learning/engineering-notes/02-frontend/nextjs/20-performance/README.md)**: RSC payload analyzer, waterfall elimination, React Compiler in Next.js, load testing.
- **[21-security](file:///E:/Learning/engineering-notes/02-frontend/nextjs/21-security/README.md)**: CVE-2025-29927 analysis, React Taint APIs, CSP with nonces, OWASP Top 10 for Next.js.
- **[22-production](file:///E:/Learning/engineering-notes/02-frontend/nextjs/22-production/README.md)**: Standalone Docker output, zero-downtime deployment, skew protection.
- **[23-architecture](file:///E:/Learning/engineering-notes/02-frontend/nextjs/23-architecture/README.md)**: Feature-Sliced Design (FSD), Data Access Layer boundaries, hexagonal architecture.

## Projects, Interviews & Cheatsheets
- **[24-projects](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/README.md)**: 14 full production-grade reference applications with Docker Compose, schemas, and tests.
- **[25-interview](file:///E:/Learning/engineering-notes/02-frontend/nextjs/25-interview/README.md)**: Staff-level technical interview questions, system design case studies, and machine coding rounds.
- **[26-cheatsheets](file:///E:/Learning/engineering-notes/02-frontend/nextjs/26-cheatsheets/README.md)**: Quick-reference cards for caching, metadata, testing, security, and configuration.
