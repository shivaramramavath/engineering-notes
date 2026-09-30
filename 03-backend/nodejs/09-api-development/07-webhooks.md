# Webhooks

HTTP callbacks that let one system notify another the moment something happens — instead of the other system polling to ask.

## Polling vs webhooks

```
Polling                                   Webhook
───────                                   ───────
Client: "Any new payments?"  → none       Payment succeeds
Client: "Any new payments?"  → none       Provider → POST https://yourapp.com/webhooks/stripe
Client: "Any new payments?"  → none                   { "type": "payment.succeeded", ... }
Client: "Any new payments?"  → 1 new!     Your app reacts immediately
```

| | Polling | Webhooks |
|---|---|---|
| Latency | As slow as the poll interval | Near real time |
| Wasted requests | Most polls return nothing | Only fires when something happens |
| Who initiates | Consumer | Producer |
| Complexity | Low | Higher: security, retries, ordering |

Webhooks are a **push** model: the provider makes an HTTP `POST` to a URL **you** registered. You'll meet them in payments (Stripe, Razorpay), source control (GitHub), messaging (Twilio), e-commerce (Shopify), and CI/CD.

There are two roles, each with its own challenges:

1. **Receiving** webhooks from someone else (Part 1)
2. **Sending** webhooks to your own API's customers (Part 2)

---

# Part 1 — Receiving Webhooks

## The webhook endpoint is a public, unauthenticated door

Anyone on the internet can `POST` to your webhook URL. So the endpoint must:

1. **Verify** the request really came from the provider (signature check)
2. **Respond fast** (usually within a few seconds) with a `2xx`
3. **Deduplicate** (providers retry, so you'll see the same event more than once)
4. **Process asynchronously** (queue the work instead of doing it inline)

---

## 1. Verify the signature (HMAC)

Providers sign each payload with a secret shared between you and them. You recompute the signature and compare.

```
Provider computes:  signature = HMAC-SHA256(secret, timestamp + "." + rawBody)
Provider sends:     Webhook-Signature: t=1759227300,v1=5257a869e7ecebeda32affa62cdca3fa...
You recompute and compare → mismatch means reject
```

### The raw body problem

The signature is computed over the **exact bytes** the provider sent. If `express.json()` parses the body first and you re-serialize it (`JSON.stringify(req.body)`), whitespace and key order can differ and the signature won't match. You need the **raw body**.

```js
import express from "express";

const app = express();

// Webhook route with RAW body — mounted BEFORE the global express.json()
app.post(
  "/webhooks/provider",
  express.raw({ type: "application/json", limit: "1mb" }),    // req.body is a Buffer
  webhookHandler
);

app.use(express.json());      // everything else gets parsed JSON
```

Order matters: if `express.json()` runs first for that path, the raw bytes are gone. Alternatively, capture the raw body with `express.json({ verify: (req, res, buf) => { req.rawBody = buf; } })`.

### Verification code

```js
import crypto from "node:crypto";

function verifySignature({ rawBody, header, secret, toleranceSeconds = 300 }) {
  // header format: "t=1759227300,v1=abcdef..."
  const parts = Object.fromEntries(header.split(",").map((p) => p.split("=")));
  const timestamp = Number(parts.t);
  const received = parts.v1;

  if (!timestamp || !received) return false;

  // 1. Reject old requests → blocks replay attacks
  const ageSeconds = Math.abs(Date.now() / 1000 - timestamp);
  if (ageSeconds > toleranceSeconds) return false;

  // 2. Recompute the expected signature
  const expected = crypto
    .createHmac("sha256", secret)
    .update(`${timestamp}.${rawBody.toString("utf8")}`)
    .digest("hex");

  // 3. Constant-time comparison (avoids timing attacks)
  const a = Buffer.from(expected);
  const b = Buffer.from(received);
  return a.length === b.length && crypto.timingSafeEqual(a, b);
}
```

Key points:

- Use `crypto.timingSafeEqual`, never `===` (`08-authentication-security/05-common-vulnerabilities.md`, section 13).
- **Check the timestamp.** A valid captured request can otherwise be replayed by an attacker days later. The signed timestamp prevents that.
- **Every provider differs** in header name and signing recipe (GitHub uses `X-Hub-Signature-256` with `sha256=<hex>` over the body alone; Stripe includes a timestamp as above). Follow the provider's docs, and prefer their official SDK helper where one exists (for example Stripe's `webhooks.constructEvent`).
- Store the webhook secret in an environment variable, never in Git (`16-production/01-environment-management.md`).

Use the verification in the handler:

