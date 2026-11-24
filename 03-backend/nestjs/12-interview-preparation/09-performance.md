---
title: "NestJS Interview: Performance Optimization and V8 Profiling"
description: "Event loop lag, V8 heap optimization, memory leak diagnosis, and clustering in NestJS"
tags: [nestjs, interview, performance, event-loop, v8]
---

# NestJS Interview: Performance Optimization and V8 Profiling

### Q1: Why is event loop lag the most critical metric in a NestJS backend?
**Answer**: Because Node.js is single-threaded. If a synchronous task or regex blocks the event loop for 150ms, all concurrent user requests are frozen for that duration, resulting in massive latency spikes across the board.
