# Stack Traces and Debugging

When something goes wrong, you need two skills: **reading** the evidence the JVM gives you (the stack trace) and **investigating** methodically (the debugger, logging, tests). This is the most-used practical skill in day-to-day Java work.

```
Exception in thread "main" java.lang.IllegalStateException: Order 42 is already paid
    at com.example.orders.OrderService.pay(OrderService.java:31)
    at com.example.web.OrderController.handle(OrderController.java:18)
    at com.example.Main.main(Main.java:9)
```

## Reading a stack trace

A stack trace is a snapshot of the **call stack** at the moment the exception object was created ([methods: the call stack](../02-methods/00_methods.md#what-happens-at-a-call-the-call-stack)).

```
java.lang.IllegalStateException: Order 42 is already paid     ◄── 1. exception TYPE and MESSAGE
    at com.example.orders.OrderService.pay(OrderService.java:31)   ◄── 2. TOP frame: where it was created/thrown
    at com.example.web.OrderController.handle(OrderController.java:18)   ◄── who called that
    at com.example.Main.main(Main.java:9)                          ◄── BOTTOM: where the thread started
```

| Part | Meaning |
|------|---------|
| First line | Exception class and message: **what** went wrong |
| Each `at` line | `package.Class.method(File.java:line)`: a frame |
| **Top** frame | The code that threw: usually the first place to look |
| Frames below | The chain of callers, from the nearest to the original entry point |
| `Native Method` / `Unknown Source` | Native code, or classes compiled without debug info |

### Strategy

1. Read the **exception type and message** first
2. Find the **first frame in your own code** (skip `java.*`, `jdk.*`, `org.springframework.*` frames at the top)
3. Open that file and line; inspect the values involved
4. If the cause is not there, walk **down** the frames to see how bad data arrived

### `Caused by:` (chained exceptions)

```
com.example.OrderException: Cannot save order 42
    at com.example.orders.OrderService.place(OrderService.java:30)
    at com.example.web.OrderController.handle(OrderController.java:18)
    ... 3 more
Caused by: java.sql.SQLException: Connection refused
    at com.example.db.Dao.save(Dao.java:77)
    at com.example.orders.OrderService.place(OrderService.java:28)
    ... 5 more
Caused by: java.net.ConnectException: Connection refused (Connection refused)
    at java.base/sun.nio.ch.Net.connect0(Native Method)
    ...
```

- The **top** exception is what the caller saw; the **last `Caused by:`** is usually the **root cause**
- `... 5 more` means the remaining frames are identical to the enclosing exception's frames (elided to save space)
- `Suppressed:` entries show exceptions from cleanup code ([03](./03_try-with-resources.md))

**Always look at the last `Caused by:`**: it names the real problem (here, a refused database connection).

### Getting traces programmatically

```java
e.printStackTrace();                         // to System.err: fine in a quick experiment
log.error("Cannot save order {}", id, e);    // production: pass the exception as the LAST argument
StringWriter sw = new StringWriter();
e.printStackTrace(new PrintWriter(sw));      // as a String

StackTraceElement[] frames = e.getStackTrace();
Throwable root = e; while (root.getCause() != null) root = root.getCause();    // find the root cause

StackWalker.getInstance().walk(s -> s.limit(5).toList());   // inspect the current stack cheaply (Java 9+)
```

Log the **exception object**, not only `e.getMessage()` (which loses the type and trace). More: [22-production-engineering/00_logging.md](../22-production-engineering/00_logging.md).

## Common exceptions and where to look

| Exception | Typical cause | First step |
|-----------|---------------|------------|
| `NullPointerException` | A `null` was dereferenced | Read the helpful message: it names the null expression; trace where it should have been set |
| `ArrayIndexOutOfBoundsException` / `IndexOutOfBoundsException` | Off-by-one, empty collection | Print the index and length; check loop bounds (`<` vs `<=`) |
| `ClassCastException` | Wrong downcast, mixed types in a raw collection | Print `obj.getClass()` |
| `NumberFormatException` | Bad input to `parseInt` etc. | Log the offending string (and check whitespace, locale) |
| `ConcurrentModificationException` | Modifying a collection while iterating | Use `Iterator.remove()` or `removeIf` ([iterators](../08-collections/12_iterators-and-fail-fast-behavior.md)) |
| `StackOverflowError` | Missing base case or too-deep recursion | Look at the repeating frames ([recursion](../02-methods/03_recursion.md)) |
| `OutOfMemoryError` | Leak or huge data | Heap dump and profiler ([20-performance](../20-performance/README.md)) |
| `ClassNotFoundException` / `NoClassDefFoundError` | Missing dependency | [classpath](../05-packages-and-modules/01_classpath-and-jars.md) |
| `NoSuchMethodError` | Library version mismatch | `mvn dependency:tree` |
| `UnsupportedOperationException` | Modifying an immutable collection (`List.of`) | Copy to a mutable list first |
| `IllegalArgumentException` / `IllegalStateException` | A precondition failed | Read the message; check the caller |
| `SQLException` | Database problem | Read SQLState and error code; check the SQL and connection |
| `StackOverflowError` in `toString`/`hashCode`/`equals` | Cyclic references | Break the cycle |

### Reading a `NullPointerException` message

```
Cannot invoke "String.length()" because "name" is null
Cannot invoke "com.example.User.getName()" because the return value of "com.example.Repo.find(int)" is null
Cannot read field "value" because "node.next" is null
Cannot load from int array because "scores" is null
```

The message names the exact expression that was `null`. Then ask: **why** was it `null`? (never assigned, a lookup returned nothing, an optional parameter, a failed earlier step.)

## Debugging methodically

Guessing wastes time. Use a loop:

```
1. Reproduce        ──► make the failure happen on demand (a failing test or a script)
2. Isolate          ──► shrink the input and code until the smallest case still fails
3. Hypothesize      ──► "I think X is null because Y"
4. Test             ──► check with a debugger, log or assertion
5. Fix              ──► change the cause, not the symptom
6. Verify & protect ──► the failing test now passes; keep it as a regression test
```

| Tip | Why |
|-----|-----|
| Write a **failing unit test** first | Reproducible and permanent ([19-testing](../19-testing/README.md)) |
| Change **one thing at a time** | Otherwise you do not know what fixed it |
| Believe the evidence over your assumptions | The bug is usually where you "know" it cannot be |
| **Bisect**: halve the search space (comment out half, `git bisect` across commits) | Finds the culprit in log(n) steps |
| Explain the problem aloud ("rubber duck") | Often reveals the flaw |
| Read the **actual** values, not what you expect | Print or inspect them |
| Take a break | Fresh eyes spot typos |
| Check recent changes first | Most regressions come from what changed |

## The debugger

Print statements work, but a **debugger** shows the real state at any moment without editing code. All major IDEs share these features (shortcuts in [IntelliJ](../00-setup/04_intellij-idea.md)).

```java
int total = 0;
for (int i = 1; i <= 5; i++) {
    total += i * i;            // ◄── breakpoint: execution pauses here
}
System.out.println(total);
```

### Core operations

| Operation | What it does | IntelliJ (Windows/Linux) |
|-----------|--------------|--------------------------|
| **Breakpoint** | Pause when execution reaches the line | Click the gutter, `Ctrl+F8` |
| **Resume** | Run until the next breakpoint | `F9` |
| **Step over** | Run the current line; stay in this method | `F8` |
| **Step into** | Enter the method being called | `F7` |
| **Step out** | Run until the current method returns | `Shift+F8` |
| **Run to cursor** | Continue to the cursor line | `Alt+F9` |
| **Evaluate expression** | Run arbitrary code against the paused state | `Alt+F8` |
| **Variables / Watches** | See locals, fields and your own watched expressions | Debug panel |
| **Frames (call stack)** | Click a frame to see its variables | Debug panel |

### Advanced breakpoints

| Type | Use |
|------|-----|
| **Conditional** (`i == 1000`) | Stop only on the interesting iteration |
| **Hit count** | Stop on the Nth hit |
| **Logging breakpoint** ("evaluate and log", no suspend) | Print without editing code |
| **Exception breakpoint** (`NullPointerException`) | Stop **at the moment of the throw**, even if it is caught later |
| **Field watchpoint** | Stop when a field is read or modified |
| **Method breakpoint** | Stop on entry/exit |
| **Drop frame / Reset frame** | Rewind to the start of the current method and try again |

An **exception breakpoint** is the fastest way to find where a `NullPointerException` originates when the trace is long.

### Debugger pitfalls

- A paused thread can hide **timing and concurrency** bugs (use logs, thread dumps, or thread-specific suspension: [14-concurrency](../14-concurrency/README.md))
- Evaluating expressions with side effects (`list.remove(0)`) **changes** the program state
- Debugging **optimized/production** code may show variables as unavailable: compile with `-g` (Maven/Gradle default for debug info)

### Remote debugging

Attach the IDE to a running JVM (a container, a test server):

```bash
java -agentlib:jdwp=transport=dt_socket,server=y,suspend=n,address=*:5005 -jar app.jar
```

Then create a "Remote JVM Debug" configuration pointing at port 5005. Use `suspend=y` to wait for the debugger before starting. **Never expose the debug port on a public network**: it allows remote code execution. Prefer SSH tunnels or local-only binding (`address=127.0.0.1:5005`).

## Logging as a debugging tool

When you cannot attach a debugger (production, concurrency, long-running jobs), logs are your evidence.

```java
log.debug("Calculating discount for order {} with total {}", order.id(), order.total());
log.error("Payment failed for order {}", order.id(), exception);
```

| Do | Don't |
|----|-------|
| Log **identifiers** and key values (order id, user id, request id) | Log secrets, tokens or personal data |
| Use levels: `ERROR`, `WARN`, `INFO`, `DEBUG`, `TRACE` | `System.out.println` in production |
| Log the **exception object** | `log.error(e.getMessage())` |
| Add a **correlation id** across a request | Log inside tight loops at `INFO` |

Details: [22-production-engineering/00_logging.md](../22-production-engineering/00_logging.md).

## Assertions

`assert` checks **internal invariants** during development and testing. It is **disabled by default** and enabled with `-ea`.

```java
assert index >= 0 : "negative index: " + index;     // AssertionError if false (when -ea is on)
```

```bash
java -ea -jar app.jar        # enable assertions (tests usually do)
```

| Use `assert` for | Use exceptions for |
|------------------|--------------------|
| "This cannot happen" internal invariants | Validating **arguments** to public methods |
| Private-method preconditions | Anything that must run in production |
| | Anything with **side effects** inside the condition |

## Tools beyond the debugger

| Problem | Tool |
|---------|------|
| Hung or slow application | **Thread dump**: `jstack <pid>` or `jcmd <pid> Thread.print`; look for `BLOCKED`/`WAITING` and deadlock reports ([14-concurrency/14_deadlock-livelock-starvation.md](../14-concurrency/14_deadlock-livelock-starvation.md)) |
| Memory growth, `OutOfMemoryError` | **Heap dump** (`jcmd <pid> GC.heap_dump file.hprof`, `-XX:+HeapDumpOnOutOfMemoryError`) analysed with Eclipse MAT or VisualVM |
| High CPU, slow code | **Profiler** / Java Flight Recorder ([20-performance/00_profiling.md](../20-performance/00_profiling.md)) |
| Wrong dependency version | `mvn dependency:tree`, `./gradlew dependencies` |
| Which class file or JAR was loaded | `java -verbose:class` |
| Intermittent failures | Logging with timestamps and request ids; reproduce under load |
| Reading bytecode | `javap -c` ([02_how-java-works](../00-setup/02_how-java-works.md)) |

Full playbook: [22-production-engineering/05_troubleshooting-playbook.md](../22-production-engineering/05_troubleshooting-playbook.md), [04_jvm-diagnostics.md](../22-production-engineering/04_jvm-diagnostics.md).

## A debugging session, step by step

```java
public double average(List<Integer> scores) {
    int sum = 0;
    for (int s : scores) sum += s;
    return sum / scores.size();             // returns 3.0 instead of 3.5 for [3, 4]
}
```

1. **Reproduce:** `average(List.of(3, 4))` returns `3.0`; expected `3.5`
2. **Isolate:** a one-line test: `assertEquals(3.5, average(List.of(3, 4)))`
3. **Hypothesize:** "the division loses the fraction"
4. **Test:** set a breakpoint, evaluate `sum / scores.size()` (→ `3`, an `int`), evaluate `(double) sum / scores.size()` (→ `3.5`)
5. **Fix:** `return (double) sum / scores.size();` ([operators](../01-fundamentals/02_operators.md))
6. **Verify:** the test passes; keep it. Also consider the empty-list case, which would divide by zero

## Preventing bugs

| Practice | Effect |
|----------|--------|
| Small methods with one purpose | Faster isolation |
| Unit tests with edge cases | Catch regressions early |
| Immutability, `final` fields, minimal mutable state | Fewer places for state to go wrong |
| Fail-fast validation | Errors near their source ([02](./02_throw-and-throws.md)) |
| Static analysis (compiler warnings, SonarLint, SpotBugs) | Catches many bugs before running |
| Code review | A second pair of eyes |
| Clear logging | Faster diagnosis in production |

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Reading only the last line of a trace | Misdiagnosis | Read the type, the top frames, and the **last** `Caused by:` |
| Looking in library frames first | Wasted time | Find the first frame in **your** code |
| Logging only `e.getMessage()` | No type, no trace | Log the exception object |
| `printStackTrace()` left in production | Unstructured output | Use a logger |
| Fixing the symptom (adding a `null` check everywhere) | Bug returns in another form | Find why the value was `null` |
| Changing several things at once | Cannot tell what worked | One change per experiment |
| Debugging without a reproduction | Endless guessing | Write a failing test |
| `assert` with side effects or for validation | Behaves differently with and without `-ea` | Use exceptions for validation |
| Leaving a remote debug port open | Security hole | Bind locally, tunnel, disable in production |
| Ignoring `Suppressed:` and chained causes | Missing the real problem | Read every section |
| Debugging concurrency bugs by stepping | Bug disappears (Heisenbug) | Logs, thread dumps, stress tests |
| Assuming the bug is "in the framework" | Rarely true | Prove it with a minimal reproduction |

## Key takeaways

- Read a trace from the exception type and message, to the first frame in **your** code, then to the last `Caused by:`
- NPE messages tell you exactly which expression was `null`; ask why, not just where
- Debug scientifically: reproduce, isolate, hypothesize, test, fix, verify, and keep a regression test
- Use the debugger (conditional and exception breakpoints, evaluate expression) and logging; know thread and heap dumps for production
- `assert` is for internal invariants only; exceptions validate input

**Next:** [07-generics](../07-generics/README.md)