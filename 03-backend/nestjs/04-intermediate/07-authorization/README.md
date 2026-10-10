# Authorization

Authentication tells you **who** is calling ([previous section](../06-authentication/README.md)). Authorization decides **what they may do**: which routes, which records, which fields. It's where most real-world API breaches happen, usually not through clever exploits but through a missing check on a single endpoint.

```text
 request ──► authenticate (req.user) ──► authorize
                                           │
                 coarse:   may this user call this route at all?        (guard: roles/permissions)
                 fine:     may they do THIS action on THIS record?      (policy in service / CASL)
                 data:     which records may they even see in a list?   (query scoping)
                 fields:   which properties may they read or write?     (field-level rules)
```

> Applies to NestJS 10/11. The CASL note targets `@casl/ability` v6 and its Prisma/Mongoose adapters; confirm API details against the current CASL docs for your versions.

## Reading order

| # | Note | Answers |
|---|------|---------|
| 01 | [Authorization fundamentals](./01-authorization-fundamentals.md) | Models (ACL/RBAC/ABAC/ReBAC), where checks live, 401/403/404, IDOR/BOLA, deny by default |
| 02 | [RBAC](./02-rbac.md) | Roles and permissions, `@Roles()` guards, hierarchies, token staleness |
| 03 | [ABAC and policies](./03-abac-and-policies.md) | Ownership/attribute rules, policy functions, list scoping, field-level rules |
| 04 | [CASL integration](./04-casl-integration.md) | `AbilityFactory`, policy guards, field and query-level checks |

## Prerequisites

- [Guards](../../03-core-concepts/01-request-pipeline/04-guards.md) and [execution context](../../03-core-concepts/01-request-pipeline/09-execution-context.md) (`Reflector`, metadata)
- [Custom decorators](../../03-core-concepts/01-request-pipeline/10-custom-decorators.md)
- [Authentication architecture](../06-authentication/01-authentication-architecture.md) (what `req.user` is and how fresh it is)

## Related

- [Security fundamentals](../../07-production/01-security/01-security-fundamentals.md)
- [Multi-tenancy](../../08-architecture-and-patterns/04-real-world-patterns/08-multi-tenancy.md)
- [Audit logging](../../08-architecture-and-patterns/04-real-world-patterns/01-audit-logging.md)
- [Interview questions: authentication and authorization](../../12-interview-preparation/04-authentication-and-authorization.md)