# Custom Exceptions

A **custom exception** is your own exception class that expresses a failure in your domain (`InsufficientFundsException`, `OrderNotFoundException`) instead of a generic one (`RuntimeException`). Used well, it makes failures **specific, catchable and informative**. Used badly, it floods a codebase with near-identical classes.

```java
public class InsufficientFundsException extends Exception {          // checked
    private final double balance;
    private final double requested;

    public InsufficientFundsException(double balance, double requested) {
        super("Insufficient funds: balance " + balance + ", requested " + requested);
        this.balance = balance;
        this.requested = requested;
    }

    public double getShortfall() { return requested - balance; }
}
```

```java
try {
    account.withdraw(500);
} catch (InsufficientFundsException e) {
    System.out.println("Short by " + e.getShortfall());     // structured data, not string parsing
}
```

## When to create one

| Create a custom exception when | Reuse a standard one when |
|--------------------------------|---------------------------|
| Callers need to **catch and handle it separately** from other failures | A standard type already says it: `IllegalArgumentException`, `IllegalStateException`, `NoSuchElementException`, `UnsupportedOperationException` |
| It carries **extra data** the handler needs (ids, codes, amounts) | The message alone is enough |
| It names a **domain concept** (`PaymentDeclinedException`) | The failure is a generic precondition violation |
| It marks an **API boundary** (`MyLibraryException` wrapping internals) | You would create a class per message |
| You want to map failures to responses (HTTP status, exit code) | Only one `catch` block would ever use it |

Rule of thumb: **if no caller will ever catch it specifically, or if it adds nothing over a standard exception, do not create it.**

## Checked or unchecked?

Extend `Exception` for **checked**, `RuntimeException` for **unchecked** ([00](./00_exception-hierarchy-and-types.md)).

| Extend `Exception` (checked) | Extend `RuntimeException` (unchecked) |
|------------------------------|---------------------------------------|
| The caller can realistically **recover** and you want the compiler to force them to consider it | Programming errors, violated preconditions, failures most callers cannot fix |
| Public library API for a recoverable condition | Domain failures in application code that propagate to a global handler |
| | Anything used in lambdas and streams |

Many modern codebases default to **unchecked** domain exceptions, with checked ones only where recovery is part of the contract. Make the choice deliberately and consistently per project.

## The standard four constructors

Provide the constructors callers expect:

```java
public class StorageException extends RuntimeException {

    public StorageException() { super(); }

    public StorageException(String message) { super(message); }

    public StorageException(String message, Throwable cause) { super(message, cause); }

    public StorageException(Throwable cause) { super(cause); }
}
```

| Constructor | Use |
|-------------|-----|
| `(String message)` | A new failure |
| `(String message, Throwable cause)` | **Wrapping** a lower-level exception with context: the most important one |
| `(Throwable cause)` | Wrapping with a default message (`cause.toString()`) |
| `()` | Rarely useful |

Include only the constructors you need, but **always** offer the `(message, cause)` form so chaining works ([02](./02_throw-and-throws.md)).

## Carrying data

Add fields for what a handler or log entry needs. Make them `final` and immutable.

```java
public class OrderNotFoundException extends RuntimeException {
    private final long orderId;

    public OrderNotFoundException(long orderId) {
        super("Order not found: " + orderId);
        this.orderId = orderId;
    }
    public long orderId() { return orderId; }
}
```

```java
public class ValidationException extends RuntimeException {
    private final List<String> errors;
    public ValidationException(List<String> errors) {
        super("Validation failed: " + errors.size() + " error(s)");
        this.errors = List.copyOf(errors);            // defensive copy: exception state must not change
    }
    public List<String> errors() { return errors; }
}
```

Keep the data **small and serializable-friendly**: store ids and values, not entire domain objects, sessions or connections. Exceptions are often logged, serialized and kept alive in traces.

## Designing a hierarchy

One **base exception per application or module** lets callers catch "anything from this library" or a specific case:

```
RuntimeException
   └── ShopException                         base for the application
         ├── OrderException
         │     ├── OrderNotFoundException
         │     └── OrderAlreadyPaidException
         └── PaymentException
               ├── PaymentDeclinedException
               └── PaymentGatewayException
```

```java
public class ShopException extends RuntimeException {
    public ShopException(String message) { super(message); }
    public ShopException(String message, Throwable cause) { super(message, cause); }
}
public class OrderNotFoundException extends ShopException { ... }

try { checkout(); }
catch (PaymentDeclinedException e) { showCardError(); }      // specific handling
catch (ShopException e) { showGenericError(); }              // anything else from the shop
```

