# Module 05: Middleware, Streaming & Realtime

Welcome to **Module 05: Middleware, Streaming & Realtime**. This module covers the request pipeline, custom ASGI middleware, file streaming, Server-Sent Events, WebSockets, and background tasks.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-middleware-in-depth.md](./01-middleware-in-depth.md) | **Middleware in Depth** | Execution order, pure ASGI vs BaseHTTPMiddleware caveats. |
| [02-cors.md](./02-cors.md) | **CORS & Origin Policies** | CORSMiddleware configuration, preflight caching, allowlists. |
| [03-trusted-hosts-and-https-redirect.md](./03-trusted-hosts-and-https-redirect.md) | **Trusted Hosts & HTTPS** | Host header attack mitigation and HTTPS redirection. |
| [04-gzip-and-compression.md](./04-gzip-and-compression.md) | **GZip & Brotli Compression**| Compressing response payloads to reduce network egress. |
| [05-request-ids-and-correlation.md](./05-request-ids-and-correlation.md) | **Request IDs & Correlation** | Tracking distributed requests across microservices. |
| [06-custom-exception-handlers.md](./06-custom-exception-handlers.md) | **Custom Exception Handlers** | Overriding default 422 and 500 error formats. |
| [07-request-and-response-streaming.md](./07-request-and-response-streaming.md) | **Request & Response Streaming**| Streaming large files and generators without buffering. |
| [08-file-upload-and-download.md](./08-file-upload-and-download.md) | **File Upload & Download** | `UploadFile`, multipart streaming, and S3 offload. |
| [09-server-sent-events.md](./09-server-sent-events.md) | **Server-Sent Events (SSE)** | Unidirectional realtime event streaming over HTTP. |
| [10-websockets-basics.md](./10-websockets-basics.md) | **WebSockets Basics** | Bidirectional persistent WebSocket connections. |
| [11-background-tasks-and-their-limits.md](./11-background-tasks-and-their-limits.md) | **Background Tasks** | Starlette `BackgroundTasks` vs distributed Celery queues. |
| [12-rate-limiting-middleware.md](./12-rate-limiting-middleware.md) | **Rate Limiting Middleware** | Sliding window rate limiting using Redis and Token Buckets. |
| [13-forms-and-multipart.md](./13-forms-and-multipart.md) | **Forms & Multipart Data** | Form fields, file uploads, and encoding mechanisms. |
| [14-webhooks-sender-and-receiver.md](./14-webhooks-sender-and-receiver.md) | **Webhooks Sender & Receiver**| HMAC signature verification and reliable dispatch. |
