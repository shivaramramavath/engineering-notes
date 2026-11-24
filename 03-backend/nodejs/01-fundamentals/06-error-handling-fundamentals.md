# Error Handling Fundamentals in Node.js

> Comprehensive guide to operational vs programmer errors, unhandled rejections, uncaught exceptions, custom error hierarchies, and the demise of Node.js domains.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Events** | `uncaughtException`, `unhandledRejection` |
| **Level** | Core Fundamentals |

---

## 1. Operational Errors vs. Programmer Errors

The foundation of robust Node.js error handling is categorizing errors into two distinct classes:

| Category | Definition | Examples | Correct Response |
| :--- | :--- | :--- | :--- |
| **Operational Errors** | Expected runtime failures that occur in working systems. | Network timeout, invalid user input, file not found (`ENOENT`), database unique key collision. | Catch error, log context, return error response (e.g. HTTP 400/404/503), retry if transient. |
| **Programmer Errors** | Bugs, logic flaws, or type mismatches in the application code. | `TypeError: Cannot read property of undefined`, syntax error, passing wrong argument types, reference error. | **Log error and restart the process** (the process state is corrupted). |

---

## 2. Uncaught Exceptions & Unhandled Rejections

### `process.on('uncaughtException')`
When an exception is thrown synchronously and never caught in a `try...catch`, it bubbles up to the process level:
```javascript
process.on('uncaughtException', (err, origin) => {
  console.error(`Fatal Uncaught Exception (${origin}):`, err);
  // The process memory and state may be corrupted!
  // Perform emergency logging, then terminate:
  process.exit(1);
});
```

### `process.on('unhandledRejection')`
When a Promise rejects without a `.catch()` or `try...catch` wrapper:
- In modern Node.js, an unhandled rejection terminates the process with exit code `1` by default (`--unhandled-rejections=throw`).
```javascript
process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Promise Rejection at:', promise, 'reason:', reason);
  process.exit(1);
});
```

---

## 3. Custom Application Error Hierarchy

```javascript
export class AppError extends Error {
  constructor(message, statusCode = 500, isOperational = true) {
    super(message);
    this.name = this.constructor.name;
    this.statusCode = statusCode;
    this.isOperational = isOperational;
    Error.captureStackTrace(this, this.constructor);
  }
}

export class NotFoundError extends AppError {
  constructor(resource = 'Resource') {
    super(`${resource} not found`, 404, true);
  }
}

export class ValidationError extends AppError {
  constructor(message, details = []) {
    super(message, 400, true);
    this.details = details;
  }
}
```

---

## 4. History: Why Node.js Domains Were Deprecated

In early Node.js (v0.8), the `domain` module was introduced to catch errors originating within a specific execution context.
- **The Failure**: Domains could not reliably clean up resources. When an asynchronous operation failed midway, closures, sockets, and database transactions remained half-open, causing state corruption and memory leaks.
- **Modern Solution**: Use `async/await` with standard `try/catch` and `AsyncLocalStorage` for context tracking.
