# Rate Limiter & Throttling

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Traffic Control) |
| **Origin** | Networking Traffic Shaping / API Gateways |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Backpressure & Flow Control](file:///E:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/13-backpressure-and-flow-control.md) |
| **Status** | Complete |

> **TL;DR**: Rate Limiting caps how many requests a caller/tenant can make per time window—protecting services from abuse, noisy neighbors, and billing spikes, and returning **`429 Too Many Requests`** with `Retry-After`.

---

## 1. Four Classic Algorithms

| Algorithm | How It Works | Burst Handling | Memory |
| :--- | :--- | :--- | :--- |
| **Token Bucket** | Bucket refills at `R` tokens/sec up to `C` capacity; each request takes 1 token | Allows short bursts up to `C` | $O(1)$ per key |
| **Leaky Bucket** | Queue drains at constant rate `R` | Smooths bursts into steady rate | $O(\text{queue})$ |
| **Fixed Window Counter** | Increment counter for `floor(now / window)` | **Edge-burst bug**: 2x limit at window boundary | $O(1)$ |
| **Sliding Window Log / Counter** | Tracks timestamps or weighted adjacent windows | Accurate, no boundary spike | $O(N)$ or $O(1)$ approx |

---

## 2. Token Bucket TypeScript Example (Verified)

```ts
class TokenBucket {
  private tokens: number;
  private lastRefill = Date.now();
  private readonly capacity: number;
  private readonly refillPerSec: number;
  constructor(capacity: number, refillPerSec: number) {
    this.capacity = capacity; this.tokens = capacity; this.refillPerSec = refillPerSec;
  }
  allow(): boolean {
    const now = Date.now();
    this.tokens = Math.min(this.capacity, this.tokens + ((now - this.lastRefill) / 1000) * this.refillPerSec);
    this.lastRefill = now;
    if (this.tokens >= 1) { this.tokens -= 1; return true; }
    return false;
  }
}
```

---

## 3. Distributed Rate Limiting

In multi-instance deployments, store counters in **Redis** using an **atomic Lua script** (or `INCR` + `EXPIRE`) so check-and-increment has no race condition. For ultra-high scale, combine local in-memory approximate counters with periodic Redis sync.

---

## 4. Interview Questions

<details>
<summary>1. What is the Fixed Window boundary burst problem?</summary>

If the limit is 100 req/min, a user can send 100 requests at `00:59` and another 100 at `01:01`—achieving 200 requests in 2 seconds. Sliding Window or Token Bucket fixes this.

</details>

<details>
<summary>2. Which HTTP headers should a rate-limited response include?</summary>

Status `429 Too Many Requests`, `Retry-After` (seconds/timestamp), and `RateLimit-Limit` / `RateLimit-Remaining` / `RateLimit-Reset`.

</details>

<details>
<summary>3. Why use a Redis Lua script for distributed rate limiting?</summary>

A Lua script executes atomically on the Redis server, preventing race conditions between reading the token count and decrementing it.

</details>

---

## 5. Cheat Sheet

- **Token Bucket** = standard default (bursts up to capacity, $O(1)$ state).
- **Distributed**: Atomic Redis Lua script; return `429` + `Retry-After`.

---

## 6. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Bulkhead](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/04-bulkhead.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [Idempotency Key & Idempotent Consumer](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/06-idempotency-key-and-idempotent-consumer.md) |
