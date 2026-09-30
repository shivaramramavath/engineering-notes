# `child_process` — Running Other Programs

Node's built-in module for spawning and controlling other processes — running shell commands, other executables, or even additional Node scripts from within a running Node process.

```js
import { exec, execFile, spawn, fork } from "node:child_process";
```

## Why you'd need this

Node is great at I/O-bound work but is single-threaded for JavaScript execution. `child_process` covers two different needs:

- **Running an external program** — ImageMagick for image processing, `ffmpeg` for video, a shell command, a Python script
- **Offloading CPU-heavy work** — running another Node process to do heavy computation without blocking your main process's event loop (worker_threads, covered in `10-cluster-worker-threads.md`, is usually the better fit for pure CPU work _within_ Node specifically)

---

## `exec()` — run a shell command, get buffered output

```js
import { exec } from "node:child_process";

exec("ls -la", (err, stdout, stderr) => {
  if (err) {
    console.error("Error:", err);
    return;
  }
  console.log(stdout);
});
```

Runs the command through a shell, buffering all output in memory and calling back once the process exits.

### Promise version

```js
import { promisify } from "node:util";
import { exec } from "node:child_process";

const execAsync = promisify(exec);

const { stdout } = await execAsync("git rev-parse HEAD");
console.log(stdout.trim());
```

or, using the dedicated promises import:

```js
import { exec } from "node:child_process";
import util from "node:util";
const execAsync = util.promisify(exec);
```

### ⚠️ Security risk with untrusted input

```js
// ❌ never do this with user-supplied input
exec(`convert ${userProvidedFilename} output.png`);
```

Since `exec` runs through a shell, a malicious filename like `; rm -rf /` gets interpreted as shell syntax, not a literal argument — this is a **command injection** vulnerability. If any part of the command comes from user input, use `execFile` or `spawn` instead (below), which don't invoke a shell and pass arguments literally.

---

## `execFile()` — run a specific executable, no shell involved

```js
import { execFile } from "node:child_process";

execFile("convert", [userProvidedFilename, "output.png"], (err, stdout) => {
  if (err) throw err;
  console.log("Done");
});
```

Runs the executable directly, passing arguments as an array rather than interpolating them into a shell command string — the safe alternative to `exec` whenever any part of the input isn't fully trusted, hardcoded text.

---

## `spawn()` — for streaming output / long-running processes

```js
import { spawn } from "node:child_process";

const child = spawn("ping", ["-c", "4", "google.com"]);

child.stdout.on("data", (data) => {
  console.log(`stdout: ${data}`);
});

child.stderr.on("data", (data) => {
  console.error(`stderr: ${data}`);
});

child.on("close", (code) => {
  console.log(`Process exited with code ${code}`);
});
```

Unlike `exec`, `spawn` gives you **streams** (`05-streams.md`) for stdout/stderr rather than buffering everything until completion — the right choice for long-running processes, large output, or anything where you want to react to output as it arrives rather than waiting for the whole thing to finish.

### `exec` vs `spawn`

|            | Output                                        | Best for                                                  |
| ---------- | --------------------------------------------- | --------------------------------------------------------- |
| `exec`     | Buffered, all at once, via callback           | Short commands with small output                          |
| `execFile` | Buffered, no shell (safer with dynamic input) | Same as `exec`, but with untrusted arguments              |
| `spawn`    | Streamed, as it arrives                       | Long-running processes, large output, real-time reactions |

---

## `fork()` — spawn another Node process, with built-in messaging

```js
// parent.js
import { fork } from "node:child_process";

const child = fork("./worker.js");

child.send({ task: "process", data: [1, 2, 3] });

child.on("message", (result) => {
  console.log("Result from child:", result);
});
```

```js
// worker.js
process.on("message", (msg) => {
  const result = msg.data.map((n) => n * 2);
  process.send(result);
});
```

`fork()` is specifically for spawning **another Node.js script**, and sets up a built-in IPC (inter-process communication) channel automatically — `.send()`/`.on("message", ...)` — which `spawn` doesn't provide for arbitrary programs.

### `fork` vs `worker_threads`

Both let you offload work to a separate execution context within a Node app, but they differ meaningfully:

|                 | `fork()`                                      | `worker_threads`                                           |
| --------------- | --------------------------------------------- | ---------------------------------------------------------- |
| Isolation       | Separate OS process                           | Separate thread, same process                              |
| Memory          | Fully separate                                | Can share memory (`SharedArrayBuffer`)                     |
| Overhead        | Higher (new process)                          | Lower (thread, not a full process)                         |
| Crash isolation | A crash in the child doesn't crash the parent | A worker crash can be caught, but shares more risk surface |

`worker_threads` (`10-cluster-worker-threads.md`) is generally preferred for CPU-bound work _within_ the same app; `fork`/`spawn`/`exec` are for running genuinely separate programs or scripts.

---

## Common patterns

### Running a build/CLI tool from a Node script

```js
import { execFile } from "node:child_process";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);

await execFileAsync("npm", ["run", "build"]);
```

### Piping one process's output into another

```js
const grep = spawn("grep", ["error"]);
const logs = spawn("cat", ["app.log"]);

logs.stdout.pipe(grep.stdin);
grep.stdout.pipe(process.stdout);
```

Same `.pipe()` mechanism from `05-streams.md` — a child process's stdout/stdin are streams, just like a file or an HTTP response.

## Common mistakes

- **Using `exec()` with any user-influenced input** — command injection risk; use `execFile`/`spawn` with an arguments array instead.
- **Using `exec()` for large output** — its buffering has a default size limit (`maxBuffer`), and it holds everything in memory; `spawn` streams instead.
- **Not handling the `error` and `close`/`exit` events on `spawn`** — a child process that fails silently (wrong path, missing binary) is hard to debug without listening for these.
- **Reaching for `child_process` when `worker_threads` would fit better** — if the goal is purely CPU-bound work inside the same Node app (not running an external program), a worker thread is usually lighter weight than a forked process.

## Quick summary

- `exec`/`execFile` buffer output and call back on completion; `spawn` streams output as it happens
- Never build a shell command string from untrusted input — use `execFile`/`spawn` with an argument array instead of `exec`
- `fork()` is specifically for spawning another Node script, with a built-in message-passing channel
- Prefer `worker_threads` over `fork` for CPU-bound work that doesn't need full process isolation

## Next

**`10-cluster-worker-threads.md`** covers using multiple CPU cores from within a single Node application — `cluster` for scaling a server across cores, and `worker_threads` for parallel CPU-bound work.
