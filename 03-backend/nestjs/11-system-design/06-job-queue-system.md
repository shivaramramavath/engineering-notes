---
title: "System Design: Distributed Job Queue and Scheduling System"
description: "Designing an enterprise background job processor with BullMQ, Redis Cluster, delayed jobs, and rate limiters"
tags: [nestjs, system-design, job-queue, bullmq, distributed-scheduling]
---

# System Design: Distributed Job Queue and Scheduling System

## Architecture
- **BullMQ on Redis Cluster**: Sharded queues across Redis master nodes.
- **Dead-Letter Queue (DLQ)**: Failed jobs retry 3 times with exponential backoff before being isolated in DLQ for engineer inspection.
