# Notification System

A platform that delivers messages to users across channels — **email, SMS, push, in-app** — triggered by events elsewhere in the product (order shipped, password reset, new follower, payment failed). The interesting problems: **not blocking the caller**, **retries and provider failures**, **never sending duplicates**, **respecting user preferences**, and **not melting a provider's rate limits**.

It leans on `11-async-processing/` (queues, workers, retry, DLQ) and `06-job-processing-system.md`.

## Requirements

**Functional**
- Send notifications over multiple channels: email, SMS, mobile push, in-app
- Triggered by internal services (events or API calls)
- Template-based content with per-user data and localization
- Respect user preferences (channel opt-in/out, quiet hours) and legal opt-outs
- Support scheduled and batched ("daily digest") notifications

**Non-functional**
- Producers must not wait for delivery — callers get an immediate acknowledgement
- **Reliable**: transient failures are retried; permanent failures are recorded
- **No duplicates**: a user must not receive the same password-reset email twice because of a retry
- **Priority**: a password reset must not queue behind a million marketing emails
- Handle bursts (a flash sale triggers a spike)

**Out of scope:** the content authoring UI, deep analytics, building our own email/SMS delivery infrastructure (we use third-party providers).

## Scale estimate

Assumptions: 10 million users, ~5 notifications/user/day.

| Quantity | Calculation | Result |
|----------|-------------|--------|
| Notifications/day | 10M × 5 | 50 million |
| Average rate | 50M ÷ 86,400 | ~580/sec |
| Burst (campaign) | | Tens of thousands/sec briefly |

Because bursts far exceed what providers accept per second, we need a **buffer (queue)** between "decided to notify" and "handed to provider".

## High-level design

```
 Services ──► ┌─────────────────────┐
 (orders,     │ Notification API /  │   validate, check preferences,
  auth, ...)  │ event consumer      │   dedupe, render, enqueue
              └─────────┬───────────┘
                        │
          ┌─────────────┼─────────────┬──────────────┐
          ▼             ▼             ▼              ▼
     ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌──────────┐
     │ email   │   │  sms    │   │  push   │   │ in-app   │   one queue
     │ queue   │   │ queue   │   │ queue   │   │ (WS/DB)  │   per channel
     └────┬────┘   └────┬────┘   └────┬────┘   └──────────┘
          ▼             ▼             ▼
     ┌─────────┐   ┌─────────┐   ┌─────────┐
     │ email   │   │  sms    │   │  push   │   workers: rate-limited,
     │ workers │   │ workers │   │ workers │   retry w/ backoff
     └────┬────┘   └────┬────┘   └────┬────┘
          ▼             ▼             ▼
      SendGrid/SES    Twilio       FCM / APNs       third-party providers
          │             │             │
          └──────► delivery webhooks (bounced, delivered, failed) ──► status store
```

**Why one queue per channel?** Each channel has different providers, rate limits, latency, and failure modes. An SMS provider outage must not stall email. Separate queues isolate failures and let you scale each worker pool independently (`11-async-processing/02-workers-retry-dlq.md`).

**Why a queue at all?** The producing service (say, the order service) must not wait on SendGrid or fail because Twilio is down. It enqueues and moves on — `202 Accepted`, not "delivered".

## The API

```
POST /api/notifications
  Idempotency-Key: order-1234-shipped
  body: {
    "userId": "u_42",
    "type": "order.shipped",
    "data": { "orderId": "1234", "trackingUrl": "..." },
    "priority": "normal"
  }
  returns: 202 { "notificationId": "n_abc" }
```

Note the **type + data** shape rather than raw message text: callers say *what happened*, and the notification service decides *how to say it*, in which language, over which channels. That keeps wording and channel logic in one place.

## Processing pipeline

```js
async function handleNotificationRequest(req) {
  const { userId, type, data, priority = "normal" } = req;

  // 1. Deduplicate producer retries
  const isNew = await redis.set(`notif:dedupe:${req.idempotencyKey}`, "1", { NX: true, EX: 86_400 });
  if (!isNew) return { status: "duplicate" };

  // 2. Load user contact info + preferences
  const user = await users.get(userId);
  const prefs = await preferences.get(userId, type);

  // 3. Decide channels: type defaults ∩ user opt-ins ∩ allowed right now
  const channels = chooseChannels(type, prefs, user);   // e.g. ["push", "email"]

  // 4. For each channel, render + enqueue
  for (const channel of channels) {
    const content = await renderTemplate(type, channel, user.locale, data);
    await queues[channel].add(
      "send",
      { notificationId, userId, channel, content },
      {
        jobId: `${notificationId}:${channel}`,   // idempotent enqueue
        priority: PRIORITY[priority],
        attempts: 5,
        backoff: { type: "exponential", delay: 2000 },
      }
    );
  }
  return { status: "queued" };
}
```

