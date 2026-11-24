---
title: "Testing Best Practices in NestJS"
description: "Designing fast, reliable test suites: unit testing with mocks, integration testing with Testcontainers, and end-to-end testing"
tags: [nestjs, best-practices, testing, unit-tests, e2e, testcontainers]
nestjs_version: "11.x"
node_version: ">=20.0.0"
express_version: "5.x"
---

# Testing Best Practices in NestJS

## Core Principles
1. **The Testing Pyramid**: 70% Unit tests (fast, isolated), 20% Integration tests (database, cache), 10% E2E tests (HTTP endpoints).
2. **Use Testcontainers for Real Databases**: Avoid in-memory SQLite mocks that behave differently from PostgreSQL. Use Docker Testcontainers in integration tests.
3. **Override Providers Cleanly**: Use `moduleFixture.overrideProvider(Service).useValue(mock)`.

---

## Cross-References
- [05-ci-cd.md](../../07-production/04-deployment/05-ci-cd.md) - Automated CI testing
