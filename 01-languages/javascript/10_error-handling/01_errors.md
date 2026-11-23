# JavaScript Errors: Built-in Error Hierarchy, Error.cause & V8 Stack Traces

> Comprehensive technical reference on ECMAScript `Error` architecture, the built-in error taxonomy, `Error.cause` chaining (ES2022), `AggregateError` (ES2021), V8 stack trace capture, and APM serialization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Production Architecture |
| **Prerequisites** | [01_fundamentals/02_data-types.md](../01_fundamentals/02_data-types.md), [04_scope-and-execution/03_execution-context-and-call-stack.md](../04_scope-and-execution/03_execution-context-and-call-stack.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES3 (Error) / ES2021 (AggregateError) / ES2022 (Error.cause) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The Standard Error Taxonomy](#2-core-concepts-the-standard-error-taxonomy)
3. [How It Works: Engine Stack Trace Capture & V8 Mechanics](#3-how-it-works-engine-stack-trace-capture--v8-mechanics)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: The Cost of Capturing Stack Traces](#8-performance-characteristics-the-cost-of-capturing-stack-traces)
9. [Engine Internals: V8 CallSite Objects & stackTraceLimit](#9-engine-internals-v8-callsite-objects--stacktracelimit)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise Observability & APM Error Pipeline](#11-real-world-case-study-enterprise-observability--apm-error-pipeline)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, unexpected runtime failures, input contract violations, and asynchronous rejections are represented by instances of the **`Error`** class.

Throwing an error halts the linear execution of the current function call frame, unwinding the call stack until it encounters an enclosing `try...catch` block. If no handler is found, the exception bubbles to the top of the event loop, triggering an `uncaughtException` in Node.js or an `error` event on the browser `window`.

Modern production error handling requires understanding:
1. **The Native Taxonomy:** Using the correct built-in error class (`TypeError`, `RangeError`, `ReferenceError`) so callers can perform reliable catch-filtering.
2. **Error Chaining via `Error.cause` (ES2022):** Wrapping low-level network or database errors in high-level domain exceptions without destroying the original diagnostic stack trace.
3. **Engine-Level Stack Trace Generation:** How V8 snapshots the CPU execution frames when `new Error()` is allocated.

---

## 2. Core Concepts: The Standard Error Taxonomy

ECMAScript standardizes seven built-in subclasses inheriting from `Error.prototype`:

```
                                  Error
                                    |
     +-----------+----------+-------+-------+----------+-----------+
     |           |          |       |       |          |           |
 TypeError  RangeError  ReferenceError  SyntaxError  URIError  EvalError  AggregateError
 (ES3)       (ES3)        (ES3)          (ES3)       (ES3)     (Legacy)     (ES2021)
```

| Error Class | Triggered When | Example |
| :--- | :--- | :--- |
| **`TypeError`** | Value is not of the expected type, or an unmodifiable value is mutated | Calling a non-function `x()`, mutating `#private` |
| **`RangeError`** | A numeric value is outside its allowed mathematical or physical range | `arr.length = -1`, `(10).toFixed(200)`, stack overflow |
| **`ReferenceError`** | Accessing an undeclared identifier or a `let`/`const` inside its TDZ | `console.log(undeclaredVar)`, TDZ access |
| **`SyntaxError`** | Parsing invalid JavaScript code | `eval("foo bar")`, `JSON.parse("{")` |
| **`URIError`** | Malformed parameters passed to `encodeURI` or `decodeURIComponent` | `decodeURIComponent("%")` |
| **`AggregateError`** | Multiple independent errors wrapped into a single exception (ES2021) | `Promise.any()` when all input promises reject |
| **`EvalError`** | Legacy error representing issues in `eval()`; largely unused today | Preserved for backward compatibility |

### 2.3 Anatomy of an Error Instance
Every error instance contains standardized and de facto properties:
- `err.name`: The string identifier of the error constructor (e.g., `"TypeError"`).
- `err.message`: Human-readable description of what failed.
- `err.cause`: The underlying error that triggered this failure (ES2022).
- `err.stack`: Non-standard de facto string detailing the active call frames at allocation time.

---

## 3. How It Works: Engine Stack Trace Capture & V8 Mechanics

A critical, often misunderstood invariant in JavaScript:
**The call stack is captured at the moment `new Error()` is INSTANTIATED, not when it is THROWN.**

```
Step 1: Allocation
const err = new Error("Disk Full"); 
  |---> V8 pauses execution, traverses C++ stack frames,
        and constructs the formatted .stack string.

Step 2: Delayed Throw
setTimeout(() => {
  throw err; // Stack trace points to Step 1, NOT to setTimeout!
}, 1000);
```

### V8 Stack Trace Format:
In Chromium and Node.js, `err.stack` is a formatted string structured as:
```text
Error: Something went wrong
    at loadConfig (/app/config.js:15:9)
    at initializeServer (/app/server.js:42:5)
    at processTicksAndRejections (node:internal/process/task_queues:95:5)
```

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Creating and Chaining Errors (`Error.cause`)
Standardized in ES2022, the constructor accepts an options bag containing `cause`:
```javascript
try {
  JSON.parse(malformedJson);
} catch (syntaxErr) {
  // Wrap low-level parser error in high-level domain error:
  throw new Error("Failed to load user configuration", { cause: syntaxErr });
}
```

### 4.2 Multi-Error Bundling (`AggregateError`)
Standardized in ES2021, `AggregateError` takes an iterable of errors and a message:
```javascript
const errors = [
  new TypeError("Invalid email format"),
  new RangeError("Password must be at least 8 characters")
];

const validationFailure = new AggregateError(errors, "Validation failed for registration form");

console.log(validationFailure.message); // "Validation failed for registration form"
console.log(validationFailure.errors.length); // 2
for (const subError of validationFailure.errors) {
  console.log(` - [${subError.name}] ${subError.message}`);
}
```

### 4.3 V8 Stack Trace Customization API
In Node.js and Chromium environments:
```javascript
// Change maximum number of frames captured (Default: 10):
Error.stackTraceLimit = 25;

// Strip internal framework frames from stack trace:
class FrameworkService {
  execute() {
    const err = new Error("Execution failure");
    // Omits 'execute' frame from the stack trace:
    Error.captureStackTrace(err, FrameworkService.prototype.execute);
    throw err;
  }
}
```

---

## 5. Practical Examples

### Example 1: End-to-End Error Chaining Across Microservices
```javascript
class PaymentGatewayError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = "PaymentGatewayError";
  }
}

async function chargeCard(userId, amount) {
  try {
    // Simulating external network failure
    throw new Error("ECONNRESET: Socket closed by payment provider");
  } catch (networkErr) {
    // Preserve low-level network failure inside domain-level error:
    throw new PaymentGatewayError(`Failed to process payment of $${amount} for user #${userId}`, {
      cause: networkErr
    });
  }
}

