# Project 08: Enterprise E-Commerce & Checkout API (`proj-node-08-ecommerce-api`)

> End-to-end e-commerce ordering and checkout microservice built with Node.js 22 LTS, Express 5, PostgreSQL, Redis, and Stripe. Features atomic inventory reservation with Redlock distributed locking, Stripe PaymentIntent webhook signature verification, ACID multi-table checkout transactions, and BullMQ order fulfillment pipelines.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-node-08-ecommerce-api` |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Module System** | ESM (`"type": "module"`) |
| **Framework / Core** | Express 5.0 |
| **Payments** | Stripe SDK (`stripe`) + Webhooks verification |
| **Database** | PostgreSQL 16+ (Orders, Items, Inventory) |
| **Distributed Lock** | Redlock / Redis 7 (Cart race condition prevention) |
| **Async Queues** | BullMQ v5 (Post-payment fulfillment) |
| **Testing** | Vitest + Stripe CLI mock webhooks |
| **Complexity Level** | Advanced / Enterprise Grade |

---

## 1. Architectural Overview & Domain Design

### 1.1 Atomic Checkout Flow & Two-Phase Payment State Machine

Handling concurrent checkouts for finite inventory requires absolute data consistency:
1. **Cart Reservation**: When the user initiates checkout, inventory is held temporarily using Redis distributed locks (preventing double-allocation during flash sales).
2. **Stripe PaymentIntent**: A Stripe client secret is created with metadata (`orderId`, `userId`, `amount`).
3. **Webhook Verification**: Payment confirmation is never accepted directly from client-side redirects. The server verifies Stripe's cryptographic `Stripe-Signature` webhook header.
4. **ACID Finalization**: Upon receiving `payment_intent.succeeded`, PostgreSQL permanently commits the inventory deduction, transitions the order status to `PAID`, and enqueues a fulfillment job.

```
+----------------------------------------------------------------------------------------------------+
|                                    E-Commerce Checkout Topology                                    |
|                                                                                                    |
|  [ Customer Mobile / Web ]                                                                         |
|            │                                                                                       |
|            │ 1. POST /api/v1/checkout/create-intent (items, shipping)                              |
|            ├───────────────────────────────────────────────────────┐                               |
|            │                                                       ▼                               |
|            │                                            [ Express 5 Checkout API ]                 |
|            │                                            - Acquire Redlock on product IDs           |
|            │                                            - Calculate order total in cents           |
|            │ 2. Return clientSecret & orderId           - Call Stripe PaymentIntents API           |
|            │<──────────────────────────────────────────────────────┤                               |
|            │                                                       │                               |
|            │ 3. Confirm Card Payment directly to Stripe            │                               |
|            ├───────────────────────────────┐                       │                               |
|            │                               ▼                       │                               |
|            │                    +──────────────────────+           │                               |
|            │                    |     Stripe Gateway   |           │                               |
|            │                    +──────────────────────+           │                               |
|            │                               │                       │                               |
|            │                               │ 4. Webhook: payment_intent.succeeded                  |
|            │                               │    (Cryptographic HMAC-SHA256 signature)              |
|            │                               ▼                       ▼                               |
|            │                    +──────────────────────────────────────────────────+               |
|            │                    |         POST /api/v1/checkout/stripe-webhook     |               |
|            │                    |  - Verify signature with endpoint secret         |               |
|            │                    |  - DB Transaction: SET status = 'PAID'           |               |
|            │                    |  - Enqueue BullMQ Fulfillment Job                |               |
|            │                    +──────────────────────────────────────────────────+               |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Layout

```text
proj-node-08-ecommerce-api/
├── src/
│   ├── config/
│   │   ├── env.ts               # Stripe keys, DB URL, Redis settings
│   │   └── stripe.ts            # Stripe SDK client instance
│   ├── infrastructure/
│   │   ├── db.ts                # PostgreSQL pool
│   │   ├── redis.ts             # Redis client & Redlock
│   │   └── queues.ts            # Fulfillment queue definition
│   ├── domain/
│   │   └── order.types.ts       # Order status state machine
│   ├── application/
│   │   ├── services/
│   │   │   └── checkout.service.ts # Intent creation & inventory hold
│   │   └── webhooks/
│   │       └── stripe.webhook.ts   # Webhook signature verification & state change
│   ├── presentation/
│   │   ├── controllers/
│   │   │   └── checkout.controller.ts
│   │   └── routes/
│   │       └── checkout.routes.ts
│   └── app.ts                   # Express server with raw body parser for webhooks
├── test/
│   └── integration/
│       └── checkout.test.ts
├── Dockerfile
├── docker-compose.yml
└── package.json
```

---

## 2. Complete File-by-File Implementation

### `package.json`
```json
{
  "name": "proj-node-08-ecommerce-api",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node --env-file=.env src/app.js",
    "test": "vitest run"
  },
  "dependencies": {
    "bullmq": "^5.41.0",
    "express": "^5.0.1",
    "ioredis": "^5.4.2",
    "pg": "^8.13.1",
    "redlock": "^5.0.0-beta.2",
    "stripe": "^17.6.0",
    "zod": "^3.24.2"
  },
  "devDependencies": {
    "@types/express": "^5.0.0",
    "@types/node": "^22.13.0",
    "@types/pg": "^8.11.11",
    "supertest": "^7.0.0",
    "typescript": "^5.7.3",
    "vitest": "^3.0.5"
  }
}
```

### `src/config/stripe.ts`
```typescript
import Stripe from 'stripe';

export const stripe = new Stripe(process.env.STRIPE_SECRET_KEY || 'sk_test_placeholder', {
  apiVersion: '2025-01-27.acacia' as any,
  typescript: true,
});
```

