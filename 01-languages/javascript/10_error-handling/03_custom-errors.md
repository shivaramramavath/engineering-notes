# JavaScript Custom Errors: Subclassing, Error.captureStackTrace & RFC 7807

> Authoritative engineering guide to authoring custom ECMAScript error classes, prototype chains, V8 `Error.captureStackTrace`, causal chaining, and RFC 7807 Problem Details serialization.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Architecture |
| **Prerequisites** | [10_error-handling/01_errors.md](01_errors.md), [05_this-and-oop/05_classes.md](../05_this-and-oop/05_classes.md) |
| **Time to Read** | 26 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Classes & Subclassing) / ES2022 (Error.cause support in super) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Subclassing the Error Class](#2-core-concepts-subclassing-the-error-class)
3. [How It Works: Engine Construction & Prototype Chains](#3-how-it-works-engine-construction--prototype-chains)
4. [Syntax & Architectural Template](#4-syntax--architectural-template)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Error.captureStackTrace & new.target](#9-engine-internals-v8-errorcapturestacktrace--newtarget)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: REST API Error Taxonomy & RFC 7807 Problem Details](#11-real-world-case-study-rest-api-error-taxonomy--rfc-7807-problem-details)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In production JavaScript applications, throwing generic `Error` instances (`throw new Error("Invalid password")`) is an architectural anti-pattern:
1. **Ambiguous Catch Filtering:** Callers cannot differentiate between transient network timeouts, operational contract violations, and fatal programmer bugs without fragile regex string parsing on `err.message`.
2. **Missing Structured Metadata:** A generic error cannot cleanly convey HTTP status codes, validation field pointers, error severity, or retry eligibility.
3. **Cluttered Stack Traces:** Generic errors capture stack frames inside internal framework helper functions rather than pointing to the caller's trigger point.

By creating a **Domain-Specific Error Taxonomy** using ES6 class inheritance, applications can:
- Categorize failures semantically (`NotFoundError`, `ValidationError`, `DatabaseTimeoutError`).
- Enable clean conditional branching using `if (err instanceof ValidationError)`.
- Attach structured JSON payloads for automated telemetry ingestion (Datadog, Sentry, AWS CloudWatch).

---

## 2. Core Concepts: Subclassing the Error Class

### 2.1 The Standard Subclass Template
Subclassing `Error` requires adhering to three fundamental mechanics:
1. **Forwarding Arguments to `super()`:** Call `super(message, options)` so the native engine initializes `message` and sets up `err.cause` (ES2022).
2. **Setting `this.name`:** Set `this.name = this.constructor.name` so logging outputs `ValidationError: ...` instead of `Error: ...`.
3. **Cleaning the Stack Trace:** Call `Error.captureStackTrace(this, this.constructor)` to strip the constructor itself from the top of the stack trace.

```javascript
class ApplicationError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = this.constructor.name;
    if (typeof Error.captureStackTrace === "function") {
      Error.captureStackTrace(this, this.constructor);
    }
  }
}
```

```
                  +-----------------------------------+
                  |         Error.prototype           |
                  +-----------------------------------+
                                    ^
                                    | [[Prototype]]
                  +-----------------------------------+
                  |      ApplicationError.prototype   |
                  +-----------------------------------+
                                    ^
                                    | [[Prototype]]
        +---------------------------+---------------------------+
        |                                                       |
+------------------------------+                +------------------------------+
|   ValidationError.prototype  |                |     NotFoundError.prototype  |
+------------------------------+                +------------------------------+
```

---

## 3. How It Works: Engine Construction & Prototype Chains

### Why ES5 Error Subclassing Was Broken:
In ES5, developers attempted to subclass errors using function constructors:
```javascript
// BROKEN in ES5:
function OldCustomError(message) {
  Error.call(this, message); // DOES NOT WORK! Error ignores 'this' and returns a new object!
}
OldCustomError.prototype = Object.create(Error.prototype);
```
In ES5, the native `Error` constructor was designed to allocate a brand new instance internally. It ignored the `this` context passed via `Error.call(this)`, resulting in an instance that lacked a valid stack trace.

### The ES6 `new.target` Resolution:
In ES6, `class CustomError extends Error` passes `new.target` down to the native `Error` constructor. V8 uses `new.target` to allocate the instance with the derived class's prototype and properly attaches the C++ native stack frame to `this`.

---

## 4. Syntax & Architectural Template

### Production-Grade Base Error Class
```javascript
class BaseDomainError extends Error {
  constructor(message, options = {}) {
    const { cause, code = "INTERNAL_ERROR", statusCode = 500, details = null } = options;
    super(message, { cause });

    this.name = this.constructor.name;
    this.code = code;
    this.statusCode = statusCode;
    this.details = details;
    this.timestamp = new Date().toISOString();

    if (typeof Error.captureStackTrace === "function") {
      Error.captureStackTrace(this, this.constructor);
    }
  }

  toJSON() {
    return {
      name: this.name,
      code: this.code,
      statusCode: this.statusCode,
      message: this.message,
      details: this.details,
      timestamp: this.timestamp
    };
  }
}
```

---

## 5. Practical Examples

### Example 1: HTTP API Error Hierarchy
```javascript
class HttpError extends Error {
  constructor(message, statusCode, options) {
    super(message, options);
    this.name = this.constructor.name;
    this.statusCode = statusCode;
    if (typeof Error.captureStackTrace === "function") {
      Error.captureStackTrace(this, this.constructor);
    }
  }
}

class BadRequestError extends HttpError {
  constructor(message = "Bad Request", options) {
    super(message, 400, options);
  }
}

class UnauthorizedError extends HttpError {
  constructor(message = "Unauthorized", options) {
    super(message, 401, options);
  }
}

class NotFoundError extends HttpError {
  constructor(message = "Resource Not Found", options) {
    super(message, 404, options);
  }
}

// Verification:
const err = new NotFoundError("User #42 not found");
console.log(err instanceof NotFoundError); // true
console.log(err instanceof HttpError);     // true
console.log(err instanceof Error);         // true
console.log(err.statusCode);               // 404
console.log(err.name);                     // "NotFoundError"
```

### Example 2: Form Field Validation Error with Structured Pointer Data
```javascript
class ValidationError extends Error {
  constructor(message, fieldErrors = []) {
    super(message);
    this.name = "ValidationError";
    this.statusCode = 422;
    this.fieldErrors = fieldErrors; // Array of { field, issue }
    if (typeof Error.captureStackTrace === "function") {
      Error.captureStackTrace(this, this.constructor);
    }
  }

  toJSON() {
    return {
      error: this.name,
      message: this.message,
      status: this.statusCode,
      errors: this.fieldErrors
    };
  }
}

// Emulating validation execution:
function validateUserRegistration(payload) {
  const issues = [];
  if (!payload.email || !payload.email.includes("@")) {
    issues.push({ field: "email", issue: "Must be a valid email address" });
  }
  if (!payload.password || payload.password.length < 8) {
    issues.push({ field: "password", issue: "Must contain at least 8 characters" });
  }

  if (issues.length > 0) {
    throw new ValidationError("User input validation failed", issues);
  }
  return true;
}

try {
  validateUserRegistration({ email: "invalid", password: "123" });
} catch (err) {
  if (err instanceof ValidationError) {
    console.log(JSON.stringify(err.toJSON(), null, 2));
  }
}
```

### Example 3: Transient Database Error with Retry Strategy
```javascript
class DatabaseError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = this.constructor.name;
    this.isRetryable = false;
  }
}

class TransientDatabaseError extends DatabaseError {
  constructor(message, options = {}) {
    super(message, options);
    this.isRetryable = true;
    this.retryAfterMs = options.retryAfterMs || 1000;
  }
}

async function executeWithRetry(operation, maxRetries = 3) {
  let attempt = 0;
  while (true) {
    try {
      return await operation();
    } catch (err) {
      attempt++;
      if (err instanceof TransientDatabaseError && attempt < maxRetries) {
        console.warn(`[Retry #${attempt}] ${err.message}. Retrying in ${err.retryAfterMs}ms...`);
        await new Promise(res => setTimeout(res, err.retryAfterMs));
        continue;
      }
      throw err; // Non-retryable or max retries exhausted
    }
  }
}
```

### Example 4: Causal Wrapping of Third-Party Library Errors
```javascript
class PaymentProviderError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = "PaymentProviderError";
    this.gatewayErrorCode = options?.gatewayErrorCode || "UNKNOWN";
  }
}

