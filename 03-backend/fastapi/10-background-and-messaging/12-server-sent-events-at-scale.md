---
title: "Server-Sent Events (SSE) at Scale in FastAPI"
category: "10-background-and-messaging"
order: 12
tags: ["fastapi", "sse", "server-sent-events", "streaming", "redis-pubsub", "http2"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Server-Sent Events (SSE) at Scale in FastAPI

## 1. Conceptual Foundation & Mental Model

While WebSockets provide full-duplex, bidirectional communication over a dedicated TCP upgrade, many modern real-time requirements are **strictly unidirectional** from server to client:
- Streaming LLM token responses (OpenAI / ChatGPT style)
- Live financial market tickers and stock quotes
- Build server deployment logs and progress bars
- System status notifications and dashboard widgets

For these use cases, **Server-Sent Events (SSE)** is significantly superior to WebSockets. SSE runs over standard HTTP (`GET` request with `Content-Type: text/event-stream`). It is natively supported by browsers via the standard `EventSource` JavaScript API, automatically handles connection reconnection, supports message IDs for stream resumption (`Last-Event-ID`), and traverses corporate proxies, firewalls, and load balancers without specialized WebSocket proxy configurations.

### The Mental Model: The Ticker Tape Telegraph vs The Walkie-Talkie
- **WebSockets** is a **Two-Way Walkie-Talkie**. Both you and the server can speak at any moment. But it requires special operating licenses (WebSocket handshake protocol), different batteries (TCP state management), and careful etiquette.
- **Server-Sent Events (SSE)** is a **Wall Street Ticker Tape Machine**. It is a one-way telegraph wire printing paper tape. The server prints prices on the tape; the client watches the tape unravel. If the wire is accidentally cut, the ticker tape machine automatically reconnects, looks at the serial number of the last printed letter (**`Last-Event-ID`**), and resumes printing seamlessly without losing a single character.

---

## 2. Architecture & Wire Protocol

```
   Browser / Client (EventSource)                FastAPI Server (StreamingResponse)
          |                                                       |
          | 1. HTTP GET /events/stream                            |
          |    Accept: text/event-stream                          |
          |    Last-Event-ID: 42 (optional)                       |
          +------------------------------------------------------>|
          |                                                       |
          | 2. HTTP 200 OK                                        |
          |    Content-Type: text/event-stream                    |
          |    Cache-Control: no-cache                            |
          |    Connection: keep-alive                             |
          |<------------------------------------------------------+
          |                                                       |
          | 3. Data Chunk: "id: 43\nevent: ticker\ndata: 104.5\n\n|
          |<------------------------------------------------------+
          |                                                       |
          | 4. Data Chunk: "id: 44\nevent: ticker\ndata: 104.8\n\n|
          |<------------------------------------------------------+
          |                                                       |
          | [ Network Dropout Occurs! ]                           |
          | Browser automatically waits 3 seconds and reconnects: |
          | GET /events/stream                                    |
          | Last-Event-ID: 44                                     |
          +------------------------------------------------------>|
```

### The W3C SSE Text Wire Format
Every event chunk transmitted over the HTTP connection consists of plain UTF-8 text lines separated by newlines (`\n`) and terminated by an empty line (`\n\n`):
```text
id: 101
event: update
retry: 5000
data: {"stock": "NVDA", "price": 128.50}

```
- `id:`: Sets the event ID; stored by the browser and sent back in the `Last-Event-ID` header on reconnection.
- `event:`: Custom event name (dispatches to `eventSource.addEventListener("update", ...)`).
- `data:`: The actual payload. Multiple `data:` lines are concatenated with newlines.
- `retry:`: Reconnection delay in milliseconds if the connection drops.

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Basic Async Generator Streaming in FastAPI
```python
import asyncio
from typing import AsyncGenerator
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def event_publisher() -> AsyncGenerator[str, None]:
    event_id = 0
    try:
        while True:
            await asyncio.sleep(1.0)
            event_id += 1
            # Wire format: "id: ...\nevent: ...\ndata: ...\n\n"
            yield f"id: {event_id}\nevent: tick\ndata: {{\"tick\": {event_id}}}\n\n"
    except asyncio.CancelledError:
        # Triggered when client closes browser or disconnects
        print("Client disconnected from SSE stream.")

@app.get("/events")
async def sse_endpoint():
    return StreamingResponse(
        event_publisher(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no"  # CRITICAL: Disables Nginx reverse proxy buffering!
        }
    )
```

---

## 4. Practical Implementation

### Example 1: Basic AI LLM Token Streamer (OpenAI Style)
```python
import asyncio
import json
from typing import AsyncGenerator
from fastapi import FastAPI
from fastapi.responses import StreamingResponse
from pydantic import BaseModel

app = FastAPI()

class PromptRequest(BaseModel):
    prompt: str

async def mock_llm_token_stream(prompt: str) -> AsyncGenerator[str, None]:
    words = f"This is an automated streaming response for: {prompt}".split()
    for word in words:
        await asyncio.sleep(0.08)  # Simulate token generation latency
        payload = json.dumps({"token": word + " "})
        yield f"event: delta\ndata: {payload}\n\n"
    # End of stream marker
    yield "event: done\ndata: [DONE]\n\n"

@app.post("/chat/completions/stream")
async def stream_chat(req: PromptRequest):
    return StreamingResponse(
        mock_llm_token_stream(req.prompt),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```

### Example 2: Production Clustered SSE via Redis Pub/Sub
Scaling SSE across multiple Kubernetes pods so that events published to Redis are broadcast to thousands of streaming clients:
```python
import asyncio
from typing import AsyncGenerator
import redis.asyncio as redis
from fastapi import FastAPI, Request
from fastapi.responses import StreamingResponse

app = FastAPI()
redis_pool = redis.ConnectionPool(host="localhost", port=6379, decode_responses=True)

async def redis_sse_generator(channel_name: str) -> AsyncGenerator[str, None]:
    r = redis.Redis(connection_pool=redis_pool)
    pubsub = r.pubsub()
    await pubsub.subscribe(channel_name)
    
    try:
        # Initial keepalive heartbeat
        yield ": ping\n\n"
        
        while True:
            # Poll with timeout to allow periodic ping heartbeat injection
            msg = await pubsub.get_message(ignore_subscribe_messages=True, timeout=15.0)
            if msg:
                data = msg["data"]
                yield f"event: update\ndata: {data}\n\n"
            else:
                # 15s timeout reached: send comment heartbeat to prevent proxy timeout
                yield ": keepalive-heartbeat\n\n"
    except asyncio.CancelledError:
        print(f"SSE Client disconnected from channel {channel_name}")
    finally:
        await pubsub.unsubscribe(channel_name)
        await pubsub.close()
        await r.close()

@app.get("/sse/stocks/{symbol}")
async def stock_ticker_sse(symbol: str):
    return StreamingResponse(
        redis_sse_generator(f"stocks:{symbol}"),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no"
        }
    )
```

### Example 3: Edge Case — Resuming Streams with `Last-Event-ID` via Redis Streams
If a client disconnects, the browser sends the `Last-Event-ID` header upon reconnect. We can replay all missed events from a Redis Stream!
```python
import redis.asyncio as redis
from fastapi import FastAPI, Header
from fastapi.responses import StreamingResponse

app = FastAPI()
r = redis.Redis(host="localhost", port=6379, decode_responses=True)

async def replayable_sse_stream(stream_key: str, last_event_id: str | None):
    # If client provides Last-Event-ID, start reading from that exact ID
    # Otherwise, read only newly arriving events ('$')
    start_id = last_event_id if last_event_id else "$"
    
    try:
        while True:
            # Read messages newer than start_id with 2-second block
            res = await r.xread({stream_key: start_id}, count=20, block=2000)
            if res:
                for stream_name, messages in res:
                    for msg_id, fields in messages:
                        start_id = msg_id  # Advance cursor
                        yield f"id: {msg_id}\nevent: audit\ndata: {fields['data']}\n\n"
            else:
                # Heartbeat to keep connection open
                yield ": ping\n\n"
    except asyncio.CancelledError:
        pass

@app.get("/sse/audit-logs")
async def audit_logs_sse(last_event_id: str | None = Header(default=None)):
    return StreamingResponse(
        replayable_sse_stream("audit_event_stream", last_event_id),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```

### Example 4: Failure Case — Proxy Buffering Deadlock (`X-Accel-Buffering`)
```python
# CATASTROPHIC ANTI-PATTERN
@app.get("/bad-sse")
async def bad_sse():
    # If Nginx or Cloudflare reverse proxies this request, Nginx will buffer
    # all chunks in its 64KB proxy buffer!
    # The client receives NOTHING until the buffer fills up or connection terminates!
    # Missing: "X-Accel-Buffering": "no"
    return StreamingResponse(generate_events(), media_type="text/event-stream")
```

### Example 5: Idiomatic Modern — SSE Comment Heartbeats
Lines starting with a colon (`:`) are treated by the W3C specification as comments and ignored by JavaScript clients. They are ideal for zero-cost proxy keepalives:
```python
async def sse_with_comment_heartbeats():
    while True:
        # Transmit zero-overhead keepalive comment every 15s
        await asyncio.sleep(15.0)
        yield ": ths_is_a_proxy_keepalive_comment\n\n"
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **HTTP/1.1 Maximum 6 Connection Browser Limit**: Under HTTP/1.1, web browsers (Chrome, Firefox) enforce a hard limit of **6 concurrent open HTTP connections per domain**. If a user opens 6 tabs with SSE streams, opening a 7th tab hangs the entire browser! **Remedy**: Always serve SSE over **HTTP/2 or HTTP/3**, which multiplexes hundreds of streams over a single TCP connection.
2. **Reverse Proxy Buffering (Nginx / Cloudflare)**: Nginx buffers HTTP response bodies by default. An SSE stream will appear frozen until the server closes the connection. Always include the header `"X-Accel-Buffering": "no"`, and in Nginx configure `proxy_buffering off;`.
3. **Dead Socket Accumulation on Client Drop**: When a client closes a browser tab, Uvicorn detects the broken socket and cancels the underlying async generator. If your generator does not yield control or catches `Exception` incorrectly without yielding, the generator task hangs in memory.
4. **Compression Middleware Conflicts**: Using Gzip or Brotli middleware on SSE streams forces the compression engine to buffer chunks, destroying real-time delivery. Never enable Gzip on `text/event-stream` routes.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Missing `X-Accel-Buffering: no` | Nginx default proxy buffering | Events delayed until buffer fills (appears broken) | Always include `"X-Accel-Buffering": "no"` header |
| Serving SSE over HTTP/1.1 to multi-tab users | Browser 6-connection per host limit | 7th browser tab hangs completely | Serve over HTTP/2 or HTTP/3 |
| Enabling Gzip middleware on SSE routes | Gzip buffers chunks before compressing | Real-time streaming becomes batch delayed | Exclude `text/event-stream` from Gzip middleware |
| Missing comment heartbeats (`: ping\n\n`) | Cloud load balancers drop idle streams | Connections cut every 60 seconds | Emit `: keepalive\n\n` comment every 15 seconds |
| Formatted lines without trailing `\n\n` | Violating W3C SSE specification | Client browser parser never fires `onmessage` | Terminate every event block with double newline `\n\n` |
| Using WebSockets for simple unidirectional streaming | Over-engineering protocol complexity | Firewall blocking, manual reconnect logic | Use Server-Sent Events (SSE) |

---

## 7. Performance & Scalability Considerations

- **Resource Consumption**: Unlike WebSockets, SSE uses standard HTTP streaming. In Python / AnyIO, an idle SSE connection consumes ~15KB to 25KB of RAM.
- **HTTP/2 Multiplexing**: With HTTP/2, 100 browser tabs or components can stream from separate SSE endpoints simultaneously over a single TLS connection.

---

## 8. Security & OWASP Guardrails

- **Cross-Origin Resource Sharing (CORS)**: If SSE is served from a subdomain (e.g. `stream.api.com`), configure `Access-Control-Allow-Origin` and `Access-Control-Allow-Credentials` headers to authorize cross-origin `EventSource` requests.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

test_app = FastAPI()

async def dummy_sse_stream():
    yield "event: test\ndata: payload_1\n\n"
    yield "event: test\ndata: payload_2\n\n"

@test_app.get("/stream")
async def stream_route():
    return StreamingResponse(dummy_sse_stream(), media_type="text/event-stream")

@pytest.mark.anyio
async def test_sse_endpoint_streaming():
    transport = ASGITransport(app=test_app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        # Use aiter_text() or aiter_lines() to test stream chunks
        async with client.stream("GET", "/stream") as response:
            assert response.status_code == 200
            assert response.headers["content-type"].startswith("text/event-stream")
            
            lines = [line async for line in response.aiter_lines() if line]
            assert "event: test" in lines
            assert "data: payload_1" in lines
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A generative AI startup deployed their LLM streaming API on AWS behind an Application Load Balancer and Nginx reverse proxy. Customers complained that streaming did not work: instead of words appearing sequentially, the browser hung for 12 seconds, and then the entire paragraph appeared all at once.

**Root Cause**:
1. Nginx had default `proxy_buffering on`, which buffered the first 16KB of LLM tokens before flushing to the client.
2. The FastAPI endpoint omitted the `X-Accel-Buffering` header.

**The Fix**:
1. Added `"X-Accel-Buffering": "no"` to the `StreamingResponse` headers in FastAPI.
2. Configured Nginx:
   ```nginx
   location /chat/stream {
       proxy_buffering off;
       proxy_cache off;
       proxy_set_header Connection '';
       proxy_http_version 1.1;
       chunked_transfer_encoding off;
   }
   ```
3. Tokens immediately streamed to users at a responsive 50ms per token.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What are the key differences between Server-Sent Events (SSE) and WebSockets?</summary>
<b>SSE</b> is a unidirectional (server-to-client) streaming protocol running over standard HTTP (<code>text/event-stream</code>). It natively supports automatic reconnection, event IDs, and proxy traversal. <b>WebSockets</b> is a bidirectional, full-duplex protocol running over a custom TCP upgrade. SSE is simpler, firewall-friendly, and better suited for one-way streams (LLMs, stock tickers, notifications).
</details>

<details>
<summary>2. Why does HTTP/1.1 cause problems for SSE when a user opens multiple browser tabs?</summary>
Browsers enforce a strict limit of <b>6 concurrent HTTP/1.1 connections per domain</b>. Because an SSE stream remains open permanently, opening 6 browser tabs consumes all 6 available connections. Any subsequent request to that domain hangs indefinitely. Serving SSE over HTTP/2 solves this by multiplexing streams over a single connection.
</details>

<details>
<summary>3. What is the purpose of the <code>X-Accel-Buffering: no</code> HTTP header in SSE?</summary>
Nginx reverse proxies automatically buffer HTTP response bodies in 4KB–64KB buffers before sending them to the client. The <code>X-Accel-Buffering: no</code> header instructs Nginx to immediately disable response buffering for that stream, ensuring events are flushed to the client in real-time.
</details>

<details>
<summary>4. How does SSE implement automatic stream resumption on reconnection?</summary>
When the server emits an event with an <code>id: <id></code> field, the browser records this identifier. If the network drops and reconnects, the browser's <code>EventSource</code> automatically sends this identifier in the <code>Last-Event-ID</code> HTTP request header. The server inspects this header and replays any missed events.
</details>

<details>
<summary>5. How do you implement keepalive heartbeats in an SSE stream without triggering client-side message events?</summary>
By emitting lines that start with a colon (e.g. <code>: keepalive\n\n</code>). The W3C SSE standard specifies that lines starting with a colon are comments. Browsers ignore comments completely, but the data transfer keeps firewalls and load balancers from closing the idle TCP connection.
</details>

<details>
<summary>6. What happens if you apply Gzip compression middleware to an SSE route?</summary>
Gzip compression algorithms buffer data blocks before computing compression dictionaries. This prevents immediate flushing of small text events, destroying real-time interactivity. Compression middleware must explicitly exclude <code>text/event-stream</code>.
</details>

<details>
<summary>7. How does a FastAPI application know when an SSE client has disconnected?</summary>
When the client closes the TCP connection, the underlying ASGI server (Uvicorn) detects the closed socket and injects an <code>asyncio.CancelledError</code> into the async generator coroutine driving the <code>StreamingResponse</code>, triggering its <code>finally</code> block.
</details>

<details>
<summary>8. Can you send binary data over Server-Sent Events?</summary>
No. The W3C SSE specification strictly mandates UTF-8 encoded text. To transmit binary data (e.g. images or audio chunks), the payload must be Base64-encoded or converted to JSON before transmission.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic SSE Event Formatter
**Task**: Write a Python helper function `format_sse(data: str, event: str = None, id: str = None) -> str` that generates a valid W3C-compliant SSE chunk.
<details>
<summary>Solution</summary>

```python
def format_sse(data: str, event: str | None = None, event_id: str | None = None) -> str:
    lines = []
    if event_id is not None:
        lines.append(f"id: {event_id}")
    if event is not None:
        lines.append(f"event: {event}")
    # Handle multi-line data
    for data_line in data.splitlines():
        lines.append(f"data: {data_line}")
    lines.append("\n")  # Trailing newline for completion
    return "\n".join(lines)
```
</details>

### Exercise 2 (Easy): Realtime Counter Streamer
**Task**: Build a route `GET /sse/count` that emits a counter from 1 to 5 with a 0.5s pause between numbers, followed by an `event: close`.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def counter_generator():
    for i in range(1, 6):
        await asyncio.sleep(0.5)
        yield f"id: {i}\nevent: count\ndata: {i}\n\n"
    yield "event: close\ndata: [COMPLETE]\n\n"

@app.get("/sse/count")
async def count_stream():
    return StreamingResponse(
        counter_generator(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```
</details>

### Exercise 3 (Medium): Comment Keepalive Heartbeat Stream
**Task**: Implement an async generator that emits a data event every 10 seconds, but emits a comment ping `: ping\n\n` every 2 seconds in between to prevent proxy timeouts.
<details>
<summary>Solution</summary>

```python
import asyncio
from typing import AsyncGenerator

async def heartbeat_stream() -> AsyncGenerator[str, None]:
    counter = 0
    while True:
        # 5 cycles of 2s sleeps = 10s data interval
        for _ in range(5):
            await asyncio.sleep(2.0)
            yield ": ping\n\n"  # Comment heartbeat
            
        counter += 1
        yield f"event: update\ndata: Event #{counter}\n\n"
```
</details>

### Exercise 4 (Medium): Resumable Stream with `Last-Event-ID`
**Task**: Build a route that reads `Last-Event-ID` from request headers. If provided, resume emission from that integer offset; otherwise, start at 0.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, Header
from fastapi.responses import StreamingResponse

app = FastAPI()

async def resumable_sequence(start_from: int):
    current = start_from
    while True:
        await asyncio.sleep(1.0)
        current += 1
        yield f"id: {current}\nevent: number\ndata: {current}\n\n"

@app.get("/sse/sequence")
async def sequence_endpoint(last_event_id: str | None = Header(default=None)):
    start_offset = int(last_event_id) if last_event_id and last_event_id.isdigit() else 0
    return StreamingResponse(
        resumable_sequence(start_offset),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```
</details>

### Exercise 5 (Hard): Distributed Broadcast Fan-Out via Redis Pub/Sub
**Task**: Create an SSE endpoint `/sse/alerts` that subscribes to a Redis Pub/Sub channel `system:alerts` and streams published messages to connected browsers.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI
from fastapi.responses import StreamingResponse
import redis.asyncio as redis

app = FastAPI()
redis_pool = redis.ConnectionPool(host="localhost", port=6379, decode_responses=True)

async def redis_alert_stream():
    r = redis.Redis(connection_pool=redis_pool)
    pubsub = r.pubsub()
    await pubsub.subscribe("system:alerts")
    try:
        yield ": connected\n\n"
        while True:
            msg = await pubsub.get_message(ignore_subscribe_messages=True, timeout=10.0)
            if msg:
                yield f"event: alert\ndata: {msg['data']}\n\n"
            else:
                yield ": keepalive\n\n"
    except asyncio.CancelledError:
        pass
    finally:
        await pubsub.unsubscribe("system:alerts")
        await pubsub.close()
        await r.close()

@app.get("/sse/alerts")
async def alerts_sse():
    return StreamingResponse(
        redis_alert_stream(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```
</details>

### Exercise 6 (Hard): LLM Streaming Backpressure Controller
**Task**: Implement an SSE token streamer that accepts an external token queue. If the client stops reading (backpressure detected), pause token production until socket drainage occurs.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def backpressure_aware_generator():
    token_queue: asyncio.Queue[str] = asyncio.Queue(maxsize=10)
    
    # Background producer simulating AI engine
    async def ai_producer():
        try:
            for i in range(1, 50):
                # Pauses if queue has 10 tokens (client is slow)
                await token_queue.put(f"Token_{i}")
                await asyncio.sleep(0.05)
            await token_queue.put("[DONE]")
        except asyncio.CancelledError:
            print("AI generation cancelled by client disconnect.")

    producer_task = asyncio.create_task(ai_producer())
    
    try:
        while True:
            token = await token_queue.get()
            if token == "[DONE]":
                yield "event: done\ndata: [DONE]\n\n"
                break
            yield f"event: token\ndata: {token}\n\n"
            token_queue.task_done()
    finally:
        producer_task.cancel()

@app.get("/sse/llm-stream")
async def stream_llm():
    return StreamingResponse(
        backpressure_aware_generator(),
        media_type="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"}
    )
```
</details>
