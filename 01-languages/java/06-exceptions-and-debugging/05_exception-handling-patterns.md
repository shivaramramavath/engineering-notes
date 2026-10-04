# Exception Handling Patterns

The syntax of exceptions is small; the **design** is where projects go wrong. This file collects the patterns that keep error handling clean and the anti-patterns that make failures invisible or noisy.

```
 low level ──► throws specific failure ──► middle layer wraps/translates with context ──► top level handles ONCE
 (IOException,                              (OrderException, cause kept)                (log, map to HTTP 500,
  SQLException)                                                                           show message, exit code)
```

## Guiding principles

| Principle | Meaning |
|-----------|---------|
| **Fail fast** | Detect problems as early as possible, near their cause |
| **Handle where you can act** | Catch only if you can recover, add context, or translate; otherwise let it propagate |
| **Don't lose information** | Keep the cause, the message and the stack trace |
| **Be specific** | Throw and catch precise types; avoid `Exception`/`Throwable` |
| **Handle once** | Log or rethrow at one point, not at every layer |
| **Clean up always** | try-with-resources and `finally` |
| **Don't use exceptions for normal flow** | They are for exceptional conditions |
| **Never leak internals** | Users see friendly messages; logs see detail |

## Patterns

### 1. Fail fast (guard clauses)

```java
public void transfer(Account from, Account to, long cents) {
    Objects.requireNonNull(from, "from");
    Objects.requireNonNull(to, "to");
    if (cents <= 0) throw new IllegalArgumentException("cents must be > 0, was " + cents);
    if (from.equals(to)) throw new IllegalArgumentException("cannot transfer to the same account");
    ...                                       // main logic with all preconditions known to hold
}
```

See [02_throw-and-throws.md](./02_throw-and-throws.md) and [defensive programming](../23-design-and-clean-code/05_defensive-programming.md).

### 2. Exception translation (wrapping at layer boundaries)

Lower layers throw implementation-specific exceptions (`SQLException`); higher layers should not depend on them. **Translate** to an exception that fits the abstraction, and keep the cause.

```java
public Order findOrder(long id) {
    try {
        return dao.load(id);
    } catch (SQLException e) {
        throw new RepositoryException("Cannot load order " + id, e);     // cause preserved
    }
}
```

Benefits: callers depend only on your API's exceptions; the storage technology can change; the cause is still visible in the trace. Frameworks do this at scale (Spring translates `SQLException` into `DataAccessException`).

**Exception chaining only when it adds value**: one translation at each real boundary (repository → service → controller), not at every method.

### 3. Add context

Failures need to say *which* thing failed.

```java
for (Path p : files) {
    try {
        process(p);
    } catch (IOException e) {
        throw new BatchException("Failed while processing " + p, e);
    }
}
```

### 4. Handle at the top: one global handler

Let exceptions propagate to a single place that logs, reports and responds.

```java
// Last-resort handler for any thread
Thread.setDefaultUncaughtExceptionHandler((thread, ex) ->
    log.error("Uncaught exception in thread {}", thread.getName(), ex));

// Command-line main
public static void main(String[] args) {
    try {
        new App().run(args);
    } catch (UserInputException e) {
        System.err.println("Error: " + e.getMessage());       // friendly
        System.exit(2);
    } catch (Exception e) {
        log.error("Unexpected failure", e);                    // detail goes to the log
        System.exit(1);
    }
}
```

Web frameworks provide the same idea (`@ControllerAdvice`/`@ExceptionHandler` in Spring, exception mappers in JAX-RS) to map exceptions to HTTP responses:

```
OrderNotFoundException → 404     ValidationException → 400     anything else → 500 (generic body, details only in logs)
```

### 5. Log **or** rethrow, not both

```java
// Anti-pattern: every layer logs and rethrows → the same failure appears 5 times in the log
catch (SQLException e) {
    log.error("db failed", e);
    throw new RepositoryException(e);
}

// Pattern: translate/wrap and throw; log once where it is finally handled
catch (SQLException e) {
    throw new RepositoryException("Cannot load order " + id, e);
}
```

Exception: log **and** swallow only when you really handle the failure (fallback, retry) and want a record.

### 6. Recover: fallback, default, retry

```java
// Fallback value
String config;
try { config = Files.readString(userConfig); }
catch (NoSuchFileException e) { config = DEFAULT_CONFIG; }          // a missing file is normal here

// Retry transient failures with backoff
for (int attempt = 1; ; attempt++) {
    try { return client.call(); }
    catch (TransientException e) {
        if (attempt == MAX) throw e;
        sleepWithBackoff(attempt);
    }
}
```

