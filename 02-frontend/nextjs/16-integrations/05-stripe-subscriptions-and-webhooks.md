# Stripe Submissions & Webhook Security (`stripe-subscriptions-and-webhooks`)

> Production Stripe integration in Next.js App Router: Subscription billing cycles, Stripe Checkout sessions, raw body signature verification in Route Handlers, and idempotent webhook processing.

| Metadata | Details |
| :--- | :--- |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Integrations** | Stripe API v17+, Webhook Signatures |
| **Tested With** | Next.js 15.2, Vitest 3.0, TypeScript 5.7 |

---

## 1. Concept Overview & Mental Model

### 1.1 The Payment Life Cycle in Next.js
Payment processing consists of two decoupled flows:
1. **User Initiated Checkout**: A Server Action creates a Stripe Checkout Session and redirects the customer to Stripe's hosted checkout page.
2. **Asynchronous Webhook Reconciliation**: When payment succeeds, Stripe notifies your application via an HTTP POST webhook (`/api/webhooks/stripe`). Your server must verify the cryptographic signature and fulfill the subscription in your database.

```
[ User Clicks "Upgrade" ] ──> [ Server Action: createCheckoutSession() ]
                                          │
                                          ▼ (Returns session.url)
                                [ Stripe Hosted Checkout ]
                                          │
                                          ▼ (Payment Completed)
[ Stripe Webhook Server ] ──(POST /api/webhooks/stripe)──> [ Route Handler ]
                                                                 │
                                                                 ├── 1. Read Raw Text Body
                                                                 ├── 2. Verify stripe-signature
                                                                 ├── 3. Idempotent DB Update
                                                                 └── 4. Return 200 OK
```

---

## 2. Naive vs. Idiomatic Comparison

### Naive: Parsing JSON before Signature Verification
```ts
// app/api/webhooks/stripe/route.ts (Naive - BROKEN & VULNERABLE!)
import { NextResponse } from 'next/server';

export async function POST(req: Request) {
  // CRITICAL ERROR: Parsing as JSON alters raw whitespace and encoding,
  // causing stripe.webhooks.constructEvent to fail with "Signature verification failed!"
  const body = await req.json();
  return NextResponse.json({ received: true });
}
```

### Idiomatic: Raw String Body Verification
```ts
// app/api/webhooks/stripe/route.ts (Idiomatic)
import { NextResponse } from 'next/server';
import { headers } from 'next/headers';
import Stripe from 'stripe';
import { db } from '@/db';

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!, { apiVersion: '2024-12-18.acacia' });
const endpointSecret = process.env.STRIPE_WEBHOOK_SECRET!;

export async function POST(req: Request) {
  // Must read as raw text string!
  const rawBody = await req.text();
  const headerList = await headers();
  const signature = headerList.get('stripe-signature');

  if (!signature) {
    return NextResponse.json({ error: 'Missing signature' }, { status: 400 });
  }

  let event: Stripe.Event;
  try {
    event = stripe.webhooks.constructEvent(rawBody, signature, endpointSecret);
  } catch (err: any) {
    console.error('Webhook signature verification failed:', err.message);
    return NextResponse.json({ error: 'Invalid signature' }, { status: 400 });
  }

  // Handle verified event idempotently...
  return NextResponse.json({ received: true });
}
```

---

## 3. Five Progressive Examples

### Example 1: Creating Stripe Checkout Session in Server Action
```ts
// actions/checkout.ts
'use server';

import Stripe from 'stripe';
import { verifySession } from '@/dal/session';
import { unauthorized } from 'next/navigation';

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!);

export async function createCheckoutSession(priceId: string) {
  const session = await verifySession();
  if (!session) unauthorized();

  const checkoutSession = await stripe.checkout.sessions.create({
    mode: 'subscription',
    payment_method_types: ['card'],
    customer_email: session.user.email,
    line_items: [{ price: priceId, quantity: 1 }],
    success_url: `${process.env.NEXT_PUBLIC_APP_URL}/dashboard?billing=success`,
    cancel_url: `${process.env.NEXT_PUBLIC_APP_URL}/pricing?billing=canceled`,
    metadata: {
      userId: session.user.id,
    },
  });

  return { checkoutUrl: checkoutSession.url };
}
```

