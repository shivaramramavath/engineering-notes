# System Design Blueprint: High-Concurrency E-Commerce Checkout & Flash Sale Inventory

## Metadata

- **Topic**: High-Contention Flash Sale Checkout & Distributed Inventory Allocation
- **Scale Requirements**: 100,000 concurrent checkout attempts on 1,000 limited inventory units in under 60 seconds (Flash Sale / Ticket Drop)
- **Target SLA**: 100% stock precision (zero overselling / zero phantom stock), sub-100ms response time, ACID consistency
- **Tech Stack**: FastAPI, Redis Cluster (Atomic Lua Stock Decrement), PostgreSQL (Transactional Ledger), Apache Kafka, Stripe / Payment Gateway

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Flash Sale Inventory Allocation**: Atomically claim 1 unit of stock with sub-millisecond latency.
2. **Checkout Reservation Window**: Hold claimed stock for 10 minutes while customer completes payment.
3. **Automatic Expiration & Release**: If payment fails or timer expires, automatically release held stock back to available pool.
4. **Zero Overselling**: Under no circumstances may 1,001 items be sold for a 1,000-unit inventory lot.

### 1.2 Concurrency Strategies Comparison

| Strategy | Performance | Complexity | Oversell Risk | Ideal Context |
|:---|:---|:---|:---|:---|
| **Pessimistic Lock (`FOR UPDATE`)** | Low (Heavy DB row lock queuing) | Low | Zero | Low-volume, standard B2B orders |
| **Optimistic Lock (`version_id`)** | Medium (Fails fast under high contention) | Medium | Zero | Low-to-moderate contention |
| **Redis In-Memory Atomic Reservation (DECR)** | Extreme (> 100,000 QPS) | High | Zero (with Lua guards) | High-volume flash sales & ticket drops |

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |       Flash Sale Clients (100k Peak QPS)      |
                 +-----------------------+-----------------------+
                                         |
                                         | POST /api/v1/checkout/reserve
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI Edge Gateway Cluster (50 Nodes)       |
                 +-----------------------+-----------------------+
                                         |
                                         | 1. Execute Atomic Redis Lua Script:
                                         |    - Check stock > 0
                                         |    - Decrement stock counter
                                         |    - Create reservation token (TTL: 10m)
                                         v
                         +---------------+---------------+
                         | Redis Cluster (Memory Master) |
                         +---------------+---------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
             [Stock Available: OK]                [Stock == 0: SOLD OUT]
                       |                                   |
                       v                                   v
         +-------------+-------------+             +-------+-------------+
         | Generate Reservation ID   |             | Return 409 Conflict |
         | & Send Stripe ClientSecret|             | "Sold Out" (< 5ms)  |
         +-------------+-------------+             +---------------------+
                       |
                       | Customer Pays via Stripe
                       v
         +-------------+-------------+
         | Stripe Webhook Receiver   |
         | POST /webhooks/stripe     |
         +-------------+-------------+
                       |
                       | 2. Publish "order.paid" event
                       v
         +-------------+-------------+
         | Apache Kafka / Debezium   |
         +-------------+-------------+
                       |
                       v (Async DB Writer Worker)
         +-------------+-------------+
         | PostgreSQL Ledger DB      |
         | Permanent Order Record &  |
         | Financial Audit Log       |
         +---------------------------+
```

---

## 3. Atomic Redis Flash Sale Reservation Script

```lua
-- KEYS[1]: "stock:item_123"
-- KEYS[2]: "reservations:item_123"
-- ARGV[1]: user_id
-- ARGV[2]: quantity (usually 1)
-- ARGV[3]: hold_seconds (e.g. 600)

local stock = tonumber(redis.call('GET', KEYS[1]) or 0)
local qty = tonumber(ARGV[2])

if stock >= qty then
    -- Atomically deduct from available pool
    redis.call('DECRBY', KEYS[1], qty)
    -- Record reservation under user id
    local res_id = ARGV[1] .. ':' .. redis.call('TIME')[1]
    redis.call('HSET', KEYS[2], res_id, qty)
    -- Expire reservation key if needed or push to delayed stream
    return {1, res_id} -- Success
else
    return {0, "SOLD_OUT"} -- Out of stock
end
```

---

## 4. FastAPI Production Implementation Blueprint

```python
import uuid
from fastapi import APIRouter, HTTPException, Header, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter(prefix="/checkout", tags=["Checkout"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)

RESERVE_LUA = """
local stock = tonumber(redis.call('GET', KEYS[1]) or 0)
local qty = tonumber(ARGV[2])
if stock >= qty then
    redis.call('DECRBY', KEYS[1], qty)
    local res_id = ARGV[1] .. ':' .. tostring(redis.call('TIME')[1])
    redis.call('SET', 'res:' .. res_id, qty, 'EX', tonumber(ARGV[3]))
    return {1, res_id}
else
    return {0, 'SOLD_OUT'}
end
"""


class ReservationRequest(BaseModel):
    item_id: str
    user_id: str
    quantity: int = 1


@router.post("/reserve", status_code=status.HTTP_201_CREATED)
async def reserve_stock(
    req: ReservationRequest,
    idempotency_key: str = Header(..., alias="Idempotency-Key"),
):
    stock_key = f"stock:{req.item_id}"

    # Atomic reservation evaluation in Redis
    res = await redis_client.eval(RESERVE_LUA, 1, stock_key, req.user_id, req.quantity, 600)
    success, res_id_or_err = res[0], res[1]

    if not success:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Item is sold out or insufficient quantity available.",
        )

    return {
        "status": "RESERVED",
        "reservation_id": res_id_or_err,
        "expires_in_seconds": 600,
        "checkout_url": f"https://pay.store.com/checkout/{res_id_or_err}",
    }
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Customer Abandons Checkout** | Stock remains deducted; unsold inventory stays trapped. | Redis reservation key has 10-minute TTL (`EX 600`). A background worker listens to Redis Keyspace expiration notifications (`__keyevent@0__:expired`) and automatically executes `INCRBY stock:{item} {qty}`. |
| **Network Split during Payment Webhook** | User is charged by Stripe, but webhook fails to reach FastAPI. | Stripe automatically retries webhook delivery with exponential backoff for 72 hours; customer portal provides a manual "Verify My Payment" sync button. |
| **Redis Crash before DB Sync** | In-memory reservations lost on power outage. | Configure Redis `appendonly yes` (AOF with `appendfsync everysec`) or Redis Enterprise active-active replication to maintain durability guarantees. |
