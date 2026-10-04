# throw and throws

Two keywords with similar names and different jobs:

| Keyword | Where | Meaning |
|---------|-------|---------|
| **`throw`** | A **statement** inside a method body | Actually throws an exception object **now** |
| **`throws`** | A **clause** in a method signature | Declares which checked exceptions the method **may** throw |

```java
public void withdraw(double amount) throws InsufficientFundsException {   // throws: declaration
    if (amount > balance) {
        throw new InsufficientFundsException(balance, amount);             // throw: action
    }
    balance -= amount;
}
```

## `throw`

```java
throw new IllegalArgumentException("amount must be positive: " + amount);
```

- Operand: any object of type `Throwable` (in practice an `Exception` or `RuntimeException`)
- Execution of the current block stops; the JVM searches up the call stack for a matching `catch`
- Code immediately after `throw` is unreachable (compile error)
- The **stack trace is recorded where the exception object is created** (`new ...`), not where it is thrown

```java
// Create-and-throw in one statement: the trace points at this line
throw new IllegalStateException("not initialized");

// Equivalent but discouraged: creating an exception far from where it is thrown gives a misleading trace
IllegalStateException e = new IllegalStateException("x");   // trace says "here"
... lots of code ...
throw e;
```

`throw` can also appear in expressions:

```java
String name = optional.orElseThrow(() -> new NoSuchElementException("no name"));

int days = switch (month) {
    case 1, 3, 5, 7, 8, 10, 12 -> 31;
    case 4, 6, 9, 11 -> 30;
    case 2 -> 28;
    default -> throw new IllegalArgumentException("month: " + month);
};
```

## `throws`

Lists the **checked** exceptions a method or constructor can let escape.

```java
public String readFirstLine(Path p) throws IOException {      // callers must handle or declare IOException
    return Files.readAllLines(p).get(0);
}

public Data load() throws IOException, ParseException { ... }  // several, comma-separated
```

| Rule | Detail |
|------|--------|
| Checked exceptions thrown (directly or by calls) and **not caught** must be declared | Else: `unreported exception X; must be caught or declared to be thrown` |
| Unchecked exceptions **may** be declared (documentation) but need not be | |
| Subclass covers: `throws IOException` also allows `FileNotFoundException` | |
| Constructors can declare `throws` too | |
| `main` can declare `throws Exception` | Fine for tiny programs and examples |
| Declaring an exception that is never thrown | Allowed for checked exceptions (but pointless) |

The caller then **chooses**:

```java
// 1. Handle
try { String line = readFirstLine(path); } catch (IOException e) { fallback(); }

// 2. Pass it on
void caller() throws IOException { readFirstLine(path); }

// 3. Translate into another exception
void caller() { try { readFirstLine(path); } catch (IOException e) { throw new UncheckedIOException(e); } }
```

### Be specific

```java
void process() throws Exception { }          // forces callers to deal with "anything"
void process() throws IOException, SQLException { }    // precise: callers know what can happen
```

## Overriding and `throws`

An overriding method may throw **no new or broader checked exceptions** than the method it overrides (it can throw fewer, narrower ones, or none), because callers using the parent type only prepared for the parent's exceptions.

```java
class Reader1 { void read() throws IOException { } }
class Reader2 extends Reader1 {
    @Override void read() throws FileNotFoundException { }     // narrower: OK
}
class Reader3 extends Reader1 {
    @Override void read() throws Exception { }                 // ERROR: broader
}
```

Unchecked exceptions are unrestricted. See [06-inheritance](../04-oop/06_inheritance.md).

## Exception chaining: keep the cause

When you catch one exception and throw another, pass the original as the **cause** so the full story is not lost.

```java
try {
    repository.save(order);
} catch (SQLException e) {
    throw new OrderException("Could not save order " + order.id(), e);     // e becomes the cause
}
```

Resulting trace:

```
OrderException: Could not save order 42
    at OrderService.place(OrderService.java:30)
    ...
Caused by: java.sql.SQLException: Connection refused
    at ...
```

| Do | Don't |
|----|-------|
| `new MyException("context", e)` | `new MyException(e.getMessage())` (cause and trace lost) |
| `initCause(e)` if the constructor has no cause parameter | `throw new MyException("failed")` inside a `catch` that ignores `e` |

## Validating arguments: fail fast

Check preconditions at the **start** of a method and throw the **right** unchecked exception.

| Situation | Exception |
|-----------|-----------|
| Argument is `null` where not allowed | `NullPointerException` via `Objects.requireNonNull(x, "x")` (the JDK convention) |
| Argument value is invalid (negative, out of range, malformed) | `IllegalArgumentException` |
| Object is not in a valid state for this call | `IllegalStateException` |
| Index out of range | `IndexOutOfBoundsException` (`Objects.checkIndex(i, size)`) |
| Operation not supported | `UnsupportedOperationException` |
| Requested element does not exist | `NoSuchElementException` |