Retries, timeouts and circuit breakers: [25-real-world-patterns/02_retry-and-backoff.md](../25-real-world-patterns/02_retry-and-backoff.md), [06_circuit-breaker-and-resilience.md](../25-real-world-patterns/06_circuit-breaker-and-resilience.md). Retry only **idempotent** operations ([04_idempotency.md](../25-real-world-patterns/04_idempotency.md)).

### 7. Cleanup always

Use try-with-resources ([03](./03_try-with-resources.md)); use `try/finally` for locks and non-`AutoCloseable` cleanup. When a failure leaves state half-changed, **restore** it (rollback a transaction, delete the temporary file):

```java
connection.setAutoCommit(false);
try {
    debit(from, cents);
    credit(to, cents);
    connection.commit();
} catch (RuntimeException | SQLException e) {
    connection.rollback();
    throw e;
}
```

See [16-jdbc-and-databases/04_transactions.md](../16-jdbc-and-databases/04_transactions.md).

### 8. Return types for expected absence or failure

Not every "no" is exceptional.

```java
Optional<User> find(long id);                    // absence is normal
List<Order> search(Criteria c);                  // empty list, never null

sealed interface Result<T> permits Ok, Err {}    // explicit success/failure, no exception for expected business outcomes
record Ok<T>(T value) implements Result<T> {}
record Err<T>(String reason) implements Result<T> {}
```

Use exceptions for **unexpected** problems and broken contracts; use return types for **expected alternatives** ([09-functional-java/03_optional.md](../09-functional-java/03_optional.md), [12-modern-java/02_sealed-classes.md](../12-modern-java/02_sealed-classes.md)).

### 9. Collect errors instead of failing on the first

For form or batch validation, accumulate all problems and report them together:

```java
List<String> errors = new ArrayList<>();
if (name.isBlank()) errors.add("name is required");
if (age < 0) errors.add("age must be >= 0");
if (!errors.isEmpty()) throw new ValidationException(errors);
```

### 10. Exceptions inside lambdas and streams

Checked exceptions do not fit functional interfaces. Wrap them in one helper rather than repeating `try/catch` in every lambda:

```java
@FunctionalInterface
interface ThrowingFunction<T, R> { R apply(T t) throws Exception; }

static <T, R> Function<T, R> unchecked(ThrowingFunction<T, R> f) {
    return t -> {
        try { return f.apply(t); }
        catch (RuntimeException e) { throw e; }                  // do not double-wrap unchecked ones
        catch (Exception e) { throw new RuntimeException(e); }
    };
}

List<String> contents = paths.stream().map(unchecked(Files::readString)).toList();
```

Use `UncheckedIOException` for I/O specifically. Alternatively, process with a loop when each failure needs custom handling ([09-functional-java/05_stream-operations.md](../09-functional-java/05_stream-operations.md)).

### 11. Exceptions across threads and async code

An exception in a worker thread does **not** propagate to the thread that submitted the work.

```java
Future<Integer> f = executor.submit(() -> compute());
try { f.get(); }
catch (ExecutionException e) { Throwable real = e.getCause(); ... }       // the original exception is the CAUSE

CompletableFuture.supplyAsync(this::load)
    .exceptionally(ex -> fallback());                                      // handle without blocking
```

See [14-concurrency/12_completablefuture.md](../14-concurrency/12_completablefuture.md). For `InterruptedException`, restore the interrupt flag ([01](./01_try-catch-finally.md)).

### 12. Mapping to user-facing messages

```
Internal exception  ──►  log with full detail and a correlation id
                    ──►  user sees: "Something went wrong (ref 7f3a). Please try again."
```

Never show stack traces, SQL, file paths or internal messages to end users ([21-security](../21-security/README.md)).

## Anti-patterns

