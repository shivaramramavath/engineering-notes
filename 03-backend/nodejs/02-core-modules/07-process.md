# `process` — The Running Node Instance

`process` is a global object giving you information about, and control over, the currently running Node process — environment variables, command-line arguments, exit behavior, and OS signals.

```js
// process is a global — no import needed
```

## Environment variables

```js
console.log(process.env.NODE_ENV); // "development", "production", etc.
console.log(process.env.PORT); // always a string, even for "3000"
```

Every value in `process.env` is a **string** — this is the underlying mechanism the `dotenv`/`envalid` docs (`env/dotenv.md`, `env/envalid.md`) are built on top of. `dotenv` just loads a `.env` file's contents into `process.env`; `envalid` validates and converts what's there.

```js
process.env.MY_VAR = "runtime value"; // you can also set values at runtime
```

---

## Command-line arguments

```bash
node app.js --port 3000 fast
```

```js
console.log(process.argv);
// [
//   "/usr/local/bin/node",   // path to the node executable
//   "/path/to/app.js",        // path to the script being run
//   "--port",                  // your actual arguments start here
//   "3000",
//   "fast"
// ]
```

```js
const args = process.argv.slice(2); // drop the first two, get just your args
```

For anything beyond trivial parsing, a library like `commander` or `yargs` handles flags/options far more robustly than manually parsing `argv`.

---

## Exiting the process

```js
process.exit(0); // success
process.exit(1); // failure/error
```

Exit code `0` conventionally means success; any non-zero code signals an error, and different non-zero values are sometimes used to distinguish different failure reasons in scripts/CI.

### Prefer letting Node exit naturally

```js
// ❌ can cut off pending I/O (unflushed logs, in-flight writes) before it completes
process.exit(1);

// ✅ let the event loop drain naturally once there's nothing left to do
process.exitCode = 1;
// don't call exit() — Node exits on its own once there's no pending work
```

Setting `process.exitCode` and letting Node shut down naturally, rather than forcing an immediate `process.exit()`, avoids truncating logs or aborting in-flight operations (like a final `console.log` or file write) that haven't finished flushing yet.

---

## Listening for process events

```js
process.on("exit", (code) => {
  console.log(`Process exiting with code ${code}`);
  // only synchronous work is possible here — no async operations will complete
});
```

### Handling OS signals — graceful shutdown

```js
process.on("SIGTERM", async () => {
  console.log("Received SIGTERM, shutting down gracefully...");
  await server.close(); // stop accepting new connections, finish in-flight ones
  await db.disconnect();
  process.exit(0);
});

process.on("SIGINT", async () => {
  console.log("Received SIGINT (Ctrl+C)");
  await cleanup();
  process.exit(0);
});
```

| Signal    | Typically sent by                                                                   |
| --------- | ----------------------------------------------------------------------------------- |
| `SIGTERM` | Orchestrators (Docker, Kubernetes) asking a process to stop gracefully              |
| `SIGINT`  | Ctrl+C in a terminal                                                                |
| `SIGKILL` | Force-kill — **cannot be caught or handled**, the process is terminated immediately |

Handling `SIGTERM` gracefully matters a great deal in containerized environments: `docker stop` sends `SIGTERM` and waits a grace period before force-killing with `SIGKILL` (see the Docker guide's `02_docker_commands/01_run-ps-stop.md`) — an app that ignores `SIGTERM` gets abruptly killed instead of shutting down cleanly, potentially dropping in-flight requests or leaving a database connection in a bad state.

---

## Catching unhandled errors

```js
process.on("uncaughtException", (err) => {
  console.error("Uncaught exception:", err);
  process.exit(1); // the process is in an unknown state — exit rather than continue
});

process.on("unhandledRejection", (reason) => {
  console.error("Unhandled promise rejection:", reason);
  process.exit(1);
});
```

**These are a last resort, not a substitute for proper error handling.** By the time `uncaughtException` fires, the process may be in an inconsistent state (a thrown error interrupted execution somewhere unexpected) — the standard recommendation is to log what you can and exit, rather than trying to keep running. Prefer `try/catch` around specific operations, and a centralized error-handling middleware in a framework like Express (`express/04-error-handling.md`), over relying on these as your primary error strategy.

---

## Other useful `process` properties

```js
process.cwd(); // current working directory
process.platform; // "linux", "darwin", "win32"
process.version; // Node version, e.g. "v20.11.0"
process.pid; // this process's ID
process.memoryUsage(); // { rss, heapTotal, heapUsed, external, ... }
process.uptime(); // seconds since the process started
```

```js
console.log(`Running Node ${process.version} on ${process.platform}`);
```

---

## `process.nextTick()` (a quick, advanced mention)

```js
process.nextTick(() => {
  console.log("Runs before any I/O, before even Promise microtasks");
});
```

Schedules a callback to run at the very front of the queue, before the event loop continues — even before Promise callbacks. Rarely needed directly in application code; mentioned here because you may encounter it reading library internals.

## Common mistakes

- **Calling `process.exit()` immediately after an async operation** — the process can exit before pending I/O (writes, log flushes) finishes; prefer `process.exitCode = n` and letting Node exit naturally, or explicitly awaiting cleanup first.
- **Not handling `SIGTERM`** — in a containerized deployment, this means every deploy/restart abruptly kills in-flight requests instead of draining them.
- **Relying on `uncaughtException`/`unhandledRejection` as normal error handling** — these are a safety net for truly unexpected errors, not a substitute for `try/catch` and proper error-handling middleware.
- **Forgetting `process.env` values are always strings** — `process.env.PORT === 3000` is always `false`; compare against `"3000"` or convert with `Number()`.

## Quick summary

- `process.env` holds environment variables (always strings) — the foundation `dotenv`/`envalid` build on
- `process.argv` holds command-line arguments; slice off the first two to get your actual args
- Prefer `process.exitCode = n` over `process.exit(n)` so pending I/O isn't cut off
- Handle `SIGTERM`/`SIGINT` for graceful shutdown — essential in containerized deployments
- `uncaughtException`/`unhandledRejection` are a last-resort safety net, not primary error handling

## Next

**`08-os.md`** covers the `os` module — information about the machine Node is running on, useful for logging, diagnostics, and scaling decisions.
