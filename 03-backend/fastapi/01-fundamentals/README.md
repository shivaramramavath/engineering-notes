# Module 01: Core Fundamentals

Welcome to **Module 01: Core Fundamentals** of the FastAPI curriculum. This module covers essential HTTP request handling, parameter extraction, validation error bodies, and async versus sync threadpool mechanics.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-path-operations-and-routing.md](./01-path-operations-and-routing.md) | **Path Operations & Routing** | HTTP verbs, decorators, operation IDs, and route tags. |
| [02-path-and-query-parameters.md](./02-path-and-query-parameters.md) | **Path & Query Parameters** | Parameter types, defaults, validation with `Path()` and `Query()`. |
| [03-request-body-and-pydantic-models.md](./03-request-body-and-pydantic-models.md) | **Request Bodies & Pydantic** | JSON deserialization, schemas, and nested models. |
| [04-headers-cookies-and-forms.md](./04-headers-cookies-and-forms.md) | **Headers, Cookies & Forms** | Extracting client headers, cookies, and URL-encoded forms. |
| [05-response-models-and-status-codes.md](./05-response-models-and-status-codes.md) | **Response Models & Status Codes** | `response_model`, response filtering, and HTTP status codes. |
| [06-error-handling-and-http-exception.md](./06-error-handling-and-http-exception.md) | **Error Handling & HTTPException** | Custom exception raises, error headers, and handling logic. |
| [07-validation-errors-and-the-422-body.md](./07-validation-errors-and-the-422-body.md) | **Validation Errors & 422 Payloads** | Parsing and formatting Pydantic validation error lists. |
| [08-async-vs-sync-endpoints.md](./08-async-vs-sync-endpoints.md) | **Async vs Sync Endpoints** | Threadpool offloading, event loop blocking pitfalls, and AnyIO. |
| [09-type-hints-that-fastapi-reads.md](./09-type-hints-that-fastapi-reads.md) | **Type Hints & Annotated** | Modern `Annotated` pattern for metadata declaration. |
| [10-openapi-generation-internals.md](./10-openapi-generation-internals.md) | **OpenAPI Generation Internals** | How FastAPI compiles type annotations into OpenAPI 3.1 JSON. |
| [11-request-lifecycle-end-to-end.md](./11-request-lifecycle-end-to-end.md) | **Request Lifecycle End-to-End** | Tracing a request from raw TCP socket to HTTP response. |
| [12-parameter-models-and-grouped-params.md](./12-parameter-models-and-grouped-params.md) | **Parameter Models & Groups** | Grouping query parameters into reusable Pydantic models. |
