# try, catch, finally

`try` marks code that might throw; `catch` handles a specific exception type; `finally` runs cleanup code **whether or not** an exception occurred.

```java
try {
    int n = Integer.parseInt(text);                 // may throw
    System.out.println("Parsed " + n);
} catch (NumberFormatException e) {                 // handle one type
    System.out.println("Not a number: " + text);
} finally {
    System.out.println("Done");                     // always runs
}
```

## Control flow

```
try block ──── no exception ────────────────────────────┐
   │                                                     ▼
   └── exception thrown ──► matching catch? ── yes ─► catch block ──┐
                               │ no                                  ▼
                               └───────────────────► (propagates) ─► finally ─► continue / propagate
```

| Situation | What runs |
|-----------|-----------|
| No exception | `try` → `finally` → code after |
| Exception caught | `try` (up to the throw) → matching `catch` → `finally` → code after |
| Exception **not** caught | `try` (up to the throw) → `finally` → exception propagates to the caller |
| Exception inside `catch` | `finally` still runs, then the new exception propagates |

The statements in the `try` block **after** the throwing line are skipped.

## Valid combinations

```java
try { } catch (X e) { }                  // try + catch
try { } catch (X e) { } finally { }      // try + catch + finally
try { } finally { }                      // try + finally (no catch): exceptions still propagate
try (Resource r = open()) { }            // try-with-resources, catch/finally optional
```

A `try` with neither `catch` nor `finally` (nor resources) is a compile error.

## Multiple `catch` blocks

Order from **most specific to most general**. The first matching block wins.

```java
try {
    process();
} catch (FileNotFoundException e) {        // subclass first
    System.out.println("missing file");
} catch (IOException e) {                  // then its parent
    System.out.println("I/O problem");
} catch (Exception e) {                    // broadest last (and only if you really need it)
    System.out.println("unexpected");
}
```

Putting a superclass before its subclass is a **compile error** (`exception FileNotFoundException has already been caught`).

### Multi-catch (`|`)

When several unrelated exceptions get the same handling:

```java
try {
    parseAndSave(input);
} catch (NumberFormatException | IllegalStateException e) {
    log.warn("bad request", e);
}
```

- The alternatives must **not** be subclasses of each other (`IOException | FileNotFoundException` does not compile)
- `e` is implicitly `final`; its static type is the closest common supertype

### Variable scope

```java
try {
    int value = compute();                  // visible only inside this try block
} catch (Exception e) {
    // value is not visible here
}

int value;                                  // declare outside if you need it afterwards or in finally
try { value = compute(); } catch (Exception e) { value = -1; }
```

## `finally`

Runs after the `try`/`catch` regardless of the outcome: the place for cleanup that is not covered by try-with-resources ([03](./03_try-with-resources.md)).

```java
Lock lock = new ReentrantLock();
lock.lock();
try {
    updateSharedState();
} finally {
    lock.unlock();                         // released even if updateSharedState throws
}
```

### When `finally` does **not** run

| Case | |
|------|--|
| `System.exit(...)` is called in `try` or `catch` | The JVM stops |
| The JVM crashes or the process is killed | |
| The thread dies or loops forever inside `try` | |
| Power or OS failure | |

### `finally` and `return`

The `finally` block runs **before** the method actually returns.

```java
static int a() {
    try { return 1; }
    finally { System.out.println("finally"); }      // prints "finally", returns 1
}

static int b() {
    int x = 1;
    try { return x; }                               // the value 1 is already saved for return
    finally { x = 2; }                              // does not change the returned value
}                                                   // returns 1

static int c() {
    try { return 1; }
    finally { return 2; }                           // OVERRIDES: returns 2 (and discards any exception!)
}

static int d() {
    try { throw new RuntimeException("lost"); }
    finally { return 3; }                           // the exception vanishes: returns 3
}
```

**Never `return`, `break`, `continue` or `throw` from `finally`.** It silently swallows exceptions and overrides results (the compiler may warn: *finally clause cannot complete normally*).

### Exceptions thrown from `finally`

```java
try {
    throw new IOException("first");
} finally {
    throw new RuntimeException("second");      // "first" is lost
}
```

Keep `finally` code simple and safe; wrap risky cleanup in its own `try/catch`. Try-with-resources handles this correctly by recording the second exception as **suppressed** ([03](./03_try-with-resources.md)).

