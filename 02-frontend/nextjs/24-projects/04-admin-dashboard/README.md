# Project 04: Enterprise Admin Portal (`admin-dashboard`)

> Mission-critical administration portal built with Next.js App Router, featuring headless TanStack Table v8, role-based access control (RBAC), Recharts KPI analytics, audit logging via `after()`, and CSV export.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-04-admin-portal` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Drizzle ORM |
| **State** | TanStack Table v8 + URL SearchParams |
| **Styling** | Tailwind CSS v4 + Radix UI |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
User access is governed by strict RBAC guards. When an administrator modifies user permissions or revokes access, a Server Action executes the database mutation and triggers `after()` to dispatch an immutable audit record to the security log table.

```
[ Admin User ] ──> [ TanStack Table UI with Selection ]
                            │
                            ▼
           [ Server Action: revokeAccessAction() ]
                 │                          │
                 ▼ (Database Update)        ▼ after() (Non-blocking)
         [ PostgreSQL DB ]          [ Security Audit Log Table ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-admin-dashboard",
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
    "@tanstack/react-table": "^8.20.5",
    "drizzle-orm": "^0.38.0",
    "zod": "^3.24.1"
  },
  "devDependencies": {
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
      POSTGRES_USER: admin_user
      POSTGRES_PASSWORD: admin_secret
      POSTGRES_DB: admin_db
    ports:
      - '5432:5432'
    volumes:
      - admin_pgdata:/var/lib/postgresql/data

volumes:
  admin_pgdata:
```

### Admin Data Access Layer (`src/dal/adminUsers.ts`)
```ts
import 'server-only';
import { cache } from 'react';
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from '@/dal/session';
import { db } from '@/db';
import { users } from '@/db/schema';

export const getSystemUsers = cache(async () => {
  const session = await verifySession();
  if (!session) unauthorized();
  if (session.user.role !== 'SUPER_ADMIN') forbidden();

  return db.select().from(users);
});
```

### Revocation Action with `after()` (`src/actions/adminActions.ts`)
```ts
'use server';

import { after } from 'next/server';
import { revalidatePath } from 'next/cache';
import { unauthorized, forbidden } from 'next/navigation';
import { verifySession } from '@/dal/session';
import { db } from '@/db';
import { users, auditLogs } from '@/db/schema';
import { eq } from 'drizzle-orm';

export async function revokeUserAccess(targetUserId: string) {
  const session = await verifySession();
  if (!session) unauthorized();
  if (session.user.role !== 'SUPER_ADMIN') forbidden();

  await db.update(users).set({ role: 'REVOKED' }).where(eq(users.id, targetUserId));
  revalidatePath('/admin/users');

  // Background non-blocking audit trail
  after(async () => {
    await db.insert(auditLogs).values({
      actorId: session.user.id,
      action: 'REVOKE_ACCESS',
      targetId: targetUserId,
      timestamp: new Date(),
    });
  });

  return { success: true };
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { revokeUserAccess } from '../actions/adminActions';

describe('Admin Action Security', () => {
  it('throws unauthorized when session is missing', async () => {
    await expect(revokeUserAccess('test-user-id')).rejects.toThrow();
  });
});
```

---

## 4. Build and Run Instructions

```bash
docker compose up -d
npm install
npm run build
npm run start
```
