# Module 05.8: AI and Large Language Models (LLM)

Welcome to the **AI and LLM** curriculum module for NestJS backend engineering. This module covers the architectural patterns, client libraries, vector search databases, agentic frameworks, and enterprise governance practices required to build production-grade AI systems in NestJS.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-openai-integration.md](./01-openai-integration.md) | **OpenAI Integration & Streaming** | Official SDK configuration, dynamic providers, streaming chat completions with SSE, and structured outputs. |
| [02-embeddings-and-vector-search.md](./02-embeddings-and-vector-search.md) | **Embeddings and Vector Search** | Generating dense vector embeddings, `pgvector` with HNSW indexing in PostgreSQL, and cosine similarity queries. |
| [03-rag.md](./03-rag.md) | **Retrieval-Augmented Generation (RAG)** | Ingestion pipelines, recursive text chunking, Cohere re-ranking, token budgeting, and grounded generation. |
| [04-langchain.md](./04-langchain.md) | **LangChain.js and LangGraph** | Composable LCEL chains, tool calling via injected NestJS services, and stateful multi-agent workflows. |
| [05-ai-architecture.md](./05-ai-architecture.md) | **Enterprise AI Architecture & Governance** | Per-tenant token budgeting, cost tracking, prompt firewalls, multi-provider failover, and telemetry. |

---

## Learning Objectives

1. **Integrate Frontier Models**: Configure OpenAI, Anthropic, and local models via type-safe NestJS dynamic modules and service singletons.
2. **Execute High-Performance Vector Retrieval**: Design PostgreSQL schemas utilizing `pgvector` and HNSW graph indexing for sub-10ms semantic searches.
3. **Build Enterprise RAG Pipelines**: Implement end-to-end question-answering systems with recursive text splitting, cross-encoder re-ranking, and citation tracking.
4. **Orchestrate Multi-Step Agents**: Develop cyclic agent loops with LangGraph.js that invoke NestJS services as schema-validated tools.
5. **Govern AI in Production**: Enforce multi-tenant financial token budgets, circuit breaker failovers, and defensive prompt firewalls against injection attacks.

---

## Cross-References
- [04-realtime](../04-realtime/README.md) - Realtime streaming over Server-Sent Events (SSE) and WebSockets
- [02-background-processing](../02-background-processing/README.md) - Asynchronous document ingestion with BullMQ queues
- [03-events-and-messaging](../03-events-and-messaging/README.md) - Event-driven triggers for AI processing pipelines
