---
title: "The Adapter Pattern in NestJS"
description: "Converting incompatible interfaces: AbstractHttpAdapter, legacy SDK wrapping, and external system adapters in NestJS"
tags: [nestjs, design-patterns, adapter, abstract-http-adapter, legacy-integration]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# The Adapter Pattern in NestJS

## Conceptual Overview
The Adapter pattern allows classes with incompatible interfaces to work together by wrapping one interface around another.

In NestJS:
- `AbstractHttpAdapter` adapts Express and Fastify to the internal NestJS HTTP contract.
- Domain adapters wrap third-party SDKs to prevent vendor lock-in.

---

## Key Takeaways & Cheat Sheet

- Adapters translate external technical contracts into your application's domain interfaces.
- Protect domain code from breaking third-party SDK updates by wrapping them in adapters.

---

## Cross-References
- [05-platform-adapters.md](../../06-internals/05-platform-adapters.md) - Platform adapter internals
- [02-strategy.md](./02-strategy.md) - Strategy pattern