### Example 2: Idempotent Event Processing
```ts
// dal/billing.ts
import 'server-only';
import { db } from '@/db';

export async function handleSubscriptionCreated(event: any) {
  const sub = event.data.object;
  const userId = sub.metadata.userId;

  // Idempotent: upsert prevents duplicate processing if Stripe retries webhook
  await db.subscription.upsert({
    where: { stripeSubscriptionId: sub.id },
    create: {
      userId,
      stripeSubscriptionId: sub.id,
      status: sub.status,
      currentPeriodEnd: new Date(sub.current_period_end * 1000),
    },
    update: {
      status: sub.status,
      currentPeriodEnd: new Date(sub.current_period_end * 1000),
    },
  });
}
```

### Example 3: Customer Portal Redirection
```ts
// actions/customerPortal.ts
'use server';

import Stripe from 'stripe';
import { verifySession } from '@/dal/session';

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!);

export async function getBillingPortalUrl() {
  const session = await verifySession();
  if (!session) throw new Error('Unauthorized');

  const user = await db.user.findUnique({ where: { id: session.user.id } });

  const portalSession = await stripe.billingPortal.sessions.create({
    customer: user.stripeCustomerId,
    return_url: `${process.env.NEXT_PUBLIC_APP_URL}/dashboard/billing`,
  });

  return { url: portalSession.url };
}
```

### Example 4: Testing Webhooks with Stripe CLI
```bash
# Listen to local development webhooks and forward to Next.js route handler
stripe listen --forward-to localhost:3000/api/webhooks/stripe
```

### Example 5: Handling Failed Invoices and Grace Periods
```ts
// dal/subscriptionGracePeriod.ts
export async function handleInvoicePaymentFailed(event: any) {
  const invoice = event.data.object;
  await db.subscription.update({
    where: { stripeSubscriptionId: invoice.subscription },
    data: { status: 'PAST_DUE' },
  });
  // Trigger notification email...
}
```

---

## 4. Under the Hood & Internals
Stripe webhooks sign outgoing HTTP request payloads using HMAC-SHA256. The signature header `stripe-signature` contains a timestamp `t` and signature `v1`. Next.js must supply the **raw, un-parsed string buffer** to `constructEvent()`. If any middleware or body parser parses the JSON ahead of time, subtle whitespace or key reordering will invalidate the HMAC hash, rejecting genuine payments.

---

## 5. Security & Idempotency
- **Never Trust Unsigned Payloads**: Always reject requests missing valid signatures with HTTP 400.
- **Idempotency is Mandatory**: Stripe webhooks can be retried multiple times due to temporary network partitions. Ensure database updates are idempotent (using database `upsert` or recording processed `event.id` values in an audit table).

---

## 6. Common Mistakes & Anti-Patterns (Table)

| Anti-Pattern | Root Cause | Observable Symptom | Correct Architecture |
| :--- | :--- | :--- | :--- |
| `req.json()` in webhook handler | Altered string payload | Signature verification failed | Read with `await req.text()` |
| Updating user DB on success_url | Client-side redirection spoofing | Free subscription upgrade without paying | Only update database inside webhook |
| Missing idempotency | Network retry by Stripe | Duplicate records / duplicate charges | Use `upsert` on subscription ID |
| Hardcoding test secret in prod | Deployment oversight | Webhooks fail in production | Use environment variable `STRIPE_WEBHOOK_SECRET` |

---