// Wrapping an Axios HTTP error:
async function processStripePayment(token, amount) {
  try {
    throw new Error("HTTP 502 Bad Gateway from Stripe Cloud");
  } catch (rawError) {
    throw new PaymentProviderError("Payment transaction rejected by banking gateway", {
      cause: rawError,
      gatewayErrorCode: "STRIPE_502"
    });
  }
}
```

### Example 5: Custom Stack Trace Trimming with `captureStackTrace`
```javascript
class InternalAssertError extends Error {
  constructor(message) {
    super(message);
    this.name = "InternalAssertError";
    // Omit constructor and assert helper from user stack trace:
    Error.captureStackTrace(this, assert);
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new InternalAssertError(message);
  }
}

function userBusinessLogic() {
  assert(1 === 2, "1 must equal 2"); // Stack trace will point directly HERE!
}

try {
  userBusinessLogic();
} catch (err) {
  console.log(err.stack.split("\n")[1]); // Shows userBusinessLogic line, NOT assert!
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 TypeScript / Babel Transpilation Breaking `instanceof`
When compiling modern TypeScript/ES6 down to ES5 (`"target": "ES5"`), TypeScript generates ES5 function constructors. As discussed in Section 3, calling `Error.apply(this)` breaks the prototype chain.
In transpiled ES5 code, `err instanceof CustomError` unexpectedly evaluates to `false`!

**Fix:** Restore the prototype chain explicitly in the constructor:
```javascript
class SafeCustomError extends Error {
  constructor(message) {
    super(message);
    // Explicitly restore prototype chain for ES5 down-leveling:
    Object.setPrototypeOf(this, new.target.prototype);
  }
}
```

### 6.2 Missing `this.name` Setting
If you omit `this.name = this.constructor.name`, the error will inherit `name: "Error"` from `Error.prototype`. When printed to console or sent to APM, it will display confusingly as:
`Error: Validation failed` instead of `ValidationError: Validation failed`.

### 6.3 Overriding `toJSON` Without Including Core Fields
If you implement a custom `toJSON()` on a custom error class, you must explicitly include `message` and `name` because they are non-enumerable on the base `Error.prototype`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Omitting `this.name`** | Subclassing without updating `name` | `this.name = this.constructor.name;` | Stack trace and string outputs will incorrectly say `Error: ...`. |
| **Omitting `super(message, options)`**| `constructor() { super(); }` | Pass message and `{ cause }` to `super()` | Bypasses native message initialization and ES2022 error chaining. |
| **String Checking in Catch** | `if (err.message.includes("404"))` | `if (err instanceof NotFoundError)` | String parsing is brittle and breaks on minor wording changes. |
| **Cluttered Stack Traces** | Leaving framework helpers in trace | Use `Error.captureStackTrace(this, fn)` | Eliminates noise and points trace directly to client call site. |
| **Using Plain Objects for Errors**| `throw { code: 400, msg: "bad" }` | `throw new BadRequestError("bad")` | Plain objects lack call stack snapshots and prototype inheritance. |
| **Catching `Error` Instead of Subclass**| Catching everything generically | Filter on specific subclass and re-throw | Swallows unintended system faults and syntax bugs. |

---

## 8. Performance Characteristics

1. **Subclass Instantiation Overhead:**
   - Instantiating a custom error class has identical performance characteristics to `new Error()` ($\approx 15\text{–}30\ \mu\text{s}$). The extra class inheritance layer adds less than $0.1\%$ overhead.
2. **`Error.captureStackTrace` Savings:**
   - Trimming stack frames using the second argument (`constructorOpt`) slightly reduces the string formatting time and memory footprint of the captured stack string.
3. **`instanceof` Resolution Speed:**
   - Checking `err instanceof CustomError` requires traversing 2–3 prototype links, which takes $\approx 2\text{ ns}$ in modern V8. It is orders of magnitude faster than regex checking error messages.

---

## 9. Engine Internals: V8 Error.captureStackTrace & new.target

In V8, when an ES6 derived class calls `super()`:
```
[User Class: new ValidationError()]
                |
                v
       (Passes new.target = ValidationError)
                |
                v
    Native C++ Error Constructor
                |
                v
Allocates JSObject with prototype = new.target.prototype
                |
                v
Records native C++ execution frame addresses
```

### V8's `Error.captureStackTrace(target, constructorOpt)`:
1. `target`: The object on which to define the `.stack` accessor.
2. `constructorOpt`: A function pointer. V8 walks the call frames from top to bottom; as soon as it reaches the frame corresponding to `constructorOpt`, it **discards all frames above it** and formats the stack string starting immediately at the caller's frame.

---

## 10. Best Practices & Design Guidelines

1. **Create a Unified Base Application Error:** Build all domain errors from a single base class (e.g., `AppError extends Error`) so middleware can detect application-level vs unknown errors.
2. **Always Forward `cause`:** Ensure custom constructors accept an `options` argument and pass `{ cause: options?.cause }` to `super()`.
3. **Implement `toJSON()` for API Errors:** Provide a standard serialization method that outputs RFC 7807 compliant JSON.
4. **Export Error Classes from Module Roots:** Expose error classes alongside the functions that throw them so callers can import and check them with `instanceof`.

---

## 11. Real-World Case Study: REST API Error Taxonomy & RFC 7807 Problem Details

### Problem
A SaaS backend returned inconsistent JSON error payloads across its 30 microservices. Some returned `{ error: "msg" }`, others returned `{ message: "msg", code: 400 }`, and some leaked raw database queries. Client applications could not reliably parse validation errors.

### Solution
Standardize an enterprise custom error taxonomy implementing **RFC 7807 Problem Details for HTTP APIs**:

```javascript
class ProblemDetailsError extends Error {
  constructor(message, options = {}) {
    super(message, { cause: options.cause });
    this.name = this.constructor.name;
    this.type = options.type || "about:blank";
    this.title = options.title || message;
    this.status = options.status || 500;
    this.detail = message;
    this.instance = options.instance || null;
    this.extensions = options.extensions || {};

    if (typeof Error.captureStackTrace === "function") {
      Error.captureStackTrace(this, this.constructor);
    }
  }

  // RFC 7807 Standard Serializer:
  toProblemDetails() {
    return {
      type: this.type,
      title: this.title,
      status: this.status,
      detail: this.detail,
      ...(this.instance && { instance: this.instance }),
      ...this.extensions
    };
  }
}

class ResourceNotFoundError extends ProblemDetailsError {
  constructor(resourceType, id, instance) {
    super(`The requested ${resourceType} with ID '${id}' was not found.`, {
      type: "https://api.example.com/errors/not-found",
      title: "Resource Not Found",
      status: 404,
      instance: instance,
      extensions: { resourceType, resourceId: id }
    });
  }
}

// API Route Handler Simulation:
function handleGetUser(userId) {
  // Simulating missing database record:
  throw new ResourceNotFoundError("User", userId, `/api/v1/users/${userId}`);
}

try {
  handleGetUser("usr_9901");
} catch (err) {
  if (err instanceof ProblemDetailsError) {
    console.log("RFC 7807 Compliant Response (HTTP Status:", err.status, "):");
    console.log(JSON.stringify(err.toProblemDetails(), null, 2));
  }
}
// Output:
// RFC 7807 Compliant Response (HTTP Status: 404 ):
// {
//   "type": "https://api.example.com/errors/not-found",
//   "title": "Resource Not Found",
//   "status": 404,
//   "detail": "The requested User with ID 'usr_9901' was not found.",
//   "instance": "/api/v1/users/usr_9901",
//   "resourceType": "User",
//   "resourceId": "usr_9901"
// }
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why was subclassing <code>Error</code> broken in ES5, and how did ES6 classes fix it?</strong></summary>

In ES5, calling `Error.call(this)` ignored the supplied `this` context and returned a newly constructed `Error` object attached to the global prototype. In ES6, class inheritance uses `new.target` to pass the derived constructor down to the native `Error` constructor. V8 uses `new.target` to allocate the instance directly with the custom subclass prototype and native stack capture.
</details>

<details>
<summary><strong>Q2: What is the purpose of passing a second argument to <code>Error.captureStackTrace(target, constructorOpt)</code>?</strong></summary>

The second argument `constructorOpt` specifies a function boundary. V8 omits all call frames up to and including `constructorOpt` from the resulting stack trace. This ensures internal constructor logic is hidden from developers and the stack trace begins cleanly at the caller's call site.
</details>

<details>
<summary><strong>Q3: Why must custom error classes explicitly set <code>this.name = this.constructor.name</code>?</strong></summary>

By default, all error instances inherit `name: "Error"` from `Error.prototype`. If you don't override `this.name`, the string output of `err.toString()` and the header of `err.stack` will read `Error: message` instead of reflecting the actual subclass name (e.g., `ValidationError: message`).
</details>

<details>
<summary><strong>Q4: Why does transpiling custom error classes to ES5 in TypeScript sometimes break <code>instanceof</code> checks?</strong></summary>

When TypeScript compiles classes to ES5, it generates traditional prototype assignments. The native `Error` constructor resets the prototype chain, causing `err instanceof CustomError` to return `false`. The fix is to add `Object.setPrototypeOf(this, new.target.prototype)` immediately after `super()` in the constructor.
</details>

<details>
<summary><strong>Q5: How should custom error classes handle the <code>Error.cause</code> feature introduced in ES2022?</strong></summary>

Custom error classes should accept an options argument in their constructor and pass it directly to `super(message, options)`. The native `Error` constructor will automatically assign `options.cause` to `this.cause`.
</details>

<details>
<summary><strong>Q6: Why is catch-filtering via <code>instanceof</code> superior to checking <code>err.code</code> or <code>err.message</code>?</strong></summary>

`instanceof` relies on the immutable prototype chain and language-level type verification. Checking strings in `err.message` is brittle and breaks if messages are updated, internationalized, or formatted differently.
</details>

<details>
<summary><strong>Q7: What is RFC 7807, and how does it relate to custom errors?</strong></summary>

RFC 7807 defines "Problem Details for HTTP APIs": a standardized JSON format for machine-readable HTTP error payloads (`type`, `title`, `status`, `detail`, `instance`). Custom error classes can implement `toProblemDetails()` to ensure REST APIs return uniform error contracts.
</details>

<details>
<summary><strong>Q8: Can custom errors define custom methods? Give an example.</strong></summary>

Yes, custom error classes are full ECMAScript classes. They can define methods like `.toJSON()`, `.isClientError()`, `.getRetryDelay()`, or `.formatForCLI()`, providing reusable domain behaviors to error handlers.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Multi-Level Error Hierarchy
Create a 3-level error hierarchy: `AppError` -> `DatabaseError` -> `QueryTimeoutError`. Ensure `instanceof` checks succeed across all 3 levels.

<details>
<summary><strong>Solution</strong></summary>

```javascript
class AppError extends Error {
  constructor(message, options) {
    super(message, options);
    this.name = this.constructor.name;
    if (Error.captureStackTrace) Error.captureStackTrace(this, this.constructor);
  }
}

class DatabaseError extends AppError {}
class QueryTimeoutError extends DatabaseError {
  constructor(message, query) {
    super(message);
    this.query = query;
  }
}

const err = new QueryTimeoutError("Query exceeded 5000ms", "SELECT * FROM users");
console.log(err instanceof QueryTimeoutError); // true
console.log(err instanceof DatabaseError);     // true
console.log(err instanceof AppError);          // true
console.log(err instanceof Error);             // true
console.log(err.name);                         // "QueryTimeoutError"
```
</details>

### Exercise 2: Safe Error Factory with Code Registry
Implement an error factory function `createErrorClass(name, defaultStatus, defaultCode)` that dynamically generates custom error classes adhering to all best practices.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function createErrorClass(className, defaultStatus, defaultCode) {
  const customClass = class extends Error {
    constructor(message, options = {}) {
      super(message, options);
      this.name = className;
      this.statusCode = options.statusCode || defaultStatus;
      this.code = options.code || defaultCode;
      if (Error.captureStackTrace) Error.captureStackTrace(this, customClass);
    }
  };

  // Set function name:
  Object.defineProperty(customClass, "name", { value: className });
  return customClass;
}

const ForbiddenError = createErrorClass("ForbiddenError", 403, "AUTH_FORBIDDEN");
const err = new ForbiddenError("You lack write permissions");

console.log(err instanceof ForbiddenError); // true
console.log(err.statusCode);                // 403
console.log(err.code);                      // "AUTH_FORBIDDEN"
console.log(err.name);                      // "ForbiddenError"
```
</details>

### Exercise 3: Error Serializer Preserving Custom Subclass Fields
Write a general-purpose function `serializeCustomError(err)` that extracts all own properties (including custom fields like `statusCode`, `fieldErrors`, etc.) alongside `name`, `message`, and `stack`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function serializeCustomError(err) {
  if (!(err instanceof Error)) return { error: String(err) };

  const base = {
    name: err.name,
    message: err.message,
    stack: err.stack,
    cause: err.cause instanceof Error ? serializeCustomError(err.cause) : err.cause
  };

  // Merge any custom properties attached to the instance:
  for (const key of Object.getOwnPropertyNames(err)) {
    if (!(key in base)) {
      base[key] = err[key];
    }
  }

  return base;
}

class CustomApiError extends Error {
  constructor(msg, trackingId) {
    super(msg);
    this.name = "CustomApiError";
    this.trackingId = trackingId;
  }
}

const sample = new CustomApiError("Network timeout", "trx_88910");
console.log(serializeCustomError(sample));
// Output: { name: 'CustomApiError', message: 'Network timeout', stack: '...', trackingId: 'trx_88910' }
```
</details>
