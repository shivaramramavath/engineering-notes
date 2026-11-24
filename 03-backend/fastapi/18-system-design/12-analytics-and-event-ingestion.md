# System Design Blueprint: Real-Time Analytics & Event Ingestion Pipeline

## Metadata

- **Topic**: High-Throughput Clickstream Ingestion, Columnar OLAP Storage & Real-Time Aggregations
- **Scale Requirements**: 1 Billion events daily, 50,000 peak events/second ingestion, sub-second query latency on 100M-row analytical scans
- **Target SLA**: Zero event loss at ingestion edge, end-to-end data latency < 3 seconds from client click to dashboard query
- **Tech Stack**: FastAPI (Ultra-lightweight edge collector), Apache Kafka, ClickHouse (Columnar OLAP), Grafana / Apache Superset

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Ultra-Low Latency Ingestion**: FastAPI collector acknowledges client tracking beacons in < 5ms (HTTP `202 Accepted` / `204 No Content`).
2. **Batch Buffering**: Events stream through Kafka and are written to ClickHouse in large columnar micro-batches (e.g. 5,000 rows or 2 seconds).
3. **High-Speed Aggregations**: Execute complex slice-and-dice queries (e.g. "Unique visitors by campaign and country over last 30 days") in < 200ms.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |       Client Apps & Web Analytics SDK         |
                 +-----------------------+-----------------------+
                                         |
                                         | POST /api/v1/track (Batched Beacons)
                                         v
                 +-----------------------+-----------------------+
                 | FastAPI Event Ingestion Edge (20 Pods)        |
                 | - Validates JSON envelope with Pydantic       |
                 | - Writes directly to Kafka topic (non-blocking)
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | Apache Kafka Cluster (Partitioned by User/Org)|
                 | Topic: "events.clickstream"                   |
                 +-----------------------+-----------------------+
                                         |
                                         | High-Throughput Batch Ingestion
                                         v
                 +-----------------------+-----------------------+
                 | ClickHouse Vector / Ingestion Consumer Engine |
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | ClickHouse OLAP Columnar Database             |
                 | - ReplacingMergeTree / SummingMergeTree Engine|
                 | - Real-Time Materialized Views                |
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------------------------------+
                 | Executive Dashboards & Analytics REST API     |
                 +-----------------------------------------------+
```

---

## 3. ClickHouse Table Schema (SummingMergeTree)

```sql
CREATE TABLE events_raw (
    event_time DateTime64(3, 'UTC'),
    event_name LowCardinality(String),
    user_id UUID,
    organization_id UUID,
    country LowCardinality(String),
    browser LowCardinality(String),
    duration_ms UInt32
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(event_time)
ORDER BY (organization_id, event_name, event_time);
```

---

## 4. FastAPI Ingestion Blueprint

```python
from datetime import datetime, timezone
from fastapi import APIRouter, BackgroundTasks, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter(prefix="/analytics", tags=["Analytics"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class TrackingEvent(BaseModel):
    event_name: str
    user_id: str
    organization_id: str
    properties: dict


@router.post("/track", status_code=status.HTTP_202_ACCEPTED)
async def ingest_event(event: TrackingEvent):
    # Asynchronously publish to stream/queue without waiting for disk writes
    payload = {
        "event_time": datetime.now(timezone.utc).isoformat(),
        "name": event.event_name,
        "user_id": event.user_id,
        "org_id": event.organization_id,
        "props": str(event.properties),
    }
    await redis_client.xadd("stream:events_raw", payload)
    return {"status": "ACCEPTED"}
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **ClickHouse "Too Many Parts" Error** | Application sends thousands of tiny single-row INSERTs per second, crashing ClickHouse merge threads. | **Strict Batching**: Never write single rows to ClickHouse. Collect rows in Kafka / Vector buffer and insert in batches of $\ge 5,000\text{ rows}$. |
| **Collector Pod CPU Spikes during Traffic Surges** | Ingestion gateway slows down, dropping beacons. | Keep FastAPI ingestion endpoint completely stateless; do not query DB or cache during ingestion; scale edge pods horizontally based on network ingress. |
