---
title: "Redis Streams and Pub/Sub in FastAPI"
category: "10-background-and-messaging"
order: 9
tags: ["fastapi", "redis", "redis-streams", "pubsub", "consumer-groups", "realtime"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Redis Streams and Pub/Sub in FastAPI

## 1. Conceptual Foundation & Mental Model

Redis is celebrated as an ultra-fast in-memory key-value data store, but it also provides two distinct messaging paradigms that are foundational to modern FastAPI architectures:
1. **Redis Pub/Sub (Publish/Subscribe)**: A transient, at-most-once broadcasting mechanism. Publishers push messages to channels; active subscribers receive them instantly. If a subscriber is disconnected or offline when a message is published, **the message is lost forever**. There is no message history, no persistence, and no acknowledgement.
2. **Redis Streams (Introduced in Redis 5.0)**: An append-only, durable log data structure modeled heavily after Apache Kafka. Messages appended to a stream via `XADD` are stored permanently in Redis memory (persisted to RDB/AOF disk). Streams support **Consumer Groups** (`XREADGROUP`), persistent offsets, pending message tracking (`XPENDING`), consumer acknowledgements (`XACK`), and historical time-travel replay.

### The Mental Model: The Live Radio Broadcast vs The Recorded Tape Reel
- **Redis Pub/Sub** is a **Live FM Radio Broadcast**. The radio station (FastAPI producer) transmits a song. If your car radio is turned on and tuned to 98.5 FM (subscriber), you hear the music. If your car radio was turned off for 3 seconds while driving through a tunnel, you missed those 3 seconds forever; the radio station does not replay it for you.
- **Redis Streams** is an **Endless Cassette Tape Reel**. Every event recorded onto the tape stays on the tape with an exact microsecond timestamp offset (`1700000000000-0`). Multiple listener groups can play the tape forward, pause, rewind, catch up, and mark items as processed (`XACK`). If a listener's tape player breaks, a replacement player can resume from the exact pending offset.

---

## 2. Architecture & Comparison

```
                         Redis Messaging Dual Modes
                                      |
         +----------------------------+----------------------------+
         |                                                         |
         v                                                         v
   Redis Pub/Sub                                             Redis Streams
   `PUBLISH channel payload`                                 `XADD stream * field value`
         |                                                         |
   Transient In-Memory Push                                  Durable Append-Only Log
   NO persistence / NO history                               Full history with timestamps
         |                                                         |
   Active Subscribers Receive                                Consumer Groups (`XREADGROUP`)
   (Offline = LOST)                                          (Offline = Resumes from Offset)
         |                                                         |
   Use Case:                                                 Use Case:
   - WebSocket Chat Broadcast                                - Reliable background jobs
   - Cache Invalidation signals                              - Event sourcing
   - In-memory event bus                                     - Auditing & order processing
```

### Feature Comparison Matrix

| Feature | Redis Pub/Sub | Redis Streams |
|:---|:---|:---|
| **Delivery Guarantee** | At-most-once (fire-and-forget) | At-least-once (with Consumer Groups) |
| **Persistence** | None (ephemeral in-memory socket push) | Durable (stored in memory & persisted to disk) |
| **Historical Replay** | No | Yes (via `XRANGE` or offset rewind) |
| **Consumer Acknowledgements**| No | Yes (`XACK`) |
| **Dead/Pending Tracking** | No | Yes (Pending Entries List via `XPENDING`) |
| **Ideal For** | Live WebSocket UI sync, SSE fanout | Distributed task queues, audit trails, event sourcing |

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Redis Pub/Sub Publishing and Listening
```python
import redis.asyncio as redis

# Publisher:
async def publish_chat_message(r: redis.Redis, channel: str, message: str):
    await r.publish(channel, message)

# Subscriber:
async def listen_to_channel(r: redis.Redis, channel: str):
    pubsub = r.pubsub()
    await pubsub.subscribe(channel)
    try:
        async for msg in pubsub.listen():
            if msg["type"] == "message":
                print(f"Received message: {msg['data'].decode()}")
    finally:
        await pubsub.unsubscribe(channel)
        await pubsub.close()
```

### 2. Redis Streams Appending (`XADD`) and Group Reading (`XREADGROUP`)
```python
import redis.asyncio as redis

# 1. Append event to stream
async def append_event(r: redis.Redis, stream: str, data: dict):
    # '*' automatically generates a microsecond-based ID (e.g. 1716000000000-0)
    msg_id = await r.xadd(stream, data)
    return msg_id

# 2. Read with consumer group
async def read_from_group(r: redis.Redis, stream: str, group: str, consumer_name: str):
    # '>' means read messages never delivered to any other consumer in this group
    entries = await r.xreadgroup(group, consumer_name, {stream: ">"}, count=10, block=2000)
    for stream_name, messages in entries:
        for msg_id, fields in messages:
            print(f"Processing message {msg_id}: {fields}")
            # Acknowledge completion
            await r.xack(stream, group, msg_id)
```

---

## 4. Practical Implementation

### Example 1: Basic Pub/Sub WebSocket Broadcaster in FastAPI
```python
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import redis.asyncio as redis
import asyncio

app = FastAPI()
redis_pool = redis.ConnectionPool(host="localhost", port=6379)

@app.websocket("/ws/chat/{room_id}")
async def chat_websocket(websocket: WebSocket, room_id: str):
    await websocket.accept()
    r = redis.Redis(connection_pool=redis_pool)
    pubsub = r.pubsub()
    await pubsub.subscribe(f"chat:{room_id}")
    
    async def redis_listener():
        try:
            async for msg in pubsub.listen():
                if msg["type"] == "message":
                    await websocket.send_text(msg["data"].decode())
        except Exception:
            pass

    listener_task = asyncio.create_task(redis_listener())
    
    try:
        while True:
            text = await websocket.receive_text()
            # Publish message to Redis channel: all pods receive and broadcast it!
            await r.publish(f"chat:{room_id}", text)
    except WebSocketDisconnect:
        listener_task.cancel()
        await pubsub.unsubscribe(f"chat:{room_id}")
        await pubsub.close()
        await r.close()
```

### Example 2: Production Redis Streams Worker Loop with Consumer Groups
```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator
import redis.asyncio as redis
from fastapi import FastAPI, Request
from pydantic import BaseModel

STREAM_NAME = "order_stream"
GROUP_NAME = "order_fulfillment_group"

async def setup_stream_group(r: redis.Redis):
    try:
        # Create consumer group starting from beginning ('0')
        await r.xgroup_create(STREAM_NAME, GROUP_NAME, id="0", mkstream=True)
        print("Consumer group initialized.")
    except redis.ResponseError as e:
        if "BUSYGROUP" not in str(e):
            raise

async def stream_consumer_worker(r: redis.Redis, stop_event: asyncio.Event):
    await setup_stream_group(r)
    consumer_name = "worker_pod_1"
    
    while not stop_event.is_set():
        try:
            # Poll with 2-second blocking timeout
            response = await r.xreadgroup(GROUP_NAME, consumer_name, {STREAM_NAME: ">"}, count=5, block=2000)
            if not response:
                continue
                
            for stream, messages in response:
                for msg_id, fields in messages:
                    # fields is dict of bytes
                    order_id = fields[b"order_id"].decode()
                    print(f"Fulfilling order: {order_id}")
                    await asyncio.sleep(0.1)  # Process order
                    # Acknowledge
                    await r.xack(STREAM_NAME, GROUP_NAME, msg_id)
        except asyncio.CancelledError:
            break
        except Exception as exc:
            print(f"Stream consumer error: {exc}")
            await asyncio.sleep(1.0)

app = FastAPI()

@app.post("/orders")
async def create_order(order_id: str, request: Request):
    r: redis.Redis = request.app.state.redis
    # Append to stream
    msg_id = await r.xadd(STREAM_NAME, {"order_id": order_id, "status": "CREATED"})
    return {"status": "ENQUEUED", "stream_id": msg_id.decode()}
```

### Example 3: Edge Case — Claiming Orphaned / Dead Consumer Messages (`XAUTOCLAIM`)
If a worker crashes while processing a message, the message remains in the Pending Entries List (PEL). Other workers can claim and finish the abandoned message using `xautoclaim`:
```python
import redis.asyncio as redis

async def recover_orphaned_messages(r: redis.Redis, stream: str, group: str, new_consumer: str):
    # Claim any pending message that has been idle for more than 60,000ms (1 minute)
    # Starts search from offset '0-0'
    res = await r.xautoclaim(stream, group, new_consumer, min_idle_time=60000, start_id="0-0", count=10)
    next_id, claimed_messages, deleted_ids = res
    
    for msg_id, fields in claimed_messages:
        print(f"Recovered abandoned message {msg_id}: {fields}")
        # Finish processing and ACK
        await r.xack(stream, group, msg_id)
```

### Example 4: Failure Case — Unbounded Stream Growth (Memory Leak)
```python
# CATASTROPHIC ANTI-PATTERN: Appending to stream without maxlen
async def bad_stream_producer(r: redis.Redis, metric_data: dict):
    # Appending millions of messages without trimming!
    # Redis memory grows continuously until Redis is killed by OOM!
    await r.xadd("metrics_stream", metric_data)
```

### Example 5: Idiomatic Modern — Stream Trimming with Approximate Capping (`MAXLEN ~`)
```python
async def good_stream_producer(r: redis.Redis, metric_data: dict):
    # 'maxlen=100000' with approximate flag '~' keeps approximately 100,000 records.
    # Approximate trimming is 100x faster because it deletes entire macro-nodes!
    await r.xadd(
        "metrics_stream",
        metric_data,
        maxlen=100000,
        approximate=True
    )
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Pub/Sub Client Buffer Limits**: If a WebSocket client's network connection slows down and cannot read messages as fast as Redis publishes them, Redis's output buffer (`client-output-buffer-limit pubsub`) fills up. When the limit is reached, Redis **forcibly terminates the connection**!
2. **Pending Entries List (PEL) Bloat in Streams**: If workers consume messages via `XREADGROUP` but forget to invoke `XACK`, the messages remain in the PEL forever. Redis holds references to them in RAM, preventing trimming and exhausting memory.
3. **Stream Trimming Performance (`MAXLEN` vs `MAXLEN ~`)**: Exact trimming (`maxlen=1000`) forces Redis to re-balance radix trees on every single write, degrading write latency. Always use approximate trimming (`approximate=True` or `~`).
4. **Pub/Sub Sharding**: In clustered Redis deployments, standard Pub/Sub broadcasts every message to every single node in the cluster. For large clusters, use **Sharded Pub/Sub (`SPUBLISH` / `SSUBSCRIBE`)** introduced in Redis 7.0.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Using Pub/Sub for background jobs | Believing Pub/Sub has queues | Offline workers lose all messages | Use Redis Streams with Consumer Groups |
| Omitting `MAXLEN ~` on streams | Forgetting streams are persistent | Redis memory fills up; triggers OOM crash | Specify `maxlen=50000, approximate=True` |
| Forgetting `XACK` after processing stream job | Misunderstanding consumer groups | PEL bloats memory; duplicate processing on restart | Always call `await r.xack(stream, group, id)` |
| Creating separate Pub/Sub instances per request | Leaking persistent connection handles | Redis hits maximum client limit | Share connection pool; manage client listeners |
| Exact trimming on high-frequency streams | Forcing radix tree rebuilds | Redis CPU spikes to 100% | Use approximate trimming (`~`) |
| Passing unencoded strings/bytes inconsistently | Mixing byte and str modes | Key lookup errors and unhandled TypeErrors | Explicitly set `decode_responses=True` on Redis client |

---

## 7. Performance & Scalability Considerations

- **Throughput Benchmark**: Redis Streams can ingest over 500,000 `XADD` operations per second on a single commodity core, with sub-millisecond latencies.
- **Consumer Group Parallelism**: Multiple consumers within a consumer group process separate messages concurrently from the same stream, achieving true horizontal worker scaling.

---

## 8. Security & OWASP Guardrails

- **ACL Channel Isolation**: Redis 6.0+ supports fine-grained Access Control Lists (ACLs). Restrict API worker roles to specific channels or stream patterns (e.g., `resetchannels +chat:*` or `resetkeys ~order_stream*`).

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
import redis.asyncio as redis

@pytest.mark.anyio
async def test_redis_streams_xadd_and_xread():
    r = redis.Redis(host="localhost", port=6379, decode_responses=True)
    try:
        # Ping check
        await r.ping()
    except Exception:
        pytest.skip("Local Redis instance not available")
        
    stream_key = "test_pytest_stream"
    await r.delete(stream_key)
    
    # 1. Add message
    msg_id = await r.xadd(stream_key, {"user": "alice", "action": "login"})
    assert msg_id is not None
    
    # 2. Read range
    entries = await r.xrange(stream_key, "-", "+")
    assert len(entries) == 1
    assert entries[0][1]["user"] == "alice"
    
    await r.delete(stream_key)
    await r.aclose()
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A sports betting application used Redis Pub/Sub to send live score updates and bet settlement events to mobile users. During a major football tournament, the Redis server crashed every 15 minutes with 100% CPU utilization and memory spikes.

**Root Cause**: The engineering team used standard Pub/Sub on a 12-node Redis Cluster. In standard Redis Pub/Sub, every `PUBLISH` command is broadcast across cluster interconnect buses to **all 12 nodes**, regardless of whether any subscriber is listening on that node. With 30,000 updates/sec, cluster bus traffic overwhelmed the network interface.

**The Fix**:
1. Switched from global Pub/Sub to **Sharded Pub/Sub (`SPUBLISH` / `SSUBSCRIBE`)**, which binds channels to specific cluster hash slots.
2. For bet settlements requiring reliable delivery, migrated from Pub/Sub to **Redis Streams** with consumer groups, ensuring zero lost bets.
3. Redis CPU dropped from 100% to 14%, and cluster network traffic decreased by 91%.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What is the fundamental difference between Redis Pub/Sub and Redis Streams?</summary>
<b>Redis Pub/Sub</b> is transient and at-most-once: messages are pushed directly to active connected sockets; if a client is offline, the message is lost forever without storage or acknowledgements. <b>Redis Streams</b> is a persistent, append-only log data structure: messages are stored in memory and on disk, supporting Consumer Groups, message offsets, acknowledgements (<code>XACK</code>), and historical replay.
</details>

<details>
<summary>2. How does a Redis Streams Consumer Group distribute messages among workers?</summary>
When consumers in a group read using <code>XREADGROUP</code> with the ID <code>></code>, Redis delivers each new message to exactly <b>one</b> consumer in the group. The message is placed in that consumer's Pending Entries List (PEL) until the consumer calls <code>XACK</code>.
</details>

<details>
<summary>3. What is the purpose of the <code>XAUTOCLAIM</code> command in Redis Streams?</summary>
If a worker crashes or loses network connectivity while processing a message, the message remains unacknowledged in the Pending Entries List (PEL). <code>XAUTOCLAIM</code> allows other active workers to inspect the PEL, identify messages that have been idle for longer than a specified timeout, and claim ownership to finish processing them.
</details>

<details>
<summary>4. Why should you always use approximate trimming (<code>MAXLEN ~</code>) rather than exact trimming on Redis Streams?</summary>
Exact trimming (<code>MAXLEN 1000</code>) forces Redis to delete exact items and rebalance radix tree nodes on every write. Approximate trimming (<code>MAXLEN ~ 1000</code>) allows Redis to delete entire macro-nodes when convenient, providing a 10x–100x performance boost with virtually identical bounding behavior.
</details>

<details>
<summary>5. What happens to a Redis Pub/Sub subscriber if its network connection lags behind the publisher?</summary>
Redis buffers outgoing messages in an output buffer. If the buffer exceeds <code>client-output-buffer-limit pubsub</code> (e.g. 32MB continuous or 8MB for 60 seconds), Redis detects the slow subscriber and forcibly closes its connection to protect server memory.
</details>

<details>
<summary>6. How do message IDs work in Redis Streams?</summary>
Stream IDs default to the format <code><millisecondsTime>-<sequenceNumber></code> (e.g. <code>1716000000000-0</code>). They are strictly monotonic and generated based on the server's local clock, providing chronological ordering for all events.
</details>

<details>
<summary>7. What is Sharded Pub/Sub in Redis 7.0+?</summary>
In a Redis Cluster, standard Pub/Sub broadcasts messages to every node in the cluster. Sharded Pub/Sub (<code>SPUBLISH</code> / <code>SSUBSCRIBE</code>) routes messages only to the specific cluster node that owns the hash slot of the channel name, dramatically reducing inter-node cluster bus bandwidth.
</details>

<details>
<summary>8. How do you implement Fan-Out broadcasting using Redis Streams?</summary>
By creating multiple distinct <b>Consumer Groups</b> on the same stream. Each consumer group maintains its own independent offset, allowing multiple independent services (e.g., Billing, Fraud, Analytics) to each read every message in the stream.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Stream Event Ingestion Route
**Task**: Build a route `POST /events/click` that writes a click event to a Redis stream `clicks_stream` capped at approximately 10,000 entries.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request
from pydantic import BaseModel
import redis.asyncio as redis

app = FastAPI()

class ClickData(BaseModel):
    button_id: str
    user_id: str

@app.post("/events/click")
async def track_click(data: ClickData, request: Request):
    r: redis.Redis = request.app.state.redis
    msg_id = await r.xadd(
        "clicks_stream",
        data.model_dump(),
        maxlen=10000,
        approximate=True
    )
    return {"status": "RECORDED", "event_id": msg_id}
```
</details>

### Exercise 2 (Easy): Realtime Broadcast Publisher
**Task**: Create an administrative route `POST /broadcast/system-announcement` that publishes a notification payload to a Redis Pub/Sub channel `system:announcements`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request
from pydantic import BaseModel
import redis.asyncio as redis

app = FastAPI()

class Announcement(BaseModel):
    title: str
    body: str

@app.post("/broadcast/system-announcement")
async def broadcast_announcement(payload: Announcement, request: Request):
    r: redis.Redis = request.app.state.redis
    subscribers_count = await r.publish("system:announcements", payload.model_dump_json())
    return {"status": "BROADCASTED", "active_subscribers": subscribers_count}
```
</details>

### Exercise 3 (Medium): Consumer Group Setup Lifespan
**Task**: Write a FastAPI lifespan handler that checks if a consumer group `billing_workers` exists on stream `invoices_stream`, creating it with `mkstream=True` if absent.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
import redis.asyncio as redis
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI):
    r = redis.Redis(host="localhost", port=6379, decode_responses=True)
    app.state.redis = r
    
    try:
        # Create group starting at beginning of stream
        await r.xgroup_create("invoices_stream", "billing_workers", id="0", mkstream=True)
        print("Consumer group billing_workers ready.")
    except redis.ResponseError as exc:
        if "BUSYGROUP" in str(exc):
            print("Consumer group already exists.")
        else:
            raise
            
    yield
    await r.aclose()

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 4 (Medium): Stream Historical Range Query
**Task**: Implement a route `GET /events/history` that queries the last 20 events from `clicks_stream` using `XREVRANGE`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request
import redis.asyncio as redis

app = FastAPI()

@app.get("/events/history")
async def get_stream_history(request: Request):
    r: redis.Redis = request.app.state.redis
    # XREVRANGE reads from newest (+) to oldest (-)
    raw_entries = await r.xrevrange("clicks_stream", max="+", min="-", count=20)
    
    history = []
    for msg_id, fields in raw_entries:
        history.append({"id": msg_id, "data": fields})
    return {"count": len(history), "events": history}
```
</details>

### Exercise 5 (Hard): Dead-Message Auto-Claimer Coroutine
**Task**: Implement an asynchronous background maintenance loop that runs every 30 seconds, uses `XAUTOCLAIM` to recover messages pending for $>60\text{s}$, and re-processes them.
<details>
<summary>Solution</summary>

```python
import asyncio
import redis.asyncio as redis

async def run_autoclaim_sweeper(r: redis.Redis, stream: str, group: str, worker_id: str, stop_event: asyncio.Event):
    while not stop_event.is_set():
        try:
            # Reclaim messages idle for more than 60,000ms
            res = await r.xautoclaim(stream, group, worker_id, min_idle_time=60000, start_id="0-0", count=10)
            next_id, claimed_messages, deleted_ids = res
            
            for msg_id, fields in claimed_messages:
                print(f"Sweeper claimed abandoned task {msg_id}: {fields}")
                # Reprocess and acknowledge
                await r.xack(stream, group, msg_id)
                
            await asyncio.sleep(30.0)
        except asyncio.CancelledError:
            break
        except Exception as exc:
            print(f"Sweeper error: {exc}")
            await asyncio.sleep(5.0)
```
</details>

### Exercise 6 (Hard): Multi-Room Redis Pub/Sub WebSocket Hub
**Task**: Build a complete, production-ready multi-room WebSocket hub that subscribes dynamically to room channels using a single connection pool, forwarding messages cleanly to connected client sockets.
<details>
<summary>Solution</summary>

```python
import asyncio
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import redis.asyncio as redis

app = FastAPI()
pool = redis.ConnectionPool(host="localhost", port=6379, decode_responses=True)

@app.websocket("/ws/rooms/{room_name}")
async def room_websocket(ws: WebSocket, room_name: str):
    await ws.accept()
    r = redis.Redis(connection_pool=pool)
    ps = r.pubsub()
    channel = f"room:{room_name}"
    await ps.subscribe(channel)

    async def forward_from_redis():
        try:
            async for msg in ps.listen():
                if msg["type"] == "message":
                    await ws.send_text(msg["data"])
        except Exception:
            pass

    forwarder_task = asyncio.create_task(forward_from_redis())

    try:
        while True:
            client_msg = await ws.receive_text()
            # Broadcast to all instances of room
            await r.publish(channel, client_msg)
    except WebSocketDisconnect:
        forwarder_task.cancel()
        await ps.unsubscribe(channel)
        await ps.close()
        await r.close()
```
</details>
