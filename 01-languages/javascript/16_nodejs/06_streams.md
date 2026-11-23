# Stream Engineering & Backpressure

> Node's `node:stream` module provides continuous, memory-efficient data processing pipelines, governing backpressure thresholds, binary chunking, and stream composition.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Event Emitter](./05_events.md), [Buffers & Binary](./07_buffers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard** | Node.js Core API (`node:stream`, `node:stream/promises`) / WHATWG Streams Interoperability |

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

When building server-side applications, processing large files (such as 10GB database backups or high-resolution video files) by loading them entirely into memory with `fs.readFile()` or `buffer.concat()` causes fatal errors: it exceeds the maximum V8 heap limit (typically 1.4GB–4GB) and crashes the process with `JavaScript heap out of memory`.

**Streams** solve this fundamental problem by breaking large data payloads into manageable, sequential chunks processed piece by piece over time. By using streams, a Node.js process can process a 100GB file using only **~64KB of RAM**. Furthermore, streams enable **Backpressure**: an automatic signaling mechanism that throttles fast data producers when downstream consumers are slow, preventing memory explosion.

---

## 2. Core Concepts

### The Four Stream Categories

| Stream Type | Description | Common Core Examples |
| :--- | :--- | :--- |
| **`Readable`** | An abstraction for a data source from which bytes can be consumed. | `fs.createReadStream()`, `http.IncomingMessage`, `process.stdin` |
| **`Writable`** | An abstraction for a destination to which bytes can be written. | `fs.createWriteStream()`, `http.ServerResponse`, `process.stdout` |
| **`Duplex`** | Implements both `Readable` and `Writable` interfaces independently. | `net.Socket`, TCP sockets, TLS streams |
| **`Transform`**| A Duplex stream that modifies or transforms data as it is written/read. | `zlib.createGzip()`, `crypto.createCipheriv()` |

```text
Readable Stream ──[ Chunk: 64KB ]──> Transform Stream ──[ Gzipped: 12KB ]──> Writable Stream
(e.g., File on Disk)                 (e.g., zlib.createGzip)                 (e.g., HTTP Response)
```

### Flowing vs Paused Mode (Readable Streams)
- **Paused Mode (Default)**: The stream requires the consumer to explicitly pull data by calling `stream.read()`.
- **Flowing Mode**: Data is pushed automatically to listeners as fast as possible via `'data'` events. Switched on by adding a `data` listener or calling `stream.pipe()`.

### The Backpressure Contract
If a fast disk reads data at 500MB/s and pipes it to a slow network socket that transmits at only 1MB/s, the unconsumed chunks will buffer in RAM. **Backpressure** prevents this:
1. When a Writable stream's internal buffer exceeds **`highWaterMark`** (default 16KB or 64KB), `writable.write(chunk)` returns **`false`**.
2. The producer must stop writing and wait for the Writable stream to flush its buffer.
3. Once drained, the Writable stream emits the **`'drain'`** event, signaling the producer to resume.

---

## 3. How It Works Internally

### Backpressure Control Flow

```
 Producer: readStream.on('data', chunk => ...)
                   │
                   ▼
 [ writable.write(chunk) ]
                   │
                   ▼
 Is internal buffer size >= highWaterMark?
              /                  \
        NO   /                    \ YES
            v                      v
 [ Returns TRUE ]             [ Returns FALSE (Backpressure Triggered!) ]
 [ Keep writing ]             [ Pause Producer: readStream.pause()       ]
                                   │
                                   ▼
                              Downstream consumer slowly flushes buffer
                                   │
                                   ▼
                              Buffer drains below threshold
                              Writable emits 'drain' event
                                   │
                                   ▼
                              [ Resume Producer: readStream.resume() ]
```

---

## 4. Syntax & API Reference

### Stream Composition via `pipeline`

```javascript
import fs from 'node:fs';
import zlib from 'node:zlib';
import { pipeline } from 'node:stream/promises';

// The gold standard: pipeline manages error forwarding, cleanup, and backpressure!
await pipeline(
  fs.createReadStream('source.tar'),
  zlib.createGzip(),
  fs.createWriteStream('source.tar.gz')
);
console.log('Pipeline succeeded with zero leaks');
```

### Implementing Custom Transform Stream

```javascript
import { Transform } from 'node:stream';

class UpperCaseTransform extends Transform {
  _transform(chunk, encoding, callback) {
    try {
      const upper = chunk.toString('utf8').toUpperCase();
      this.push(upper); // Push transformed chunk downstream
      callback();       // Signal ready for next chunk
    } catch (err) {
      callback(err);    // Forward error
    }
  }
}
```

### Interoperability with Web Streams (WHATWG)

