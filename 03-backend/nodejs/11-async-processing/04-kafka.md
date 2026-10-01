# Kafka

A distributed, durable, replayable log of events — the backbone of event-driven systems — and how to use it from Node.js.

## What Kafka is (and isn't)

Kafka is an **append-only commit log** that many producers write to and many consumers read from, each at their own pace. Events aren't removed when they're read; they stay until a **retention** period expires (or forever, with compaction).

```
Producers                       Kafka topic "orders"                        Consumers
─────────                  ┌──────────────────────────────┐             ─────────────
API ───────────┐           │ offset: 0  1  2  3  4  5  6 ─▶ (new events appended)
Worker ────────┼─ write ──▶│  [e0][e1][e2][e3][e4][e5][e6]│── read ──▶ Billing service  (at its own offset)
Import job ────┘           └──────────────────────────────┘── read ──▶ Analytics         (at ITS own offset)
                                                              ── read ──▶ Email service
```

The mental shift from a queue (`01-queues-and-bullmq.md`):

| | Queue (BullMQ, SQS) | Kafka |
|---|---|---|
| Model | A to-do list: take a job, finish it, it's gone | A history book: events stay; readers keep a bookmark |
| New consumer | Sees only new jobs | Can start from the beginning and replay everything |
| Multiple systems need the same event | Copy it into several queues | They all just read the same topic |
| Ordering | Best effort | Guaranteed **within a partition** |
| Scale | Thousands of jobs/s | Hundreds of thousands to millions of events/s |
| Operations | Run Redis | Run (or pay for) a cluster; real operational weight |

### When Kafka is the right tool

- **Many independent consumers** need the same stream of facts (billing, analytics, search indexing, notifications).
- You need to **replay history:** rebuild a read model, backfill a new service, reprocess after a bug fix.
- **High throughput** event ingestion: clickstreams, IoT telemetry, logs, audit trails.
- **Event-driven architecture / event sourcing** between services (`10-architecture/05-modular-monolith-vs-microservices.md`).
- Data pipelines and change-data-capture (CDC) from databases.

### When it's overkill

- You mainly need to **run background tasks** (emails, image resizing) → BullMQ or SQS is simpler.
- Low volume, one or two consumers → a queue or even a database table works.
- A small team without capacity to operate a cluster (use a managed service: Confluent Cloud, AWS MSK, Redpanda Cloud, Upstash).

Kafka is powerful but it's **infrastructure you have to run, monitor, and understand.** Choose it for what it uniquely offers: a replayable, ordered, multi-consumer log.

---

## Core concepts

### Topic

A named stream of events: `orders`, `payments`, `user-signups`. Producers write to topics; consumers subscribe to them. Think of a topic as a category or table-like log.

### Partition

Each topic is split into **partitions**, which are independent ordered logs. Partitions are the unit of **parallelism** and **ordering**.

```
Topic "orders" (3 partitions)

Partition 0:  [o1][o4][o7][o9] ...   ← events for some keys, in order
Partition 1:  [o2][o5][o8]     ...   ← events for other keys, in order
Partition 2:  [o3][o6]         ...
```

- Order is guaranteed **only within a partition**, never across partitions.
- Each event gets an **offset**: its sequential position within the partition.
- More partitions → more parallel consumers, higher throughput. (You can add partitions later but not remove them, and adding them changes key-to-partition mapping.)

### Key: how you control ordering

Each message can have a **key**. Kafka hashes the key to choose the partition, so **all events with the same key go to the same partition and stay in order**.

```js
{ key: order.id,  value: ... }     // all events for one order: created → paid → shipped, in order
{ key: user.id,   value: ... }     // all events for one user stay ordered
{ key: null,      value: ... }     // no key: spread round-robin; no ordering guarantee
```

**Choose the key by "what must be processed in order."** If events for one order must be handled sequentially, key by `orderId`. Use a key with high cardinality (many distinct values) so data spreads evenly: keying by `country` would hammer a few partitions (a "hot partition").

