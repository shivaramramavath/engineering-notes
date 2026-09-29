# 15 — Patterns & Architecture

Higher-level structural patterns for organizing a real Mongoose-backed application — separating data access from business logic, implementing soft deletes properly at scale, and supporting multiple tenants in one database.

## In this section

| File                                   | Covers                                                                                                                                        |
| -------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `01-repository-and-service-pattern.md` | Separating Mongoose-specific data access from business logic — the boundary referenced throughout `08-errors/06-custom-application-errors.md` |
| `02-soft-delete.md`                    | A full, production-ready soft-delete implementation — query middleware, the partial unique index, and the trade-offs                          |
| `03-multi-tenancy.md`                  | Supporting multiple tenants (customers/organizations) in one MongoDB deployment — the main strategies and their trade-offs                    |

## Why this section comes near the end

Every pattern here assumes everything before it: schemas, middleware, errors, relationships, and performance. A repository layer is only worth building once you understand what it's actually abstracting away (Mongoose-specific error shapes, query construction); a soft-delete implementation needs middleware, partial indexes, and filter conditions all working together; multi-tenancy touches schema design, indexing, and query patterns all at once.

## What you should be able to do after this section

- Structure an application with a clear boundary between Mongoose-specific data access and framework-agnostic business logic
- Implement soft delete correctly at the schema level, including the partial-unique-index detail that prevents a common real bug
- Choose and justify a multi-tenancy strategy for a given application's scale and isolation requirements

## Next

**`16-testing`** covers testing a Mongoose-backed application — an in-memory database, mocking, and fixtures.
