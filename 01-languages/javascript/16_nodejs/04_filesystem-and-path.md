# Filesystem & Path Engineering

> Node's `node:fs` and `node:path` modules provide cross-platform file manipulation, low-level file descriptors, recursive directory trees, atomic writes, and filesystem watchers.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [The Node.js Runtime](./01_node-runtime.md), [The Node.js Event Loop](./02_node-event-loop.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | Node.js Core API (`node:fs/promises`, `node:path`) / Baseline Widely Available |

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

Server-side applications rely fundamentally on persistent disk storage: saving user uploads, processing log archives, reading configuration files, creating temporary files, and streaming multi-gigabyte datasets.

Unlike web browsers (where direct filesystem access is blocked for security), Node.js provides first-class operating system filesystem access through **`node:fs`** and cross-platform path resolution via **`node:path`**.

Filesystem operations can easily become performance bottlenecks: reading an entire 1GB file into memory with `fs.readFile()` exhausts the V8 heap and crashes the process. Similarly, differences between Windows backslashes (`\`) and POSIX forward slashes (`/`) cause cross-platform bugs unless handled through normalized path abstractions.

---

## 2. Core Concepts

### Three Flavors of `node:fs`
The `node:fs` module provides three distinct API styles:
1. **Promise-based (`node:fs/promises`)**: Modern, asynchronous, returns native Promises. **The recommended standard for modern code.**
2. **Synchronous (`fs.readFileSync`)**: Blocks the entire JavaScript call stack until disk I/O completes. *Acceptable only during initial server startup scripts.*
3. **Callback-based (`fs.readFile(path, callback)`)**: Legacy error-first callbacks.

### `path.join()` vs `path.resolve()`
- **`path.join(...paths)`**: Concatenates path segments using the platform-specific separator (`/` or `\`) and normalizes the resulting path.
- **`path.resolve(...paths)`**: Resolves a sequence of paths into an **absolute path**, processing from right to left until an absolute path is formed, prepending `process.cwd()` if no absolute path is reached.

```text
path.join('a', 'b', '..', 'c') ──> "a/c" (Normalized relative path)
path.resolve('src', 'app.js')   ──> "/home/user/project/src/app.js" (Absolute path!)
```

### File Descriptors & `FileHandle`
A **File Descriptor (FD)** is an integer assigned by the operating system kernel representing an open file table entry. In `node:fs/promises`, this is wrapped by a **`FileHandle`** object. Managing file handles directly allows positional reads, file locking, and granular truncation.

---

## 3. How It Works Internally

### Cross-Platform Filesystem Execution via libuv

POSIX systems do not have an asynchronous file I/O system call comparable to network `epoll`. Node.js routes filesystem operations through the **libuv worker thread pool**:

```
 JavaScript: await fs.readFile('data.txt')
                    │
                    ▼
 [ libuv Thread Pool Task Queue ]
                    │
                    ▼
 Worker Thread (C++):
   - Invokes blocking OS call: open() -> fstat() -> read() -> close()
                    │
                    ▼
 Worker Thread finishes:
   - Emits completion signal to libuv Event Loop Poll Queue
                    │
                    ▼
 Event Loop executes callback on JS Thread: Promise settles with Buffer
```

---

## 4. Syntax & API Reference

### Core `node:fs/promises` APIs

```javascript
import fs from 'node:fs/promises';
import path from 'node:path';

// 1. Reading & Writing Files
await fs.writeFile('output.txt', 'Hello World', 'utf8');
const text = await fs.readFile('output.txt', 'utf8');
const buffer = await fs.readFile('image.png'); // Returns Buffer if encoding omitted

// 2. Directory Operations
await fs.mkdir('nested/dir/path', { recursive: true });
const entries = await fs.readdir('./src', { withFileTypes: true });
// entries[0].isDirectory(), entries[0].isFile()

// 3. File Metadata & Status
const stats = await fs.stat('output.txt');
console.log('Size:', stats.size);
console.log('Modified:', stats.mtime);

// 4. File Verification & Deletion
await fs.access('output.txt', fs.constants.R_OK | fs.constants.W_OK);
await fs.unlink('output.txt'); // Delete file
await fs.rm('nested', { recursive: true, force: true }); // Delete folder
```

### Core `node:path` APIs

```javascript
import path from 'node:path';

path.join('/users', 'ada', 'docs');     // '/users/ada/docs'
path.resolve('dist', 'bundle.js');      // '/current/working/dir/dist/bundle.js'
path.dirname('/app/src/index.js');      // '/app/src'
path.basename('/app/src/index.js');     // 'index.js'
path.basename('/app/src/index.js', '.js'); // 'index' (Strips extension)
path.extname('styles.css');             // '.css'
path.normalize('/a/b/../../c');         // '/c'

// Explicit cross-platform engines
path.posix.join('a', 'b'); // Always 'a/b'
path.win32.join('a', 'b'); // Always 'a\b'
```

---

## 5. Practical Examples

### Example 1: Recursive Directory Tree Scanner
```javascript
import fs from 'node:fs/promises';
import path from 'node:path';

async function scanDirectoryTree(dirPath) {
  const filePaths = [];

  async function walk(currentDir) {
    const entries = await fs.readdir(currentDir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = path.join(currentDir, entry.name);

      if (entry.isDirectory()) {
        await walk(fullPath); // Recurse into subfolder
      } else if (entry.isFile()) {
        filePaths.push(fullPath);
      }
    }
  }

  await walk(dirPath);
  return filePaths;
}

// Usage
const files = await scanDirectoryTree('./src');
console.log('Total files found:', files.length);
```

### Example 2: Atomic File Write Pattern (Preventing Corrupted Files)
```javascript
import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';

// If a server crashes or loses power mid-write, writing directly to target file
// leaves it truncated or corrupted.
// SOLUTION: Write to unique temp file on same disk, then ATOMICALLY rename!
async function writeFileAtomic(targetPath, content) {
  const dir = path.dirname(targetPath);
  const tempPath = path.join(dir, `.tmp-${crypto.randomUUID()}`);

  try {
    // 1. Write complete payload to temp file
    await fs.writeFile(tempPath, content, 'utf8');

    // 2. Atomic OS rename (POSIX rename is atomic)
    await fs.rename(tempPath, targetPath);
  } catch (err) {
    // Clean up temp file on failure
    await fs.unlink(tempPath).catch(() => {});
    throw err;
  }
}
```

### Example 3: Streaming Multi-Gigabyte File Copy with `pipeline`
```javascript
import fs from 'node:fs';
import { pipeline } from 'node:stream/promises';

// Memory usage remains flat at ~64KB regardless of whether file is 100MB or 100GB!
async function streamCopyLargeFile(sourcePath, destPath) {
  const readStream = fs.createReadStream(sourcePath);
  const writeStream = fs.createWriteStream(destPath);

  try {
    await pipeline(readStream, writeStream);
    console.log('Streaming copy complete with 0 heap bloat');
  } catch (err) {
    console.error('Copy pipeline failed:', err);
    throw err;
  }
}
```

### Example 4: Low-Level FileHandle for Positional Binary Inspection
```javascript
import fs from 'node:fs/promises';

async function readPngHeader(filePath) {
  let handle = null;

  try {
    handle = await fs.open(filePath, 'r');
    const headerBuffer = Buffer.alloc(8);

    // Read first 8 bytes directly at position 0
    await handle.read(headerBuffer, 0, 8, 0);

    // Standard PNG magic bytes: 0x89 50 4E 47 0D 0A 1A 0A
    const isPng = headerBuffer[0] === 0x89 && headerBuffer.toString('ascii', 1, 4) === 'PNG';
    return isPng;
  } finally {
    // ALWAYS close file handles to release kernel file descriptors!
    await handle?.close();
  }
}
```

### Example 5: High-Performance File Watcher using `fs.watch`
```javascript
import fs from 'node:fs';

function watchFileChanges(filePath, onChange) {
  // fs.watch uses OS notifications (inotify on Linux, FSEvents on macOS)
  const watcher = fs.watch(filePath, (eventType, filename) => {
    if (eventType === 'change') {
      console.log(`File ${filename || filePath} mutated on disk`);
      onChange();
    }
  });

  return {
    stop: () => watcher.close(),
  };
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`EMFILE` / `ENFILE` (Too Many Open Files)**: Operating systems limit how many file descriptors a process can open concurrently (often 1024 by default). Reading thousands of files in parallel via `Promise.all(files.map(fs.readFile))` triggers `Error: EMFILE: too many open files`. Limit concurrency via a pool or `p-limit`.
2. **`fs.watch` Inconsistencies & Duplicate Events**: Operating system file events (such as `inotify` or `FSEvents`) often emit multiple `change` events for a single save operation (e.g. one for content write, one for metadata/mtime update). Always debounce file watcher callbacks.
3. **`fs.rename` Cross-Device Limitation (`EXDEV`)**: `fs.rename()` fails with `EXDEV: cross-device link not permitted` if the source and destination paths reside on different physical hard drives, partitions, or Docker volume mounts. Use `fs.copyFile()` followed by `fs.unlink()` across mount boundaries.
4. **Symlink Directory Loops**: When recursively traversing directories, circular symlinks will cause infinite recursion and stack overflow unless visited inode paths are tracked in a `Set`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Unbounded `Promise.all` File Reads**| `Promise.all(10000.map(fs.readFile))` | Throttle concurrency (`pLimit(20)`) | Exceeds OS file descriptor limit (`EMFILE`). |
| **Using `fs.existsSync` in Critical Path**| `if (fs.existsSync(f)) fs.readFile(f)` | Open file directly and catch `ENOENT` | Race condition (TOCTOU: Time of check to time of use). |
| **Hardcoding Path Slashes** | `const p = dir + '/' + file` | `path.join(dir, file)` | Breaks on Windows systems that use backslashes (`\`). |
| **Loading Huge Files into RAM** | `const buf = await fs.readFile('5GB.iso')` | Use `fs.createReadStream()` | Exceeds V8 heap limit (~2GB-4GB), crashing the process. |
| **Forgetting to Close `FileHandle`** | `const h = await fs.open(f); return h.read();`| Always close in `finally` block | Leaks file descriptors until process restarts. |
| **Non-Atomic Configuration Writes** | Writing directly to active `config.json` | Write to temp file then `fs.rename` | Crash mid-write corrupts configuration permanently. |

---

## 8. Performance Characteristics

- **libuv Thread Pool Scaling**: Standard filesystem calls execute on the 4-thread pool. Under high I/O concurrency, increasing `UV_THREADPOOL_SIZE=32` boosts file throughput by preventing thread pool queue saturation.
- **Streaming Flat Memory Footprint**: `pipeline(readStream, writeStream)` streams chunks of 64KB (`highWaterMark`), preserving constant $O(1)$ memory consumption regardless of file size.

---

## 9. Engine Internals

1. **POSIX File Descriptors in C++**: In Node's C++ bindings (`src/node_file.cc`), `fs.open()` returns a raw integer file descriptor provided by the OS kernel `open(2)` system call.
2. **Buffer Slab Allocation**: When `fs.readFile()` reads data, Node's C++ layer allocates raw memory buffers outside the V8 heap using `node::Buffer`, copying bytes directly from kernel space into userspace memory.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `node:fs/promises` for Asynchronous Code**: Avoid mixing callback-based and promise-based APIs.
2. **Use `path.join` and `path.resolve` Exclusively**: Never manually concatenate file path strings with `/` or `\`.
3. **Always Close Low-Level File Handles**: Wrap `fs.open()` in `try...finally` to guarantee `handle.close()` execution.
4. **Use Atomic Writes for State and Config Files**: Write to a temporary file in the same directory and execute an atomic rename.

---

## 11. Real-World Case Study

### Resilient Rotating File Logger with Atomic Flush

An enterprise web service writes audit events to disk. Log files must rotate when reaching 10MB, handle high-frequency writes without blocking the main event loop, and recover from crashes cleanly.

```javascript
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

class RotatingFileLogger {
  #logDir;
  #baseName;
  #maxBytes;
  #currentStream = null;
  #currentSizeBytes = 0;

  constructor(logDir, baseName = 'audit.log', maxBytes = 10 * 1024 * 1024) {
    this.#logDir = logDir;
    this.#baseName = baseName;
    this.#maxBytes = maxBytes;
    this.#init();
  }

  #init() {
    if (!fs.existsSync(this.#logDir)) {
      fs.mkdirSync(this.#logDir, { recursive: true });
    }
    this.#openCurrentLog();
  }

  #openCurrentLog() {
    const activePath = path.join(this.#logDir, this.#baseName);

    if (fs.existsSync(activePath)) {
      this.#currentSizeBytes = fs.statSync(activePath).size;
    } else {
      this.#currentSizeBytes = 0;
    }

    this.#currentStream = fs.createWriteStream(activePath, { flags: 'a' });
  }

  log(entry) {
    const formatted = `[${new Date().toISOString()}] ${JSON.stringify(entry)}\n`;
    const byteLength = Buffer.byteLength(formatted, 'utf8');

    if (this.#currentSizeBytes + byteLength > this.#maxBytes) {
      this.#rotate();
    }

    this.#currentStream.write(formatted);
    this.#currentSizeBytes += byteLength;
  }

  #rotate() {
    this.#currentStream.end();

    const activePath = path.join(this.#logDir, this.#baseName);
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    const archivePath = path.join(this.#logDir, `${this.#baseName}.${timestamp}.gz`);

    // Rename current log to archive name
    fs.renameSync(activePath, archivePath);

    // Open fresh log
    this.#openCurrentLog();
    console.log(`Log file rotated to: ${archivePath}`);
  }

  close() {
    this.#currentStream?.end();
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is <code>path.resolve()</code> different from <code>path.join()</code>?</strong></summary>

`path.join()` simply concatenates path segments using the platform separator and normalizes relative dots (`..`). `path.resolve()` processes paths from right to left until an absolute path is formed; if no absolute path is reached, it prepends the current working directory (`process.cwd()`), guaranteeing that the return value is always an absolute path.
</details>

<details>
<summary><strong>2. Why should you avoid checking file existence via <code>fs.access()</code> or <code>fs.existsSync()</code> before reading a file?</strong></summary>

It introduces a Time-Of-Check to Time-Of-Use (TOCTOU) race condition. Between the moment you check if the file exists and the moment you attempt to read it, another process or thread could delete, rename, or modify permissions on the file. The standard idiom is to attempt opening or reading the file directly and handle the `ENOENT` (Error No Entry) error in `catch`.
</details>

<details>
<summary><strong>3. How does Node.js execute asynchronous filesystem operations when POSIX operating systems have no non-blocking file calls?</strong></summary>

Node.js offloads filesystem calls to the `libuv` worker thread pool. A worker thread executes the blocking OS system call (`open`, `read`, `write`) synchronously in C++. Upon completion, the worker thread notifies the libuv event loop, which schedules the JavaScript callback/promise on the main thread.
</details>

<details>
<summary><strong>4. What causes an <code>EMFILE: too many open files</code> error, and how is it resolved?</strong></summary>

It occurs when a Node.js process attempts to open more file descriptors than allowed by the operating system kernel limit (`ulimit -n`). It is resolved by throttling concurrency using concurrency limiters (like `p-limit`), closing file handles in `finally` blocks, or increasing the OS kernel limit.
</details>

<details>
<summary><strong>5. What is the advantage of using streams (<code>pipeline</code>) over <code>fs.readFile()</code>?</strong></summary>

`fs.readFile()` reads the entire file into memory at once, which will crash the process if the file exceeds available V8 heap memory (~1.4GB–4GB). Streams read and write data in small chunks (default 64KB), maintaining a constant $O(1)$ memory footprint regardless of file size.
</details>

<details>
<summary><strong>6. What is the difference between <code>fs.watch()</code> and <code>fs.watchFile()</code>?</strong></summary>

`fs.watch()` uses native operating system event notifications (`inotify` on Linux, `FSEvents` on macOS), which is fast and efficient. `fs.watchFile()` continuously polls the filesystem using `stat()` calls at fixed intervals, which consumes significant CPU and should only be used as a fallback on network filesystems.
</details>

<details>
<summary><strong>7. Why does <code>fs.rename()</code> fail with <code>EXDEV</code>?</strong></summary>

`fs.rename()` relies on the OS `rename(2)` system call, which can only rename files within the same physical filesystem mount or partition. If source and destination are on different disks or Docker volume mounts, it fails with `EXDEV`. You must copy the file and unlink the source.
</details>

<details>
<summary><strong>8. What is an atomic file write, and why is it important?</strong></summary>

An atomic write ensures that a file is either completely updated or untouched; it can never be left partially written or corrupted if the process crashes or power fails. It is achieved by writing data to a temporary file in the same directory and performing an atomic `fs.rename()`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement a Safe Cross-Device File Mover
Write an async function `moveFile(sourcePath, destPath)` that attempts an atomic `fs.rename()`. If it fails with an `EXDEV` error (cross-device move), it seamlessly falls back to streaming copy followed by source deletion.

<details>
<summary>View Solution</summary>

```javascript
import fs from 'node:fs/promises';
import { createReadStream, createWriteStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';

export async function moveFile(sourcePath, destPath) {
  try {
    // 1. Attempt fast atomic rename on same filesystem
    await fs.rename(sourcePath, destPath);
  } catch (err) {
    if (err.code === 'EXDEV') {
      // 2. Cross-device partition fallback: Stream copy + unlink
      const readStream = createReadStream(sourcePath);
      const writeStream = createWriteStream(destPath);
      await pipeline(readStream, writeStream);
      await fs.unlink(sourcePath);
    } else {
      throw err;
    }
  }
}
```
</details>

### Challenge 2: Implement a Recursive File Searcher by Extension
Write an async function `findFilesByExtension(rootDir, ext)` that recursively searches `rootDir` and returns an array of all absolute paths matching `ext` (e.g. `'.json'`), ignoring any inaccessible directories gracefully.

<details>
<summary>View Solution</summary>

```javascript
import fs from 'node:fs/promises';
import path from 'node:path';

export async function findFilesByExtension(rootDir, ext) {
  const matches = [];
  const normalizedExt = ext.startsWith('.') ? ext : `.${ext}`;

  async function walk(currentDir) {
    let entries;
    try {
      entries = await fs.readdir(currentDir, { withFileTypes: true });
    } catch {
      return; // Skip inaccessible / permission denied folders
    }

    for (const entry of entries) {
      const fullPath = path.resolve(currentDir, entry.name);

      if (entry.isDirectory()) {
        await walk(fullPath);
      } else if (entry.isFile() && path.extname(entry.name) === normalizedExt) {
        matches.push(fullPath);
      }
    }
  }

  await walk(rootDir);
  return matches;
}
```
</details>
