# Cache-Aside, Write-Through & Stampede Prevention

| Field | Value |
| :--- | :--- |
| **Category** | Distributed & Cloud Patterns (Caching) |
| **Origin** | Distributed Caching Standard Patterns |
| **Difficulty** | Intermediate |
| **Prerequisite(s)** | [Proxy](file:///E:/Learning/engineering-notes/08-solid-design-patterns/06-structural-patterns/07-proxy.md) |
| **Status** | Complete |

> **TL;DR**: **Cache-Aside** (lazy load) lets the app check Cache → on miss, read DB → populate Cache with TTL. **Write-Through** updates Cache + DB together on writes. Always protect hot keys against **Cache Stampede (Thundering Herd)** using singleflight/locks and TTL jitter.

---

## 1. Caching Strategies Compared

| Strategy | Read Path | Write Path | Best For |
| :--- | :--- | :--- | :--- |
| **Cache-Aside (Lazy)** | App checks Cache; on miss reads DB & sets Cache | App writes DB & **deletes** (invalidates) Cache key | Read-heavy general workloads (Redis) |
| **Read-Through** | Cache provider loads from DB on miss | App writes DB / invalidates | Managed cache libraries (Caffeine) |
| **Write-Through** | Read from Cache | Write to Cache + DB synchronously | Strong cache-DB consistency |
| **Write-Behind (Write-Back)** | Read from Cache | Write to Cache now, flush DB async | Extreme write throughput (risks data loss on crash) |

---

## 2. Three Classic Cache Failure Modes

1. **Cache Stampede (Thundering Herd)**: A hot key expires at `12:00:00`, and 5,000 concurrent requests miss simultaneously and slam the DB. **Fix**: Singleflight / mutex per key + **TTL Jitter** (`ttl + random(0, 60s)`).
2. **Cache Penetration**: Attackers query non-existent IDs (`id=-999`) that always miss Cache and hit DB. **Fix**: Cache negative results (`"NULL"` with short TTL) or use a **Bloom Filter**.
3. **Stale Read Race**: On write, updating DB then **deleting** the cache key (`DEL key`) is safer than overwriting cache (`SET key`) under concurrent updates.

---

## 3. Interview Questions

<details>
<summary>1. On a write in Cache-Aside, why should you DELETE the cache key instead of updating it?</summary>

Two concurrent writes (`T1` writes `v1` to DB, `T2` writes `v2` to DB) can interleave cache updates (`T2` sets `v2`, then slow `T1` overwrites cache with stale `v1`). Deleting the key forces the next read to fetch fresh `v2`.

</details>

<details>
<summary>2. How do you prevent a Cache Stampede on a hot key?</summary>

Use **request coalescing (singleflight)** so only 1 caller recomputes the missing key while others wait, add **random jitter** to TTLs, or refresh hot keys proactively before expiration.

</details>

<details>
<summary>3. What is Cache Penetration and how do you stop it?</summary>

Repeated queries for keys that don't exist in Cache *or* DB. Stop it by caching negative sentinel values with a short TTL or checking a **Bloom Filter** in front of the cache.

</details>

---

## 4. Cheat Sheet

- **Cache-Aside**: Read → Miss → DB → `SETEX` (with **TTL + jitter**); Write → DB → **`DEL` key**.
- **Hot keys**: Singleflight + negative caching / Bloom filter.

---

## 5. Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [CQRS Read Models](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/09-cqrs-read-models.md) | [Module 11](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/README.md) | [API Gateway & BFF](file:///E:/Learning/engineering-notes/08-solid-design-patterns/11-distributed-and-cloud-patterns/11-api-gateway-and-bff.md) |