```js
async function webhookHandler(req, res) {
  const valid = verifySignature({
    rawBody: req.body,                                        // Buffer from express.raw
    header: req.get("Webhook-Signature") ?? "",
    secret: process.env.WEBHOOK_SECRET,
  });

  if (!valid) return res.sendStatus(401);

  const event = JSON.parse(req.body.toString("utf8"));
  // ... dedupe + enqueue (next sections)
  res.sendStatus(200);
}
```

Some providers also offer **IP allow-lists** or **mutual TLS**. Treat those as extra layers, not replacements for signature verification, since IP ranges change and get spoofed in misconfigured proxies.

---

## 2. Acknowledge fast, process later

Providers expect a quick response. If you take too long (often 5–30 seconds), they assume failure and retry, which piles duplicate deliveries on top of a slow system.

```js
// ❌ doing the work inline: slow, and a failure means a retry storm
app.post("/webhooks/stripe", express.raw({ type: "*/*" }), async (req, res) => {
  const event = parse(req.body);
  await sendInvoiceEmail(event);          // slow
  await updateAccounting(event);          // slow, may fail
  await provisionAccount(event);          // slow
  res.sendStatus(200);
});
```

```js
// ✅ verify → store/enqueue → respond immediately
app.post("/webhooks/stripe", express.raw({ type: "application/json" }), async (req, res) => {
  if (!verifySignature(/* ... */)) return res.sendStatus(401);

  const event = JSON.parse(req.body.toString("utf8"));

  await webhookQueue.add("stripe-event", event, {
    jobId: event.id,                 // dedupe at the queue level
    attempts: 5,
    backoff: { type: "exponential", delay: 2000 },
  });

  res.sendStatus(200);               // ack in milliseconds
});
```

A worker then processes the job with retries and a dead-letter queue (`11-async-processing/01-queues-and-bullmq.md`, `11-async-processing/02-workers-retry-dlq.md`).

**Response code semantics** (most providers follow this):

| You return | Provider interprets |
|---|---|
| `2xx` | Delivered — don't retry |
| `4xx` (like `400`/`401`) | Your problem — usually **no retry** (or limited) |
| `5xx` / timeout | Temporary failure — **retry** with backoff |

Return `401` for bad signatures, `200` once safely stored, and `5xx` only if you genuinely couldn't persist the event (so the provider retries).

---

## 3. Deduplicate: handle events exactly once (effectively)

Delivery is **at-least-once**. The same event can arrive twice because of provider retries, your slow response, or network hiccups. Every event has a unique ID; record it.

```sql
CREATE TABLE webhook_events (
  id           TEXT PRIMARY KEY,            -- the provider's event ID
  provider     TEXT NOT NULL,
  type         TEXT NOT NULL,
  payload      JSONB NOT NULL,
  received_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  processed_at TIMESTAMPTZ
);
```

```js
async function recordEvent(provider, event) {
  const result = await pool.query(
    `INSERT INTO webhook_events (id, provider, type, payload)
     VALUES ($1, $2, $3, $4)
     ON CONFLICT (id) DO NOTHING
     RETURNING id`,
    [event.id, provider, event.type, event]
  );
  return result.rowCount === 1;              // true = new event, false = duplicate
}

// in the handler
const isNew = await recordEvent("stripe", event);
if (isNew) await webhookQueue.add("process", { eventId: event.id });
res.sendStatus(200);                          // ALWAYS 200 for duplicates, too
```

Storing the raw payload also gives you an **audit log** and lets you **replay** events after fixing a bug. The same principle as in `06-idempotency.md`: the unique constraint is what guarantees correctness.

---

## 4. Don't assume ordering

Events may arrive out of order: `order.updated` can land before `order.created`, or `payment.failed` after `payment.succeeded`.

Strategies:

- Make handlers **state-based, not sequence-based**: on any event, fetch the **current** object from the provider's API and reconcile, instead of trusting the payload's snapshot.
- Compare **timestamps or version numbers**, ignoring events older than what you've already applied.
- Design handlers to tolerate missing prerequisites (retry later, or create a placeholder).

```js
async function handleOrderEvent(event) {
  // The payload may be stale or out of order — ask the source of truth
  const order = await providerApi.orders.retrieve(event.data.id);
  await db.orders.upsert({ id: order.id, status: order.status, updatedAt: order.updated_at });
}
```

This is also a defense against forged payloads: even if someone tricks your endpoint, you only act on what the provider's authenticated API says.

---

## 5. Observe and recover

