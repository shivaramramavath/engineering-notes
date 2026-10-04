# try-with-resources

Many objects hold something that must be **released**: files, sockets, database connections, locks, native memory. **try-with-resources** (Java 7) closes them automatically, in the right order, even when exceptions occur, and without losing exceptions.

```java
try (BufferedReader reader = Files.newBufferedReader(path)) {
    String line = reader.readLine();
    System.out.println(line);
}                                         // reader.close() is called here, always
```

## The problem it solves

```java
// Old style: verbose and subtly wrong
BufferedReader reader = null;
try {
    reader = Files.newBufferedReader(path);
    return reader.readLine();
} finally {
    if (reader != null) {
        try { reader.close(); }            // close() may throw too...
        catch (IOException ignored) { }    // ...and swallow, or mask the original exception
    }
}
```

Problems: noise, easy to forget, and an exception from `close()` can **replace** the original exception from the body. With several resources it nests quickly.

## Syntax

```java
try (Resource1 a = open1(); Resource2 b = open2()) {      // declared in the parentheses, separated by ;
    // use a and b
} catch (IOException e) {                                  // optional: also covers exceptions from opening and closing
    ...
} finally {                                                // optional: runs AFTER the resources are closed
    ...
}
```

| Rule | Detail |
|------|--------|
| Resource type | Must implement `AutoCloseable` (or its sub-interface `Closeable`) |
| Resource variables are implicitly **`final`** | Cannot be reassigned in the block |
| Java 9+ | You may reuse an existing **effectively final** variable: `try (reader) { ... }` |
| `null` resources | Allowed: skipped, no `NullPointerException` on close |
| `catch`/`finally` | Optional; if present they run **after** all resources are closed |

## Order of closing

Resources are closed in **reverse order** of declaration (like a stack), before any `catch` or `finally` runs.

```java
try (Res a = new Res("A"); Res b = new Res("B")) {
    System.out.println("body");
} catch (Exception e) {
    System.out.println("catch");
} finally {
    System.out.println("finally");
}
```

Output (assuming `Res` prints on open and close):

```
open A
open B
body
close B       ◄── reverse order
close A
finally
```

If opening `B` fails, `A` is still closed. Dependencies should be declared first:

```java
try (FileInputStream in = new FileInputStream(file);
     BufferedInputStream buffered = new BufferedInputStream(in)) { ... }     // buffered closes first, then in
```