```javascript
import { Readable } from 'node:stream';

// Convert WHATWG Web Stream (e.g. from fetch response.body) to Node.js Stream
const nodeReadable = Readable.fromWeb(webReadableStream);

// Convert Node.js Stream to WHATWG Web Stream
const webStream = Readable.toWeb(nodeReadable);
```

---

## 5. Practical Examples

### Example 1: Robust Backpressure Handling without `pipeline`
```javascript
import fs from 'node:fs';

function writeMillionRows(writer, dataGen, encoding = 'utf8') {
  let i = 1_000_000;

  function write() {
    let canContinue = true;

    while (i > 0 && canContinue) {
      i--;
      const data = dataGen(i);

      if (i === 0) {
        // Last write: close stream
        writer.end(data, encoding);
      } else {
        // Test backpressure: write() returns false if buffer is full!
        canContinue = writer.write(data, encoding);
      }
    }

    if (i > 0) {
      // Buffer full! Wait for 'drain' event before continuing
      writer.once('drain', write);
    }
  }

  write();
}

// Usage
const fileStream = fs.createWriteStream('./large-export.csv');
writeMillionRows(fileStream, (idx) => `row_${idx},value_${idx}\n`);
```

### Example 2: Streaming Gzip HTTP Server
```javascript
import http from 'node:http';
import fs from 'node:fs';
import zlib from 'node:zlib';
import { pipeline } from 'node:stream';

const server = http.createServer((req, res) => {
  const filePath = './huge-video.mp4';

  res.writeHead(200, {
    'Content-Type': 'video/mp4',
    'Content-Encoding': 'gzip',
  });

  // Pipe file through Gzip directly into HTTP response socket
  pipeline(
    fs.createReadStream(filePath),
    zlib.createGzip(),
    res,
    (err) => {
      if (err) {
        console.error('Streaming pipeline encountered error:', err.message);
        // Note: Cannot send HTTP 500 here if headers were already sent!
      } else {
        console.log('Video stream completed successfully');
      }
    }
  );
});

server.listen(8080);
```

### Example 3: Parsing NDJSON (Newline Delimited JSON) Stream
```javascript
import { Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import fs from 'node:fs';

class NdjsonParser extends Transform {
  #buffer = '';

  constructor() {
    super({ objectMode: true }); // Emits parsed JavaScript objects!
  }

  _transform(chunk, encoding, callback) {
    this.#buffer += chunk.toString('utf8');
    const lines = this.#buffer.split('\n');
    this.#buffer = lines.pop() ?? ''; // Retain trailing fragment

    for (const line of lines) {
      if (line.trim().length > 0) {
        try {
          this.push(JSON.parse(line));
        } catch (err) {
          return callback(err);
        }
      }
    }
    callback();
  }

  _flush(callback) {
    if (this.#buffer.trim().length > 0) {
      try {
        this.push(JSON.parse(this.#buffer));
      } catch (err) {
        return callback(err);
      }
    }
    callback();
  }
}

// Usage with Async Generator
async function processNdjson() {
  const stream = fs.createReadStream('./events.ndjson');
  const parser = new NdjsonParser();

  for await (const record of stream.pipe(parser)) {
    console.log('Parsed record:', record.eventId);
  }
}
```

### Example 4: Creating a Stream from an Async Generator
```javascript
import { Readable } from 'node:stream';

async function* generateTransactions() {
  for (let i = 1; i <= 5; i++) {
    await new Promise(r => setTimeout(r, 200));
    yield JSON.stringify({ txId: i, amount: i * 100 }) + '\n';
  }
}

// Adapt async generator directly into a Node.js Readable stream!
const transactionStream = Readable.from(generateTransactions());

transactionStream.on('data', (chunk) => {
  process.stdout.write(`Received: ${chunk}`);
});
```

