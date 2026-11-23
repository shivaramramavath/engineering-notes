# Child Processes and Cluster in Node.js

> Multi-process concurrency, sub-process execution, IPC message channels, and multi-core CPU scaling with the Node.js cluster module.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Node.js Streams](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/06_streams.md), [Events](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/05_events.md), [Process and Env](file:///e:/Learning/engineering-notes/01-languages/javascript/16_nodejs/03_process-and-env.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | Node.js v18 LTS - v24+, POSIX & Windows |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Because Node.js runs JavaScript on a single thread inside a single OS process, heavy CPU computations (cryptographic key derivation, video transcoding, PDF generation, large dataset filtering) block the event loop, starving all I/O operations.

The `node:child_process` and `node:cluster` modules provide process-level parallelism. `child_process` allows executing arbitrary system binaries (`git`, `ffmpeg`, shell commands) or spawning auxiliary Node.js processes with dedicated Inter-Process Communication (IPC) channels. `cluster` builds upon this foundation, allowing a primary process to fork worker processes that share the same listening TCP port, distributing incoming traffic across all available CPU cores.

---

## 2. Core Concepts

### Process Creation Primitives
Node.js exposes four distinct functions for spawning sub-processes:
1. `spawn(command, [args], [options])`: Streams data via standard streams (`stdin`, `stdout`, `stderr`). Ideal for large data streams.
2. `exec(command, [options], [callback])`: Runs a command inside a system shell (`/bin/sh` or `cmd.exe`) and buffers the entire output in memory. Vulnerable to shell injection if user input is interpolated.
3. `execFile(file, [args], [options], [callback])`: Invokes the executable directly without spawning a shell, providing higher security and performance.
4. `fork(modulePath, [args], [options])`: A specialized `spawn()` that launches a fresh Node.js instance with an established bidirectional IPC channel (`process.send()` and `child.on('message')`).

### The Cluster Architecture
The `node:cluster` module implements a master/worker (or primary/worker) model.
- **Primary Process**: Owns the master socket, binds to the network port, and accepts incoming connections.
- **Worker Processes**: Isolated V8 instances executing application logic. On POSIX systems, the primary passes accepted socket file descriptors to workers using round-robin scheduling.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| PRIMARY PROCESS (PID 1000)                                               |
|   - Binds to Port 3000 (net.Server) via libuv uv_tcp_bind                |
|   - Distributes incoming TCP socket handles via IPC UNIX Domain Sockets  |
+--------------------------------------------------------------------------+
          |                             |                             |
          | IPC Socket Pass (SCM_RIGHTS)| IPC Socket Pass (SCM_RIGHTS)| IPC
          v                             v                             v
+-----------------------+     +-----------------------+     +---------------+
| WORKER 1 (PID 1001)   |     | WORKER 2 (PID 1002)   |     | WORKER 3 ...  |
| - Own V8 Heap         |     | - Own V8 Heap         |     | - Own V8 Heap |
| - Own Event Loop      |     | - Own Event Loop      |     |               |
| - Receives socket fd  |     | - Receives socket fd  |     |               |
| - Parses HTTP request |     | - Parses HTTP request |     |               |
| - Sends HTTP response |     | - Sends HTTP response |     |               |
+-----------------------+     +-----------------------+     +---------------+
```

---

## 4. Syntax & API Reference

### child_process Functions
```javascript
import { spawn, exec, execFile, fork } from 'node:child_process';

// 1. spawn: Streaming output
const child = spawn('git', ['status', '--porcelain']);
child.stdout.on('data', (chunk) => console.log(chunk.toString()));

// 2. exec: Shell execution (buffered)
exec('ls -la', (err, stdout, stderr) => {
  if (err) console.error(err);
  else console.log(stdout);
});

// 3. execFile: Safe direct execution
execFile('node', ['--version'], (err, stdout) => {
  console.log(stdout.trim());
});

// 4. fork: Node sub-process with IPC
const worker = fork('./worker.js');
worker.send({ type: 'START', payload: [1, 2, 3] });
worker.on('message', (msg) => console.log('Received from worker:', msg));
```

### cluster Module API
```javascript
import cluster from 'node:cluster';
import os from 'node:os';

if (cluster.isPrimary) {
  const numCPUs = os.cpus().length;
  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  cluster.on('exit', (worker, code, signal) => {
    console.log(`Worker ${worker.process.pid} died. Respawning...`);
    cluster.fork();
  });
} else {
  // Worker processes execute server code
}
```

---

## 5. Practical Examples

### Example 1: Streaming CLI Tools with `spawn`
```javascript
import { spawn } from 'node:child_process';

function runGzipCompression(inputPath, outputPath) {
  return new Promise((resolve, reject) => {
    const gzip = spawn('tar', ['-czf', outputPath, inputPath]);

    gzip.stderr.on('data', (data) => {
      console.error(`tar error: ${data}`);
    });

    gzip.on('close', (code) => {
      if (code === 0) resolve();
      else reject(new Error(`Process exited with code ${code}`));
    });

    gzip.on('error', reject);
  });
}
```

### Example 2: Robust IPC Message Broker Between Parent and Forked Child
```javascript
// parent.js
import { fork } from 'node:child_process';
import path from 'node:path';

const child = fork(path.resolve('compute-worker.js'));

function computeHeavyHash(data) {
  return new Promise((resolve, reject) => {
    const correlationId = crypto.randomUUID();

    function handler(message) {
      if (message.id === correlationId) {
        child.removeListener('message', handler);
        if (message.error) reject(new Error(message.error));
        else resolve(message.result);
      }
    }

    child.on('message', handler);
    child.send({ id: correlationId, type: 'HASH', payload: data });
  });
}

// compute-worker.js
import crypto from 'node:crypto';

process.on('message', (msg) => {
  try {
    if (msg.type === 'HASH') {
      const hash = crypto.pbkdf2Sync(msg.payload, 'salt', 100000, 64, 'sha512');
      process.send({ id: msg.id, result: hash.toString('hex') });
    }
  } catch (err) {
    process.send({ id: msg.id, error: err.message });
  }
});
```

### Example 3: Multi-Core HTTP Server with Zero-Downtime Worker Restarts
```javascript
import cluster from 'node:cluster';
import http from 'node:http';
import os from 'node:os';

const numCPUs = os.availableParallelism ? os.availableParallelism() : os.cpus().length;

if (cluster.isPrimary) {
  console.log(`Primary ${process.pid} is orchestrating ${numCPUs} workers`);

  for (let i = 0; i < numCPUs; i++) {
    cluster.fork();
  }

  // Graceful rolling reload on SIGUSR2
  process.on('SIGUSR2', async () => {
    console.log('Reloading workers sequentially...');
    const workers = Object.values(cluster.workers);

    for (const worker of workers) {
      const newWorker = cluster.fork();
      await new Promise(resolve => newWorker.on('listening', resolve));
      worker.disconnect();
    }
  });

  cluster.on('exit', (worker, code, signal) => {
    if (!worker.exitedAfterDisconnect) {
      console.warn(`Worker ${worker.process.pid} crashed (${signal || code}). Respawning...`);
      cluster.fork();
    }
  });
} else {
  const server = http.createServer((req, res) => {
    res.writeHead(200);
    res.end(`Handled by worker PID ${process.pid}\n`);
  });

  server.listen(3000, () => {
    console.log(`Worker ${process.pid} listening on port 3000`);
  });
}
```

### Example 4: Enforcing Process Execution Timeouts and Resource Limits
```javascript
import { execFile } from 'node:child_process';

function runSecureBinary(binaryPath, args, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    execFile(binaryPath, args, {
      timeout: timeoutMs,
      killSignal: 'SIGKILL',
      maxBuffer: 1024 * 1024, // 1MB output buffer maximum
      windowsHide: true,
    }, (error, stdout, stderr) => {
      if (error) {
        if (error.killed) {
          reject(new Error(`Execution timed out after ${timeoutMs}ms`));
        } else {
          reject(error);
        }
        return;
      }
      resolve({ stdout: stdout.trim(), stderr: stderr.trim() });
    });
  });
}
```

### Example 5: Detached Background Daemon Process
```javascript
import { spawn } from 'node:child_process';
import fs from 'node:fs';

