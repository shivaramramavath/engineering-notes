# AI & Vector Search Cheat Sheet

```sql
-- pgvector Cosine Search
SELECT content, 1 - (embedding <=> $1::vector) as similarity
FROM doc_embeddings
WHERE 1 - (embedding <=> $1::vector) >= 0.7
ORDER BY embedding <=> $1::vector ASC
LIMIT 5;
```
