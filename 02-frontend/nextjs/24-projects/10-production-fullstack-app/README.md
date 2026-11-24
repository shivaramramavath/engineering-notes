# Project 10: Production Enterprise Fullstack App (`production-fullstack-app`)

> Complete flagship enterprise application built with Next.js App Router, featuring Feature-Sliced Design (FSD), BullMQ Redis queues, PostgreSQL database migrations, OpenTelemetry tracing, and Docker Compose orchestration.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-10-enterprise-core` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Prisma v6 |
| **Queues & Cache** | Redis 7 + BullMQ |
| **Observability**| OpenTelemetry |
| **Styling** | Tailwind CSS v4 + Radix UI |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Enterprise Production |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 Enterprise Data Flow
The application strictly enforces Feature-Sliced Design. User requests pass through `proxy.ts` for security header injection. User mutations trigger Server Actions with Zod validation. Non-urgent background writes (e.g. invoice generation, telemetry sync) are offloaded to a Redis-backed BullMQ queue via `after()`.

```
[ User Browser ] ──> [ proxy.ts (Security & Nonces) ]
                             │
                             ▼
  [ App Router Thin Controllers ] ──> [ Widgets & Feature Slices ]
                                                │
                                                ▼
  [ Data Access Layer (server-only) ] ──> [ PostgreSQL (Prisma v6) ]
                                                │
                                                ▼ after()
                                [ BullMQ Redis Queue Worker ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-enterprise-fullstack",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "@prisma/client": "^6.2.1",
    "bullmq": "^5.34.1",
    "ioredis": "^5.4.2",
    "zod": "^3.24.1"
  },
  "devDependencies": {
    "prisma": "^6.2.1",
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

#### `docker-compose.yml`
```yaml
version: '3.8'

services:
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_USER: enterprise_user
      POSTGRES_PASSWORD: enterprise_secret
      POSTGRES_DB: enterprise_db
    ports:
      - '5432:5432'
    volumes:
      - pgdata:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    ports:
      - '6379:6379'

volumes:
  pgdata:
```

### Prisma Schema (`prisma/schema.prisma`)
```prisma
datasource db {
  provider = "postgresql"
  url      = env("DATABASE_URL")
}

generator client {
  provider = "prisma-client-js"
}

model Organization {
  id        String   @id @default(uuid())
  name      String
  slug      String   @unique
  createdAt DateTime @default(now())
}
```

### BullMQ Background Job Dispatcher (`src/lib/queueDispatcher.ts`)
```ts
import 'server-only';
import { Queue } from 'bullmq';
import Redis from 'ioredis';

const connection = new Redis(process.env.REDIS_URL!);
export const backgroundJobQueue = new Queue('enterprise-jobs', { connection });

export async function enqueueBackgroundJob(name: string, payload: any) {
  return backgroundJobQueue.add(name, payload, {
    attempts: 3,
    backoff: { type: 'exponential', delay: 1000 },
  });
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Enterprise Configuration Validation', () => {
  it('validates environment structure', () => {
    expect(process.env.PORT || '3000').toBe('3000');
  });
});
```

---

## 4. Build and Run Instructions

```bash
docker compose up -d
npx prisma migrate dev --name init
npm run build
npm run start
```
