# Error Handling Design

Failures are normal in production: networks drop, disks fill, inputs are wrong, dependencies time out. Error handling is a design concern, not an afterthought. A well-designed system decides **where errors are detected, how they travel, who handles them, and what the caller sees**.

## Principles

| Principle | Meaning |
|---|---|
| **Fail fast** | Detect problems as early as possible, close to the cause |
| **Don't swallow errors** | Never ignore a failure silently |
| **Handle where you can act** | Catch an error only where you can recover or add useful meaning |
| **Preserve the cause** | Wrap with context, but keep the original exception attached |
| **Translate at boundaries** | Don't leak low-level exceptions across layers |
| **Make failure visible** | Log once, with enough context, at the right place |
| **Separate expected from unexpected** | Business outcomes vs bugs/infrastructure failures |

## Kinds of errors

| Kind | Examples | Typical handling |
|---|---|---|
| **Programming errors (bugs)** | `NullPointerException`, illegal argument from your own code | Fail fast, log, fix the code; don't try to "recover" |
| **Invalid input** | Missing field, malformed email | Reject at the boundary with a clear message |
| **Business rule violations** | Insufficient funds, order already shipped | Model explicitly; return a meaningful result to the caller |
| **Transient infrastructure failures** | Timeout, connection reset, deadlock | Retry with limits and backoff |
| **Permanent infrastructure failures** | Service down, misconfiguration | Fail with a clear error, alert, use fallback if sensible |

Treating all of these the same way (a generic `catch (Exception e)`) is the root of most bad error handling.

## Exceptions in Java

### Checked vs unchecked

| | Checked (`Exception`) | Unchecked (`RuntimeException`) |
|---|---|---|
| Compiler forces handling | Yes | No |
| Typical use | Recoverable conditions the caller is expected to handle | Programming errors, and failures callers can't reasonably recover from |
| Drawback | Clutters signatures; leaks through layers | Easy to forget to handle |

Modern practice in most application code: prefer **unchecked exceptions** for most failures, use a small number of meaningful exception types, and handle them at well-chosen boundaries. Use checked exceptions sparingly, where the caller can realistically do something different depending on the failure.

### Design a small, meaningful hierarchy

```java
public abstract class AppException extends RuntimeException {
    protected AppException(String message, Throwable cause) { super(message, cause); }
}

public class NotFoundException extends AppException {
    public NotFoundException(String what, Object id) { super(what + " not found: " + id, null); }
}

public class ConflictException extends AppException { ... }     // e.g. stale version, duplicate
public class ValidationException extends AppException { ... }
public class ExternalServiceException extends AppException { ... }
```

Name exceptions after **what went wrong in the domain**, not after the technology (`InsufficientFundsException`, not `SqlUpdateException`).

### Rules for throwing and catching

```java
// ✘ Swallowing
try { process(); } catch (Exception e) { }

// ✘ Losing the cause
catch (SQLException e) { throw new RuntimeException("DB error"); }

// ✘ Log and rethrow (duplicate noise up the stack)
catch (IOException e) { log.error("failed", e); throw e; }

// ✔ Add context and keep the cause
catch (SQLException e) {
    throw new OrderStorageException("Could not save order " + order.id(), e);
}
```

- **Catch specific exceptions**, not `Exception` or `Throwable`, except at a top-level boundary.
- **Log once**, at the place where the error is finally handled, not at every layer.
- **Never use exceptions for ordinary control flow** (an exception for "item not in map" in a hot loop). They're slow and obscure intent.
- **Don't expose internal details** (stack traces, SQL) to end users.
- **Restore interruption**: if you catch `InterruptedException` and don't rethrow, call `Thread.currentThread().interrupt()`.

### Resource cleanup

```java
try (var in = Files.newInputStream(path);
     var out = Files.newOutputStream(target)) {
    in.transferTo(out);
}     // both closed automatically, even if an exception occurs
```

Use try-with-resources for anything closeable. It also preserves the original exception, with close failures recorded as suppressed exceptions.

## Expected failures as values

For outcomes that are a **normal part of the domain**, an exception can be the wrong tool. Return an explicit result so callers must deal with it.

```java
public sealed interface WithdrawResult {
    record Success(Money newBalance) implements WithdrawResult {}
    record InsufficientFunds(Money shortfall) implements WithdrawResult {}
    record AccountFrozen() implements WithdrawResult {}
}

WithdrawResult result = account.withdraw(amount);
String message = switch (result) {
    case WithdrawResult.Success s           -> "New balance: " + s.newBalance();
    case WithdrawResult.InsufficientFunds f -> "Short by " + f.shortfall();
    case WithdrawResult.AccountFrozen a     -> "Account is frozen";
};
```

With sealed types, the compiler checks that every case is handled (Java 21+ for pattern-matching `switch`).

| Use exceptions for | Use result values for |
|---|---|
| Unexpected failures, bugs, infrastructure errors | Expected business outcomes the caller must branch on |
| Failures that should abort the whole operation | Validation with multiple possible errors |
| When propagating up many layers is natural | When the caller is the direct decision maker |

### Absence: `Optional`, not `null`

```java
Optional<User> findByEmail(String email);          // "may not exist" is part of the contract
```

Return `Optional` from lookups that may find nothing; don't return `null`, and don't pass `Optional` as a parameter or field. For collections, return an empty collection.

## Where errors are handled