### Example 5: Stream Teeing (Writing to Multiple Destinations)
```javascript
import fs from 'node:fs';
import { PassThrough } from 'node:stream';

function splitStream(sourceStream, destA, destB) {
  const passA = new PassThrough();
  const passB = new PassThrough();

  sourceStream.pipe(passA).pipe(destA);
  sourceStream.pipe(passB).pipe(destB);
}

// Writes source.txt to both backup.txt and stdout simultaneously!
const source = fs.createReadStream('source.txt');
splitStream(source, fs.createWriteStream('backup.txt'), process.stdout);
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`stream.pipe()` Leaks Memory on Error**: The legacy `source.pipe(dest)` method **does not close destination streams or clean up listeners if an error occurs**! If `source` throws an error, `dest` remains permanently open, leaking file descriptors and sockets. **Always use `stream.pipeline()` instead.**
2. **Premature Stream Destruction (`ERR_STREAM_PREMATURE_CLOSE`)**: If a client disconnects an HTTP request while a stream is piping to `res`, the pipeline aborts with `ERR_STREAM_PREMATURE_CLOSE`. Handle this gracefully in production servers.
3. **Object Mode vs Byte Mode**: By default, streams handle binary `Buffer` or `string` chunks. Setting `{ objectMode: true }` allows streams to pass arbitrary JavaScript values (objects, numbers). However, in object mode, `highWaterMark` measures **object count** (default 16 objects), not byte size!
4. **`data` Event Disables Backpressure**: Attaching an `.on('data', ...)` listener forces a Readable stream into flowing mode. In this mode, chunks are fired continuously without respecting whether the consumer is ready, bypassing backpressure unless manually paused.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Using `source.pipe(dest)`** | `readStream.pipe(writeStream)` | Use `stream.pipeline()` | Leaks file handles and event listeners if an error is thrown. |
| **Ignoring `write()` Return Value**| Unconditional `writer.write(chunk)` in loop | Check `write()` and wait for `'drain'` | Exhausts RAM by buffering millions of chunks in memory. |
| **Missing Stream Error Handlers** | Creating stream without error handler | Wrap in `pipeline` or attach `.on('error')` | Unhandled stream error crashes the entire Node.js process. |
| **Mixing Flowing and Paused Mode** | Mixing `.on('data')` with `.read()` | Stick to one mode or use `for await...of` | Causes missed chunks or out-of-order execution. |
| **Double Pumping** | Piping to destination while consuming with `for await`| Consume via pipeline OR loop, never both | Both consumers compete for chunks, losing data. |
| **ObjectMode Buffer Confusion** | Expecting `highWaterMark: 16384` in object mode | Remember object mode measures object count | Sets buffer to 16,384 objects instead of 16KB! |

---

## 8. Performance Characteristics

- **Constant $O(1)$ Memory Footprint**: Streaming reduces memory usage from gigabytes down to the `highWaterMark` buffer size ($\approx 64\text{KB}$).
- **Throughput Boost via Backpressure**: Backpressure prevents memory thrashing, keeping buffers inside CPU L2/L3 cache sizes and eliminating severe Young Generation GC pauses.

---

## 9. Engine Internals

1. **`ReadableState` and `WritableState`**: Internally, every stream maintains a C++ and JS state object (`_readableState`, `_writableState`) holding a linked-list queue of chunks (`BufferList`).
2. **Sync vs Async Writes**: When `write()` returns `true`, data is written directly to the underlying socket or file handle buffer in C++. When it returns `false`, bytes are queued in userspace RAM until the kernel socket drains.

---

## 10. Best Practices & Design Guidelines

1. **Always Use `pipeline()` from `node:stream/promises`**: Standardize on `await pipeline(...)` for all stream forwarding.
2. **Use `for await...of` for Sequential Ingestion**: Native async iteration provides clean, backpressure-aware consumption without callback boilerplate.
3. **Explicitly Set `highWaterMark` for Network Streams**: Tune `highWaterMark` (e.g. 128KB for high-speed local disk, 16KB for slow mobile network streams).
4. **Clean Up In-Flight Streams on Abort**: Pass `{ signal }` to `pipeline(source, dest, { signal })` to support cancellation.

---

## 11. Real-World Case Study

### High-Volume Log Compression and Encryption Pipeline

An enterprise audit service archives 50GB log files every night, compressing them with Gzip and encrypting them with AES-256-GCM before uploading to cloud storage.

```javascript
import fs from 'node:fs';
import zlib from 'node:zlib';
import crypto from 'node:crypto';
import { pipeline } from 'node:stream/promises';

class LogArchiver {
  #encryptionKey;

  constructor(encryptionKey) {
    this.#encryptionKey = encryptionKey; // 32-byte Buffer
  }

