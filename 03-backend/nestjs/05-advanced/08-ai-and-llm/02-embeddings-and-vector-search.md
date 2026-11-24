---
title: "Embeddings and Vector Search in NestJS"
description: "Architecture, vector embeddings generation, storage with pgvector, Qdrant, Pinecone, and high-performance semantic retrieval in NestJS applications"
tags: [nestjs, ai, embeddings, pgvector, vector-search, hnsw, qdrant, pinecone, typeorm]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Embeddings and Vector Search in NestJS

## Conceptual Overview
Vector embeddings are dense numerical representations of unstructured data (text, images, audio, or code) mapped into high-dimensional vector space $\mathbb{R}^d$. Semantic similarity corresponds geometrically to proximity in this vector space: semantically related concepts cluster together, enabling nearest-neighbor searches that transcend literal keyword matching.

In modern enterprise NestJS systems, vector embeddings form the foundation of:
- **Semantic search**: Surfacing relevant knowledge base articles, documentation, or product catalogs despite vocabulary mismatches.
- **Retrieval-Augmented Generation (RAG)**: Identifying relevant context snippets from enterprise corpora to ground LLM inference.
- **Recommendation engines**: Matching user profiles or behavioral representations to candidate items.
- **De-duplication and clustering**: Detecting duplicate tickets or grouping customer feedback topics.

Integrating vector capabilities into NestJS requires clean separation of concerns: an **Embeddings Service** to interface with embedding providers (OpenAI `text-embedding-3`, Cohere, local transformers), a **Vector Store Repository** managing vector database interactions (PostgreSQL with `pgvector`, Qdrant, Pinecone, Milvus), and an orchestration pipeline handling batching, rate limiting, and dimensional consistency.

```
+-------------------------------------------------------------------------------+
|                             NestJS Architecture                               |
|                                                                               |
|  [ Ingestion Controller ] --> [ Document Splitter ]                           |
|                                       |                                       |
|                                [ Chunk Stream ]                               |
|                                       v                                       |
|                            [ Embeddings Service ]                             |
|                           (OpenAI / Cohere / ONNX)                            |
|                                       |                                       |
|                                 Dense Vectors                                 |
|                                 (e.g., 1536d)                                 |
|                                       v                                       |
|                           [ Vector Store Gateway ]                            |
|                               /             \                                 |
|                              v               v                                |
|                   [ PostgreSQL / pgvector ]   [ Dedicated Vector DB ]          |
|                   (HNSW / Cosine Distance)   (Qdrant / Pinecone / Milvus)     |
+-------------------------------------------------------------------------------+
```

---

## Core Mechanisms & Architecture

### 1. Embedding Models and Dimensions
Modern embedding models produce normalized floating-point vectors:
- **OpenAI `text-embedding-3-small`**: 1536 dimensions (supports dimension reduction to 512 via MRL - Matryoshka Representation Learning).
- **OpenAI `text-embedding-3-large`**: 3072 dimensions (reduction down to 1024 or 256).
- **Cohere `embed-v3`**: 1024 dimensions with native input type specification (`search_document` vs `search_query`).
- **Local HuggingFace / Transformers.js**: e.g., `all-MiniLM-L6-v2` (384 dimensions) running in-process via ONNX runtime without external API costs.

### 2. Distance Metrics
The geometric distance between vectors $\mathbf{u}$ and $\mathbf{v}$ determines similarity:
- **Cosine Distance ($1 - \cos(\theta)$)**:
  $$\text{dist}_{\cos}(\mathbf{u}, \mathbf{v}) = 1 - \frac{\mathbf{u} \cdot \mathbf{v}}{\|\mathbf{u}\|_2 \|\mathbf{v}\|_2}$$
  Scale-invariant metric preferred for text embeddings where vector magnitude may vary with token length. In `pgvector`, represented by the `<=>` operator.
- **Euclidean (L2) Distance**:
  $$\text{dist}_{L2}(\mathbf{u}, \mathbf{v}) = \sqrt{\sum_{i=1}^d (u_i - v_i)^2}$$
  Represented by `<->` in `pgvector`. When vectors are normalized ($\|\mathbf{u}\| = 1$), Cosine distance and squared L2 distance are monotonically equivalent.
- **Inner Product (Negative Dot Product)**:
  $$\text{dist}_{IP}(\mathbf{u}, \mathbf{v}) = - (\mathbf{u} \cdot \mathbf{v})$$
  Represented by `<#>` in `pgvector`. Most efficient computation if all vectors are pre-normalized to unit length.

### 3. Vector Indexing: IVFFlat vs HNSW
Performing brute-force $k$-Nearest Neighbors ($k$-NN) requires calculating distances across all $N$ vectors ($O(N \cdot d)$ complexity), which degrades when $N > 100{,}000$.

