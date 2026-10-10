# Node.js Types

Node.js is not part of JavaScript itself, so TypeScript knows nothing about `fs`, `process`, `Buffer`, or `http` until you give it the **type declarations** for them. Those come from the `@types/node` package. Getting this setup right (the right version, the right `lib`, no accidental browser types) removes a surprising amount of friction, and knowing the handful of Node-specific typing quirks saves time later.

**Prerequisites:**
- [Third-party types](../09-declaration-files/04-third-party-types.md)
- [Target, module, and lib](../13-compiler-and-tsconfig/02-target-module-and-lib.md)
- [ES modules and CommonJS](../08-modules/01-es-modules-and-commonjs.md)

---

## Setup

```bash
npm install --save-dev typescript @types/node
```

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022"],                 // no "DOM": this is not a browser
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "types": ["node"],                 // optional: pins which @types packages load as globals
    "strict": true
  }
}
```

Three things matter here:

- **Match `@types/node` to your Node major version.** The package is versioned by Node release (`@types/node@20.x` for Node 20). A mismatch shows up as APIs that exist at runtime but not in the types, or the reverse. Pin it to the version you deploy on.
- **Leave `"DOM"` out of `lib`** for server code. With it, browser globals (`window`, `document`, DOM `fetch` types) type-check on the server and fail at runtime. It also causes subtle clashes between DOM and Node typings.
- **Use a community base** (such as `@tsconfig/node20`) if you want vetted `target`, `lib`, and `module` values for your Node version ([tsconfig recipes](../13-compiler-and-tsconfig/05-tsconfig-recipes.md)).

## `node:` imports

Import Node's built-in modules with the `node:` prefix:

```ts
import { readFile } from "node:fs/promises";
import path from "node:path";
import { createServer } from "node:http";
import { randomUUID } from "node:crypto";
```

The prefix makes it unambiguous that you mean the built-in module, not an npm package with the same name, and it is the documented form. With `esModuleInterop`, default imports (`import path from "node:path"`) work. Without it, use `import * as path from "node:path"`.

## Globals

`@types/node` declares the Node globals: `process`, `Buffer`, `__dirname` and `__filename` (CommonJS only), `setTimeout` and friends, `console`, `URL`, `AbortController`, `TextEncoder`, and in recent versions `fetch`, `Request`, `Response`, and `structuredClone`.

```ts
const port = Number(process.env.PORT ?? 3000);
const buf = Buffer.from("hello", "utf8");
const res = await fetch("https://example.com");      // global fetch in modern Node
```

In **ES modules**, `__dirname` and `__filename` do not exist. Use `import.meta`:

```ts
import { fileURLToPath } from "node:url";
import path from "node:path";

const here = path.dirname(fileURLToPath(import.meta.url));
// recent Node versions also provide import.meta.dirname and import.meta.filename
```

### `setTimeout` return type

In Node, `setTimeout` returns a `NodeJS.Timeout` object, not a number. Do not annotate a timer variable as `number`:

```ts
let timer: ReturnType<typeof setTimeout> | undefined;     // correct in Node and the browser
timer = setTimeout(() => {}, 1000);
clearTimeout(timer);
```

## `process.env`

```ts
const url = process.env.DATABASE_URL;     // string | undefined
```

Every environment variable is `string | undefined`. Do not cast it to `string`. Validate it once at startup and use a typed config object ([config and environment](./01-config-and-environment.md)).

## Files and paths

The `fs/promises` API is the one to use in servers (never the `Sync` functions on a request path):

```ts
import { readFile, writeFile, mkdir } from "node:fs/promises";

const text = await readFile("config.json", "utf8");     // string
const raw = await readFile("image.png");                // Buffer
```

The return type depends on the **encoding argument**: with an encoding you get a `string`, without one a `Buffer`. TypeScript models this with overloads, so a wrong guess shows up as a type error.

File system errors carry a `code` property, typed as `NodeJS.ErrnoException`. Narrow with a guard before reading it ([catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md)):

```ts
function isErrnoException(e: unknown): e is NodeJS.ErrnoException {
  return e instanceof Error && "code" in e;
}

try {
  await readFile(path);
} catch (e) {
  if (isErrnoException(e) && e.code === "ENOENT") return null;
  throw e;
}
```

## Buffers and binary data

`Buffer` is a subclass of `Uint8Array`, so it works anywhere a `Uint8Array` is accepted.

```ts
const b = Buffer.from("héllo", "utf8");
b.toString("base64");
b.toString("hex");
Buffer.byteLength("héllo");        // bytes, not characters
```

Use `Buffer.byteLength` for sizes, not `.length` on a string. For hashing and randomness, use `node:crypto`:

```ts
import { createHash, randomUUID, timingSafeEqual } from "node:crypto";

const id = randomUUID();                                  // string
const digest = createHash("sha256").update(data).digest("hex");
```

## Streams

Streams process data in chunks, so you do not load large files into memory. Use `pipeline` from `node:stream/promises`, which wires streams together, propagates errors, and cleans up:

```ts
import { createReadStream, createWriteStream } from "node:fs";
import { createGzip } from "node:zlib";
import { pipeline } from "node:stream/promises";

