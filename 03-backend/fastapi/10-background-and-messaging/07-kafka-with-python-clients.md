---
title: "Apache Kafka Integration with FastAPI via aiokafka"
category: "10-background-and-messaging"
order: 7
tags: ["fastapi", "kafka", "aiokafka", "event-streaming", "event-driven", "distributed-systems"]
py_version: "3.12+"
fastapi_version: "0.115+"
starlette_version: "0.38+"
pydantic_version: "2.8+"
tested_with: "Python 3.12, 3.13"
---

# Apache Kafka Integration with FastAPI via aiokafka

## 1. Conceptual Foundation & Mental Model

When applications scale from thousands of background jobs per hour to hundreds of thousands of events per second, traditional task queues (Celery, RQ) experience architectural strain. Task queues delete messages immediately after consumption and ack. They lack partitioned ordering, replayability, and consumer group fan-out.

**Apache Kafka** is a distributed, append-only, horizontally partitioned commit log. It does not treat messages as transient jobs to be deleted; it treats them as an immutable stream of real-time events stored durably on disk for days or months.

Integrating Kafka into FastAPI requires asynchronous, non-blocking producer and consumer drivers. The industry standard library is **`aiokafka`** (an `asyncio` port of `kafka-python`), which maintains persistent TCP connection pools to Kafka brokers and integrates directly into FastAPI's `lifespan` event loop.

### The Mental Model: The Bulletin Board vs The High-Speed Ledger
- **RabbitMQ / Celery** is a post office box. When you pick up your mail, it is removed from the box. Once read, it is gone.
- **Apache Kafka** is an immutable financial ledger written in permanent ink in an open library hall. 
  - FastAPI writes new transaction entries at the bottom of the ledger (**Producer**).
  - Ten different departments (Analytics, Fraud Detection, Email Notifications, Inventory) each have their own bookmarks (**Consumer Group Offsets**).
  - Each department reads through the ledger at their own speed. If the Fraud Detection team discovers a bug in their logic, they can rewind their bookmark 3 days into the past and re-read all historical transactions without affecting any other department.

---

## 2. Architecture & Internal Mechanics

```
   FastAPI Web Cluster (Producers)
   +-------------------------------------------------------------+
   |   Lifespan creates single shared `AIOKafkaProducer`         |
   |                                                             |
   |   POST /events/click -> `await producer.send_and_wait(...)` |
   +-------------------------------------------------------------+
                                  |
                   High-Speed TCP Batching Protocol
                                  |
                                  v
   Apache Kafka Cluster (Topic: `user_events`)
   +-------------------------------------------------------------+
   |  Partition 0: [Offset 0] [Offset 1] [Offset 2] ...          |
   |  Partition 1: [Offset 0] [Offset 1] [Offset 2] ...          |
   |  Partition 2: [Offset 0] [Offset 1] [Offset 2] ...          |
   +-------------------------------------------------------------+
                                  |
           +----------------------+----------------------+
           |                                             |
           v                                             v
   Consumer Group A (Analytics)                  Consumer Group B (Fraud)
   [ Bookmark at Offset 1042 ]                   [ Bookmark at Offset 891 ]
   (Reads independently)                         (Rewinds independently)
```

### Key Kafka Invariants
1. **Partition Key Ordering**: Kafka guarantees strict message ordering **only within the same partition**. By providing a partition key (e.g., `key=user_id.encode()`), all events for that specific user are guaranteed to arrive at the exact same partition in chronological order.
2. **Batching & Compression**: `AIOKafkaProducer` automatically buffers messages into batches (using Snappy, Gzip, or Zstd compression) before flushing across the network, achieving millions of events per second throughput.

---

## 3. Comprehensive Syntax & Core Mechanics

### 1. `AIOKafkaProducer` Lifespan Pattern
```python
from aiokafka import AIOKafkaProducer
from contextlib import asynccontextmanager
from typing import AsyncIterator
import json
from fastapi import FastAPI

KAFKA_BOOTSTRAP_SERVERS = "localhost:9092"

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    # 1. Initialize producer with JSON serializer
    producer = AIOKafkaProducer(
        bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
        value_serializer=lambda v: json.dumps(v).encode("utf-8"),
        key_serializer=lambda k: k.encode("utf-8") if k else None,
        acks="all",             # Strongest durability: all in-sync replicas must ack
        enable_idempotence=True # Prevents duplicate messages on network retries
    )
    await producer.start()
    app.state.kafka_producer = producer
    print("Kafka producer started.")
    
    yield
    
    # 2. Flush remaining buffered messages and close sockets
    await producer.stop()
    print("Kafka producer safely stopped.")

app = FastAPI(lifespan=lifespan)
```