```java
public Account(String owner, double opening) {
    this.owner = Objects.requireNonNull(owner, "owner must not be null");
    if (opening < 0) throw new IllegalArgumentException("opening balance must be >= 0, was " + opening);
    this.balance = opening;
}

public void close() {
    if (closed) throw new IllegalStateException("account already closed");
    closed = true;
}
```

Failing early keeps the broken data from spreading and makes the stack trace point at the real culprit ([defensive programming](../23-design-and-clean-code/05_defensive-programming.md)). Validate at **public boundaries**; trusted internal code can use `assert` ([06](./06_stack-traces-and-debugging.md)).

## Writing good messages

| Guideline | Example |
|-----------|---------|
| Say **what** was wrong and include the **offending values** | `"age must be 0..150, was -5"` |
| Include identifiers needed to find the data | `"Order 42 not found"` |
| State the expectation | `"expected 3 columns but line 17 had 2"` |
| Do **not** include secrets (passwords, tokens, personal data) | Messages end up in logs and error pages |
| Do not end with generic text (`"Error"`, `"Something went wrong"`) | Useless when debugging |
| Do not repeat the exception class name | The trace already shows it |

## Checked exceptions and lambdas

Functional interfaces such as `Function`, `Consumer` and `Supplier` do not declare checked exceptions, so this does not compile:

```java
paths.stream().map(p -> Files.readString(p))    // ERROR: unreported exception IOException
```

Options:

```java
// 1. Catch inside the lambda and wrap
paths.stream().map(p -> {
    try { return Files.readString(p); }
    catch (IOException e) { throw new UncheckedIOException(e); }
}).toList();

// 2. A small helper that wraps (see 05_exception-handling-patterns.md)
paths.stream().map(unchecked(Files::readString)).toList();

// 3. Use a plain loop when failures need real handling
```

`java.io.UncheckedIOException` exists exactly for this. See [09-functional-java](../09-functional-java/README.md).

## `throws` in documentation

Document exceptions as part of the method contract with Javadoc:

```java
/**
 * Withdraws money.
 *
 * @param amount the amount to withdraw, must be positive
 * @throws IllegalArgumentException if {@code amount <= 0}
 * @throws InsufficientFundsException if the balance is lower than {@code amount}
 */
public void withdraw(double amount) throws InsufficientFundsException { ... }
```

## Exceptions in constructors and static initializers

- A constructor may throw: the object is **not created**, and the caller gets the exception ([constructors](../04-oop/01_constructors.md))
- An exception escaping a **static initializer** becomes `ExceptionInInitializerError` ([initialization order](../04-oop/05_initialization-order.md))

## Return value, `Optional` or exception?

| Situation | Prefer |
|-----------|--------|
| "No result" is a normal, expected outcome (search, lookup) | `Optional<T>`, empty collection, or `null`-free sentinel |
| The caller cannot proceed without a result and absence is abnormal | Exception |
| Failure is frequent and cheap to check | Boolean or result type |
| Failure carries rich information | Exception (or a result object) |
| Precondition violated by the caller | Unchecked exception |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Confusing `throw` and `throws` | Syntax errors | `throw` = statement, `throws` = signature |
| `throws Exception` on everything | Callers forced to catch everything | Specific types |
| Wrapping without the cause | Lost root cause in the trace | Pass `e` as the cause |
| `throw new X(e.getMessage())` | Original trace lost | `new X("context", e)` |
| Widening `throws` in an override | Compile error | Same, narrower, or none |
| Throwing `Exception` or `RuntimeException` directly | Callers cannot distinguish failures | Specific or custom types |
| Vague messages (`"error"`) | Hard to debug | Include values and context |
| Validating late (deep inside) | Failure far from the cause | Check at the start |
| `throw null` | `NullPointerException` | Throw a real exception |
| Declaring unchecked exceptions in `throws` and expecting enforcement | No compiler check | Document with Javadoc `@throws` |
| Creating exceptions ahead of time and reusing them | Wrong stack traces, thread-safety issues | Create at the throw site |

## Key takeaways

- `throw` raises an exception; `throws` declares checked exceptions a method may raise
- Handle, declare or translate: every checked exception needs one of the three
- Overrides cannot add broader checked exceptions
- Always chain causes; never lose the original exception
- Validate arguments early with the right unchecked exception and informative messages
- Use `UncheckedIOException` or wrappers to cross lambda boundaries

**Next:** [try-with-resources](./03_try-with-resources.md)
