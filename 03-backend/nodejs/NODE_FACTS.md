# Node.js Technical Facts & Architectural Standards

> Verified technical ground truths, runtime behaviors, version baselines, and security advisories for the Node.js curriculum.

---

## 1. Version Baselines & Release Cadence

- **Current Active LTS**: **Node.js 22 LTS ("Jod")** (Release: April 2024, LTS Promotion: October 2024, Maintenance: October 2025 – April 2027).
- **Maintenance LTS**: **Node.js 20 LTS ("Iron")** (EOL: April 30, 2026).
- **Current Generation / Next LTS**: **Node.js 24 ("Krypton")** (Released: May 2025, Active LTS: October 2025 – April 2028).
- **Current Short-lived / Development**: **Node.js 23** and **Node.js 25** (6-month odd-numbered releases).
- **2026+ Release Cadence Shift**: Starting with Node.js 27, Node.js moves to an annual release cadence dropping every April with promotion to LTS every October.
- **Minimum Supported Line**: Node.js 20.x (Node 18 reached EOL April 2025).

---

## 2. Modern Node.js Native Runtime Features

### Native TypeScript Support (Type Stripping)
- **Node.js 22.6.0+**: Introduced `--experimental-strip-types` allowing execution of `.ts` files directly by stripping type annotations and replacing them with whitespace to preserve sourcemap line numbers.
- **Node.js 22.18.0+ / 23+**: Type stripping is **enabled by default** (`node app.ts`).
- **Supported Syntax**: Interfaces, type aliases, type annotations, and `import type`.
- **Unsupported (Requires `--experimental-transform-types`)**: `enum`, namespaces, class parameter properties, and legacy experimental decorators (erasable syntax only).
- **Verification Rule**: Node does not typecheck; production pipelines must run `tsc --noEmit`.

### Native SQLite Module (`node:sqlite`)
- Introduced in Node.js 22.5.0 (`--experimental-sqlite`), flag removed in 22.13.0 / 23.4.0.
- Promoted to **Release Candidate (Stability 1.2)** in Node 25.
- Class renamed from `DatabaseSync` to **`Database`** (`import { Database } from 'node:sqlite'`).
- Synchronous V8-to-C binding (blocks event loop during query execution, similar to `better-sqlite3`).
- Diagnostics integration via `diagnostics_channel` (`sqlite.db.query`).

### Built-in Test Runner (`node:test`)
- Stable since Node.js 20.x. Supports `describe`, `it`, `test`, `before`, `after`, `beforeEach`, `afterEach`, `mock`, and `assert` (`node:assert/strict`).
- CLI runner: `node --test` with native watch mode (`node --test --watch`) and coverage (`node --test --experimental-test-coverage`).
- Native snapshot testing, mock timers, and module mocking (`mock.module`).

### Permission Model (`--permission`)
- Stable in Node.js 22.13.0+ and 23.5.0+.
- CLI flags: `--permission`, `--allow-fs-read=<path>`, `--allow-fs-write=<path>`, `--allow-child-process`, `--allow-worker`, `--allow-addons`.
- Runtime query API: `process.permission.has('fs.read', './config.json')`.
- Intended for blast-radius mitigation (contain supply chain attacks), not an untrusted multi-tenant sandbox.

### Single Executable Applications (SEA)
- Native `--build-sea sea-config.json` command simplifies binary generation without requiring manual `postject` injection.
- Supports embedding assets via `node:sea` (`readAsset()`).

### `require(esm)`
- Node.js 22 introduced `--experimental-require-module`.
- **Unflagged and enabled by default** in Node 22.12.0+ and Node 23+.
- CommonJS can synchronously `require()` ES modules if the ESM graph contains no top-level `await`. Returns the module namespace object.
- Throws `ERR_REQUIRE_ASYNC_MODULE` if the target module contains top-level `await`.

### Native Environment File Support (`--env-file`)
- Built-in since Node.js 20.6.0: `node --env-file=.env app.js`.
- Multiple files: `node --env-file=.env --env-file=.env.local app.js`.
- Programmatic API: `process.loadEnvFile('.env')`.