### 2. Publishing an Event from a Route
```python
from fastapi import FastAPI, Depends, Request
from pydantic import BaseModel
from aiokafka import AIOKafkaProducer

class OrderEvent(BaseModel):
    order_id: str
    user_id: str
    total_cents: int

def get_kafka_producer(request: Request) -> AIOKafkaProducer:
    return request.app.state.kafka_producer

@app.post("/orders/events")
async def publish_order_event(
    event: OrderEvent,
    producer: AIOKafkaProducer = Depends(get_kafka_producer)
):
    # Key guarantees all events for this user go to the exact same partition
    metadata = await producer.send_and_wait(
        topic="orders_v1",
        key=event.user_id,
        value=event.model_dump()
    )
    return {
        "status": "PUBLISHED",
        "topic": metadata.topic,
        "partition": metadata.partition,
        "offset": metadata.offset
    }
```

---

## 4. Practical Implementation

### Example 1: Basic High-Throughput Fire-and-Forget Producer
```python
from fastapi import FastAPI, Depends, Request
from pydantic import BaseModel
from aiokafka import AIOKafkaProducer

app = FastAPI()

class TelemetryRecord(BaseModel):
    device_id: str
    temperature: float
    battery_level: float

@app.post("/telemetry")
async def collect_telemetry(payload: TelemetryRecord, request: Request):
    producer: AIOKafkaProducer = request.app.state.kafka_producer
    # .send() buffers message in producer memory; does NOT wait for broker ack!
    # Extreme throughput (100k+ req/sec)
    future = await producer.send(
        "iot_telemetry",
        key=payload.device_id.encode(),
        value=payload.model_dump_json().encode()
    )
    return {"status": "BUFFERED"}
```

### Example 2: Production Background Kafka Consumer Loop
Running an embedded consumer inside FastAPI to process events asynchronously:
```python
import asyncio
from contextlib import asynccontextmanager
from aiokafka import AIOKafkaConsumer
import json
from fastapi import FastAPI

KAFKA_BOOTSTRAP_SERVERS = "localhost:9092"

async def kafka_consumer_loop(stop_event: asyncio.Event):
    consumer = AIOKafkaConsumer(
        "user_registrations",
        bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
        group_id="notification_service_group",
        auto_offset_reset="earliest",
        enable_auto_commit=False,  # Manual commit for at-least-once reliability
        value_deserializer=lambda v: json.loads(v.decode("utf-8"))
    )
    await consumer.start()
    print("Kafka Consumer started. Listening for registration events...")
    
    try:
        while not stop_event.is_set():
            # Fetch batch of messages with timeout
            batch = await consumer.getmany(timeout_ms=1000, max_records=50)
            for tp, messages in batch.items():
                for msg in messages:
                    print(f"Consumed user registration: {msg.value} from partition {tp.partition}")
                # Commit offset after successful batch processing
                await consumer.commit({tp: messages[-1].offset + 1})
    except asyncio.CancelledError:
        pass
    finally:
        await consumer.stop()
        print("Kafka Consumer safely stopped.")

@asynccontextmanager
async def lifespan(app: FastAPI):
    stop_event = asyncio.Event()
    consumer_task = asyncio.create_task(kafka_consumer_loop(stop_event))
    yield
    stop_event.set()
    await consumer_task

app = FastAPI(lifespan=lifespan)
```

### Example 3: Edge Case — Handling Kafka Broker Outages Gracefully
```python
from aiokafka.errors import KafkaTimeoutError, NodeNotReadyError
from fastapi import FastAPI, HTTPException, status, Request
from pydantic import BaseModel

app = FastAPI()

class ClickEvent(BaseModel):
    user_id: str
    target_url: str

@app.post("/clicks")
async def record_click(event: ClickEvent, request: Request):
    producer = request.app.state.kafka_producer
    try:
        # Strict timeout on broker acknowledgment
        async with asyncio.timeout(2.0):
            record_meta = await producer.send_and_wait(
                "ui_clicks",
                key=event.user_id.encode(),
                value=event.model_dump_json().encode()
            )
            return {"offset": record_meta.offset}
    except (KafkaTimeoutError, NodeNotReadyError, TimeoutError):
        # Fall back or return HTTP 503 instead of hanging client connection
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE,
            detail="Streaming ingestion layer temporarily degraded. Please retry."
        )
```