async function handleCheckout() {
  try {
    await chargeCard(101, 49.99);
  } catch (err) {
    console.error("Top-level message:", err.message);
    console.error("Underlying root cause:", err.cause?.message);
    console.error("Full causal stack trace:\n", err.stack);
  }
}

handleCheckout();
```

### Example 2: Batch Processing with `AggregateError`
```javascript
async function executeBatchTasks(tasks) {
  const settled = await Promise.allSettled(tasks.map(t => t()));
  const failures = settled
    .filter(r => r.status === "rejected")
    .map(r => r.reason);

  if (failures.length > 0) {
    throw new AggregateError(failures, `Batch processing failed: ${failures.length} tasks encountered errors.`);
  }

  return settled.map(r => r.value);
}

// Running batch:
const jobs = [
  () => Promise.resolve("Job 1 OK"),
  () => Promise.reject(new Error("Job 2 DB timeout")),
  () => Promise.reject(new Error("Job 3 Permission denied"))
];

executeBatchTasks(jobs).catch(err => {
  if (err instanceof AggregateError) {
    console.log(err.message);
    err.errors.forEach(e => console.log("Sub-error:", e.message));
  }
});
```

### Example 3: Sanitizing Stack Traces to Remove Absolute File Paths
```javascript
function sanitizeStackTrace(error) {
  if (!error.stack) return "";
  
  // Replace absolute host paths (e.g. C:\Users\... or /home/ubuntu/app/...) with project relative paths:
  const cwd = process.cwd().replace(/\\/g, "/");
  const normalized = error.stack.replace(/\\/g, "/");
  return normalized.replaceAll(cwd, ".");
}

