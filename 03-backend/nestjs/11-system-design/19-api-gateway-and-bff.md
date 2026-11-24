---
title: "System Design: Enterprise API Gateway and Backend-for-Frontend (BFF)"
description: "Designing an enterprise API Gateway and BFF layer: rate limiting, token translation, protocol bridging, and caching"
tags: [nestjs, system-design, api-gateway, bff, reverse-proxy]
---

# System Design: Enterprise API Gateway and Backend-for-Frontend (BFF)

## Architecture
- **BFF Pattern**: Tailored gateway endpoints for Web, Mobile, and Public API clients.
- **Cross-Cutting Concerns**: SSL termination, central authentication, correlation ID injection, and response aggregation.
