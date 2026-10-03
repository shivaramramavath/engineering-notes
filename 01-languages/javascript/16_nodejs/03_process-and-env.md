# Process and Env

`process` is a global object that describes the running Node process and lets you interact with it: command-line arguments, environment variables, standard streams, signals, and exit.

## Arguments

```bash
node app.js build --out dist --verbose
```

```js
process.argv;
// [
//   '/usr/local/bin/node',    // [0] path to the node executable
//   '/path/to/app.js',        // [1] path to the script
//   'build', '--out', 'dist', '--verbose'
// ]

const args = process.argv.slice(2);   // your arguments
```

Parse flags with the built-in `util.parseArgs`:

```js
import { parseArgs } from 'node:util';

const { values, positionals } = parseArgs({
  options: {
    out:     { type: 'string',  short: 'o', default: 'dist' },
    verbose: { type: 'boolean', short: 'v', default: false },
  },
  allowPositionals: true,
});

console.log(positionals, values);   // ['build'] { out: 'dist', verbose: true }
```

## Environment variables

`process.env` is an object of **strings**. Missing variables are `undefined`.

```js
process.env.NODE_ENV;                 // 'production' | undefined
process.env.PORT = '4000';            // changes this process (and children it spawns)

const port = Number(process.env.PORT ?? 3000);
const debug = process.env.DEBUG === '1';   // booleans are strings: parse them
```

Every value is converted to a string:

```js
process.env.FLAG = false;
process.env.FLAG;          // 'false' (truthy!)
```

### Loading a `.env` file

```bash
node --env-file=.env app.js
```

```js
process.loadEnvFile('.env');   // load from code (Node 20.12+ / 21.7+)
```

```ini
# .env
PORT=3000
DATABASE_URL=postgres://localhost/app
```

### Validate config at startup

```js
function requireEnv(name) {
  const v = process.env[name];
  if (!v) throw new Error(`Missing required env var: ${name}`);
  return v;
}

export const config = {
  port: Number(process.env.PORT ?? 3000),
  databaseUrl: requireEnv('DATABASE_URL'),
};
```

Never commit `.env` files with secrets; add them to `.gitignore`.

## Standard streams

```js
process.stdout.write('no newline');   // console.log uses stdout
process.stderr.write('error output'); // console.error uses stderr

// Read all of stdin
let input = '';
for await (const chunk of process.stdin) input += chunk;
```

```bash
echo "hello" | node upper.js        # stdin from a pipe
node app.js > out.txt 2> err.txt    # redirect stdout and stderr separately
```

Check whether output is a terminal:

```js
if (process.stdout.isTTY) { /* colors, spinners */ }
```

For line-by-line interactive input use `node:readline/promises`:

```js
import readline from 'node:readline/promises';

const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
const name = await rl.question('Name? ');
rl.close();
```

## Process info

```js
process.pid;                 // process id
process.ppid;                // parent process id
process.platform;            // 'linux' | 'darwin' | 'win32'
process.arch;                // 'x64' | 'arm64'
process.version;             // 'v22.x.x'
process.versions;            // { node, v8, uv, ... }
process.cwd();               // current working directory
process.chdir('/tmp');
process.uptime();            // seconds since start
process.execPath;            // path to the node binary
```

## Resource usage and timing

```js
process.memoryUsage();
// { rss, heapTotal, heapUsed, external, arrayBuffers }  (bytes)

process.cpuUsage();          // { user, system } in microseconds

const t0 = process.hrtime.bigint();
doWork();
const ms = Number(process.hrtime.bigint() - t0) / 1e6;   // monotonic, nanosecond clock
```

`performance.now()` is also available as a global and is fine for most timing.

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Uncaught fatal exception / general failure |
| `2` | Misuse of shell command (convention) |
| `130` | Terminated by Ctrl+C (convention: 128 + SIGINT) |

```js
process.exitCode = 1;   // preferred: sets the code, lets pending work finish
process.exit(1);        // exits immediately: pending writes and timers are dropped
```

Prefer `process.exitCode` so buffered output (especially to pipes) is flushed.

```js
try {
  await main();
} catch (err) {
  console.error(err);
  process.exitCode = 1;
}
```

## Process events

```js
process.on('exit', (code) => {
  // synchronous only: the loop is already gone
  console.log('exiting with', code);
});

process.on('beforeExit', () => {
  // fires when the loop is empty; scheduling async work here keeps the process alive
});
```

### Uncaught errors and rejections

```js
process.on('uncaughtException', (err, origin) => {
  console.error('Fatal:', err, origin);
  process.exit(1);          // state may be corrupt: log, clean up, exit
});

process.on('unhandledRejection', (reason) => {
  console.error('Unhandled rejection:', reason);
  process.exit(1);          // since Node 15 this crashes by default; make it explicit
});
```

Do not use `uncaughtException` to keep running as if nothing happened. See [Production Error Handling](../10_error-handling/05_production-error-handling.md).

## Signals and graceful shutdown

| Signal | Typical source |
|--------|----------------|
| `SIGINT` | Ctrl+C |
| `SIGTERM` | `kill <pid>`, Docker stop, Kubernetes, systemd |
| `SIGHUP` | Terminal closed, config reload convention |
| `SIGKILL` | Cannot be caught or ignored |

```js
let shuttingDown = false;

async function shutdown(signal) {
  if (shuttingDown) return;
  shuttingDown = true;
  console.log(`${signal} received, closing...`);
  try {
    await server.close();       // stop accepting, finish in-flight work
    await db.end();
    process.exitCode = 0;
  } catch (err) {
    console.error(err);
    process.exitCode = 1;
  }
}

process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
```

Send a signal to another process:

```js
process.kill(pid, 'SIGTERM');
process.kill(pid, 0);        // throws if the process does not exist (existence check)
```

On Windows only a few signals are supported (`SIGINT`, `SIGTERM` in limited form).

## Warnings

```js
process.emitWarning('Feature X is deprecated', 'DeprecationWarning');
process.on('warning', (w) => console.warn(w.name, w.message));
```

```bash
node --trace-warnings app.js
node --no-deprecation app.js
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Treating `process.env` values as booleans or numbers | They are always strings | Parse explicitly |
| `process.exit()` right after writes | Output can be truncated | Set `process.exitCode`, let the loop drain |
| Swallowing `uncaughtException` | Continuing in an unknown state | Log and exit; let a supervisor restart |
| No `SIGTERM` handler | Containers get killed mid-request after the grace period | Graceful shutdown |
| Reading `process.argv[2]` blindly | Fragile with flags | `util.parseArgs` |
| Committing `.env` | Leaks secrets | `.gitignore`, secret managers |
| Async work in `'exit'` handlers | Never runs | Do async cleanup before exiting |

## Key takeaways

- `process.argv.slice(2)` gives your arguments; use `util.parseArgs` for flags
- `process.env` values are strings; validate required config at startup
- Prefer `process.exitCode` over `process.exit()`
- Handle `SIGINT` and `SIGTERM` to shut down gracefully
- After an `uncaughtException`, log and exit

**Next:** [Filesystem and Path](./04_filesystem-and-path.md)
