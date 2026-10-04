# 19 — System Design

Everything earlier in this repo teaches **individual building blocks**: Express, MongoDB, Redis, queues, WebSockets, Docker. System design is about **combining them** — choosing which blocks, why, and how they behave when traffic grows, machines fail, and requirements collide.

Each file here walks through one classic design problem, built on pieces you have already met.

## What this folder covers

| File | Problem | Main building blocks |
|------|---------|----------------------|
| `01-scalable-api-and-rate-limiter.md` | A stateless API that scales horizontally, plus a distributed rate limiter | Load balancer, Redis, Lua scripts |
| `02-url-shortener.md` | Short links with fast redirects | ID generation, caching, read-heavy storage |
| `03-chat-system.md` | Real-time messaging | WebSockets, Redis pub/sub, message storage |
| `04-notification-system.md` | Email/SMS/push at scale | Queues, workers, retries, preferences |
| `05-file-upload-system.md` | Large uploads and downloads | Pre-signed URLs, object storage, async processing |
| `06-job-processing-system.md` | Reliable background work | BullMQ, workers, retries, DLQ, scheduling |

## A framework for any design problem

Use the same sequence every time. It keeps you from jumping into technology choices before you understand the problem.

**1. Clarify requirements**
- *Functional* — what must the system do? (create a short link, send a message)
- *Non-functional* — how well? (latency, availability, durability, consistency)
- Ask what is **out of scope** — it is as important as what is in.

**2. Estimate scale** (back-of-envelope)
- Requests per second, read/write ratio, data size, growth
- The numbers decide the architecture: 10 requests/sec and 10,000 requests/sec are different problems.

**3. Define the API and data model**
- The endpoints (or events) and the core entities

**4. Sketch the high-level design**
- Boxes and arrows: clients, load balancer, services, databases, caches, queues

**5. Deep-dive the hard parts**
- The bottleneck, the failure mode, the tricky algorithm

**6. Discuss trade-offs and failure**
- What breaks first? What do you give up? What would you change at 10× scale?

## Back-of-envelope cheat sheet

Handy numbers for estimates:

| Quantity | Rough value |
|----------|-------------|
| Seconds per day | ~86,400 (call it 10⁵) |
| 1 million requests/day | ~12 requests/second |
| 100 million requests/day | ~1,200 requests/second |
| Peak traffic | Often 2–10× the average |
| Read from memory (Redis) | ~sub-millisecond |
| SSD / database indexed read | ~1–10 ms |
| Cross-region network round trip | ~100+ ms |
| One Node.js process | Can handle many thousands of simple req/s; limited by CPU work and downstream calls |

These are **orders of magnitude**, not guarantees. The point is to reason about whether you need one server or fifty, one database or a sharded cluster.

## Recurring principles

These themes appear in every file in this folder:

- **Keep services stateless** — push state into Redis or a database so any instance can serve any request (`16-production/`)
- **Cache read-heavy data** — and plan for invalidation (`07-databases/redis/02-caching-and-sessions.md`)
- **Move slow work off the request path** — queue it (`11-async-processing/`)
- **Design for failure** — retries, timeouts, idempotency, dead-letter queues (`09-api-development/06-idempotency.md`)
- **Pick consistency vs availability deliberately** — you rarely get both perfectly (CAP trade-off)
- **Measure** — you cannot fix a bottleneck you cannot see (`14-logging-observability/`)

## A note on scope

These are **teaching designs**, scaled to be understandable. Real systems at large companies are far more involved. Treat the numbers and technology choices as reasonable defaults and reasoning exercises, not as the single right answer — in design discussions, *explaining why* matters more than the specific tool named.

## Suggested reading order

Start with `01` (it introduces rate limiting and horizontal scaling used throughout), then take the others in any order. `06` (job processing) is useful before `04` and `05`, since both rely on background workers.

## Next

**`01-scalable-api-and-rate-limiter.md`** designs a horizontally scalable API and builds a distributed rate limiter on Redis.
