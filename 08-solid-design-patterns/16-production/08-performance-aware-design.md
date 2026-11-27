# Performance-Aware Design

> **TL;DR**: Avoid structural performance traps early—**N+1 queries**, **unbounded queues/lists**, **blocking the async event loop**, and **loading whole files into RAM**—while leaving micro-optimizations until after profiling.

## Architectural Performance Habits
1. **Bound Everything**: Every list endpoint has pagination; every queue has `maxsize`; every HTTP client has a timeout.
2. **Batch & Stream**: Use `JOIN FETCH`/`selectinload` instead of N+1 loops, and lazy streams/generators for large exports.
3. **Never Block Event Loops**: Offload CPU-heavy crypto/PDF/ML work in Node.js or Python `asyncio` to worker threads/queues.
4. **Measure Before Optimizing**: Profile CPU/memory flamegraphs before complicating clean code.
