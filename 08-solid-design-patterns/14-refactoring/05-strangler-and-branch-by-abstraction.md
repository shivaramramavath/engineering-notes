# Strangler & Branch by Abstraction

| Field | Value |
| :--- | :--- |
| **Category** | Refactoring (Large-Scale Migrations) |
| **Origin** | Paul Hammant (Branch by Abstraction) / Martin Fowler (Strangler Fig) |

> **TL;DR**: **Branch by Abstraction** lets you swap a core library or persistence layer (e.g., Legacy Cache → Redis) inside a live trunk-based codebase **without long-lived Git feature branches** that cause merge hell.

## 5-Step Branch by Abstraction
1. **Introduce an Abstraction (Interface)** over the legacy component (`CachePort`).
2. **Route all callers** through `CachePort` (still backed 100% by Legacy).
3. **Implement the new adapter** (`RedisCacheAdapter`) behind the interface + Feature Flag / Shadow Compare.
4. **Flip callers** progressively to `RedisCacheAdapter` in production.
5. **Delete** the legacy implementation (and optionally inline the interface if no longer needed).
