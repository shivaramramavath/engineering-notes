# System Design Blueprint: Enterprise RAG Architecture & Hybrid Vector Search

## Metadata

- **Topic**: Production Retrieval-Augmented Generation (RAG), Hybrid Dense+Sparse Search, Cross-Encoder Reranking & LLM Guardrails
- **Scale Requirements**: 10 Million enterprise documents, 1,000 queries per second, sub-500ms end-to-end response (retrieval + rerank + stream)
- **Target SLA**: Grounded factual accuracy > 98% (hallucination reduction), sub-50ms vector similarity lookup, 99.95% API availability
- **Tech Stack**: FastAPI, Qdrant / PostgreSQL pgvector, OpenAI / Anthropic APIs, BGE-Reranker, Langfuse / OpenTelemetry

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Document Ingestion & Chunking**: Semantic and recursive token chunking with metadata preservation (author, department, access level).
2. **Hybrid Search (Dense + Sparse)**: Combine dense semantic vector embeddings with BM25 keyword matching via Reciprocal Rank Fusion (RRF).
3. **Cross-Encoder Reranking**: Re-score top 50 candidate chunks down to top 5 most relevant passages before passing to LLM context.
4. **Security & ACL Filtering**: Enforce role-based document access control directly inside vector database filters.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |       Enterprise User / Chatbot UI            |
                 +-----------------------+-----------------------+
                                         |
                                         | POST /api/v1/rag/chat
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI RAG Orchestrator Gateway              |
                 +-----------------------+-----------------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
         1. Dense Vector Embedding                 2. BM25 Sparse Keyword
            (text-embedding-3)                        (OpenSearch / BM25)
                       v                                   v
         +-------------+-------------+       +-------------+-------------+
         | Dense Vector Search       |       | Lexical Sparse Search     |
         | (pgvector / Qdrant HNSW)  |       | (Exact Match & Acronyms)  |
         +-------------+-------------+       +-------------+-------------+
                       |                                   |
                       +-----------------+-----------------+
                                         |
                                         | 3. Reciprocal Rank Fusion (RRF)
                                         v
                         +---------------+---------------+
                         | Top-50 Combined Candidate Pool|
                         +---------------+---------------+
                                         |
                                         | 4. Cross-Encoder Reranker
                                         |    (BGE-Reranker-Large)
                                         v
                         +---------------+---------------+
                         | Top-5 High-Precision Contexts |
                         +---------------+---------------+
                                         |
                                         | 5. Stream Grounded Response
                                         v
                         +---------------+---------------+
                         | LLM Generator (Claude / GPT-4)|
                         | Streaming SSE Output to Client|
                         +-------------------------------+
```

---

## 3. Reciprocal Rank Fusion (RRF) & Reranking Pipeline

```python
def reciprocal_rank_fusion(dense_ranks: list[str], sparse_ranks: list[str], k: int = 60) -> dict[str, float]:
    """Combines dense vector ranks and sparse keyword ranks into a unified score."""
    scores: dict[str, float] = {}

    for rank, doc_id in enumerate(dense_ranks):
        scores[doc_id] = scores.get(doc_id, 0.0) + (1.0 / (k + rank + 1))

    for rank, doc_id in enumerate(sparse_ranks):
        scores[doc_id] = scores.get(doc_id, 0.0) + (1.0 / (k + rank + 1))

    return dict(sorted(scores.items(), key=lambda item: item[1], reverse=True))
```

---

## 4. FastAPI Streaming Blueprint

```python
from fastapi import APIRouter
from fastapi.responses import StreamingResponse
from openai import AsyncOpenAI
from pydantic import BaseModel

router = APIRouter(prefix="/rag", tags=["RAG"])
openai_client = AsyncOpenAI()


class QueryRequest(BaseModel):
    query: str
    department: str


@router.post("/stream")
async def stream_rag_answer(req: QueryRequest):
    # 1. Retrieve top contexts (omitted for brevity)
    contexts = [
        "Employees are entitled to 25 days paid vacation annually.",
        "Vacation requests must be submitted 2 weeks in advance via Workday.",
    ]
    context_str = "\n".join(contexts)

    async def event_generator():
        stream = await openai_client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": f"Answer based strictly on context:\n{context_str}"},
                {"role": "user", "content": req.query},
            ],
            stream=True,
        )
        async for chunk in stream:
            content = chunk.choices[0].delta.content or ""
            if content:
                yield f"data: {content}\n\n"

    return StreamingResponse(event_generator(), media_type="text/event-stream")
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Hallucination / Context Fabrication** | LLM generates false facts not present in uploaded documents. | Set `temperature=0.0`; enforce strict system prompt guardrails; employ **Self-Consistency and Groundedness Evaluation (Ragas / G-Eval)** before output delivery. |
| **ACL Permission Bypass** | User retrieves private executive payroll documents via vector similarity. | Apply **Metadata Pre-Filtering in Vector DB**: Always enforce `must_filter={"allowed_roles": user.roles}` in HNSW search query. |
| **High Reranker Latency** | Cross-encoder adds 300ms latency to every request. | Offload reranker to dedicated GPU TensorRT instance (Triton / vLLM); rerank only top 25 candidates rather than 100. |
