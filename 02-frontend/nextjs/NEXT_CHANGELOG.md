# Next.js Track Changelog

All notable changes, structural migrations, note completions, and project implementations across `02-frontend/nextjs` are documented in this file.

## [2026-11-24] - Full Curriculum Overhaul, Completion & Standardization (100% Delivery)

### Batch 0: Audit, Hygiene, Governance & Templates
- Created `NEXT_FACTS.md` with verified Next.js 15.2+/16 facts, CVE details (CVE-2025-29927, CVE-2025-55184), async request APIs, `"use cache"`, `after()`, `proxy.ts`, and `next/form`.
- Created `NEXT_DECISIONS.md` recording ADRs 001 - 004.
- Created `NEXT_AUDIT.md` mapping file-by-file status, line counts, and gaps across all 27 modules.
- Created `NEXT_COMPLETION_PLAN.md` with detailed batch milestones.
- Created `NEXT_PROGRESS.md` resumable tracking checklist.
- Derived and created authoritative templates:
  - `02-frontend/nextjs/_templates/concept.md` (13 mandatory sections with Next.js/React/Node/Router metadata fields).
  - `02-frontend/nextjs/_templates/project-template.md` (5-part architecture and reproduction specification).
- Resolved 11 stray duplicate README files (`README (1).md` and `README (2).md`) across modules `00-setup`, `02-routing`, `03-components`, `04-rendering`, `05-data-fetching`, `07-server-actions`, `08-route-handlers-and-proxy`, `09-styling-and-assets`, `10-state-management`, `11-authentication`, and `12-database` by standardizing them cleanly to `README.md` via `git mv`. Commit: `249c1dd`.
- Created root track files: `02-frontend/nextjs/INDEX.md` and `02-frontend/nextjs/GLOSSARY.md`. Commit: `c561905`.

### Batch 1: Setup & Fundamentals (`95555d3`)
- Added `00-setup/06-nextjs-versions-and-upgrade-guide.md` (Next.js 13 $\rightarrow$ 14 $\rightarrow$ 15 $\rightarrow$ 16 codemods, async API migration).
- Added `00-setup/07-turbopack-and-bundlers.md` (Turbopack architecture, Webpack comparison, Rust engine).
- Created `00-setup/README.md`.
- Added `01-fundamentals/05-build-output-anatomy-and-route-table.md` (`○`, `ƒ`, `◐` route symbol taxonomy, manifest files).
- Created `01-fundamentals/README.md`.

### Batch 2: Routing, Components & Rendering (`fdba585`)
- Added `02-routing/08-not-found-forbidden-unauthorized-conventions.md` (`notFound()`, `forbidden()`, `unauthorized()` status boundaries).
- Added `03-components/04-server-only-and-client-only-boundaries.md` (`import 'server-only'`, poisoned client bundle defenses).
- Added `04-rendering/04-partial-prerendering-and-cache-components-model.md` (PPR `◐`, speculative execution, cache components).

### Batch 3: Data Fetching, Caching & Server Actions (`34b3cd0`)
- Added `05-data-fetching/03-data-access-layer-with-server-only.md` (Centralized DAL, `React.cache()` memoization).
- Added `06-caching/06-use-cache-directive-in-depth.md` (`"use cache"`, `cacheLife()`, `cacheTag()`, `updateTag()` vs `revalidateTag()`).
- Added `07-server-actions/05-security-model-of-server-actions.md` (Origin verification, CSRF defense, closure encryption, IDOR mitigation).

### Batch 4: Route Handlers, Styling & State Management (`730747f`)
- Added `08-route-handlers-and-proxy/05-proxy-patterns-and-cve-2025-29927.md` (`proxy.ts` convention, CVE-2025-29927 subrequest bypass analysis).
- Added `09-styling-and-assets/07-tailwind-v4-with-nextjs.md` (Tailwind CSS v4 `@import "tailwindcss"`, CSS variables theming).
- Added `10-state-management/04-url-state-server-client-boundaries.md` (`nuqs` URL search params state, server-client sync).

### Batch 5: Auth, Database & TypeScript (`9d24152`)
- Added `11-authentication/06-authjs-and-better-auth-compared.md` (Auth.js v5 vs Better-Auth feature matrix and architecture).
- Added `12-database/06-database-connection-management-and-pooling-serverless.md` (PgBouncer, Neon serverless driver, connection exhaustion).
- Added `13-typescript/03-typed-routes-and-search-params.md` (Typed routes, async `params`/`searchParams` typings).

