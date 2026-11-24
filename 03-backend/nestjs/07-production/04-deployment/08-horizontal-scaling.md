---
title: "Horizontal Scaling and Stateless Architecture in NestJS"
description: "Designing purely stateless NestJS applications for horizontal scaling: shared Redis state, distributed sessions, and object storage"
tags: [nestjs, deployment, horizontal-scaling, stateless, redis, distributed-systems]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Horizontal Scaling and Stateless Architecture in NestJS

## Conceptual Overview
To scale horizontally across dozens of instances, NestJS applications must remain **strictly stateless**:
- No in-memory user sessions (use JWTs or centralized Redis session stores).
- No local filesystem storage for user uploads (use Amazon S3, Google Cloud Storage, or MinIO).
- No local in-memory pub/sub for WebSockets (use `@socket.io/redis-adapter`).
- No local scheduled cron tasks running on all pods simultaneously (use Redis-backed Redlock or BullMQ).

---

## Key Takeaways & Cheat Sheet

- Keep application nodes 100% stateless.
- Store shared state in Redis or relational databases.
- Offload file storage to cloud object storage.

---

## Cross-References
- [05-scaling-websockets.md](../../05-advanced/04-realtime/05-scaling-websockets.md) - WebSocket scaling
- [07-kubernetes.md](./07-kubernetes.md) - Kubernetes orchestration
