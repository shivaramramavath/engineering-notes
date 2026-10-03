# Node Runtime

Node.js is a **runtime**: it embeds the V8 engine, adds an event loop and asynchronous I/O (via libuv), and ships a standard library of built-in modules. The language is still JavaScript; the environment around it is different from a browser.

## Architecture

| Layer | Role |
|-------|------|
| **V8** | Compiles and runs JavaScript (same engine as Chrome) |
| **libuv** | Event loop, thread pool, async file, network, and timer I/O |
| **C++ bindings** | Connect JavaScript APIs (`fs`, `net`, `crypto`) to native code |
| **Core modules** | `fs`, `http`, `path`, `stream`, `events`, `os`, ... written in JS on top of the bindings |
| **Third-party OpenSSL, zlib, llhttp, c-ares, ...** | Crypto, compression, HTTP parsing, DNS |

```
your code → core modules (JS) → bindings (C++) → libuv / OS
                    ↑
                    V8 runs the JS
```

## Browser vs Node

| | Browser | Node.js |
|---|---------|---------|
| Global object | `window` / `globalThis` | `globalThis` (`global` is an alias) |
| DOM, `document`, `localStorage` | Yes | No |
| File system, processes, raw sockets | No | Yes |
| Modules | ES modules | ES modules and CommonJS |
| Security model | Sandboxed | Runs with the user's permissions (opt-in permission model exists) |
| `fetch`, `URL`, `AbortController`, `structuredClone` | Yes | Yes (built in) |

## Running code

```bash
node app.js                 # run a file
node -e "console.log(1+1)"  # evaluate a string
node                        # start the REPL
node --watch app.js         # restart on file changes
node --env-file=.env app.js # load environment variables from a file
node --test                 # run the built-in test runner
```

Useful flags:

| Flag | Purpose |
|------|---------|
| `--watch` | Restart when imported files change |
| `--env-file=<path>` | Load a `.env` file into `process.env` |
| `--inspect` / `--inspect-brk` | Attach Chrome DevTools or VS Code debugger |
| `--max-old-space-size=<MB>` | Raise the V8 heap limit |
| `--trace-warnings` | Show stack traces for process warnings |
| `--test` | Discover and run tests with `node:test` |
| `--permission` | Opt in to the permission model (restrict fs, child processes, ...) |

Recent Node versions can also run TypeScript files that use only erasable syntax (type stripping). Check `node --version` and the release notes for what your version supports.

## Globals

Available without importing:

```js
globalThis;                     // the global object
process;                        // current process (see 03_process-and-env.md)
Buffer;                         // binary data (see 07_buffers.md)
console;
setTimeout; setInterval; setImmediate;
queueMicrotask; structuredClone;
URL; URLSearchParams;
TextEncoder; TextDecoder;
AbortController; AbortSignal;
fetch; Request; Response; Headers; FormData;
EventTarget; Event;
performance;                    // high-resolution timing
crypto;                         // Web Crypto (also node:crypto)
```

CommonJS files also get module-scoped helpers: `require`, `module`, `exports`, `__dirname`, `__filename`. ES modules do not. Use `import.meta.dirname` and `import.meta.filename` (Node 20.11+) instead.

## Built-in modules

Import core modules with the `node:` prefix. It makes it clear the import is built in and cannot be shadowed by an npm package.

```js
import fs from 'node:fs/promises';
import path from 'node:path';
import { EventEmitter } from 'node:events';
import os from 'node:os';
```

| Module | Use |
|--------|-----|
| `node:fs`, `node:fs/promises` | Files and directories |
| `node:path`, `node:url` | Paths and URLs |
| `node:os` | CPU, memory, platform info |
| `node:events` | `EventEmitter` |
| `node:stream`, `node:stream/promises` | Streaming data |
| `node:buffer` | Binary data |
| `node:http`, `node:https`, `node:http2`, `node:net` | Networking |
| `node:child_process`, `node:cluster`, `node:worker_threads` | Processes and threads |
| `node:crypto` | Hashing, encryption, random bytes |
| `node:zlib` | gzip, brotli, deflate |
| `node:util` | `promisify`, `parseArgs`, `inspect`, `styleText` |
| `node:assert`, `node:test` | Assertions and test runner |
| `node:readline` | Interactive input line by line |

## Modules in Node

Node supports two systems (full comparison in [ESM vs CommonJS](../13_modules/03_esm-vs-commonjs.md)):

- **ES modules**: `import`/`export`. Used when the file is `.mjs`, or `.js` inside a package with `"type": "module"`.
- **CommonJS**: `require`/`module.exports`. Used for `.cjs`, or `.js` in a package without `"type": "module"`.

```json
{
  "name": "my-app",
  "type": "module",
  "engines": { "node": ">=20" }
}
```

Top-level `await` works in ES modules:

```js
import fs from 'node:fs/promises';
const config = JSON.parse(await fs.readFile('./config.json', 'utf8'));
```

## Useful built-ins you may not expect

```js
// Built-in test runner
import { test } from 'node:test';
import assert from 'node:assert/strict';

test('adds', () => {
  assert.equal(1 + 1, 2);
});

// Argument parsing
import { parseArgs } from 'node:util';
const { values } = parseArgs({ options: { port: { type: 'string', default: '3000' } } });

// Promise versions of timers
import { setTimeout as sleep } from 'node:timers/promises';
await sleep(500);

// Info about the machine
import os from 'node:os';
os.availableParallelism();   // usable CPU count
os.totalmem();               // bytes
```

## Versions and LTS

- Node publishes a new major release every six months; **even-numbered** majors become **LTS** (Long Term Support)
- Use an **active LTS** release for production; use `engines` in `package.json` to document the minimum
- Manage versions with `nvm`, `fnm`, `volta`, or `mise`
- Check what a version supports before using newer APIs: https://nodejs.org/docs/latest/api/

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Importing core modules without `node:` | Can be shadowed by a package of the same name | `import fs from 'node:fs'` |
| Using `__dirname` in ES modules | `ReferenceError` | `import.meta.dirname` |
| Mixing `require` and `import` carelessly | Interop errors | Pick one system per package |
| Running production on an odd (non-LTS) release | Short support window | Use active LTS |
| Assuming browser APIs exist (`window`, `document`) | `ReferenceError` | Use `globalThis`, check features |
| No `engines` field | Silent failures on old Node | Declare the minimum version |

## Key takeaways

- Node = V8 + libuv + C++ bindings + a standard library
- Use the `node:` prefix for built-in modules
- `globalThis` is the global; `window` and `document` do not exist
- Modern Node ships `fetch`, a test runner, `--watch`, and `--env-file`: you often need fewer dependencies than you think
- Run an LTS release in production

**Next:** [Node Event Loop](./02_node-event-loop.md)
