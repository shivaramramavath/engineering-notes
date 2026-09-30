# Production Checklist

A consolidated, final checklist pulling together the important details from across this entire guide — organized by section, so each item links back to where it was actually covered in depth.

## Setup & Connection

- [ ] Connection string loaded from an environment variable, never hardcoded (`03-setup/02-connecting-to-mongodb.md`)
- [ ] Connected exactly once at application startup — never inside a request handler (`03-setup/02-`, `14-performance/04-connection-pooling.md`)
- [ ] `mongoose.connection.on("error", ...)` attached — an unhandled connection error crashes the process (`03-setup/03-connection-events-and-lifecycle.md`)
- [ ] Process exits (fails fast) if the _initial_ connection attempt fails; doesn't crash on later, transient disconnects (`03-setup/03-`, `17-production/01-`)
- [ ] Graceful shutdown on `SIGTERM`/`SIGINT`: close the HTTP server before the MongoDB connection, with a force-exit timeout fallback (`17-production/01-connection-management-in-production.md`)
- [ ] `maxPoolSize` sized with instance count in mind (`pool size × instances` vs. MongoDB's connection limit) (`14-performance/04-`)

## Schemas

- [ ] `timestamps: true` on schemas that need `createdAt`/`updatedAt` (`04-schemas/05-schema-options.md`)
- [ ] `toJSON` transform strips sensitive fields (password hashes, internal flags) from every serialized response (`04-schemas/05-`)
- [ ] Sensitive fields marked `select: false` (`04-schemas/03-schema-type-options.md`)
- [ ] `unique: true` fields paired with a `partialFilterExpression` if the schema also supports soft delete (`15-patterns-and-architecture/02-soft-delete.md`)
- [ ] `mongoose.models.Name || mongoose.model(...)` guard on every model file, to avoid `OverwriteModelError` under hot-reloading (`05-models/03-`)

## Errors

- [ ] Centralized error-handling middleware maps `ValidationError`, `CastError`, and duplicate-key (`code === 11000`) errors to appropriate, distinct HTTP responses (`08-errors/05-turning-errors-into-friendly-responses.md`)
- [ ] Duplicate-key errors return `409`, not `400` or `500` (`08-errors/04-`, `08-errors/05-`)
- [ ] Application error classes (`AppError` hierarchy) used at the service boundary, keeping Mongoose-specific error shapes out of the rest of the codebase (`08-errors/06-custom-application-errors.md`)
- [ ] Unexpected (non-`AppError`) errors are logged loudly (`console.error`/structured logging) before returning a generic `500`

## Validation

- [ ] `runValidators: true` on every `updateOne`/`updateMany`/`findOneAndUpdate`/`findByIdAndUpdate` call that could introduce invalid data (`09-validation/04-validating-updates.md`) — the single most commonly missed item on this entire checklist
- [ ] `{ new: true }` included wherever the updated document is actually needed back (`06-crud-methods/03-update-methods.md`)
- [ ] Async validators kept fast (indexed DB lookups, not slow external API calls) (`09-validation/03-async-validators.md`)
- [ ] Async uniqueness validators, if used, backed by a real unique index — never relied on alone (`09-validation/03-`)

## Indexes

- [ ] Indexes exist for actual query/sort patterns, verified with `explain("executionStats")` (`14-performance/01-indexes-in-mongoose.md`)
- [ ] `autoIndex: false` in production; indexes managed deliberately via `syncIndexes()` or a migration step (`04-schemas/07-`, `14-performance/01-`)
- [ ] Compound index field order follows the leftmost-prefix rule for actual queries (`14-performance/01-`)

## Relationships

- [ ] Embed-vs-reference decisions made deliberately per relationship, not defaulted uniformly (`11-relationships/02-embedding-vs-referencing-in-mongoose.md`)
- [ ] `.populate()` field-selects rather than fetching entire referenced documents (`11-relationships/01-`, `11-relationships/04-`)
- [ ] `$lookup` used instead of `.populate()` wherever filtering/sorting the primary query depends on a related collection's field (`11-relationships/04-populate-performance.md`)

## Performance

- [ ] `.lean()` used on read-only queries that don't need Document features (`14-performance/02-query-optimization-and-lean.md`)
- [ ] Cursor-based pagination used for large or growing, user-facing lists — not `skip`/`limit` at depth (`14-performance/03-pagination.md`)
- [ ] Maximum requested page size capped server-side (`14-performance/03-`)
- [ ] No queries awaited inside a loop where batching (`$in`, `bulkWrite`) or `Promise.all` would work instead (`14-performance/02-`, `06-crud-methods/06-`)

## Transactions

- [ ] Multi-document/multi-collection operations that must be all-or-nothing wrapped in a real transaction, not left to hooks alone (`13-transactions/`, `10-middleware-hooks/03-common-hook-patterns.md`)
- [ ] Every operation inside a transaction explicitly passed `{ session }` — the most common transaction bug (`13-transactions/01-sessions-and-transactions.md`)
- [ ] No external side effects (emails, API calls) inside a transaction callback, since it may be retried (`13-transactions/02-transaction-patterns.md`)
- [ ] Confirmed the MongoDB deployment is a replica set (Atlas: automatic; self-hosted: verified explicitly) (`13-transactions/00-README.md`)

## Security-adjacent

- [ ] `mongoose.Types.ObjectId.isValid()` (or equivalent) checked before querying by a user-supplied ID, or `CastError` handled explicitly and mapped to `400` (`08-errors/03-cast-errors.md`)
- [ ] User input never used to build a regex directly without escaping (`07-filter-conditions/06-regex-and-text-filters.md`)
- [ ] No full documents (including `select: false` fields loaded explicitly) logged wholesale (`17-production/02-monitoring-and-logging.md`)

## Testing

- [ ] Business logic covered by fast, mocked unit tests at the service/repository boundary (`16-testing/02-mocking-and-fixtures.md`)
- [ ] Schema-dependent behavior (validation, unique indexes, middleware) covered by real integration tests against `mongodb-memory-server` (`16-testing/01-`)

## Migrations

- [ ] A plan exists for schema changes affecting existing data — not just "edit the schema and deploy" (`17-production/03-migrations.md`)
- [ ] Large backfills are batched, not run as one unbounded operation (`17-production/03-`)
- [ ] Migrations tested against realistic data before running in production, with a rollback plan (`17-production/03-`)

## Monitoring

- [ ] Slow-query monitoring in place (Atlas Performance Advisor, or self-hosted profiling) (`17-production/02-monitoring-and-logging.md`)
- [ ] Connection pool health watched as a leading indicator, not discovered during an outage (`14-performance/04-`, `17-production/02-`)
- [ ] Alerting reserved for genuinely actionable signals — expected, handled errors (duplicate keys, not-found) don't page anyone (`17-production/02-`)
- [ ] Structured logging with a request/correlation ID for tracing a specific failing request (`17-production/02-`)

---

## The five items most worth double-checking before every deploy

If nothing else, verify these — they're the ones covered repeatedly throughout this guide as the most common, most consequential mistakes:

1. **`runValidators: true`** on update operations that could introduce invalid data
2. **`{ session }`** passed to every single operation inside a transaction
3. **`autoIndex: false`** in production, with indexes managed deliberately
4. **A `toJSON` transform** stripping sensitive fields from every response
5. **Centralized error-handling middleware** correctly distinguishing `ValidationError`, `CastError`, and duplicate-key errors into the right HTTP responses

## Guide complete

That's the full Mongoose Mastery guide — from MongoDB fundamentals and the native driver, through every layer of Mongoose itself, to running it reliably in production. From here, the best next step is applying this to a real project, returning to specific sections as reference whenever something doesn't behave as expected.
