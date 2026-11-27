# Architecture Tests

| Field | Value |
| :--- | :--- |
| **Category** | Testing & Architecture Governance |
| **Related** | [Architecture Fitness Functions](file:///E:/Learning/engineering-notes/08-solid-design-patterns/10-architecture/13-architecture-fitness-functions.md) |

> **TL;DR**: Architecture tests run as normal unit tests in CI (`ArchUnit`, `dependency-cruiser`, `import-linter`) to guarantee layering, module encapsulation, and naming/annotation conventions never drift.

## Common Rules Worth Automating
- `domain` package must have **0** dependencies on `infra`, `web`, or framework annotations.
- No circular dependencies between top-level modules.
- Every `@RestController` method must be annotated with auth or explicitly `@PublicEndpoint`.
- No `System.out.println` / `print()` in production packages.
