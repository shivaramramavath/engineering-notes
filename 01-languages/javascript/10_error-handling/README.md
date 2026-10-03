# 10 · Error Handling

Things go wrong: bad input, failed network calls, missing files, programmer mistakes. Good error handling makes failures **visible, understandable and recoverable** instead of silent or catastrophic.

```
throw ──► propagate up the call stack ──► catch ──► recover / translate / report
```

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [Errors](./01_errors.md) | The `Error` object, built-in types, `stack`, `cause`, throwing non-errors |
| 2 | [try, catch, finally](./02_try-catch-finally.md) | Control flow, optional catch binding, async errors |
| 3 | [Custom Errors](./03_custom-errors.md) | Subclassing, error codes, serialization, type checks |
| 4 | [Error Propagation](./04_error-propagation.md) | Where to catch, wrapping, rethrowing, Result values, promises |
| 5 | [Production Error Handling](./05_production-error-handling.md) | Global handlers, logging, monitoring, graceful shutdown, user messages |

## Two kinds of errors

| | Operational errors | Programmer errors |
|---|--------------------|-------------------|
| Cause | Environment and input: network down, file missing, invalid form, timeout | Bugs: `undefined` property access, wrong argument type, broken invariant |
| Expected? | Yes, plan for them | No, fix the code |
| Response | Handle, retry, inform the user | Log, crash or abort the operation, fix the bug |

## Goal

By the end you can throw meaningful errors, catch them at the right level, design your own error types, and keep a production app observable when things fail.

## Prerequisites

- [Promises](../11_asynchronous-javascript/03_promises.md) and [async/await](../11_asynchronous-javascript/05_async-await.md) (read alongside the async parts)
- [Classes and inheritance](../05_this-and-oop/06_inheritance.md)

**Next:** [Errors](./01_errors.md)
