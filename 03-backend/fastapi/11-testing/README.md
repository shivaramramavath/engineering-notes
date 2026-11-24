# Module 11: Testing Strategies & Quality Engineering

Welcome to **Module 11: Testing Strategies & Quality Engineering**. This module covers the full testing pyramid: fast unit tests, async HTTP client integration tests, real database testcontainers, mutation testing, and load benchmarking.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-testing-fundamentals-for-apis.md](./01-testing-fundamentals-for-apis.md) | **API Testing Fundamentals** | Testing pyramid, test scopes, and deterministic fixtures. |
| [02-testclient-vs-httpx-asyncclient.md](./02-testclient-vs-httpx-asyncclient.md) | **TestClient vs AsyncClient** | Sync Starlette TestClient vs real ASGI `httpx.AsyncClient`. |
| [03-pytest-fixtures-for-fastapi.md](./03-pytest-fixtures-for-fastapi.md) | **Pytest Fixtures** | Scoped fixtures, event loop management, and clean state resets. |
| [04-dependency-overrides.md](./04-dependency-overrides.md) | **Dependency Overrides** | Mocking authentication, storage, and external APIs in tests. |
| [05-testing-databases-with-transactions.md](./05-testing-databases-with-transactions.md)| **Database Testing & Testcontainers**| Rolling back transactions and testing against real PostgreSQL containers. |
| [06-testing-auth-and-permissions.md](./06-testing-auth-and-permissions.md) | **Testing Auth & Permissions** | Fixtures for generating mock JWTs and testing role boundaries. |
| [07-testing-websockets-and-sse.md](./07-testing-websockets-and-sse.md) | **Testing WebSockets & SSE** | Testing persistent bidirectional and event-stream connections. |
| [08-testing-background-tasks-and-queues.md](./08-testing-background-tasks-and-queues.md)| **Testing Background Tasks** | Verifying queued jobs, Celery task unit tests, and mock workers. |
| [09-mocking-external-apis-with-respx.md](./09-mocking-external-apis-with-respx.md) | **Mocking APIs with RESPX** | Mocking external HTTP requests cleanly with `respx`. |
| [10-contract-testing-with-schemathesis.md](./10-contract-testing-with-schemathesis.md)| **Contract Testing (Schemathesis)** | Automated property-based conformance testing against OpenAPI specs. |
| [11-property-based-testing-with-hypothesis.md](./11-property-based-testing-with-hypothesis.md)| **Property-Based Testing** | Generating edge case inputs automatically using Hypothesis. |
| [12-coverage-and-mutation-testing.md](./12-coverage-and-mutation-testing.md) | **Coverage & Mutation Testing** | Branch coverage (`pytest-cov`) and test robustness via `mutmut`. |
| [13-load-testing-with-locust-and-k6.md](./13-load-testing-with-locust-and-k6.md) | **Load Testing (Locust & k6)** | Simulating thousands of concurrent users to find saturation points. |
| [14-testing-lifespan-and-startup.md](./14-testing-lifespan-and-startup.md) | **Testing Lifespan Lifecycle** | Testing startup initialization failures and graceful shutdown hooks. |
| [15-flaky-tests-and-isolation.md](./15-flaky-tests-and-isolation.md) | **Flaky Tests & Isolation** | Eliminating race conditions, leaky global state, and port collisions. |
