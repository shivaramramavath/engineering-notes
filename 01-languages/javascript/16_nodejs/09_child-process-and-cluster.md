# Child Process and Cluster

Node runs your JavaScript on one thread. To run other programs, or to use more than one CPU core, you can start **separate processes**:

- `node:child_process`: launch any program (shell commands, other Node scripts) and talk to it
- `node:cluster`: run several copies of the same Node server that share one port

For threads inside one process see [Worker Threads](../17_concurrency-and-parallelism/03_worker-threads.md).

## Choosing a method

| Function | Runs | Output | Use for |
|----------|------|--------|---------|
| `spawn` | A command (no shell by default) | Streams | Long-running or large-output processes (default choice) |
| `execFile` | A file (no shell) | Buffered (stdout, stderr strings) | Short commands with small output |
| `exec` | A command **in a shell** | Buffered | Quick scripts that need shell features (pipes, globs). Risky with user input |
| `fork` | A Node script, with an IPC channel | Streams plus messages | Running Node code in a separate process |
| `*Sync` variants | Same | Blocking | CLI scripts and build tools, never servers |

## `spawn`

```js
import { spawn } from 'node:child_process';

const child = spawn('ls', ['-la', '/tmp']);

child.stdout.on('data', (chunk) => process.stdout.write(chunk));
child.stderr.on('data', (chunk) => process.stderr.write(chunk));

child.on('error', (err) => console.error('failed to start:', err));   // e.g. ENOENT: command not found
child.on('close', (code, signal) => {
  console.log(`exited: code=${code} signal=${signal}`);
});
```

| Property | Meaning |
|----------|---------|
| `child.pid` | Process id |
| `child.stdin` / `stdout` / `stderr` | Streams to and from the process |
| `child.kill([signal])` | Send a signal (`SIGTERM` by default) |
| `child.exitCode` / `child.signalCode` | Result after exit |

Events:

| Event | When |
|-------|------|
| `'spawn'` | Process started |
| `'error'` | Could not start, could not be killed, or messaging failed |
| `'exit'` | Process ended (stdio streams may still be open) |
| `'close'` | Process ended **and** stdio streams are closed (use this one) |

### Options

```js
spawn('node', ['worker.js'], {
  cwd: '/path/to/dir',
  env: { ...process.env, MODE: 'test' },   // by default the child inherits process.env
  stdio: 'inherit',                        // share the parent's stdin/stdout/stderr
  timeout: 10_000,                         // kill after 10 s
  signal: abortController.signal,          // kill on abort
  detached: false,
});
```

`stdio` choices:

| Value | Effect |
|-------|--------|
| `'pipe'` (default) | Streams you read and write in code |
| `'inherit'` | Child uses the parent's terminal streams directly |
| `'ignore'` | Discard (`/dev/null`) |
| `['pipe', 'inherit', 'inherit']` | Mix per stream: stdin, stdout, stderr |

### Piping to and from a child

```js
import { pipeline } from 'node:stream/promises';
import { createReadStream, createWriteStream } from 'node:fs';

const gzip = spawn('gzip', ['-c']);
await pipeline(createReadStream('big.txt'), gzip.stdin).catch(() => {});
await pipeline(gzip.stdout, createWriteStream('big.txt.gz'));
```

Always consume `stdout` and `stderr` (or set `stdio: 'ignore'` / `'inherit'`). If a child fills its pipe buffer and nobody reads it, the child blocks forever.

## Promise wrapper for `spawn`

```js
import { spawn } from 'node:child_process';

function run(cmd, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(cmd, args, options);
    let stdout = '';
    let stderr = '';

    child.stdout?.setEncoding('utf8').on('data', (d) => (stdout += d));
    child.stderr?.setEncoding('utf8').on('data', (d) => (stderr += d));

    child.on('error', reject);
    child.on('close', (code) => {
      if (code === 0) resolve({ stdout, stderr });
      else reject(Object.assign(new Error(`${cmd} exited with ${code}`), { code, stdout, stderr }));
    });
  });
}

const { stdout } = await run('git', ['rev-parse', 'HEAD']);
```