const err = new Error("Diagnostic trace");
console.log(sanitizeStackTrace(err));
```

### Example 4: Type-Specific Catch Filtering
```javascript
function processInput(data) {
  try {
    if (typeof data !== "string") throw new TypeError("Expected string input");
    if (data.length === 0) throw new RangeError("String cannot be empty");
    JSON.parse(data);
  } catch (err) {
    if (err instanceof TypeError) {
      console.warn("[Recoverable] Bad client type:", err.message);
    } else if (err instanceof RangeError) {
      console.warn("[Recoverable] Bounds error:", err.message);
    } else if (err instanceof SyntaxError) {
      console.error("[Fatal] Corrupt JSON payload:", err.message);
    } else {
      throw err; // Re-throw unknown exceptions!
    }
  }
}
```

### Example 5: JSON Serialization for Error Objects
```javascript
function serializeError(err) {
  return {
    name: err.name,
    message: err.message,
    stack: err.stack,
    cause: err.cause instanceof Error ? serializeError(err.cause) : err.cause
  };
}

const original = new Error("DNS lookup timeout");
const appErr = new Error("API service unreachable", { cause: original });

console.log(JSON.stringify(serializeError(appErr), null, 2));
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `JSON.stringify(new Error())` Evaluates to `{}`
In standard JavaScript, the `name`, `message`, and `stack` properties of `Error` instances are defined with `enumerable: false`. Because `JSON.stringify()` strictly serializes enumerable properties, passing an error directly returns an empty object:
```javascript
const err = new Error("Something broke");
console.log(JSON.stringify(err)); // "{}" (Silent information loss!)
```
**Fix:** Always use an explicit serialization helper or custom `toJSON()` method.

### 6.2 Throwing Non-Errors (`throw "string"`)
JavaScript allows throwing *any* value (`throw "oops"`, `throw 404`, `throw null`).
Throwing primitives is an anti-pattern:
- Primitives do not capture a stack trace.
- Downstream catch blocks expecting `err.message` will evaluate to `undefined` or crash.
```javascript
// DANGEROUS:
// throw "Database disconnected"; 

// SAFE:
throw new Error("Database disconnected");
```

### 6.3 Stack Trace Memory Retention
Because stack traces capture local execution frame snapshots, retaining error instances in global arrays or long-lived caches can keep memory referenced in stack frames alive, causing unexpected memory retention.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Throwing primitive strings** | `throw "User not found";` | `throw new Error("User not found");` | Strings have no `.stack`, `.name`, or `.cause` properties. |
| **Direct `JSON.stringify(err)`** | `logger.info(JSON.stringify(err))` | Extract `name`, `message`, `stack` | Error properties are non-enumerable; produces `{}`. |
| **Swallowing the Root Cause** | `catch (e) { throw new Error("Failed"); }` | Pass `{ cause: e }` | Destroys the original root stack trace and error diagnostics. |
| **Catching without Re-throwing** | `catch (e) { console.log(e); }` | Re-throw unexpected error types | Silently swallows programmer bugs and syntax errors. |
| **Using `new Error()` without throwing** | `new Error("Validation failed");` | `throw new Error("Validation failed");` | Instantiating an error does nothing unless thrown or passed. |
| **Overriding `err.stack` carelessly** | `err.stack = "custom"` | Use `Error.captureStackTrace` | Breaks stack trace line formatting expected by APM tools. |

---

## 8. Performance Characteristics: The Cost of Capturing Stack Traces

1. **Instantiation Latency:**
   - Instantiating a plain object `{ message: "failed" }` takes $\approx 5\text{ ns}$.
   - Instantiating `new Error("failed")` takes **$15,000\text{–}50,000\text{ ns}$ ($15\text{–}50\ \mu\text{s}$)** because V8 must pause, walk the native C++ and bytecode execution frames, resolve source line maps, and allocate string descriptors.
2. **Never Use Exceptions for Normal Control Flow:**
   - Throwing and catching errors in tight loops causes TurboFan JIT deoptimization and severely degrades throughput. Reserve errors strictly for **exceptional conditions**.
3. **Limiting Stack Depth:**
   - In production microservices with deep async call stacks, setting `Error.stackTraceLimit = 10` prevents excessive memory allocation during high-volume error spikes.

---

## 9. Engine Internals: V8 CallSite Objects & stackTraceLimit

Inside V8, stack trace formatting is deferred until the `.stack` property is first accessed (lazy evaluation):

