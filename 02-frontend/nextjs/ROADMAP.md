# Next.js Engineering Mastery Roadmap

> Structured career learning paths for engineering progression: from React transition to Staff / Principal Software Architect on Next.js 15+ and 16.0.

---

## Visual Learning Trajectories

```text
  Level 1: Core Framework & Modern Routing
  [00-setup] ───► [01-fundamentals] ───► [02-routing] ───► [03-components] ───► [04-rendering]
                                                                                     │
  Level 2: Data Architecture, Mutations & Caching                                    ▼
  [08-route-handlers] ◄─── [07-server-actions] ◄─── [06-caching] ◄─── [05-data-fetching]
        │
        ▼
  Level 3: Full-Stack Systems, Auth & Tooling
  [09-styling] ───► [10-state] ───► [11-auth] ───► [12-database] ───► [13-typescript]
                                                                             │
  Level 4: Enterprise Scale, Security & AI                                    ▼
  [17-ai] ◄─── [16-integrations] ◄─── [15-advanced] ◄─── [14-seo-and-metadata]
        │
        ▼
  Level 5: Verification, Reliability & Architecture
  [18-testing] ───► [19-debugging] ───► [20-performance] ───► [21-security] ───► [22-production]
                                                                                        │
  Level 6: Reference Projects & Senior Interview Mastery                                ▼
  [26-cheatsheets] ◄─── [25-interview] ◄─── [24-projects] ◄─── [23-architecture]
```

---

## Track 1: Foundations (Transitioning from React / SPA)
*Goal: Understand the inverted paradigm of React Server Components, file-system routing, and component-level data fetching.*

1. **Modern Setup & Turbopack**: Master Next.js 15/16 upgrades, ESLint 9 Flat Config, and tsconfig bundler resolution ([00-setup](./00-setup/README.md)).
2. **App Router Fundamentals**: Grasp server runtime defaults, node vs edge targets, and build route symbols (`○`, `ƒ`, `◐`) ([01-fundamentals](./01-fundamentals/README.md)).
3. **Routing & Dynamic Segments**: Implement dynamic parameters, async parameter unwrapping (`await params`), layouts, and route groups ([02-routing](./02-routing/README.md)).
4. **Server vs Client Boundaries**: Master RSC wire protocol, interleaving Server and Client components via `children`, and compilation guards with `server-only` ([03-components](./03-components/README.md)).
5. **Rendering & PPR**: Learn Static, Dynamic, Suspense streaming, and Partial Prerendering (`experimental_ppr = true`) ([04-rendering](./04-rendering/README.md)).

---

## Track 2: Data Engineering, Mutations & Caching
*Goal: Build robust data access pipelines, handle form mutations, and master the modern `"use cache"` directive.*

1. **Component-Level Data Fetching**: Colocate queries with RSC and implement memoization with `React.cache()` ([05-data-fetching](./05-data-fetching/README.md)).
2. **The Modern Caching Model**: Transition from legacy `revalidate = N` to `"use cache"`, `cacheLife()` profiles, and `updateTag()` vs `revalidateTag()` ([06-caching](./06-caching/README.md)).
3. **Server Actions & Mutations**: Build progressive forms with React 19 `useActionState` and instant optimistic UI with `useOptimistic` ([07-server-actions](./07-server-actions/README.md)).
4. **Route Handlers & Edge Proxy**: Build REST/Webhook endpoints, stream Server-Sent Events (SSE), and handle edge routing via `proxy.ts` ([08-route-handlers-and-proxy](./08-route-handlers-and-proxy/README.md)).

---

## Track 3: Full-Stack Production Engineering
*Goal: Implement production authentication, persistent database ORMs, state management, and asset pipelines.*

1. **Styling & Assets**: Adopt Tailwind CSS v4, zero-layout-shift `next/font`, and optimized `next/image` ([09-styling-and-assets](./09-styling-and-assets/README.md)).
2. **State Management**: Bridge URL state via `nuqs`, server-to-client hydration, and Zustand v5 stores in App Router ([10-state-management](./10-state-management/README.md)).
3. **Authentication & RBAC**: Implement defense-in-depth auth via the Data Access Layer (DAL), encrypted session tokens, and Better-Auth / Auth.js ([11-authentication](./11-authentication/README.md)).
4. **Database & Connection Pooling**: Connect to Postgres via Drizzle ORM / Prisma v6 with PgBouncer connection pooling in serverless environments ([12-database](./12-database/README.md)).
5. **Strict TypeScript Integration**: Enable typed routes, type async route contexts, and enforce end-to-end schema validation ([13-typescript](./13-typescript/README.md)).

---

## Track 4: Enterprise Scale, Security & Systems Architecture
*Goal: Design high-throughput, secure, multi-tenant enterprise architectures and deploy to production.*

1. **Advanced Telemetry & Tasks**: Configure OpenTelemetry with `instrumentation.ts` and run non-blocking post-response jobs with `after()` ([15-advanced-features](./15-advanced-features/README.md)).
2. **AI Applications**: Build generative streaming chat interfaces with Vercel AI SDK Core & UI and vector embeddings ([17-ai](./17-ai/README.md)).
3. **Security Architecture**: Enforce React Taint APIs, dynamic CSP nonces, and defend against CVE-2025-29927 and CVE-2025-55184 ([21-security](./21-security/README.md)).
4. **Production Containers & Self-Hosting**: Build ~80MB standalone Docker images (`output: 'standalone'`), deploy on Kubernetes, or run OpenNext on AWS ([22-production](./22-production/README.md)).
5. **Clean Architecture & Multi-Zones**: Implement Clean Architecture with strict DAL boundaries, multi-tenant subdomain routing, and micro-frontend multi-zones ([23-architecture](./23-architecture/README.md)).

---

## Track 5: Reference Projects & Senior / Staff Interview Mastery
*Goal: Build portfolio-grade enterprise applications and pass high-bar technical interviews.*

1. **14 Reference Projects**: Implement production SaaS, E-commerce with PPR, RAG Search, BullMQ Queue Dashboard, and Offline Sync ([24-projects](./24-projects/README.md)).
2. **Senior & Staff Interview Prep**: Master 11 in-depth interview modules covering internals, system design scenarios, and tricky output predictions ([25-interview](./25-interview/README.md)).
3. **Everyday Cheatsheets**: Keep 11 quick-reference cheatsheets bookmarked for day-to-day high-velocity engineering ([26-cheatsheets](./26-cheatsheets/README.md)).