| Anti-pattern | Example | Why it hurts | Instead |
|--------------|---------|--------------|---------|
| **Swallowing** | `catch (Exception e) { }` | The failure vanishes; bugs surface far away | Handle, translate, or log with the exception and rethrow |
| **Catching too broadly** | `catch (Throwable t)` | Hides bugs and fatal errors | Specific types |
| **Log and rethrow everywhere** | `log.error(...); throw e;` at each layer | Duplicate noise | Log once at the boundary |
| **Losing the cause** | `throw new X(e.getMessage())` | No original trace | `new X("context", e)` |
| **`e.printStackTrace()`** | In production code | Unstructured, not captured by logging | A logger |
| **Throwing generic types** | `throw new Exception("failed")` | Callers cannot react specifically | Specific or custom |
| **Exceptions for control flow** | Ending loops with exceptions, parsing by catching `NumberFormatException` in a hot path | Slow, obscure | Conditions, `hasNext()`, validation |
| **Returning `null`/magic codes instead of exceptions** | `return -1;` on failure | Callers forget to check | Exceptions, `Optional`, result types |
| **`return`/`throw` in `finally`** | Overrides exceptions | Silent loss | Keep `finally` clean |
| **Checked exception leakage** | `throws SQLException` all the way to the UI | Layers coupled to implementation | Translate at boundaries |
| **Over-wrapping** | Five nested wrappers with identical messages | Noisy traces | Wrap only at real boundaries |
| **Pokemon catching** ("gotta catch 'em all") | Long chain of unrelated catches doing the same | Noise | Multi-catch or a common supertype |
| **Using exceptions to implement business rules everywhere** | `UserExistsException` for a normal duplicate check | Heavy and unclear | Explicit checks or result types |
| **Exposing internal details to users** | Stack trace in an HTTP response | Information leak | Generic message + log id |
| **Partial updates** | Failure after half of a multi-step change | Inconsistent state | Transactions, rollback, ordering |
| **Ignoring `InterruptedException`** | Empty catch | Thread cannot stop | Restore the flag, then exit |

## A worked example

```java
// repository layer: translate
public Order load(long id) {
    try (Connection con = ds.getConnection();
         PreparedStatement ps = con.prepareStatement("SELECT ... WHERE id = ?")) {
        ps.setLong(1, id);
        try (ResultSet rs = ps.executeQuery()) {
            if (!rs.next()) throw new OrderNotFoundException(id);      // domain failure, specific
            return map(rs);
        }
    } catch (SQLException e) {
        throw new RepositoryException("Cannot load order " + id, e);   // infrastructure failure, wrapped
    }
}

// service layer: no try/catch needed, exceptions propagate
public Receipt checkout(long orderId) {
    Order order = repo.load(orderId);
    payments.charge(order);                                            // may throw PaymentDeclinedException
    return Receipt.of(order);
}

// web layer: one handler maps exceptions to responses
@ExceptionHandler(OrderNotFoundException.class) → 404
@ExceptionHandler(PaymentDeclinedException.class) → 402 with a friendly message
@ExceptionHandler(Exception.class) → 500 "unexpected error (ref abc)"; log full detail once
```

Each layer does one job; no layer swallows, logs twice or leaks internals.

## Checklist for reviewing exception handling

- [ ] No empty `catch` blocks
- [ ] Specific exception types caught and thrown
- [ ] Causes preserved when wrapping; messages include useful values
- [ ] Each failure logged **once**, with the exception object, at the boundary
- [ ] Resources closed with try-with-resources
- [ ] State left consistent after a failure
- [ ] No `return`/`throw` in `finally`
- [ ] Users never see internal details
- [ ] Expected "absence" uses `Optional`/empty collections, not exceptions
- [ ] Tests cover the failure paths ([19-testing](../19-testing/README.md))

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Catching just to print | Lost control flow, duplicate output | Handle meaningfully or let it propagate |
| Catching `Exception` in the middle of a call chain | Programming errors hidden | Catch where you can act |
| Translating exceptions without context | Cannot tell which input failed | Add ids and values |
| Retrying non-idempotent operations | Duplicate side effects (double charge) | Idempotency keys |
| Showing `e.getMessage()` to end users | Technical or sensitive text exposed | Map to friendly messages |
| Using one giant `try` around a whole method | Cannot tell which step failed | Narrow scopes |
| Forgetting rollback on failure | Corrupt or partial data | Transactions with `catch`/`finally` |

## Key takeaways

- Fail fast, handle where you can act, translate at layer boundaries, and **preserve the cause**
- Log or rethrow, never both at every layer; handle once at the top with a global handler
- Use return types (`Optional`, results) for expected outcomes and exceptions for the unexpected
- Avoid swallowing, over-broad catches, `printStackTrace`, and exception-driven control flow
- Always clean up, and leave state consistent after failure

**Next:** [Stack Traces and Debugging](./06_stack-traces-and-debugging.md)