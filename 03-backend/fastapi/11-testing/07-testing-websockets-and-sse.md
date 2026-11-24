---
title: "Testing WebSockets and Server-Sent Events (SSE) in FastAPI"
category: "11-testing"
order: 7
tags: ["fastapi", "testing", "websockets", "sse", "streaming", "testclient", "httpx"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Testing WebSockets and Server-Sent Events (SSE) in FastAPI

## 1. Conceptual Foundation & Mental Model

Testing persistent, bidirectional, or streaming network connections—such as **WebSockets** and **Server-Sent Events (SSE)**—requires techniques fundamentally different from standard Request/Response HTTP testing.

In standard HTTP testing, a client sends a single request and awaits a single complete response body (`resp = await client.get("/url")`).
In real-time streaming testing:
1. **WebSockets**: The connection is a persistent full-duplex channel. Tests must send messages, await responses asynchronously, handle binary frames, test disconnect exceptions (`WebSocketDisconnect`), and verify protocol close codes.
2. **Server-Sent Events (SSE)**: The HTTP response never "finishes" during active streaming. Tests must open a non-blocking stream context (`async with client.stream(...)`), process individual incoming event chunks iteratively (`aiter_lines()`), and test abrupt client-side cancellations.

### The Mental Model: Testing a Telegram vs Testing a Live Phone Call
- Testing a standard **REST endpoint** is testing a **Telegram**. You send a typed message in an envelope; the post office sends back a signed paper slip. You read the whole slip at once.
- Testing a **WebSocket** is testing a **Live Telephone Call**. You dial the number, wait for the pick-up tone (`accept()`), speak a sentence into the receiver, listen for the reply, interrupt with another sentence, and finally hang up the receiver (`close()`). You must verify what happens if the wire is unplugged midway through the conversation.
- Testing an **SSE endpoint** is testing a **Stock Ticker Telegraph Tape**. You turn on the machine, watch paper tape print line by line, inspect each printed symbol as it rolls off the wheel, and cut the paper tape when satisfied.

---

## 2. Architecture & Wire Testing Mechanics

```
   Testing WebSockets (Starlette TestClient)    Testing SSE Streams (httpx.AsyncClient)
   +------------------------------------+      +------------------------------------+
   | with client.websocket_connect(url):|      | async with client.stream("GET",...)|
   |     ws.send_text("Hello")          |      |     async for line in aiter_lines: |
   |     data = ws.receive_text()       |      |         assert "data:" in line     |
   |     assert data == "ECHO: Hello"   |      +------------------------------------+
   +------------------------------------+                        |
                     |                                           v
                     v                                 Tests stream chunk-by-chunk
           Tests bidirectional frames                  Closes stream on context exit
           Tests custom close codes
```

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. WebSocket Testing via Starlette `TestClient`
Starlette's `TestClient` provides the cleanest, most reliable synchronous test interface for WebSockets:
```python
from fastapi import FastAPI, WebSocket
from fastapi.testclient import TestClient

app = FastAPI()

@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    data = await ws.receive_text()
    await ws.send_text(f"ACK: {data}")
    await ws.close()

def test_websocket_basic():
    client = TestClient(app)
    # Opens persistent WebSocket connection in test
    with client.websocket_connect("/ws") as websocket:
        websocket.send_text("Ping")
        data = websocket.receive_text()
        assert data == "ACK: Ping"
```

### 2. SSE Testing via `httpx.AsyncClient.stream()`
```python
import pytest
from httpx import AsyncClient, ASGITransport

@pytest.mark.anyio
async def test_sse_stream_iterative():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        async with client.stream("GET", "/events/stream") as response:
            assert response.status_code == 200
            assert response.headers["content-type"].startswith("text/event-stream")
            
            # Read first 3 event lines
            events = []
            async for line in response.aiter_lines():
                if line.startswith("data:"):
                    events.append(line.replace("data: ", ""))
                    if len(events) == 3:
                        break  # Break out to terminate stream
                        
            assert len(events) == 3
```

---

## 4. Practical Implementation

### Example 1: Testing Full WebSocket Chat Room Broadcasting
```python
import pytest
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.testclient import TestClient

app = FastAPI()

class RoomManager:
    def __init__(self):
        self.connections: list[WebSocket] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self.connections.append(ws)

    def disconnect(self, ws: WebSocket):
        self.connections.remove(ws)

    async def broadcast(self, message: str):
        for conn in self.connections:
            await conn.send_text(message)

manager = RoomManager()

@app.websocket("/ws/chat")
async def chat_ws(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            text = await websocket.receive_text()
            await manager.broadcast(f"Broadcast: {text}")
    except WebSocketDisconnect:
        manager.disconnect(websocket)

def test_multi_client_websocket_broadcast():
    client = TestClient(app)
    
    # Connect Client 1 and Client 2 simultaneously
    with client.websocket_connect("/ws/chat") as ws1:
        with client.websocket_connect("/ws/chat") as ws2:
            # Client 1 sends message
            ws1.send_text("Hello Everyone")
            
            # Both Client 1 and Client 2 receive broadcast!
            msg1 = ws1.receive_text()
            msg2 = ws2.receive_text()
            
            assert msg1 == "Broadcast: Hello Everyone"
            assert msg2 == "Broadcast: Hello Everyone"
```

### Example 2: Testing WebSocket Authentication and Rejection Codes
```python
import pytest
from fastapi import FastAPI, WebSocket, Query, status
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

app = FastAPI()

@app.websocket("/ws/secure")
async def secure_ws(websocket: WebSocket, token: str = Query(default="")):
    if token != "secret-token-123":
        # Reject with 1008 Policy Violation
        await websocket.close(code=status.WS_1008_POLICY_VIOLATION)
        return
    await websocket.accept()
    await websocket.send_text("Access Granted")
    await websocket.close()

def test_websocket_rejects_unauthorized_token():
    client = TestClient(app)
    # Attempt connecting with invalid token
    with pytest.raises(WebSocketDisconnect) as exc_info:
        with client.websocket_connect("/ws/secure?token=bad-token") as ws:
            ws.receive_text()
            
    # Verify exact WebSocket close code
    assert exc_info.value.code == status.WS_1008_POLICY_VIOLATION

def test_websocket_accepts_valid_token():
    client = TestClient(app)
    with client.websocket_connect("/ws/secure?token=secret-token-123") as ws:
        msg = ws.receive_text()
        assert msg == "Access Granted"
```

### Example 3: Edge Case — Testing Abrupt Client Disconnection in SSE
Testing that when an SSE client disconnects, the server-side generator catches `CancelledError` and releases resources:
```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()
resource_cleaned = {"cleaned": False}

async def monitored_sse_generator():
    resource_cleaned["cleaned"] = False
    try:
        while True:
            yield "data: tick\n\n"
            await asyncio.sleep(0.05)
    except asyncio.CancelledError:
        # Triggered when client closes stream
        resource_cleaned["cleaned"] = True
        raise

@app.get("/sse/monitored")
async def sse_monitored():
    return StreamingResponse(monitored_sse_generator(), media_type="text/event-stream")

@pytest.mark.anyio
async def test_sse_abrupt_disconnect_cleans_resources():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        async with client.stream("GET", "/sse/monitored") as response:
            assert response.status_code == 200
            # Read exactly 1 line and immediately exit the async with context (abrupt disconnect)
            async for line in response.aiter_lines():
                if line:
                    break

    # Yield event loop tick to allow cancellation cleanup to run
    await asyncio.sleep(0.01)
    assert resource_cleaned["cleaned"] is True
```

### Example 4: Failure Case — Deadlock in WebSocket Tests (Awaiting Unsent Messages)
```python
# CATASTROPHIC ANTI-PATTERN
def test_deadlock():
    client = TestClient(app)
    with client.websocket_connect("/ws/chat") as ws:
        # FAILS: Endpoint only sends when a message is received!
        # Calling receive_text() when server is waiting to receive causes
        # pytest to hang forever until test timeout kills it!
        data = ws.receive_text()
```

### Example 5: Idiomatic Modern — Testing JSON WebSocket Protocols
```python
def test_json_rpc_protocol_over_websocket():
    client = TestClient(app)
    with client.websocket_connect("/ws/rpc") as ws:
        # Send JSON frame
        ws.send_json({"method": "add", "params": [10, 25], "id": 1})
        # Receive JSON frame
        res = ws.receive_json()
        assert res == {"id": 1, "result": 35}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Deadlocks on Synchronous Receive**: In `TestClient`, calling `websocket.receive_text()` blocks execution until a message arrives. If the server does not emit a message, the test hangs indefinitely. Always structure test sequences to match the server's expected send/receive protocol.
2. **Missing `raise` in CancelledError**: In SSE streaming generators, if you catch `asyncio.CancelledError` to clean up resources, you **must re-raise it**. Swallowing `CancelledError` prevents the ASGI server from terminating the connection frame.
3. **Buffer Deadlocks in SSE Testing**: If your SSE test uses `response.read()` instead of `client.stream()`, `httpx` will attempt to read the entire infinite stream into memory before returning, hanging the test suite indefinitely.
4. **WebSocket Binary Frames vs Text Frames**: Calling `ws.send_bytes(b"data")` sends binary frames (`OPCODE 2`), whereas `ws.send_text("data")` sends text frames (`OPCODE 1`). Calling `receive_text()` when binary was sent raises `RuntimeError`.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Calling `response.read()` on infinite SSE streams | Treating stream like standard HTTP response | Test hangs forever until timeout | Use `async with client.stream()` |
| Calling `receive_text()` before server sends data | Sequence misalignment | Test suite hangs in permanent deadlock | Match client send/receive to server state machine |
| Swallowing `CancelledError` in SSE generator | Writing `except BaseException: pass` | Leaks background task on client disconnect | Always re-raise `asyncio.CancelledError` |
| Attempting WebSocket testing with raw `httpx.AsyncClient` | `httpx` lacks native WebSocket support | `InvalidURL` or handshake protocol error | Use `TestClient.websocket_connect` or `httpx-ws` |
| Not asserting on WebSocket close codes | Ignoring disconnect metadata | Fails to catch incorrect policy violation codes | Assert `exc_info.value.code == 1008` |
| Testing SSE without checking `Cache-Control` | Missing proxy header verification | SSE breaks when deployed behind Nginx | Assert `Cache-Control: no-cache` in headers |

---

## 7. Performance & Scalability Considerations

- **Speed of In-Memory WebSockets**: Starlette's `websocket_connect()` runs completely in-memory via ASGI message queues, executing a full handshake, 10 frame exchanges, and clean disconnect in $<3\text{ms}$.
- **Iterative Stream Chunking**: Reading 1,000 SSE events with `aiter_lines()` consumes $<100\text{KB}$ of memory because lines are evaluated as they arrive without buffering the stream.

---

## 8. Security & OWASP Guardrails

- **Origin Header Validation in Tests**: Write automated tests specifically passing unauthorized `Origin: https://malicious-site.com` headers to verify the WebSocket handshake rejects Cross-Site WebSocket Hijacking (CSWSH) attempts.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from fastapi import FastAPI, WebSocket, status
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

app = FastAPI()

@app.websocket("/ws/origin-check")
async def origin_check_ws(ws: WebSocket):
    origin = ws.headers.get("origin")
    if origin != "https://trusted.corp":
        await ws.close(code=status.WS_1008_POLICY_VIOLATION)
        return
    await ws.accept()
    await ws.send_text("OK")
    await ws.close()

def test_origin_header_security_rejection():
    client = TestClient(app)
    
    # 1. Untrusted origin rejected
    with pytest.raises(WebSocketDisconnect) as exc:
        with client.websocket_connect("/ws/origin-check", headers={"Origin": "https://evil.com"}) as ws:
            ws.receive_text()
    assert exc.value.code == status.WS_1008_POLICY_VIOLATION
    
    # 2. Trusted origin accepted
    with client.websocket_connect("/ws/origin-check", headers={"Origin": "https://trusted.corp"}) as ws:
        msg = ws.receive_text()
        assert msg == "OK"
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A financial analytics platform deployed a real-time cryptocurrency order book stream using Server-Sent Events. After 48 hours in production, the Kubernetes cluster ran out of memory. All pods were terminated by the OOM killer.

**Root Cause**: The SSE generator opened a database cursor and subscribed to a Redis pub/sub channel. The developer wrote unit tests that only verified `resp.status_code == 200` by reading the first line. The developer had **never written a disconnection test**. When real users closed browser tabs, the server-side generator caught `Exception` (ignoring `asyncio.CancelledError`), keeping the Redis pub/sub listener running in memory forever. With 50,000 user disconnects per day, 50,000 zombie generators consumed 16GB of RAM.

**The Fix**:
1. Fixed the generator to strictly handle `CancelledError` and close resources in a `finally` block:
   ```python
   try:
       yield event
   finally:
       await pubsub.unsubscribe()
       await pubsub.close()
   ```
2. Added an automated pytest test (identical to Example 3) that abruptly breaks an SSE stream and asserts `pubsub.is_closed is True`.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. How do you test WebSocket endpoints in FastAPI without starting a live Uvicorn network server?</summary>
Use Starlette's <code>TestClient(app).websocket_connect("/ws_url")</code>. It connects directly to the FastAPI ASGI application in-memory via internal asyncio queues, allowing synchronous frame transmission (<code>send_text()</code>) and reception (<code>receive_text()</code>) in sub-milliseconds without opening physical TCP sockets.
</details>

<details>
<summary>2. Why will calling <code>response.read()</code> on an SSE streaming endpoint cause a test to hang?</summary>
Server-Sent Events endpoints use infinite (or long-lived) HTTP response streams. <code>response.read()</code> attempts to read the entire HTTP body until the connection closes. Because the stream never terminates, <code>response.read()</code> hangs indefinitely. You must use <code>async with client.stream(...)</code> and iterate over events with <code>aiter_lines()</code>.
</details>

<details>
<summary>3. What exception is raised in <code>TestClient</code> when a server closes a WebSocket connection?</summary>
<code>starlette.websockets.WebSocketDisconnect</code>. The exception object contains a <code>code</code> attribute (e.g. <code>exc.value.code == 1000</code> for normal closure or <code>1008</code> for policy violation).
</details>

<details>
<summary>4. How do you test that server resources are properly cleaned up when an SSE client abruptly closes the connection?</summary>
Open the stream inside an <code>async with client.stream(...)</code> block, read one event line, and immediately exit the <code>async with</code> context. This closes the client transport, injecting an <code>asyncio.CancelledError</code> into the server generator. Assert that your cleanup fixture flag or mock close method was executed.
</details>

<details>
<summary>5. How can you test multi-client WebSocket broadcasting in a single test function?</summary>
Nest multiple <code>client.websocket_connect()</code> context managers inside the same test function (e.g. <code>with client.websocket_connect(...) as ws1, client.websocket_connect(...) as ws2:</code>). Send a message from <code>ws1</code> and assert that <code>ws2.receive_text()</code> receives the broadcast.
</details>

<details>
<summary>6. What is the difference between <code>send_text()</code> and <code>send_bytes()</code> in WebSocket testing?</summary>
<code>send_text()</code> transmits UTF-8 text frames (opcode 1). <code>send_bytes()</code> transmits raw binary frames (opcode 2). The server endpoint must match the expected frame type (e.g. <code>receive_bytes()</code> vs <code>receive_text()</code>), otherwise a runtime error occurs.
</details>

<details>
<summary>7. Why is testing the <code>Origin</code> header critical for WebSocket security?</summary>
Browsers do not enforce the Same-Origin Policy on WebSockets; they allow any malicious website to open a WebSocket connection to your domain, automatically sending user session cookies. Testing that your endpoint rejects unexpected <code>Origin</code> headers with close code 1008 prevents Cross-Site WebSocket Hijacking (CSWSH).
</details>

<details>
<summary>8. How do you test JSON-based protocols (like JSON-RPC) over WebSockets?</summary>
Use <code>websocket.send_json(payload)</code> and <code>res = websocket.receive_json()</code>. These methods automatically handle JSON serialization and deserialization over text frames.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Echo WebSocket Test
**Task**: Create a WebSocket endpoint that echoes incoming text prefixed with `"ECHO: "`, and write a test using `TestClient`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, WebSocket
from fastapi.testclient import TestClient

app = FastAPI()

@app.websocket("/ws/echo")
async def echo(ws: WebSocket):
    await ws.accept()
    msg = await ws.receive_text()
    await ws.send_text(f"ECHO: {msg}")
    await ws.close()

def test_ws_echo():
    client = TestClient(app)
    with client.websocket_connect("/ws/echo") as ws:
        ws.send_text("Hello World")
        reply = ws.receive_text()
        assert reply == "ECHO: Hello World"
```
</details>

### Exercise 2 (Easy): WebSocket Disconnect Code Verification
**Task**: Write an endpoint that immediately closes with status code `1001` (Going Away). In your test, catch `WebSocketDisconnect` and assert `code == 1001`.
<details>
<summary>Solution</summary>

```python
import pytest
from fastapi import FastAPI, WebSocket, status
from fastapi.testclient import TestClient
from starlette.websockets import WebSocketDisconnect

app = FastAPI()

@app.websocket("/ws/close-fast")
async def close_fast(ws: WebSocket):
    await ws.accept()
    await ws.close(code=status.WS_1001_GOING_AWAY)

def test_close_code():
    client = TestClient(app)
    with pytest.raises(WebSocketDisconnect) as exc:
        with client.websocket_connect("/ws/close-fast") as ws:
            ws.receive_text()
    assert exc.value.code == status.WS_1001_GOING_AWAY
```
</details>

### Exercise 3 (Medium): Iterative SSE Stream Line Matcher
**Task**: Write an endpoint that streams 3 lines of SSE data. Test using `httpx.AsyncClient` that all 3 lines are received in order with correct `data:` prefix.
<details>
<summary>Solution</summary>

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()

async def gen_three():
    for i in range(1, 4):
        yield f"data: item_{i}\n\n"

@app.get("/sse/three")
async def three_items():
    return StreamingResponse(gen_three(), media_type="text/event-stream")

@pytest.mark.anyio
async def test_three_sse_items():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        async with client.stream("GET", "/sse/three") as resp:
            lines = [line async for line in resp.aiter_lines() if line.startswith("data:")]
            assert lines == ["data: item_1", "data: item_2", "data: item_3"]
```
</details>

### Exercise 4 (Medium): JSON WebSocket Bidirectional Ping-Pong
**Task**: Build a WebSocket route that receives `{"type": "PING"}`, replies with `{"type": "PONG", "server": "fastapi"}`, and closes. Write a test asserting on the received JSON structure.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, WebSocket
from fastapi.testclient import TestClient

app = FastAPI()

@app.websocket("/ws/pingpong")
async def pingpong(ws: WebSocket):
    await ws.accept()
    data = await ws.receive_json()
    if data.get("type") == "PING":
        await ws.send_json({"type": "PONG", "server": "fastapi"})
    await ws.close()

def test_json_pingpong():
    client = TestClient(app)
    with client.websocket_connect("/ws/pingpong") as ws:
        ws.send_json({"type": "PING"})
        reply = ws.receive_json()
        assert reply == {"type": "PONG", "server": "fastapi"}
```
</details>

### Exercise 5 (Hard): Multi-Client Peer-to-Peer Relay Test
**Task**: Create an in-memory relay room. Client 1 sends a message addressed to Client 2 (`{"to": "client2", "msg": "secret"}`). Verify Client 2 receives the message, while Client 3 receives nothing.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.testclient import TestClient

app = FastAPI()
registry: dict[str, WebSocket] = {}

@app.websocket("/ws/direct/{client_id}")
async def direct_ws(ws: WebSocket, client_id: str):
    await ws.accept()
    registry[client_id] = ws
    try:
        while True:
            data = await ws.receive_json()
            target_id = data.get("to")
            target_ws = registry.get(target_id)
            if target_ws:
                await target_ws.send_json({"from": client_id, "msg": data.get("msg")})
    except WebSocketDisconnect:
        registry.pop(client_id, None)

def test_targeted_direct_relay():
    client = TestClient(app)
    with client.websocket_connect("/ws/direct/c1") as ws1, \
         client.websocket_connect("/ws/direct/c2") as ws2, \
         client.websocket_connect("/ws/direct/c3") as ws3:
        
        # c1 sends targeted message to c2
        ws1.send_json({"to": "c2", "msg": "hello_c2"})
        
        # c2 receives message
        received = ws2.receive_json()
        assert received == {"from": "c1", "msg": "hello_c2"}
```
</details>

### Exercise 6 (Hard): SSE Stream Termination Verification
**Task**: Build an SSE endpoint that queries an active flag. In pytest, break the stream after 2 items and assert that the server-side generator's `finally` block set `active_connections = 0`.
<details>
<summary>Solution</summary>

```python
import pytest
import asyncio
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from fastapi.responses import StreamingResponse

app = FastAPI()
state = {"active_streams": 0}

async def monitored_stream():
    state["active_streams"] += 1
    try:
        count = 0
        while True:
            count += 1
            yield f"data: {count}\n\n"
            await asyncio.sleep(0.01)
    finally:
        state["active_streams"] -= 1

@app.get("/sse/track")
async def track_sse():
    return StreamingResponse(monitored_stream(), media_type="text/event-stream")

@pytest.mark.anyio
async def test_stream_active_counter():
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        async with client.stream("GET", "/sse/track") as response:
            assert state["active_streams"] == 1
            lines = []
            async for line in response.aiter_lines():
                if line.startswith("data:"):
                    lines.append(line)
                    if len(lines) == 2:
                        break
                        
    # Outside context: connection closed, generator torn down
    await asyncio.sleep(0.02)
    assert state["active_streams"] == 0
```
</details>
