# Pagination Strategies & Mitigating the N+1 Query Problem in Node.js

> Engineering high-scale data retrieval: Offset vs Keyset (Cursor) pagination, and resolving the N+1 query dilemma using dataloader and relational joins.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Database** | PostgreSQL 16+ |
| **Patterns** | Keyset / Cursor Pagination, Dataloader Batching |

---

## 1. Offset Pagination vs. Keyset (Cursor) Pagination

### Offset Pagination (`OFFSET 10000 LIMIT 20`)
- **How it works**: The database scans through 10,020 rows, discards the first 10,000, and returns the last 20.
- **Flaws**:
  1. **Performance Cliff**: As offset increases, query execution time degrades linearly ($O(N)$ table scan).
  2. **Page Drift**: If rows are inserted or deleted while a user navigates, users see duplicate items or skip items entirely.

### Keyset / Cursor Pagination (`WHERE id < $last_id ORDER BY id DESC LIMIT 20`)
- **How it works**: Directly seeks the B-tree index from the last seen cursor ID.
- **Benefits**: Constant $O(\log N)$ performance regardless of whether reading page 1 or page 10,000. Immune to page drift.

```javascript
// Idiomatic Keyset Pagination in Node.js
export async function getPaginatedPosts({ cursor, limit = 20 }) {
  const query = cursor
    ? 'SELECT * FROM posts WHERE id < $1 ORDER BY id DESC LIMIT $2'
    : 'SELECT * FROM posts ORDER BY id DESC LIMIT $1';

  const params = cursor ? [cursor, limit + 1] : [limit + 1];
  const { rows } = await pool.query(query, params);

  const hasNextPage = rows.length > limit;
  const items = hasNextPage ? rows.slice(0, limit) : rows;
  const nextCursor = hasNextPage ? items[items.length - 1].id : null;

  return { items, nextCursor };
}
```

---

## 2. Solving the N+1 Query Problem

When rendering a list of 50 posts and fetching the author for each post:
- **Naive approach**: 1 query for posts + 50 individual queries for authors = **51 database roundtrips (N+1)**.

### Solution 1: SQL Joins with JSON Aggregation
```sql
SELECT 
  p.id, 
  p.title, 
  json_build_object('id', u.id, 'name', u.name) as author
FROM posts p
JOIN users u ON u.id = p.author_id
WHERE p.status = 'published'
LIMIT 20;
```
Executes in **1 single database query**!

### Solution 2: Batching with DataLoader (for GraphQL / Services)
```javascript
import DataLoader from 'dataloader';

// Batches individual calls across the same event loop tick into a single IN (...) query
export const userLoader = new DataLoader(async (userIds) => {
  const { rows } = await pool.query(
    'SELECT * FROM users WHERE id = ANY($1)',
    [userIds]
  );
  // Map rows back to the exact order of requested IDs:
  const userMap = new Map(rows.map(u => [u.id, u]));
  return userIds.map(id => userMap.get(id) || null);
});
```
