# Database Errors

Databases fail in specific, meaningful ways: a unique constraint is violated, a deadlock is detected, the connection drops. Left alone, these surface as generic `500 Internal Server Error` responses, which is wrong when the real problem is "that email is already registered" (a 409) and dangerous when raw SQL errors leak to clients.

This note covers the categories of database errors, how each ORM exposes them, and where to translate them into meaningful application errors.

Prerequisites: [HTTP exceptions](../../03-core-concepts/01-request-pipeline/07-http-exceptions.md), [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md), [transactions](./04-transactions.md).

## The categories that matter

| Category | Example cause | Typical HTTP mapping |
|----------|---------------|----------------------|
| **Unique violation** | Duplicate email/username | `409 Conflict` |
| **Foreign key violation** | Referencing a missing parent, or deleting a row still referenced | `409` or `422` (or `404` if the referenced id came from the client) |
| **Not-null / check violation** | Missing required value that passed app validation | Usually `400`/`422`; often means a validation gap or a bug |
| **Not found** (ORM-level) | `update`/`delete` of a missing row, `findOrFail` | `404` |
| **Deadlock / serialization failure** | Concurrent transactions conflicting | Retry; if still failing, `409`/`503` |
| **Timeout** (statement, lock, pool) | Slow query, lock wait, exhausted pool | `503`/`504`; investigate |
| **Connection errors** | DB down, network, too many clients | `503`; alert |
| **Query errors** (syntax, missing column) | Bug or un-applied migration | `500`; fix the code/migration |

## Error identification by database/driver

Don't match on **message strings**; match on **stable codes**.

### PostgreSQL (SQLSTATE codes)

| Code | Meaning |
|------|---------|
| `23505` | `unique_violation` |
| `23503` | `foreign_key_violation` |
| `23502` | `not_null_violation` |
| `23514` | `check_violation` |
| `40001` | `serialization_failure` (retry) |
| `40P01` | `deadlock_detected` (retry) |
| `57014` | `query_canceled` (statement timeout) |

The `pg` driver error carries these as `err.code`, plus `err.constraint` and `err.detail` that identify *which* constraint.

### MySQL

Errors expose a numeric `errno` / symbolic `code`: `ER_DUP_ENTRY` (1062) for duplicates, `ER_NO_REFERENCED_ROW_2` (1452) for a missing parent row, `ER_LOCK_DEADLOCK` (1213) for deadlocks.

### MongoDB

Duplicate key errors have **code `11000`**. Other Mongo errors (validation, cast) come through Mongoose's own error classes.

## How each ORM wraps them

| ORM | Error type | Where the code lives |
|-----|-----------|----------------------|
| **TypeORM** | `QueryFailedError` | `err.driverError.code` (PostgreSQL SQLSTATE, MySQL code) |
| **Prisma** | `Prisma.PrismaClientKnownRequestError` | `err.code` (Prisma codes) and `err.meta` |
| **Mongoose** | `MongoServerError`, `mongoose.Error.ValidationError`, `CastError` | `err.code === 11000`; validation/cast errors by class |

Useful Prisma codes (check the docs for your version):

| Prisma code | Meaning |
|-------------|---------|
| `P2002` | Unique constraint failed (`err.meta.target` lists the fields) |
| `P2003` | Foreign key constraint failed |
| `P2025` | Operation depends on a record that wasn't found (update/delete of missing row) |
| `P2034` | Transaction failed due to a write conflict or deadlock (retry) |