function launchDaemon(scriptPath) {
  const out = fs.openSync('./daemon.log', 'a');
  const err = fs.openSync('./daemon-error.log', 'a');

  const sub = spawn('node', [scriptPath], {
    detached: true,
    stdio: ['ignore', out, err], // Detach standard file descriptors
  });

  sub.unref(); // Allow the parent process to terminate without waiting for child
  console.log(`Daemon process launched with PID ${sub.pid}`);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`exec` Command Injection**: If any part of the command string passed to `exec()` contains unvalidated user input, attackers can chain shell commands via `;`, `|`, or backticks. Always use `execFile()` or `spawn()` with argument arrays.
2. **`maxBuffer` Exhaustion in `exec`**: By default, `exec()` buffers stdout/stderr up to 1 MB. If a command prints 1.01 MB, the process throws `ERR_CHILD_PROCESS_STDIO_MAXBUFFER` and abruptly terminates. Use `spawn()` for arbitrary or streaming outputs.
3. **Zombie Processes on Unix**: If a child process terminates while the parent is still running and does not wait on its exit status or attach an `'exit'` / `'close'` listener, the operating system keeps its PID entry in the kernel process table as a zombie (`<defunct>`).
4. **Cluster Port Conflicts**: In Node.js cluster mode, only the primary binds the physical network port. If multiple standalone Node scripts or worker threads attempt to call `server.listen(3000)` without the cluster module, they throw `EADDRINUSE`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `exec('rm -rf ' + userDir)` | Remote Code Injection vulnerability | Use `execFile('rm', ['-rf', userDir])` |
| Over-forking beyond CPU cores | Increases context-switching overhead and CPU contention | Fork at most `os.availableParallelism()` workers |
| Storing session state in cluster worker memory | Requests from the same user hit different workers without sessions | Use Redis or external stores for distributed session state |
| Missing error handler on `child.stdin` | Throws unhandled `EPIPE` if child exits before write finishes | Add `child.stdin.on('error', ...)` handler |
| Using `execSync` inside an HTTP request | Completely freezes the event loop for all concurrent requests | Use asynchronous `spawn` or `execFile` with promises |
| Forgetting `sub.unref()` on detached processes | Keeps the parent event loop alive indefinitely | Call `sub.unref()` if parent must exit immediately |

---

## 8. Performance Characteristics

- **Memory Footprint**: Each forked Node.js process requires its own independent V8 instance, consuming roughly 30–60 MB of base RAM. For thousands of concurrent tasks, use `worker_threads` instead of `child_process`.
- **IPC Serialization Cost**: Messages sent via `child.send()` are serialized to JSON strings and written across a local socket pipe. Passing multi-megabyte payloads incurs significant CPU serialization and deserialization overhead.
- **Round-Robin Scheduling**: On Unix, the primary cluster process uses round-robin load distribution. On Windows, socket distribution is delegated to the OS kernel, which may result in uneven worker load.

---

## 9. Engine Internals

1. **`uv_spawn` Implementation**:
   - In libuv, `uv_spawn` uses `fork()` and `execvp()` on POSIX, setting up pipe file descriptors for `stdin`, `stdout`, and `stderr`.
   - On Windows, it calls `CreateProcessW` and initializes asynchronous I/O completion ports (IOCP) on the pipe handles.
2. **Socket Passing via IPC (`SCM_RIGHTS`)**:
   - The primary listens on the TCP port. When a connection handshake completes, libuv receives the raw socket file descriptor.
   - Using Unix Domain Sockets (`AF_UNIX`) and the `sendmsg()` system call with `SCM_RIGHTS` control messages, the primary transmits the file descriptor directly into the worker's file descriptor table.

---

## 10. Best Practices & Design Guidelines

- **Use `os.availableParallelism()`**: Available since Node.js v19.4.0, this respects container CPU limits (such as Docker cgroups), unlike `os.cpus().length` which returns total host physical cores.
- **Implement Heartbeats**: Ping child processes periodically over IPC. If a worker fails to respond within a timeout window, force kill (`SIGKILL`) and restart it.
- **Externalize Shared State**: Since cluster workers share no memory, design stateless application servers relying on databases, caches, and message queues.

---

## 11. Real-World Case Study

### Resilient Background Task Runner with Heartbeat Detection
```javascript
// task-supervisor.js
import { fork } from 'node:child_process';
import path from 'node:path';

class TaskSupervisor {
  constructor(workerScript) {
    this.workerScript = workerScript;
    this.child = null;
    this.heartbeatTimer = null;
    this.lastHeartbeat = Date.now();
  }

  start() {
    this.child = fork(path.resolve(this.workerScript));
    this.lastHeartbeat = Date.now();

    this.child.on('message', (msg) => {
      if (msg.type === 'PONG') {
        this.lastHeartbeat = Date.now();
      }
    });

    this.child.on('exit', (code) => {
      console.warn(`Worker died with code ${code}. Restarting...`);
      clearInterval(this.heartbeatTimer);
      this.start();
    });

    // Check heartbeat every 3 seconds
    this.heartbeatTimer = setInterval(() => {
      if (Date.now() - this.lastHeartbeat > 7000) {
        console.error('Worker heartbeat timeout. Killing unresponsive process...');
        this.child.kill('SIGKILL');
        clearInterval(this.heartbeatTimer);
        return;
      }
      this.child.send({ type: 'PING' });
    }, 3000);
  }
}

// Instantiate supervisor
const supervisor = new TaskSupervisor('./heavy-task.js');
supervisor.start();
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between exec and execFile?</summary>
<code>exec()</code> invokes an OS shell (like <code>/bin/sh</code> or <code>cmd.exe</code>) to interpret and execute the command string, making it vulnerable to shell injection attacks. <code>execFile()</code> directly invokes the target executable binary without spawning an intermediary shell, passing arguments safely as an array.
</details>

<details>
<summary>2. How does Node.js cluster share a single network port among multiple workers?</summary>
The primary process opens and binds the TCP server socket. When incoming TCP connections arrive, the primary distributes the underlying socket file descriptors to the worker processes over IPC pipes using <code>sendmsg()</code> with <code>SCM_RIGHTS</code> on POSIX, allowing workers to read and write directly to the client connection.
</details>

<details>
<summary>3. What is the difference between child_process.fork() and worker_threads?</summary>
<code>child_process.fork()</code> spawns a completely independent operating system process with its own V8 instance, memory space, and event loop (~30MB+ RAM overhead). <code>worker_threads</code> spawn threads within the <i>same</i> OS process, sharing memory address space via <code>SharedArrayBuffer</code> and incurring much lower resource overhead (~1-2MB).
</details>

<details>
<summary>4. What happens when stdout exceeds maxBuffer in child_process.exec()?</summary>
The parent process terminates the child sub-process via the configured kill signal (default <code>SIGTERM</code>) and invokes the callback with an <code>ERR_CHILD_PROCESS_STDIO_MAXBUFFER</code> error.
</details>

<details>
<summary>5. How does cluster handle worker crashes?</summary>
When a worker crashes, the cluster module's primary process receives an <code>'exit'</code> event. Application code can inspect <code>worker.exitedAfterDisconnect</code> and fork a replacement worker to maintain optimal system capacity.
</details>

<details>
<summary>6. Why is os.availableParallelism() preferred over os.cpus().length?</summary>
<code>os.cpus().length</code> returns the total physical and logical CPU cores on the host machine. In containerized environments (Docker, Kubernetes) with CPU quotas, it overreports capacity. <code>os.availableParallelism()</code> inspects cgroup quotas and returns the actual CPU allotment available to the container.
</details>

<details>
<summary>7. What causes an EPIPE error when writing to a child process?</summary>
An <code>EPIPE</code> error occurs when the parent writes to <code>child.stdin</code> after the child process has already closed its standard input stream or abruptly terminated.
</details>

<details>
<summary>8. How do you implement zero-downtime restarts with the cluster module?</summary>
By performing rolling reloads: fork a new replacement worker, wait for its <code>'listening'</code> event, and only then call <code>disconnect()</code> on the old worker so it finishes in-flight requests before exiting cleanly.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Resilient Worker Pool with Task Queue
Implement a master-worker dispatcher using `child_process.fork()` that maintains a pool of 2 workers and distributes computationally heavy prime number generation tasks from a shared FIFO queue.

<details>
<summary>View Solution</summary>

```javascript
import { fork } from 'node:child_process';

class SimpleWorkerPool {
  constructor(workerPath, poolSize = 2) {
    this.workerPath = workerPath;
    this.poolSize = poolSize;
    this.workers = [];
    this.available = [];
    this.queue = [];
    this.init();
  }

  init() {
    for (let i = 0; i < this.poolSize; i++) {
      const worker = fork(this.workerPath);
      this.workers.push(worker);
      this.available.push(worker);

      worker.on('message', (msg) => {
        const { taskId, result, error } = msg;
        const task = this.inFlight?.get(taskId);
        if (task) {
          this.inFlight.delete(taskId);
          if (error) task.reject(new Error(error));
          else task.resolve(result);
        }
        this.available.push(worker);
        this.drain();
      });
    }
    this.inFlight = new Map();
  }

  dispatch(taskData) {
    return new Promise((resolve, reject) => {
      const taskId = crypto.randomUUID();
      this.queue.push({ taskId, taskData, resolve, reject });
      this.drain();
    });
  }

  drain() {
    while (this.available.length > 0 && this.queue.length > 0) {
      const worker = this.available.shift();
      const task = this.queue.shift();
      this.inFlight.set(task.taskId, task);
      worker.send({ taskId: task.taskId, data: task.taskData });
    }
  }
}
```
</details>

### Exercise 2: Graceful Rolling Worker Upgrades
Write a script that creates a cluster of HTTP workers and, upon receiving a custom IPC signal `'RELOAD'`, restarts workers one-by-one without dropping active HTTP connections.

<details>
<summary>View Solution</summary>

```javascript
import cluster from 'node:cluster';
import http from 'node:http';
import os from 'node:os';

if (cluster.isPrimary) {
  const count = 2;
  for (let i = 0; i < count; i++) {
    cluster.fork();
  }

  async function restartWorkers() {
    const activeWorkers = Object.values(cluster.workers);
    for (const oldWorker of activeWorkers) {
      const newWorker = cluster.fork();

      await new Promise((resolve) => {
        newWorker.on('listening', resolve);
      });

      oldWorker.disconnect();
      await new Promise((resolve) => {
        oldWorker.on('disconnect', resolve);
      });
    }
  }

  process.on('SIGUSR2', restartWorkers);
} else {
  http.createServer((req, res) => {
    res.writeHead(200);
    res.end(`PID: ${process.pid}\n`);
  }).listen(3000);
}
```
</details>