### Example 4: Failure Case — Creating Fresh `AIOKafkaProducer` on Every Request
```python
from fastapi import FastAPI
from aiokafka import AIOKafkaProducer

app = FastAPI()

# CATASTROPHIC ANTI-PATTERN
@app.post("/bad-produce")
async def bad_produce(data: dict):
    # Instantiating producer per request destroys performance!
    # Connects to Zookeeper/KRaft, initiates metadata discovery, opens 3 TCP connections...
    p = AIOKafkaProducer(bootstrap_servers="localhost:9092")
    await p.start()
    try:
        await p.send_and_wait("topic", b"data")
    finally:
        await p.stop()
```

### Example 5: Idiomatic Modern — Avro / Schema Registry Integration
```python
from pydantic import BaseModel
from fastapi import FastAPI

# In enterprise architectures, events conform to strict schemas (JSON Schema, Protobuf, Avro)
class UserUpdatedEvent(BaseModel):
    schema_version: int = 1
    user_id: str
    updated_fields: list[str]
    timestamp_epoch: int

@app.post("/users/{user_id}/audit")
async def audit_user_update(user_id: str, fields: list[str], request: Request):
    producer = request.app.state.kafka_producer
    event = UserUpdatedEvent(
        user_id=user_id,
        updated_fields=fields,
        timestamp_epoch=int(asyncio.get_running_loop().time())
    )
    
    await producer.send_and_wait("user_audit_stream", key=user_id.encode(), value=event.model_dump_json().encode())
    return {"status": "AUDITED"}
```

---

## 5. Edge Cases, Pitfalls & Failure Modes

1. **Rebalance Storms in Consumers**: If an embedded consumer task takes too long to process a batch of messages, Kafka's coordinator assumes the consumer died and triggers a consumer group **rebalance**. During rebalancing, all consumers stop processing! Increase `max_poll_interval_ms` (e.g. to 300,000ms / 5 minutes) for slow tasks.
2. **Partition Hotspots**: If you use a partition key with low cardinality (e.g. `gender` or `country_code="US"`), 90% of all events land on a single partition, overloading one consumer while other partition consumers sit idle. Use high-cardinality keys like `user_id` or `uuid`.
3. **Buffer Memory Overflow**: In high-throughput fire-and-forget mode (`producer.send()`), if Kafka brokers slow down, messages accumulate in the producer's internal buffer. If `buffer_memory` (default: 32MB) is exceeded, `producer.send()` blocks or raises `BufferError`.
4. **Uncommitted Offset Data Loss**: Enabling `enable_auto_commit=True` commits offsets periodically in the background. If the container crashes midway through processing a batch, the offset was already committed, causing permanent data loss! Always use manual commits (`enable_auto_commit=False`).

---

## 6. Common Mistakes & Anti-Patterns

| Anti-Pattern | Root Cause | Consequence | Correct Practice |
|:---|:---|:---|:---|
| Creating `AIOKafkaProducer` inside route handler | Lack of lifecycle understanding | 100x latency spike, socket exhaustion | Initialize single shared producer in `lifespan` |
| Using `enable_auto_commit=True` for critical jobs | Convenience over reliability | Silent data loss on worker crash | Use manual offset commits after processing |
| Not specifying partition keys for ordered streams | Missing domain partitioning | Events for the same user arrive out of order | Provide entity ID (e.g. `user_id`) as partition key |
| Setting `acks=0` for financial transactions | Sacrificing durability for throughput | Messages silently lost on broker restart | Set `acks="all"` and `enable_idempotence=True` |
| Long blocking operations in consumer loop | Processing heavy sync logic in loop | Consumer rebalance storms and duplicate processing | Offload processing to worker pool; keep poll loop fast |
| Forgetting `await producer.stop()` on shutdown | Abrupt termination during rollout | Buffered in-flight messages lost | Flush and stop producer in `lifespan` teardown |