Closing a **wrapper** (`BufferedReader`) normally closes the stream it wraps, so declaring just the outer one is usually enough: `try (BufferedReader r = new BufferedReader(new FileReader(f)))`. (Be careful if the wrapper's constructor can throw after the inner resource is opened: declaring both separately is safest.)

## Suppressed exceptions

If both the body and `close()` throw, the **body's exception is the one thrown**; the one from `close()` is attached to it as a **suppressed** exception, so nothing is lost.

```java
class Faulty implements AutoCloseable {
    @Override public void close() { throw new IllegalStateException("close failed"); }
}

try (Faulty f = new Faulty()) {
    throw new RuntimeException("body failed");
} catch (RuntimeException e) {
    System.out.println(e.getMessage());                       // body failed
    for (Throwable s : e.getSuppressed())
        System.out.println("suppressed: " + s.getMessage());  // suppressed: close failed
}
```

The stack trace also prints them:

```
java.lang.RuntimeException: body failed
    at ...
    Suppressed: java.lang.IllegalStateException: close failed
        at ...
```

If only `close()` throws, that exception propagates normally. With `try/finally`, the `close()` exception would have **replaced** the original one ([01](./01_try-catch-finally.md)).

## `AutoCloseable` vs `Closeable`

| | `AutoCloseable` | `Closeable` |
|---|-----------------|-------------|
| Package | `java.lang` | `java.io` |
| `close()` throws | `Exception` | `IOException` |
| Idempotent `close()` required | Strongly advised | **Yes**: calling twice must have no effect |
| Extends | n/a | `AutoCloseable` |

## Implementing your own

```java
public class Connection implements AutoCloseable {
    private boolean open = true;

    public void send(String msg) {
        if (!open) throw new IllegalStateException("connection closed");
        ...
    }

    @Override
    public void close() {                       // you may narrow the throws clause, even drop it
        if (!open) return;                      // idempotent
        open = false;
        System.out.println("closed");
    }
}

try (Connection c = new Connection()) {
    c.send("hello");
}
```

Guidelines for `close()`:
- Make it **idempotent**
- Prefer **not** declaring `throws Exception`: it forces every caller to catch it. Declare only what you really throw (or nothing)
- Avoid throwing `InterruptedException`; it is a bad fit for resources. If your cleanup can be interrupted, restore the flag instead
- Release in a robust order, and do not let one failure skip the rest

## Common resources

| Resource | Notes |
|----------|-------|
| `InputStream`, `OutputStream`, `Reader`, `Writer` | The classic case ([11-io-and-networking](../11-io-and-networking/README.md)) |
| `Scanner` | Closing it closes the wrapped source (do not close one wrapping `System.in` unless done with input) |
| `Connection`, `Statement`, `ResultSet` (JDBC) | Always twr; closing a `Connection` closes its statements ([16-jdbc-and-databases](../16-jdbc-and-databases/README.md)) |
| Streams from `Files.lines`, `Files.walk`, `Files.list`, `Files.newDirectoryStream` | **Must** be closed: they hold file handles |
| `ExecutorService` | `AutoCloseable` since Java 19 (waits for tasks, then shuts down) |
| `HttpClient` | `AutoCloseable` since Java 21 |
| `FileChannel`, `Socket`, `ServerSocket`, `ZipFile`, `JarFile` | Yes |
| Locks | `Lock` is **not** `AutoCloseable`: use `try/finally { lock.unlock(); }` |

```java
try (Stream<String> lines = Files.lines(path)) {                  // closes the file when done
    long count = lines.filter(l -> l.contains("ERROR")).count();
}

try (Connection con = dataSource.getConnection();
     PreparedStatement ps = con.prepareStatement(SQL)) {
    ps.setInt(1, id);
    try (ResultSet rs = ps.executeQuery()) {
        while (rs.next()) { ... }
    }
}
```

## try-with-resources and `return`

The resource is closed **after** the return value is computed but **before** control returns to the caller:

```java
String firstLine(Path p) throws IOException {
    try (BufferedReader r = Files.newBufferedReader(p)) {
        return r.readLine();           // value evaluated, then r is closed, then returned
    }
}
```

Never return a lazily evaluated object that depends on a resource that gets closed (for example, returning a `Stream` from inside `try (Files.lines(...))` leaves the caller with a closed stream).

## When resources have no `AutoCloseable` type

```java
Lock lock = ...;
lock.lock();
try { ... } finally { lock.unlock(); }

// Adapt anything with a lambda
try (AutoCloseable cleanup = () -> restoreState()) { ... }
```

Because `AutoCloseable` has a single abstract method, a lambda or method reference works as a one-off cleanup action (and `close()` still declares `Exception`, so catch or declare accordingly).

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Manual `close()` in `finally` | Verbose, leaks on error paths, masks exceptions | try-with-resources |
| Not closing `Files.lines`/`walk`/`list` streams | File handle leaks, "too many open files" | `try (Stream<...> s = ...)` |
| Declaring the resource outside and using it after the block | `IllegalStateException`/`ClosedChannelException` | Keep all use inside the block |
| Returning a lazy object that needs the closed resource | Empty or failing results | Materialize inside (`toList()`), then return |
| Assuming `catch` runs before the resource is closed | Resource accessed in `catch` is already closed | Resources close first; handle in the block |
| `close()` that is not idempotent | Errors on double close | Guard with a flag |
| `close()` declared `throws Exception` | Callers must catch `Exception` | Narrow the signature |
| Ignoring suppressed exceptions while debugging | Missing half the story | Check `getSuppressed()` or the full trace |
| Closing `System.in`/`System.out` by wrapping them | Later reads/writes fail | Do not close the standard streams |
| Opening several resources in a single expression that might throw halfway | Leak of the first | Declare each as its own resource |
| Using `Lock` in try-with-resources | Does not compile | `try/finally` with `unlock()` |

## Key takeaways

- Use try-with-resources for **anything** `AutoCloseable`: it closes in reverse order, on every path
- If the body and `close()` both throw, the body's exception wins and the other is **suppressed**
- Resources are closed **before** `catch`/`finally` run
- Implement `AutoCloseable` for your own resource types, with an idempotent `close()`
- Streams from `Files.lines`/`walk`/`list`, JDBC objects, executors and HTTP clients need closing too

**Next:** [Custom Exceptions](./04_custom-exceptions.md)