```
 new Error("msg")
        |
        v
 V8 captures raw CallSite frame addresses (C++ lightweight array)
        |
        v
 User accesses err.stack
        |
        v
 V8 invokes Error.prepareStackTrace(err, structuredStackTrace)
        |
        v
 Formats final stack string with line and column numbers
```

### Customizing V8 CallSite Formatting:
In Node.js, you can customize stack trace formatting globally via `Error.prepareStackTrace`:
```javascript
Error.prepareStackTrace = (error, structuredStackTrace) => {
  return structuredStackTrace.map(callSite => ({
    file: callSite.getFileName(),
    func: callSite.getFunctionName(),
    line: callSite.getLineNumber(),
    col: callSite.getColumnNumber()
  }));
};
```

---

## 10. Best Practices & Design Guidelines

1. **Always Chain Errors with `Error.cause`:** Whenever translating lower-level exceptions (HTTP/DB) into high-level business exceptions, attach `{ cause: originalError }`.
2. **Use Built-in Error Types for Semantics:** Throw `TypeError` for bad parameter types and `RangeError` for out-of-bounds indices.
3. **Always Throw Error Instances:** Never throw strings, numbers, or plain objects.
4. **Implement Structured Logging:** Serialize `err.name`, `err.message`, `err.stack`, and recursive `err.cause` chains when sending logs to Datadog or Sentry.

---

## 11. Real-World Case Study: Enterprise Observability & APM Error Pipeline

### Problem
An enterprise Node.js microservice processed 5,000 requests per second. When database outages occurred, service logs flooded with unhelpful messages: `Error: Request failed`. The original PostgreSQL connection errors were swallowed inside catch blocks, preventing engineers from diagnosing root causes in Datadog APM.

### Solution
Implement an enterprise-wide causal error pipeline utilizing `Error.cause` and structured APM extraction:

```javascript
class BaseAppError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = this.constructor.name;
    this.timestamp = Date.now();
  }

  toAPMPayload() {
    const causalChain = [];
    let current = this;

    while (current) {
      causalChain.push({
        type: current.name,
        message: current.message,
        stack: current.stack
      });
      current = current.cause instanceof Error ? current.cause : null;
    }

    return {
      errorType: this.name,
      errorMessage: this.message,
      chainLength: causalChain.length,
      causes: causalChain
    };
  }
}

class DatabaseQueryError extends BaseAppError {}
class UserProfileServiceError extends BaseAppError {}

// Simulating layered service execution:
async function executeDbQuery() {
  throw new Error("FATAL: connection pool exhausted (port 5432)");
}

async function fetchUserProfile(userId) {
  try {
    await executeDbQuery();
  } catch (dbErr) {
    throw new DatabaseQueryError(`DB query failed for user #${userId}`, { cause: dbErr });
  }
}

async function handleControllerRequest(userId) {
  try {
    await fetchUserProfile(userId);
  } catch (serviceErr) {
    const appErr = new UserProfileServiceError("User service unavailable", { cause: serviceErr });
    
    // Send structured causal chain to APM:
    const payload = appErr.toAPMPayload();
    console.log("Structured APM Telemetry:\n", JSON.stringify(payload, null, 2));
  }
}

handleControllerRequest(42);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: When exactly is an error's stack trace captured in JavaScript?</strong></summary>

The stack trace is captured at the moment the `new Error()` constructor is executed, **not** when the `throw` statement is executed. If an error is allocated and thrown later (or inside a callback), its stack trace reflects the execution call stack at the time of allocation.
</details>

<details>
<summary><strong>Q2: Why does <code>JSON.stringify(new Error("oops"))</code> output <code>{}</code>?</strong></summary>

The properties of standard `Error` instances (`name`, `message`, `stack`) are configured with `enumerable: false`. Because `JSON.stringify()` only serializes own enumerable properties, standard error instances serialize into empty JSON objects unless custom replacer functions or serialization methods are used.
</details>

<details>
<summary><strong>Q3: What was introduced in ECMAScript 2022 to standardize error chaining?</strong></summary>

ES2022 standardized the `Error.cause` property. The `Error` constructor now accepts an optional options object as its second argument: `new Error("Failure", { cause: originalError })`. This preserves the complete causal chain and stack traces of nested exceptions.
</details>

<details>
<summary><strong>Q4: What is <code>AggregateError</code> and when was it introduced?</strong></summary>

Introduced in ECMAScript 2021, `AggregateError` represents a single error that wraps multiple independent underlying errors. It is primarily used by `Promise.any()` when all input promises reject, storing the array of individual rejections in its `.errors` property.
</details>

