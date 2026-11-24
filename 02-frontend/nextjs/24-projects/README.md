# Module 24: Reference Projects (`24-projects`)

> Complete, enterprise-grade full-stack reference applications bridging theoretical concepts with production architecture. Every project features Docker Compose configurations (PostgreSQL & Redis), TypeScript database schemas, modular service layers, automated testing suites, and reproduction steps.

---

## 1. Project Catalog & Architectural Matrix

| # | Project Name | Slug | Stack & Database | Key Architectures Demonstrated |
| :---: | :--- | :--- | :--- | :--- |
| **01** | [MDX Developer Blog](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/01-blog/README.md) | `blog` | MDX + PostgreSQL | Static generation via `generateStaticParams()`, on-demand ISR via `revalidateTag(tag, 'max')`, dynamic OpenGraph cards |
| **02** | [Fullstack Authentication](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/02-authentication/README.md) | `authentication` | Better Auth + PostgreSQL | Database session tokens, Passkeys/WebAuthn, `httpOnly` secure cookies, defense-in-depth authorization in DAL |
| **03** | [E-Commerce Storefront](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/03-ecommerce/README.md) | `ecommerce` | Stripe + PostgreSQL | `"use cache"` catalog components, Zustand persistent cart, atomic inventory stock decrement in Postgres transactions |
| **04** | [Enterprise Admin Portal](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/04-admin-dashboard/README.md) | `admin-dashboard` | TanStack Table + Postgres | Headless data tables, RBAC guards (`forbidden()`), audit logging via `after()`, CSV export |
| **05** | [Multi-Tenant SaaS](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/05-saas-application/README.md) | `saas-application` | Stripe Subscriptions + Postgres | Subdomain rewriting in `proxy.ts`, Row-Level Security tenant data isolation, billing portal |
| **06** | [Realtime Chat Workspace](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/06-realtime-chat/README.md) | `realtime-chat` | SSE + Redis Pub/Sub | Server-Sent Events streaming Route Handler, Redis Pub/Sub broadcast, optimistic messaging |
| **07** | [AI Conversational Assistant](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/07-ai-chatbot/README.md) | `ai-chatbot` | Vercel AI SDK v4 + OpenAI | Real-time token streaming, Generative UI interactive widgets, Redis token bucket rate limiting |
| **08** | [RAG Document Retrieval](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/08-rag-application/README.md) | `rag-application` | PostgreSQL (`pgvector`) + OpenAI | Vector embeddings, cosine similarity search (`<=>`), grounded source citations |
| **09** | [Timetable Scheduler](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/09-timetable-application/README.md) | `timetable-application` | `@dnd-kit` + PostgreSQL | Drag-and-drop course grid, schedule collision detection, optimistic state updates |
| **10** | [Flagship Enterprise App](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/10-production-fullstack-app/README.md) | `production-fullstack-app` | Prisma v6 + BullMQ + Redis | Feature-Sliced Design (FSD), background job queues, OpenTelemetry tracing, Docker Compose |
| **11** | [High-Throughput URL Shortener](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/11-url-shortener-with-redis/README.md) | `url-shortener-with-redis` | Redis 7 In-Memory | Sub-10ms HTTP 307 redirects, non-blocking click counters, custom slug collision checks |
| **12** | [BullMQ Queue Dashboard](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/12-job-queue-dashboard-with-bullmq/README.md) | `job-queue-dashboard-with-bullmq` | BullMQ + Redis Streams | Background job state machine, visual failure inspection, one-click retry actions |
| **13** | [Link-in-Bio Multi-Tenant](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/13-link-in-bio-multi-tenant/README.md) | `link-in-bio-multi-tenant` | Custom Domains + Postgres | Edge proxy hostname rewriting, custom domain mapping, Partial Prerendering (PPR) |
| **14** | [Offline Notes with Sync](file:///E:/Learning/engineering-notes/02-frontend/nextjs/24-projects/14-notes-app-with-offline-sync/README.md) | `notes-app-with-offline-sync` | Dexie.js + PostgreSQL | Zero-latency local storage, `useSyncExternalStore` network detection, background sync queue |

---

## 2. Progressive Learning Pathways

### Pathway A: Fullstack SaaS & Enterprise Architecture
- Start with **02-authentication** ➔ **05-saas-application** ➔ **04-admin-dashboard** ➔ **10-production-fullstack-app**.

### Pathway B: High Performance & Serverless Data
- Start with **01-blog** (ISR & MDX) ➔ **03-ecommerce** (Stripe & Transactions) ➔ **11-url-shortener-with-redis** (Sub-10ms Redis).

### Pathway C: Modern AI Engineering & Queues
- Start with **07-ai-chatbot** (Vercel AI SDK) ➔ **08-rag-application** (pgvector) ➔ **12-job-queue-dashboard-with-bullmq** (Redis Queues).
