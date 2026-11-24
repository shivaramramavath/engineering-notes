---
title: "System Design: Designing a Scalable REST API in NestJS"
description: "Designing a high-throughput REST API supporting 100,000 requests/second with NestJS, Fastify, and Redis"
tags: [nestjs, system-design, rest-api, fastify, scalability]
---

# System Design: Designing a Scalable REST API in NestJS

## Requirements
- **Functional**: CRUD operations, filtering, pagination, search, authentication.
- **Non-Functional**: 100,000 QPS peak, P99 latency $< 20\text{ms}$, 99.99% availability.

## Architecture
- **Perimeter**: Cloudflare Anycast CDN + AWS Application Load Balancer.
- **Compute**: NestJS on Fastify 5 adapter, stateless container pods on Kubernetes with HPA.
- **Caching**: Multi-tier (Nginx edge caching + ElastiCache Redis cluster).
- **Database**: Amazon Aurora PostgreSQL with 1 Primary + 3 Read Replicas.
