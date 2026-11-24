# Cheatsheet 04: Streams & Buffers Reference Card

> Production syntax reference for Node.js binary data handling, Buffer methods, and stream pipelines.

---

### 1. Modern Stream Pipeline (`stream/promises`)
Always prefer `pipeline` over `.pipe()` for automatic error handling and resource destruction:
```javascript
import { pipeline } from 'node:stream/promises';
import fs from 'node:fs';
import zlib from 'node:zlib';

await pipeline(
  fs.createReadStream('input.tar'),
  zlib.createGzip(),
  fs.createWriteStream('output.tar.gz')
);
```

---

### 2. Async Iteration over Streams (`for await...of`)
```javascript
import fs from 'node:fs';

const stream = fs.createReadStream('large-file.txt', { encoding: 'utf8', highWaterMark: 64 * 1024 });

for await (const chunk of stream) {
  // Process 64KB chunk
  console.log('Received chunk of length:', chunk.length);
}
```

---

### 3. Creating Streams from Iterables (`Readable.from`)
```javascript
import { Readable } from 'node:stream';

const data = ['Alice', 'Bob', 'Charlie'];
const readableStream = Readable.from(data);

readableStream.on('data', (chunk) => console.log(chunk));
```

---

### 4. Custom Transform Stream
```javascript
import { Transform } from 'node:stream';

export class UpperCaseTransform extends Transform {
  _transform(chunk, encoding, callback) {
    const uppercased = chunk.toString().toUpperCase();
    callback(null, uppercased);
  }
}
```

---

### 5. Essential Buffer Methods

| Method | Syntax | Description |
| :--- | :--- | :--- |
| **`Buffer.alloc()`** | `Buffer.alloc(1024)` | Allocates 1KB zero-filled safe buffer. |
| **`Buffer.allocUnsafe()`** | `Buffer.allocUnsafe(1024)` | Fast allocation without zero-fill (use only when overwriting immediately). |
| **`Buffer.from()`** | `Buffer.from('hello', 'utf8')` | Creates buffer from string or array. |
| **`Buffer.concat()`** | `Buffer.concat([buf1, buf2])` | Concatenates an array of buffers into a single buffer. |
| **`buf.toString()`** | `buf.toString('hex')` | Decodes buffer to string (`'utf8'`, `'hex'`, `'base64'`). |
| **`buf.byteLength`** | `Buffer.byteLength(str)` | Returns byte length of string (accounts for multi-byte Unicode). |
| **`buf.slice()`** | `buf.subarray(0, 4)` | Creates a zero-copy view of buffer segment. |