| Feature | IVFFlat (Inverted File Flat) | HNSW (Hierarchical Navigable Small World) |
| :--- | :--- | :--- |
| **Index Mechanism** | Partitions space into Voronoi cells via $k$-means | Multi-layer proximity graph with logarithmic skip links |
| **Build Time** | Fast, linear scaling | Slower, CPU and memory intensive |
| **Memory Footprint** | Low, stores centroids and lists | High (graph edges + vector data in RAM) |
| **Search Latency** | Moderate (proportional to `probes`) | Extremely fast ($< 5\text{ms}$ for millions of vectors) |
| **Recall / Accuracy** | Degrades if data distributions shift post-train | Consistently high recall (>98%) |
| **Post-Build Insertions** | Poor recall without index retraining | Seamlessly supports dynamic inserts without rebuild |
| **Production Recommendation**| Legacy/resource-constrained environments | **Primary default for production search** |

---

## Implementation Details & NestJS APIs

### 1. TypeORM & PostgreSQL `pgvector` Integration

Ensure the `pgvector` extension is installed in PostgreSQL (`CREATE EXTENSION IF NOT EXISTS vector;`).

```typescript
// src/ai/entities/document-embedding.entity.ts
import { Entity, PrimaryGeneratedColumn, Column, Index, CreateDateColumn } from 'typeorm';

@Entity('document_embeddings')
export class DocumentEmbeddingEntity {
  @PrimaryGeneratedColumn('uuid')
  id: string;

  @Column({ type: 'text' })
  content: string;

  @Column({ type: 'jsonb', default: {} })
  metadata: Record<string, any>;

  // 1536 dimensions for text-embedding-3-small
  @Column({ type: 'vector', length: 1536 })
  embedding: number[];

  @CreateDateColumn({ name: 'created_at' })
  createdAt: Date;
}
```

```sql
-- Migration: Add pgvector and HNSW index
CREATE EXTENSION IF NOT EXISTS vector;

CREATE TABLE IF NOT EXISTS document_embeddings (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    content TEXT NOT NULL,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    embedding vector(1536) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- HNSW index using cosine distance
CREATE INDEX IF NOT EXISTS document_embeddings_hnsw_idx 
ON document_embeddings 
USING hnsw (embedding vector_cosine_ops)
WITH (m = 16, ef_construction = 64);
```

### 2. Embeddings Provider Service

```typescript
// src/ai/services/embeddings.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import OpenAI from 'openai';

@Injectable()
export class EmbeddingsService {
  private readonly logger = new Logger(EmbeddingsService.name);
  private readonly openai: OpenAI;
  private readonly defaultModel = 'text-embedding-3-small';

  constructor(private readonly configService: ConfigService) {
    this.openai = new OpenAI({
      apiKey: this.configService.getOrThrow<string>('OPENAI_API_KEY'),
    });
  }

  async generateEmbedding(text: string): Promise<number[]> {
    const cleanedText = text.replace(/\n+/g, ' ').trim();
    const response = await this.openai.embeddings.create({
      model: this.defaultModel,
      input: cleanedText,
      encoding_format: 'float',
    });

    return response.data[0].embedding;
  }

  async generateBatchEmbeddings(texts: string[]): Promise<number[][]> {
    const cleaned = texts.map((t) => t.replace(/\n+/g, ' ').trim());
    
    // Process in batches of 100 to stay within token/payload bounds
    const batchSize = 100;
    const results: number[][] = [];

    for (let i = 0; i < cleaned.length; i += batchSize) {
      const slice = cleaned.slice(i, i + batchSize);
      const response = await this.openai.embeddings.create({
        model: this.defaultModel,
        input: slice,
        encoding_format: 'float',
      });
      
      // Sort responses by original index in the slice
      const sorted = response.data
        .sort((a, b) => a.index - b.index)
        .map((item) => item.embedding);

      results.push(...sorted);
    }

    return results;
  }
}
```

### 3. Vector Repository with Cosine Similarity

