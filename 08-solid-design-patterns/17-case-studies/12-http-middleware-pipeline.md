# Case Study 12: HTTP Middleware Pipeline

**Patterns Used**: **Chain of Responsibility / Onion Middleware** (`(ctx, next) => Promise<void>`), **Decorator** (wrapping core handler), **Adapter** (normalizing framework `Request`/`Response` to internal `HttpContext`).

## Key Design Insight
Order middleware deliberately (`ErrorHandler -> RequestId/Trace -> Logger -> CORS -> Auth -> RateLimit -> Validator -> RouteHandler`) and guard `dispatch(i)` so `next()` can never be invoked twice by one middleware.

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Workflow Engine](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/11-workflow-engine.md) | [Module 17](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/README.md) | [Module 18: Pattern Selection](file:///E:/Learning/engineering-notes/08-solid-design-patterns/18-pattern-selection/README.md) |
