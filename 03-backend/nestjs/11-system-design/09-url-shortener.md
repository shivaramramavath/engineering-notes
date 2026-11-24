---
title: "System Design: TinyURL URL Shortener and Click Telemetry"
description: "Designing a high-throughput URL shortener handling 100B links with Base62 encoding and sub-millisecond redirection"
tags: [nestjs, system-design, url-shortener, base62, fastify]
---

# System Design: TinyURL URL Shortener and Click Telemetry

## Architecture
- **Base62 Key Generation**: Pre-allocated distributed sequence counters via Redis / Snowflake IDs.
- **Redirection**: Fastify 5 engine with 99% cache hit ratio on Redis cluster; asynchronous click tracking pushed to Kafka.
