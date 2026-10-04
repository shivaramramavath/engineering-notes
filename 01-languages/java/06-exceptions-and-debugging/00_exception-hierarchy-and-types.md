# Exception Hierarchy and Types

An **exception** is an object that represents an abnormal event during execution. When one is **thrown**, normal flow stops and the JVM looks up the call stack for a handler. All exceptions are instances of classes in one hierarchy rooted at `Throwable`.

```java
int[] a = new int[3];
a[5] = 1;      // throws ArrayIndexOutOfBoundsException; the next statement never runs
```

## The hierarchy

```
                       Object
                         │
                     Throwable                        can be thrown and caught
                    /         \
               Error           Exception
        (JVM / system         /          \
         problems)     checked          RuntimeException
                     exceptions         (unchecked)
                          │                  │
   OutOfMemoryError   IOException     NullPointerException
   StackOverflowError SQLException    IllegalArgumentException
   AssertionError     InterruptedException   IllegalStateException
   NoClassDefFound…   ClassNotFoundException IndexOutOfBoundsException
                      TimeoutException       ArithmeticException, ClassCastException ...
```

| Branch | Meaning | Must you handle it? | Should you catch it? |
|--------|---------|---------------------|----------------------|
| **`Error`** | Serious JVM or environment failure | No | Almost never |
| **`Exception`** (excluding `RuntimeException`): **checked** | Foreseeable problem outside your control | **Yes** (catch or declare) | Yes, where you can recover or translate |
| **`RuntimeException`**: **unchecked** | Programming error or violated precondition | No | Rarely; fix the bug instead |

## Checked vs unchecked

The difference is enforced by the **compiler**: a method that can throw a checked exception must either **catch** it or **declare** it with `throws` ("catch or specify").

```java
void read() {
    new FileReader("data.txt");           // ERROR: unreported exception FileNotFoundException;
}                                         //        must be caught or declared to be thrown

void read() throws FileNotFoundException {   // declare it...
    new FileReader("data.txt");
}

void read() {
    try { new FileReader("data.txt"); }      // ...or handle it
    catch (FileNotFoundException e) { /* recover */ }
}
```

Unchecked exceptions need no declaration:

```java
int divide(int a, int b) { return a / b; }      // may throw ArithmeticException; nothing declared
```

| | Checked | Unchecked |
|---|---------|-----------|
| Extends | `Exception` (not `RuntimeException`) | `RuntimeException` (or `Error`) |
| Compiler enforces handling | **Yes** | No |
| Typical cause | I/O, network, database, external systems | Bugs, bad arguments, illegal state |
| Examples | `IOException`, `SQLException`, `InterruptedException`, `TimeoutException`, `ClassNotFoundException` | `NullPointerException`, `IllegalArgumentException`, `IllegalStateException`, `IndexOutOfBoundsException`, `ArithmeticException`, `ClassCastException` |
| Caller can reasonably recover? | Often yes (retry, fall back, ask the user) | Usually no: fix the code |

### Which should you use?

The classic guideline (Effective Java, Item 70):

- **Checked** for **recoverable** conditions the caller can reasonably be expected to handle
- **Unchecked** for **programming errors** and precondition violations

In practice, many modern libraries and frameworks (Spring, Hibernate) use **unchecked** exceptions throughout, because checked exceptions:

- Force `throws` clauses through every layer (leaky abstractions)
- Do not mix well with **lambdas and streams** (functional interfaces declare no checked exceptions)
- Lead to empty `catch` blocks written just to silence the compiler

A pragmatic rule: use checked exceptions at **boundaries with unreliable resources** where handling is genuinely possible (file and network I/O in the JDK), and **translate** them into unchecked domain exceptions at layer boundaries ([05_exception-handling-patterns.md](./05_exception-handling-patterns.md)).

## `Throwable`: what every exception provides

| Method | Purpose |
|--------|---------|
| `getMessage()` | Human-readable description (may be `null`) |
| `getCause()` | The exception that caused this one (chaining) |
| `getStackTrace()` | The call stack at construction time, as an array |
| `printStackTrace()` | Prints the trace to `System.err` (use a logger in real code) |
| `getSuppressed()` / `addSuppressed(t)` | Exceptions suppressed during cleanup ([03](./03_try-with-resources.md)) |
| `initCause(t)`, `fillInStackTrace()` | Advanced |
| `toString()` | Class name + `": " + message` |

```java
try {
    Integer.parseInt("abc");
} catch (NumberFormatException e) {
    System.out.println(e.getMessage());     // For input string: "abc"
    System.out.println(e);                  // java.lang.NumberFormatException: For input string: "abc"
}
```

## `Error`: not for application code

Errors signal conditions that a normal application cannot sensibly handle.

| Error | Meaning | Typical cause |
|-------|---------|---------------|
| `OutOfMemoryError` | Heap (or other memory) exhausted | Leaks, huge data, too small `-Xmx` ([20-performance](../20-performance/README.md)) |
| `StackOverflowError` | Call stack exhausted | Infinite or too-deep recursion ([recursion](../02-methods/03_recursion.md)) |
| `NoClassDefFoundError` | Class present at compile time, missing or failed to load at runtime | Packaging, static initializer failure ([classpath](../05-packages-and-modules/01_classpath-and-jars.md)) |
| `ExceptionInInitializerError` | A static initializer threw | [05-initialization-order](../04-oop/05_initialization-order.md) |
| `AssertionError` | An `assert` failed (or you threw it) | Violated internal invariant |
| `VirtualMachineError`, `LinkageError`, `InternalError` | JVM-level faults | Rare |