## `execFile` and `exec`

Buffered results with promises via `util.promisify`:

```js
import { execFile, exec } from 'node:child_process';
import { promisify } from 'node:util';

const execFileP = promisify(execFile);

const { stdout } = await execFileP('git', ['status', '--short'], {
  maxBuffer: 10 * 1024 * 1024,     // default is 1 MiB: larger output throws
  timeout: 5000,
});

const { stdout: out2 } = await promisify(exec)('ls | wc -l');   // shell pipe
```

A non-zero exit code rejects the promise; the error has `code`, `stdout`, and `stderr`.

## Shell injection

`exec` passes the string to a shell. Interpolating untrusted input lets an attacker run arbitrary commands:

```js
// DANGEROUS: filename = "a.txt; rm -rf ~"
exec(`cat ${filename}`);

// SAFE: arguments are passed as an array, never parsed by a shell
execFile('cat', [filename]);
spawn('cat', [filename]);
```

Rules:

- Use `execFile` / `spawn` with an **argument array**
- Avoid `shell: true` and `exec` with any user-controlled data
- Validate input and keep a short allow-list of commands
- A filename starting with `-` can still be read as an option by some programs; use `--` before user-supplied paths when the command supports it

## `fork` and IPC

`fork` starts another Node script and opens a message channel (JSON-like, using structured serialization).

```js
// parent.js
import { fork } from 'node:child_process';

const child = fork(new URL('./worker.js', import.meta.url));

child.on('message', (msg) => console.log('parent got:', msg));
child.send({ task: 'sum', numbers: [1, 2, 3] });

child.on('exit', (code) => console.log('worker exited', code));
```

```js
// worker.js
process.on('message', ({ task, numbers }) => {
  if (task === 'sum') {
    process.send({ result: numbers.reduce((a, b) => a + b, 0) });
    process.exit(0);
  }
});
```

| API | Side | Purpose |
|-----|------|---------|
| `child.send(msg)` | Parent | Message to child |
| `child.on('message')` | Parent | Messages from child |
| `process.send(msg)` | Child | Message to parent |
| `process.on('message')` | Child | Messages from parent |
| `child.disconnect()` | Either | Close the channel |

Each forked process has its own V8 instance and memory (tens of MB each), so it is heavier than a worker thread. It also provides **isolation**: a crash in the child does not take down the parent.

## Cleaning up

```js
const child = spawn('long-task');

process.on('SIGINT', () => child.kill('SIGTERM'));   // forward signals
process.on('exit', () => child.kill());               // do not leave orphans

const ac = new AbortController();
const c2 = spawn('sleep', ['60'], { signal: ac.signal });
c2.on('error', (e) => { if (e.name !== 'AbortError') throw e; });
ac.abort();
```

Children are tied to the parent's lifetime by default. With `detached: true` plus `child.unref()` you can let a child outlive the parent (also set `stdio: 'ignore'`).

## Sync versions

```js
import { execFileSync, spawnSync } from 'node:child_process';

const branch = execFileSync('git', ['branch', '--show-current'], { encoding: 'utf8' }).trim();

const r = spawnSync('npm', ['test'], { stdio: 'inherit' });
process.exitCode = r.status ?? 1;
```

Fine for build scripts and CLIs; never in a request handler.

## Platform notes

- On Windows, `.cmd` and `.bat` files (like `npm.cmd`) need `shell: true`, or the full file name; `.exe` files do not
- Signals other than `SIGTERM`/`SIGKILL`-style termination have limited meaning on Windows
- Commands differ between systems (`ls` vs `dir`); prefer Node APIs or cross-platform tools

## Cluster: using every core

A single Node process uses one core for JavaScript. The `cluster` module starts several worker processes that **share the same server port**. The primary process distributes incoming connections.