```text
Adapter (DB, HTTP client)   → translate technical exceptions into your own types
Domain / services           → throw domain exceptions or return results; no HTTP concepts
Boundary (controller, listener, job)
                            → ONE place that converts errors into responses / retries / dead-letter
```

A centralized handler at the boundary keeps the rest of the code free of response logic:

```java
@RestControllerAdvice
class ApiExceptionHandler {

    @ExceptionHandler(NotFoundException.class)
    ResponseEntity<ApiError> notFound(NotFoundException e) {
        return ResponseEntity.status(404).body(new ApiError("NOT_FOUND", e.getMessage()));
    }

    @ExceptionHandler(ValidationException.class)
    ResponseEntity<ApiError> invalid(ValidationException e) {
        return ResponseEntity.badRequest().body(new ApiError("INVALID_REQUEST", e.getMessage()));
    }

    @ExceptionHandler(Exception.class)
    ResponseEntity<ApiError> unexpected(Exception e) {
        log.error("Unhandled error", e);                                      // full detail in logs
        return ResponseEntity.status(500).body(new ApiError("INTERNAL_ERROR", "Something went wrong"));
    }
}
```

Return a stable, documented error shape with a machine-readable code and a safe message. Never return stack traces.

## Resilience patterns for external calls

Failures between components are normal, so design calls to dependencies defensively.

| Technique | Purpose | Notes |
|---|---|---|
| **Timeouts** | Don't wait forever | Set on every network call; the most important and most forgotten |
| **Retries with backoff and jitter** | Survive transient errors | Limit attempts; add randomness so clients don't retry in lockstep |
| **Idempotency** | Make retries safe | Use idempotency keys so a repeated request doesn't double-charge |
| **Circuit breaker** | Stop hammering a failing dependency | Fail fast while it recovers |
| **Bulkheads** | Isolate resource pools | One slow dependency can't use all threads |
| **Fallbacks** | Degrade gracefully | Cached data, default value, reduced feature |
| **Dead-letter queue** | Park messages that keep failing | Inspect and reprocess later |

```java
// Conceptual retry with exponential backoff
for (int attempt = 1; ; attempt++) {
    try {
        return client.call(request);
    } catch (TransientException e) {
        if (attempt == MAX_ATTEMPTS) throw new ExternalServiceException("Gave up after " + attempt, e);
        sleep(backoff(attempt));                      // e.g. 100ms, 200ms, 400ms plus jitter
    }
}
```

Only retry **transient** failures, and only for **idempotent** operations. Retrying a non-idempotent call may duplicate its effect. In practice use a resilience library rather than hand-written loops. These can be applied as a [Decorator](../05-structural-patterns/04-decorator.md) around a gateway:

```java
PaymentGateway gateway = new CircuitBreakerGateway(new RetryingGateway(new StripeGateway()));
```

## Validation

| Where | What | Example |
|---|---|---|
| **Boundary (DTO)** | Shape and format | Required, length, email format |
| **Domain objects** | Invariants that must always hold | Quantity > 0, order can't be empty when submitted |
| **Services** | Rules needing other data | Email must be unique |

Validate early, and make invariants enforced in constructors so invalid objects can't exist ([DDD](../08-architecture/04-domain-driven-design.md)). Collect multiple validation errors when helpful for the user instead of failing on the first.

## Logging errors usefully

- **Include context**: IDs, operation, relevant parameters (never secrets or personal data).
- **Use a correlation ID** to link logs across layers and services.
- **Choose the right level**: `ERROR` for failures needing attention, `WARN` for recoverable anomalies, not `ERROR` for expected business outcomes such as "not found".
- **Don't log and throw** at every layer.
- **Alert on symptoms** (error rate, latency), not on every single log line.

## Common mistakes

- **Empty catch blocks** or `catch (Exception e) { e.printStackTrace(); }`.
- **Throwing `RuntimeException("error")`** with no meaning or cause.
- **Returning `null` or magic values** (-1, empty string) to signal failure.
- **Catching too broadly** and hiding bugs.
- **Leaking internals**: SQL errors, stack traces or server paths in API responses.
- **No timeouts** on network calls, so one slow dependency exhausts all threads.
- **Unbounded or immediate retries** that amplify an outage (retry storms).
- **Retrying non-idempotent operations**, causing duplicate charges or messages.
- **Exceptions for normal control flow.**
- **Inconsistent error formats** across endpoints.
- **Losing the original exception** when wrapping.
- **Partial failure left unhandled**: some steps committed, others not, with no compensation (see [unit of work](../07-application-patterns/02-unit-of-work.md)).
- **Treating every error as retryable**, including validation errors that will never succeed.

## Quick Summary

- Distinguish bugs, invalid input, business outcomes and infrastructure failures, and handle each differently.
- Fail fast, preserve causes, translate at boundaries and handle errors in one central place.
- Use exceptions for the unexpected and result types for expected business outcomes.
- For external calls: timeouts, bounded retries with backoff, idempotency and circuit breakers.
- Log once, with context, and never expose internals to users.

## Related

[Testable design](../09-testing-and-debugging/01-testable-design.md) · [Decorator](../05-structural-patterns/04-decorator.md) · [Service layer](../07-application-patterns/04-service-layer.md) · [Event-driven](../08-architecture/05-event-driven.md) (retries, dead letters)

## Next

[03-performance-tradeoffs.md](./03-performance-tradeoffs.md)