- **Log** every delivery: event ID, type, signature result, outcome, duration (`14-logging-observability/`). Never log the secret or the full signature header.
- **Alert** on spikes in signature failures (someone probing you) or on the dead-letter queue growing.
- **Reconcile periodically.** Webhooks can be lost permanently (your downtime outlasted their retry window). A nightly job that compares your records against the provider's API catches gaps.
- Most providers have a dashboard to **resend** events; your stored payloads let you replay locally too.

### Local development

Your laptop has no public URL. Use a tunnel (ngrok, Cloudflare Tunnel) or the provider's CLI (for example `stripe listen --forward-to localhost:3000/webhooks/stripe`) to forward real events to your dev server.

---

# Part 2 — Sending Webhooks

When *your* API lets customers subscribe to events (`invoice.paid`, `user.created`), you become the provider, and all the problems above flip to your side.

## The architecture

```
Business event ──▶ events table ──▶ [ queue ] ──▶ delivery worker ──▶ customer endpoint
                        │                              │   ▲
                        │                              ▼   │ retry with backoff
                        └── delivery attempts log ◀────┘   └─ dead-letter after N failures
```

1. Something happens in your app → write an **event** record.
2. For each subscriber of that event type → enqueue a **delivery** job.
3. A worker `POST`s the payload, records the result, and retries on failure.

Never call customer URLs inline inside a user-facing request: a slow or hanging endpoint would freeze your API.

## Data model

```sql
CREATE TABLE webhook_endpoints (
  id          UUID PRIMARY KEY,
  account_id  UUID NOT NULL,
  url         TEXT NOT NULL,
  secret      TEXT NOT NULL,                  -- per-endpoint signing secret
  events      TEXT[] NOT NULL,                -- subscribed event types, e.g. {'invoice.paid'}
  enabled     BOOLEAN NOT NULL DEFAULT true
);

CREATE TABLE webhook_deliveries (
  id              UUID PRIMARY KEY,
  endpoint_id     UUID NOT NULL REFERENCES webhook_endpoints(id),
  event_id        UUID NOT NULL,
  attempt         INTEGER NOT NULL DEFAULT 0,
  status          TEXT NOT NULL,              -- pending | delivered | failed
  response_status INTEGER,
  last_error      TEXT,
  next_attempt_at TIMESTAMPTZ,
  UNIQUE (endpoint_id, event_id)
);
```

## Payload design

```jsonc
{
  "id": "evt_01J8Z3X9",                        // unique → receivers dedupe on this
  "type": "invoice.paid",
  "apiVersion": "2026-09-01",                  // payload schema version
  "createdAt": "2026-09-30T10:15:00.000Z",
  "data": {
    "id": "inv_42",
    "amount": 5000,
    "currency": "USD",
    "status": "paid"
  }
}
```

- Give **every event a unique, stable ID** and reuse it for retries.
- Include `type` and `createdAt` so receivers can route and order.
- **Version your payloads** (`02-versioning-and-pagination.md`): webhook schemas are contracts too.
- Choose "thin" (IDs only; receiver calls your API for details) or "fat" (full object) deliberately. Thin events are safer for ordering and staleness; fat events save a round trip.

## Sign every delivery

```js
import crypto from "node:crypto";

function signPayload(secret, body) {
  const timestamp = Math.floor(Date.now() / 1000);
  const signature = crypto
    .createHmac("sha256", secret)
    .update(`${timestamp}.${body}`)
    .digest("hex");
  return { timestamp, header: `t=${timestamp},v1=${signature}` };
}
```

Use a **unique secret per endpoint**, let customers **rotate** secrets (accept both the old and new signature during a grace period), and document the exact verification recipe with code samples.

## The delivery worker

```js
import { Worker } from "bullmq";

new Worker("webhook-delivery", async (job) => {
  const { deliveryId } = job.data;
  const delivery = await loadDelivery(deliveryId);           // joins endpoint + event
  const body = JSON.stringify(delivery.event);
  const { header } = signPayload(delivery.endpoint.secret, body);

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 10_000);   // never wait forever

  try {
    const res = await fetch(delivery.endpoint.url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "User-Agent": "MyApp-Webhooks/1.0",
        "Webhook-Id": delivery.event.id,
        "Webhook-Signature": header,
      },
      body,
      signal: controller.signal,
      redirect: "error",                                     // don't follow redirects
    });

    await recordAttempt(deliveryId, res.status);

    if (res.status >= 200 && res.status < 300) return;       // success
    throw new Error(`Endpoint responded ${res.status}`);     // triggers retry
  } catch (err) {
    await recordAttempt(deliveryId, null, err.message);
    throw err;                                               // let BullMQ retry with backoff
  } finally {
    clearTimeout(timeout);
  }
}, {
  connection,
  concurrency: 20,
});
```

