---
title: "NestJS Interview: What's New in NestJS 11?"
description: "Mastering the major changes in NestJS 11: Express 5 default, reversed shutdown order, ConsoleLogger JSON, and unwrap()"
tags: [nestjs, interview, nestjs11, express5, upgrade]
---

# NestJS Interview: What's New in NestJS 11?

### Key Additions:
1. **Express 5.0 Default Engine**: Path-to-regexp changes (requires named wildcards like `*path`).
2. **Reversed Shutdown Order**: Shutdown hooks run in reverse topological order to protect database pools.
3. **Structured ConsoleLogger**: Native `{ json: true }` output.
4. **Transporter `.unwrap()`**: Direct access to underlying microservice client drivers.