### Batch 6: SEO, Advanced Features & Integrations (`f969584`)
- Added `14-seo-and-metadata/04-generate-metadata-and-json-ld.md` (Dynamic metadata, JSON-LD schemas, OpenGraph images).
- Added `15-advanced-features/05-opentelemetry-and-instrumentation.md` (`instrumentation.ts`, distributed tracing, span exporters).
- Added `16-integrations/05-stripe-subscriptions-and-webhooks.md` (Stripe Customer Portal, webhook signature verification).

### Batch 7: AI, Testing & Debugging (`a83acaa`)
- Added `17-ai/06-vercel-ai-sdk-core-and-ui-hooks.md` (Vercel AI SDK Core & UI, `useChat`, streaming tool calls).
- Added `18-testing/06-testing-server-components-and-async-components.md` (Testing async RSC with Vitest 3, mocking DAL).
- Added `19-debugging/04-debugging-caching-streaming-and-hydration.md` (Debugging hydration traps, cache staleness, streaming TTFB).

### Batch 8: Performance, Security, Production & Architecture (`f41f7ad`)
- Added `20-performance/05-rsc-payload-and-bundle-analysis.md` (RSC flight payload diet, `@next/bundle-analyzer`).
- Added `21-security/07-server-actions-and-rsc-security-advisories.md` (React Taint APIs, CVE-2025-29927, CVE-2025-55184).
- Added `22-production/08-self-hosting-nextjs-and-deployment-adapters.md` (Docker standalone output, OpenNext on AWS Lambda).
- Added `23-architecture/06-feature-based-architecture-and-clean-boundaries.md` (Feature-sliced design, Clean Architecture, Multi-Zones).

### Batch 9: Reference Projects Part 1 (`a98dd29`)
- Added `24-projects/01-blog/README.md` (MDX Content Platform with Static Generation).
- Added `24-projects/02-authentication/README.md` (Fullstack Auth with Better-Auth & Passkeys).
- Added `24-projects/03-ecommerce/README.md` (E-commerce Storefront with Partial Prerendering & Stripe).
- Added `24-projects/04-admin-dashboard/README.md` (Real-Time Analytics Dashboard with Server-Sent Events).
- Added `24-projects/05-saas-application/README.md` (B2B Multi-Tenant SaaS with Stripe Subscriptions & RBAC).
- Added `24-projects/06-realtime-chat/README.md` (Real-Time Chat with Supabase Realtime & Optimistic UI).
- Added `24-projects/07-ai-chatbot/README.md` (AI Streaming Chatbot with Vercel AI SDK & Tool Calling).

### Batch 10: Reference Projects Part 2 (`180a157`)
- Added `24-projects/08-rag-application/README.md` (Document Search & RAG with pgvector & AI SDK).
- Added `24-projects/09-timetable-application/README.md` (Interactive Schedule & Timetable Planner).
- Added `24-projects/10-production-fullstack-app/README.md` (Enterprise Fullstack Application with Drizzle ORM).
- Added `24-projects/11-url-shortener-with-redis/README.md` (High-Throughput URL Shortener with Upstash Redis & Analytics).
- Added `24-projects/12-job-queue-dashboard-with-bullmq/README.md` (Background Job Processing & Queue Dashboard with BullMQ).
- Added `24-projects/13-link-in-bio-multi-tenant/README.md` (Multi-Tenant Link-in-Bio Platform with Custom Domains).
- Added `24-projects/14-notes-app-with-offline-sync/README.md` (Local-First Notes App with Offline CRDT Sync & PWA).
- Updated `24-projects/README.md` (Complete 14-project portfolio catalog).

### Batch 11: Interview Prep & Cheatsheets (`c532a47` & `76bb833`)
- Added all 11 Module 25 interview guides + `25-interview/README.md`:
  - `00-fundamentals.md`, `01-components.md`, `02-rendering-and-caching.md`, `03-server-actions-and-api.md`, `04-auth.md`, `05-performance.md`, `06-security.md`, `07-architecture.md`, `08-deployment.md`, `09-system-design.md`, `10-output-based-questions.md`.
- Added all 11 Module 26 cheatsheet cards + `26-cheatsheets/README.md`:
  - `00-nextjs.md`, `01-routing.md`, `02-components.md`, `03-rendering.md`, `04-caching.md`, `05-server-actions.md`, `06-route-handlers.md`, `07-authentication.md`, `08-performance.md`, `09-deployment.md`, `10-cli.md`.

### Batch 12: Root Documentation, Tree & Final Delivery
- Created production `02-frontend/nextjs/README.md` with complete curriculum catalog.
- Created `02-frontend/nextjs/ROADMAP.md` with 5 progressive learning trajectories.
- Created `02-frontend/nextjs/CONTRIBUTING.md` with strict authoring and quality guidelines.
- Regenerated root `tree.txt`.
- Generated final audit report `02-frontend/nextjs/NEXT_FINAL_REPORT.md`.