```typescript
// src/ai/repositories/vector.repository.ts
import { Injectable } from '@nestjs/common';
import { DataSource } from 'typeorm';

export interface SearchResult {
  id: string;
  content: string;
  metadata: Record<string, any>;
  similarity: number;
}

@Injectable()
export class VectorRepository {
  constructor(private readonly dataSource: DataSource) {}

  async insertEmbedding(
    content: string,
    metadata: Record<string, any>,
    vector: number[],
  ): Promise<string> {
    const vectorString = `[${vector.join(',')}]`;
    const query = `
      INSERT INTO document_embeddings (content, metadata, embedding)
      VALUES ($1, $2, $3::vector)
      RETURNING id;
    `;
    const result = await this.dataSource.query(query, [content, metadata, vectorString]);
    return result[0].id;
  }

  async searchSimilar(
    queryVector: number[],
    limit: number = 5,
    threshold: number = 0.7,
  ): Promise<SearchResult[]> {
    const vectorString = `[${queryVector.join(',')}]`;
    
    // 1 - (embedding <=> queryVector) translates cosine distance to cosine similarity [0, 1]
    const query = `
      SELECT 
        id,
        content,
        metadata,
        1 - (embedding <=> $1::vector) AS similarity
      FROM document_embeddings
      WHERE 1 - (embedding <=> $1::vector) >= $2
      ORDER BY embedding <=> $1::vector ASC
      LIMIT $3;
    `;

    const rows = await this.dataSource.query(query, [vectorString, threshold, limit]);
    return rows.map((row: any) => ({
      id: row.id,
      content: row.content,
      metadata: row.metadata,
      similarity: parseFloat(row.similarity),
    }));
  }
}
```

---

## Production Patterns & Best Practices

### 1. Hybrid Search (Dense Vectors + BM25 Full-Text Search)
Pure vector search can fail on exact identifiers (e.g., ticket numbers, SKUs, error codes). Hybrid search fuses vector similarity with PostgreSQL full-text search (`tsvector`) via Reciprocal Rank Fusion (RRF).

```typescript
// Reciprocal Rank Fusion implementation
@Injectable()
export class HybridSearchService {
  constructor(private readonly dataSource: DataSource) {}

  async searchHybrid(
    queryText: string,
    queryVector: number[],
    limit = 10,
    k = 60, // RRF constant
  ): Promise<SearchResult[]> {
    const vectorStr = `[${queryVector.join(',')}]`;

    const sql = `
      WITH vector_search AS (
        SELECT id, ROW_NUMBER() OVER (ORDER BY embedding <=> $1::vector ASC) as rank
        FROM document_embeddings
        LIMIT 50
      ),
      text_search AS (
        SELECT id, ROW_NUMBER() OVER (ORDER BY ts_rank_cd(to_tsvector('english', content), plainto_tsquery('english', $2)) DESC) as rank
        FROM document_embeddings
        WHERE to_tsvector('english', content) @@ plainto_tsquery('english', $2)
        LIMIT 50
      )
      SELECT 
        d.id, 
        d.content, 
        d.metadata,
        COALESCE(1.0 / ($3 + v.rank), 0.0) + COALESCE(1.0 / ($3 + t.rank), 0.0) AS rrf_score
      FROM document_embeddings d
      LEFT JOIN vector_search v ON d.id = v.id
      LEFT JOIN text_search t ON d.id = t.id
      WHERE v.id IS NOT NULL OR t.id IS NOT NULL
      ORDER BY rrf_score DESC
      LIMIT $4;
    `;

    return this.dataSource.query(sql, [vectorStr, queryText, k, limit]);
  }
}
```

### 2. Multi-Tenant Vector Partitioning
When building SaaS platforms, prevent cross-tenant vector leakage by either:
1. **Metadata filtering**: `WHERE metadata->>'tenantId' = $tenantId`. Must add composite indexes or partial indexes:
   ```sql
   CREATE INDEX idx_tenant_vectors ON document_embeddings USING hnsw (embedding vector_cosine_ops)
   WHERE (metadata->>'tenantId' = 'org_123');
   ```
2. **PostgreSQL Row Level Security (RLS)**: Enforce database-level session policies:
   ```sql
   ALTER TABLE document_embeddings ENABLE ROW LEVEL SECURITY;
   CREATE POLICY tenant_isolation_policy ON document_embeddings
     USING (metadata->>'tenantId' = current_setting('app.current_tenant_id'));
   ```

---

## Performance & Scale Considerations

- **HNSW Tuning**:
  - `m` (number of bi-directional links per node, default 16, range 4-64): Higher values increase recall and index size.
  - `ef_construction` (size of dynamic candidate list during build, default 64, range 32-512): Higher values produce higher quality graphs at the expense of indexing time.
  - `hnsw.ef_search` (runtime search depth, default 40): Set dynamically per transaction (`SET LOCAL hnsw.ef_search = 100;`) to balance latency vs recall on individual requests.
- **Dedicated Memory**: HNSW requires the graph index to reside in RAM for optimal latency. Ensure PostgreSQL `maintenance_work_mem` is sufficiently high during index creation, and `shared_buffers` / OS file cache accommodates vector indexes.

---

## Security & Failure Modes