<details>
<summary><strong>Q5: What is the purpose of V8's <code>Error.captureStackTrace(targetObject, constructorOpt)</code>?</strong></summary>

It is a V8-specific API used when authoring custom error classes. It creates a `.stack` property on `targetObject`. The second argument `constructorOpt` hides all stack frames up to and including that constructor function, ensuring internal framework plumbing does not clutter the user-visible stack trace.
</details>

<details>
<summary><strong>Q6: Why is using exceptions for normal application control flow an anti-pattern?</strong></summary>

Exceptions are performance-heavy: capturing stack traces requires walking C++ frames and allocating string descriptors ($1000\times$ slower than returning values). Furthermore, throwing exceptions causes JIT compilers like TurboFan to deoptimize the containing functions, forcing execution back to the slow bytecode interpreter.
</details>

<details>
<summary><strong>Q7: What is the difference between a <code>TypeError</code> and a <code>RangeError</code>?</strong></summary>

A `TypeError` occurs when a value is not of the expected data type or when attempting an invalid operation on a value (e.g., calling `null()`). A `RangeError` occurs when a numeric value is of the correct type, but outside its permissible mathematical bounds (e.g., setting `array.length = -1` or passing `100` to `toFixed(20)`).
</details>

<details>
<summary><strong>Q8: What does <code>Error.stackTraceLimit</code> control in Node.js?</strong></summary>

`Error.stackTraceLimit` is a numeric property in V8 (defaulting to 10) that dictates how many stack frames the engine records when generating an error's stack trace. Setting it to `Infinity` records the entire call stack; setting it to `0` disables stack trace capture completely for maximum performance.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Recursive Error Cause Unroller
Write a function `unrollErrorCauses(error)` that traverses an error's `cause` chain and returns an array of all error messages from the outermost wrapper down to the root cause.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function unrollErrorCauses(err) {
  const chain = [];
  let curr = err;

  while (curr) {
    chain.push(curr.message || String(curr));
    curr = curr.cause;
  }

  return chain;
}

const root = new Error("Database timeout");
const mid = new Error("User query failed", { cause: root });
const top = new Error("HTTP 500: Internal Server Error", { cause: mid });

console.log(unrollErrorCauses(top));
// Output: [ 'HTTP 500: Internal Server Error', 'User query failed', 'Database timeout' ]
```
</details>

### Exercise 2: Safe Async Function Wrapper (Golang / Tuple Style)
Implement a wrapper `toSafeAsync(promise)` that resolves to `[null, data]` on success and `[error, null]` on failure, eliminating the need for `try...catch` blocks.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function toSafeAsync(promise) {
  try {
    const data = await promise;
    return [null, data];
  } catch (err) {
    const errorInstance = err instanceof Error ? err : new Error(String(err));
    return [errorInstance, null];
  }
}

// Verification:
async function test() {
  const [err, user] = await toSafeAsync(Promise.resolve({ id: 101 }));
  if (err) console.error("Error:", err.message);
  else console.log("User:", user.id); // 101

  const [failErr, failData] = await toSafeAsync(Promise.reject(new Error("Network dead")));
  if (failErr) console.log("Caught cleanly:", failErr.message); // "Caught cleanly: Network dead"
}

test();
```
</details>

### Exercise 3: Custom Error Deserializer
Write a function `deserializeError(plainObject)` that takes a serialized error object (with `name`, `message`, `stack`, and optional `cause`) and reconstructs a proper native Error instance with matching prototypes.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function deserializeError(payload) {
  const errorMap = {
    TypeError,
    RangeError,
    ReferenceError,
    SyntaxError,
    URIError,
    Error
  };

  const Constructor = errorMap[payload.name] || Error;
  const options = payload.cause ? { cause: deserializeError(payload.cause) } : undefined;
  const err = new Constructor(payload.message, options);
  
  if (payload.stack) {
    err.stack = payload.stack;
  }
  return err;
}

const serialized = {
  name: "TypeError",
  message: "Invalid token",
  cause: { name: "Error", message: "Signature expired" }
};

const reconstructed = deserializeError(serialized);
console.log(reconstructed instanceof TypeError); // true
console.log(reconstructed.cause instanceof Error); // true
console.log(reconstructed.cause.message); // "Signature expired"
```
</details>
