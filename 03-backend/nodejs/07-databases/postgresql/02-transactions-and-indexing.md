# PostgreSQL Transactions, Isolation Levels & Indexing in Node.js

> Mastering database transactions (ACID), transaction isolation levels (`READ COMMITTED`, `REPEATABLE READ`, `SERIALIZABLE`), B-tree indexes, and index design strategies.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Driver** | `pg` (`node-postgres`) |
| **Concepts** | ACID, Isolation Levels, B-tree, Partial Indexes, Composite Indexes |

---

## 1. Transaction Isolation Levels in PostgreSQL

PostgreSQL supports 4 standard SQL isolation levels:

| Isolation Level | Dirty Reads? | Non-Repeatable Reads? | Phantom Reads? | Serialization Anomaly? |
| :--- | :---: | :---: | :---: | :---: |
| **Read Committed** (Default) | ❌ No | ✅ Allowed | ✅ Allowed | ✅ Allowed |
| **Repeatable Read** | ❌ No | ❌ No | ❌ No | ✅ Allowed |
| **Serializable** | ❌ No | ❌ No | ❌ No | ❌ No |

```javascript
// Setting isolation level on transaction start
await client.query('BEGIN TRANSACTION ISOLATION LEVEL REPEATABLE READ');
```

---

## 2. Preventing Race Conditions: `SELECT ... FOR UPDATE`

When debiting an account balance or reserving inventory during concurrent requests:

```javascript
export async function transferFunds(fromUserId, toUserId, amount) {
  return withTransaction(async (client) => {
    // 1. Lock the sender row to prevent concurrent double-spending
    const senderRes = await client.query(
      'SELECT balance FROM accounts WHERE user_id = $1 FOR UPDATE',
      [fromUserId]
    );

    const balance = senderRes.rows[0]?.balance;
    if (balance < amount) {
      throw new Error('Insufficient funds');
    }

    // 2. Perform balance deduction and credit
    await client.query(
      'UPDATE accounts SET balance = balance - $1 WHERE user_id = $2',
      [amount, fromUserId]
    );
    await client.query(
      'UPDATE accounts SET balance = balance + $1 WHERE user_id = $2',
      [amount, toUserId]
    );

    return { success: true };
  });
}
```

---

## 3. Indexing Strategies for Node.js Backends

### Composite Indexes & Column Ordering
When querying `WHERE organization_id = $1 AND status = $2 ORDER BY created_at DESC`:
```sql
-- Most selective / equality column first, range/sort column last
CREATE INDEX idx_orders_org_status_created 
ON orders (organization_id, status, created_at DESC);
```

### Partial Indexes
Index only the active subset of rows (saving RAM and disk I/O):
```sql
-- Indexes only unread notifications (90% of notifications are read!)
CREATE INDEX idx_notifications_unread 
ON notifications (user_id) 
WHERE is_read = FALSE;
```