  async archiveFile(inputLogPath, outputEncryptedPath) {
    const iv = crypto.randomBytes(16); // Initialization vector

    const readStream = fs.createReadStream(inputLogPath);
    const gzipStream = zlib.createGzip({ level: 9 });
    const cipher = crypto.createCipheriv('aes-256-gcm', this.#encryptionKey, iv);
    const writeStream = fs.createWriteStream(outputEncryptedPath);

    // Write IV directly at head of target file for decryption
    writeStream.write(iv);

    try {
      console.log(`Starting archive streaming pipeline for ${inputLogPath}...`);
      const startTime = performance.now();

      // Flawless pipeline: auto-cleans all streams if compression or write fails!
      await pipeline(
        readStream,
        gzipStream,
        cipher,
        writeStream
      );

      const duration = ((performance.now() - startTime) / 1000).toFixed(2);
      console.log(`Archive completed successfully in ${duration}s. Final size: ${fs.statSync(outputEncryptedPath).size} bytes`);
    } catch (err) {
      console.error('Archive pipeline aborted:', err.message);
      // Clean up partial corrupted file
      await fs.promises.unlink(outputEncryptedPath).catch(() => {});
      throw err;
    }
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. Why is <code>stream.pipeline()</code> strongly preferred over <code>source.pipe(dest)</code>?</strong></summary>

`source.pipe(dest)` does not forward errors properly between streams and does not automatically close destination streams if an error occurs on the source, creating resource and memory leaks. `stream.pipeline()` ensures that if any stream in the chain fails or closes prematurely, all streams in the pipeline are automatically destroyed and cleaned up.
</details>

<details>
<summary><strong>2. What is Backpressure, and how does it prevent memory crashes?</strong></summary>

Backpressure occurs when data is read from a producer faster than a downstream consumer can process or write it. Streams handle this via `writable.write()`, which returns `false` when its internal buffer reaches `highWaterMark`. The producer pauses reading until the consumer flushes its buffer and emits the `'drain'` event.
</details>

<details>
<summary><strong>3. What are the two reading modes of a Readable stream, and how do they differ?</strong></summary>

- **Paused Mode (Default)**: Chunks remain in the stream buffer until the consumer explicitly pulls them via `stream.read()`.
- **Flowing Mode**: Chunks are read from the underlying system and delivered to the consumer as fast as possible via `'data'` events without explicit pulling.
</details>

<details>
<summary><strong>4. What does the <code>highWaterMark</code> option configure?</strong></summary>

`highWaterMark` defines the maximum threshold (in bytes, or in object count for `objectMode`) of the stream's internal buffer before `write()` returns `false` or `read()` stops reading from the underlying resource. Default is 16KB for standard streams, 64KB for `fs` streams, and 16 objects for `objectMode`.
</details>

<details>
<summary><strong>5. What is the difference between a Duplex stream and a Transform stream?</strong></summary>

A `Duplex` stream has independent `Readable` and `Writable` sides (such as a TCP network socket where bytes read have no correlation to bytes written). A `Transform` stream is a Duplex stream where the output is directly calculated or transformed from the input (such as a Gzip compressor or crypto cipher).
</details>

<details>
<summary><strong>6. How does <code>Readable.from(iterable)</code> work?</strong></summary>

It converts any synchronous or asynchronous iterable (such as an array or an async generator function) into a first-class Node.js `Readable` stream, managing pausing, resuming, and backpressure automatically.
</details>

<details>
<summary><strong>7. What happens if you do not listen to the <code>'error'</code> event on a stream?</strong></summary>

If an error occurs on a stream and no error handler is registered, Node.js treats it as an unhandled exception and crashes the entire process.
</details>

<details>
<summary><strong>8. How do you convert between Node.js streams and WHATWG Web Streams?</strong></summary>

Using the core methods `Readable.toWeb(nodeStream)` and `Readable.fromWeb(webStream)`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement a Line-by-Line Chunking Transform Stream
Write a custom `Transform` stream `LineSplitter` that takes raw byte buffers containing newline-delimited text and emits individual string lines downstream in object mode.

<details>
<summary>View Solution</summary>

```javascript
import { Transform } from 'node:stream';

export class LineSplitter extends Transform {
  #buffer = '';

  constructor() {
    super({ objectMode: true });
  }

  _transform(chunk, encoding, callback) {
    this.#buffer += chunk.toString('utf8');
    const lines = this.#buffer.split('\n');
    this.#buffer = lines.pop() ?? ''; // Keep trailing incomplete fragment

    for (const line of lines) {
      this.push(line);
    }
    callback();
  }

  _flush(callback) {
    if (this.#buffer.length > 0) {
      this.push(this.#buffer);
    }
    callback();
  }
}
```
</details>

### Challenge 2: Streaming Byte Counter Passthrough
Implement a `PassThrough` subclass `ByteCounterStream` that counts total bytes flowing through it without mutating the data, emitting a `'progress'` event every time 1MB passes through.

<details>
<summary>View Solution</summary>

```javascript
import { PassThrough } from 'node:stream';

export class ByteCounterStream extends PassThrough {
  #totalBytes = 0;
  #lastReportedMb = 0;

  _transform(chunk, encoding, callback) {
    this.#totalBytes += chunk.length;
    const currentMb = Math.floor(this.#totalBytes / (1024 * 1024));

    if (currentMb > this.#lastReportedMb) {
      this.#lastReportedMb = currentMb;
      this.emit('progress', { totalBytes: this.#totalBytes, megabytes: currentMb });
    }

    super._transform(chunk, encoding, callback);
  }

  get totalBytes() {
    return this.#totalBytes;
  }
}
```
</details>
