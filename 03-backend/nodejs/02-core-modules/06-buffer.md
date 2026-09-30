# `Buffer` — Raw Binary Data

`Buffer` is Node's way of handling raw binary data — fixed-size chunks of memory outside JavaScript's normal string/object model. Streams, file I/O, and network protocols all move data around as buffers under the hood.

```js
// Buffer is a global — no import needed
```

## Why buffers exist

JavaScript strings are great for text, but text is only one kind of data. Files, network packets, and images are fundamentally **binary** — raw bytes that may not represent valid text at all. `Buffer` is Node's type for handling that raw byte data directly, without forcing an assumption about character encoding.

```js
const buf = Buffer.from("hello");
console.log(buf);
// <Buffer 68 65 6c 6c 6f>   — the raw byte values, in hex
```

---

## Creating buffers

```js
Buffer.from("hello"); // from a string (UTF-8 by default)
Buffer.from("hello", "utf-8"); // explicit encoding
Buffer.from([72, 101, 108, 108, 111]); // from an array of byte values
Buffer.alloc(10); // 10 zero-filled bytes
Buffer.alloc(10, 1); // 10 bytes, each filled with the value 1
```

`Buffer.alloc()` is preferred over the older `Buffer(size)` constructor (deprecated, and historically a security issue since it could expose old, uninitialized memory contents) — always use `alloc`/`from` explicitly.

---

## Converting between buffers and strings

```js
const buf = Buffer.from("héllo", "utf-8");

buf.toString(); // "héllo" (defaults to utf-8)
buf.toString("hex"); // "68c3a96c6c6f"
buf.toString("base64"); // "aMOpbGxv"
```

### Common encodings

| Encoding          | Use for                                                                               |
| ----------------- | ------------------------------------------------------------------------------------- |
| `utf-8` (default) | Normal text                                                                           |
| `base64`          | Encoding binary data as ASCII text — email attachments, embedding images in JSON/HTML |
| `hex`             | Human-readable representation of raw bytes — hashes, debugging                        |
| `ascii`           | Legacy 7-bit text (rarely needed now)                                                 |

---

## Buffer length vs string length

```js
const str = "héllo";
console.log(str.length); // 5 — character count
console.log(Buffer.byteLength(str)); // 6 — byte count (é takes 2 bytes in UTF-8)
```

This distinction matters whenever you're dealing with byte-based limits (e.g. a database column's byte size, or a network protocol's length field) rather than character counts — non-ASCII characters commonly take more than one byte each in UTF-8.

---

## Reading and writing individual bytes

```js
const buf = Buffer.from("hello");

buf[0]; // 104 — the byte value of "h"
buf[0] = 72; // mutate it — now starts with "H"
console.log(buf.toString()); // "Hello"
```

Buffers are mutable, fixed-length arrays of bytes — indexing into one gives you the raw numeric byte value, not a character.

---

## Combining and comparing buffers

```js
const buf1 = Buffer.from("Hello, ");
const buf2 = Buffer.from("world!");

const combined = Buffer.concat([buf1, buf2]);
combined.toString(); // "Hello, world!"
```

```js
Buffer.from("abc").equals(Buffer.from("abc")); // true
Buffer.compare(buf1, buf2); // -1, 0, or 1 — like a string comparator
```

---

## Where buffers show up in practice

### Reading a file without an encoding

```js
import { readFile } from "node:fs/promises";

const data = await readFile("image.png"); // returns a Buffer, not a string
```

No encoding was specified, so `fs` gives you the raw bytes — appropriate for binary files like images, where "decoding as UTF-8 text" wouldn't make sense.

### Working with streams

```js
stream.on("data", (chunk) => {
  console.log(chunk); // chunk is a Buffer, unless an encoding was set on the stream
});
```

### Crypto and hashing

```js
import { createHash } from "node:crypto";

const hash = createHash("sha256").update("some data").digest();
console.log(hash); // a Buffer
console.log(hash.toString("hex")); // the familiar hex-string hash you usually see
```

(Full coverage of `crypto` in `crypto/node-crypto.md`.)

### Base64-encoding binary data for JSON/text contexts

```js
const imageBuffer = await readFile("photo.png");
const base64Image = imageBuffer.toString("base64");
// now safe to embed in JSON, a data: URI, etc.
```

---

## Buffers vs `TypedArray`/`ArrayBuffer`

`Buffer` is actually a subclass of JavaScript's standard `Uint8Array`, with extra Node-specific convenience methods (`toString(encoding)`, `Buffer.concat`, etc.) layered on top. If you're used to `ArrayBuffer`/`Uint8Array` from browser JavaScript, most of that knowledge transfers directly — `Buffer` is Node's specialized, more convenient version of the same underlying idea.

## Common mistakes

- **Using the deprecated `new Buffer(size)` constructor** — use `Buffer.alloc(size)` instead, which is safer and doesn't expose old memory contents.
- **Assuming `.length` gives byte count for a string** — it gives the character count; use `Buffer.byteLength(str)` when the byte count specifically matters (e.g. for a protocol length field).
- **Treating an image/binary file's buffer as text** — calling `.toString()` on binary data (rather than base64/hex) usually produces garbled, meaningless output, since it forces a text encoding interpretation onto non-text bytes.

## Quick summary

- `Buffer` holds raw binary data — the byte-level representation underneath strings, files, and network data
- `Buffer.from()` creates one; `.toString(encoding)` converts back to a string, defaulting to UTF-8
- `utf-8`, `base64`, and `hex` are the encodings you'll use most often
- String `.length` counts characters; `Buffer.byteLength()` counts actual bytes — these differ for non-ASCII text
- `fs.readFile` without an encoding, streams, and `crypto` hashes all hand you (or expect) `Buffer`s

## Next

**`07-process.md`** covers the global `process` object — environment variables, exit codes, and signal handling.
