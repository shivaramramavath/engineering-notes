# System Design Blueprint: High-Scale URL Shortener Service (TinyURL / Bitly)

## Metadata

- **Topic**: Distributed High-Throughput URL Shortener Architecture
- **Scale Requirements**: 100 Million daily active links, 1 Billion daily redirect queries (11,600 QPS read, 1,160 QPS write), 10:1 Read-to-Write ratio
- **Target SLA**: 99.99% availability, sub-10ms p99 redirect latency
- **Tech Stack**: FastAPI, Uvicorn, Redis (Cluster / Replication), PostgreSQL (Partitioned), Nginx / Cloudflare CDN

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Shorten URL**: Given an arbitrary long URL, generate a globally unique, compact alphanumeric alias (e.g. `https://sho.rt/aZ9k2`).
2. **High-Speed Redirection**: Navigating to `https://sho.rt/{code}` redirects users via HTTP `307 Temporary Redirect` (preserving analytics) or `301 Permanent Redirect` (browser cached).
3. **Custom Aliases**: Users can specify custom vanity slugs (e.g. `sho.rt/summer-sale`).
4. **Link Expiration & TTL**: Links can optionally expire after a defined date/time.
5. **Real-Time Click Analytics**: Aggregate clicks by country, referrer, and timestamp without degrading redirection throughput.

### 1.2 Non-Functional Requirements & Capacity Estimations
- **Write QPS**: $100\text{M links} / 86400\text{s} \approx 1,160\text{ writes/sec}$.
- **Read QPS**: $1\text{B redirects} / 86400\text{s} \approx 11,574\text{ reads/sec}$. Peak load (3x) $\approx 35,000\text{ QPS}$.
- **Storage Calculation**:
  - Average record size: `id (8B) + short_code (8B) + original_url (512B) + created_at (8B) + user_id (16B) \approx 600\text{ bytes}`.
  - Daily storage: $100\text{M} \times 600\text{ bytes} \approx 60\text{ GB / day}$.
  - 5-Year Storage: $60\text{ GB} \times 365 \times 5 \approx 110\text{ TB}$.
- **Memory / Cache Calculation (80/20 Rule)**:
  - Cache 20% of daily active read links in Redis: $0.20 \times 1\text{B} \times 600\text{ bytes} \approx 120\text{ GB RAM}$. Readily fits within a 3-node Redis Cluster.

---

## 2. High-Level Architecture Diagram

```
                                      +------------------------+
                                      |    Global Edge CDN     |
                                      |  (Cloudflare / Fastly) |
                                      +-----------+------------+
                                                  |
                          +-----------------------+-----------------------+
                          |                                               |
                          | (Cache Miss / Writes)                         | Cached Static /
                          v                                               | Hot Redirects
             +------------+------------+                                  v
             |     Load Balancer       |                         +--------+--------+
             |       (AWS ALB)         |                         | Edge Cache (POP)|
             +------------+------------+                         +-----------------+
                          |
             +------------+------------+
             | FastAPI Application Pods| (Horizontally Scaled ASGI instances)
             +---+------------------+--+
                 |                  |
    Read Hit     |                  | Write / Cache Miss
    (< 5ms)      v                  v
+----------------+----+       +-----+---------------+
|  Redis Cluster      |       | PostgreSQL Primary  |
|  - Key: "url:{code}"|       | (Range Partitioned  |
|  - Value: JSON/URL  |       |  by created_at)     |
|  - In-Memory Cache  |       +-----+---------------+
+----------------+----+             | Async Streaming Replication
                 |                  v
                 | Click Increment  +---------------------+
                 v                  | PostgreSQL Read     |
+----------------+----+             | Replicas (Read Pool)|
| Redis Stream        |             +---------------------+
| "stream:clicks"     |
+--------+------------+
         |
         v
+--------+------------+
| Click Analytics     |
| Worker (Batched)    |-------> ClickHouse / TimescaleDB
+---------------------+
```

---

## 3. Data Model & Key Generation Strategies

### 3.1 Key Generation Approaches
1. **MD5 / SHA-256 Truncation**: High collision probability requiring iterative hashing or DB lookup loops.
2. **Distributed ID Generator (Snowflake / ULID) + Base62**: Guarantees uniqueness across distributed nodes without inter-service coordination.
3. **Pre-Generated Key Service (KGS)**: Dedicated microservice with Redis tokens dispensing pre-allocated unique 7-character Base62 keys.

### 3.2 Relational Schema (PostgreSQL Partitioned Table)

```sql
CREATE TABLE short_links (
    id BIGINT NOT NULL,
    short_code VARCHAR(16) NOT NULL,
    original_url TEXT NOT NULL,
    user_id UUID NULL,
    is_active BOOLEAN NOT NULL DEFAULT TRUE,
    expires_at TIMESTAMPTZ NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (created_at, id)
) PARTITION BY RANGE (created_at);

CREATE INDEX ix_short_links_code ON short_links (short_code);
```

---

## 4. FastAPI Production Implementation Blueprint

```python
from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException, status
from fastapi.responses import RedirectResponse
import redis.asyncio as aioredis
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine

app = FastAPI(title="URL Shortener Engine")

redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)
engine = create_async_engine("postgresql+asyncpg://postgres:pass@localhost:5432/shortener")
session_factory = async_sessionmaker(engine, expire_on_commit=False)


@app.get("/{short_code}", response_class=RedirectResponse, status_code=status.HTTP_307_TEMPORARY_REDIRECT)
async def redirect_short_url(short_code: str):
    cache_key = f"url:{short_code}"

    # 1. Redis In-Memory Lookup (< 2ms)
    target_url = await redis_client.get(cache_key)
    if target_url:
        # Asynchronously record click event without delaying response
        await redis_client.xadd("stream:clicks", {"code": short_code, "ts": datetime.now(timezone.utc).isoformat()})
        return RedirectResponse(url=target_url, status_code=status.HTTP_307_TEMPORARY_REDIRECT)

    # 2. Database Read Replica Lookup (< 15ms)
    async with session_factory() as session:
        # Execute query against PostgreSQL Read Replica
        result = await session.execute(
            select(short_links.c.original_url).where(short_links.c.short_code == short_code)
        )
        url_row = result.scalar_one_or_none()

    if not url_row:
        raise HTTPException(status_code=404, detail="Short code not found")

    # 3. Populate Redis Cache with 24-Hour TTL
    await redis_client.set(cache_key, url_row, ex=86400)
    await redis_client.xadd("stream:clicks", {"code": short_code, "ts": datetime.now(timezone.utc).isoformat()})

    return RedirectResponse(url=url_row, status_code=status.HTTP_307_TEMPORARY_REDIRECT)
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Redis Cache Stampede** | Hot link expires; 50,000 concurrent requests hit PostgreSQL directly, crashing database pool. | Use **Probabilistic Early Expiration (XFetch algorithm)** or distributed mutex lock (`SET NX EX`) so only 1 worker refreshes cache. |
| **Hash Collision in Custom Aliases** | Two users register same alias simultaneously. | Database `UNIQUE INDEX` constraint on `short_code` catches collision, returning HTTP `409 Conflict`. |
| **High Write Latency** | DB primary overloaded during marketing campaigns. | Pre-generate short codes asynchronously into Redis queue; link creation becomes an in-memory `RPOP` + async bulk DB write. |
| **Spam & Malicious Redirections** | Attackers shorten phishing and malware URLs. | Integrate Google Safe Browsing API / VirusTotal webhook validation prior to accepting link creation. |
