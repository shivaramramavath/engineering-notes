---
title: "System Design: Realtime Social Media Activity Feed"
description: "Designing a Twitter/Instagram-style activity feed: Fan-out on Write vs Fan-out on Read in NestJS"
tags: [nestjs, system-design, social-feed, fanout, redis-timelines]
---

# System Design: Realtime Social Media Activity Feed

## Architecture
- **Hybrid Fan-Out**:
  - Regular users: Fan-out on write (push post ID to followers' Redis timelines).
  - Celebrity users ($> 50{,}000$ followers): Fan-out on read (merge celebrity posts at query time).