(Job options shown follow BullMQ's style — see `11-async-processing/01-queues-and-bullmq.md` and check your version's docs for exact option names.)

## Reliability: retries, idempotency, and duplicates

Delivery is **at-least-once**: a worker might call the provider, succeed, then crash before recording success — so the job is retried and the user gets the message twice. You cannot fully eliminate this window, but you can shrink and manage it:

1. **Idempotency at the entry** — the `Idempotency-Key` stops *producers* from creating duplicate notifications (`09-api-development/06-idempotency.md`)
2. **Idempotent job IDs** — the same `(notificationId, channel)` can't be enqueued twice
3. **Provider-side idempotency** — many providers accept an idempotency key or client reference so the *provider* ignores duplicate sends; pass `notificationId` where supported
4. **Record status in a store** and check before sending:

```js
async function sendEmailJob(job) {
  const { notificationId, userId, content } = job.data;

  const state = await deliveries.get(notificationId, "email");
  if (state?.status === "sent") return;                  // already delivered; skip

  try {
    const res = await emailProvider.send({ to: ..., ...content, reference: notificationId });
    await deliveries.mark(notificationId, "email", "sent", res.id);
  } catch (err) {
    if (isPermanent(err)) {                              // bad address, blocked, invalid template
      await deliveries.mark(notificationId, "email", "failed", err.message);
      return;                                            // do NOT retry forever
    }
    throw err;                                           // transient (timeout, 5xx, 429) → retry with backoff
  }
}
```

**Classify errors.** Transient (network timeout, provider 5xx, rate limit) → retry with exponential backoff and jitter. Permanent (invalid email, unsubscribed, malformed payload) → fail immediately and record it. Retrying a permanent error just wastes capacity.

**Dead-letter queue.** After the maximum attempts, move the job to a DLQ for inspection and manual or automated replay (`11-async-processing/02-workers-retry-dlq.md`). Alert when the DLQ grows.

**Provider failover.** For critical channels (password reset email), keep a secondary provider and switch when the primary's error rate spikes. This is the **Adapter** pattern in practice — both providers sit behind one interface (`18-design-patterns/03-adapter-and-decorator.md`). Be careful: failing over mid-send can itself cause duplicates.

## Priorities

A security code that expires in 5 minutes cannot wait behind a marketing blast.

- Use **separate queues (or priority levels) per urgency**: transactional/critical vs bulk/marketing
- Give critical queues dedicated workers so a campaign can't starve them
- Bulk sends are **throttled** and can run off-peak

## Rate limiting outbound traffic

Providers cap requests per second (and mailbox providers punish sudden volume spikes from a sender). Workers must respect that:

```js
const worker = new Worker("email", sendEmailJob, {
  connection,
  concurrency: 20,
  limiter: { max: 100, duration: 1000 },   // ≤100 jobs/sec for this queue
});
```

The queue absorbs a burst; workers drain it at a rate the provider tolerates. Also rate limit **per user** so one bug or event storm can't send someone 500 notifications — and so legitimate batching ("you have 12 new likes") replaces a flood.

## User preferences and compliance

A correctness feature, not just a nicety:

```
preferences
  user_id, notification_type, channel, enabled
  quiet_hours_start, quiet_hours_end, timezone
```

- Check preferences **at send time** (a user may opt out between enqueue and delivery)
- **Quiet hours:** delay non-urgent notifications rather than dropping them
- **Unsubscribe is mandatory** for marketing email in many jurisdictions; transactional messages (receipts, password resets) are treated differently. Rules vary by region — confirm requirements for your markets rather than assuming
- **Suppression list:** hard bounces and spam complaints must be recorded and honored — continuing to email bad addresses damages your sender reputation
- Consent matters especially for SMS and push; keep records of opt-in

## Templates and localization

