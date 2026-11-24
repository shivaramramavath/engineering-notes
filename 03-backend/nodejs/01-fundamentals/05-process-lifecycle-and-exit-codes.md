# Process Lifecycle, POSIX Signals & Exit Codes in Node.js

> Comprehensive guide to the Node.js process lifecycle, lifecycle events (`beforeExit`, `exit`), POSIX signal handling (`SIGINT`, `SIGTERM`), and standard exit codes.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **APIs** | `process.on`, `process.exit`, `process.kill` |
| **Level** | Core Fundamentals / DevOps |

---

## 1. Process Lifecycle Overview

A Node.js process begins execution when the V8 engine initializes and executes the entry script.
The process remains alive as long as there are **active event loop handles, active timers, open sockets, or pending promises**.

```
[ Process Start ]
        │
        ▼
[ Run Synchronous Entry Script ]
        │
        ▼
[ Event Loop Tick & I/O Polling ] <── (Active handles, sockets, timers)
        │
(No more work in Event Loop)
        │
        ├── Emits 'beforeExit' event (can schedule more work)
        │
        ▼
[ Emits 'exit' event ] ──(Only synchronous code allowed!)──> [ Process Terminated ]
```

---

## 2. Standard Exit Codes

Node.js exits with `0` when no further asynchronous work is scheduled. A non-zero exit code indicates an abnormal termination:

| Exit Code | Meaning | Typical Trigger |
| :---: | :--- | :--- |
| **0** | Success / Normal exit | Event loop ran out of work or explicit `process.exit(0)`. |
| **1** | Uncaught Fatal Exception | An unhandled exception was thrown and not caught by an error handler. |
| **13** | Unfinished Top-Level Await | A top-level await promise rejected or never resolved. |
| **128 + n**| Terminated by POSIX Signal | Process was killed by OS signal `n` (e.g. `128 + 9 = 137` for `SIGKILL`, `128 + 15 = 143` for `SIGTERM`). |

---

## 3. POSIX Signal Handling & Graceful Shutdown

In containerized environments (Docker, Kubernetes), orchestrators stop containers by sending **`SIGTERM`**, waiting for a grace period (e.g. 30 seconds), and sending **`SIGKILL`** if the process has not terminated.

```javascript
import http from 'node:http';
import { db } from './db.js';

const server = http.createServer((req, res) => {
  res.end('Hello, World!');
});

server.listen(3000, () => {
  console.log('Server listening on port 3000');
});

function gracefulShutdown(signal) {
  console.log(`Received ${signal}. Initiating graceful shutdown...`);

  // 1. Stop accepting new connections
  server.close(async (err) => {
    if (err) {
      console.error('Error closing HTTP server:', err);
      process.exit(1);
    }

    console.log('HTTP server closed. Closing database connection pool...');
    try {
      // 2. Close database pools and message queue workers
      await db.end();
      console.log('Database connections closed cleanly.');
      process.exit(0);
    } catch (dbErr) {
      console.error('Error closing database pool:', dbErr);
      process.exit(1);
    }
  });

  // 3. Force exit if cleanup takes longer than timeout threshold
  setTimeout(() => {
    console.error('Forced shutdown: Timed out waiting for connections to close.');
    process.exit(1);
  }, 10000).unref(); // unref so timer itself does not hold event loop open
}

process.on('SIGTERM', () => gracefulShutdown('SIGTERM'));
process.on('SIGINT', () => gracefulShutdown('SIGINT'));
```

---

## 4. `beforeExit` vs `exit` Events

- **`beforeExit`**: Emitted when the event loop empties its work queue. You can schedule asynchronous tasks in this callback, which keeps the process running.
- **`exit`**: Emitted when the process is definitely shutting down. **Only synchronous code can execute**; any asynchronous timers or promises scheduled in `exit` will never run.
