# Case Study 10: Rate Limiter Library

**Patterns Used**: **Strategy** (`TokenBucket`, `SlidingWindowCounter`), **Decorator / Middleware** (`withRateLimit(handler)`), **Factory** (instantiate limiter from config), **Atomic Store** (Redis Lua script or `ConcurrentHashMap`).

## Key Design Insight
Separate the **Algorithm** (`RateLimitStrategy.allow(key)`) from the **Storage Backend** (`InMemoryStore`, `RedisLuaStore`) and **Framework Integration** (Express/FastAPI/Spring middleware).
