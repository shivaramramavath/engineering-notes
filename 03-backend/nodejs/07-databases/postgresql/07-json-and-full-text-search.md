# PostgreSQL JSONB & Full-Text Search in Node.js

> Leveraging PostgreSQL's hybrid relational/document capabilities via JSONB indexing (`GIN`) and language-aware Full-Text Search (`tsvector`, `tsquery`).

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Database** | PostgreSQL 16+ |
| **Features** | `JSONB`, `GIN` index, `to_tsvector`, `websearch_to_tsquery` |

---

## 1. JSONB in Node.js: The Best of Both Worlds

PostgreSQL's `JSONB` stores decomposed binary JSON. It supports indexing, nested key extraction, and containment queries while maintaining ACID transactions:

```sql
CREATE TABLE audit_events (
  id SERIAL PRIMARY KEY,
  event_type VARCHAR(50) NOT NULL,
  metadata JSONB NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- GIN (Generalized Inverted Index) for fast JSONB containment search:
CREATE INDEX idx_audit_metadata ON audit_events USING gin (metadata);
```

### Querying JSONB from Node.js
```javascript
export async function getEventsByUser(userId) {
  // Uses the JSONB containment operator (@>) backed by the GIN index
  const sql = `
    SELECT * FROM audit_events 
    WHERE metadata @> $1::jsonb
  `;
  const filter = JSON.stringify({ actor: { id: userId } });
  const { rows } = await pool.query(sql, [filter]);
  return rows;
}
```

---

## 2. Native Full-Text Search in PostgreSQL

Skip complex Elasticsearch clusters for standard search requirements using native Postgres Full-Text Search:

```sql
CREATE TABLE articles (
  id SERIAL PRIMARY KEY,
  title TEXT NOT NULL,
  body TEXT NOT NULL,
  search_vector tsvector GENERATED ALWAYS AS (
    to_tsvector('english', coalesce(title, '') || ' ' || coalesce(body, ''))
  ) STORED
);

CREATE INDEX idx_articles_search ON articles USING gin (search_vector);
```

### Executing Search from Node.js
```javascript
export async function searchArticles(userSearchTerm) {
  // websearch_to_tsquery supports Google-like syntax: quotes, minus, OR
  const sql = `
    SELECT id, title, ts_rank(search_vector, query) AS rank
    FROM articles, websearch_to_tsquery('english', $1) query
    WHERE search_vector @@ query
    ORDER BY rank DESC
    LIMIT 20;
  `;
  const { rows } = await pool.query(sql, [userSearchTerm]);
  return rows;
}
```
Fast, ranked, stemmed, and requires zero external search infrastructure.
