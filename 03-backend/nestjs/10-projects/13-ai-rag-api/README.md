# Project 13: AI Knowledge Base & RAG Copilot (`13-ai-rag-api`)

> Enterprise Retrieval-Augmented Generation (RAG) assistant: Document ingestion pipeline, OpenAI `text-embedding-3`, PostgreSQL `pgvector` HNSW index, Cohere ReRank, and SSE response streaming.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-13-ai-rag-api` |
| **Vector DB** | PostgreSQL 16 + pgvector (HNSW Indexing) |
| **LLM & Embeddings** | OpenAI GPT-4o / `text-embedding-3-small` |
| **Re-Ranking** | Cohere ReRank v3 |
| **Streaming** | Server-Sent Events (SSE) |
| **Complexity Level** | Cutting-Edge AI Reference |

---

## 1. RAG Ingestion & Vector Retrieval Pipeline

```typescript
// src/ai/rag.service.ts
import { Injectable } from '@nestjs/common';
import OpenAI from 'openai';
import { DataSource } from 'typeorm';

@Injectable()
export class RagService {
  private readonly openai = new OpenAI();

  constructor(private readonly dataSource: DataSource) {}

  async searchAndAnswer(query: string) {
    // 1. Vectorize query
    const res = await this.openai.embeddings.create({
      model: 'text-embedding-3-small',
      input: query,
    });
    const vector = res.data[0].embedding;

    // 2. Query HNSW cosine index in pgvector
    const chunks = await this.dataSource.query(
      `SELECT content, 1 - (embedding <=> $1::vector) as similarity 
       FROM doc_chunks 
       ORDER BY embedding <=> $1::vector ASC 
       LIMIT 4`,
      [`[${vector.join(',')}]`],
    );

    // 3. Grounded completion
    const context = chunks.map((c: any) => c.content).join('\n---\n');
    return this.openai.chat.completions.create({
      model: 'gpt-4o-mini',
      messages: [
        { role: 'system', content: `Answer using context:\n${context}` },
        { role: 'user', content: query },
      ],
    });
  }
}
```
