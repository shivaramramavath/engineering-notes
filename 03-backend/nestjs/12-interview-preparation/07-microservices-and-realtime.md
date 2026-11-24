---
title: "NestJS Interview: Microservices and Realtime"
description: "Transporters, message patterns, event patterns, Socket.IO gateways, and gRPC in NestJS"
tags: [nestjs, interview, microservices, websockets, grpc, transporters]
---

# NestJS Interview: Microservices and Realtime

### Q1: What is the difference between `@MessagePattern()` and `@EventPattern()` in NestJS microservices?
**Answer**:
- `@MessagePattern()`: Implements **Request-Response** messaging. The client expects an acknowledgment and return payload.
- `@EventPattern()`: Implements **Event-Driven / Fire-and-Forget** messaging. No response is sent back to the publisher.