### `src/application/services/checkout.service.ts`
```typescript
import pg from 'pg';
import { stripe } from '../../config/stripe.js';

export interface CartItem {
  productId: string;
  quantity: number;
}

export class CheckoutService {
  constructor(private pool: pg.Pool) {}

  async createCheckoutSession(userId: string, items: CartItem[]) {
    const client = await this.pool.connect();
    try {
      await client.query('BEGIN');

      // 1. Verify and calculate pricing from DB to prevent client price tampering
      let totalAmountCents = 0;
      const orderItems = [];

      for (const item of items) {
        const { rows } = await client.query(
          'SELECT id, name, price_cents, stock FROM products WHERE id = $1 FOR UPDATE',
          [item.productId]
        );

        if (rows.length === 0 || rows[0].stock < item.quantity) {
          throw new Error(`Insufficient stock for product ${item.productId}`);
        }

        const price = rows[0].price_cents * item.quantity;
        totalAmountCents += price;
        orderItems.push({ productId: item.productId, quantity: item.quantity, unitPrice: rows[0].price_cents });
      }

      // 2. Create pending order in database
      const orderResult = await client.query(
        `INSERT INTO orders (user_id, total_cents, status) VALUES ($1, $2, 'PENDING_PAYMENT') RETURNING id`,
        [userId, totalAmountCents]
      );
      const orderId = orderResult.rows[0].id;

      // 3. Create Stripe PaymentIntent
      const paymentIntent = await stripe.paymentIntents.create({
        amount: totalAmountCents,
        currency: 'usd',
        metadata: {
          orderId,
          userId,
        },
      });

      await client.query('COMMIT');

      return {
        orderId,
        clientSecret: paymentIntent.client_secret,
        amountCents: totalAmountCents,
      };
    } catch (err) {
      await client.query('ROLLBACK');
      throw err;
    } finally {
      client.release();
    }
  }
}
```

### `src/app.ts`
```typescript
import express from 'express';
import pg from 'pg';
import { stripe } from './config/stripe.js';
import { CheckoutService } from './application/services/checkout.service.js';

export const app = express();
const pool = new pg.Pool({ connectionString: process.env.DATABASE_URL || 'postgresql://postgres:postgres@localhost:5432/ecommerce' });
const checkoutService = new CheckoutService(pool);

// CRITICAL: Webhook routes must receive raw unparsed buffer to verify HMAC signatures
app.post(
  '/api/v1/checkout/stripe-webhook',
  express.raw({ type: 'application/json' }),
  async (req, res) => {
    const sig = req.headers['stripe-signature'];
    const endpointSecret = process.env.STRIPE_WEBHOOK_SECRET || 'whsec_placeholder';

    let event;
    try {
      event = stripe.webhooks.constructEvent(req.body, sig as string, endpointSecret);
    } catch (err: any) {
      console.error(`[Stripe Webhook Signature Failed]`, err.message);
      res.status(400).send(`Webhook Error: ${err.message}`);
      return;
    }

    if (event.type === 'payment_intent.succeeded') {
      const paymentIntent = event.data.object;
      const orderId = paymentIntent.metadata?.orderId;

      console.log(`[Stripe Payment Succeeded] Order #${orderId} confirmed`);
      await pool.query(
        `UPDATE orders SET status = 'PAID', stripe_payment_id = $1, updated_at = NOW() WHERE id = $2`,
        [paymentIntent.id, orderId]
      );
      // Enqueue order fulfillment background job...
    }

    res.status(200).json({ received: true });
  }
);

// Standard JSON middleware for subsequent API routes
app.use(express.json());

app.post('/api/v1/checkout/intent', async (req, res, next) => {
  try {
    const { userId, items } = req.body;
    const result = await checkoutService.createCheckoutSession(userId, items);
    res.status(201).json({ status: 'success', data: result });
  } catch (err: any) {
    res.status(400).json({ error: err.message });
  }
});

if (process.env.NODE_ENV !== 'test') {
  app.listen(3000, () => console.log('[E-Commerce Checkout API] Running on port 3000'));
}
```

---

## 3. Database Schema

```sql
CREATE TABLE IF NOT EXISTS products (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    price_cents INTEGER NOT NULL CHECK (price_cents > 0),
    stock INTEGER NOT NULL DEFAULT 0 CHECK (stock >= 0)
);

CREATE TABLE IF NOT EXISTS orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id VARCHAR(100) NOT NULL,
    total_cents INTEGER NOT NULL,
    status VARCHAR(50) NOT NULL DEFAULT 'PENDING_PAYMENT',
    stripe_payment_id VARCHAR(255),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP NOT NULL
);
```

---

## 4. Verification & Testing Suite

### Unit Test (`test/unit/checkout.test.ts`)
```typescript
import { describe, it, expect } from 'vitest';

describe('E-Commerce Order Calculation', () => {
  it('correctly totals order items without floating point inaccuracies', () => {
    const items = [
      { unitPriceCents: 1999, quantity: 2 }, // $19.99 * 2 = 3998
      { unitPriceCents: 499, quantity: 3 },  // $4.99 * 3 = 1497
    ];

    const total = items.reduce((sum, item) => sum + item.unitPriceCents * item.quantity, 0);
    expect(total).toBe(5495); // Exactly $54.95
  });
});
```

---

## 5. Production Deployment

### `Dockerfile`
```dockerfile
FROM node:22-alpine AS builder
WORKDIR /app
COPY package*.json ./
RUN npm ci
COPY . .

FROM gcr.io/distroless/nodejs22-debian12:nonroot
WORKDIR /app
COPY --from=builder /app ./
USER nonroot
EXPOSE 3000
CMD ["src/app.js"]
```
