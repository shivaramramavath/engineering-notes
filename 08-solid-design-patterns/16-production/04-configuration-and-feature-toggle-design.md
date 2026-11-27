# Configuration & Feature Toggle Design

> **TL;DR**: Validate all environment configuration (`DATABASE_URL`, `PORT`, timeouts) with a strict schema at **application startup** (fail fast!), never scatter `process.env.X` across 30 files, and keep Feature Flags at single decision points.

## Best Practices
1. **Typed Config Object at Composition Root**: Parse & validate env vars once at boot (Pydantic `BaseSettings`, Zod, Spring `@ConfigurationProperties`).
2. **Secrets Never in Git**: Load via Vault / Cloud Secret Manager / env vars, never hardcode or commit `.env`.
3. **Clean Up Release Toggles**: Set expiry owners on feature flags so they don't rot into Lava Flow.