### Watch Mode (`--watch`)
- Native process restart upon file change: `node --watch app.js` (eliminates `nodemon`).

---

## 3. Web Platform Standards in Node.js

- **Global Fetch API**: Native `fetch`, `Request`, `Response`, `Headers`, `FormData` powered by Undici.
- **Web Streams**: `ReadableStream`, `WritableStream`, `TransformStream` natively integrated with Node streams via `stream/web` and `Stream.toWeb()` / `Stream.fromWeb()`.
- **WebSocket**: Global `WebSocket` client natively available in Node.js 22+.
- **Crypto & Web Crypto**: `node:crypto` and global `crypto.subtle` (SubtleCrypto) matching browser standards.

---

## 4. Ecosystem & Framework Release Facts

### Express 5.0 (Released October 15, 2024)
- **Minimum Node**: Node.js 18+.
- **Async Error Handling**: Rejected promises and thrown errors in `async` route handlers and middleware are **automatically caught and forwarded** to `next(err)`.
- **`path-to-regexp` v8**: Raw RegExp routes (`app.get(/^\/abc/)`) are removed to prevent ReDoS. Wildcards must be named (e.g. `*name`), optional parameters enclosed in `{}` (e.g. `{:id}`).
- **Removed Methods**: `app.del()` (use `app.delete()`), `req.param(name)` (use `req.params[name]`, `req.body[name]`, `req.query[name]`).
- **Signature Changes**: `res.send(body, status)` and `res.json(obj, status)` removed in favor of `res.status(status).json(obj)`.
- **Security Defaults**: `express.urlencoded` `extended` defaults to `false`. `express.static` ignores dotfiles by default. `req.query` is a read-only getter.

### Fastify v5 (Released 2024)
- **Minimum Node**: Node.js 20+.
- **Strict JSON Schemas**: Full JSON Schema strictly required; shorthand syntax removed.
- **Listen Signature**: Variadic `fastify.listen(port, host, cb)` removed; requires options object `fastify.listen({ port, host })`.
- **`request.params`**: Prototype removed (`Object.create(null)`) to prevent prototype pollution.

### Hono & `@hono/node-server`
- Lightweight web-standards framework running on Node.js via official `@hono/node-server` adapter (`serve({ fetch: app.fetch, port: 3000 })`).

### Modern Database ORMs & Drivers
- **pg (`node-postgres`)**: v8.13+, native connection pooling via `new Pool({ max: 20 })`.
- **Drizzle ORM (v0.38+)**: Zero-runtime-dependency TypeScript SQL builder with Drizzle Kit migrations.
- **Prisma (v6+)**: Native typed raw SQL queries (`$queryRawTyped`), slimmed query engine footprint, connection pooling.
- **Kysely**: Type-safe pure TypeScript SQL query builder without runtime code generation.
- **BullMQ (v5+)**: Distributed background queue over Redis, requires mandatory `connection` object, `maxRetriesPerRequest: null` for blocking Redis commands.

---

## 5. Security & Anti-Pattern Standards

1. **No Outdated Libraries**:
   - `request` is deprecated $\rightarrow$ use native `fetch()` or `undici`.
   - `moment.js` is deprecated $\rightarrow$ use `date-fns` or native `Intl`.
   - `Buffer()` constructor without size/alloc is insecure $\rightarrow$ use `Buffer.from()` or `Buffer.alloc()`.
   - `body-parser` separate package $\rightarrow$ use built-in `express.json()` and `express.urlencoded()`.
2. **Event Loop & Stream Safety**:
   - Never use synchronous filesystem methods (`fs.readFileSync`) in network request paths.
   - Always handle backpressure; use `stream.pipeline()` rather than raw `.pipe()` to ensure error propagation and resource cleanup.
3. **Database Security**:
   - Never concatenate variables into SQL strings $\rightarrow$ always use parameterized queries (`$1`, `$2` in `pg`, `?` in `sqlite`).
4. **Secrets & Injection**:
   - Zero hardcoded API keys or database passwords $\rightarrow$ use `--env-file` or environment variables with Zod validation.
   - Prevent Prototype Pollution: sanitize JSON keys or use `Object.create(null)`.
