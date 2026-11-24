---
title: "NestJS Code Review Checklist"
description: "Comprehensive 25-point pull request review checklist covering security, DI scopes, error handling, typing, and performance"
tags: [nestjs, clean-code, code-review, pull-requests, checklist]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# NestJS Code Review Checklist

## Pull Request Review Checklist

### 1. Architecture & DI
- [ ] No circular dependencies introduced (`forwardRef()` used only with justification).
- [ ] No accidental `Scope.REQUEST` bubble-up into Singleton services.
- [ ] Controllers contain zero business logic or raw database queries.
- [ ] Module `exports` are limited strictly to public APIs.

### 2. Validation & Security
- [ ] DTOs have class-validator / Zod decorators on every property.
- [ ] Mutation endpoints require authentication and authorization guards.
- [ ] Raw SQL queries use parameterized bindings ($1, $2), no string templates.
- [ ] No secrets or API keys committed to code or git.

### 3. Error Handling & Performance
- [ ] Independent asynchronous calls parallelized via `Promise.all()`.
- [ ] No empty `catch (err) {}` blocks.
- [ ] Database queries are indexed on foreign keys and search columns.
- [ ] Unit and integration tests cover new edge cases and error branches.

---

## Cross-References
- [01-clean-code-and-code-organization.md](./01-clean-code-and-code-organization.md) - Clean code principles
- [08-security-checklist.md](../../07-production/01-security/08-security-checklist.md) - Security checklist
