# System Design Blueprint: IoT Telemetry Ingestion & Real-Time Time-Series Pipeline

## Metadata

- **Topic**: Massive IoT Sensor Telemetry Ingestion, Time-Series Storage & Real-Time Threshold Alerting
- **Scale Requirements**: 1 Million active connected IoT devices, 100,000 telemetry readings per second, 8.6 Billion daily metrics
- **Target SLA**: Ingestion latency < 5ms per reading, anomaly threshold alert dispatched in < 2 seconds, 99.99% ingestion uptime
- **Tech Stack**: FastAPI, EMQX / Mosquitto (MQTT Broker), TimescaleDB / InfluxDB, Apache Kafka, Redis (Rolling Window Anomaly Detection)

---

## 1. Requirements & System Estimation

### 1.1 Functional Requirements
1. **Multi-Protocol Ingestion**: Ingest telemetry from resource-constrained devices via MQTT and cellular gateways via HTTP/HTTPS POST.
2. **Time-Series Compression**: Store continuous temperature, vibration, pressure, and voltage readings with high compression (> 90%).
3. **Automated Downsampling**: Retain raw 1-second telemetry for 7 days; downsample to 1-minute aggregates for 90 days, and 1-hour averages for 2 years.
4. **Real-Time Anomaly Detection**: Trigger instant PagerDuty/SMS alerts when sensor metrics exceed critical safety thresholds.

---

## 2. High-Level Architecture Diagram

```
                 +-----------------------------------------------+
                 |   1,000,000 IoT Sensors / Industrial Devices  |
                 +-----------------------+-----------------------+
                                         |
                                         | MQTT / HTTPS Telemetry
                                         v
                 +-----------------------+-----------------------+
                 | EMQX MQTT Broker / FastAPI HTTP Telemetry Edge|
                 +-----------------------+-----------------------+
                                         |
                                         v
                 +-----------------------+-----------------------+
                 | Apache Kafka Telemetry Cluster                |
                 | Topic: "iot.telemetry.raw"                    |
                 +-----------------------+-----------------------+
                                         |
                       +-----------------+-----------------+
                       |                                   |
             [Timeseries Persistence]             [Real-Time Anomaly Check]
                       |                                   |
                       v                                   v
         +-------------+-------------+       +-------------+-------------+
         | TimescaleDB (PostgreSQL)  |       | Flink / Python Stream     |
         | - Hypertable Partitioning |       | Anomaly Detector          |
         | - Continuous Aggregates   |       | (Z-Score / Thresholds)    |
         +---------------------------+       +-------------+-------------+
                       |                                   |
                       v                                   v
         +-------------+-------------+       +-------------+-------------+
         | Automated Downsampling    |       | Real-Time Critical Alert  |
         | Policy (7d -> 90d -> 2yr) |       | (PagerDuty / Opsgenie)    |
         +---------------------------+       +---------------------------+
```

---

## 3. TimescaleDB Hypertable & Downsampling Policy

```sql
-- Create base hypertable partitioned by timestamp
CREATE TABLE sensor_telemetry (
    time TIMESTAMPTZ NOT NULL,
    device_id VARCHAR(64) NOT NULL,
    metric_name VARCHAR(32) NOT NULL,
    value DOUBLE PRECISION NOT NULL
);

SELECT create_hypertable('sensor_telemetry', 'time');

-- Enable native chunk compression
ALTER TABLE sensor_telemetry SET (
    timescaledb.compress,
    timescaledb.compress_segmentby = 'device_id, metric_name',
    timescaledb.compress_orderby = 'time DESC'
);

-- Automatically compress chunks older than 2 days
SELECT add_compression_policy('sensor_telemetry', INTERVAL '2 days');

-- Automatically drop raw chunks older than 90 days
SELECT add_retention_policy('sensor_telemetry', INTERVAL '90 days');
```

---

## 4. FastAPI Telemetry Ingest Blueprint

```python
from datetime import datetime, timezone
from fastapi import APIRouter, status
from pydantic import BaseModel
import redis.asyncio as aioredis

router = APIRouter(prefix="/iot", tags=["IoT Telemetry"])
redis_client = aioredis.from_url("redis://localhost:6379/0", decode_responses=True)


class TelemetryBatch(BaseModel):
    device_id: str
    readings: list[dict[str, float]]


@router.post("/telemetry", status_code=status.HTTP_202_ACCEPTED)
async def ingest_telemetry(batch: TelemetryBatch):
    now_iso = datetime.now(timezone.utc).isoformat()
    # Asynchronously pipe into stream buffer for batch insertion
    await redis_client.xadd(
        "stream:iot:raw",
        {"device_id": batch.device_id, "timestamp": now_iso, "count": len(batch.readings)},
    )
    return {"status": "BUFFERED"}
```

---

## 5. Failure Modes, Scalability & Bottlenecks

| Failure Scenario | Impact | Mitigation Strategy |
|:---|:---|:---|
| **Out-of-Order Sensor Data** | Offline sensors re-establish connection and upload 48 hours of buffered history simultaneously. | TimescaleDB hypertables handle late-arriving data natively; ingest workers write directly to corresponding historical partition chunks without locking active chunks. |
| **Ingestion Traffic Spike during Network Outage Recovery** | Thousands of field gateways reconnect at once, hitting API with 500,000 QPS. | Distribute sensor uploads using client-side random jitter (0 to 300 seconds); scale Kafka partitions to 64+ to absorb ingress bursts. |
