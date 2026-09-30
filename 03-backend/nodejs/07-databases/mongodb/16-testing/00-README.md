# 16 — Testing

Testing a Mongoose-backed application — running real Mongoose code against a genuine, disposable database instead of a real MongoDB server, and mocking data access at the repository boundary established in `15-patterns-and-architecture/01-repository-and-service-pattern.md`.

## In this section

| File                                       | Covers                                                                                                          |
| ------------------------------------------ | --------------------------------------------------------------------------------------------------------------- |
| `01-testing-with-an-in-memory-database.md` | `mongodb-memory-server` — running real Mongoose queries against a genuine, temporary MongoDB instance for tests |
| `02-mocking-and-fixtures.md`               | Mocking the repository layer for fast unit tests, and building reusable test data (fixtures/factories)          |

## Two different testing strategies, and when each fits

```
Unit tests    → mock the repository layer entirely → fast, no real database, tests business logic in isolation
Integration tests → a real (in-memory) MongoDB instance → slower, but tests actual Mongoose behavior: casting, validation, indexes, middleware
```

Neither replaces the other — a service function's business logic can be unit-tested with a mocked repository; whether a schema's validators, indexes, and hooks actually behave as expected needs a real database to verify against, even if that "real" database is a temporary in-memory one rather than a shared test server.

## What you should be able to do after this section

- Set up `mongodb-memory-server` for integration tests that exercise real Mongoose/MongoDB behavior
- Mock a repository layer to unit-test service-layer business logic quickly, without touching a database at all
- Build reusable test fixtures/factories instead of duplicating test data setup across many test files

## Next

**`17-production`** covers deploying and operating a Mongoose-backed application in production — connection management, monitoring, migrations, and a final checklist.
