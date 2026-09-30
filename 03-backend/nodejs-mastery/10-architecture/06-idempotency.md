# Idempotency

Making it safe for a client to send the same request more than once — because on a real network, they will.

## The problem

```
Client                          Server
  │  POST /payments  ($50)        │
  │ ─────────────────────────────▶│  charge the card ✅
  │                               │  save to database ✅
  │      ◀── 201 Created ─────X   │  (response lost: timeout, dropped connection, app crash)
  │                               │
  │  (no response... retry?)      │
  │  POST /payments  ($50)        │
  │ ─────────────────────────────▶│  charge the card AGAIN 💸
```

The client can't tell whether the first request failed before, during, or after the work. Its only safe option is to retry — and without protection, every retry may create a duplicate payment, order, email, or record.

Retries also come from places you don't control: mobile apps on flaky connections, HTTP client libraries with automatic retries, load balancers, API gateways, queue workers that redeliver on failure (`11-async-processing/02-workers-retry-dlq.md`), and users double-clicking a button.

---

## What idempotent means

An operation is **idempotent** if performing it many times has the same effect as performing it once.

| Method | Idempotent by definition? | Why |
|---|---|---|
| `GET`, `HEAD`, `OPTIONS` | ✅ | Reads don't change state |
| `PUT` | ✅ | Setting a resource to state X twice leaves it at X |
| `DELETE` | ✅ | Deleting something already deleted leaves the same end state |
| `POST` | ❌ | Each call typically creates something new |
| `PATCH` | Depends | `{ "status": "shipped" }` is idempotent; `{ "increment": 1 }` is not |

The "same effect" refers to **server state**, not identical responses. A second `DELETE` may return `404` instead of `204` and still be idempotent.

Design-level idempotency is the first tool:

```js
// ❌ not idempotent: retrying adds $10 every time
POST /accounts/7/deposit   { "amount": 1000 }

// ✅ idempotent: retrying sets the same value
PUT /accounts/7/limit      { "dailyLimit": 5000 }

// ✅ naturally idempotent with a client-supplied ID: a retry hits the same resource
PUT /orders/2f9c0a4e-...   { ...order... }
```

Letting the **client generate the resource ID** (a UUID) and using `PUT` makes creation idempotent for free. When that doesn't fit, use an idempotency key.

---

## The Idempotency-Key pattern

Popularized by Stripe and now standardized as an IETF draft. The client attaches a unique key to a `POST`; the server remembers the result under that key and replays it on retries.

```
POST /api/v1/payments
Idempotency-Key: 8e03978e-40d5-43e8-bc93-6894a57f9324
Content-Type: application/json

{ "amount": 5000, "currency": "USD", "customerId": "cus_42" }
```

Server behavior:

| Situation | Response |
|---|---|
| First time seeing the key | Process the request, store the result, return it |
| Same key, same request, **already completed** | Return the **stored response** (same status and body) — do not run the work again |
| Same key, **still processing** | `409 Conflict` (`request_in_progress`) — the client retries shortly |
| Same key, **different** request body | `422` / `409` (`idempotency_key_reused`) — the client is misusing keys |
| No key on an endpoint that requires one | `400` |

Client rules:

- Generate a **random UUID (v4)** per *logical operation* — not per attempt.
- Reuse the **same key** for every retry of that operation.
- Generate a **new key** for a genuinely new operation.

---

## Implementation with Redis

