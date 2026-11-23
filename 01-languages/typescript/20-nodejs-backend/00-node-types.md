# Node.js Types, Runtimes, and Native Type Stripping in TypeScript

> Comprehensive typing for Node.js in TypeScript: @types/node, Node 22+ native type stripping (`--experimental-strip-types`), Buffer vs Uint8Array, Stream typing, and EventLoop mechanics.

| Metadata | Details |
| :--- | :--- |
| **Category** | Node.js Backend Typing |
| **Difficulty** | Intermediate |
| **Prerequisites** | [08-modules/00-imports-and-exports.md](../08-modules/00-imports-and-exports.md), [13-compiler-and-tsconfig/02-target-module-and-lib.md](../13-compiler-and-tsconfig/02-target-module-and-lib.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Historically, running TypeScript on Node.js required either an ahead-of-time build step (`tsc` emitting to `dist/`) or third-party runtime transpilers (`ts-node`, `tsx`, `esbuild`).

Starting in **Node.js 22 LTS**, Node natively supports executing `.ts` files via **Native Type Stripping** (`node --experimental-strip-types index.ts`). By stripping type annotations in C++ (via an integrated parser), Node runs TypeScript directly without third-party loaders.

However, writing production backend services requires deep mastery over Node.js's type definitions (`@types/node`):
- **Buffer vs Uint8Array**: Node's legacy `Buffer` extends JavaScript's standard `Uint8Array`, but subtle API discrepancies exist across environments (Cloudflare Workers, Deno, Bun).
- **Stream Typing**: Typing `Readable`, `Writable`, and modern web streams (`ReadableStream`).
- **Global Pollution**: Ambient types in `@types/node` (e.g. `process`, `global`, `setTimeout` returning `Timeout` instead of `number`).

```
+-----------------------------------------------------------+
|               Node.js 22 Native Type Stripping            |
|                                                           |
|   server.ts  ------(Node.js native C++ parser)-----> V8   |
|   (Strips type syntax in memory, zero config, zero npm)   |
|                                                           |
|   Supported: type, interface, type annotations, generics  |
|   Forbidden: enums, namespaces, parameter properties      |
+-----------------------------------------------------------+
```

---

## 2. Core Concepts

### 1. Native Type Stripping Constraints (Node 22+)
Node's native type stripper transforms code strictly by replacing type annotations with whitespace (preserving line numbers).
Consequently, **Non-Erasable Syntax** is prohibited under `--experimental-strip-types`:
- ❌ `enum Foo { A, B }` -> Use `const Foo = { A: 'A', B: 'B' } as const` instead!
- ❌ `namespace Bar {}` -> Use standard ES modules!
- ❌ Constructor parameter properties (`constructor(public name: string)`) -> Use explicit class property assignments!

### 2. `node:` Protocol Imports
Always use explicit `node:` namespace imports:
```typescript
import fs from 'node:fs/promises';
import path from 'node:path';
import { Buffer } from 'node:buffer';
```
This guarantees resolution to Node's built-in core modules and prevents collision with malicious npm packages.

### 3. `setTimeout` Return Type Trap
In browser DOM typing, `setTimeout()` returns a `number`. In Node.js `@types/node`, `setTimeout()` returns a `NodeJS.Timeout` object!
```typescript
// Node.js:
const timer: NodeJS.Timeout = setTimeout(() => {}, 1000);
// Universal:
const timerId: ReturnType<typeof setTimeout> = setTimeout(() => {}, 1000);
```

---

## 3. How It Works Internally

The `@types/node` package installs ambient declarations inside `node_modules/@types/node`.
When `"moduleResolution": "NodeNext"` is configured in `tsconfig.json`:
TypeScript automatically associates `import ... from 'node:fs'` with the declared interfaces for Node.js file system APIs, providing full autocomplete and strict promise typing.

---

## 4. Syntax & API Reference

### Typed Node.js Streams Pipeline
```typescript
import { pipeline } from 'node:stream/promises';
import { createReadStream, createWriteStream } from 'node:fs';
import { createGzip } from 'node:zlib';

await pipeline(
  createReadStream('input.txt'),
  createGzip(),
  createWriteStream('input.txt.gz')
);
```

---

## 5. Practical Examples

### Example 1: Basic Node.js File System I/O with Promises
```typescript
import fs from 'node:fs/promises';
import path from 'node:path';

export async function readJsonConfig<T>(relativePath: string): Promise<T> {
  const fullPath = path.resolve(process.cwd(), relativePath);
  const rawData = await fs.readFile(fullPath, 'utf-8');
  return JSON.parse(rawData) as T;
}
```

### Example 2: Real-World Safe Stream Chunk Transformer
```typescript
import { Transform, type TransformCallback } from 'node:stream';

export class UpperCaseTransform extends Transform {
  override _transform(chunk: Buffer, encoding: BufferEncoding, callback: TransformCallback): void {
    try {
      const upper = chunk.toString('utf-8').toUpperCase();
      this.push(Buffer.from(upper, 'utf-8'));
      callback();
    } catch (err) {
      callback(err as Error);
    }
  }
}
```

### Example 3: Edge Case - Cross-Platform Uint8Array vs Buffer Handling
```typescript
export function toUint8Array(input: Buffer | Uint8Array): Uint8Array {
  // Buffer is a subclass of Uint8Array, but may share underlying ArrayBuffer pool
  return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
}
```

### Example 4: Failure Case - Using Forbidden Enum with Node Type Stripping
```typescript
// When executed with `node --experimental-strip-types`:
// @ts-expect-error TS1454: Enum declarations are not erasable syntax!
enum LogLevel {
  DEBUG,
  INFO,
  ERROR
}
// Throws: SyntaxError: The enum syntax is not supported when type stripping is enabled.
```

### Example 5: Idiomatic Erasable TypeScript Class Pattern
```typescript
export class DatabaseConnection {
  public readonly uri: string;
  public isConnected: boolean;

  // Explicit property assignment instead of parameter properties
  constructor(uri: string) {
    this.uri = uri;
    this.isConnected = false;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`node:test` vs `@types/jest` Conflict**:
   Installing both `@types/node` and `@types/jest` can cause global namespace clashes (`test`, `expect`, `describe`). Use local imports rather than global type references.
2. **`Buffer.from(number)` Security Flaw**:
   In older Node versions, `new Buffer(100)` allocated uninitialized memory, leaking sensitive server memory. Always use `Buffer.alloc(size)` or `Buffer.from(array)`.
3. **`process.env` Returns `string | undefined`**:
   Every key in `process.env` is typed as `string | undefined`. Reading `process.env.PORT` requires coercing to number and providing fallbacks.
4. **Unhandled Rejection Tracking**:
   Always listen to `process.on('unhandledRejection', (reason, promise) => ...)` to prevent unhandled asynchronous errors from corrupting server state.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **Using `new Buffer()`** | `new Buffer(10)` | `Buffer.alloc(10)` | Security vulnerability (memory leakage) |
| **Banned Syntax in Type Stripping** | `enum Status { OK }` | `const Status = { OK: 'OK' } as const` | Crashes Node 22 native type stripper |
| **Non-Prefix Core Imports** | `import fs from 'fs'` | `import fs from 'node:fs'` | Ambiguous import; vulnerable to dependency confusion |
| **Assuming Timer is Number** | `let t: number = setTimeout(...)` | `let t: NodeJS.Timeout` | TS2322 in Node.js environment |
| **Sync I/O in Hot Paths** | `fs.readFileSync(...)` | `await fs.readFile(...)` | Blocks Node.js event loop for all users |
| **Ignoring Stream Errors** | `stream.pipe(dest)` without error handler | `pipeline(stream, dest)` | Unhandled error crash on connection drop |

---

## 8. Performance & Type-Checker Complexity

- **Type Stripping Performance**: Node 22 native type stripping executes cold startup in ~15ms, compared to ~350ms for `ts-node`.
- **Event Loop Overhead**: Keep stream chunk transformations monomorphic and avoid allocating intermediate strings when dealing with raw binary buffers.

---

## 9. Compiler Internals

When `"moduleResolution": "NodeNext"` is specified:
The TypeScript compiler enforces that relative imports must include file extensions:
`import { helper } from './helper.js';`
even though the source file on disk is named `helper.ts`! This matches Node's native ESM resolution algorithm.

---

## 10. Best Practices & Design Guidelines

1. **Use `node:` Protocol for Core Imports**: `import fs from 'node:fs/promises'`.
2. **Avoid Non-Erasable TypeScript Features**: Stick to erasable syntax for Node 22 native compatibility.
3. **Use `node:stream/promises.pipeline`**: Safely pipe streams with automatic error and cleanup handling.
4. **Type `process.env` with Declaration Merging**: Ensure environment variables are validated.

---

## 11. Real-World Case Study

### High-Throughput HTTP File Stream Proxy
Streaming multi-gigabyte logs directly to S3 with gzip compression:

```typescript
import { pipeline } from 'node:stream/promises';
import { createReadStream, createWriteStream } from 'node:fs';
import { createGzip } from 'node:zlib';
import path from 'node:path';

export async function compressAndArchive(sourceFile: string, destDir: string): Promise<string> {
  const destFile = path.join(destDir, `${path.basename(sourceFile)}.gz`);
  
  await pipeline(
    createReadStream(sourceFile),
    createGzip(),
    createWriteStream(destFile)
  );

  return destFile;
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is Node.js Native Type Stripping introduced in Node 22?</summary>

Node 22 introduced `--experimental-strip-types`, allowing Node to run `.ts` files directly without external compilers or loaders. It replaces type syntax with whitespace in memory, preserving original line numbers and stack traces without emitting `.js` files to disk.
</details>

<details>
<summary>2. Why are TypeScript Enums forbidden under Node's native type stripping?</summary>

Enums generate runtime JavaScript code (a bidirectional lookup object). Type stripping only erases type annotations; it does not transform or emit new runtime JavaScript code.
</details>

<details>
<summary>3. Why should you always import built-in modules using the `node:` protocol?</summary>

The `node:` prefix (e.g. `node:fs`) guarantees that Node resolves the module directly from its internal core C++ libraries, bypassing `node_modules` and preventing dependency confusion attacks where an attacker publishes a malicious npm package named `fs`.
</details>

<details>
<summary>4. What is the return type of `setTimeout()` in Node.js versus the browser?</summary>

In Node.js, `setTimeout()` returns a `NodeJS.Timeout` object (which has methods like `.unref()` and `.ref()`). In browser DOM environments, `setTimeout()` returns an integer ID (`number`).
</details>

<details>
<summary>5. Why does `"moduleResolution": "NodeNext"` require importing `./utils.js` when the file is `./utils.ts`?</summary>

TypeScript does not alter import specifiers during compilation. Because Node.js native ESM requires exact file extensions in `import` statements at runtime, you write the extension that will exist at runtime (`.js`), which TypeScript resolves to the source `.ts` file during compilation.
</details>

<details>
<summary>6. How does `stream/promises.pipeline` prevent memory leaks compared to `.pipe()`?</summary>

Traditional `.pipe()` does not forward errors or destroy streams if one stream in the chain encounters an error, causing memory leaks and hanging file descriptors. `stream/promises.pipeline` automatically listens for errors, destroys all participating streams, and resolves or rejects a Promise.
</details>

<details>
<summary>7. What is the relationship between Node's `Buffer` and standard JavaScript `Uint8Array`?</summary>

`Buffer` is a subclass of `Uint8Array`. However, Node's `Buffer.allocUnsafe` shares an internal 8KB memory pool, meaning `buffer.buffer` (the ArrayBuffer) may be larger than the buffer itself.
</details>

<details>
<summary>8. How do you type `process.on('SIGTERM')` signal handlers?</summary>

Signal names are typed as `NodeJS.Signals` (`'SIGTERM' | 'SIGINT' | 'SIGHUP'`).
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Typed Buffer String Conversion</summary>

**Problem:** Write a function `toBase64(content: string): string` using Node's `Buffer`.

**Solution:**
```typescript
import { Buffer } from 'node:buffer';

export function toBase64(content: string): string {
  return Buffer.from(content, 'utf-8').toString('base64');
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Safe Timer Handle</summary>

**Problem:** Type an object holding a nullable active Node.js interval timer and provide a cancel method.

**Solution:**
```typescript
export class TimerManager {
  private timer: NodeJS.Timeout | null = null;

  public set(fn: () => void, ms: number) {
    this.clear();
    this.timer = setInterval(fn, ms);
  }

  public clear() {
    if (this.timer) {
      clearInterval(this.timer);
      this.timer = null;
    }
  }
}
```
</details>

<details>
<summary>Exercise 3 (Medium): Typed Line-by-Line File Reader</summary>

**Problem:** Use `node:readline/promises` to read a file line-by-line asynchronously with strong typing.

**Solution:**
```typescript
import fs from 'node:fs';
import readline from 'node:readline/promises';

export async function countLines(filePath: string): Promise<number> {
  const fileStream = fs.createReadStream(filePath);
  const rl = readline.createInterface({ input: fileStream, crlfDelay: Infinity });
  let count = 0;
  for await (const _ of rl) {
    count++;
  }
  return count;
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Process Signal Teardown Handler</summary>

**Problem:** Register a graceful shutdown hook on `'SIGTERM'` and `'SIGINT'` that runs cleanup and exits with code 0.

**Solution:**
```typescript
export function registerShutdown(cleanup: () => Promise<void>) {
  const handler = async (signal: NodeJS.Signals) => {
    console.log(`Received ${signal}, shutting down...`);
    await cleanup();
    process.exit(0);
  };

  process.on('SIGTERM', handler);
  process.on('SIGINT', handler);
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Typed Readable Stream Chunk Collector</summary>

**Problem:** Write a function `streamToBuffer(stream: NodeJS.ReadableStream): Promise<Buffer>` collecting all chunks into a single Buffer.

**Solution:**
```typescript
import { Buffer } from 'node:buffer';

export async function streamToBuffer(stream: NodeJS.ReadableStream): Promise<Buffer> {
  const chunks: Buffer[] = [];
  for await (const chunk of stream) {
    chunks.push(typeof chunk === 'string' ? Buffer.from(chunk) : chunk);
  }
  return Buffer.concat(chunks);
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Zero-Dependency Erasable Const Enum Replacement</summary>

**Problem:** Create a type-safe enum replacement using `as const` that provides reverse value lookups and strict union types compatible with Node 22 type stripping.

**Solution:**
```typescript
export const HttpStatus = {
  OK: 200,
  CREATED: 201,
  BAD_REQUEST: 400,
  NOT_FOUND: 404,
  INTERNAL_SERVER_ERROR: 500
} as const;

export type HttpStatusCode = typeof HttpStatus[keyof typeof HttpStatus];

export function isSuccessStatus(code: HttpStatusCode): boolean {
  return code >= 200 && code < 300;
}
```
</details>
