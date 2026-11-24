# System Design Blueprint: Real-Time Chat & Instant Messaging Platform (Slack / WhatsApp)

## Metadata

- **Topic**: Massively Scalable Real-Time Messaging & Presence Engine
- **Scale Requirements**: 50 Million Daily Active Users (DAU), 1 Billion messages per day, 5 Million concurrent WebSocket connections, 50,000 peak messages/sec
- **Target SLA**: End-to-end message delivery latency < 100ms globally; zero message loss; accurate online/offline presence
- **Tech Stack**: FastAPI, WebSockets, Redis Pub/Sub & Sorted Sets, Apache Cassandra / ScyllaDB, PostgreSQL (User/Channel metadata)

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **1-on-1 & Group Messaging**: Seamless real-time message exchange across direct messages and group channels.
2. **Online Presence & Typing Indicators**: Ephemeral real-time broadcast of "User is typing..." and "Online / Away" status.
3. **Read Receipts & Delivery Ack**: Sent $\to$ Delivered $\to$ Read checkmarks.
4. **Persistent Message History**: Fast historical message retrieval with cursor-based pagination.

### 1.2 Storage Estimation
- 1 Billion messages / day $\times$ 300 bytes average record size $\approx 300\text{ GB / day}$.
- Annual message storage: $\approx 110\text{ TB / year}$.
- High write ratio favors a wide-column NoSQL store like **Apache Cassandra / ScyllaDB** partitioned by `(channel_id, bucket_month)` with clustering key `message_id DESC`.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |       Web & Mobile Clients (5M Active WS)     |
                 +-----------------------+-----------------------+
                                         |
                                         | WebSocket Bi-Directional TCP
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI WebSocket Connection Gateways (50 Pods)
                 | - Tracks local client socket references       |
                 +-----------------------+-----------------------+
                                         |
                                         | 1. Inbound Message from Client A
                                         v
                         +---------------+---------------+
                         | Redis Pub/Sub & Cluster       |
                         | Channel: "channel:{id}"       |
                         +---------------+---------------+
                                         |
                       +-----------------+-----------------+
                       | Broadcast Event                   |
                       v                                   v
         +-------------+-------------+       +-------------+-------------+
         | FastAPI Gateway Pod 2     |       | Async Storage Ingestion   |
         | Delivers to Client B (WS) |       | Worker (Kafka / Streams)  |
         +---------------------------+       +-------------+-------------+
                                                           |
                                                           v
                                             +-------------+-------------+
                                             | ScyllaDB / Cassandra      |
                                             | Immutable Message History |
                                             +---------------------------+
```

---

## 3. Data Model for High-Scale Messaging (ScyllaDB / Cassandra CQL)

```sql
-- Partitioned by channel_id to keep all messages for a room co-located on disk
-- Clustered by message_id (TimeUUID) descending for instant pagination of recent messages
CREATE TABLE chat_messages (
    channel_id UUID,
    message_id TIMEUUID,
    sender_id UUID,
    content TEXT,
    attachments LIST<TEXT>,
    created_at TIMESTAMP,
    PRIMARY KEY ((channel_id), message_id)
) WITH CLUSTERING ORDER BY (message_id DESC);
```

---

## 4. FastAPI Production Implementation Blueprint

```python
import asyncio
from datetime import datetime, timezone
from fastapi import APIRouter, WebSocket, WebSocketDisconnect, status
import orjson
import redis.asyncio as aioredis

router = APIRouter(tags=["Chat Engine"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class ConnectionManager:
    def __init__(self):
        # channel_id -> set of active WebSockets on this local node
        self.active_rooms: dict[str, set[WebSocket]] = {}

    def connect(self, channel_id: str, ws: WebSocket):
        if channel_id not in self.active_rooms:
            self.active_rooms[channel_id] = set()
        self.active_rooms[channel_id].add(ws)

    def disconnect(self, channel_id: str, ws: WebSocket):
        if channel_id in self.active_rooms:
            self.active_rooms[channel_id].discard(ws)

    async def broadcast_local(self, channel_id: str, payload_str: str):
        if channel_id in self.active_rooms:
            for socket in list(self.active_rooms[channel_id]):
                try:
                    await socket.send_text(payload_str)
                except Exception:
                    self.disconnect(channel_id, socket)


manager = ConnectionManager()


@router.websocket("/ws/channels/{channel_id}")
async def chat_websocket(websocket: WebSocket, channel_id: str, user_id: str):
    await websocket.accept()
    manager.connect(channel_id, websocket)

    # Update online presence in Redis (TTL: 60s)
    await redis_client.set(f"presence:{user_id}", "ONLINE", ex=60)

    try:
        while True:
            raw_text = await websocket.receive_text()
            data = orjson.loads(raw_text)

            # Publish message to Redis Pub/Sub to synchronize all gateway pods
            envelope = {
                "channel_id": channel_id,
                "sender_id": user_id,
                "content": data.get("content"),
                "timestamp": datetime.now(timezone.utc).isoformat(),
            }
            await redis_client.publish(f"chat:channel:{channel_id}", orjson.dumps(envelope).decode())
            # Refresh presence heartbeat
            await redis_client.set(f"presence:{user_id}", "ONLINE", ex=60)
    except WebSocketDisconnect:
        manager.disconnect(channel_id, websocket)
        await redis_client.set(f"presence:{user_id}", "OFFLINE", ex=300)
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Celebrity / Giant Channel Fanout (100k users in 1 room)** | 1 message triggers 100,000 simultaneous WebSocket writes, saturating Redis Pub/Sub and gateway sockets. | Shift giant public channels to **Pull-Based Micro-Polling** or hierarchical broadcast trees rather than full mesh push. |
| **Network Flapping & Reconnect Storms** | Mobile app loses connection in tunnels, reconnects 50 times a minute triggering DB queries. | Implement exponential backoff on client SDK; cache recent 50 messages per room in Redis list (`LRANGE`) to avoid DB hits on reconnect. |
| **Out-of-Order Message Delivery** | Client sees answers before questions due to network races. | Use **Monotonic Server-Generated TimeUUIDs** (ULID / Snowflake) rather than client-reported device timestamps. |
