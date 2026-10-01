# Scheduled Jobs

Work that happens on a timetable rather than in response to a request: nightly reports, hourly cleanups, "every 5 minutes, check for stuck orders."

## What scheduled jobs are used for

- **Cleanup:** delete expired sessions, tokens, and soft-deleted rows
- **Reports and digests:** nightly summaries, weekly emails
- **Reconciliation:** compare your data with a provider's (payments, webhooks missed during downtime, see `09-api-development/07-webhooks.md`)
- **Sweeps:** find "orders pending for 30+ minutes" and act on them
- **Refreshing data:** sync a catalog, rebuild a cache, rotate keys
- **Outbox relays:** publish pending events (`02-workers-retry-dlq.md`)
- **Reminders and billing cycles:** "charge subscriptions due today"

The hard part isn't *starting* a function on a timer. It's doing it **once, not zero times and not N times**, when you run multiple instances, deploy mid-run, or restart at 02:59.

---

## Cron syntax

Most schedulers use the five-field **cron expression**:

```
┌───────────── minute        (0–59)
│ ┌─────────── hour          (0–23)
│ │ ┌───────── day of month  (1–31)
│ │ │ ┌─────── month         (1–12)
│ │ │ │ ┌───── day of week   (0–7; 0 and 7 are Sunday)
│ │ │ │ │
* * * * *
```

| Expression | Meaning |
|---|---|
| `* * * * *` | Every minute |
| `*/5 * * * *` | Every 5 minutes |
| `0 * * * *` | Every hour, on the hour |
| `0 2 * * *` | Every day at 02:00 |
| `30 8 * * 1-5` | 08:30 on weekdays |
| `0 0 1 * *` | Midnight on the 1st of each month |
| `0 9 * * 1` | 09:00 every Monday |
| `0 */6 * * *` | Every 6 hours |
| `15,45 * * * *` | At minute 15 and 45 of every hour |