## Predict the output

```java
static void run() {
    try {
        System.out.println("A");
        throw new IllegalStateException();
    } catch (IllegalStateException e) {
        System.out.println("B");
        return;
    } finally {
        System.out.println("C");
    }
}
// run() prints: A B C
```

```java
try {
    try { throw new RuntimeException("inner"); }
    finally { System.out.println("inner finally"); }
} catch (RuntimeException e) {
    System.out.println("caught " + e.getMessage());
}
// inner finally
// caught inner
```

## Rethrowing

```java
try {
    process();
} catch (IOException e) {
    log.error("failed", e);
    throw e;                                     // rethrow the same exception
}

// Wrap with context, keeping the cause
} catch (IOException e) {
    throw new DataLoadException("Cannot load " + path, e);
}
```

Precise rethrow: if `catch (Exception e) { throw e; }` and the `try` block can only throw `IOException`, the compiler accepts `throws IOException` on the method. See [02](./02_throw-and-throws.md) and [05](./05_exception-handling-patterns.md).

## What to catch

| Rule | Explanation |
|------|-------------|
| Catch **specific** exceptions | `catch (Exception e)` also swallows bugs such as `NullPointerException` |
| Catch where you can **do something useful**: recover, retry, translate, report | Otherwise let it propagate |
| Do not catch `Throwable` or `Error` | System failures ([00](./00_exception-hierarchy-and-types.md)) |
| Do not leave `catch` blocks **empty** | At minimum log with the exception, or comment why ignoring is safe |
| Keep `try` blocks **small** | You know exactly which call failed |
| Do not use `catch` for normal control flow | Use conditions |

```java
// Smaller try: the cause is unambiguous
String text;
try { text = Files.readString(path); }
catch (IOException e) { throw new ConfigException("cannot read " + path, e); }
int port = Integer.parseInt(text.strip());       // let a NumberFormatException surface or handle it separately
```

## Special case: `InterruptedException`

```java
try {
    Thread.sleep(1000);
} catch (InterruptedException e) {
    Thread.currentThread().interrupt();          // restore the interrupt flag
    return;                                      // and stop what you are doing
}
```

Never swallow it: the interrupt is a request to stop ([14-concurrency/00_threads-and-lifecycle.md](../14-concurrency/00_threads-and-lifecycle.md)).

## Performance

A `try` block costs nothing when no exception occurs. **Throwing** is the expensive part (stack trace capture). Do not use exceptions for flow that occurs frequently:

```java
// Bad: exception per invalid input in a hot loop
try { n = Integer.parseInt(s); } catch (NumberFormatException e) { n = 0; }

// Better when most inputs are invalid: check first
n = looksNumeric(s) ? Integer.parseInt(s) : 0;
```

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Empty `catch` block | Failures disappear | Log with the exception, or rethrow |
| `catch (Exception e)` everywhere | Bugs masked | Specific types |
| Superclass `catch` before subclass | Compile error | Specific first |
| `return` inside `finally` | Overrides results, swallows exceptions | Remove it |
| Throwing from `finally` | Original exception lost | Keep `finally` safe; use try-with-resources |
| Using variables from `try` in `catch` or `finally` | `cannot find symbol` | Declare outside the `try` |
| Catching, logging, and rethrowing at every layer | Duplicate log entries | Log once, at the boundary ([05](./05_exception-handling-patterns.md)) |
| Forgetting that statements after the throwing line are skipped | Partially updated state | Order operations safely; undo or use transactions |
| `catch (InterruptedException e) { }` | Thread cannot be stopped | Restore the flag |
| Cleanup in `finally` for closeable resources | Verbose and error-prone | try-with-resources |
| `e.printStackTrace()` in production | Unstructured output | Use a logger ([22-production-engineering/00_logging.md](../22-production-engineering/00_logging.md)) |

## Key takeaways

- `catch` blocks are tried in order: specific before general; multi-catch (`A | B`) shares handling for unrelated types
- `finally` runs on every path (except JVM exit); never `return` or `throw` from it
- A `return` value is fixed before `finally` runs, unless `finally` itself returns (do not do that)
- Keep `try` blocks small, catch only what you can handle, never leave `catch` empty
- Restore the interrupt flag when catching `InterruptedException`

**Next:** [throw and throws](./02_throw-and-throws.md)
