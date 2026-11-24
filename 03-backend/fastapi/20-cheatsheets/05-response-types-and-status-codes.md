# Quick Reference: Response Types & Semantic HTTP Status Codes

## Response Classes Matrix

```python
from fastapi import FastAPI, status
from fastapi.responses import (
    FileResponse,
    HTMLResponse,
    JSONResponse,
    ORJSONResponse,
    PlainTextResponse,
    RedirectResponse,
    StreamingResponse,
)

app = FastAPI(default_response_class=ORJSONResponse)

# 1. High-Performance ORJSON
@app.get("/fast-json", response_class=ORJSONResponse)
def get_fast():
    return {"data": [1, 2, 3]}

# 2. File Download
@app.get("/report.pdf", response_class=FileResponse)
def download_pdf():
    return FileResponse("exports/report.pdf", filename="monthly_report.pdf")

# 3. HTTP 307 Temporary Redirect
@app.get("/legacy-url", response_class=RedirectResponse)
def redirect_to_v2():
    return RedirectResponse(url="/api/v2/items", status_code=status.HTTP_307_TEMPORARY_REDIRECT)

# 4. Server-Sent Events (SSE) Stream
@app.get("/events", response_class=StreamingResponse)
async def event_stream():
    async def sse_gen():
        while True:
            yield "data: ping\n\n"
            await asyncio.sleep(1)
    return StreamingResponse(sse_gen(), media_type="text/event-stream")
```

---

## Semantic HTTP Status Codes Guide

| Status Code | Enum Name | Semantic Meaning & Idiomatic Usage |
|:---|:---|:---|
| **200** | `status.HTTP_200_OK` | Standard successful response for GET, PUT, PATCH |
| **201** | `status.HTTP_201_CREATED` | New resource successfully created (POST) |
| **202** | `status.HTTP_202_ACCEPTED` | Asynchronous task queued; processing not complete |
| **204** | `status.HTTP_204_NO_CONTENT` | Success, no body returned (DELETE, empty PUT) |
| **301** | `status.HTTP_301_MOVED_PERMANENTLY`| Permanent URL redirect; cached indefinitely by browser |
| **307** | `status.HTTP_307_TEMPORARY_REDIRECT`| Temporary redirect; preserves HTTP method (e.g. POST) |
| **400** | `status.HTTP_400_BAD_REQUEST` | Malformed request, syntax error, domain validation fail |
| **401** | `status.HTTP_401_UNAUTHORIZED` | Missing or invalid authentication token |
| **403** | `status.HTTP_403_FORBIDDEN` | Authenticated, but lacks sufficient permissions (RBAC) |
| **404** | `status.HTTP_404_NOT_FOUND` | Resource does not exist |
| **409** | `status.HTTP_409_CONFLICT` | Resource state conflict (duplicate key, lock timeout) |
| **422** | `status.HTTP_422_UNPROCESSABLE_ENTITY`| Pydantic schema validation failure |
| **429** | `status.HTTP_429_TOO_MANY_REQUESTS`| Rate limit exceeded |
| **500** | `status.HTTP_500_INTERNAL_SERVER_ERROR`| Unhandled backend exception |
| **503** | `status.HTTP_503_SERVICE_UNAVAILABLE`| Circuit breaker open, maintenance, overload |