Special characters: `*` (every), `,` (list), `-` (range), `/` (step). Some libraries (node-cron, BullMQ's cron parser) also accept a **sixth, seconds field** at the front: `*/10 * * * * *` is every 10 seconds. Check your library's docs, since it's an easy way to be off by a field. Use [crontab.guru](https://crontab.guru) to sanity-check expressions.

---

## Option 1: `setInterval` (and why it's not enough)

```js
// ❌ naive "scheduler"
setInterval(async () => {
  await cleanupExpiredSessions();
}, 60 * 60 * 1000);
```

Problems:

- **Resets on every restart/deploy:** "daily at 02:00" isn't expressible; the first run is "an hour after boot."
- **Overlap:** if a run takes longer than the interval, runs pile up. And `setInterval` doesn't wait for async work.
- **Unhandled errors:** a rejected promise inside the callback is an unhandled rejection (can crash the process).
- **Drift and timer limits:** intervals drift, and values above ~24.8 days (2³¹−1 ms) overflow and fire immediately.
- **Multiple instances = multiple runs:** three app replicas means three cleanups per hour.
- **No history, no retries, no visibility.**

`setInterval` is fine for **in-process housekeeping** (flushing a metrics buffer every 10 seconds) where duplicates and missed runs don't matter. For business work, use something better.

If you must, at least prevent overlap and handle errors:

```js
let running = false;

const timer = setInterval(async () => {
  if (running) return;                 // skip if the previous run is still going
  running = true;
  try {
    await cleanupExpiredSessions();
  } catch (err) {
    logger.error({ err }, "Cleanup failed");
  } finally {
    running = false;
  }
}, 60_000);

timer.unref();                          // don't keep the process alive just for this timer
```

---

## Option 2: `node-cron` (in-process cron)

```bash
npm install node-cron
```

```js
import cron from "node-cron";

const task = cron.schedule(
  "0 2 * * *",                          // every day at 02:00
  async () => {
    try {
      await cleanupExpiredSessions();
    } catch (err) {
      logger.error({ err }, "Nightly cleanup failed");
    }
  },
  { timezone: "Asia/Kolkata" }           // ALWAYS set a timezone explicitly
);

// later
task.stop();                             // pause
task.start();                            // resume
```

```js
cron.validate("*/5 * * * *");            // true / false: validate user-supplied expressions
```

Alternatives with similar APIs: `node-schedule` (supports Date objects and recurrence rules) and `croner` (small, zero-dependency, supports overrun protection built in):

```js
import { Cron } from "croner";

new Cron("*/5 * * * *", { protect: true, timezone: "UTC" }, async () => {
  await sweepStuckOrders();             // `protect: true` skips a tick if the previous run is still going
});
```

### What in-process cron gives you, and what it doesn't

✅ Simple, no infrastructure, proper cron expressions and timezones.

❌ **Runs in every instance.** Deploy 3 replicas and each one fires the schedule.
❌ **Missed runs are lost.** If the process is down at 02:00, that run never happens.
❌ **No retries, no history, no dashboard.**
❌ **Heavy work shares the event loop** with your API traffic.

So in-process cron is acceptable for a **single instance** or for jobs that are **harmless when duplicated or skipped**. Otherwise you need one of the approaches below.

---

## The multi-instance problem

```
         app instance 1 ──┐
cron →   app instance 2 ──┼──▶ all three run "charge due subscriptions" at 09:00 → triple charges
         app instance 3 ──┘
```

Solutions, from most to least robust:

1. **A queue-based scheduler** (BullMQ job schedulers): the *schedule* creates a job in a shared queue; exactly one worker runs it.
2. **A dedicated scheduler process:** run cron in exactly one place (a single "scheduler" deployment with `replicas: 1`) that just *enqueues* jobs.
3. **A platform scheduler:** Kubernetes CronJob, cloud schedulers (EventBridge, Cloud Scheduler), or OS cron/systemd timers that trigger a job or an HTTP endpoint.
4. **A distributed lock:** all instances run cron, but only the one that acquires the lock proceeds.
5. **Idempotent jobs:** whatever else you do, make running twice harmless.

---

## Option 3: BullMQ job schedulers (recommended for most Node apps)

If you already run BullMQ (`01-queues-and-bullmq.md`), schedule through it. Schedules live in Redis, jobs are created once per tick no matter how many instances exist, and you get retries, concurrency control, and a dashboard for free.

```js
import { Queue, Worker } from "bullmq";

const maintenanceQueue = new Queue("maintenance", { connection });

// Idempotent upsert: safe to run on every app boot, from every instance
await maintenanceQueue.upsertJobScheduler(
  "nightly-cleanup",                                   // scheduler ID: the identity of this schedule
  { pattern: "0 2 * * *", tz: "Asia/Kolkata" },        // cron expression + timezone
  {
    name: "cleanup-expired-sessions",
    data: {},
    opts: { attempts: 3, backoff: { type: "exponential", delay: 60_000 }, removeOnComplete: true },
  }
);

// Fixed interval instead of cron
await maintenanceQueue.upsertJobScheduler(
  "sweep-stuck-orders",
  { every: 5 * 60 * 1000 },
  { name: "sweep-stuck-orders", data: {} }
);

new Worker("maintenance", async (job) => {
  switch (job.name) {
    case "cleanup-expired-sessions": return cleanupExpiredSessions();
    case "sweep-stuck-orders": return sweepStuckOrders();
  }
}, { connection });
```

Key points:

- `upsertJobScheduler` **creates or updates** the schedule keyed by its ID, so calling it on every deploy doesn't create duplicates, and changing the pattern in code updates the existing schedule.
- It's available in current BullMQ versions (v5.16+). Older versions use `queue.add(name, data, { repeat: { pattern: "..." } })` ("repeatable jobs"), which is the same idea with a clumsier API: removing a repeatable job requires the exact original options (`removeRepeatable`). If you're on an older release, check your version's docs.
- Manage schedules:

```js
await maintenanceQueue.getJobSchedulers();                 // list all schedulers
await maintenanceQueue.removeJobScheduler("nightly-cleanup");
```

- **Remove schedules you delete from code.** A schedule stored in Redis keeps firing after you delete the line that created it, so remove it explicitly when decommissioning a job.
- The worker runs only if a worker is up. If workers are down at 02:00, the job waits in the queue and runs when they return (BullMQ doesn't "skip" it, so it may run late, not never).

### Don't create a pile-up

If a job occasionally takes longer than its interval, ticks stack up in the queue. Options: use a worker `concurrency: 1` for that queue, make the job itself check "is another run active?" (lock below), or schedule the *next* run only when the current one finishes (self-rescheduling with `delay`).

---

## Option 4: Platform and OS schedulers

### Kubernetes CronJob

```yaml
apiVersion: batch/v1
kind: CronJob
metadata:
  name: nightly-report
spec:
  schedule: "0 2 * * *"
  timeZone: "Asia/Kolkata"
  concurrencyPolicy: Forbid          # skip if the previous run is still going
  startingDeadlineSeconds: 600       # if missed by >10 min (cluster down), skip it
  successfulJobsHistoryLimit: 3
  failedJobsHistoryLimit: 5
  jobTemplate:
    spec:
      backoffLimit: 2
      template:
        spec:
          restartPolicy: OnFailure
          containers:
            - name: report
              image: myapp:latest
              command: ["node", "scripts/nightly-report.js"]
```

Each run is a fresh container that starts, does one job, and exits, with clean isolation, its own resources, and no overlap with your API pods. The script must exit with code `0` on success and non-zero on failure so Kubernetes can retry and report it. See `16-production/03-docker-and-compose.md`.

### Cloud schedulers

AWS EventBridge Scheduler, Google Cloud Scheduler, and Azure Logic Apps can trigger a Lambda/Cloud Run job, or call an HTTP endpoint on a schedule. If you trigger an endpoint, **authenticate it** (a shared secret or signed header; never expose an unauthenticated `/run-cleanup`) and make it **return quickly**: enqueue the real work instead of running it inside the request.

```js
app.post("/internal/cron/cleanup", requireCronSecret, async (req, res) => {
  await maintenanceQueue.add("cleanup-expired-sessions", {}, { jobId: `cleanup-${todayKey()}` });
  res.sendStatus(202);
});
```

### OS cron / systemd timers

```cron
# crontab -e
0 2 * * * cd /srv/app && /usr/bin/node scripts/nightly-report.js >> /var/log/nightly-report.log 2>&1
```

Reliable and boring, but remember: environment variables, `PATH`, and working directory differ from your shell, and with multiple servers you'd have to ensure only one has the entry.

### Database-level scheduling

PostgreSQL's `pg_cron` extension runs SQL on a schedule inside the database, ideal for pure-SQL maintenance (partition rotation, deleting old rows):

```sql
SELECT cron.schedule('purge-old-sessions', '0 3 * * *', $$DELETE FROM sessions WHERE expires_at < now()$$);
```

### Choosing

| Approach | Pros | Cons | Best for |
|---|---|---|---|
| `setInterval` | Trivial | Unreliable, runs per-instance | In-process housekeeping only |
| `node-cron` / `croner` | Easy, cron syntax | Per-instance, no durability | Single instance, or idempotent harmless tasks |
| **BullMQ scheduler** | One run per tick, retries, dashboard | Needs Redis + worker | Most Node apps already using queues |
| Dedicated scheduler process | Simple mental model | One more deployable; single point | Teams not using queues |
| **K8s CronJob / cloud scheduler** | Isolation, platform-managed, missed-run policy | Cold start, platform tied | Heavy/batch jobs, container platforms |
| OS cron | Ubiquitous | Per-host, ops-heavy | Single-server deployments |
| `pg_cron` | Runs in the DB | SQL only | Database maintenance |

---

## Preventing duplicate and overlapping runs

Even with a good scheduler, guard against overlap (a slow run meeting the next tick) and duplicates (retries, manual triggers).

### Redis lock

```js
import crypto from "node:crypto";

async function withLock(redis, key, ttlMs, work) {
  const token = crypto.randomUUID();                         // identifies THIS holder

  const acquired = await redis.set(key, token, { NX: true, PX: ttlMs });
  if (!acquired) return { skipped: true };                   // someone else is running it

  try {
    return { skipped: false, result: await work() };
  } finally {
    // release ONLY if we still own the lock (it may have expired and been taken by another worker)
    await redis.eval(
      `if redis.call("get", KEYS[1]) == ARGV[1] then return redis.call("del", KEYS[1]) else return 0 end`,
      { keys: [key], arguments: [token] }
    );
  }
}

// usage inside the scheduled job
await withLock(redis, "lock:nightly-report", 30 * 60_000, generateNightlyReport);
```

The details matter:

- `SET key value NX PX ttl` is **atomic**, so only one caller wins.
- The **TTL** guarantees a crashed holder can't block forever.
- The **token + Lua release** prevents deleting a lock that expired and was re-acquired by someone else.
- Set the TTL comfortably above the job's normal runtime; for jobs that may run longer, renew the lock periodically (a "heartbeat"). A lock that expires mid-run lets a second instance start, so the lock alone is *not* a correctness guarantee. Pair it with idempotent work.

### PostgreSQL advisory locks

If the job already talks to Postgres, a session-level advisory lock needs no extra infrastructure and is released automatically if the connection drops:

```js
const client = await pool.connect();
try {
  const { rows } = await client.query("SELECT pg_try_advisory_lock($1) AS locked", [738_001]);   // any agreed integer
  if (!rows[0].locked) return;                                                                   // another instance holds it

  try {
    await generateNightlyReport(client);
  } finally {
    await client.query("SELECT pg_advisory_unlock($1)", [738_001]);
  }
} finally {
  client.release();
}
```

Because the lock is tied to the **connection**, hold the same `client` for the whole run (not `pool.query`, which may use different connections), and note that pooled-connection proxies in transaction mode (like PgBouncer) can break session-level locks. Use `pg_try_advisory_xact_lock` inside a transaction in that case.

### Work claiming for sweeps

For "process everything that's due" jobs, avoid locking the whole job. Instead let each run **claim rows**, so overlapping runs split the work instead of colliding:

```sql
-- each run grabs a batch no one else holds
SELECT id FROM subscriptions
WHERE next_charge_at <= now() AND status = 'active'
ORDER BY next_charge_at
LIMIT 100
FOR UPDATE SKIP LOCKED;
```

Combined with an idempotent charge (an idempotency key derived from `subscription_id + billing_period`), double-runs become harmless.

---

## Designing scheduled jobs well

### 1. Make them idempotent and safe to re-run

A run might execute twice (retries, overlapping ticks, a manual re-run during an incident) or be skipped and later backfilled. Tie work to **the period it covers**, not to "now":

```js
// ❌ "generate today's report": which "today"? re-running tomorrow gives a different result
await generateReport(new Date());

// ✅ the period is explicit, and the output is keyed by it: re-running overwrites/skips cleanly
async function generateDailyReport({ date }) {                  // date = "2026-09-30"
  const exists = await reportRepository.exists("daily", date);
  if (exists) return;
  const data = await collectStats({ from: startOfDay(date), to: endOfDay(date) });
  await reportRepository.upsert("daily", date, data);
}
```

Scheduled jobs often pass `scheduledFor` (the tick's intended time) in the payload, not `new Date()`, so a delayed run still processes the right window.

### 2. Handle missed runs ("catch-up")

If the system was down at 02:00, what should happen when it comes back?

| Strategy | When |
|---|---|
| **Skip it:** the next tick will cover it | Pure "sweep" jobs ("find stuck orders now") that look at current state |
| **Run once on recovery** | Idempotent jobs that need to happen *at least once per period* |
| **Backfill each missed period** | Period-based jobs (daily reports, billing); loop over missing dates |

The most robust pattern for sweeps: have the job always query "everything that's due" (`next_charge_at <= now()`) instead of "things due in the last 5 minutes". Then a missed run is automatically absorbed by the next one.

### 3. Time zones and daylight saving

- Set the timezone **explicitly** on the scheduler (`tz: "Asia/Kolkata"`); don't depend on the server's local time (containers usually default to UTC).
- **Store and compare times in UTC;** only convert for display and for "local-time" schedules.
- With DST, "02:30 daily" **doesn't exist** on spring-forward day and happens **twice** on fall-back day in affected zones. Avoid scheduling at 02:00–03:00 local time in zones with DST, or schedule in UTC. (India has no DST, but your users and servers might not be in India.)
- For per-user local times ("send at 9am *their* time"), don't create one cron per user. Run a job every 15 minutes that finds users whose local time just crossed 9:00 and hasn't been notified today.

### 4. Don't thunder at the top of the hour

Everyone schedules `0 * * * *` and `0 0 * * *`. Hundreds of jobs (yours and everyone else's third-party APIs) fire simultaneously. Stagger with an offset (`7 * * * *`) or add random jitter at the start of the job.

### 5. Keep ticks short; fan out the work

A schedule should *start* work, not *be* the work. For large jobs, the scheduled tick enqueues many small jobs:

```js
// the scheduled job: fast, just fans out
async function enqueueDailyDigests() {
  for await (const batch of userRepository.iterateBatches({ digestEnabled: true }, 500)) {
    await digestQueue.addBulk(
      batch.map((u) => ({ name: "digest", data: { userId: u.id, date: todayKey() }, opts: { jobId: `digest-${todayKey()}-${u.id}` } }))
    );
  }
}

// each small job: send one digest, with retries, rate limits, DLQ (02-workers-retry-dlq.md)
```

The deterministic `jobId` (date + user) means a double-triggered fan-out creates no duplicate digests.

### 6. Time-box and set timeouts

Every scheduled job should have a maximum runtime. A job that hangs silently blocks all future ticks (with `concurrency: 1` or a lock). Use `AbortSignal.timeout` for network calls and an overall deadline for the run.

### 7. Don't run heavy scheduled work in the web process

A 5-minute CPU-heavy report inside your API process steals the event loop from users (`15-performance/01-event-loop-performance.md`). Run schedulers and workers as separate processes or containers.

---

## Monitoring: scheduled jobs fail *silently*

Nobody notices when a job that should run nightly stops running. Alert on the **absence** of success, not just on errors.

### Log every run

```js
async function runScheduled(name, fn) {
  const startedAt = Date.now();
  logger.info({ job: name }, "Scheduled job started");
  try {
    const result = await fn();
    logger.info({ job: name, ms: Date.now() - startedAt, result }, "Scheduled job finished");
    metrics.lastSuccess.set({ job: name }, Date.now() / 1000);
  } catch (err) {
    logger.error({ job: name, err, ms: Date.now() - startedAt }, "Scheduled job failed");
    metrics.failures.inc({ job: name });
    throw err;
  }
}
```

### Dead man's switch (heartbeat monitoring)

Ping an external monitor (Healthchecks.io, Cronitor, Better Stack, Sentry Crons) when each run **succeeds**. If no ping arrives within the expected window, *they* alert you, and this catches the case where the scheduler itself died.

```js
await generateNightlyReport();
await fetch(process.env.HEALTHCHECK_PING_URL, { signal: AbortSignal.timeout(5000) }).catch(() => {});
```

### Prometheus alert on staleness

```
# alert if the nightly cleanup hasn't succeeded in 26 hours
time() - scheduled_job_last_success_timestamp_seconds{job="nightly-cleanup"} > 26 * 3600
```

More in `14-logging-observability/03-metrics-and-prometheus.md`.

---

## Testing scheduled jobs

Separate **what runs** from **when it runs**, so the logic is testable without waiting for a clock:

```js
// jobs/cleanupExpiredSessions.js: plain function, no scheduling knowledge
export async function cleanupExpiredSessions({ sessionRepository, now = () => new Date() }) {
  return sessionRepository.deleteExpired(now());
}

// test: no timers, no cron, no Redis
test("deletes sessions that expired before 'now'", async () => {
  const deleted = [];
  await cleanupExpiredSessions({
    sessionRepository: { deleteExpired: async (t) => deleted.push(t) },
    now: () => new Date("2026-09-30T02:00:00Z"),
  });
  expect(deleted[0].toISOString()).toBe("2026-09-30T02:00:00.000Z");
});
```

- Inject the **clock** (`10-architecture/04-dependency-injection.md`) instead of calling `new Date()` inside the logic.
- Test the schedule *configuration* separately: `expect(cron.validate(SCHEDULE)).toBe(true)`.
- If you use fake timers (`jest.useFakeTimers()`), test the overlap guard: advance time while a run is pending and assert it doesn't start twice.
- Provide a way to **trigger a job manually** (a CLI script or admin endpoint) for incident response and backfills.

---

## Common mistakes

```js
// ❌ in-process cron in an app that runs several replicas → each tick runs N times
cron.schedule("0 9 * * *", chargeSubscriptions);

// ❌ no timezone set → "9am" means 9am UTC in the container, not where you meant
// ❌ new Date() inside the job → re-runs and late runs process the wrong window
// ❌ job duration > interval with no overlap protection → runs pile up
// ❌ scheduler registered on every boot without upsert → duplicate schedules accumulate
// ❌ removed the code but not the stored schedule → the old job keeps firing from Redis
// ❌ everything at minute 0 → thundering herd on your DB and third parties
// ❌ swallowing errors inside the job so the scheduler/queue thinks it succeeded
// ❌ no alert on "didn't run": a dead scheduler looks identical to a healthy quiet system
// ❌ unauthenticated HTTP endpoint that triggers a cron job
// ❌ 02:00–03:00 local schedules in a DST timezone
```

## Checklist

- [ ] Each scheduled task runs **once per tick** regardless of replica count (queue scheduler, single scheduler process, platform cron, or lock)
- [ ] Timezone set explicitly; times stored in UTC
- [ ] Job is idempotent and keyed by the **period it covers**, not "now"
- [ ] Missed-run behavior decided (skip, run once, or backfill)
- [ ] Overlap prevented (lock, `concurrencyPolicy: Forbid`, `protect: true`, or `concurrency: 1`)
- [ ] Heavy work fans out to queue jobs; ticks stay short; timeouts set
- [ ] Runs on separate worker processes, not the web server
- [ ] Success and failure logged; staleness alert or heartbeat monitor configured
- [ ] Manual trigger path exists; logic testable without a clock
- [ ] Schedules removed explicitly when a job is retired

## Next

**`04-kafka.md`** moves from "do this task" to "this thing happened": Kafka's log-based model, partitions and consumer groups, ordering and delivery guarantees, and how to use it from Node.js with KafkaJS.
