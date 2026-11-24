---
title: "RabbitMQ and AMQP Integration in FastAPI with aio-pika"
category: "10-background-and-messaging"
order: 8
tags: ["fastapi", "rabbitmq", "aio-pika", "amqp", "exchanges", "message-queues"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# RabbitMQ and AMQP Integration in FastAPI with aio-pika

## 1. Conceptual Foundation & Mental Model

While Apache Kafka is designed for high-throughput, partitioned stream logs, **RabbitMQ** is the industry standard for sophisticated, fine-grained **Enterprise Message Routing**. Built on the Advanced Message Queuing Protocol (AMQP 0-9-1), RabbitMQ decouples message producers from consumer queues using an intermediate routing abstraction known as an **Exchange**.

In asynchronous FastAPI applications, interacting with RabbitMQ requires a non-blocking AMQP library. The gold standard is **`aio-pika`**, an asynchronous Python client built directly on top of `asyncio` and `aiormq`.

### The Core AMQP Routing Primitives
1. **Producer**: The FastAPI application generating the message.
2. **Exchange**: The message postal routing hub. The producer *never* sends messages directly to a queue; it sends them to an exchange. The exchange inspects the **routing key** and message headers to determine which queues receive copies of the message.
3. **Queue**: A FIFO storage buffer in RabbitMQ holding messages until consumers process them.
4. **Binding**: The relationship link connecting an exchange to a queue (e.g. "Route all messages matching `orders.eu.*` to the `eu_orders_queue`").
5. **Consumer**: The worker process consuming messages and issuing an acknowledgement (`ack`) or rejection (`nack`).

### The Mental Model: The Central Post Office
- **FastAPI** is a citizen dropping a letter into a neighborhood mailbox.
- The **Exchange** is the regional automated sorting facility.
  - A **Direct Exchange** sorts mail strictly by exact postal zip code (exact match).
  - A **Topic Exchange** sorts mail by pattern matching (e.g. `urgent.*.california`).
  - A **Fanout Exchange** behaves like a megaphone, copying the letter and placing it into every single mailbox in town (broadcast).
- The **Queue** is the mail delivery pouch carried by the letter carrier.
- When the carrier hands you the letter, you sign for it (**`message.ack()`**). If the letter was damaged in transit, you reject it (**`message.nack(requeue=False)`**).

---

## 2. Architecture & Internal Mechanics

```
   FastAPI Web Cluster (Producer)
   +-------------------------------------------------------------+
   |   `await exchange.publish(`                                 |
   |       Message(body=b"..."),                                 |
   |       routing_key="orders.created.us"                       |
   |   )                                                         |
   +-------------------------------------------------------------+
                                  |
                                  v
   RabbitMQ Topic Exchange: `amq.topic`
   +-------------------------------------------------------------+
   |   Binding 1: `orders.*.us`   --> US Orders Processing Queue |
   |   Binding 2: `orders.#`      --> Global Audit Log Queue     |
   |   Binding 3: `*.canceled.*`  --> Cancellation Handler Queue |
   +-------------------------------------------------------------+
              |                                   |
              v                                   v
   Queue: `us_orders_queue`             Queue: `global_audit_queue`
   [ Msg 1 ] [ Msg 2 ]                  [ Msg 1 ] [ Msg 2 ]
              |                                   |
              v                                   v
   Worker 1 (aio-pika Consumer)         Worker 2 (aio-pika Consumer)
   `await message.ack()`                `await message.ack()`
```

### Publisher Confirms and Transactional Durability
In critical financial or enterprise systems, producers cannot simply push bytes into a socket and assume RabbitMQ received them. With **Publisher Confirms** enabled in `aio-pika`, the broker sends an asynchronous confirmation back to the FastAPI producer once the message has been written to disk (durable queue) and routed.

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. Robust Connection Pool Pattern in FastAPI Lifespan
```python
from aio_pika import connect_robust, Message, DeliveryMode, ExchangeType
from aio_pika.abc import AbstractRobustConnection, AbstractRobustChannel
from contextlib import asynccontextmanager
from typing import AsyncIterator
from fastapi import FastAPI

RABBITMQ_URL = "amqp://guest:guest@localhost:5672/"

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    # 1. Establish self-healing robust connection (auto-reconnects on drop)
    connection: AbstractRobustConnection = await connect_robust(RABBITMQ_URL)
    channel: AbstractRobustChannel = await connection.channel()
    
    # 2. Declare durable exchange
    exchange = await channel.declare_exchange(
        "order_events_topic",
        ExchangeType.TOPIC,
        durable=True
    )
    
    app.state.amqp_connection = connection
    app.state.amqp_channel = channel
    app.state.order_exchange = exchange
    print("RabbitMQ connection and exchange established.")
    
    yield
    
    # 3. Clean shutdown
    await channel.close()
    await connection.close()
    print("RabbitMQ connection cleanly closed.")

app = FastAPI(lifespan=lifespan)
```

### 2. Publishing Persistent Messages with Routing Keys
```python
from aio_pika import Message, DeliveryMode
from fastapi import FastAPI, Request
from pydantic import BaseModel

class OrderCreateEvent(BaseModel):
    order_id: str
    region: str
    amount: float

@app.post("/orders")
async def create_order(payload: OrderCreateEvent, request: Request):
    exchange = request.app.state.order_exchange
    
    # DeliveryMode.PERSISTENT ensures message is written to disk in RabbitMQ
    message = Message(
        body=payload.model_dump_json().encode(),
        delivery_mode=DeliveryMode.PERSISTENT,
        content_type="application/json",
        headers={"source": "fastapi_checkout"}
    )
    
    routing_key = f"order.created.{payload.region}"
    await exchange.publish(message, routing_key=routing_key)
    return {"status": "DISPATCHED", "routing_key": routing_key}
```

---

## 4. Practical Implementation

### Example 1: Direct Exchange with Work Queue Pattern
```python
from aio_pika import Message, DeliveryMode
from fastapi import FastAPI, Request, status
from pydantic import BaseModel

app = FastAPI()

class ResizeImageRequest(BaseModel):
    image_url: str
    target_width: int

@app.post("/tasks/resize-image", status_code=status.HTTP_202_ACCEPTED)
async def dispatch_image_resize(payload: ResizeImageRequest, request: Request):
    channel = request.app.state.amqp_channel
    
    # Direct publishing to default exchange with queue name as routing key
    await channel.default_exchange.publish(
        Message(
            body=payload.model_dump_json().encode(),
            delivery_mode=DeliveryMode.PERSISTENT
        ),
        routing_key="image_processing_queue"
    )
    return {"status": "QUEUED"}
```

### Example 2: Production Asynchronous Consumer with Manual Acknowledgement
```python
import asyncio
from aio_pika import connect_robust, IncomingMessage

async def start_amqp_consumer(rabbitmq_url: str, stop_event: asyncio.Event):
    connection = await connect_robust(rabbitmq_url)
    channel = await connection.channel()
    
    # Set prefetch count: worker pulls at most 10 unacknowledged messages
    await channel.set_qos(prefetch_count=10)
    
    queue = await channel.declare_queue("invoice_processing_queue", durable=True)
    
    async def on_message_received(message: IncomingMessage):
        # async with message.process() automatically ACKs on clean exit
        # and NACKs (requeue=False) if an unhandled exception occurs!
        async with message.process(requeue=False):
            data = message.body.decode()
            print(f"Processing invoice task: {data}")
            await asyncio.sleep(0.5)  # Simulate invoice generation
            print("Invoice processed successfully. ACK sent to RabbitMQ.")

    # Start consuming
    await queue.consume(on_message_received)
    print("AMQP Consumer started. Awaiting messages...")
    
    await stop_event.wait()
    await connection.close()
```

### Example 3: Edge Case — Dead-Letter Exchange (DLX) Configuration
Configuring a queue with an automatic Dead-Letter Exchange so that rejected (`nack`) or expired (TTL) messages are forwarded to an audit dead-letter queue:
```python
from aio_pika import ExchangeType
from aio_pika.abc import AbstractRobustChannel

async def setup_resilient_queues(channel: AbstractRobustChannel):
    # 1. Declare Dead-Letter Exchange (DLX)
    dlx_exchange = await channel.declare_exchange("dlx.exchange", ExchangeType.FANOUT, durable=True)
    dlx_queue = await channel.declare_queue("dead_letter_audit_queue", durable=True)
    await dlx_queue.bind(dlx_exchange)

    # 2. Declare Main Work Queue with DLX arguments
    main_queue = await channel.declare_queue(
        "payment_processing_queue",
        durable=True,
        arguments={
            "x-dead-letter-exchange": "dlx.exchange",      # Forward rejected msgs here
            "x-message-ttl": 86400000,                     # 24-hour message expiry
            "x-max-length": 50000                          # Prevent queue memory exhaustion
        }
    )
    print("Main queue declared with DLX protection.")
```

### Example 4: Failure Case — Missing `prefetch_count` Causing Consumer OOM
```python
# CATASTROPHIC ANTI-PATTERN
async def bad_consumer(channel):
    # If prefetch_count is NOT set, RabbitMQ dumps ALL 500,000 pending messages
    # across the network into this single Python process memory at once!
    # Linux OOM killer destroys the consumer instantly!
    # await channel.set_qos(prefetch_count=20) <-- CRUCIAL LINE OMITTED!
    queue = await channel.declare_queue("heavy_jobs")
    await queue.consume(process_heavy_job)
```

### Example 5: Idiomatic Modern — RPC (Request-Response Pattern) over RabbitMQ
FastAPI can publish a request message to RabbitMQ and wait for a worker to return a response via an exclusive temporary callback queue:
```python
import uuid
import asyncio
from aio_pika import Message, connect_robust
from fastapi import FastAPI, HTTPException

app = FastAPI()

@app.post("/rpc/calculate-tax")
async def calculate_tax_rpc(gross_amount: float):
    connection = await connect_robust("amqp://guest:guest@localhost:5672/")
    channel = await connection.channel()
    
    # 1. Create exclusive, auto-deleting callback queue for this request
    callback_queue = await channel.declare_queue(exclusive=True, auto_delete=True)
    corr_id = str(uuid.uuid4())
    fut = asyncio.get_running_loop().create_future()

    async def on_response(msg: IncomingMessage):
        if msg.correlation_id == corr_id:
            fut.set_result(msg.body.decode())
            await msg.ack()

    await callback_queue.consume(on_response)

    # 2. Send message with reply_to and correlation_id
    await channel.default_exchange.publish(
        Message(
            body=str(gross_amount).encode(),
            correlation_id=corr_id,
            reply_to=callback_queue.name
        ),
        routing_key="tax_calculator_service"
    )

    try:
        # 3. Wait for worker response with 3.0s timeout
        async with asyncio.timeout(3.0):
            result = await fut
            return {"gross": gross_amount, "tax": float(result)}
    except TimeoutError:
        raise HTTPException(status_code=504, detail="Tax calculation RPC timed out")
    finally:
        await connection.close()
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Unacknowledged Message Accumulation (Memory Leaks)**: If a consumer receives a message, processes it, but forgets to call `await message.ack()`, RabbitMQ marks the message as `unacked`. Unacked messages remain in RabbitMQ RAM forever, preventing other workers from consuming them.
2. **Channel vs Connection Churn**: Creating and destroying TCP connections to RabbitMQ per HTTP request destroys performance. A single application should maintain 1 or 2 long-lived AMQP connections, and multiplex concurrent operations across lightweight **Channels**.
3. **Poison Pill Infinite Loops (`nack(requeue=True)`)**: If a message causes a Python syntax error or uncaught exception, calling `message.nack(requeue=True)` immediately puts it back at the front of the queue, where it is instantly re-consumed and crashes again, pegging the CPU at 100%. **Always set `requeue=False` on persistent errors and route to DLX**.
4. **Heartbeat Timeouts**: RabbitMQ monitors consumer health using AMQP heartbeat frames (default: 60s). If a worker runs a synchronous blocking task on the event loop for 65 seconds, heartbeat frames are not sent; RabbitMQ closes the TCP connection and re-queues all unacked messages.

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Correct Practice |
|:---|:---|:---|:---|
| Missing `channel.set_qos(prefetch_count=N)` | Defaulting to unlimited prefetch | Consumer process flooded with RAM exhaustion | Always configure `prefetch_count` (e.g. 10 to 50) |
| Calling `nack(requeue=True)` on unhandled crash | Hoping another attempt will succeed | Infinite crash loop (100% CPU utilization) | Use `requeue=False` and Dead-Letter Exchange |
| Opening new AMQP connections per HTTP request | Treating AMQP like stateless HTTP | Massive latency overhead; socket exhaustion | Open persistent connection in `lifespan` |
| Forgetting `delivery_mode=DeliveryMode.PERSISTENT` | Defaulting to transient memory messages | All messages lost on RabbitMQ broker reboot | Mark messages persistent; make queues `durable=True` |
| Blocking worker event loop longer than heartbeat | Synchronous CPU computation in worker | RabbitMQ drops connection due to missing heartbeats | Offload blocking work via `anyio.to_thread.run_sync` |
| Hardcoding routing keys without validation | Typos in dotted routing strings | Messages silently dropped (unroutable) | Use Pydantic enums for routing keys |

---

## 7. Performance & Scalability Considerations

- **Prefetch Tuning**: For fast jobs ($<5\text{ms}$), set `prefetch_count=100`. For long-running jobs ($>1\text{s}$), set `prefetch_count=1` to ensure fair work distribution.
- **Connection Multiplexing**: A single AMQP TCP connection can support thousands of concurrent channels. FastAPI should share a single connection pool across all routes.

---

## 8. Security & OWASP Guardrails

- **Virtual Host Isolation (vhosts)**: Use RabbitMQ vhosts (e.g. `/production`, `/staging`) with distinct credentials to strictly segregate environments.
- **TLS Encryption (AMQPS)**: Always use `amqps://` (port 5671) with TLS certificate verification in production.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from unittest.mock import AsyncMock

app = FastAPI()

@app.post("/publish")
async def publish_route():
    # Production route
    return {"status": "ok"}

@pytest.mark.anyio
async def test_rabbitmq_route_with_mocked_exchange():
    mock_exchange = AsyncMock()
    app.state.order_exchange = mock_exchange
    
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.post("/publish")
        assert resp.status_code == 200
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A logistics tracking backend experienced total consumer fleet collapse. All 24 RabbitMQ worker pods were killed by Kubernetes with `OOMKilled` within 90 seconds of deploying a new version.

**Root Cause**: During a database migration, 400,000 package tracking updates were queued in RabbitMQ. The developer deployed a newly rewritten consumer using `aio-pika`, but forgot to call `await channel.set_qos(prefetch_count=20)`. The moment the workers connected, RabbitMQ dumped all 400,000 messages across the 24 workers. Each worker attempted to parse and hold 16,000 JSON messages simultaneously in RAM. Memory spiked past the 1GB container limit, triggering an immediate OOM kill.

**The Fix**:
1. Added `await channel.set_qos(prefetch_count=10)` before attaching the consumer.
2. Configured `async with message.process(requeue=False)` to catch unhandled errors and route them to a Dead-Letter Exchange (`dlx.exchange`).
3. Workers processed the 400,000 messages smoothly at a steady 1,200 msgs/sec while container memory stayed flat at 45MB.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. What are the three primary types of Exchanges in RabbitMQ and how do they route messages?</summary>
<b>Direct Exchange</b>: Routes messages to queues whose binding key exactly matches the message's routing key. <b>Fanout Exchange</b>: Ignores routing keys and broadcasts every message to all queues bound to it. <b>Topic Exchange</b>: Routes messages based on wildcard pattern matching against routing keys using <code>*</code> (exactly one word) and <code>#</code> (zero or more words).
</details>

<details>
<summary>2. What is the purpose of RabbitMQ's <code>prefetch_count</code> (QoS)?</summary>
<code>prefetch_count</code> specifies the maximum number of unacknowledged messages RabbitMQ will deliver to a consumer channel. Without it, RabbitMQ pushes all available queue messages into the consumer's memory buffer at once, which can cause out-of-memory crashes and uneven load distribution.
</details>

<details>
<summary>3. What is the difference between an AMQP Connection and an AMQP Channel?</summary>
An <b>AMQP Connection</b> is a physical TCP connection between the application client and the RabbitMQ broker, involving authentication and handshakes. An <b>AMQP Channel</b> is a lightweight, multiplexed virtual connection operating inside the single TCP connection. Applications maintain one long-lived connection and open multiple channels for concurrent tasks.
</details>

<details>
<summary>4. How does <code>aio-pika</code>'s <code>async with message.process()</code> context manager handle acknowledgements?</summary>
If the code inside the context block completes without errors, it automatically calls <code>await message.ack()</code>. If an unhandled exception is raised, it automatically calls <code>await message.nack(requeue=requeue)</code>, ensuring messages are never left in an unacknowledged orphan state.
</details>

<details>
<summary>5. What is a Dead-Letter Exchange (DLX) in RabbitMQ?</summary>
A DLX is an exchange where RabbitMQ automatically redirects messages that cannot be processed. Messages are dead-lettered when they are rejected (<code>nack</code> with <code>requeue=False</code>), when their TTL expires, or when queue length limits are exceeded.
</details>

<details>
<summary>6. What happens if a RabbitMQ consumer fails to acknowledge a message and disconnects?</summary>
RabbitMQ detects the closed TCP socket (or missed heartbeat). It immediately returns all unacknowledged messages held by that consumer back to the front of their respective queues, making them available for other active consumers to process.
</details>

<details>
<summary>7. What is the danger of setting <code>message.nack(requeue=True)</code> on an unexpected runtime exception?</summary>
If the exception is caused by a persistent bug or malformed data payload, requeuing the message causes it to be immediately redelivered to the consumer, which will crash and requeue it again. This creates a tight infinite crash loop that burns 100% CPU.
</details>

<details>
<summary>8. How do you implement RPC (Request-Response) over RabbitMQ?</summary>
The client publishes a request message specifying a unique <code>correlation_id</code> and a temporary exclusive <code>reply_to</code> queue name in the message properties. The worker processes the request and publishes the response to the specified <code>reply_to</code> queue with the matching <code>correlation_id</code>.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Fanout Publisher Route
**Task**: Build a route `POST /broadcast-announcement` that publishes a message to a Fanout exchange `notifications_fanout` in RabbitMQ using `aio-pika`.
<details>
<summary>Solution</summary>

```python
from aio_pika import Message, ExchangeType
from fastapi import FastAPI, Request
from pydantic import BaseModel

app = FastAPI()

class Announcement(BaseModel):
    message: str

@app.post("/broadcast-announcement")
async def broadcast(announcement: Announcement, request: Request):
    channel = request.app.state.amqp_channel
    fanout_exchange = await channel.declare_exchange("notifications_fanout", ExchangeType.FANOUT, durable=True)
    
    await fanout_exchange.publish(
        Message(body=announcement.model_dump_json().encode()),
        routing_key=""  # Ignored by fanout
    )
    return {"status": "BROADCASTED"}
```
</details>

### Exercise 2 (Easy): Topic Routing Publisher
**Task**: Implement an endpoint `POST /sensors/{sensor_type}/{location}` that publishes sensor telemetry to a topic exchange with routing key `sensor.{sensor_type}.{location}`.
<details>
<summary>Solution</summary>

```python
from aio_pika import Message
from fastapi import FastAPI, Request
from pydantic import BaseModel

app = FastAPI()

class Reading(BaseModel):
    value: float

@app.post("/sensors/{sensor_type}/{location}")
async def publish_reading(sensor_type: str, location: str, reading: Reading, request: Request):
    exchange = request.app.state.order_exchange
    routing_key = f"sensor.{sensor_type}.{location}"
    
    await exchange.publish(
        Message(body=reading.model_dump_json().encode()),
        routing_key=routing_key
    )
    return {"status": "SENT", "routing_key": routing_key}
```
</details>

### Exercise 3 (Medium): Safe Lifespan AMQP Manager
**Task**: Write a complete FastAPI lifespan function that creates a robust connection using `connect_robust`, handles channel creation, and guarantees clean closure on shutdown.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
from typing import AsyncIterator
from aio_pika import connect_robust
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    connection = await connect_robust("amqp://guest:guest@localhost:5672/")
    channel = await connection.channel()
    await channel.set_qos(prefetch_count=20)
    
    app.state.amqp_connection = connection
    app.state.amqp_channel = channel
    yield
    await channel.close()
    await connection.close()

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 4 (Medium): Bounded Worker Consumer Loop
**Task**: Implement an AMQP consumer function that sets `prefetch_count=5` and processes messages using `async with message.process(requeue=False)`.
<details>
<summary>Solution</summary>

```python
import asyncio
from aio_pika import connect_robust, IncomingMessage

async def run_safe_consumer():
    conn = await connect_robust("amqp://guest:guest@localhost:5672/")
    channel = await conn.channel()
    await channel.set_qos(prefetch_count=5)
    
    queue = await channel.declare_queue("orders_queue", durable=True)
    
    async def handler(msg: IncomingMessage):
        async with msg.process(requeue=False):
            print(f"Processing order payload: {msg.body.decode()}")
            await asyncio.sleep(0.1)

    await queue.consume(handler)
```
</details>

### Exercise 5 (Hard): Dead-Letter Exchange (DLX) Setup Script
**Task**: Create an initialization utility that configures an exchange, a work queue with DLX arguments (`x-dead-letter-exchange`), and an audit queue bound to the DLX.
<details>
<summary>Solution</summary>

```python
from aio_pika import ExchangeType
from aio_pika.abc import AbstractRobustChannel

async def configure_dlx_infrastructure(channel: AbstractRobustChannel):
    # 1. Dead-letter exchange and queue
    dlx = await channel.declare_exchange("dlx_exchange", ExchangeType.FANOUT, durable=True)
    dlq = await channel.declare_queue("rejected_messages_dlq", durable=True)
    await dlq.bind(dlx)

    # 2. Main work exchange and queue
    work_exchange = await channel.declare_exchange("work_exchange", ExchangeType.DIRECT, durable=True)
    work_queue = await channel.declare_queue(
        "critical_work_queue",
        durable=True,
        arguments={
            "x-dead-letter-exchange": "dlx_exchange",
            "x-message-ttl": 3600000  # 1 hour TTL
        }
    )
    await work_queue.bind(work_exchange, routing_key="task_key")
    print("Configured work queue with Dead-Letter protection.")
```
</details>

### Exercise 6 (Hard): Synchronous RPC Client over aio-pika
**Task**: Implement a reusable async function `call_rpc_service(channel, routing_key, payload, timeout=2.0)` that handles ephemeral callback queue creation, correlation ID matching, and timeout cancellation.
<details>
<summary>Solution</summary>

```python
import uuid
import asyncio
from aio_pika import Message, IncomingMessage
from aio_pika.abc import AbstractRobustChannel

async def call_rpc_service(channel: AbstractRobustChannel, routing_key: str, payload: str, timeout: float = 2.0) -> str:
    # Exclusive temporary callback queue
    cb_queue = await channel.declare_queue(exclusive=True, auto_delete=True)
    correlation_id = str(uuid.uuid4())
    future: asyncio.Future[str] = asyncio.get_running_loop().create_future()

    async def on_reply(msg: IncomingMessage):
        if msg.correlation_id == correlation_id:
            future.set_result(msg.body.decode())
            await msg.ack()

    consumer_tag = await cb_queue.consume(on_reply)

    try:
        await channel.default_exchange.publish(
            Message(
                body=payload.encode(),
                correlation_id=correlation_id,
                reply_to=cb_queue.name
            ),
            routing_key=routing_key
        )
        async with asyncio.timeout(timeout):
            return await future
    finally:
        await cb_queue.cancel(consumer_tag)
        await cb_queue.delete()
```
</details>
