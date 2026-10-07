# Webhooks

A **webhook** is an HTTP request another service sends to *your* URL when something happens: a payment succeeds, a CMS entry is published, a repo is pushed. A Route Handler is the natural receiver. Webhooks look simple, but the handler is a public endpoint that accepts input from the internet and often triggers money, data or deploy side effects, so the details matter.

> Verified against the Next.js 16.4 docs. Signature schemes differ per provider; always follow the provider's own documentation for the exact header names and signing format.

## What a safe webhook receiver does

```text
request ─► read RAW body ─► verify signature (+ timestamp) ─► parse
        ─► dedupe by event id ─► respond 2xx fast ─► do the work (async if slow)
```

| Step | Why |
|---|---|
| Read the **raw** body | The signature is computed over the exact bytes; re-serialized JSON will not match |
| Verify the signature | Proves the sender knows the shared secret. Without it, anyone can POST fake events |
| Check the timestamp | Stops replay of an old, valid request |
| Dedupe by event ID | Providers retry; you will see the same event more than once |
| Respond quickly with 2xx | Slow responses are treated as failures and retried |
| Return non-2xx only to request a retry | A `400` for a bad signature, a `5xx` for a temporary failure |

## A minimal receiver

```ts
// app/api/webhooks/example/route.ts  →  POST /api/webhooks/example
export async function POST(request: Request) {
  let payload: string;
  try {
    payload = await request.text();      // raw body
  } catch (error) {
    return new Response("Bad request", { status: 400 });
  }

  // verify, parse, process (see below)
  return new Response("OK", { status: 200 });
}
```

Unlike Pages Router API routes, Route Handlers have no body parser to turn off. `request.text()` gives you the raw body directly.

## Verifying a signature

Many providers sign the body with HMAC-SHA256 using a shared secret and send the result in a header. A generic implementation:

```ts
// lib/webhooks/verify.ts
import { createHmac, timingSafeEqual } from "node:crypto";

export function verifySignature(rawBody: string, signatureHex: string, secret: string) {
  const expected = createHmac("sha256", secret).update(rawBody, "utf8").digest();
  const received = Buffer.from(signatureHex, "hex");

  // lengths must match before timingSafeEqual, or it throws
  if (received.length !== expected.length) return false;
  return timingSafeEqual(expected, received);
}
```

```ts
// app/api/webhooks/example/route.ts
import { verifySignature } from "@/lib/webhooks/verify";

export async function POST(request: Request) {
  const rawBody = await request.text();
  const signature = request.headers.get("x-signature") ?? "";   // header name varies by provider

  if (!verifySignature(rawBody, signature, process.env.EXAMPLE_WEBHOOK_SECRET!)) {
    return new Response("Invalid signature", { status: 400 });
  }

  const event = JSON.parse(rawBody);   // parse only AFTER verifying
  // ...
  return new Response("OK");
}
```

Points that matter:

- Use `timingSafeEqual`, not `===`. A normal comparison can leak information through timing.
- Compute the HMAC over the **raw string**, not `JSON.stringify(await request.json())`.
- Providers often include a timestamp in the signed content. Reject events older than a few minutes to prevent replay.
- Keep the secret in an environment variable, never in the repo.

### Use the provider's SDK when there is one

SDKs implement the exact scheme (header format, tolerance, versioning). For Stripe:

```ts
import Stripe from "stripe";

const stripe = new Stripe(process.env.STRIPE_SECRET_KEY!);

export async function POST(request: Request) {
  const body = await request.text();                         // raw
  const signature = request.headers.get("stripe-signature");
  if (!signature) return new Response("Missing signature", { status: 400 });

  let event: Stripe.Event;
  try {
    event = stripe.webhooks.constructEvent(body, signature, process.env.STRIPE_WEBHOOK_SECRET!);
  } catch {
    return new Response("Invalid signature", { status: 400 });
  }

  switch (event.type) {
    case "checkout.session.completed":
      await handleCheckoutCompleted(event.data.object);
      break;
    default:
      // ignore events you did not subscribe to
      break;
  }

  return new Response("OK", { status: 200 });
}
```

Subscribe only to the events you handle, and return `200` for the rest so the provider does not keep retrying them.

## Idempotency: assume duplicates

Providers deliver **at least once**. The same event can arrive twice, and events can arrive out of order. Make processing safe to repeat:

```ts
// table: processed_events(id text primary key, received_at timestamp)
async function handleOnce(eventId: string, work: () => Promise<void>) {
  try {
    await db.processedEvent.create({ data: { id: eventId } });   // unique constraint
  } catch (e) {
    if (isUniqueViolation(e)) return;                            // already handled
    throw e;
  }
  await work();
}
```

Better still, make the work itself idempotent (`upsert` by external ID; "mark paid" rather than "add 10 credits"). If the work can fail after you record the event, record it in the same transaction as the work, or only mark it processed at the end.

Do not trust event ordering. Where the order matters, compare a version or timestamp in the payload, or re-fetch the object from the provider's API.

## Respond fast, work in the background

Providers time out in seconds. If the work is slow:

- Acknowledge with `200` immediately and queue a job (a queue, a table polled by a worker).
- Or run the follow-up after the response is sent with `after()` from `next/server`:

