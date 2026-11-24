# Module 16: AI & Machine Learning Model Serving

Welcome to **Module 16: AI & Machine Learning Model Serving**. This module focuses on using FastAPI as a high-throughput, low-latency serving gateway for machine learning models, streaming LLMs, and RAG pipelines.

> Note: Machine learning model training, architecture, and mathematics belong in `05-ai/`. This module strictly covers the backend serving layer.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-serving-ml-models-with-fastapi.md](./01-serving-ml-models-with-fastapi.md) | **ML Serving Fundamentals** | Serving Scikit-learn, PyTorch, and ONNX models via REST endpoints. |
| [02-loading-models-at-startup-with-lifespan.md](./02-loading-models-at-startup-with-lifespan.md)| **Model Loading via Lifespan** | Preloading heavy weights into memory and GPU VRAM safely during startup. |
| [03-batching-and-concurrency-for-inference.md](./03-batching-and-concurrency-for-inference.md)| **Dynamic Batching** | Micro-batching concurrent requests to maximize GPU tensor core utilization. |
| [04-streaming-llm-responses-sse.md](./04-streaming-llm-responses-sse.md) | **Streaming LLMs via SSE** | Real-time token streaming using Server-Sent Events and async generators. |
| [05-openai-anthropic-and-langchain-integration.md](./05-openai-anthropic-and-langchain-integration.md)| **LLM API Integration** | Proxying and chaining external LLM SDKs (OpenAI, Anthropic) safely. |
| [06-rag-api-with-pgvector.md](./06-rag-api-with-pgvector.md) | **RAG with pgvector** | Vector similarity search in PostgreSQL using SQLAlchemy and pgvector. |
| [07-vector-databases-integration.md](./07-vector-databases-integration.md) | **Vector DB Integration** | Connecting to dedicated vector stores (Qdrant, Pinecone, Milvus, Chroma). |
| [08-background-inference-jobs.md](./08-background-inference-jobs.md) | **Asynchronous Inference** | Offloading long-running transcription and video generation to worker queues. |
| [09-guardrails-cost-and-rate-control.md](./09-guardrails-cost-and-rate-control.md)| **Guardrails & Token Budgeting** | Token counting, cost tracking, prompt injection protection, and rate limits. |
| [10-model-versioning-and-deployment.md](./10-model-versioning-and-deployment.md)| **Model Versioning & Rollout** | Shadow deployments, A/B model routing, and zero-downtime model swaps. |
