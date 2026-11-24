---
title: "System Design: Realtime Telemetry and Analytics Ingestion"
description: "Processing 100,000 events/second: Apache Kafka, NestJS microservices, ClickHouse OLAP, and Grafana dashboards"
tags: [nestjs, system-design, analytics, kafka, clickhouse, olap]
---

# System Design: Realtime Telemetry and Analytics Ingestion

## Architecture
- **Ingestion Transporter**: NestJS microservice consuming Kafka event partitions.
- **Micro-Batching**: Accumulating events in memory buffers for batch insertion into ClickHouse columnar storage.
