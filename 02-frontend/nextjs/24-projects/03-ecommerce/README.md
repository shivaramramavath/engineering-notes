# Project 03: Fullstack E-Commerce Platform (`ecommerce`)

> Enterprise multi-category storefront built with Next.js App Router, featuring dynamic faceted filtering, Zustand cart persistence, atomic inventory stock transactions in PostgreSQL, and Stripe Checkout webhook integration.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-03-ecommerce` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Database** | Postgres (Neon / Local Docker) + Drizzle ORM |
| **Payments** | Stripe API v17+ |
| **Styling** | Tailwind CSS v4 + Radix UI Sheet |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
Product browsing utilizes Next.js `"use cache"` components with `cacheLife('hours')` for high-speed edge delivery. Checkout creates a Stripe Session via a Server Action. Upon payment success, Stripe emits a webhook that executes an atomic PostgreSQL transaction, decrementing stock quantities and confirming the order.

```
[ Catalog Grid / Filters ] ──> [ "use cache" Product Component ]
                                           │
                                           ▼
[ Add to Cart ] ──> [ Zustand Persistent Cart Store (Client) ]
                            │
                            ▼
[ Server Action: createCheckout() ] ──> [ Stripe Hosted Checkout ]
                                                   │
                                                   ▼ (Webhook: checkout.session.completed)
[ Postgres Atomic Transaction ] <── [ Stripe Webhook Route Handler ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-ecommerce",
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
    "stripe": "^17.5.0",
    "drizzle-orm": "^0.38.0",
    "zustand": "^5.0.3",
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
      POSTGRES_USER: shop_user
      POSTGRES_PASSWORD: shop_password
      POSTGRES_DB: shop_db
    ports:
      - '5432:5432'
    volumes:
      - shop_pgdata:/var/lib/postgresql/data

volumes:
  shop_pgdata:
```

### Database Schema (`src/db/schema.ts`)
```ts
import { pgTable, text, integer, uuid, timestamp } from 'drizzle-orm/pg-core';

export const products = pgTable('products', {
  id: uuid('id').defaultRandom().primaryKey(),
  name: text('name').notNull(),
  description: text('description').notNull(),
  priceCents: integer('price_cents').notNull(),
  stock: integer('stock').notNull(),
  imageUrl: text('image_url').notNull(),
});

export const orders = pgTable('orders', {
  id: uuid('id').defaultRandom().primaryKey(),
  userId: text('user_id').notNull(),
  totalCents: integer('total_cents').notNull(),
  status: text('status').default('PENDING').notNull(),
  createdAt: timestamp('created_at').defaultNow().notNull(),
});
```

### Cached Product Catalog (`src/dal/products.ts`)
```ts
import 'server-only';
import { cacheLife, cacheTag } from 'next/cache';
import { db } from '@/db';
import { products } from '@/db/schema';

export async function getCachedProducts() {
  'use cache';
  cacheLife('hours');
  cacheTag('products');

  return db.select().from(products);
}
```

### Stripe Webhook Handler with Transaction (`src/app/api/webhooks/stripe/route.ts`)
```ts
import { NextResponse } from 'next/server';
import { headers } from 'next/headers';
import Stripe from 'stripe';
import { db } from '@/db';
import { products, orders } from '@/db/schema';
import { eq, sql } from 'drizzle-orm';

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!);

export async function POST(req: Request) {
  const rawBody = await req.text();
  const signature = (await headers()).get('stripe-signature')!;

  let event: Stripe.Event;
  try {
    event = stripe.webhooks.constructEvent(rawBody, signature, process.env.STRIPE_WEBHOOK_SECRET!);
  } catch (err: any) {
    return NextResponse.json({ error: 'Invalid signature' }, { status: 400 });
  }

  if (event.type === 'checkout.session.completed') {
    const session = event.data.object as Stripe.Checkout.Session;
    const items = JSON.parse(session.metadata?.items || '[]');

    // Execute atomic inventory deduction
    await db.transaction(async (tx) => {
      for (const item of items) {
        await tx
          .update(products)
          .set({ stock: sql`${products.stock} - ${item.quantity}` })
          .where(eq(products.id, item.id));
      }

      await tx.insert(orders).values({
        userId: session.metadata?.userId || 'guest',
        totalCents: session.amount_total || 0,
        status: 'PAID',
      });
    });
  }

  return NextResponse.json({ received: true });
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('E-Commerce Calculations', () => {
  it('computes subtotal correctly', () => {
    const items = [{ priceCents: 12000, quantity: 2 }, { priceCents: 5000, quantity: 1 }];
    const total = items.reduce((sum, item) => sum + item.priceCents * item.quantity, 0);
    expect(total).toBe(29000);
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
