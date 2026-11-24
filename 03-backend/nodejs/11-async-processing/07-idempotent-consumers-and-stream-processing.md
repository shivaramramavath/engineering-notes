# Idempotent Consumers & Stream Processing in Node.js

> Building resilient message consumers that handle at-least-once delivery duplicates safely using Redis idempotency locks and stream offsets.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Guarantees** | At-Least-Once Delivery $\rightarrow$ Exactly-Once Processing via Idempotency |

---

## 1. At-Least-Once Delivery Reality

In message brokers (RabbitMQ, SQS, Kafka), network timeouts during consumer acknowledgment cause messages to be re-delivered.
**Message consumers MUST be designed as idempotent**: Processing the identical message twice must produce the exact same outcome as processing it once without side-effects.

---

## 2. Idempotent Consumer Pattern with Redis

```javascript
import Redis from 'ioredis';

const redis = new Redis(process.env.REDIS_URL);

export async function processMessageIdempotently(messageId, jobPayload, processor) {
  const lockKey = `processed:job:${messageId}`;

  // Atomic SET with NX (set only if Not eXists) and 24h TTL
  const acquired = await redis.set(lockKey, 'in_progress', 'EX', 86400, 'NX');

  if (!acquired) {
    console.log(`[Duplicate Ignored] Message ${messageId} already processed or in progress`);
    return { duplicate: true };
  }

  try {
    const result = await processor(jobPayload);
    // Mark completed
    await redis.set(lockKey, 'completed', 'EX', 86400);
    return { success: true, result };
  } catch (err) {
    // Release lock on unexpected failure so retry can execute
    await redis.del(lockKey);
    throw err;
  }
}
```
Ensures zero duplicate charges, duplicate emails, or double database writes even under heavy network retries.
