---
title: "Full-Text Search Architecture in NestJS"
description: "Implementing full-text search: PostgreSQL tsvector/GIN indexing, Elasticsearch, and MeiliSearch in NestJS"
tags: [nestjs, patterns, full-text-search, postgresql, elasticsearch, meilisearch, gin-index]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Full-Text Search Architecture in NestJS

## Conceptual Overview
Traditional SQL `LIKE '%query%'` queries require full table scans ($O(N)$), causing severe CPU and disk I/O bottlenecks. Enterprise text search requires inverted indexes, stemming, stop-word elimination, and relevance ranking:
- **PostgreSQL Full-Text Search**: Native `tsvector`, `tsquery`, and GIN indexing for medium workloads ($< 2\text{M}$ documents).
- **Dedicated Search Engines (Elasticsearch, MeiliSearch)**: Distributed clustering, typo tolerance, faceted filtering for large-scale enterprise corpora.

---

## PostgreSQL GIN Full-Text Search Implementation

```sql
-- Migration: Add tsvector column and GIN index
ALTER TABLE articles ADD COLUMN document_tokens tsvector;

UPDATE articles SET document_tokens = to_tsvector('english', coalesce(title, '') || ' ' || coalesce(body, ''));

CREATE INDEX idx_articles_fts ON articles USING GIN(document_tokens);
```

```typescript
// NestJS Repository Query:
const results = await this.repo
  .createQueryBuilder('article')
  .where("article.document_tokens @@ plainto_tsquery('english', :searchQuery)", { searchQuery })
  .orderBy("ts_rank(article.document_tokens, plainto_tsquery('english', :searchQuery))", 'DESC')
  .limit(20)
  .getMany();
```

---

## Key Takeaways & Cheat Sheet

- Use PostgreSQL GIN indexes on `tsvector` before introducing external search clusters.
- Migrate to MeiliSearch or Elasticsearch when typo tolerance and faceted search are required.

---

## Cross-References
- [02-embeddings-and-vector-search.md](../../05-advanced/08-ai-and-llm/02-embeddings-and-vector-search.md) - Semantic vector search
- [08-multi-tenancy.md](./08-multi-tenancy.md) - Multi-tenancy
