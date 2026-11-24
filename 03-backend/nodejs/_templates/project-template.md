# Project Template — Node.js Backend Curriculum

> **Source of Truth** for all production reference applications in `03-backend/nodejs/21-projects/` and `20-aws-sdk/05-projects/`. Every project folder must contain a `README.md` conforming to this structure, accompanied by modular architecture specifications and runnable source blocks.

---

```markdown
# Project [XX]: [Project Title] (`[project-slug]`)

> [Comprehensive summary of the application domain, architecture, backend technologies, and production-grade capabilities demonstrated.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-[xx]-[slug]` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5 / Fastify 5 / Hono / Native HTTP |
| **Database** | PostgreSQL (pg / Drizzle / Prisma) / MongoDB / Redis |
| **Queue / Async** | BullMQ / RabbitMQ / Kafka / Redis Streams |
| **Validation & Auth** | Zod + Argon2 / Jose (JWT) / Better-Auth |
| **Testing** | Vitest / Supertest / Testcontainers |
| **Complexity Level** | Production-Grade / Enterprise |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow
[Detailed explanation: Layered or Hexagonal architecture, boundary interfaces, dependency injection container, database transaction boundaries, background worker synchronization, and external API gateways.]

```
+-------------------------------------------------------------------------+
|                       Node.js Backend Architecture                      |
|                                                                         |
|  [ Client / API Gateway ] ──(HTTP/REST/SSE)──> [ Express 5 / Fastify 5 ]|
|                                                      │                  |
|                                                      ▼                  |
|  [ Controllers / Route Handlers ] <─────────── [ Zod Validation ]       |
|            │                                                            |
|            ▼                                                            |
|  [ Application Services / Domain Use Cases ] ──(DI via Constructors)    |
|            │                                                            |
|            ├──> [ Repositories / Data Access Layer (Drizzle / pg) ]     |
|            │             │                                              |
|            │             ▼                                              |
|            │      [ PostgreSQL DB with Connection Pooling ]             |
|            │                                                            |
|            └──> [ BullMQ Producers / Event Emitters ]                   |
|                          │                                              |
|                          ▼                                              |
|                   [ Redis Queue Cluster ] ──> [ BullMQ Workers ]        |
+-------------------------------------------------------------------------+
```

### 1.2 Directory Layout
```text
src/
├── domain/                   # Enterprise entities & business rules (pure TS/JS)
│   ├── entities/
│   └── interfaces/           # Repository & service interface contracts
├── application/              # Application use cases & service orchestration
│   ├── dtos/                 # Request/response schemas & types
│   └── services/             # Core business logic implementation
├── infrastructure/           # Database, cache, queues, external APIs
│   ├── database/             # pg Pool, Drizzle schema, migrations
│   ├── redis/                # ioredis client singleton
│   ├── queues/               # BullMQ worker & queue definitions
│   └── repositories/         # Concrete repository implementations
├── presentation/             # HTTP server, routing, controllers, middleware
│   ├── middleware/           # Auth guard, error handler, rate limiter
│   ├── controllers/          # Request handling & DTO mapping
│   └── routes/               # Express/Fastify route registration
├── config/                   # Environment variables with Zod validation
└── app.ts                    # Application bootstrapper & lifecycle
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-slug",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "dev": "node --watch --env-file=.env src/app.js",
    "start": "node --env-file=.env.production src/app.js",
    "test": "node --test"
  },
  "dependencies": {},
  "devDependencies": {}
}
```

### `src/config/env.ts`
```typescript
// Environment validation schema with Zod
```

### Core Business Logic & Repositories
```typescript
// Domain entities, repository implementations, service classes
```

### HTTP Routes & Controllers
```typescript
// Presentation layer with Express/Fastify routes and controllers
```

---

## 3. Database Schema & Migration Specification

### Drizzle / SQL Schema
```sql
-- DDL schema definitions with indexes and constraints
```

---

## 4. Verification & Testing Suite

### Unit & Integration Tests
```typescript
// Test suites testing controllers, services, and repositories
```

### Supertest API Verification
```typescript
// E2E HTTP API tests
```

---

## 5. Production Deployment & Containerization

### `Dockerfile`
```dockerfile
# Multi-stage production container build
```

### `docker-compose.yml`
```yaml
# Local and CI service orchestration
```
```