Do not catch `Error` or `Throwable` in business code. The main legitimate places are top-level handlers that **log and shut down** (a thread-pool worker, a service entry point).

## Common exceptions you will meet

### Unchecked

| Exception | Thrown when |
|-----------|-------------|
| `NullPointerException` | Using `null` as an object (method call, field access, unboxing, `throw null`) |
| `IllegalArgumentException` | A method received an invalid argument |
| `IllegalStateException` | The object is in the wrong state for the call (closed stream, not initialized) |
| `NumberFormatException` | Parsing a malformed number (`extends IllegalArgumentException`) |
| `IndexOutOfBoundsException` | Invalid index (`ArrayIndexOutOfBoundsException`, `StringIndexOutOfBoundsException`) |
| `ArithmeticException` | Integer division by zero, `BigDecimal` non-terminating division |
| `ClassCastException` | Invalid downcast ([polymorphism](../04-oop/07_polymorphism.md)) |
| `ArrayStoreException`, `NegativeArraySizeException` | Array misuse |
| `UnsupportedOperationException` | Operation not supported (modifying `List.of(...)`) |
| `ConcurrentModificationException` | Collection modified while iterating ([iterators](../08-collections/12_iterators-and-fail-fast-behavior.md)) |
| `NoSuchElementException` | `next()` on an exhausted iterator, `Optional.get()` on empty, `Scanner` at EOF |
| `DateTimeException` | Invalid date/time values ([10-date-and-time](../10-date-and-time/README.md)) |
| `UncheckedIOException`, `CompletionException` | Wrappers for checked causes |

### Checked

| Exception | Thrown when |
|-----------|-------------|
| `IOException` | Any I/O failure; subclasses `FileNotFoundException`, `NoSuchFileException`, `SocketException`, ... |
| `SQLException` | Database errors ([16-jdbc-and-databases](../16-jdbc-and-databases/README.md)) |
| `InterruptedException` | A waiting or sleeping thread was interrupted ([14-concurrency](../14-concurrency/README.md)) |
| `ExecutionException`, `TimeoutException` | Futures |
| `ClassNotFoundException`, `NoSuchMethodException`, `IllegalAccessException` | Reflection |
| `CloneNotSupportedException` | `clone()` on a non-`Cloneable` ([clone](../04-oop/15_clone-and-copying.md)) |
| `ParseException`, `URISyntaxException`, `GeneralSecurityException` | Parsing, URIs, crypto |

## `NullPointerException` in modern Java

Since Java 14 (on by default in 15), NPE messages say **what** was null:

```
Exception in thread "main" java.lang.NullPointerException:
    Cannot invoke "String.length()" because "name" is null
```
```
Cannot invoke "com.example.User.getName()" because the return value of "com.example.Repo.find(int)" is null
```

(Local variable names appear only if the class was compiled with `-g`; otherwise they show as `"<local1>"`.) Useful for debugging; still best to avoid `null` by design ([09-functional-java/03_optional.md](../09-functional-java/03_optional.md)).

## Exceptions are objects, so inheritance matters

```java
try { ... }
catch (FileNotFoundException e) { }     // specific
catch (IOException e) { }               // more general: parent class
```

A `catch (IOException e)` also catches `FileNotFoundException` and every other subclass. Order of catch blocks must go from **specific to general** ([next file](./01_try-catch-finally.md)). Your own exceptions join the hierarchy ([04_custom-exceptions.md](./04_custom-exceptions.md)).

## Cost of exceptions

Creating an exception **captures the stack trace**, which is relatively expensive (microseconds, more for deep stacks). Exceptions are for **exceptional** conditions, not routine control flow (`while(true)` loops ended by an exception, or using exceptions instead of `if`/`hasNext()`).

## Quick decision guide

| Situation | Use |
|-----------|-----|
| Caller passed a bad argument | `IllegalArgumentException` (unchecked) |
| Method called at the wrong time | `IllegalStateException` (unchecked) |
| External resource may fail, and the caller can react | Checked (`IOException`) or a domain exception |
| Programming bug detected | Unchecked; let it surface |
| JVM out of resources | Do not catch `Error` |
| Domain rule violated (insufficient funds) | Custom exception ([04](./04_custom-exceptions.md)) |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Catching `Throwable` or `Error` in business code | Hides fatal problems | Catch specific exceptions |
| Treating all exceptions as checked or all as unchecked | Rigid or sloppy APIs | Apply the recoverable-vs-bug guideline |
| Using `RuntimeException` itself as the thrown type | Callers cannot distinguish failures | A specific subclass |
| Declaring `throws Exception` on everything | Pushes handling onto every caller | Declare specific exceptions |
| Using exceptions for normal control flow | Slow, confusing code | Conditions and return values |
| Catching a checked exception just to silence the compiler | Swallowed failures | Handle, translate or declare |
| Printing only `e.getMessage()` | Lost class and stack trace | Log the exception object |
| Confusing `Error` subclasses with `Exception`s (`NoClassDefFoundError` vs `ClassNotFoundException`) | Wrong handling | Know both ([classpath](../05-packages-and-modules/01_classpath-and-jars.md)) |

## Key takeaways

- All throwables descend from `Throwable`: `Error` (system problems) and `Exception`
- `Exception` minus `RuntimeException` is **checked** (catch or declare); `RuntimeException` and `Error` are **unchecked**
- Use checked exceptions for recoverable external failures, unchecked for bugs and violated preconditions
- Know the common ones and what they mean; `getMessage`, `getCause` and the stack trace are your diagnostics
- Exceptions are costly to create: use them for exceptional cases

**Next:** [try, catch, finally](./01_try-catch-finally.md)