```js
import cluster from 'node:cluster';
import http from 'node:http';
import os from 'node:os';

if (cluster.isPrimary) {
  const count = os.availableParallelism();
  console.log(`primary ${process.pid} starting ${count} workers`);

  for (let i = 0; i < count; i++) cluster.fork();

  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker ${worker.process.pid} died (${signal ?? code}), restarting`);
    cluster.fork();                      // simple self-healing
  });
} else {
  http.createServer((req, res) => {
    res.end(`handled by ${process.pid}\n`);
  }).listen(3000);                       // all workers listen on the same port
}
```

How it works:

- `cluster.isPrimary` is true in the original process; workers are forked copies running the same file with `cluster.isWorker` true
- On Linux and macOS the primary accepts connections and hands them to workers round-robin (the default); Windows lets the OS decide
- Workers are separate processes: **no shared memory**, each has its own heap, caches, and global variables
- Workers and primary can exchange messages like `fork` (`worker.send`, `process.send`)

### Graceful restart (rolling)

```js
if (cluster.isPrimary) {
  process.on('SIGUSR2', async () => {
    for (const worker of Object.values(cluster.workers)) {
      const replacement = cluster.fork();
      await new Promise((r) => replacement.once('listening', r));
      worker.disconnect();               // stop taking connections, finish current ones
    }
  });
} else {
  process.on('SIGTERM', () => server.close(() => process.exit(0)));
}
```

### Sharing state between workers

Since memory is not shared, move shared state out of process:

| Need | Solution |
|------|----------|
| Sessions, caches, counters | Redis or another external store |
| Rate limiting across workers | Shared store, not an in-memory `Map` |
| WebSocket broadcast | Pub/sub (Redis, a message broker) |
| Background jobs | A queue |

An in-memory `Map` cache is duplicated and diverges across workers.

## Cluster vs worker threads vs a process manager

| | `cluster` | `worker_threads` | Process manager / orchestrator |
|---|-----------|------------------|-------------------------------|
| Unit | Process | Thread in one process | Process or container |
| Memory | Separate | Separate heaps; can share `SharedArrayBuffer` | Separate |
| Best for | Scaling an I/O-bound server across cores | CPU-heavy tasks inside one app | Production restarts, scaling, logs |
| Port sharing | Built in | No (one thread owns the server) | Load balancer in front |
| Isolation | Strong (crash is contained) | Weaker | Strong |

In modern deployments, running **one Node process per container** and scaling containers (Kubernetes, ECS) often replaces `cluster`. PM2 and similar tools can also run cluster mode for you. Use `cluster` directly when you manage a single multi-core machine yourself.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `exec` with user input | Shell injection | `execFile` / `spawn` with an args array |
| Not reading `stdout` / `stderr` | Child blocks when its pipe fills | Consume or `ignore` / `inherit` |
| `exec` with large output | `maxBuffer` exceeded error | `spawn` and stream |
| Listening to `'exit'` and reading output | Streams may not be finished | Use `'close'` |
| Forgetting the `'error'` handler | Crash on `ENOENT` | Always handle it |
| Orphaned children | Resource leaks | Forward signals, `kill` on exit |
| In-memory state with `cluster` | Each worker has different data | External store |
| Auto-restarting crashed workers in a tight loop | Crash loop burns CPU | Add backoff and a restart limit |
| `*Sync` child process calls in servers | Blocks the event loop | Async variants |
| Using `cluster` for CPU-heavy tasks inside one request | Still blocks that worker | Worker threads or a job queue |
| Assuming `npm` runs without a shell on Windows | `ENOENT` | `shell: true` or `npm.cmd` |

## Key takeaways

- `spawn` for streams, `execFile` for small buffered output, `fork` for Node children with messaging, `exec` only with trusted strings
- Never interpolate user input into a shell command
- Handle `'error'`, consume the output streams, and wait for `'close'`
- `cluster` runs one server per core as separate processes sharing a port; there is no shared memory
- Keep shared state in an external store
- Choose between `cluster`, `worker_threads`, and container-level scaling by workload and deployment

**Next:** [Concurrency and Parallelism](../17_concurrency-and-parallelism/00_README.md)
