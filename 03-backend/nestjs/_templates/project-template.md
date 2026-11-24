# Project Template — NestJS Backend Curriculum

> **Source of Truth** for all production reference applications in `03-backend/nestjs/10-projects/`. Every project folder must contain a `README.md` conforming to this structure, accompanied by modular architecture specifications and runnable source blocks.

---

```markdown
# Project [XX]: [Project Title] (`[project-slug]`)

> [Comprehensive summary of the application domain, architecture, NestJS features demonstrated, and production-grade capabilities.]

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-[xx]-[slug]` |
| **NestJS Version** | NestJS 11+ |
| **Node.js Baseline** | Node.js >= 20.0.0 (Node.js 22 LTS recommended) |
| **HTTP Platform** | Express 5 (default) / Fastify 5 |
| **Database & ORM** | PostgreSQL (TypeORM / Prisma / Drizzle / MikroORM) / MongoDB |
| **Queue & Messaging** | BullMQ / RabbitMQ / Kafka / Redis |
| **Validation & Auth** | class-validator / nestjs-zod + Passport JWT / Argon2 |
| **Testing** | Vitest / Jest + Supertest |
| **Complexity Level** | Enterprise Production Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow
[Detailed explanation of the modular architecture, dependency injection boundaries, ports & adapters, transaction scope, and external integrations.]

```
+----------------------------------------------------------------------------------------------------+
|                                    NestJS Modular Architecture                                     |
|                                                                                                    |
|  [ Client / API Gateway ] ──(HTTP/REST)──> [ NestJS Application Controller ]                       |
|                                                       │                                            |
|                                                       ▼                                            |
|  [ Pipes / Validation ] <────────────────── [ Guards / Auth Strategy ]                             |
|            │                                                                                       |
|            ▼                                                                                       |
|  [ Application Service Layer ] ──(Constructor Dependency Injection)                                |
|            │                                                                                       |
|            ├──> [ Repository / Data Access Layer (TypeORM / Prisma / Drizzle) ]                    |
|            │             │                                                                         |
|            │             ▼                                                                         |
|            │      [ PostgreSQL DB with Connection Pooling ]                                        |
|            │                                                                                       |
|            └──> [ BullMQ Job Producers / Event Emitters ]                                          |
|                          │                                                                         |
|                          ▼                                                                         |
|                   [ Redis Queue Cluster ] ──> [ BullMQ Worker Processors ]                         |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout
```text
src/
├── app.module.ts              # Root application module orchestrating sub-modules
├── main.ts                    # Bootstrap entrypoint with global pipes, filters, Swagger
├── config/                    # ConfigModule schemas & env validation
├── common/                    # Shared guards, interceptors, filters, decorators
│   ├── filters/
│   ├── guards/
│   ├── interceptors/
│   └── pipes/
└── modules/                   # Cohesive feature modules (Domain-driven)
    ├── auth/
    │   ├── auth.module.ts
    │   ├── auth.controller.ts
    │   ├── auth.service.ts
    │   ├── dto/
    │   └── strategies/
    └── [feature]/
        ├── [feature].module.ts
        ├── [feature].controller.ts
        ├── [feature].service.ts
        ├── entities/
        └── dto/
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-nest-slug",
  "version": "1.0.0",
  "scripts": {
    "start:dev": "nest start --watch",
    "build": "nest build",
    "start:prod": "node dist/main",
    "test": "vitest run",
    "test:e2e": "vitest run --config ./vitest.config.e2e.ts"
  },
  "dependencies": {},
  "devDependencies": {}
}
```

### `tsconfig.json`
```json
{
  "compilerOptions": {
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "declaration": true,
    "removeComments": true,
    "emitDecoratorMetadata": true,
    "experimentalDecorators": true,
    "allowSyntheticDefaultImports": true,
    "target": "ES2023",
    "sourceMap": true,
    "outDir": "./dist",
    "baseUrl": "./",
    "incremental": true,
    "skipLibCheck": true,
    "strictNullChecks": true,
    "noImplicitAny": true,
    "strictBindCallApply": true,
    "forceConsistentCasingInFileNames": true,
    "noFallthroughCasesInSwitch": true
  }
}
```

### `src/main.ts` & Core Module Files
```typescript
// Application bootstrapper and feature implementations
```

---

## 3. Database Schema & Migration Specification

### ORM Entity & SQL Schema
```sql
-- DDL schema definitions with indexes and constraints
```

---

## 4. Verification & Testing Suite

### Unit & Integration Tests
```typescript
// Test suites testing controllers, services, and repositories
```

### E2E API Verification with Supertest
```typescript
// End-to-end API tests against application instance
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
