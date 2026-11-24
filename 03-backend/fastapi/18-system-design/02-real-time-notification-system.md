# System Design Blueprint: Distributed Real-Time Notification System

## Metadata

- **Topic**: Scalable Multi-Channel Notification Platform (Push, In-App, Email, SMS)
- **Scale Requirements**: 500 Million daily notifications, 50,000 peak event throughput per second, 10 Million persistent WebSocket connections
- **Target SLA**: In-app notifications delivered in < 500ms; critical 2FA SMS/Email in < 5 seconds; 99.99% durability
- **Tech Stack**: FastAPI, WebSockets, Redis Streams, Apache Kafka, PostgreSQL, Celery / ARQ

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Multi-Channel Dispatch**: Deliver messages across Push (APNs, FCM), In-App (WebSockets), SMS (Twilio), and Email (SendGrid / AWS SES).
2. **Prioritization**: Strict priority queues (P0: Security codes / 2FA; P1: Transaction alerts; P2: Marketing newsletters).
3. **User Preferences & Rate Limiting**: Honor opt-outs, quiet hours (DND), and maximum notification frequency per user.
4. **Templating Engine**: Render personalized dynamic templates with localized multi-language support.

### 1.2 Non-Functional Requirements & Capacity
- **Peak Throughput**: $50,000\text{ notifications/sec}$.
- **WebSocket State**: 10M concurrent users $\times$ 5 KB state per TCP connection $\approx 50\text{ GB RAM}$ across 50 API nodes.
- **Message Durability**: Zero data loss for P0/P1 messages; at-least-once delivery with idempotent client handling.

---

## 2. High-Level Architecture Diagram

```
                       +---------------------------------------+
                       | Internal Microservices (Order, Auth)  |
                       +-------------------+-------------------+
                                           |
                                           | POST /api/v1/notifications
                                           v
                       +-------------------+-------------------+
                       |    FastAPI Notification Ingestion     |
                       |       Gateway (Stateless REST)        |
                       +-------------------+-------------------+
                                           |
                                           | Validate User Preferences, DND
                                           | Partition by Priority & User ID
                                           v
                       +-------------------+-------------------+
                       | Apache Kafka / Redis Streams Cluster  |
                       | |-- topic: notif.p0.critical          |
                       | |-- topic: notif.p1.transactional     |
                       | `-- topic: notif.p2.marketing         |
                       +-------------------+-------------------+
                                           |
            +------------------------------+------------------------------+
            |                              |                              |
            v                              v                              v
+-----------+-----------+      +-----------+-----------+      +-----------+-----------+
| In-App WS Consumer    |      | Push / Mobile Consumer|      | Email / SMS Consumer  |
| Pods (FastAPI WS)     |      | (FCM / APNs Dispatch) |      | (Twilio / SendGrid)   |
+-----------+-----------+      +-----------+-----------+      +-----------+-----------+
            |                              |                              |
            v                              v                              v
     Active Browser                 Apple / Google               Carrier Networks
   WebSocket Clients                 Push Services                   (SMS Gateway)
```

---

## 3. Data Model & User Preferences

```sql
CREATE TABLE notification_preferences (
    user_id UUID PRIMARY KEY,
    email_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    sms_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    push_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    dnd_start_utc TIME NULL,
    dnd_end_utc TIME NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE notification_audit_log (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    channel VARCHAR(16) NOT NULL,
    priority VARCHAR(8) NOT NULL,
    status VARCHAR(16) NOT NULL, -- QUEUED, SENT, FAILED, SUPPRESSED
    idempotency_key VARCHAR(128) UNIQUE NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

---

## 4. FastAPI Production Implementation Blueprint

```python
import enum
from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field
import redis.asyncio as aioredis

router = APIRouter(prefix="/notifications", tags=["Notifications"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class Priority(str, enum.Enum):
    P0 = "P0_CRITICAL"
    P1 = "P1_TRANSACTIONAL"
    P2 = "P2_MARKETING"


class NotificationRequest(BaseModel):
    user_id: str
    channel: str = Field(..., pattern="^(EMAIL|SMS|PUSH|IN_APP)$")
    priority: Priority
    template_id: str
    context: dict
    idempotency_key: str


@router.post("/send", status_code=status.HTTP_202_ACCEPTED)
async def enqueue_notification(req: NotificationRequest):
    # 1. Idempotency Check in Redis (24-hour cache window)
    idem_key = f"notif:idem:{req.idempotency_key}"
    is_new = await redis_client.set(idem_key, "1", nx=True, ex=86400)
    if not is_new:
        return {"status": "ALREADY_ENQUEUED", "idempotency_key": req.idempotency_key}

    # 2. Enqueue into Priority Redis Stream
    stream_name = f"stream:notif:{req.priority.value.lower()}"
    await redis_client.xadd(
        stream_name,
        {
            "user_id": req.user_id,
            "channel": req.channel,
            "template_id": req.template_id,
            "context_json": str(req.context),
        },
    )

    return {"status": "QUEUED", "priority": req.priority}
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Downstream Third-Party Outage (Twilio / SendGrid down)** | Notification queues back up, risk of memory exhaustion. | Circuit breaker with automated failover to fallback vendor (e.g. Twilio $\to$ MessageBird; SendGrid $\to$ AWS SES). |
| **Notification Thunder Herd (Breaking News push to 10M users)** | Sudden ingestion spike saturates backend workers. | Rate-limit bulk broadcasts via leaky bucket; prioritize P0/P1 over bulk marketing fanouts. |
| **Duplicate Delivery (User receives 5 identical OTP codes)** | User confusion, compliance violations. | Enforce end-to-end `idempotency_key` check in Redis with atomic `SET NX EX`. |
| **Disconnected WebSocket Drops** | User misses critical in-app notification while offline. | Buffer unacknowledged in-app notifications in Redis sorted set (`ZADD notif:{user_id} {ts} {json}`); sync on reconnect. |