```ts
import { after } from "next/server";

export async function POST(request: Request) {
  const event = await verifyAndParse(request);
  if (!event) return new Response("Invalid", { status: 400 });

  after(async () => {
    await processEvent(event);   // runs after the response is sent
  });

  return new Response("OK");
}
```

On serverless hosts a function can be frozen shortly after responding, and background work may be cut off. For anything that must complete (payments, provisioning), use a durable queue rather than relying on `after()` alone. Check your host's behavior.

## Webhook that refreshes the cache (CMS)

A common use: when content changes in a CMS, invalidate the cached pages.

```ts
// app/api/revalidate/route.ts
import { revalidateTag } from "next/cache";
import { NextResponse, type NextRequest } from "next/server";
import { timingSafeEqual } from "node:crypto";

export async function POST(request: NextRequest) {
  const provided = Buffer.from(request.headers.get("x-revalidate-secret") ?? "");
  const expected = Buffer.from(process.env.REVALIDATE_SECRET ?? "");

  if (provided.length !== expected.length || !timingSafeEqual(provided, expected)) {
    return NextResponse.json({ success: false }, { status: 401 });
  }

  const { tag } = (await request.json()) as { tag?: string };
  if (!tag) return NextResponse.json({ success: false }, { status: 400 });

  revalidateTag(tag, "max");
  return NextResponse.json({ success: true });
}
```

- `revalidateTag(tag, "max")` marks the tag stale and refreshes it in the background. The one-argument form is deprecated. See [Revalidation](../06-caching/04-revalidation.md).
- A secret in a **header** is better than a query-string token (`?token=...`), because URLs are logged by proxies and CDNs. Use whichever your CMS supports, over HTTPS only.
- Revalidation from a Route Handler does not clear the visiting user's browser cache; that clears on the next navigation after the stale window. See [Router Cache](../06-caching/03-router-cache.md).

## Callback URLs (OAuth and payment returns)

A browser is redirected to your callback with a code or token. This is different from a server-to-server webhook:

```ts
// app/auth/callback/route.ts
import { NextResponse, type NextRequest } from "next/server";

export async function GET(request: NextRequest) {
  const code = request.nextUrl.searchParams.get("code");
  const next = request.nextUrl.searchParams.get("next") ?? "/";

  // prevent open redirects: allow only same-origin destinations
  const destination = new URL(next, request.url);
  if (destination.origin !== request.nextUrl.origin) {
    return new Response("Invalid redirect", { status: 400 });
  }

  // exchange `code` for a session on the server, verify `state`, then:
  const response = NextResponse.redirect(destination);
  response.cookies.set({ name: "session", value: "…", httpOnly: true, secure: true, sameSite: "lax", path: "/" });
  return response;
}
```

Validate any `state` parameter you issued, and never redirect to an arbitrary user-supplied URL.

## Local development

Providers cannot reach `localhost`. Options:

- The provider's CLI forwards events to your machine (for Stripe, `stripe listen --forward-to localhost:3000/api/webhooks/stripe`, which also prints a signing secret for local use).
- A tunnel (such as Cloudflare Tunnel or ngrok) gives you a public URL.
- Save real payloads to files and replay them with `curl` plus a locally computed signature to write tests.

Use **separate secrets** for development, staging and production.

## Debugging

| Symptom | Likely cause | Fix |
|---|---|---|
| "Signature verification failed" always | Verified a re-serialized body, or wrong secret | Use `request.text()`; check the secret per environment |
| Fails only in production | Body altered by a proxy, wrong secret, wrong endpoint secret | Compare raw bodies; check the env var |
| Same event handled twice | No idempotency | Dedupe by event ID |
| Provider shows timeouts and retries | Slow synchronous work | Ack first; queue the work |
| Endpoint returns `405` | Only `GET` exported | Export `POST` |
| Handler returns `404` | Wrong path or the file is not named `route.ts` | Check the folder structure |
| Events processed out of order | At-least-once, unordered delivery | Compare versions or re-fetch |
| Cached/redirected to login | A `proxy.ts` matcher covers the webhook path and blocks it | Exclude the webhook route from auth checks |

## Common mistakes

| Mistake | Fix |
|---|---|
| No signature check ("it's an obscure URL") | Always verify; the URL is not a secret |
| Verifying `JSON.stringify(parsed)` | Verify the raw string |
| `===` comparison of signatures | `timingSafeEqual` |
| Returning `500` for events you do not handle | Return `200` and ignore |
| Doing slow work before responding | Ack, then process |
| Assuming exactly-once delivery | Idempotent handlers |
| Auth middleware blocking the provider | Exclude webhook paths from user-auth proxy rules |
| Same secret across environments | One secret per environment |
| Logging full payloads with personal data | Log IDs and types |
| Open redirect on a callback URL | Validate the destination's origin |

## Quick Summary

- Webhook endpoints are public; the signature is your only authentication.
- Read the raw body, verify with HMAC and `timingSafeEqual` (or the provider SDK), check timestamps, then parse.
- Expect retries and reordering: dedupe by event ID and make work idempotent.
- Respond `2xx` quickly; move slow work to a queue (or `after()` where it is safe).
- Return `400` for bad signatures, `5xx` only when you want a retry.
- CMS refresh webhooks call `revalidateTag(tag, "max")` behind a secret.

## Next

- [File Upload](./04-file-upload.md)
- [Revalidation](../06-caching/04-revalidation.md)
- [Route Handlers](./00-route-handlers.md)
