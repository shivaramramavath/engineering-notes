# 05-06 — Microservices and Distributed Architecture

Microservices architecture breaks down monolithic codebases into autonomous, independently deployable services that communicate across networks using specialized transport protocols.

This module provides the complete architectural guide to building distributed systems in NestJS: from core tenets and transport selection to message patterns, hybrid API gateways, and enterprise **gRPC** services with Protocol Buffers, bidirectional streaming, and metadata security.

---

## Module Index

| # | File | Core Topics |
|---|---|---|
| 01 | [Architecture Overview](./01-microservices-architecture.md) | Bounded contexts, decentralized data, sync vs async, cascading failure prevention |
| 02 | [Transporters](./02-transporters.md) | TCP, Redis, NATS, RMQ, Kafka, gRPC, `.unwrap()` native driver access in NestJS 11 |
| 03 | [Message Patterns](./03-message-patterns.md) | `@MessagePattern` (RPC) vs `@EventPattern` (Events), object patterns, validation pipes |
| 04 | [Broker Transporters](./04-broker-transporters.md) | Deep comparison: Kafka, RabbitMQ, NATS, and Redis Pub/Sub trade-offs |
| 05 | [Hybrid Apps & API Gateway](./05-hybrid-apps-and-api-gateway.md) | BFF pattern, concurrent fan-out aggregation (`forkJoin`), correlation IDs |
| 06 | [gRPC & Protocol Buffers](./06-grpc-fundamentals-and-protobuf.md) | HTTP/2 multiplexing, `.proto` contracts, binary serialization, `nest-cli.json` assets |
| 07 | [gRPC Client & Server](./07-grpc-server-and-client.md) | `ClientGrpc`, typed client stubs, gRPC-to-HTTP error status mapping |
| 08 | [gRPC Streaming](./08-grpc-streaming.md) | Server streaming, `@GrpcStreamMethod` bi-directional streams, database cursor piping |
| 09 | [gRPC Authentication](./09-grpc-authentication.md) | HTTP/2 metadata headers, `GrpcAuthGuard`, `Metadata`, mTLS channel encryption |

---

## Key Learning Outcomes

1. Structure microservices to prevent the "distributed monolith" anti-pattern.
2. Select the optimal transporter based on latency, throughput, and durability requirements.
3. Build unified API Gateways aggregating downstream services concurrently via RxJS.
4. Implement lightning-fast, type-safe gRPC services and client stubs using Protocol Buffers.
5. Secure internal inter-service communication using gRPC metadata authentication and mTLS.
