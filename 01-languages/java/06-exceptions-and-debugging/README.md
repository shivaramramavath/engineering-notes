# 06 - Exceptions and Debugging

Programs fail: files are missing, networks drop, users type nonsense, and code has bugs. **Exceptions** are Java's mechanism for signalling and handling failure. This folder covers the language mechanics, how to design good exceptions, the patterns that keep error handling clean, and how to **read stack traces and debug** when something goes wrong.

```
hierarchy & types ─► try/catch/finally ─► throw & throws ─► try-with-resources
                                                                   │
        stack traces & debugging ◄─ handling patterns ◄─ custom exceptions
```

## Prerequisites

[04-oop](../04-oop/README.md), especially [inheritance](../04-oop/06_inheritance.md) (the exception hierarchy is a class hierarchy) and [classes and objects](../04-oop/00_classes-and-objects.md). Familiarity with [methods](../02-methods/00_methods.md) and the [call stack](../02-methods/00_methods.md#what-happens-at-a-call-the-call-stack).

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 0 | [00_exception-hierarchy-and-types.md](./00_exception-hierarchy-and-types.md) | `Throwable`, `Error`, `Exception`, `RuntimeException`; checked vs unchecked |
| 1 | [01_try-catch-finally.md](./01_try-catch-finally.md) | Catching, multi-catch, `finally`, and its traps |
| 2 | [02_throw-and-throws.md](./02_throw-and-throws.md) | Throwing, declaring, chaining causes, validating arguments |
| 3 | [03_try-with-resources.md](./03_try-with-resources.md) | Automatic cleanup, `AutoCloseable`, suppressed exceptions |
| 4 | [04_custom-exceptions.md](./04_custom-exceptions.md) | When and how to define your own exceptions |
| 5 | [05_exception-handling-patterns.md](./05_exception-handling-patterns.md) | Wrapping, translation, fail fast, global handlers, anti-patterns |
| 6 | [06_stack-traces-and-debugging.md](./06_stack-traces-and-debugging.md) | Reading traces, using the debugger, a systematic debugging method |

## Practice

| After file | Try |
|------------|-----|
| 00 | Classify 15 exceptions as `Error` / checked / unchecked without looking them up |
| 01 | Predict the output and return value of five `try/catch/finally` snippets (including `return` in `finally`) |
| 02 | Write `withdraw(amount)` that validates arguments with the right exception types |
| 03 | Write a class implementing `AutoCloseable` that logs open and close; use two in one `try` and note the order |
| 04 | Create `InsufficientFundsException` carrying the balance and the requested amount |
| 05 | Take a method that swallows exceptions and rewrite it with translation, context and a single log point |
| 06 | Deliberately cause five common exceptions; for each, read the trace and fix the bug using the debugger |

**Project:** [26-projects/02-banking-system](../26-projects/02-banking-system/) uses everything here (validation, custom exceptions, resource handling).

## You are done when you can

- [ ] Say which of `NullPointerException`, `IOException`, `OutOfMemoryError`, `IllegalArgumentException` are checked, unchecked or errors, and why it matters
- [ ] Predict what `finally` does when the `try` block returns or throws
- [ ] Explain why try-with-resources replaces `try/finally` for cleanup and what "suppressed" exceptions are
- [ ] Choose between a standard exception and a custom one, and between checked and unchecked
- [ ] Wrap an exception without losing its cause
- [ ] Read a long stack trace with `Caused by:` sections and find the line in your code
- [ ] Set a conditional breakpoint and evaluate an expression in the debugger

## Key takeaways

- Exceptions separate normal logic from error handling; `Error`s are not for you to catch
- Checked exceptions must be declared or caught; unchecked ones indicate bugs or violated preconditions
- Never swallow exceptions; keep the cause when wrapping; log **or** rethrow, not both
- Use try-with-resources for anything closeable
- A stack trace is a map to the problem: read it from the top, then follow `Caused by:`

**Next:** [07-generics](../07-generics/README.md)