## 7. Performance & Optimization Guide
Return an HTTP 200 response to Stripe immediately after validating the signature and dispatching the event processing. If processing takes longer than 20 seconds, Stripe marks the webhook as timed out and schedules retries. For heavy workflows, enqueue processing into BullMQ.

---

## 8. TypeScript & Strict Typing Recipes
```ts
import type Stripe from 'stripe';

export type StripeWebhookHandler = (event: Stripe.Event) => Promise<void>;
```

---

## 9. Testing & Quality Verification

```ts
import { describe, it, expect, vi } from 'vitest';
import { POST } from './route';

describe('Stripe Webhook Handler', () => {
  it('rejects requests without signature', async () => {
    const req = new Request('http://localhost:3000/api/webhooks/stripe', {
      method: 'POST',
      body: 'test-body',
    });
    const res = await POST(req);
    expect(res.status).toBe(400);
  });
});
```

---

## 10. When NOT to Use This Pattern
- If selling digital one-time products without recurring memberships, a simpler Checkout Session mode (`mode: 'payment'`) can be used rather than complex subscription lifecycle management.

---

## 11. Frequently Asked Interview Questions

<details>
<summary>1. Why must Stripe webhooks be verified using raw text instead of JSON?</summary>
Stripe creates a cryptographic HMAC-SHA256 signature across the exact raw bytes of the request. Parsing JSON normalizes whitespace and key ordering, changing the byte sequence and causing signature validation to fail.
</details>

<details>
<summary>2. Why is updating database state on the `success_url` page dangerous?</summary>
Because a user can navigate directly to the success URL (`/dashboard?billing=success`) without ever paying. Only Stripe's server-to-server webhook guarantees that payment succeeded.
</details>

<details>
<summary>3. What is webhook idempotency and why is it necessary?</summary>
Stripe retries webhook delivery if your server fails to return HTTP 200 within 20 seconds. An idempotent handler ensures that receiving the same event multiple times produces the exact same database state without duplicate subscriptions or charges.
</details>

<details>
<summary>4. How does the Stripe Customer Portal streamline billing?</summary>
It provides a secure, hosted Stripe interface where customers can update payment methods, download PDF tax invoices, and cancel or upgrade subscriptions without building custom UI.
</details>

<details>
<summary>5. What does the `apiVersion` parameter do in the Stripe SDK?</summary>
It locks the Stripe API version (e.g. `'2024-12-18.acacia'`) to ensure that API responses and webhook payloads match the exact TypeScript types expected by your application.
</details>

<details>
<summary>6. How do you test webhooks locally without public URLs?</summary>
Use the official Stripe CLI command: `stripe listen --forward-to localhost:3000/api/webhooks/stripe`.
</details>

<details>
<summary>7. How do you handle failed recurring subscription payments?</summary>
Listen for `invoice.payment_failed` webhooks, update the subscription status in your database to `PAST_DUE`, and trigger an automated email directing the customer to update their card.
</details>

<details>
<summary>8. Can Next.js Server Actions initiate Stripe payments?</summary>
Yes. A Server Action validates user session, creates a Checkout Session, and returns the redirect URL to the client.
</details>

---

## 12. Hands-on Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a Stripe checkout action</summary>
Build a Server Action returning a Stripe Checkout redirect URL.
</details>

<details>
<summary>Exercise 2 (Medium): Build a signature-verified webhook</summary>
Implement `app/api/webhooks/stripe/route.ts` with raw body reading and error handling.
</details>

<details>
<summary>Exercise 3 (Hard): Implement subscription state machine</summary>
Handle `customer.subscription.created`, `updated`, and `deleted` events with database upserts and status tracking.
</details>

---

## 13. Cross-References & Further Reading
- [Server Actions: Security & Queues](file:///E:/Learning/engineering-notes/02-frontend/nextjs/07-server-actions/05-security-model-of-server-actions.md)
- [Official Stripe Next.js Integration Guide](https://stripe.com/docs/development/quickstart)