Storing state in Redis is fast and gives you automatic expiry (keys needn't live forever — 24 hours is typical). You need an **atomic** way to say "I'm the first to claim this key," which Redis gives you with `SET ... NX`.

### The data model

```
idem:{userId}:{key}  →  JSON { state, fingerprint, status, body }
```

Scope keys **per user** (or per API key). Otherwise one client could replay another client's responses by guessing keys.

### The middleware

```js
// middleware/idempotency.js
import crypto from "node:crypto";
import { redis } from "../lib/redis.js";
import { AppError } from "../utils/AppError.js";

const TTL_SECONDS = 24 * 60 * 60;
const LOCK_TTL_SECONDS = 60;                     // max time we'll wait on an in-flight request

const fingerprint = (req) =>
  crypto
    .createHash("sha256")
    .update(`${req.method}:${req.originalUrl}:${JSON.stringify(req.body ?? {})}`)
    .digest("hex");

export function idempotent({ required = true } = {}) {
  return async (req, res, next) => {
    const key = req.get("Idempotency-Key");

    if (!key) {
      if (required) {
        return next(new AppError(400, "idempotency_key_required",
          "This endpoint requires an Idempotency-Key header"));
      }
      return next();
    }
    if (key.length < 16 || key.length > 128) {
      return next(new AppError(400, "invalid_idempotency_key",
        "Idempotency-Key must be 16–128 characters"));
    }

    const redisKey = `idem:${req.user.id}:${key}`;
    const fp = fingerprint(req);

    // Atomically claim the key. Only ONE concurrent request wins.
    const claimed = await redis.set(
      redisKey,
      JSON.stringify({ state: "processing", fingerprint: fp }),
      { NX: true, EX: LOCK_TTL_SECONDS }
    );

    if (!claimed) {
      // Key exists — inspect it
      const existing = JSON.parse(await redis.get(redisKey));

      if (existing.fingerprint !== fp) {
        return next(new AppError(422, "idempotency_key_reused",
          "This Idempotency-Key was already used with a different request"));
      }
      if (existing.state === "processing") {
        res.set("Retry-After", "1");
        return next(new AppError(409, "request_in_progress",
          "A request with this Idempotency-Key is still being processed"));
      }

      // Completed: replay the stored response exactly
      res.set("Idempotent-Replayed", "true");
      return res.status(existing.status).json(existing.body);
    }

    // We own the key. Capture the response so we can store it.
    const originalJson = res.json.bind(res);
    res.json = (body) => {
      // Don't cache server errors — a retry should get a fresh attempt
      if (res.statusCode < 500) {
        redis
          .set(
            redisKey,
            JSON.stringify({ state: "completed", fingerprint: fp, status: res.statusCode, body }),
            { EX: TTL_SECONDS }
          )
          .catch((err) => req.log?.error({ err }, "failed to store idempotent response"));
      } else {
        redis.del(redisKey).catch(() => {});      // release the lock so the client can retry
      }
      return originalJson(body);
    };

    // If the client disconnects or the handler crashes, release the lock
    res.on("close", () => {
      if (!res.writableEnded) redis.del(redisKey).catch(() => {});
    });

    next();
  };
}
```

### Using it

```js
router.post("/payments", requireAuth, idempotent(), validate({ body: createPaymentSchema }), payments.create);
router.post("/orders",   requireAuth, idempotent(), orders.create);
```

### Why each piece exists

| Piece | Reason |
|---|---|
| `SET ... NX` | Atomic check-and-claim: two simultaneous retries can't both run |
| `EX` on the lock | If the server crashes mid-request, the lock expires instead of blocking forever |
| Request **fingerprint** | Detects reuse of a key with a different payload (a client bug) |
| `req.user.id` in the key | Isolates users from each other |
| Replay stored response | Client receives the same outcome as the first attempt |
| Don't store `5xx` | A transient failure shouldn't be replayed forever |
| 24 h TTL | Bounded storage; long enough to cover realistic retry windows |

Redis basics are in `07-databases/redis/01-basics-and-data-structures.md`.

---

## The subtle part: Redis alone isn't enough

The middleware above has a gap: if the server crashes **after** charging the card but **before** storing the result, the lock expires and a retry would run the operation a second time.

For money-moving operations, make the *database* the source of truth:

### Option 1: Unique constraint on the key (most robust)

Store the idempotency key **in the same transaction** as the business change.

```sql
CREATE TABLE payments (
  id              UUID PRIMARY KEY,
  user_id         UUID NOT NULL,
  idempotency_key TEXT NOT NULL,
  amount          INTEGER NOT NULL,
  status          TEXT NOT NULL,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (user_id, idempotency_key)        -- the database enforces "at most once"
);
```

```js
export async function createPayment(userId, key, input) {
  const client = await pool.connect();
  try {
    await client.query("BEGIN");

    const { rows } = await client.query(
      `INSERT INTO payments (id, user_id, idempotency_key, amount, status)
       VALUES (gen_random_uuid(), $1, $2, $3, 'pending')
       ON CONFLICT (user_id, idempotency_key) DO NOTHING
       RETURNING *`,
      [userId, key, input.amount]
    );

    if (rows.length === 0) {
      // Someone already created it — return the existing record (a replay)
      await client.query("ROLLBACK");
      const existing = await pool.query(
        "SELECT * FROM payments WHERE user_id = $1 AND idempotency_key = $2",
        [userId, key]
      );
      return { payment: existing.rows[0], replayed: true };
    }

    // ... call the payment provider, passing the SAME key so it dedupes too ...
    await client.query("COMMIT");
    return { payment: rows[0], replayed: false };
  } catch (err) {
    await client.query("ROLLBACK");
    throw err;
  } finally {
    client.release();
  }
}
```

Transactions are covered in `07-databases/postgresql/02-transactions-and-indexing.md` and `07-databases/mongodb/03-transactions.md`.

### Option 2: Propagate the key downstream

If you call a payment provider, email service, or another API, **pass an idempotency key to them too** (Stripe, for example, accepts `Idempotency-Key`). Derive it from your own operation's ID so retries of your operation produce the same downstream key.

```js
await stripe.paymentIntents.create(
  { amount: 5000, currency: "usd" },
  { idempotencyKey: `payment-${payment.id}` }
);
```

Redis is good for fast replay and lock coordination; **a unique database constraint is what guarantees correctness.** Serious systems use both.

---

## Other places idempotency matters

### Queue consumers and workers

Message queues typically guarantee **at-least-once** delivery, meaning your worker *will* occasionally get the same job twice. Every job handler must be idempotent:

```js
async function sendWelcomeEmail(job) {
  const { userId } = job.data;

  // "claim" the work with a unique record; ignore if it's already claimed
  const inserted = await db.query(
    `INSERT INTO sent_emails (user_id, kind) VALUES ($1, 'welcome')
     ON CONFLICT DO NOTHING RETURNING 1`,
    [userId]
  );
  if (inserted.rowCount === 0) return;            // already sent — do nothing

  await mailer.send(userId, "welcome");
}
```

See `11-async-processing/01-queues-and-bullmq.md` (use a deterministic `jobId` to dedupe) and `11-async-processing/02-workers-retry-dlq.md`.

### Webhooks

Providers retry deliveries, so receivers must dedupe by event ID. See `07-webhooks.md`.

### Natural idempotency with upserts

```sql
-- set-based operations are retry-safe by nature
INSERT INTO subscriptions (user_id, plan) VALUES ($1, $2)
ON CONFLICT (user_id) DO UPDATE SET plan = EXCLUDED.plan;
```

```js
// ❌ read-modify-write: a retry may apply twice
user.credits = user.credits + 10; await user.save();

// ✅ idempotent: tie the change to an event ID recorded uniquely
await db.query(
  `INSERT INTO credit_events (event_id, user_id, delta) VALUES ($1, $2, 10)
   ON CONFLICT (event_id) DO NOTHING`,
  [eventId, userId]
);
```

---

## Retries on the client side

Idempotency is half the story; the client needs to retry *well*:

```js
async function postWithRetry(url, body, { retries = 3 } = {}) {
  const idempotencyKey = crypto.randomUUID();       // ONE key for ALL attempts

  for (let attempt = 0; ; attempt++) {
    try {
      const res = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json", "Idempotency-Key": idempotencyKey },
        body: JSON.stringify(body),
      });

      // Retry only transient failures
      if ([408, 409, 429, 500, 502, 503, 504].includes(res.status) && attempt < retries) {
        throw new Error(`retryable status ${res.status}`);
      }
      return res;
    } catch (err) {
      if (attempt >= retries) throw err;
      const backoff = Math.min(1000 * 2 ** attempt, 10_000);
      const jitter = Math.random() * backoff * 0.3;
      await new Promise((r) => setTimeout(r, backoff + jitter));
    }
  }
}
```

- **Exponential backoff with jitter** prevents thundering herds.
- Retry **only transient** errors (timeouts, `429`, `502`–`504`), never `400`/`401`/`403`/`404`/`422`.
- Honor `Retry-After` when present.

---

## Choosing an approach

| Situation | Approach |
|---|---|
| Reads, `PUT`, `DELETE` | Already idempotent — just implement them correctly |
| Creating resources where the client can choose the ID | `PUT /things/{client-uuid}` |
| Payments, orders, anything irreversible | `Idempotency-Key` + **DB unique constraint** + downstream keys |
| Low-stakes duplicate risk (e.g. a "like") | A unique constraint `(user_id, post_id)` is enough |
| Queue workers | Dedupe by job/event ID recorded in the database |
| Webhook receivers | Dedupe by event ID |

---

## Testing idempotency

```js
test("replays the original response for a repeated key", async () => {
  const key = crypto.randomUUID();
  const send = () =>
    request(app).post("/api/v1/payments").set(auth).set("Idempotency-Key", key)
      .send({ amount: 5000 });

  const first = await send();
  const second = await send();

  expect(second.status).toBe(first.status);
  expect(second.body).toEqual(first.body);
  expect(second.headers["idempotent-replayed"]).toBe("true");
  expect(await countPayments()).toBe(1);              // exactly one charge
});

test("handles concurrent duplicates", async () => {
  const key = crypto.randomUUID();
  const results = await Promise.all(
    Array.from({ length: 5 }, () =>
      request(app).post("/api/v1/payments").set(auth).set("Idempotency-Key", key).send({ amount: 5000 })
    )
  );
  expect(await countPayments()).toBe(1);              // the race is won exactly once
});

test("rejects key reuse with a different body", async () => {
  const key = crypto.randomUUID();
  await request(app).post("/api/v1/payments").set(auth).set("Idempotency-Key", key).send({ amount: 5000 });
  const res = await request(app).post("/api/v1/payments").set(auth).set("Idempotency-Key", key).send({ amount: 9999 });
  expect(res.status).toBe(422);
});
```

The **concurrent** test is the important one — it's what catches a missing atomic lock.

---

## Common mistakes

```js
// ❌ check-then-set without atomicity: two parallel requests both pass the check
if (!(await redis.get(key))) { await redis.set(key, "1"); doWork(); }
// ✅ single atomic command
await redis.set(key, "1", { NX: true, EX: 60 });

// ❌ keys not scoped per user → cross-user replay
// ❌ new key generated on each retry attempt (defeats the whole purpose)
// ❌ caching 5xx responses, so the client is stuck with a transient failure
// ❌ locks without an expiry → a crash blocks that key forever
// ❌ ignoring the request body: same key, different payload silently replays the old result
// ❌ relying ONLY on Redis for money-critical work (no DB constraint)
// ❌ non-idempotent worker/webhook handlers behind an at-least-once queue
```

## Next

**`07-webhooks.md`** covers the other direction of integration — your API *pushing* events to other systems, and receiving them from providers like Stripe — where retries, signatures, and idempotency all come together.