Prisma also has `PrismaClientValidationError` (a bad query shape: a bug) and `PrismaClientInitializationError` (can't connect).

## Where to translate errors

**Option A: in the repository (recommended for domain meaning).** The repository knows which constraint is which, and the service gets a clean domain error:

```ts
export class EmailAlreadyUsedError extends Error {}

@Injectable()
export class TypeOrmUsersRepository extends UsersRepository {
  async create(data: NewUser) {
    try {
      return toUser(await this.repo.save(this.repo.create(data)));
    } catch (err) {
      if (isUniqueViolation(err, 'uq_users_email')) throw new EmailAlreadyUsedError();
      throw err;                       // unknown errors propagate untouched
    }
  }
}

// helper (PostgreSQL via TypeORM)
function isUniqueViolation(err: unknown, constraint?: string) {
  const e = err as { driverError?: { code?: string; constraint?: string } };
  return (
    err instanceof QueryFailedError &&
    e.driverError?.code === '23505' &&
    (!constraint || e.driverError?.constraint === constraint)
  );
}
```

Prisma equivalent:

```ts
catch (err) {
  if (err instanceof Prisma.PrismaClientKnownRequestError && err.code === 'P2002') {
    throw new EmailAlreadyUsedError();
  }
  throw err;
}
```

A domain error is then mapped to HTTP once, in a filter or the service (`ConflictException`). The service stays free of ORM and driver details.

**Option B: in a global exception filter**, mapping ORM error types to HTTP generically (a `Prisma.PrismaClientKnownRequestError` filter is shown in [exception filters](../../03-core-concepts/01-request-pipeline/08-exception-filters.md)). It's quick and catches everything, but it can only say "something was unique", not "the **email** was taken", and it ties your HTTP layer to the ORM.

Use both: repositories translate the errors you care about; a global filter is the safety net that turns anything left into a safe response.

## The check-then-insert race

```ts
// ❌ looks safe, isn't
if (await this.users.findByEmail(email)) throw new ConflictException();
await this.users.create({ email });      // two concurrent requests can both pass the check
```

The unique constraint is the real guarantee. Keep the pre-check if you want a friendly early error, but **also** handle the constraint violation, since concurrent requests will occasionally reach it. The same applies to "insert if not exists" and counters.

## Retrying transient errors

Deadlocks (`40P01`), serialization failures (`40001`, Prisma `P2034`), and brief connection hiccups are expected under load. Retry **the whole transaction** a small, bounded number of times with backoff and jitter:

```ts
async function withRetry<T>(fn: () => Promise<T>, attempts = 3): Promise<T> {
  for (let i = 1; ; i++) {
    try {
      return await fn();
    } catch (err) {
      if (i >= attempts || !isRetryable(err)) throw err;
      await new Promise((r) => setTimeout(r, 50 * 2 ** i + Math.random() * 50));
    }
  }
}
```

Only retry operations that are **safe to repeat** (the transaction rolled back entirely, and external side effects haven't happened). Never retry constraint violations or syntax errors; they fail identically every time. See [idempotency](../08-api-design/06-idempotency.md) and [retry and circuit breaker](../../08-architecture-and-patterns/04-real-world-patterns/05-retry-and-circuit-breaker.md).

## Don't leak internals

- Never return raw database error messages, SQL, constraint names, or stack traces to clients. They expose schema details useful to attackers.
- **Log the full error** (with `cause`, constraint, and a correlation ID) server-side; return a safe, generic message plus a stable error `code`.
- Unknown database errors should become a generic `500`, logged with detail ([production error handling](../../07-production/04-deployment/09-production-error-handling.md)).

## Common mistakes

- **Matching error message text** instead of codes (breaks across versions and locales).
- **Returning 500 for unique violations**, so clients can't react.
- **Leaking SQL/constraint names** in responses.
- **Pre-check only**, with no handling of the constraint violation under concurrency.
- **Swallowing errors** and returning `null`, hiding real failures.
- **Retrying non-transient errors**, or retrying non-idempotent work.
- **Retrying inside a transaction** instead of retrying the whole transaction.
- **Mapping every database error to `400`**, hiding genuine server/availability problems.
- **Forgetting that a failed statement aborts a PostgreSQL transaction**; later statements in the same transaction fail until rollback.

## Debugging

- Log the error object's `code`, `constraint`, `detail` (PostgreSQL) or `code`/`meta` (Prisma) to see exactly what failed.
- Unique error but you don't know which constraint? Check `err.driverError.constraint` / `err.meta.target`, and name constraints explicitly in migrations so they're recognizable.
- Intermittent deadlocks: consistent lock ordering, shorter transactions, retries ([transactions](./04-transactions.md)).
- "Connection terminated"/timeouts under load: pool exhaustion or DB limits ([connection](./02-database-connection.md)).
- Errors only in production: an un-applied migration or different constraints/data than local.

## Quick Summary

- Identify errors by stable codes (PostgreSQL SQLSTATE, Prisma `P2xxx`, Mongo `11000`), never by message text.
- Typical mapping: unique → 409, FK/not-null/check → 4xx or a bug to fix, not found → 404, deadlock/serialization → retry, timeouts/connection → 503.
- Translate meaningful errors in the repository into domain errors; keep a global filter as the safety net.
- Constraints, not pre-checks, are the true guard against races; handle the violation.
- Retry only transient errors, whole-transaction, bounded, and idempotently; never leak database internals.

## Next

[ORM comparison →](./08-orm-comparison.md)