await pipeline(
  createReadStream("big.log"),
  createGzip(),
  createWriteStream("big.log.gz"),
);
```

Readable streams are **async iterable**, so you can consume them with `for await`:

```ts
for await (const chunk of createReadStream("data.txt", { encoding: "utf8" })) {
  process(chunk);                         // chunk is a string (or Buffer without an encoding)
}
```

See [iterators and generators](../12-async-and-iteration/04-iterators-and-generators.md). Chunk types are loosely typed in places (`any`), so validate or annotate what you read.

## HTTP types

The `http` module's types (`IncomingMessage`, `ServerResponse`, `Server`) are what frameworks build on. You usually meet them through a framework's own types ([Express](./02-express.md)), but they appear in places such as `req.socket`, raw headers, and `server.close`.

```ts
import { createServer, type IncomingMessage, type ServerResponse } from "node:http";

const server = createServer((req: IncomingMessage, res: ServerResponse) => {
  res.writeHead(200, { "Content-Type": "text/plain" });
  res.end("ok");
});
```

Note `req.headers[name]` is `string | string[] | undefined`, because some headers can repeat.

## Events

Node's `EventEmitter` is the base for streams, servers, and many libraries. Recent `@types/node` versions let it be generic over an event map (see [typed event emitter](../17-design-patterns/07-typed-event-emitter.md)). Use `events.once` to turn an event into a promise, and `events.on` for an async iterator.

## Request-scoped context

`AsyncLocalStorage` carries values (request id, current user) through async calls without passing them as parameters, which is useful for logging:

```ts
import { AsyncLocalStorage } from "node:async_hooks";

interface RequestContext { requestId: string; userId?: string }
export const requestContext = new AsyncLocalStorage<RequestContext>();

// in a middleware
requestContext.run({ requestId: randomUUID() }, () => next());

// anywhere downstream
const ctx = requestContext.getStore();     // RequestContext | undefined
```

The store is typed through the generic, and `getStore()` returns `undefined` outside a `run`, so handle that. Use it for cross-cutting data (logging, tracing), not as a hidden channel for business inputs ([middleware](./03-middleware.md)).

## Other useful typed APIs

| API | Use |
|---|---|
| `util.promisify` | convert callback APIs to promises, keeping types for standard signatures |
| `util.parseArgs` | typed command-line argument parsing |
| `child_process` | spawn processes (`spawn`, `execFile`: prefer these over `exec` with string commands) |
| `worker_threads` | CPU-heavy work off the main thread ([event loop](../12-async-and-iteration/00-event-loop.md)) |
| `node:test` | the built-in test runner ([test runners](../18-testing-and-debugging/03-test-runners.md)) |
| `readline` | line-by-line input, async iterable |

## Running TypeScript directly

Recent Node versions can run `.ts` files by **stripping types**, with restrictions: only syntax that can simply be erased works (no `enum`, value `namespace`, or parameter properties), and nothing is type-checked. Other options are `tsx`, a build step with `tsc`, or a bundler. In every case run `tsc --noEmit` for type checking ([type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)).

## Important rules and misconceptions

- **`@types/node` describes Node, it does not provide it.** The runtime version is what you actually deploy. Keep them aligned.
- **TypeScript does not know which Node you run on.** `lib` and `@types/node` are claims. An API that type-checks may not exist on an older runtime.
- **Env vars are strings.** There are no numbers or booleans until you convert them.
- **Sync filesystem and crypto calls block the event loop.** Avoid them on request paths.
- **Types do not validate** what you read from files, streams, or the network.

## Common mistakes

- Missing or mismatched `@types/node`, so `process` or `Buffer` is "not found" or has the wrong API.
- Including `DOM` in `lib` for a server project.
- Typing timers as `number`.
- Using `__dirname` in an ES module.
- Casting `process.env.X as string`.
- Using `readFileSync` in request handlers.
- Forgetting to narrow `NodeJS.ErrnoException` before reading `code`.
- Ignoring `string | string[] | undefined` for request headers.
- Reading large files fully into memory instead of streaming.
- Using `exec` with unsanitized strings (command injection).

## Debugging

- If `process`, `Buffer`, or `__dirname` are "not found", check that `@types/node` is installed and not excluded by `types` or `include`.
- If you get two conflicting `setTimeout` or `fetch` declarations, remove `"DOM"` from `lib`, or investigate which package pulls in DOM types.
- If an API is missing from the types, compare `npm ls @types/node` with your Node version.
- `tsc --traceResolution` and `--listFiles` show which declaration files are loaded ([compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)).
- If code type-checks but fails with "x is not a function", confirm the runtime Node version supports it.

## Quick summary

- Install `@types/node` matching your Node major version, leave `DOM` out of `lib`, and use `node:` imports.
- `process.env` values are `string | undefined`. Use `ReturnType<typeof setTimeout>` for timers, and `import.meta` instead of `__dirname` in ESM.
- Use `fs/promises`, stream `pipeline`, and `crypto`. Narrow `ErrnoException` and header arrays before use.
- `AsyncLocalStorage` carries request-scoped context. `events.once`/`on` adapt events to promises and async iteration.
- Types describe the API, not the runtime version or the data. Validate inputs and run `tsc --noEmit` separately.

**Next:** [Config and environment](./01-config-and-environment.md)