Keep it **shallow** (two or three levels) and avoid creating a class for every message. Prefer a specific class only where handlers differ.

## Error codes and categories

Sometimes an `enum` code scales better than dozens of classes, for example for API responses:

```java
public enum ErrorCode { NOT_FOUND, CONFLICT, INVALID_INPUT, UNAUTHORIZED }

public class ApiException extends RuntimeException {
    private final ErrorCode code;
    public ApiException(ErrorCode code, String message) { super(message); this.code = code; }
    public ErrorCode code() { return code; }
}
```

A global handler maps `code` to an HTTP status ([05](./05_exception-handling-patterns.md)). Do not ship **internal** messages to end users: map codes to safe user-facing text.

## Naming and style

| Guideline | Example |
|-----------|---------|
| End the name with `Exception` | `PaymentDeclinedException` |
| Name the **problem**, not the layer | `UserNotFoundException`, not `UserServiceException` |
| Place it near the code that throws it (same package) | `com.example.orders.OrderNotFoundException` |
| Write a precise Javadoc: when it is thrown, what the data means | |
| Include **specifics** in the message (ids, values), no secrets | `"Order 42 not found"` |
| Be consistent: all checked or all unchecked per module | |

## Serialization and `serialVersionUID`

`Throwable` implements `Serializable`, so compilers and IDEs may warn that your exception lacks a `serialVersionUID`:

```java
private static final long serialVersionUID = 1L;
```

Add it if exceptions cross serialization boundaries (RMI, some messaging, application servers) or if your build treats the warning as an error. Make any extra fields serializable (or `transient`). Records **cannot** be used for exceptions: records cannot extend a class, and `Exception` is a class.

## Performance: disabling the stack trace

Creating an exception captures a stack trace, which dominates its cost. For exceptions used as a **high-frequency signal** in a hot path (rare, and usually a design smell), you can skip it:

```java
public class FastFailException extends RuntimeException {
    public FastFailException(String message) {
        super(message, null, false, false);     // enableSuppression=false, writableStackTrace=false
    }
}
// Or override fillInStackTrace() { return this; }
```

Do this only after profiling, and never for exceptions developers need to debug. See [20-performance](../20-performance/README.md).

## Using custom exceptions in a method contract

```java
/**
 * Withdraws money from the account.
 *
 * @throws InsufficientFundsException if the balance is lower than the amount
 * @throws IllegalArgumentException   if the amount is not positive
 */
public void withdraw(double amount) throws InsufficientFundsException {
    if (amount <= 0) throw new IllegalArgumentException("amount must be positive: " + amount);
    if (amount > balance) throw new InsufficientFundsException(balance, amount);
    balance -= amount;
}
```

Note the split: the **caller's bug** (negative amount) is a standard unchecked exception; the **business condition** (not enough money) is your domain exception.

## Testing

```java
@Test
void withdrawMoreThanBalanceFails() {
    Account account = new Account("Ada", 100);
    InsufficientFundsException e =
        assertThrows(InsufficientFundsException.class, () -> account.withdraw(150));
    assertEquals(50, e.getShortfall());
}
```

Assert on the **type and data**, not on message text. See [19-testing/01_junit-5.md](../19-testing/01_junit-5.md).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| One exception class per error message | Class explosion | Reuse; add fields or codes |
| Extending `Exception` for everything | `throws` clauses everywhere, swallowed exceptions | Prefer unchecked unless recovery is expected |
| No `(message, cause)` constructor | Causes lost when wrapping | Provide it |
| Storing heavy objects in the exception | Memory held, leaks into logs | Ids and primitives |
| Mutable exception state | Surprising changes after it is thrown | `final` fields, defensive copies |
| Parsing `getMessage()` in handlers | Fragile | Add fields or subclasses |
| Putting secrets or full personal data in messages | Data leaks via logs and responses | Keep messages safe |
| Throwing `new Exception("...")` or `new RuntimeException("...")` directly | Handlers cannot discriminate | A specific type |
| Catching your base exception too early | Specific handling never runs | Catch subclasses first |
| Naming it `FooError` | Confusable with `java.lang.Error` | `...Exception` |
| Creating exceptions for control flow in hot paths | Slow | Return values or checks |

## Key takeaways

- Create a custom exception when callers must distinguish it or it carries data; otherwise reuse a standard one
- Choose checked only when recovery is genuinely expected; unchecked is the common default in application code
- Provide at least `(message)` and `(message, cause)` constructors; keep fields `final` and small
- One base exception per module, a shallow hierarchy, names ending in `Exception`
- Test the type and data, never the message text

**Next:** [Exception Handling Patterns](./05_exception-handling-patterns.md)