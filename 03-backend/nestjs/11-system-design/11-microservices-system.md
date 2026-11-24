---
title: "System Design: Polyglot Enterprise Microservices System"
description: "Architecting microservices with API Gateway, RabbitMQ asynchronous events, and gRPC internal communication"
tags: [nestjs, system-design, microservices, grpc, rabbitmq]
---

# System Design: Polyglot Enterprise Microservices System

## Architecture
- **External Traffic**: NestJS API Gateway terminating SSL and authenticating JWTs.
- **Internal RPC**: gRPC with HTTP/2 and Protobuf for synchronous service-to-service calls.
- **Asynchronous Events**: RabbitMQ topic exchanges for decoupled domain events.
