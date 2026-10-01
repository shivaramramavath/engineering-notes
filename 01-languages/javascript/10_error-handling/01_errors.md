# Errors

An **error** in JavaScript is usually an instance of `Error`, created to describe what went wrong and **thrown** to interrupt normal execution.

```js
throw new Error("Something went wrong");
```

## The `Error` object

```js
const err = new Error("Not found", { cause: originalError });

err.name;       // "Error"
err.message;    // "Not found"
err.stack;      // "Error: Not found\n    at load (app.js:10:9)\n    at ..."
err.cause;      // originalError (ES2022)
String(err);    // "Error: Not found"
```

| Property | Standard? | Purpose |
|----------|-----------|---------|
| `name` | Yes | type label, defaults to the constructor name |
| `message` | Yes | human-readable description |
| `cause` | Yes (ES2022) | the underlying error that led to this one |
| `stack` | De facto (all engines, format differs) | call stack at creation time |
| `fileName`, `lineNumber`, `columnNumber` | Non-standard | Firefox only, avoid |

The **stack is captured when the error is created**, not when it is thrown.

## Built-in error types

| Type | Thrown when | Example |
|------|-------------|---------|
| `Error` | generic base type | `new Error("x")` |
| `TypeError` | wrong type or invalid operation | `null.x`, `undefined()`, `const` reassignment |
| `ReferenceError` | using an undeclared variable or TDZ | `console.log(nope)` |
| `SyntaxError` | invalid code or data | `JSON.parse("{")`, `new RegExp("(")`, `eval("if")` |
| `RangeError` | value outside the allowed range | `new Array(-1)`, `(1).toFixed(101)`, stack overflow |
| `URIError` | bad URI escape | `decodeURIComponent("%")` |
| `EvalError` | legacy, essentially unused | |
| `AggregateError` | several errors at once | `Promise.any` when all reject |

Environment-specific:

| Type | Where |
|------|-------|
| `DOMException` | browser and Node web APIs (`AbortError`, `NotFoundError`, `QuotaExceededError`, `DataCloneError`) |
| `SystemError` with `code` (`ENOENT`, `EACCES`, `ECONNREFUSED`) | Node.js |
| `WebAssembly.CompileError`, `RuntimeError` | WebAssembly |

## Common triggers

```js
null.name;                     // TypeError: Cannot read properties of null (reading 'name')
undefined();                   // TypeError: undefined is not a function
const a = 1; a = 2;            // TypeError: Assignment to constant variable
missing;                       // ReferenceError: missing is not defined
JSON.parse("{bad");            // SyntaxError: Expected property name...
new Array(-1);                 // RangeError: Invalid array length
function f() { f(); } f();     // RangeError: Maximum call stack size exceeded
BigInt(1.5);                   // RangeError: The number 1.5 cannot be converted to a BigInt
1n + 1;                        // TypeError: Cannot mix BigInt and other types
```

## Throwing

`throw` accepts **any value**, but always throw `Error` objects (or subclasses).

```js
throw new Error("failed");     // good: has a stack and a message
throw "failed";                // bad: no stack, loses context
throw { code: 500 };           // bad: no stack, inconsistent shape
```

Throwing stops execution of the current function and unwinds the call stack until a `catch` is found.

## Wrapping with `cause`

Keep the original error while adding context.

```js
async function loadUser(id) {
  try {
    return await fetchJson(`/users/${id}`);
  } catch (err) {
    throw new Error(`Could not load user ${id}`, { cause: err });
  }
}

try {
  await loadUser(7);
} catch (err) {
  console.error(err.message);       // "Could not load user 7"
  console.error(err.cause);         // the original network or parse error
}
```

Walk the chain when logging:

```js
function* chain(err) { while (err) { yield err; err = err.cause; } }
for (const e of chain(error)) console.error(e.name, e.message);
```

## The stack trace

```
TypeError: Cannot read properties of undefined (reading 'name')
    at getName (/app/user.js:12:20)
    at render (/app/view.js:30:5)
    at async main (/app/index.js:8:3)
```

- Read from the **top**: the first line in your code is usually where to look
- `async` frames show awaited callers in modern engines
- Source maps translate bundled stacks back to original files
- `Error.captureStackTrace(obj, constructorOpt)` (V8 only) customizes stack capture
- `Error.stackTraceLimit = 50` (V8) changes the frame count

```js
console.trace("checkpoint");         // prints the current stack
new Error().stack;                   // capture without throwing
```

Stacks can leak file paths: never send raw stacks to end users.

## AggregateError

```js
try {
  await Promise.any([Promise.reject(new Error("a")), Promise.reject(new Error("b"))]);
} catch (err) {
  err instanceof AggregateError;     // true
  err.errors;                        // [Error("a"), Error("b")]
}

throw new AggregateError([err1, err2], "Multiple validation failures");
```

## Error-like values you will meet

| Value | Notes |
|-------|-------|
| `DOMException` | `instanceof Error` is `true` in modern runtimes; check `name` (`"AbortError"`) |
| Promise rejection reasons | Can be anything, including `undefined` |
| `event.error` in `window.onerror` | The thrown value |
| HTTP failures from `fetch` | `fetch` does **not** reject on 404 or 500 |

```js
const res = await fetch(url);
if (!res.ok) throw new Error(`HTTP ${res.status} ${res.statusText}`);
```

## Type-checking errors

```js
err instanceof Error;              // true for built-ins and subclasses (same realm)
err?.name === "AbortError";        // name checks work across realms
Error.isError?.(err);              // newer standard helper, cross-realm (check support)
```

`instanceof` can fail across realms (iframes, `vm` contexts). Use `name` or a `code` field for robustness.

## Error messages that help

| Good message | Poor message |
|--------------|--------------|
| `Invalid port "abc": expected an integer 1 to 65535` | `Invalid input` |
| `User 42 not found` | `Error` |
| `Cannot read config at /etc/app.json: permission denied` | `Something went wrong` |

Include: what failed, the offending value (safely), and what was expected. Do not include secrets.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Throwing strings or plain objects | No stack, inconsistent handling | `throw new Error(...)` |
| Losing the original error when rethrowing | Root cause hidden | `{ cause: err }` |
| Assuming `fetch` rejects on HTTP errors | Silent failures | Check `response.ok` |
| Empty messages | Impossible to debug | Descriptive text |
| Relying on message text for logic | Brittle, locale-dependent | Use `name`, `code`, or custom classes |
| Exposing stacks to users | Information leak | Generic message + server log |
| `instanceof` across realms | False negatives | `name` / `code` checks |
| Catching `TypeError` to hide a bug | Masks programmer errors | Fix the bug |

## Key takeaways

- Always throw `Error` instances; they carry a `message`, `name`, `stack` and optional `cause`
- Know the built-ins: `TypeError`, `ReferenceError`, `SyntaxError`, `RangeError`, `AggregateError`
- Wrap with `cause` to keep the full story
- Stack traces are for developers; never show them to end users

**Next:** [try, catch, finally](./02_try-catch-finally.md)
