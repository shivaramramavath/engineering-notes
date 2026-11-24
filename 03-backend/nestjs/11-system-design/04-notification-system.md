---
title: "System Design: Distributed Multi-Channel Notification Engine"
description: "Designing a notification engine processing 50M messages daily across Email, SMS, Push, and Webhooks"
tags: [nestjs, system-design, notifications, bullmq, queues]
---

# System Design: Distributed Multi-Channel Notification Engine

## Architecture
- **Ingestion**: NestJS REST API validating notification requests and assigning correlation IDs.
- **Queue Tier**: BullMQ with priority queues (HIGH: OTP/verification; LOW: marketing).
- **Worker Tier**: Dedicated NestJS WorkerHost pods with rate limiting and exponential backoff.
