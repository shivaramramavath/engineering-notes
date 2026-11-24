# Node.js Technical Interview: Streams & Buffers Q&A

> Essential questions and technical deep-dives into Node.js binary data handling, Buffer security vulnerabilities, stream mechanics, backpressure orchestration, and memory leak prevention.

---

### Q1: What is Backpressure in Node.js Streams? Explain the internal buffer mechanics, `highWaterMark`, and the `drain` event.

#### In-Depth Answer
**Backpressure** is the feedback mechanism that prevents a fast data producer (e.g. reading from a 10Gbps SSD) from overwhelming a slower consumer (e.g. streaming over a 1Mbps mobile connection), which would otherwise cause unbounded memory growth and process termination via Out-Of-Memory (OOM).

#### Internal Mechanics:
1. Every Stream instance maintains an internal buffer governed by `highWaterMark` (default: 64KB for binary streams; 16 objects for `objectMode` streams).
2. When calling `writable.write(chunk)`:
   - If internal buffer size is below `highWaterMark`, `write()` returns `true`.
   - If the chunk pushes the internal buffer to or beyond `highWaterMark`, `write()` returns **`false`**.
3. When `false` is returned, **the producer MUST immediately pause writing**:
   - For a `Readable` stream, call `readable.pause()`.
4. As the destination OS socket flushes the bytes across the network, the internal buffer drains.
5. Once the buffer is emptied, the `Writable` stream emits the **`'drain'`** event.
6. The producer listens for `'drain'`, resumes emitting (`readable.resume()`), and continues writing.

```javascript
// Manual backpressure handling implementation:
import fs from 'node:fs';

function writeMillionRows(writer, data, encoding, callback) {
  let i = 1000000;
  function write() {
    let ok = true;
    do {
      i--;
      if (i === 0) {
        // Last write triggers callback
        writer.write(data, encoding, callback);
      } else {
        // See if we should continue, or wait for drain
        ok = writer.write(data, encoding);
      }
    } while (i > 0 && ok);

    if (i > 0) {
      // Buffer full! Pause writing and wait for drain
      writer.once('drain', write);
    }
  }
  write();
}
```

---

### Q2: Why is `readable.pipe(writable)` considered dangerous in production code? What is the modern recommended alternative?

#### In-Depth Answer
While `readable.pipe(writable)` handles backpressure automatically, it possesses **critical architectural flaws**:
1. **Broken Error Propagation**: Errors are **not** forwarded across the pipe. If an error occurs on `readable`, `writable` is left open; if an error occurs on `writable`, `readable` continues reading.
2. **Resource Leaks**: Neither stream is automatically destroyed on error, causing file descriptor leaks, zombie TCP sockets, and unclosed database connections.
3. **Complex Multi-Stream Teardown**: In a chain like `source.pipe(gzip).pipe(dest)`, handling errors requires attaching `.on('error', ...)` handlers to every individual stream in the chain manually.

#### The Production Standard: `stream.pipeline()`
Node.js provides `stream.pipeline` (and `stream/promises` `pipeline`), which:
- Automatically handles backpressure.
- Forwards errors from any point in the pipeline to a single callback or rejected Promise.
- **Properly destroys all streams in the pipeline** if any single stream emits an error or is abruptly closed, guaranteeing zero file descriptor leaks.

```javascript
import { pipeline } from 'node:stream/promises';
import fs from 'node:fs';
import zlib from 'node:zlib';

async function compressFile(source, destination) {
  try {
    await pipeline(
      fs.createReadStream(source),
      zlib.createGzip(),
      fs.createWriteStream(destination)
    );
    console.log('Pipeline succeeded! All handles closed cleanly.');
  } catch (err) {
    console.error('Pipeline failed; all streams cleanly destroyed:', err);
  }
}
```

---

### Q3: What is the security danger of using `Buffer.allocUnsafe()` over `Buffer.alloc()`? What is the 8KB Buffer Pool?

#### In-Depth Answer

#### Security Vulnerability:
- **`Buffer.alloc(size)`**: Allocates memory outside the V8 heap and **zero-fills (clears)** all bytes. It is safe against information disclosure, but carries a small CPU zeroing overhead.
- **`Buffer.allocUnsafe(size)`**: Allocates memory **without clearing existing bytes**. The newly allocated buffer contains uninitialized, raw historical RAM memory that was previously used by other operations or processes.

#### CVE Real-World Exploit Scenario:
If a developer allocates an unsafe buffer and sends it over the network (or serves a partial HTTP response), the uninitialized buffer may contain sensitive data residing in recently freed memory—including user passwords, TLS private keys, credit card numbers, or auth tokens.

#### The 8KB Buffer Pool (`Buffer.poolSize`):
To optimize performance and avoid excessive system calls (`malloc`), Node.js pre-allocates an internal 8KB slab (`Buffer.poolSize = 8192`):
- Any buffer allocated with size $\le 4096$ bytes (`Buffer.allocUnsafe(size)` or `Buffer.from(string)`) is sliced from this shared 8KB internal slab.
- **Gotcha / Memory Leak**: If you allocate a 10-byte buffer from the slab and retain a reference to it in a long-lived closure, the **entire 8KB slab cannot be garbage collected** by V8 until that tiny 10-byte buffer reference is freed!

---

### Q4: Explain the 4 Stream types in Node.js and the difference between `Duplex` and `Transform`.

#### In-Depth Answer

1. **`Readable` Stream**: Source from which data can be consumed (e.g. `fs.createReadStream`, `http.IncomingMessage`, `process.stdin`).
2. **`Writable` Stream**: Destination to which data can be written (e.g. `fs.createWriteStream`, `http.ServerResponse`, `process.stdout`).
3. **`Duplex` Stream**: An entity that is **both** Readable and Writable, but where the read and write channels are **independent** (e.g. a TCP socket via `net.Socket`: writing bytes down the socket does not directly transform or produce the bytes read back from the remote peer).
4. **`Transform` Stream**: A specialized type of Duplex stream where the output is causally computed from the input (e.g. `zlib.createGzip()` compresses input bytes; `crypto.createCipheriv()` encrypts input bytes). It requires implementing a `_transform(chunk, encoding, callback)` method.

---

### Q5: What is `objectMode` in Node.js streams, and when should you use it?

#### In-Depth Answer
By default, Node.js streams operate strictly on binary chunks (`Buffer` or `Uint8Array`, or `string` decoded as utf8).
When a stream is initialized with `{ objectMode: true }`:
- The stream accepts and emits arbitrary JavaScript values (objects, arrays, numbers, symbols) instead of byte chunks.
- The meaning of `highWaterMark` changes: instead of representing **bytes** (default: 64KB), it represents **the number of objects** (default: 16).
- Use cases: ETL pipelines, CSV line transformers, database cursor streaming (e.g. streaming 100,000 DB user records through a transform stream that maps them to DTO objects).
