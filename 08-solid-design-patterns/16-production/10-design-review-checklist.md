# Production Design Review Checklist

Use this checklist before approving major PRs or launching new services:

## Architecture & Boundaries
- [ ] Are domain rules isolated from HTTP/SQL/SDK details?
- [ ] Are dependencies injected (no hidden mutable global singletons)?
- [ ] Are abstractions justified (no speculative over-engineering)?

## Reliability & Scale
- [ ] Explicit connect + read timeouts on every network/DB call?
- [ ] Retries have exponential backoff + jitter and apply **only** to idempotent operations?
- [ ] All queues, caches, and list queries bounded (`maxsize`, TTL, pagination)?
- [ ] N+1 SQL queries checked?

## Security & Operations
- [ ] AuthN + object-level AuthZ enforced on every endpoint?
- [ ] Request DTOs prevent mass assignment; secrets/PII redacted from logs?
- [ ] Backward-compatible DB/API changes (`Expand-and-Contract`)?
- [ ] Structured logs + trace IDs + `/healthz` (no DB check) & `/readyz` probes wired?

---

## Navigation

| Previous | Up | Next |
| :--- | :--- | :--- |
| [Documentation & Design Reviews](file:///E:/Learning/engineering-notes/08-solid-design-patterns/16-production/09-documentation-and-design-reviews.md) | [Module 16](file:///E:/Learning/engineering-notes/08-solid-design-patterns/16-production/README.md) | [Module 17: Case Studies](file:///E:/Learning/engineering-notes/08-solid-design-patterns/17-case-studies/README.md) |