---

## 7. Performance & Scalability Considerations

- **Producer Compression**: Enabling `compression_type="zstd"` or `"snappy"` on the producer reduces network bandwidth consumption by 60%–80% with minimal CPU overhead.
- **Consumer Parallelism**: A Kafka consumer group can only have as many active consumers as there are partitions in the topic. If a topic has 12 partitions, running 20 consumer replicas means 8 replicas will sit completely idle. Always align worker counts to partition counts.

---

## 8. Security & OWASP Guardrails

- **mTLS and SASL/SCRAM Authentication**: Never run Kafka in production with plaintext listeners. Enforce TLS encryption in transit and authenticate clients using SASL/SCRAM-SHA-512 or mutual TLS certificates.
- **Topic ACLs**: Enforce strict Kafka Access Control Lists (ACLs) so that the FastAPI producer service only possesses write permissions to specific topics, and consumer services have read-only permissions.

---

## 9. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import AsyncClient, ASGITransport
from fastapi import FastAPI
from unittest.mock import AsyncMock, MagicMock

test_app = FastAPI()

@test_app.post("/events")
async def post_event():
    # Production route logic
    return {"status": "sent"}

@pytest.mark.anyio
async def test_kafka_route_with_mocked_producer():
    # Mock AIOKafkaProducer to test endpoints in CI without real Kafka cluster
    mock_producer = AsyncMock()
    mock_meta = MagicMock()
    mock_meta.topic = "test_topic"
    mock_meta.partition = 0
    mock_meta.offset = 42
    mock_producer.send_and_wait.return_value = mock_meta
    
    test_app.state.kafka_producer = mock_producer
    
    transport = ASGITransport(app=test_app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.post("/events")
        assert resp.status_code == 200
        assert resp.json() == {"status": "sent"}
```

---

## 10. Real-World Case Study / Production Incident

**The Incident**: A ride-hailing company experienced out-of-order GPS tracking data in their driver routing system. Drivers who turned left were displayed on passenger maps as turning right, or jumping backwards in time 50 meters.

**Root Cause**: The FastAPI ingestion service published driver telemetry events without a partition key:
```python
await producer.send("driver_telemetry", value=gps_payload)  # NO KEY PROVIDED!
```
When no key is provided, Kafka distributes messages in round-robin fashion across all 64 partitions. As a result, Ping 1 landed on Partition 2, and Ping 2 landed on Partition 5. The downstream mapping service consumed Partition 5 faster than Partition 2, processing Ping 2 *before* Ping 1, causing GPS coordinate time reversal.

**The Fix**:
Updated the producer to use the `driver_id` as the partition key:
```python
await producer.send("driver_telemetry", key=driver_id.encode(), value=gps_payload)
```
Kafka's hashing algorithm guaranteed all pings for the same driver always landed on the exact same partition, restoring strict chronological ordering.

---

## 11. Technical Interview Questions & Answers

<details>
<summary>1. How does Apache Kafka achieve strict message ordering, and what are its limitations?</summary>
Kafka guarantees strict total order <b>only within a single partition</b>. Messages across different partitions are not guaranteed to be ordered. To ensure events for a specific domain entity (e.g., a specific user or order) are processed strictly in chronological order, the producer must supply that entity's identifier as the message's <b>partition key</b>.
</details>

<details>
<summary>2. Why should an <code>AIOKafkaProducer</code> be initialized once in FastAPI's <code>lifespan</code> rather than per request?</summary>
Initializing an <code>AIOKafkaProducer</code> involves establishing TCP handshakes with multiple broker nodes, downloading cluster metadata, initiating heartbeat timers, and allocating internal compression buffers. Instantiating it per request adds 50ms–200ms of latency per call, exhausts system sockets, and destroys performance.
</details>

<details>
<summary>3. What is the difference between <code>producer.send()</code> and <code>producer.send_and_wait()</code> in aiokafka?</summary>
<code>producer.send()</code> is asynchronous fire-and-forget: it appends the message to the producer's internal memory buffer and returns immediately, achieving maximum throughput. <code>producer.send_and_wait()</code> suspends execution until the broker network acknowledgment (ACK) is received, guaranteeing persistence before proceeding.
</details>

<details>
<summary>4. What does setting <code>acks="all"</code> do on a Kafka producer?</summary>
With <code>acks="all"</code> (or <code>acks=-1</code>), the broker leader will not acknowledge the message until it has been successfully replicated across all in-sync replicas (ISRs). This provides the highest durability guarantee against data loss during broker crashes.
</details>

<details>
<summary>5. What is a "Consumer Rebalance" in Kafka and what triggers it?</summary>
A rebalance occurs when the Kafka consumer group coordinator redistributes partition assignments among consumers (e.g. when a consumer joins, crashes, or is killed). Rebalances can also be triggered if a consumer takes longer than <code>max_poll_interval_ms</code> to finish processing a batch of messages.
</details>

<details>
<summary>6. How does Kafka's consumer group model allow scaling consumers?</summary>
Each partition in a topic is consumed by exactly one consumer within a given consumer group. By adding more consumer instances up to the number of topic partitions, processing capacity scales horizontally. If consumer instances exceed partition count, the excess consumers sit idle as hot standbys.
</details>

<details>
<summary>7. What is the danger of setting <code>enable_auto_commit=True</code> on a Kafka consumer?</summary>
Auto-commit commits offsets periodically based on a timer, independent of whether the application has finished processing the data. If the worker crashes while processing a batch of messages, the offsets were already committed, causing those messages to be permanently skipped on restart (data loss).
</details>

<details>
<summary>8. How does Kafka compaction differ from retention by time?</summary>
Standard retention deletes messages older than a specified duration (e.g., 7 days). <b>Log Compaction</b> retains the most recent record for every key indefinitely, deleting older versions of that key. It allows Kafka to act as a changelog or distributed key-value database.
</details>

---

## 12. Practical Exercises & Solutions

### Exercise 1 (Easy): Basic Kafka Publisher Route
**Task**: Build a route `POST /logs/event` that publishes a JSON string to a Kafka topic `app_logs` using a shared `AIOKafkaProducer` on `app.state`.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request, status
from pydantic import BaseModel
from aiokafka import AIOKafkaProducer

app = FastAPI()

class LogEvent(BaseModel):
    level: str
    message: str

@app.post("/logs/event", status_code=status.HTTP_202_ACCEPTED)
async def log_event(event: LogEvent, request: Request):
    producer: AIOKafkaProducer = request.app.state.kafka_producer
    await producer.send_and_wait("app_logs", value=event.model_dump_json().encode())
    return {"status": "PUBLISHED"}
```
</details>

### Exercise 2 (Easy): Partitioned User Event Dispatcher
**Task**: Create an endpoint that receives a `user_id` and an action name, ensuring that the event is dispatched to Kafka with `key=user_id.encode()` to enforce partition ordering.
<details>
<summary>Solution</summary>

```python
from fastapi import FastAPI, Request
from pydantic import BaseModel
from aiokafka import AIOKafkaProducer

app = FastAPI()

class UserAction(BaseModel):
    user_id: str
    action: str

@app.post("/user-actions")
async def track_user_action(payload: UserAction, request: Request):
    producer: AIOKafkaProducer = request.app.state.kafka_producer
    meta = await producer.send_and_wait(
        "user_actions_stream",
        key=payload.user_id.encode(),
        value=payload.model_dump_json().encode()
    )
    return {"partition": meta.partition, "offset": meta.offset}
```
</details>

### Exercise 3 (Medium): Kafka Producer Lifespan Manager with Retries
**Task**: Implement a production-grade FastAPI `lifespan` function that initializes `AIOKafkaProducer` with `enable_idempotence=True` and `acks="all"`, handling connection retries during broker startup.
<details>
<summary>Solution</summary>

```python
import asyncio
from contextlib import asynccontextmanager
from typing import AsyncIterator
from aiokafka import AIOKafkaProducer
from fastapi import FastAPI

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    producer = AIOKafkaProducer(
        bootstrap_servers="localhost:9092",
        acks="all",
        enable_idempotence=True,
        compression_type="zstd"
    )
    
    # Retry connecting up to 5 times
    connected = False
    for attempt in range(5):
        try:
            await producer.start()
            connected = True
            print("Kafka Producer connected successfully.")
            break
        except Exception as exc:
            print(f"Connecting to Kafka failed (attempt {attempt+1}): {exc}")
            await asyncio.sleep(2.0)
            
    if not connected:
        raise SystemExit("Fatal: Unable to connect to Kafka cluster.")
        
    app.state.kafka_producer = producer
    yield
    await producer.stop()
    print("Kafka Producer flushed and stopped.")

app = FastAPI(lifespan=lifespan)
```
</details>

### Exercise 4 (Medium): Bounded Kafka Batch Consumer Task
**Task**: Build an async background consumer loop that polls up to 100 messages at a time using `consumer.getmany()`, processes them, and manually commits the partition offsets.
<details>
<summary>Solution</summary>

```python
import asyncio
from aiokafka import AIOKafkaConsumer

async def batch_consumer_worker(stop_event: asyncio.Event):
    consumer = AIOKafkaConsumer(
        "orders_topic",
        bootstrap_servers="localhost:9092",
        group_id="order_fulfillment_group",
        enable_auto_commit=False,
        max_poll_records=100
    )
    await consumer.start()
    try:
        while not stop_event.is_set():
            batch = await consumer.getmany(timeout_ms=1000, max_records=100)
            if not batch:
                continue
            
            commit_offsets = {}
            for tp, records in batch.items():
                for record in records:
                    print(f"Processed order: {record.value.decode()}")
                # Commit next expected offset
                commit_offsets[tp] = records[-1].offset + 1
                
            await consumer.commit(commit_offsets)
    finally:
        await consumer.stop()
```
</details>

### Exercise 5 (Hard): High-Throughput Buffering Producer with Flush Hook
**Task**: Implement a high-speed telemetry intake endpoint using `producer.send()` (fire-and-forget), and build a shutdown hook that calls `await producer.flush()` to ensure zero buffered message loss on graceful deployment.
<details>
<summary>Solution</summary>

```python
from contextlib import asynccontextmanager
from aiokafka import AIOKafkaProducer
from fastapi import FastAPI, Request, status
from pydantic import BaseModel

class MetricPoint(BaseModel):
    metric: str
    value: float

@asynccontextmanager
async def lifespan(app: FastAPI):
    producer = AIOKafkaProducer(
        bootstrap_servers="localhost:9092",
        linger_ms=20,  # Buffer for up to 20ms to assemble large batches
        batch_size=65536  # 64KB batches
    )
    await producer.start()
    app.state.producer = producer
    yield
    # Flush all buffered in-flight messages before terminating process
    await producer.flush()
    await producer.stop()

app = FastAPI(lifespan=lifespan)

@app.post("/metrics", status_code=status.HTTP_202_ACCEPTED)
async def ingest_metric(data: MetricPoint, request: Request):
    p: AIOKafkaProducer = request.app.state.producer
    # Non-blocking buffer append
    await p.send("system_metrics", value=data.model_dump_json().encode())
    return {"status": "BUFFERED"}
```
</details>

### Exercise 6 (Hard): DLQ Fallback on Consumer Deserialization Errors
**Task**: Implement a Kafka consumer that reads messages from `incoming_events`. If a message payload contains corrupted JSON that cannot be parsed, forward the raw bytes to an `incoming_events_dlq` topic and commit the offset to prevent infinite poison pill loops.
<details>
<summary>Solution</summary>

```python
import json
from aiokafka import AIOKafkaConsumer, AIOKafkaProducer

async def resilient_event_consumer():
    consumer = AIOKafkaConsumer(
        "incoming_events",
        bootstrap_servers="localhost:9092",
        group_id="parsing_workers",
        enable_auto_commit=False
    )
    producer = AIOKafkaProducer(bootstrap_servers="localhost:9092")
    
    await consumer.start()
    await producer.start()
    
    try:
        async for msg in consumer:
            try:
                data = json.loads(msg.value.decode("utf-8"))
                print(f"Successfully processed event: {data}")
            except Exception as exc:
                print(f"Malformed event at offset {msg.offset}! Routing to DLQ: {exc}")
                # Forward poisoned message to Dead-Letter Topic
                await producer.send_and_wait(
                    "incoming_events_dlq",
                    key=msg.key,
                    value=msg.value,
                    headers=[("dlq_error", str(exc).encode())]
                )
            finally:
                # Always commit offset to keep consumer moving forward
                await consumer.commit()
    finally:
        await consumer.stop()
        await producer.stop()
```
</details>