### Broker, cluster, replication

A Kafka **cluster** is several **brokers** (servers). Each partition has a **leader** and several **replicas** on other brokers. If a broker dies, a replica takes over.

- `replication.factor = 3` is the common production setting.
- `min.insync.replicas = 2` plus producer `acks: -1` ("all") means a write is only confirmed once at least two replicas have it, so a single broker failure doesn't lose acknowledged data.

### Producer and consumer

- **Producer:** appends messages to a topic.
- **Consumer:** reads messages from a topic, tracking its position (offset).

### Consumer group: how consumption scales

Consumers join a **group** with a shared `groupId`. Kafka divides the topic's partitions among the group's members, so **each partition is read by exactly one consumer in the group**.

```
Topic "orders": P0  P1  P2  P3

Group "billing"  (2 consumers):      Group "analytics" (1 consumer):
  consumer A ← P0, P1                  consumer X ← P0, P1, P2, P3
  consumer B ← P2, P3

Each group independently receives EVERY event; within a group, partitions are shared.
```

| Rule | Consequence |
|---|---|
| One partition → at most one consumer per group | Max useful consumers in a group = number of partitions |
| Different groups are independent | Each downstream system gets its own full copy of the stream |
| Consumers join/leave | Kafka **rebalances**: reassigns partitions (brief pause) |

So: **partitions are your ceiling on consumer parallelism.** Plan partition counts for peak parallelism (commonly 6–50 for moderate systems; avoid thousands without reason).

### Offsets and committing