- Store templates per `(type, channel, locale)` with variables (`Hi {{name}}, your order {{orderId}} shipped`)
- **Escape variables** in HTML emails — user-supplied values rendered unescaped are an injection risk (`08-authentication-security/05-common-vulnerabilities.md`)
- Render in the **worker or pipeline**, not in the calling service
- Version templates so a change doesn't alter in-flight messages unpredictably

## Scheduled and digest notifications

- **Scheduled:** enqueue with a delay, or use a scheduler that enqueues at a time (`11-async-processing/03-scheduled-jobs.md`, `06-job-processing-system.md`)
- **Digests:** accumulate events per user in Redis/DB, and a scheduled job collapses them into one message. Fewer, better-batched notifications also beat spamming users.

## Delivery tracking

Providers report outcomes asynchronously — **webhooks** for delivered, bounced, opened, complained (`09-api-development/07-webhooks.md`):

```
provider ──webhook──► /webhooks/email ──► verify signature ──► update delivery status
```

- **Verify webhook signatures** — otherwise anyone can forge "bounce" events
- Keep a `deliveries` table: `notification_id, channel, status (queued/sent/delivered/failed/bounced), provider_id, attempts, timestamps`
- Surface this for support ("did the user get the reset email?") and feed bounces into the suppression list

## In-app notifications

These need no external provider: store a row in the database and push live over the user's socket if they're online (`03-chat-system.md`, `12-realtime/`). The unread count and list come from the database; the socket is only a "something new" signal.

## Push-specific notes

- Device tokens expire and change; treat "token invalid/unregistered" responses from FCM/APNs as a signal to **delete that token**
- A user may have several devices; send to each, and avoid double-notifying someone who's active on screen
- Payload size limits are small — send an identifier and let the app fetch details

## Failure scenarios

| Failure | Behavior |
|---------|----------|
| Provider outage | Jobs retry with backoff; queue buffers; failover provider for critical types |
| Worker crash mid-job | Job becomes visible again and is retried (hence idempotency) |
| Queue/Redis down | Producers need a fallback (e.g. an outbox table, below) |
| Poison message that always fails | Capped retries → DLQ → alert, doesn't block the queue |
| Burst traffic | Queue absorbs; workers drain at the allowed rate |

**Outbox pattern.** If the business action (creating an order) and the notification enqueue are separate steps, a crash between them loses the notification. Writing the notification request in the *same database transaction* as the business change, and having a relay publish it afterward, makes them atomic (`07-databases/postgresql/02-transactions-and-indexing.md`).

## Trade-offs summary

| Decision | Trade-off |
|----------|-----------|
| Queue per channel | Isolation and tuning vs more infrastructure |
| At-least-once delivery | Never lose a message vs possible duplicates (managed via idempotency) |
| Async hand-off | Producer speed and resilience vs no instant "delivered" answer |
| Check preferences at send time | Respects late opt-outs vs extra lookups |
| Provider failover | Availability vs complexity and duplicate risk |

## Common mistakes

- **Sending notifications inline in the request handler** — a slow provider slows or fails the user's action.
- **A single shared queue for all channels and priorities** — one outage or campaign blocks everything, including critical messages.
- **Retrying permanent errors** — wastes capacity and can hurt sender reputation.
- **No idempotency anywhere** — retries and producer re-sends cause duplicate messages.
- **Ignoring provider rate limits** — throttling and blocklisting.
- **Skipping unsubscribe/suppression handling** — compliance and deliverability problems.
- **Not verifying delivery webhooks** — forged status updates.
- **Unescaped template variables** — injection into HTML emails.
- **Never cleaning invalid push tokens** — wasted sends and error noise.
- **No DLQ monitoring** — failures pile up silently.

## Quick summary

- Producers send **type + data** to a notification service that decides channels, content, and timing
- A **queue per channel** (and per priority) isolates failures and lets workers be rate-limited
- Delivery is **at-least-once** — use idempotency keys, idempotent job IDs, and recorded status to avoid duplicates
- Retry **transient** errors with backoff; fail **permanent** ones fast; park exhausted jobs in a **DLQ**
- Honor preferences, quiet hours, unsubscribes, and suppression lists
- Track outcomes through **verified provider webhooks**
- Use an **outbox** to avoid losing notifications when the queue or process fails mid-way

## Next

**`05-file-upload-system.md`** designs large file uploads and downloads without routing every byte through your API servers.
