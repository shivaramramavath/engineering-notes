# Module 26: Route Handlers (`route.ts`) Cheatsheet

> **Quick Reference**: REST endpoints, Webhooks, HTTP streaming (SSE), cookies, headers, and CORS handling in Next.js 15+ Route Handlers.

---

## 1. Supported HTTP Methods in `route.ts`

Declare exported functions matching standard HTTP methods:

```typescript
// app/api/items/route.ts
import { NextResponse, type NextRequest } from 'next/server';

export async function GET(request: NextRequest) {
  return NextResponse.json({ message: 'List items' });
}

export async function POST(request: NextRequest) {
  const body = await request.json();
  return NextResponse.json({ item: body }, { status: 201 });
}

export async function PUT(request: NextRequest) { /* ... */ }
export async function PATCH(request: NextRequest) { /* ... */ }
export async function DELETE(request: NextRequest) { /* ... */ }
export async function HEAD(request: NextRequest) { /* ... */ }
export async function OPTIONS(request: NextRequest) { /* ... */ }
```

---

## 2. Dynamic Route Segments in Handlers

```typescript
// app/api/users/[id]/route.ts
interface RouteContext {
  params: Promise<{ id: string }>;
}

export async function GET(request: Request, { params }: RouteContext) {
  // Await params in Next.js 15+
  const { id } = await params;
  const user = await db.user.findUnique({ where: { id } });

  if (!user) {
    return NextResponse.json({ error: 'User not found' }, { status: 404 });
  }

  return NextResponse.json(user);
}
```

---

## 3. Streaming Server-Sent Events (SSE)

Stream real-time updates to the browser using standard `ReadableStream`:

```typescript
// app/api/events/route.ts
export async function GET() {
  const encoder = new TextEncoder();

  const stream = new ReadableStream({
    async start(controller) {
      for (let i = 1; i <= 5; i++) {
        controller.enqueue(encoder.encode(`data: ${JSON.stringify({ count: i })}\n\n`));
        await new Promise(res => setTimeout(res, 1000));
      }
      controller.close();
    },
  });

  return new Response(stream, {
    headers: {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache, no-transform',
      'Connection': 'keep-alive',
    },
  });
}
```

---

## 4. Webhook Verification Example (Stripe)

Verify raw request signatures before parsing JSON:

```typescript
// app/api/webhooks/stripe/route.ts
import { stripe } from '@/lib/stripe';
import { headers } from 'next/headers';
import { NextResponse } from 'next/server';

export async function POST(req: Request) {
  const body = await req.text(); // Raw text for signature verification!
  const signature = (await headers()).get('stripe-signature')!;

  try {
    const event = stripe.webhooks.constructEvent(
      body,
      signature,
      process.env.STRIPE_WEBHOOK_SECRET!
    );

    if (event.type === 'checkout.session.completed') {
      // Process order
    }

    return NextResponse.json({ received: true });
  } catch (err: any) {
    return NextResponse.json({ error: err.message }, { status: 400 });
  }
}
```