A consumer group records the **last processed offset** per partition (stored in Kafka's internal `__consumer_offsets` topic). When a consumer restarts, it resumes from the committed offset.

When you **commit** is what defines your delivery semantics (next section).

### Retention and compaction

| Setting | Behavior |
|---|---|
| **Time/size retention** (default 7 days) | Old events are deleted after the period or size limit, whether or not anyone read them |
| **Log compaction** | Keeps only the **latest event per key**, giving "current state per key" (a changelog), useful for tables, configs, and entity snapshots |

---

## Delivery guarantees

| Guarantee | Meaning | How you get it |
|---|---|---|
| **At-most-once** | May lose messages, never duplicates | Commit offset **before** processing |
| **At-least-once** (the common default) | Never loses, may duplicate | Commit offset **after** processing succeeds |
| **Exactly-once** | Each message's effect applied once | Idempotent producer + transactions (Kafka-to-Kafka), or **idempotent consumers** (everything else) |

For anything touching the outside world (databases, emails, payments), **design for at-least-once and make consumers idempotent** (`09-api-development/06-idempotency.md`, `02-workers-retry-dlq.md`). Kafka's "exactly-once semantics" (EOS) only covers read-process-write *within Kafka*; once you write to PostgreSQL or call an HTTP API, you're back to needing idempotency.

---

## Using Kafka from Node.js

Popular clients:

| Library | Notes |
|---|---|
| **KafkaJS** | Pure JavaScript, easy API, very widely used (used below). Check its release activity before committing to it for a long-lived project, as maintenance has been slower in recent years. |
| **@confluentinc/kafka-javascript** | Confluent's official client built on `librdkafka`; high performance; offers a KafkaJS-compatible API |
| **node-rdkafka** | Native `librdkafka` binding; fast, but native build complexity |

### Local development with Docker

```yaml
# docker-compose.yml: single-node Kafka (KRaft mode, no ZooKeeper)
services:
  kafka:
    image: apache/kafka:3.8.0
    ports: ["9092:9092"]
```

(Or Redpanda, a Kafka-compatible single binary that's convenient for local development. Image tags and settings change, so check the image's docs.)

```bash
npm install kafkajs
```

```js
// kafka/client.js
import { Kafka, logLevel } from "kafkajs";

export const kafka = new Kafka({
  clientId: "orders-api",
  brokers: (process.env.KAFKA_BROKERS ?? "localhost:9092").split(","),
  logLevel: logLevel.WARN,
  // production: TLS + SASL
  // ssl: true,
  // sasl: { mechanism: "scram-sha-512", username: process.env.KAFKA_USER, password: process.env.KAFKA_PASSWORD },
  retry: { initialRetryTime: 300, retries: 8 },
});
```

### Create a topic

In production, topics are usually created by infrastructure code (Terraform, a platform team), not by app code. For development:

```js
const admin = kafka.admin();
await admin.connect();
await admin.createTopics({
  topics: [{ topic: "orders", numPartitions: 6, replicationFactor: 1 }],   // replicationFactor 3 in production
});
await admin.disconnect();
```

### Producing

```js
// kafka/producer.js
import { kafka } from "./client.js";

export const producer = kafka.producer({
  idempotent: true,             // retries can't create duplicates within a partition (implies acks=all)
  maxInFlightRequests: 5,
});

await producer.connect();       // connect ONCE at startup; reuse for the lifetime of the process
```

```js
// publish an event
await producer.send({
  topic: "orders",
  acks: -1,                                           // wait for all in-sync replicas
  messages: [
    {
      key: order.id,                                  // keeps all events for this order in one partition, in order
      value: JSON.stringify({
        id: randomUUID(),                             // unique EVENT id: consumers dedupe on it
        type: "order.placed",
        version: 1,                                   // payload schema version
        occurredAt: new Date().toISOString(),
        data: { orderId: order.id, userId: order.userId, totalCents: order.totalCents },
      }),
      headers: { "correlation-id": req.id, "content-type": "application/json" },
    },
  ],
});
```

Producer practices:

- **One long-lived producer per process.** Don't create one per request.
- **Batch:** `send` accepts many messages; `producer.sendBatch` spans topics. Batching dramatically improves throughput.
- Always set a **key** when order matters.
- Use **`acks: -1`** and an idempotent producer for important data; `acks: 1` or `0` trades safety for speed.
- Include an **event ID, type, version, and timestamp** in every event.
- `send` rejecting means the write failed *after* the client's retries, so handle it (retry, or write to the outbox, below).
- **Keep messages small** (the default broker limit is ~1 MB). Put big payloads in object storage and send a reference.

### Consuming

```js
// kafka/consumer.js
import { kafka } from "./client.js";

const consumer = kafka.consumer({
  groupId: "billing-service",                         // identity of this consumer group
  sessionTimeout: 30_000,
  heartbeatInterval: 3_000,
});

await consumer.connect();
await consumer.subscribe({ topics: ["orders"], fromBeginning: false });   // true = start from earliest on a NEW group

await consumer.run({
  autoCommit: false,                                  // we commit manually, AFTER success (at-least-once)
  eachMessage: async ({ topic, partition, message, heartbeat }) => {
    const event = JSON.parse(message.value.toString());

    await handleEvent(event);                         // your idempotent handler

    // commit the NEXT offset to read (offset + 1)
    await consumer.commitOffsets([
      { topic, partition, offset: (Number(message.offset) + 1).toString() },
    ]);
  },
});
```

Notes:

- `message.value` is a `Buffer`: parse it. `message.key` is a `Buffer` too.
- Offsets are strings (they can exceed JavaScript's safe integer range in theory; `BigInt` is safest for arithmetic).
- **`autoCommit: true` (the default)** commits periodically in the background, which is simpler but means a crash can commit messages that were still being processed (at-most-once for those) or reprocess some. For critical flows, commit manually after success. A middle path: keep `autoCommit` on but only resolve `eachMessage` after the work completes: KafkaJS commits resolved offsets, so awaited work before return is covered.
- Call `heartbeat()` inside long-running handlers so the group coordinator doesn't think you've died and trigger a rebalance.
- **Throwing from `eachMessage`** makes KafkaJS retry the message (with its retry settings) and eventually restart the consumer, so decide your error strategy deliberately (next section).

### Batch processing

For throughput, process several messages at a time:

```js
await consumer.run({
  eachBatchAutoResolve: false,
  eachBatch: async ({ batch, resolveOffset, heartbeat, commitOffsetsIfNecessary, isRunning, isStale }) => {
    for (const message of batch.messages) {
      if (!isRunning() || isStale()) break;           // stop promptly during shutdown / rebalance

      await handleEvent(JSON.parse(message.value.toString()));

      resolveOffset(message.offset);                  // mark this message done
      await heartbeat();
    }
    await commitOffsetsIfNecessary();
  },
});
```

### Parallelism within one consumer

By default KafkaJS processes **one message per partition at a time** (preserving order) but can process different partitions concurrently via `partitionsConsumedConcurrently`:

```js
await consumer.run({ partitionsConsumedConcurrently: 3, eachMessage: handler });
```

Don't parallelize *within* a partition (for example, `Promise.all` over a batch) unless ordering truly doesn't matter, because you'd lose the guarantee that makes the key meaningful.

---

## Error handling in consumers

The key question: **what happens to a message that fails?** In a log, you can't "skip and come back later" without a plan: the consumer's offset moves forward in order, and a message that always fails can **block the whole partition** (a *poison pill*).

### Strategy 1: Retry in place, then dead-letter

```js
async function processWithRetry(message, { attempts = 3 } = {}) {
  for (let i = 1; i <= attempts; i++) {
    try {
      return await handleEvent(JSON.parse(message.value.toString()));
    } catch (err) {
      if (isPermanent(err) || i === attempts) throw err;
      await sleep(2 ** i * 500 + Math.random() * 250);          // backoff + jitter
    }
  }
}

await consumer.run({
  eachMessage: async ({ topic, partition, message }) => {
    try {
      await processWithRetry(message);
    } catch (err) {
      // give up: park it in a dead-letter topic so the partition keeps moving
      await producer.send({
        topic: `${topic}.dlq`,
        messages: [{
          key: message.key,
          value: message.value,
          headers: {
            ...message.headers,
            "dlq-error": String(err.message),
            "dlq-original-topic": topic,
            "dlq-original-partition": String(partition),
            "dlq-original-offset": message.offset,
            "dlq-failed-at": new Date().toISOString(),
          },
        }],
      });
      logger.error({ err, topic, partition, offset: message.offset }, "Message sent to DLQ");
    }
    await consumer.commitOffsets([{ topic, partition, offset: (Number(message.offset) + 1).toString() }]);
  },
});
```

The DLQ is just another topic (`orders.dlq`), and you build tooling to inspect and replay it (same discipline as `02-workers-retry-dlq.md`): alert on arrival, investigate, fix, replay.

### Strategy 2: Retry topics (non-blocking retries)

Blocking retries with sleeps stall the entire partition. For slow-recovering failures, route failed messages to **retry topics with delays**, while the main topic keeps flowing:

```
orders ──fail──▶ orders.retry-1m ──fail──▶ orders.retry-10m ──fail──▶ orders.dlq
                  (consumer waits until message age ≥ 1 min)
```

Each retry topic has its own consumer that waits until the message is old enough before reprocessing. More moving parts, but **no head-of-line blocking**, so use it when failures are common and slow to resolve (flaky third-party dependency). Ordering for that key is relaxed once a message is diverted.

### Strategy 3: Stop and fix

For streams where **order and completeness are critical** (a ledger), you may *want* the consumer to halt on an unprocessable message, alert loudly, and wait for a human, rather than skipping events and corrupting state. Choose deliberately per topic.

### Classify errors

Same rule as for queues: **transient** (timeouts, DB down, `503`) → retry; **permanent** (invalid payload, unknown schema version, missing required entity) → dead-letter immediately; don't loop.

---

## Idempotent consumers

Because of at-least-once delivery and rebalances, you will process some events twice. Make handlers safe.

```sql
CREATE TABLE processed_events (
  consumer   TEXT NOT NULL,
  event_id   UUID NOT NULL,
  processed_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (consumer, event_id)
);
```

```js
async function handleEvent(event) {
  await withTransaction(async (tx) => {
    // 1. Claim the event. If already processed, the insert does nothing.
    const claimed = await tx.query(
      `INSERT INTO processed_events (consumer, event_id) VALUES ('billing', $1)
       ON CONFLICT DO NOTHING RETURNING event_id`,
      [event.id]
    );
    if (claimed.rowCount === 0) return;                // duplicate: skip

    // 2. Apply the effect in the SAME transaction: both commit or neither does
    await billingService.createInvoice(event.data, tx);
  });
}
```

Because the dedupe record and the business effect commit atomically, a crash at any point leaves the system consistent: either both happened or neither did. Alternatives: natural idempotency (`UPSERT` keyed by an entity ID), or version checks (ignore events older than the stored version).

---

## Producing reliably: the outbox pattern

The dual-write problem again: you can't atomically update PostgreSQL **and** publish to Kafka.

```js
await db.query("INSERT INTO orders ...");           // ✅
await producer.send({ topic: "orders", ... });       // 💥 broker unavailable → order exists, event never published
```

Use the **transactional outbox** (introduced in `00-README.md`, implemented in `02-workers-retry-dlq.md`): write the event to an `outbox` table in the same DB transaction as the business change, and have a **relay** publish it to Kafka.

```js
// relay: publishes unsent rows, then marks them sent
async function relayToKafka() {
  const client = await pool.connect();
  try {
    await client.query("BEGIN");
    const { rows } = await client.query(
      `SELECT * FROM outbox WHERE sent_at IS NULL ORDER BY created_at LIMIT 200 FOR UPDATE SKIP LOCKED`
    );
    if (rows.length === 0) { await client.query("COMMIT"); return; }

    await producer.send({
      topic: "orders",
      acks: -1,
      messages: rows.map((r) => ({ key: r.aggregate_id, value: JSON.stringify(r.payload) })),
    });

    await client.query("UPDATE outbox SET sent_at = now() WHERE id = ANY($1)", [rows.map((r) => r.id)]);
    await client.query("COMMIT");
  } catch (err) {
    await client.query("ROLLBACK");
    throw err;
  } finally {
    client.release();
  }
}
```

If the relay crashes after `send` but before `COMMIT`, the rows are re-sent later, producing duplicates, which **idempotent consumers** absorb. The result: no lost events, and duplicates are harmless.

A more advanced variant is **change data capture (CDC)** with Debezium, which tails the database's write-ahead log and publishes outbox rows to Kafka without a polling relay.

---

## Event design

Events are a long-lived contract between teams. Treat their design as seriously as a public API (`09-api-development/`).

```jsonc
{
  "id": "6f1c1e0e-4a1d-4a71-9a52-2d0d8f9c3a10",   // unique event id → dedupe
  "type": "order.placed",                          // past tense: it HAPPENED
  "version": 1,                                    // schema version
  "occurredAt": "2026-09-30T10:15:00.000Z",
  "source": "orders-service",
  "correlationId": "req_8f3a2c1d",                 // trace across services
  "data": { "orderId": "ord_42", "userId": "usr_7", "totalCents": 5000, "currency": "USD" }
}
```

| Guideline | Why |
|---|---|
| **Name events in the past tense** (`order.placed`, not `place.order`) | They're facts, not commands |
| **Event vs command** | Events say what happened and have *many* possible reactions; commands ask a specific service to do something |
| **Include a stable `id`, `type`, `version`, `occurredAt`** | Dedup, routing, evolution, ordering |
| **Thin vs fat events** | *Thin* (IDs only; consumers call back for details) avoid stale data but add coupling and load. *Fat* (full state) let consumers be autonomous but are larger and harder to evolve. State-carrying events are the usual compromise. |
| **Evolve compatibly** | Add optional fields; never remove or repurpose fields in-place; bump `version` for breaking changes and run both during migration |
| **Don't leak your database schema** | The event is a public contract, not a row dump |
| **Never put secrets or unnecessary PII in events** | The log is retained, replayable, and read by many teams |

### Schema management

For serious use, adopt a **Schema Registry** (Confluent, Redpanda, Apicurio) with **Avro**, **Protobuf**, or **JSON Schema**. Producers register schemas; the registry enforces compatibility rules (for example, backward compatible), preventing a producer from breaking consumers. With plain JSON you rely on discipline and tests (validate with `zod` on both sides, as in `09-api-development/04-validation.md`).

---

## Rebalancing, shutdown, and health

### Graceful shutdown

```js
const shutdown = async (signal) => {
  logger.info({ signal }, "Shutting down Kafka clients");
  try {
    await consumer.disconnect();       // stops fetching, finishes in-flight work, commits, leaves the group cleanly
    await producer.disconnect();       // flushes pending sends
  } finally {
    process.exit(0);
  }
};
process.on("SIGTERM", () => shutdown("SIGTERM"));
process.on("SIGINT", () => shutdown("SIGINT"));
```

A clean leave triggers a **fast** rebalance. A crash makes the group wait for `sessionTimeout` before reassigning partitions. See `16-production/02-graceful-shutdown-and-health-checks.md`.

### Rebalances

Rebalances pause consumption briefly while partitions are reassigned. They're triggered by consumers joining or leaving, crashes, deploys, and **missed heartbeats**. To reduce them:

- Shut down gracefully; avoid killing all pods at once (rolling deploys).
- Don't block the event loop (heartbeats come from the same thread), and call `heartbeat()` in long handlers.
- Keep `sessionTimeout` sensible; don't make handlers run longer than `maxPollInterval`-style limits.
- Use static membership (`groupInstanceId`) where supported to avoid rebalances on quick restarts.

Because partitions can be reassigned, in-flight work for a revoked partition may be repeated by the new owner. Another reason handlers must be idempotent.

### Consumer lag: the number to watch

**Lag** = (latest offset in a partition) − (the group's committed offset). It measures how far behind a consumer is.

```js
const admin = kafka.admin();
await admin.connect();
const offsets = await admin.fetchOffsets({ groupId: "billing-service", topics: ["orders"] });
const latest = await admin.fetchTopicOffsets("orders");
// lag per partition = Number(latest[i].offset) - Number(offsets[...][i].offset)
```

Or via CLI:

```bash
kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group billing-service
```

Alert on **lag that grows over time**. Steady small lag is normal. Growing lag means consumers can't keep up (scale consumers up to the partition count, optimize handlers, or add partitions).

Other signals: DLQ topic message count, under-replicated partitions, producer error rate, end-to-end latency (event time → processed time). Export them via Prometheus (`14-logging-observability/03-metrics-and-prometheus.md`), commonly through `kafka-exporter` or your managed provider's metrics.

---

## Security basics

- **TLS** for encryption in transit; **SASL** (SCRAM, OAUTHBEARER, or IAM on MSK) for authentication.
- **ACLs:** each service gets only the topics it needs (produce to `orders`; consume from `payments`).
- Don't expose brokers to the public internet.
- **No secrets/PII** in events unless necessary (and consider field-level encryption); retention means they live on.
- Topic and group names are part of your access model: use naming conventions per service/domain (`orders.v1`, `billing.invoices.v1`, `orders.dlq`).

---

## Testing

```js
// 1. Unit-test handlers as pure functions: no Kafka
test("handleEvent ignores duplicates", async () => {
  await handleEvent(event);
  await handleEvent(event);
  expect(await countInvoices(event.data.orderId)).toBe(1);
});

// 2. Integration: real broker via Testcontainers (Kafka or Redpanda)
//    produce → consume → assert, using a unique topic and groupId per test run

// 3. Contract tests: validate event payloads against the shared schema on BOTH producer and consumer sides

// 4. In app-level tests, inject a fake publisher (10-architecture/04-dependency-injection.md)
const events = [];
const app = buildApp({ eventPublisher: { publish: async (e) => events.push(e) } });
```

Use unique topics and `groupId`s per test run to avoid reading leftovers (`13-testing/`).

---

## Kafka vs the alternatives

| Need | Reach for |
|---|---|
| Background tasks with retries (emails, thumbnails) | **BullMQ / SQS**: simpler, built for jobs |
| Simple pub/sub fan-out, no replay | **Redis Pub/Sub** (`07-databases/redis/03-pub-sub.md`), SNS, NATS. Note Redis Pub/Sub is fire-and-forget: messages are lost if no subscriber is listening |
| Lightweight durable streams on Redis | **Redis Streams** (consumer groups, acknowledgements, modest scale) |
| Complex routing, per-message acks, priorities | **RabbitMQ** |
| Replayable event log, many consumers, high throughput | **Kafka** (or Redpanda, Pulsar) |
| Fully managed, low operations | **SQS/SNS**, **Pub/Sub**, **Kinesis**, or managed Kafka |

---

## Common mistakes

```js
// ❌ creating a producer per request (connection storm); create ONE at startup
// ❌ no key when ordering matters → events for the same entity land in different partitions and arrive out of order
// ❌ low-cardinality key (country, status) → hot partitions
// ❌ assuming order across partitions
// ❌ auto-committing before the work completes, then crashing → silently lost messages
// ❌ non-idempotent consumers → double charges/emails on rebalance or retry
// ❌ a poison message with no DLQ → one bad event blocks the partition forever
// ❌ blocking the event loop / long handlers without heartbeat() → rebalance storms
// ❌ more consumers than partitions → idle consumers
// ❌ too few partitions chosen up front; later increases remap keys and break ordering assumptions
// ❌ huge messages (MBs) instead of references to object storage
// ❌ publishing events without schema/version → one producer change breaks every consumer
// ❌ dual-write to the DB and Kafka without an outbox
// ❌ Kafka as a database or a task queue: it's a log
// ❌ no lag monitoring: you find out consumers fell behind from customer complaints
```

## Checklist

**Design**
- [ ] Chosen because you need replay / many consumers / high throughput, not "everything is Kafka"
- [ ] Topic naming convention; partition count planned for peak consumer parallelism
- [ ] Key chosen by "what must stay ordered", with good cardinality
- [ ] Events: unique `id`, past-tense `type`, `version`, `occurredAt`; no secrets; schema validated (or registry)

**Producing**
- [ ] One long-lived producer; idempotent; `acks: -1` for important data
- [ ] Outbox (or CDC) when the event must match a database change atomically

**Consuming**
- [ ] At-least-once with commit after success; **idempotent handlers** (dedupe by event ID)
- [ ] Poison-message strategy: retries with backoff, DLQ topic (and/or retry topics), alerting and replay tooling
- [ ] `heartbeat()` in long handlers; no blocked event loop
- [ ] Graceful shutdown (`consumer.disconnect()`)

**Operations**
- [ ] Replication factor 3, `min.insync.replicas` 2 in production
- [ ] TLS + SASL + ACLs; brokers private
- [ ] Monitor consumer lag, DLQ volume, under-replicated partitions, error rates
- [ ] Retention and compaction chosen per topic

## Wrap-up

This completes `11-async-processing/`. The through-line: **respond fast and do slow work elsewhere** (`01`), **assume duplicates and failures and design for them** (`02`), **make time-based work run exactly once** (`03`), and **use a log when many systems need the same facts, with replay** (`04`). The thread through all four is idempotency, so if you remember only one thing from this section, remember that.

## Next

Section **`12-realtime/`**: WebSockets and Socket.IO, authentication and rooms, and scaling real-time connections across instances with the Redis adapter. It's the other half of "tell the user what happened" when background jobs finish.
