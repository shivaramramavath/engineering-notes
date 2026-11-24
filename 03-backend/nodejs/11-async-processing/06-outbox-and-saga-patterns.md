# Transactional Outbox & Saga Patterns in Node.js

> Solving dual-write hazards in distributed systems using the Transactional Outbox pattern and managing distributed multi-service transactions with Sagas.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Patterns** | Transactional Outbox, Choreography vs Orchestration Saga |

---

## 1. The Dual-Write Problem

A common architectural disaster in microservices:
```javascript
// ❌ DUAL-WRITE HAZARD:
await db.orders.create(order); // 1. Writes to PostgreSQL
await kafkaProducer.send(event); // 2. Network timeout! Event lost forever!
```
If the database write succeeds but the Kafka/RabbitMQ publish fails (or the process crashes between lines 1 and 2), your system becomes permanently inconsistent.

---

## 2. The Transactional Outbox Pattern

Instead of publishing to the message broker directly from HTTP handlers:
1. Write the domain mutation **AND** an `outbox` record inside the **same atomic database transaction**.
2. A separate background worker reads unpublished outbox rows, publishes them to Kafka/RabbitMQ, and marks them published.

```sql
CREATE TABLE outbox_messages (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  aggregate_type VARCHAR(50) NOT NULL,
  aggregate_id VARCHAR(50) NOT NULL,
  event_type VARCHAR(50) NOT NULL,
  payload JSONB NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  published_at TIMESTAMP
);
```

### Writing Atomically
```javascript
export async function createOrder(orderData) {
  return withTransaction(async (client) => {
    // 1. Insert domain entity
    const orderRes = await client.query(
      'INSERT INTO orders (user_id, total) VALUES ($1, $2) RETURNING id',
      [orderData.userId, orderData.total]
    );
    const orderId = orderRes.rows[0].id;

    // 2. Insert outbox record in the SAME transaction!
    await client.query(`
      INSERT INTO outbox_messages (aggregate_type, aggregate_id, event_type, payload)
      VALUES ($1, $2, $3, $4)
    `, ['Order', orderId, 'OrderCreated', JSON.stringify({ orderId, ...orderData })]);

    return { orderId };
  });
}
```

---

## 3. The Saga Pattern (Distributed Transactions)

When a business transaction spans multiple microservices (e.g. Order Service $\rightarrow$ Payment Service $\rightarrow$ Inventory Service), traditional 2-phase commit (2PC) does not scale.

**Saga**: A sequence of local transactions where each step updates its local database and publishes an event that triggers the next step.
- **Compensating Transactions**: If any intermediate step fails (e.g. Payment Rejected), the saga executes a backwards sequence of compensating transactions (e.g. Cancel Order, Release Reserved Inventory) to restore consistency.
