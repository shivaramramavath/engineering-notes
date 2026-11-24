# Cheatsheet 13: Common Node.js Errors & Fixes Reference Card

> Rapid troubleshooting guide for the 10 most common Node.js backend exceptions, root causes, and production solutions.

---

### 1. `ERR_HTTP_HEADERS_SENT: Cannot set headers after they are sent to the client`
- **Root Cause**: Attempting to set an HTTP header or send a response (`res.send()`, `res.json()`, `res.redirect()`) after the response headers have already been transmitted to the client. Usually caused by missing a `return` keyword before `res.json()`.
- **The Fix**:
  ```javascript
  // BAD: Execution continues after res.status(404)
  if (!user) {
    res.status(404).json({ error: 'Not found' });
  }
  res.json(user); // THROWS ERR_HTTP_HEADERS_SENT!

  // GOOD: Always return early
  if (!user) {
    return res.status(404).json({ error: 'Not found' });
  }
  return res.json(user);
  ```

---

### 2. `ERR_MODULE_NOT_FOUND: Cannot find module`
- **Root Cause**: In ECMAScript Modules (`"type": "module"`), relative imports require explicit file extensions (`.js`), which was not required in CommonJS.
- **The Fix**:
  ```javascript
  // BAD in ESM:
  import { helper } from './utils/helper';

  // GOOD in ESM:
  import { helper } from './utils/helper.js'; // Must include .js extension!
  ```

---

### 3. `ERR_REQUIRE_ESM: require() of ES Module not supported`
- **Root Cause**: In Node.js versions prior to 22.12, attempting to synchronously `require()` a package that publishes only ESM throws this error.
- **The Fix**:
  1. Upgrade to **Node.js 22 LTS (22.12+)** or Node.js 24, where `require(esm)` is supported by default.
  2. Or convert your application to native ESM by adding `"type": "module"` to `package.json`.
  3. Or use dynamic import: `const pkg = await import('esm-package');`.

---

### 4. `EADDRINUSE: address already in use :::3000`
- **Root Cause**: Another process (or a previous zombie instance of your app) is already bound to port 3000.
- **The Fix**:
  ```bash
  # Find and kill the process holding port 3000:
  # On Linux / macOS:
  lsof -i :3000 -t | xargs kill -9

  # On Windows PowerShell:
  Get-Process -Id (Get-NetTCPConnection -LocalPort 3000).OwningProcess | Stop-Process -Force
  ```

---

### 5. `ECONNRESET` & `ECONNREFUSED`
- **`ECONNREFUSED`**: The target host is reachable, but nothing is listening on that port (e.g. PostgreSQL or Redis container is stopped).
  - *Fix*: Verify target service is running: `docker compose ps` or `pg_isready`.
- **`ECONNRESET`**: The connection was forcibly closed by the remote peer (e.g. upstream load balancer timed out or server restarted mid-request).
  - *Fix*: Implement automatic exponential backoff retries and verify HTTP Keep-Alive timeouts match between client and server.

---

### 6. `MaxListenersExceededWarning: Possible EventEmitter memory leak detected`
- **Root Cause**: More than 10 listeners (the default limit) were registered to a single `EventEmitter` instance, often caused by adding listeners inside request handlers.
- **The Fix**:
  1. Remove listeners on cleanup: `emitter.off(event, handler)`.
  2. If 10+ listeners are legitimate, increase the threshold explicitly:
     `emitter.setMaxListeners(50);`.

---

### 7. `FATAL ERROR: Ineffective mark-compacts near heap limit Allocation failed - JavaScript heap out of memory`
- **Root Cause**: The application exceeded the maximum V8 heap memory size (default: ~2GB on 64-bit systems) due to a memory leak or loading massive files entirely into RAM.
- **The Fix**:
  1. **Stream Large Data**: Never use `fs.readFile()` on files $>100$MB; use `fs.createReadStream()` and streams.
  2. **Increase Heap Limit (Temporary workaround)**:
     `node --max-old-space-size=4096 server.js` (Increases limit to 4GB).
  3. Identify leaks using heap snapshots (`v8.writeHeapSnapshot()`).

---

### 8. `UnhandledPromiseRejection`
- **Root Cause**: A Promise rejected, but no `.catch()` handler or `try/catch` block caught the error.
- **The Fix**:
  - In Express 5, async route errors are automatically caught.
  - In Express 4, wrap in try/catch or use an async wrapper.
  - Listen globally for diagnostic logging:
    ```javascript
    process.on('unhandledRejection', (reason, promise) => {
      console.error('Unhandled Promise Rejection at:', promise, 'reason:', reason);
      // In production, exit cleanly so container orchestrator restarts the pod:
      process.exit(1);
    });
    ```
