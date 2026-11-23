# JavaScript Production Error Handling: Operational vs Programmer Errors, PII Sanitization & Resilience

> Complete architectural reference on production error engineering, operational vs programmer errors, centralized Express/Fastify middleware, PII sanitization, circuit breakers, and APM telemetry pipelines.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Production Architecture |
| **Prerequisites** | [10_error-handling/03_custom-errors.md](03_custom-errors.md), [10_error-handling/04_error-propagation.md](04_error-propagation.md) |
| **Time to Read** | 28 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2022 (Error.cause) / Node.js 18+ LTS / Express & Fastify Standards |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Operational vs Programmer Errors](#2-core-concepts-operational-vs-programmer-errors)
3. [How It Works: Centralized Error Middleware & Boundary Architecture](#3-how-it-works-centralized-error-middleware--boundary-architecture)
4. [Syntax & Architectural Patterns](#4-syntax--architectural-patterns)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Logging Backpressure & Telemetry](#8-performance-characteristics-logging-backpressure--telemetry)
9. [Engine Internals: Node.js process.stdout & Worker Thread Loggers](#9-engine-internals-nodejs-processstdout--worker-thread-loggers)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Enterprise Payment Gateway Resilience & Circuit Breaker Engine](#11-real-world-case-study-enterprise-payment-gateway-resilience--circuit-breaker-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In development, errors are informational: engineers want full stack traces, verbose payloads, and immediate console logs.
In **production**, however, error handling is a critical discipline involving security, reliability, compliance, and user experience:
1. **Security (CWE-209):** Leaking internal stack traces, database table names, or file paths in API responses exposes implementation details to malicious attackers.
2. **Compliance & Privacy (GDPR / HIPAA / PCI-DSS):** Logging raw user requests frequently writes Personally Identifiable Information (PII), passwords, authorization headers, and credit card numbers directly to persistent log files.
3. **Cascading Failures:** When an upstream dependency (e.g., database or payment provider) goes down, unthrottled retries can trigger an accidental self-inflicted Denial of Service (DoS).

A production-grade error handling strategy separates errors into distinct operational domains, enforces centralized sanitization middleware, and protects services using resilience patterns like **Circuit Breakers**.

---

## 2. Core Concepts: Operational vs Programmer Errors

The foundation of production resilience (formalized by Node.js maintainers) is distinguishing between two fundamentally different types of failures:

```
                                  Runtime Failure
                                         |
         +-------------------------------+-------------------------------+
         |                                                               |
  Operational Error                                               Programmer Error
(Known, expected runtime condition)                              (Unanticipated bug in code)
         |                                                               |
 - Network request timeout                                        - TypeError: Cannot read property of undefined
 - Invalid user input / 400 Bad Request                           - ReferenceError: typo in variable name
 - Database connection reset                                      - SyntaxError / Broken assertion
 - Out of disk space                                              - Passing wrong argument types to library
         |                                                               |
    [Strategy]                                                      [Strategy]
 Handle gracefully, retry,                                       Crash fast, restart container,
 return client HTTP response                                     alert on-call engineering team
```

### Key Differences:
- **Operational Errors:** Natural events that occur in any healthy system. They must be anticipated, caught, and handled with graceful user messaging or retries.
- **Programmer Errors:** Bugs that should never happen. You cannot reliably "handle" a programmer error because the program's internal invariants have been broken. The safest response is to crash the process immediately and let the orchestrator (Kubernetes/Docker) restart a clean container.

---

## 3. How It Works: Centralized Error Middleware & Boundary Architecture

In production web applications, error handling should never be scattered across hundreds of route handlers. Instead, all errors flow into a **Centralized Error Handling Pipeline**:

```
 Client Request ---> [Route Handler A] ---> Throws Error!
                                                |
                                                v (next(err))
                                  [Centralized Error Middleware]
                                                |
         +--------------------------------------+--------------------------------------+
         |                                      |                                      |
         v                                      v                                      v
[PII / Secret Sanitizer]               [Observability / APM]                 [Client HTTP Response]
Masks passwords, cards,                Ships structured JSON to              Safe public JSON (RFC 7807)
and Bearer tokens                      Datadog / Sentry / CloudWatch         (Hides internal stack trace!)
```

### Express / Fastify Middleware Contract:
In Express, error middleware is uniquely identified by having **exactly four arguments**:
`(err, req, res, next)`. If you declare only three arguments `(req, res, next)`, Express treats it as normal request middleware and skips it during error bubbling!

---

## 4. Syntax & Architectural Patterns

### 4.1 Production Express Error Middleware Signature
```javascript
// Express Error Handling Middleware: EXACTLY 4 ARGUMENTS!
app.use((err, req, res, next) => {
  // 1. Determine Status Code:
  const statusCode = err.statusCode || 500;

  // 2. Log Internal Diagnostics (Server-side):
  logger.error({
    message: err.message,
    stack: err.stack,
    cause: err.cause,
    requestId: req.headers["x-request-id"]
  });

  // 3. Return Clean, Sanitized Response to Client:
  res.status(statusCode).json({
    status: statusCode,
    error: err.isOperational ? err.name : "InternalServerError",
    message: err.isOperational ? err.message : "An unexpected error occurred. Please try again later.",
    requestId: req.headers["x-request-id"]
  });
});
```

---

## 5. Practical Examples

### Example 1: High-Performance PII and Secret Redaction Engine
```javascript
class LogSanitizer {
  static SENSITIVE_KEYS = new Set([
    "password", "token", "authorization", "secret", 
    "apikey", "access_token", "cookie", "ssn", "cvv"
  ]);

  static sanitize(data) {
    if (typeof data !== "object" || data === null) return data;

    if (Array.isArray(data)) {
      return data.map(item => this.sanitize(item));
    }

    const clean = {};
    for (const [key, value] of Object.entries(data)) {
      const lowerKey = key.toLowerCase();
      
      if (this.SENSITIVE_KEYS.has(lowerKey)) {
        clean[key] = "[REDACTED]";
      } else if (typeof value === "string") {
        // Redact JWT tokens:
        clean[key] = value.replace(/Bearer\s+[A-Za-z0-9-_=]+\.[A-Za-z0-9-_=]+\.?[A-Za-z0-9-_.+/=]*/g, "Bearer [REDACTED_JWT]");
      } else if (typeof value === "object") {
        clean[key] = this.sanitize(value);
      } else {
        clean[key] = value;
      }
    }
    return clean;
  }
}

const reqBody = {
  user: "ada",
  password: "superSecretPassword123!",
  profile: {
    creditCard: "4111-2222-3333-4444",
    email: "ada@example.com"
  },
  authHeader: "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.e30.t-IDNUtbYTt4"
};

console.log(JSON.stringify(LogSanitizer.sanitize(reqBody), null, 2));
```

### Example 2: The Circuit Breaker Pattern
When an external service is failing, continuing to bombard it with requests wastes resources and slows down your service. A **Circuit Breaker** fails fast immediately:

```javascript
class CircuitBreaker {
  constructor(action, options = {}) {
    this.action = action;
    this.failureThreshold = options.failureThreshold || 3;
    this.cooldownPeriod = options.cooldownPeriod || 5000;
    
    this.state = "CLOSED"; // CLOSED (Normal), OPEN (Failing), HALF_OPEN (Testing)
    this.failureCount = 0;
    this.nextAttempt = Date.now();
  }

  async execute(...args) {
    if (this.state === "OPEN") {
      if (Date.now() > this.nextAttempt) {
        this.state = "HALF_OPEN";
        console.log("[Circuit Breaker] Entering HALF_OPEN: Testing dependency...");
      } else {
        throw new Error("CircuitBreakerOpenError: Upstream service unavailable; failing fast.");
      }
    }

    try {
      const result = await this.action(...args);
      this.onSuccess();
      return result;
    } catch (err) {
      this.onFailure();
      throw err;
    }
  }

  onSuccess() {
    this.failureCount = 0;
    this.state = "CLOSED";
  }

  onFailure() {
    this.failureCount++;
    if (this.failureCount >= this.failureThreshold || this.state === "HALF_OPEN") {
      this.state = "OPEN";
      this.nextAttempt = Date.now() + this.cooldownPeriod;
      console.warn(`[Circuit Breaker] Tripped to OPEN! Cooling down for ${this.cooldownPeriod}ms.`);
    }
  }
}

// Verification:
let externalServiceFailing = true;
const breaker = new CircuitBreaker(async () => {
  if (externalServiceFailing) throw new Error("503 Service Unavailable");
  return "SUCCESS";
}, { failureThreshold: 2, cooldownPeriod: 100 });

async function runDemo() {
  for (let i = 1; i <= 3; i++) {
    try {
      await breaker.execute();
    } catch (e) {
      console.log(`Call #${i}:`, e.message);
    }
  }
}
runDemo();
// Output:
// Call #1: 503 Service Unavailable
// [Circuit Breaker] Tripped to OPEN! Cooling down for 100ms.
// Call #2: 503 Service Unavailable
// Call #3: CircuitBreakerOpenError: Upstream service unavailable; failing fast.
```

### Example 3: Structured JSON Logger to STDOUT (12-Factor App)
```javascript
class ProductionLogger {
  static log(level, message, metadata = {}) {
    const entry = {
      timestamp: new Date().toISOString(),
      level: level.toUpperCase(),
      message: message,
      pid: process.pid,
      ...metadata
    };
    // Non-blocking write to stdout:
    process.stdout.write(JSON.stringify(entry) + "\n");
  }

  static info(msg, meta) { this.log("info", msg, meta); }
  static warn(msg, meta) { this.log("warn", msg, meta); }
  static error(msg, err, meta = {}) {
    this.log("error", msg, {
      ...meta,
      errorName: err?.name,
      errorMessage: err?.message,
      stack: err?.stack,
      cause: err?.cause?.message
    });
  }
}

ProductionLogger.error("Failed to charge credit card", new Error("Card expired"), {
  userId: 42,
  orderId: "ord_9901"
});
```

### Example 4: Frontend React Error Boundary Model
In browser UI frameworks, a single unhandled render error crashes the entire component tree, leaving a blank white screen. An Error Boundary catches render errors and renders a fallback UI:
```javascript
// Pure JS emulation of React Error Boundary contract:
class ErrorBoundaryManager {
  constructor(fallbackRenderer) {
    this.fallbackRenderer = fallbackRenderer;
    this.hasError = false;
    this.error = null;
  }

  capture(renderFunction) {
    try {
      if (this.hasError) {
        return this.fallbackRenderer(this.error);
      }
      return renderFunction();
    } catch (err) {
      this.hasError = true;
      this.error = err;
      // Report to Sentry / Datadog:
      console.error("[Telemetry] Component render crashed:", err.message);
      return this.fallbackRenderer(err);
    }
  }
}

const boundary = new ErrorBoundaryManager((err) => {
  return `<div class="error-banner">Something went wrong. <button>Retry</button></div>`;
});

console.log(boundary.capture(() => {
  throw new Error("Render pipeline crashed!");
}));
// Output: '<div class="error-banner">Something went wrong. <button>Retry</button></div>'
```

### Example 5: Complete Async Route Handler Wrapper
In Express 4, unhandled promise rejections inside async route handlers hang the request forever unless wrapped. This utility automatically forwards async errors to `next()`:
```javascript
function asyncHandler(fn) {
  return (req, res, next) => {
    Promise.resolve(fn(req, res, next)).catch(next);
  };
}

// Route definition:
// app.get("/users/:id", asyncHandler(async (req, res) => {
//   const user = await database.findUser(req.params.id);
//   res.json(user);
// }));
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Information Leakage via Error Stack Traces
Exposing `err.stack` directly to HTTP clients reveals internal file paths, operating system user accounts, dependencies, and database table structures.
**Security Rule:** Never send `err.stack` in HTTP responses in production environments (`process.env.NODE_ENV === 'production'`).

### 6.2 The "Double Next" Express Trap
Calling `next(err)` and subsequently executing code that attempts to send headers throws `ERR_HTTP_HEADERS_SENT`:
```javascript
app.get("/data", (req, res, next) => {
  if (isInvalid) {
    next(new Error("Invalid"));
    // BUG: Missing return! Execution continues below:
  }
  res.json({ ok: true }); // Crashes with ERR_HTTP_HEADERS_SENT!
});

// FIX: Always return next(err):
if (isInvalid) {
  return next(new Error("Invalid"));
}
```

### 6.3 Synchronous `console.log` Blocking the Event Loop
In Node.js, `console.log()` is **synchronous** when writing to terminals or redirected pipes on certain operating systems. Logging massive objects inside high-frequency loops can freeze the entire Node.js event loop. Always use high-performance asynchronous loggers (like `pino`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Exposing Stack Traces to Clients** | `res.status(500).send(err.stack)` | Send generic message, log stack internally | Breaches security (CWE-209); reveals server vulnerabilities. |
| **Logging Sensitive PII** | Logging raw headers containing Bearer tokens | Sanitize passwords, tokens, and cards | Violates GDPR, PCI-DSS, and SOC2 compliance. |
| **3-Argument Error Middleware** | `app.use((req, res, next) => {})` | Declare all 4 parameters: `(err, req, res, next)` | Express skips error middleware if it lacks 4 arguments. |
| **Silent Catch in Async Routes** | `try { ... } catch (e) {}` | Forward to pipeline: `next(e)` | Leaves HTTP requests hanging indefinitely until timeout. |
| **Treating All Errors as Fatal** | Exiting process on bad user input | Return 400 Bad Request; save exits for bugs | Causes self-inflicted denial of service on malformed client requests. |
| **Unbounded Retries** | Retrying failing requests in an infinite loop | Use Exponential Backoff + Circuit Breaker | Thundering herd problem crashes recovering databases. |

---

## 8. Performance Characteristics: Logging Backpressure & Telemetry

1. **Structured JSON vs String Concatenation:**
   - Serializing structured JSON via `pino` or `process.stdout.write` is up to **$5\times$ faster** than standard `console.log()` string concatenation.
2. **Worker Thread Logging:**
   - High-performance logging frameworks offload JSON stringification and network shipping to dedicated background Worker Threads, keeping the main event loop thread completely free for HTTP requests.
3. **APM Batching:**
   - Telemetry SDKs (Datadog, Sentry) buffer error events in memory and transmit them in batches every 500ms to avoid saturating HTTP sockets.

---

## 9. Engine Internals: Node.js process.stdout & Worker Thread Loggers

In Node.js, `process.stdout` is a `net.Socket` stream:
```
 JavaScript Runtime (Main Event Loop)
                   |
     logger.info({ event: "PAYMENT" })
                   |
                   v
     process.stdout.write(jsonBuffer)
                   |
                   v
 Libuv Stream Socket ---> OS File Descriptor 1 (STDOUT)
```
- If the downstream logging daemon (Docker/Fluentd) is slow to read stdout, the OS socket buffer fills up.
- `process.stdout.write()` returns `false`, signaling **backpressure**.
- High-performance architectures pipe logs directly to background worker threads using `worker_threads` to ensure backpressure never pauses the API thread.

---

## 10. Best Practices & Design Guidelines

1. **Follow the 12-Factor App Logging Standard:** Treat logs as event streams. Write structured JSON lines strictly to `stdout`, and let container orchestration tools (Datadog Agent, Promtail, FluentBit) handle aggregation.
2. **Implement Correlation IDs (Request Tracing):** Inject an `X-Request-Id` into incoming HTTP requests and attach it to all error logs and downstream microservice RPC calls.
3. **Use RFC 7807 for REST API Error Contracts:** Format all 4xx and 5xx client responses as standard Problem Details JSON objects.
4. **Deploy Circuit Breakers on All External Network Calls:** Wrap database queries, payment gateways, and third-party APIs in circuit breakers with sensible timeouts.

---

## 11. Real-World Case Study: Enterprise Payment Gateway Resilience & Circuit Breaker Engine

### Problem
A global ticketing platform processed thousands of ticket sales per minute. During a major concert release, the upstream banking authorization gateway crashed, throwing 500 errors. The ticketing backend continuously retried every failed transaction 5 times, creating a thundering herd of 50,000 requests that completely overwhelmed the recovering gateway, taking the entire platform offline for 3 hours.

### Solution
Implement an enterprise resilience pipeline featuring Circuit Breakers, structured correlation IDs, and PII-sanitized APM logging:

```javascript
class PaymentGatewayService {
  constructor(httpClient, logger) {
    this.logger = logger;
    this.breaker = new CircuitBreaker(this.executePaymentCall.bind(this), {
      failureThreshold: 5,
      cooldownPeriod: 10_000
    });
  }

  async processTransaction(requestId, paymentDetails) {
    // 1. Sanitize payload for logging:
    const safePayload = LogSanitizer.sanitize(paymentDetails);
    this.logger.info("Initiating payment transaction", { requestId, payload: safePayload });

    try {
      // 2. Execute through Circuit Breaker:
      const response = await this.breaker.execute(paymentDetails);
      this.logger.info("Payment transaction succeeded", { requestId, txnId: response.txnId });
      return response;
    } catch (err) {
      // 3. Structured Causal Logging:
      this.logger.error("Payment transaction failed", err, {
        requestId,
        circuitState: this.breaker.state
      });

      // 4. Transform into Client-Safe Domain Error:
      if (err.message.includes("CircuitBreakerOpenError")) {
        throw new Error("Payment service temporarily unavailable. Please retry in a few moments.", {
          cause: err
        });
      }
      throw new Error("Payment processing declined.", { cause: err });
    }
  }

  async executePaymentCall(details) {
    // Upstream bank API simulation
    if (details.amount > 1000) throw new Error("Bank gateway timeout (504)");
    return { status: "APPROVED", txnId: "tx_99812" };
  }
}

// Running payment processing:
const service = new PaymentGatewayService(null, ProductionLogger);

service.processTransaction("req_001", {
  userId: "usr_55",
  amount: 250,
  cardNumber: "4111-2222-3333-4444"
}).then(res => {
  console.log("Client Received:", res);
});
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the fundamental difference between an Operational Error and a Programmer Error?</strong></summary>

An **Operational Error** is an expected runtime problem that occurs naturally in a working system (e.g., network timeout, invalid user input, full disk). The system must anticipate and handle it gracefully. A **Programmer Error** is a bug in the code (e.g., `TypeError`, syntax error, referencing undefined). The system cannot recover safely because its internal state is corrupted; the correct action is to crash fast, alert developers, and restart.
</details>

<details>
<summary><strong>Q2: Why must Express error-handling middleware have exactly four parameters?</strong></summary>

Express inspects the `Function.prototype.length` property of registered middleware. Standard middleware has 2 or 3 parameters `(req, res, next)`. When an error is forwarded via `next(err)`, Express iterates through its router stack and invokes **only** those middleware functions that declare exactly 4 parameters `(err, req, res, next)`.
</details>

<details>
<summary><strong>Q3: What is CWE-209 and how does it relate to JavaScript error handling?</strong></summary>

CWE-209 is the Common Weakness Enumeration for "Generation of Error Message Containing Sensitive Information". In JavaScript applications, returning raw `err.stack` or internal database messages to client HTTP responses leaks backend directory paths, database table names, and dependency versions to attackers, facilitating targeted exploits.
</details>

<details>
<summary><strong>Q4: How does a Circuit Breaker prevent cascading system failures?</strong></summary>

When an upstream dependency fails repeatedly, a Circuit Breaker trips from `CLOSED` to `OPEN`. While in the `OPEN` state, all subsequent requests fail fast immediately without making actual network calls to the struggling service. After a cooldown period, it enters `HALF_OPEN` to test if the dependency has recovered. This stops thundering herd retries and gives the upstream service time to recover.
</details>

<details>
<summary><strong>Q5: Why should production Node.js applications log structured JSON lines to STDOUT instead of writing directly to log files?</strong></summary>

Following the 12-Factor App methodology, applications should treat logs as event streams and remain completely agnostic to storage destinations. Writing JSON lines to `stdout` decouples the application from file rotation and log shipping. Container orchestrators (Docker, Kubernetes) capture stdout and route logs to centralized storage (Datadog, ElasticSearch) without consuming Node.js event loop cycles.
</details>

<details>
<summary><strong>Q6: What is a Correlation ID (Request ID), and why is it essential for distributed error tracking?</strong></summary>

A Correlation ID is a unique UUID generated at the API gateway and passed across all downstream microservices via HTTP headers (`X-Request-Id`). When an error occurs deep in the call stack, attaching the Correlation ID to the log allows developers to trace the entire lifecycle of that specific user request across multiple distributed servers in APM tools.
</details>

<details>
<summary><strong>Q7: What happens if an error occurs inside an Express error-handling middleware itself?</strong></summary>

If an error is thrown inside error middleware, it skips normal execution and is caught by Express's default built-in final error handler. In development, it prints a stack trace to the client; in production, it sends a generic 500 Internal Server Error text response.
</details>

<details>
<summary><strong>Q8: How does an Error Boundary work in modern frontend frameworks like React?</strong></summary>

An Error Boundary is a class component implementing `componentDidCatch(error, info)` or `static getDerivedStateFromError(error)`. When a child component throws an error during rendering or lifecycle execution, the Error Boundary catches it, prevents the entire UI tree from unmounting, and renders a localized fallback UI.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Asynchronous Express Route Handler Wrapper
Implement `wrapAsync(fn)` that accepts an asynchronous Express route handler and forwards any rejected promise to `next(err)`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function wrapAsync(fn) {
  return function(req, res, next) {
    fn(req, res, next).catch(next);
  };
}

// Verification:
const mockNext = (err) => console.log("Caught and forwarded to next:", err.message);

const failingRoute = wrapAsync(async (req, res, next) => {
  throw new Error("Async failure inside route handler");
});

failingRoute({}, {}, mockNext);
// Output: Caught and forwarded to next: Async failure inside route handler
```
</details>

### Exercise 2: Masking Credit Card Numbers in Strings
Write a utility function `maskCreditCards(logString)` that finds all 16-digit credit card patterns (with or without dashes/spaces) and replaces the middle 8 digits with `XXXX-XXXX`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function maskCreditCards(text) {
  const ccRegex = /\b(\d{4})[-\s]?(\d{4})[-\s]?(\d{4})[-\s]?(\d{4})\b/g;
  return text.replace(ccRegex, "$1-XXXX-XXXX-$4");
}

console.log(maskCreditCards("Processed card 4111222233334444 for $100."));
// "Processed card 4111-XXXX-XXXX-4444 for $100."

console.log(maskCreditCards("Card: 5500 1234 5678 9012"));
// "Card: 5500-XXXX-XXXX-9012"
```
</details>

### Exercise 3: Resilient Health Check Aggregator
Implement an asynchronous health check function `checkServiceHealth(checks)` that executes multiple health checks in parallel, returns `{ status: "HEALTHY" | "DEGRADED" | "UNHEALTHY", details }`, and never throws.

<details>
<summary><strong>Solution</strong></summary>

```javascript
async function checkServiceHealth(checks) {
  const results = {};
  let failedCount = 0;

  const entries = Object.entries(checks);
  const settled = await Promise.allSettled(entries.map(([name, checkFn]) => checkFn()));

  settled.forEach((outcome, idx) => {
    const name = entries[idx][0];
    if (outcome.status === "fulfilled") {
      results[name] = { status: "OK", value: outcome.value };
    } else {
      failedCount++;
      results[name] = { status: "FAILED", error: outcome.reason?.message || "Unknown error" };
    }
  });

  let overallStatus = "HEALTHY";
  if (failedCount === entries.length) {
    overallStatus = "UNHEALTHY";
  } else if (failedCount > 0) {
    overallStatus = "DEGRADED";
  }

  return { status: overallStatus, timestamp: new Date().toISOString(), checks: results };
}

const mockChecks = {
  db: () => Promise.resolve("Connected"),
  redis: () => Promise.reject(new Error("Redis ping timeout")),
  disk: () => Promise.resolve("45% free")
};

checkServiceHealth(mockChecks).then(report => {
  console.log(JSON.stringify(report, null, 2));
});
// Outputs report with status: "DEGRADED"
```
</details>
