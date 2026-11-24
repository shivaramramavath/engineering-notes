# Project Template — Next.js Curriculum

> **Source of Truth** for all production reference applications in `02-frontend/nextjs/24-projects/`. Every project folder must contain a `README.md` conforming to this structure, accompanied by modular architecture and source files where applicable.

---

```markdown
# Project [XX]: [Project Title] (`[project-slug]`)

> [Comprehensive summary of the application domain, architecture, full-stack technologies, and production-grade capabilities demonstrated.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-[xx]-[slug]` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | Postgres (Neon / Supabase / Local Docker) + Prisma / Drizzle |
| **Cache & Queues** | Redis (Upstash / ioredis) + BullMQ |
| **Auth** | Auth.js v5 / Better Auth / Clerk |
| **Styling** | Tailwind CSS v4 + Radix UI / shadcn/ui |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49, Docker Compose |
| **Complexity Level** | Production-Grade / Enterprise |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Boundaries
[Detailed explanation: Data Access Layer (DAL) separation, Server Action mutation flows, Cache Components boundary, background BullMQ queue integration, and external service gateways.]

```
+-------------------------------------------------------------------------+
|                       Next.js App Router Architecture                   |
|                                                                         |
|  [ Client Browser / UI ] ──(Server Actions / Forms)──> [ proxy.ts ]     |
|                                                               │         |
|                                                               ▼         |
|  [ RSC Layouts & Pages ] <──(Streaming Suspense)─── [ RSC Engine ]      |
|                                                               │         |
|                                                               ▼         |
|  [ Data Access Layer (server-only) ] <─────────── [ "use cache" ]       |
|            │                                                            |
|            ├──> [ Database (Postgres via Prisma/Drizzle) ]              |
|            └──> [ Redis / BullMQ Queue Workers ] ──(after())           |
+-------------------------------------------------------------------------+
```

### 1.2 Directory Layout
```text
src/
├── app/                      # App router routes, layouts, error boundaries
│   ├── (auth)/               # Auth route group
│   ├── (dashboard)/          # Authenticated workspace group
│   ├── api/                  # Route handlers (webhooks, SSE)
│   ├── layout.tsx            # Root layout with fonts & providers
│   └── page.tsx              # Landing / hero page
├── components/               # Presentation & UI components
│   ├── ui/                   # Primitive design system (shadcn/radix)
│   └── feature/              # Feature-scoped components
├── dal/                      # Data Access Layer (`server-only`)
├── db/                       # Database client, migrations, schema definitions
├── lib/                      # Utilities, auth client, redis, queues
├── types/                    # Domain models, Zod validation schemas
└── proxy.ts                  # Edge network routing & proxy interceptor
```

---

## 2. Complete Project Source Code

### Configuration Files
- `package.json`
- `next.config.ts`
- `tsconfig.json`
- `docker-compose.yml` (Postgres 16 + Redis 7)
- `.env.example`

### Core Application Implementation
- Database Schema & Migrations (`src/db/schema.ts` / `prisma/schema.prisma`)
- Data Access Layer (`src/dal/...`)
- Server Actions & Mutation Services (`src/actions/...`)
- Route Handlers & Webhooks (`src/app/api/...`)
- UI Presentation & Client Shells (`src/app/...`)

---

## 3. Testing Suite & Verification

### 3.1 Unit & Integration Tests (Vitest)
```ts
// src/tests/integration.test.ts
```

### 3.2 End-to-End Tests (Playwright)
```ts
// tests/e2e.spec.ts
```

---

## 4. Build, Docker & Run Instructions

```bash
# 1. Spin up Postgres & Redis containers
docker compose up -d

# 2. Install dependencies
npm install

# 3. Run database migrations & seed
npx prisma migrate dev --name init # or drizzle-kit push
npm run db:seed

# 4. Start Next.js in production build mode
npm run build
npm run start
```
```
