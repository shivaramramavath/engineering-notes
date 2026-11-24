# High-Concurrency E-Commerce Flash Sale System in Node.js

> Architecting a flash sale inventory checkout engine handling 50,000+ requests per second with atomic Redis decr, BullMQ asynchronous order creation, and zero overselling.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Scale Target** | 50,000 RPS, < 50ms latency |
| **Technologies** | Node.js, Redis (Atomic Lua), PostgreSQL, BullMQ |

---

## 1. The Flash Sale Challenge

During flash sale events (e.g. 5,000 high-demand concert tickets or limited sneakers):
- Direct database writes will lock rows, exhaust connection pools, and crash PostgreSQL.
- Naive check-then-act logic (`if (stock > 0) stock--`) suffers from race conditions, causing catastrophic overselling.

---

## 2. High-Throughput Architecture

```
[ 50,000 Users ] ──> [ Nginx / Cloudflare Ingress ]
                                │
                                ▼
                   [ Node.js Fastify Cluster ]
                                │
                                ▼
                     [ Redis Cluster (RAM) ]
                  (Atomic Lua Script Stock DECR)
                                │
               ┌────────────────┴────────────────┐
               ▼                                 ▼
      [ Stock > 0: SUCCESS ]            [ Stock == 0: REJECTED ]
               │                                 │
               ▼                                 ▼
   [ Push to BullMQ Queue ]             [ Instant HTTP 409 ]
               │
               ▼
   [ Asynchronous DB Worker ]
               │
               ▼
      [ PostgreSQL Orders ]
```

---

## 3. Atomic Stock Deduction via Redis Lua Script

Lua scripts execute atomically in Redis single-threaded memory:

```javascript
import Redis from 'ioredis';

const redis = new Redis(process.env.REDIS_URL);

// Atomic check-and-decrement Lua script
const deductStockLua = `
  local stock = redis.call('get', KEYS[1])
  if not stock or tonumber(stock) <= 0 then
    return -1 -- Out of stock
  end
  local remaining = redis.call('decr', KEYS[1])
  return remaining
`;

export async function attemptOrder(userId, productId) {
  const stockKey = `stock:item:${productId}`;

  // Execute atomic Lua script
  const remaining = await redis.eval(deductStockLua, 1, stockKey);

  if (remaining < 0) {
    return { success: false, reason: 'OUT_OF_STOCK' };
  }

  // Stock secured! Enqueue order creation asynchronously:
  await orderQueue.add('createOrder', {
    userId,
    productId,
    reservedAt: Date.now(),
  });

  return { success: true, message: 'Order reservation confirmed' };
}
```
Zero row locks on PostgreSQL; instantaneous response for users.
