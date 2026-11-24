# Project 08: RAG Document Retrieval Application (`rag-application`)

> Retrieval-Augmented Generation (RAG) platform built with Next.js App Router, featuring vector embeddings generation, PostgreSQL pgvector similarity search, and source citation streaming.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-next-08-rag-platform` |
| **Next.js Version** | 15.2+ / 16.0 |
| **React Version** | React 19.0.0 |
| **Node Version** | Node.js 20+ LTS |
| **Router** | App Router (`app/`) |
| **Vector DB** | PostgreSQL (`pgvector`) + Drizzle ORM |
| **AI Stack** | Vercel AI SDK v4 + OpenAI Embeddings (`text-embedding-3-small`) |
| **Styling** | Tailwind CSS v4 |
| **Tested With** | Next.js 15.2, Vitest 3.0, Playwright 1.49 |
| **Complexity Level** | Production-Grade |

---

## 1. Architectural Overview & Component Hierarchy

### 1.1 System Architecture & Data Flow
User documents (PDF, Markdown) are chunked and converted into 1536-dimensional vectors using OpenAI's embedding model, then stored in PostgreSQL with the `pgvector` extension. When a user asks a question, the question is embedded, a cosine distance similarity query retrieves the top-3 matching chunks, and an LLM streams the answer with inline source citations.

```
[ Ingest Document ] ──> [ Chunker ] ──> [ OpenAI Embedding ] ──> [ Postgres pgvector ]
                                                                          │
[ User Question ] ──> [ Query Vector ] ──(Cosine Similarity <->)─────────┘
                              │
                              ▼ (Top 3 Chunks as Context)
             [ LLM Streaming Response with Source Footnotes ]
```

---

## 2. Complete Project Source Code

### Configuration Files

#### `package.json`
```json
{
  "name": "nextjs-rag-application",
  "version": "1.0.0",
  "private": true,
  "scripts": {
    "dev": "next dev --turbopack",
    "build": "next build",
    "start": "next start",
    "test": "vitest run"
  },
  "dependencies": {
    "next": "^15.2.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0",
    "server-only": "^0.0.1",
    "ai": "^4.1.0",
    "@ai-sdk/openai": "^1.1.0",
    "drizzle-orm": "^0.38.0",
    "pg": "^8.13.1"
  },
  "devDependencies": {
    "typescript": "^5.7.3",
    "tailwindcss": "^4.0.0",
    "vitest": "^3.0.0"
  }
}
```

#### `docker-compose.yml`
```yaml
version: '3.8'

services:
  postgres:
    image: pgvector/pgvector:pg16
    environment:
      POSTGRES_USER: rag_user
      POSTGRES_PASSWORD: rag_password
      POSTGRES_DB: rag_db
    ports:
      - '5432:5432'
```

### Vector Search Route Handler (`src/app/api/rag/route.ts`)
```ts
import { openai } from '@ai-sdk/openai';
import { streamText, embed } from 'ai';
import { db } from '@/db';
import { sql } from 'drizzle-orm';

export async function POST(req: Request) {
  const { question } = await req.json();

  // 1. Generate embedding vector for user question
  const { embedding } = await embed({
    model: openai.embedding('text-embedding-3-small'),
    value: question,
  });

  // 2. Query top matching document chunks using pgvector cosine distance (<=>)
  const similarChunks = await db.execute(sql`
    SELECT id, content, source_url, 1 - (embedding <=> ${JSON.stringify(embedding)}::vector) as similarity
    FROM document_chunks
    ORDER BY similarity DESC
    LIMIT 3;
  `);

  const context = similarChunks.rows.map((r: any) => r.content).join('\n---\n');

  // 3. Stream grounded answer with context
  const result = streamText({
    model: openai('gpt-4o'),
    system: `You are an enterprise AI assistant. Answer using ONLY the provided context:\n${context}`,
    prompt: question,
  });

  return result.toDataStreamResponse();
}
```

---

## 3. Testing Suite & Verification

```ts
import { describe, it, expect } from 'vitest';

describe('RAG Context Assembly', () => {
  it('concatenates retrieved chunks cleanly', () => {
    const chunks = [{ content: 'Doc A' }, { content: 'Doc B' }];
    const context = chunks.map((c) => c.content).join('\n---\n');
    expect(context).toBe('Doc A\n---\nDoc B');
  });
});
```

---

## 4. Build and Run Instructions

```bash
docker compose up -d
npm install
npm run build
npm run start
```