- **Dimensional Mismatches**: Generating embeddings with model $A$ (1536d) and inserting them into a table configured for model $B$ (3072d) will crash transactions. Enforce model version tags in metadata.
- **SQL Vector Injection**: Never concatenate unvalidated user strings into raw vector arrays. Always serialize through strict numerical validation: `vector.map(n => Number(n))` before passing as parameterized queries.
- **Rate Limit Exhaustion**: Embedding APIs (OpenAI, Cohere) enforce TPM (Tokens Per Minute) and RPM limits. Offload ingestion to BullMQ queues with rate limiting to avoid HTTP 429 failures.

---

## Anti-Patterns & Common Pitfalls

- **Re-embedding Static Documents on Every Query**: Never recalculate embeddings dynamically for static database records. Compute once upon creation/update and store durably.
- **Indexing Before Bulk Ingestion**: Creating an HNSW index before inserting 500,000 vectors slows ingestion tenfold. Bulk insert all records first, then execute `CREATE INDEX ... USING hnsw`.
- **Ignoring Similarity Floor**: Returning top-$k$ nearest neighbors without a similarity cutoff produces hallucinated and irrelevant context when user queries fall completely outside your domain.

---

## Testing Strategies

```typescript
// src/ai/services/embeddings.service.spec.ts
import { Test, TestingModule } from '@nestjs/testing';
import { ConfigService } from '@nestjs/config';
import { EmbeddingsService } from './embeddings.service';

describe('EmbeddingsService', () => {
  let service: EmbeddingsService;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [
        EmbeddingsService,
        {
          provide: ConfigService,
          useValue: {
            getOrThrow: jest.fn().mockReturnValue('mock-openai-key'),
          },
        },
      ],
    }).compile();

    service = module.get<EmbeddingsService>(EmbeddingsService);
    
    // Mock internal OpenAI SDK
    (service as any).openai = {
      embeddings: {
        create: jest.fn().mockResolvedValue({
          data: [{ index: 0, embedding: new Array(1536).fill(0.01) }],
        }),
      },
    };
  });

  it('should generate vector embedding with exact 1536 dimensions', async () => {
    const vector = await service.generateEmbedding('NestJS architectural patterns');
    expect(vector).toHaveLength(1536);
    expect(typeof vector[0]).toBe('number');
  });
});
```

---

## Migration & Version Evolution

- **pgvector 0.5+ vs 0.7+**: Early pgvector supported only IVFFlat indexes. Version 0.5+ introduced HNSW. Version 0.7+ added half-precision floats (`halfvec`) and sparse vectors, cutting memory usage by 50%.
- **Dimension Reduction (MRL)**: With OpenAI `text-embedding-3`, you can pass `dimensions: 512` during generation. When migrating existing columns:
  ```sql
  ALTER TABLE document_embeddings ADD COLUMN embedding_512 vector(512);
  -- Repopulate via background worker before dropping legacy 1536 column
  ```

---

## Real-World Case Study: Enterprise Document Search

An enterprise documentation platform with 250,000 internal engineering documents experienced 3-second search latencies and poor relevance with traditional keyword search.
1. **Pipeline**: Documents were chunked using recursive character splitting (800 tokens, 100 token overlap).
2. **Storage**: Chunks were vectorized via `text-embedding-3-small` and persisted to PostgreSQL 16 with `pgvector` HNSW indexes (`m=16, ef_construction=64`).
3. **Retrieval**: NestJS microservice implemented hybrid search combining RRF (HNSW + `tsvector`) with a 0.68 cosine similarity floor.
4. **Results**: P95 query latency dropped to 18ms, and developer document retrieval success rate increased from 42% to 94%.

---

## Key Takeaways & Cheat Sheet

- Vector embeddings map text to dense numerical vectors where cosine proximity indicates semantic meaning.
- Prefer **HNSW** over IVFFlat for production retrieval; it supports continuous inserts and delivers sub-10ms queries.
- Combine vector search with full-text search via **Reciprocal Rank Fusion (RRF)** for optimal domain search accuracy.
- Protect database queries by using parameterized queries and strictly validated float arrays.
- Always enforce similarity thresholds (`similarity >= 0.7`) to prevent context pollution in RAG systems.

---

## Cross-References
- [01-openai-integration.md](file:///03-backend/nestjs/05-advanced/08-ai-and-llm/01-openai-integration.md) - OpenAI client integration and streaming
- [03-rag.md](file:///03-backend/nestjs/05-advanced/08-ai-and-llm/03-rag.md) - Complete Retrieval-Augmented Generation pipeline
- [02-database-foundations](../../04-intermediate/02-database-foundations/README.md) - TypeORM and PostgreSQL connection management
