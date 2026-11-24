# PostgreSQL `LISTEN`/`NOTIFY` & Logical Replication from Node.js

> Building real-time event-driven notifications directly from PostgreSQL using pub/sub (`LISTEN`/`NOTIFY`) and Change Data Capture (CDC).

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Driver** | `pg` Client (dedicated connection) |
| **Patterns** | Event Sourcing, Cache Invalidation, Live Data Push |

---

## 1. What is `LISTEN`/`NOTIFY`?

PostgreSQL provides a built-in asynchronous publish/subscribe mechanism:
- A client issues `LISTEN <channel_name>`.
- Any database transaction or trigger issues `NOTIFY <channel_name>, '<payload>'`.
- All listening clients receive the notification payload immediately upon transaction commit.

> **Important**: `LISTEN`/`NOTIFY` requires a **dedicated persistent client connection**. Never use a client checked out from a shared connection pool, as it must stay open indefinitely.

---

## 2. Implementing `LISTEN`/`NOTIFY` in Node.js

```javascript
import pg from 'pg';

const client = new pg.Client({ connectionString: process.env.DATABASE_URL });

export async function startDatabaseListener(onOrderCreated) {
  await client.connect();

  // Listen to order_events channel
  await client.query('LISTEN order_events');
  console.log('PostgreSQL LISTEN active on channel: order_events');

  client.on('notification', (msg) => {
    console.log(`[PG NOTIFY] Channel: ${msg.channel}`);
    const payload = JSON.parse(msg.payload);
    onOrderCreated(payload);
  });

  client.on('error', (err) => {
    console.error('Listener client error:', err);
  });
}
```

### PostgreSQL Trigger Setup
```sql
CREATE OR REPLACE FUNCTION notify_order_created()
RETURNS trigger AS $$
BEGIN
  PERFORM pg_notify('order_events', json_build_object(
    'orderId', NEW.id,
    'totalCents', NEW.total_cents,
    'status', NEW.status
  )::text);
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER order_after_insert
AFTER INSERT ON orders
FOR EACH ROW EXECUTE FUNCTION notify_order_created();
```

Whenever a new order is inserted into PostgreSQL (even from background workers or direct SQL scripts), your Node.js application receives an instant real-time push notification!
