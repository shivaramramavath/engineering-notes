# Master Completion Plan — Next.js Track (`02-frontend/nextjs`)

> **Roadmap & Batch Schedule**: Sequential, dependency-ordered batches covering curriculum fixes, core expansions, reference projects, interview prep, and cheatsheets.

---

## 1. Batch Roadmap Overview

- **Batch 0: Structural Foundations & Root Documentation**
  - Create `INDEX.md`, `GLOSSARY.md`, update `README.md`, `ROADMAP.md`, `CONTRIBUTING.md`.
  - Validate `_templates/concept.md` and `_templates/project-template.md`.

- **Batch 1: Toolchain & Fundamentals (Modules 00 & 01)**
  - Module 00 (Setup): Upgrade guide (13 -> 15/16), Turbopack vs Webpack, Biome vs ESLint 9 Flat Config, Next.js DevTools & MCP.
  - Module 01 (Fundamentals): Build output anatomy & route table symbols (`○`, `ƒ`, `◐` PPR), request lifecycle end-to-end, Pages-to-App Router migration.

- **Batch 2: Routing, Components & Rendering (Modules 02, 03 & 04)**
  - Module 02 (Routing): Typed routes, `unauthorized()`/`forbidden()` auth interrupts, parallel routes `default.js`, link prefetching.
  - Module 03 (Components): `"server-only"` boundaries, Flight protocol serialization, third-party component wrappers.
  - Module 04 (Rendering): Partial Prerendering (PPR), static export SPA mode, Edge vs Node.js runtime, draft mode.

- **Batch 3: Data Fetching, Modern Caching & Server Actions (Modules 05, 06 & 07)**
  - Module 05 (Data Fetching): Expand to 8 notes (preload pattern, DAL with `server-only`, `nuqs` URL state, external API resiliency).
  - Module 06 (Caching): Modern `"use cache"` directive, `cacheLife`, `cacheTag`, `updateTag` vs `revalidateTag(tag, profile)`, Redis cache handler.
  - Module 07 (Server Actions): Security model, `after()` deferred work, BullMQ background queues, `useActionState` integration.

- **Batch 4: Route Handlers, Proxy, Styling & State (Modules 08, 09 & 10)**
  - Module 08 (Route Handlers & Proxy): `proxy.ts` rename, CVE-2025-29927 mitigation, streaming SSE, signed webhooks.
  - Module 09 (Styling & Assets): Tailwind CSS v4 `@theme`, `@vercel/og` dynamic social images, font optimization, layout shifts.
  - Module 10 (State Management): URL state as single source of truth, SSR-safe Zustand, `useOptimistic`.

- **Batch 5: Auth, Database & TypeScript (Modules 11, 12 & 13)**
  - Module 11 (Authentication): Auth.js v5 vs Better Auth vs Clerk, defense-in-depth authorization, Passkeys.
  - Module 12 (Database): Serverless connection management, Prisma v6 vs Drizzle, Row-Level Security (RLS).
  - Module 13 (TypeScript): Expand to 7 notes (typed routes, Zod end-to-end validation, shared monorepo types, env typing).

- **Batch 6: SEO, Advanced Features & Integrations (Modules 14, 15 & 16)**
  - Module 14 (SEO & Metadata): Dynamic metadata, JSON-LD schemas, sitemaps.
  - Module 15 (Advanced Features): `next-intl` internationalization, OpenTelemetry instrumentation, Resend + React Email, S3/UploadThing.
  - Module 16 (Integrations): Stripe subscriptions & webhook signature verification, typed BFF clients.

- **Batch 7: AI Engineering, Testing & Debugging (Modules 17, 18 & 19)**
  - Module 17 (AI): Vercel AI SDK Core & UI, streaming UI with RSC, MCP servers in Next.js, Redis token bucket rate limiting.
  - Module 18 (Testing): Testing async Server Components, Server Actions, MSW v2, Playwright E2E.
  - Module 19 (Debugging): Expand to 7 notes (caching debugging, streaming suspense issues, source maps, production OpenTelemetry traces).

- **Batch 8: Performance, Security, Production & Architecture (Modules 20, 21, 22 & 23)**
  - Module 20 (Performance): Expand to 8 notes (RSC bundle analyzer, React Compiler in Next.js, load testing).
  - Module 21 (Security): In-depth CVE-2025-29927 analysis, React Taint APIs, CSP with nonces, OWASP Top 10.
  - Module 22 (Production): Standalone Docker output, zero-downtime deployment, skew protection.
  - Module 23 (Architecture): Feature-Sliced Design (FSD), Data Access Layer boundaries, hexagonal architecture.

- **Batch 9: Reference Projects (Module 24 — Part 1: Projects 01 - 07)**
  - `01-blog` (MDX, ISR, dynamic SEO)
  - `02-authentication` (Better Auth / Auth.js, Passkeys, session store)
  - `03-ecommerce` (Product catalog, cart, Stripe checkout, inventory transactions)
  - `04-admin-dashboard` (TanStack Table, RBAC, metric charts)
  - `05-saas-application` (Multi-tenant subdomains, Stripe subscriptions, team roles)
  - `06-realtime-chat` (WebSockets / SSE, presence, optimistic feed)
  - `07-ai-chatbot` (Vercel AI SDK streaming, Generative UI widgets)

- **Batch 10: Reference Projects (Module 24 — Part 2: Projects 08 - 14 + README)**
  - `08-rag-application` (Vector embeddings, pgvector, document retrieval)
  - `09-timetable-application` (Drag-and-drop schedule planner, collision detection)
  - `10-production-fullstack-app` (Full enterprise architecture, BullMQ queues, DAL)
  - `11-url-shortener-with-redis` (High-throughput link redirection, analytics)
  - `12-job-queue-dashboard-with-bullmq` (Asynchronous queue worker dashboard)
  - `13-link-in-bio-multi-tenant` (Custom domains, multi-tenant profile links)
  - `14-notes-app-with-offline-sync` (IndexedDB offline sync queue)
  - `24-projects/README.md` (Project matrix & reproduction guides)

- **Batch 11: Interview Prep & Cheatsheets (Modules 25 & 26)**
  - Module 25: 11 technical interview deep-dives + README.
  - Module 26: 11 quick-reference cheatsheets + README.

- **Batch 12: Final Verification & Delivery**
  - Run link checks, template compliance, regenerate root `tree.txt`.
  - Compile `NEXT_PROGRESS.md`, `NEXT_CHANGELOG.md`, and `NEXT_FINAL_REPORT.md`.
