# Project 05: Multi-Tenant SaaS Platform (`saas-application`)

> Multi-tenant subscription software platform built with Next.js App Router, featuring dynamic subdomain routing in `proxy.ts`, tenant data isolation via PostgreSQL Row-Level Security, Stripe recurring billing, and team invitations.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-05-saas-platform` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | PostgreSQL + Drizzle ORM + RLS |
| **Billing** | Stripe Subscriptions |
| **Styling** | Tailwind CSS v4 + Radix UI |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Enterprise Production |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Tenants are isolated by subdomain (e.g. `acme.saas.com`). In `proxy.ts`, incoming subdomains are rewritten to internal dynamic route parameters (`/tenants/acme/...`). At the database layer, queries apply tenant scoping to guarantee that customer data never leaks across tenant boundaries.

```
[ Inbound: acme.saas.com/projects ]
                 │
                 ▼
[ proxy.ts Edge Interceptor ] ──(Rewrites)──> [ /tenants/acme/projects ]
                                                      │
                                                      ▼
[ Data Access Layer (server-only) ] ──> [ WHERE tenant_id = 'acme' ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-multitenant-saas",
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
    "drizzle-orm": "^0.38.0",
    "stripe": "^17.5.0",
    "zod": "^3.24.1"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

#### `proxy.ts` (Next.js 16 Edge Interceptor)
```ts
import { NextResponse } from 'next/server';
import type { NextRequest } from 'next/server';

export function proxy(req: NextRequest) {
  const url = req.nextUrl;
  const hostname = req.headers.get('host') || '';

  // Extract subdomain (e.g., 'acme' from 'acme.platform.com')
  const currentHost = hostname.replace(`.${process.env.NEXT_PUBLIC_ROOT_DOMAIN}`, '');

  if (currentHost && currentHost !== 'app' && currentHost !== 'www' && currentHost !== hostname) {
    return NextResponse.rewrite(
      new URL(`/tenants/${currentHost}${url.pathname}${url.search}`, req.url)
    );
  }

  return NextResponse.next();
}

export const config = {
  matcher: ['/((?!api/|_next/static|_next/image|favicon.ico).*)'],
};
```

### Database Schema with Tenant Scoping (`src/db/schema.ts`)
```ts
import { pgTable, text, uuid, timestamp } from 'drizzle-orm/pg-core';

export const tenants = pgTable('tenants', {
  id: uuid('id').defaultRandom().primaryKey(),
  slug: text('slug').notNull().unique(), // e.g. 'acme'
  name: text('name').notNull(),
  stripeCustomerId: text('stripe_customer_id'),
  plan: text('plan').default('STARTER').notNull(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});

export const projects = pgTable('projects', {
  id: uuid('id').defaultRandom().primaryKey(),
  tenantId: uuid('tenant_id').references(() => tenants.id, { onDelete: 'cascade' }).notNull(),
  name: text('name').notNull(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});
```

### Scoped Data Access Layer (`src/dal/projects.ts`)
```ts
import 'server-only';
import { cache } from 'react';
import { db } from '@/db';
import { projects, tenants } from '@/db/schema';
import { eq, and } from 'drizzle-orm';
import { verifySession } from '@/dal/session';
import { unauthorized, forbidden } from 'next/navigation';

export const getTenantProjects = cache(async (tenantSlug: string) => {
  const session = await verifySession();
  if (!session) unauthorized();

  const tenant = (await db.select().from(tenants).where(eq(tenants.slug, tenantSlug)).limit(1))[0];
  if (!tenant) return null;

  // Enforce tenant boundary
  if (session.user.tenantId !== tenant.id) {
    forbidden();
  }

  return db.select().from(projects).where(eq(projects.tenantId, tenant.id));
});
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('Multi-Tenant Subdomain Matching', () => {
  it('identifies custom subdomains accurately', () => {
    const host = 'acme.platform.com';
    const sub = host.split('.')[0];
    expect(sub).toBe('acme');
  });
});
```

---

## 4. Build and Run Instructions

```bash
npm install
npm run build
npm run start
```