Enqueueing with retries:

```js
await deliveryQueue.add("deliver", { deliveryId }, {
  attempts: 8,
  backoff: { type: "exponential", delay: 30_000 },           // 30s, 60s, 2m, 4m, ... 
  removeOnComplete: true,
});
```

Typical retry schedule: seconds → minutes → hours, spread over 1–3 days, then **give up** and mark the delivery failed.

### Auto-disable broken endpoints

If an endpoint fails continuously (say 3+ days), disable it and email the account owner. Otherwise you'll waste resources retrying a dead URL forever.

## Security when *you* call customer URLs: SSRF

Customers supply the URL, so a malicious one can point at `http://169.254.169.254/` (cloud metadata), `http://localhost:6379` (your Redis), or private network addresses. See `08-authentication-security/05-common-vulnerabilities.md`, section 8.

```js
import dns from "node:dns/promises";
import net from "node:net";

function isPrivateIp(ip) {
  return (
    /^10\./.test(ip) ||
    /^127\./.test(ip) ||
    /^192\.168\./.test(ip) ||
    /^172\.(1[6-9]|2\d|3[01])\./.test(ip) ||
    /^169\.254\./.test(ip) ||
    ip === "::1" || ip.startsWith("fc") || ip.startsWith("fd") || ip.startsWith("fe80")
  );
}

async function assertSafeUrl(urlString) {
  const url = new URL(urlString);
  if (url.protocol !== "https:") throw new Error("HTTPS required");

  const addresses = net.isIP(url.hostname)
    ? [{ address: url.hostname }]
    : await dns.lookup(url.hostname, { all: true });

  if (addresses.some((a) => isPrivateIp(a.address))) {
    throw new Error("Private addresses are not allowed");
  }
}
```

Run this check **when registering** the endpoint *and* **before every delivery** — DNS can change after registration (DNS rebinding). For strong protection, connect to the already-validated IP, or route delivery through an isolated egress proxy that blocks internal ranges. Also: require HTTPS, disable redirects, set timeouts, cap response size, and ignore the response body.

## Give customers tooling

A webhook system customers can't debug gets support tickets. Good products provide:

- A dashboard of recent **deliveries** with status, response code, timestamps, and payloads
- A **"resend"** button and a **"send test event"** action
- Delivery logs retained for ~30 days
- Clear docs: event catalog, payload examples, verification code, retry policy, IP ranges (if any)

---

## Receiving vs sending: quick comparison

| Concern | As receiver | As sender |
|---|---|---|
| Authenticity | **Verify** HMAC signature + timestamp | **Sign** every payload; per-endpoint secrets |
| Duplicates | Dedupe by event ID | Give events stable unique IDs |
| Speed | Ack in milliseconds, process via queue | Deliver via queue; 10 s timeout |
| Failures | Return `5xx` only if you couldn't persist | Retry with exponential backoff; dead-letter; auto-disable |
| Ordering | Don't rely on it; re-fetch current state | Document that it isn't guaranteed |
| Main security risk | Forged requests, replay | **SSRF** when calling customer URLs |
| Observability | Log + alert on signature failures | Delivery dashboard, resend, test events |

---

## Common mistakes

```js
// ❌ skipping signature verification ("it's just an internal URL")
// ❌ comparing signatures with === instead of timingSafeEqual
// ❌ parsing JSON before verifying → signature computed over re-serialized body
// ❌ no timestamp tolerance → replay attacks
// ❌ doing slow work before responding → provider timeouts and retry storms
// ❌ returning 500 for duplicate events (should be 200)
// ❌ assuming exactly-once delivery or in-order delivery
// ❌ trusting the payload as truth instead of re-fetching from the provider
// ❌ (sender) calling customer URLs inside the user's request cycle
// ❌ (sender) no timeout, no SSRF protection, following redirects
// ❌ (sender) one shared secret for all customers
// ❌ (sender) retrying forever with no auto-disable
```

## Wrap-up

This completes `09-api-development/`. You've now covered the lifecycle of a well-behaved API: sensible resource design (`01`), evolution and scale (`02`, `03`), safe input handling (`04`), predictable failures (`05`), safe retries (`06`), and event-driven integration (`07`).

## Next

Section **`10-architecture/`** — with API conventions in place, move up a level to how you structure the code behind them: layered architecture, clean architecture, repositories, dependency injection, and when to split a monolith.
