# Error Handling Design

| Field | Value |
| :--- | :--- |
| **Category** | Production Design |

> **TL;DR**: Never swallow exceptions silently, never leak raw stack traces or SQL errors to clients, and distinguish **expected domain failures** (`Result<T, E>` / `400`/`409`) from **unexpected bugs** (`500` + alert).

## Best Practices
1. **RFC 9457 Problem Details**: Return structured API errors (`type`, `title`, `status`, `detail`, `instance` / `traceId`).
2. **Fail Fast at Boundaries**: Validate config at startup and DTOs at the HTTP edge.
3. **Never Catch-and-Ignore**: `catch (Exception ignored) {}` hides data corruption; log with context or rethrow.
4. **Separate Expected vs Unexpected**: Return `Result` / `4xx` for bad input or business conflicts; let unexpected bugs hit the global `500` handler + error tracker (Sentry/OTel).
