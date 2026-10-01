# Typed Arrays and ArrayBuffer

For working with **binary data** (files, network protocols, audio, images, WebGL, WebAssembly) JavaScript provides `ArrayBuffer`, typed array views and `DataView`.

```
ArrayBuffer (raw bytes)  ◄── view ──  Uint8Array / Float32Array / DataView
```

## ArrayBuffer: raw memory

```js
const buffer = new ArrayBuffer(16);      // 16 zeroed bytes
buffer.byteLength;                        // 16
buffer.slice(4, 8);                       // copy of bytes 4..7 (new buffer)
```

A buffer has no way to read or write by itself: you need a **view**.

## Typed arrays

| Type | Element | Bytes | Range |
|------|---------|-------|-------|
| `Int8Array` / `Uint8Array` | 8-bit int | 1 | -128..127 / 0..255 |
| `Uint8ClampedArray` | 8-bit, clamped | 1 | 0..255 (canvas) |
| `Int16Array` / `Uint16Array` | 16-bit int | 2 | |
| `Int32Array` / `Uint32Array` | 32-bit int | 4 | |
| `Float32Array` | 32-bit float | 4 | |
| `Float64Array` | 64-bit float | 8 | |
| `BigInt64Array` / `BigUint64Array` | 64-bit BigInt | 8 | |

```js
const bytes = new Uint8Array(4);          // [0, 0, 0, 0]
bytes[0] = 255;
bytes[1] = 256;                            // wraps: 0
bytes[2] = -1;                             // wraps: 255
new Uint8Array([1, 2, 3]);                 // from array
new Float32Array(buffer);                  // view over an existing buffer (byteLength % 4 === 0)
new Uint8Array(buffer, 4, 8);              // view: byteOffset 4, length 8
Uint8Array.from("abc", (c) => c.charCodeAt(0));
```

Typed arrays have most array methods: `map`, `filter`, `slice`, `find`, `includes`, `sort` (numeric by default), `fill`, `set`, `at`, `toSorted`, `join`, and iterate with `for...of`. They have fixed length: no `push`/`pop`.

## Views share memory

```js
const buf = new ArrayBuffer(4);
const u8 = new Uint8Array(buf);
const u32 = new Uint32Array(buf);

u32[0] = 0x01020304;
[...u8];        // [4, 3, 2, 1] on little-endian machines
```

- `subarray(a, b)` creates a **view** (no copy)
- `slice(a, b)` creates a **copy**
- `set(source, offset)` copies values in bulk

```js
const big = new Uint8Array(10);
big.set([1, 2, 3], 2);       // [0, 0, 1, 2, 3, 0, ...]
const view = big.subarray(2, 5);
view[0] = 99;                // big[2] is now 99
```

## Endianness and DataView

`DataView` reads and writes any type at any byte offset with explicit endianness. Use it for protocols and file formats.

```js
const dv = new DataView(new ArrayBuffer(8));

dv.setUint16(0, 0xcafe);            // big-endian by default
dv.setUint16(2, 0xcafe, true);      // little-endian
dv.setFloat32(4, 3.14, true);

dv.getUint8(0);                      // 0xca
dv.getUint16(0);                     // 0xcafe
dv.getFloat32(4, true);
dv.getBigUint64?.(0);
```

Typed arrays use the platform's byte order (almost always little-endian); `DataView` is explicit.

## Text and bytes

```js
const enc = new TextEncoder();               // always UTF-8
const bytes = enc.encode("héllo");           // Uint8Array
const dec = new TextDecoder("utf-8");
dec.decode(bytes);                            // "héllo"

new TextDecoder("utf-16le").decode(u16);
enc.encodeInto("abc", target);                // no allocation
```

## Base64 and hex

```js
btoa(String.fromCharCode(...bytes));          // small data only (binary string)
Uint8Array.from(atob("aGk="), (c) => c.charCodeAt(0));

const toHex = (u8) => [...u8].map((b) => b.toString(16).padStart(2, "0")).join("");
const fromHex = (s) => Uint8Array.from(s.match(/../g), (h) => parseInt(h, 16));
```

Node: `Buffer.from(data).toString("base64" | "hex")` (see the Node chapter). Newer runtimes also add `Uint8Array.prototype.toBase64` / `fromBase64` (check support).

## Where binary data appears

| API | Gives / takes |
|-----|---------------|
| `fetch` | `response.arrayBuffer()`, `response.body` (stream of `Uint8Array`), request bodies |
| `Blob` / `File` | `blob.arrayBuffer()`, `blob.stream()`, `new Blob([bytes])` |
| `FileReader` | `readAsArrayBuffer` |
| `crypto` | `getRandomValues(typedArray)`, `subtle.digest(...)` returns `ArrayBuffer` |
| WebSocket | `binaryType = "arraybuffer"` |
| Canvas | `ImageData.data` is a `Uint8ClampedArray` |
| WebGL / WebGPU | `Float32Array` vertex data |
| WebAssembly | `WebAssembly.Memory.buffer` |
| Node streams | `Buffer` (a `Uint8Array` subclass) |

```js
const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode("hello"));
toHex(new Uint8Array(digest));
```

## Resizable and transferable buffers (ES2024)

```js
const rab = new ArrayBuffer(8, { maxByteLength: 64 });
rab.resize(32);
rab.resizable;                    // true

const moved = rab.transfer();     // moves ownership, old buffer becomes detached (zero length)
```

Buffers can also be **transferred** to workers without copying:

```js
worker.postMessage(buffer, [buffer]);   // buffer is now unusable (detached) here
```

## SharedArrayBuffer and Atomics

`SharedArrayBuffer` shares memory between threads (workers); `Atomics` provides race-free operations. It requires cross-origin isolation in browsers (`COOP`/`COEP` headers). See `17_concurrency-and-parallelism/04_sharedarraybuffer-and-atomics.md`.

## Performance

- Typed arrays store raw numbers contiguously: much faster and smaller than arrays of numbers for big numeric data
- Avoid creating many tiny buffers; reuse and use `subarray`
- Copying is O(n): prefer views, `subarray`, and transfer over `slice`
- Access out of range silently does nothing (read gives `undefined`)

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming values are clamped or errors on overflow | They wrap (except `Uint8ClampedArray`) | Validate ranges |
| Constructing a view with misaligned offset | `RangeError` | Offset must be a multiple of the element size |
| Endianness assumptions | Different machines/protocols | `DataView` with explicit flag |
| Using `slice` where you wanted a view | Copies, edits not shared | `subarray` |
| Spreading huge arrays into `String.fromCharCode` | Stack overflow | Chunk, or `TextDecoder` |
| Using `btoa` on Unicode text | `InvalidCharacterError` | Encode to bytes first (`TextEncoder`) |
| Using a detached buffer | Length 0 or errors | Track ownership after transfer |
| Treating `Buffer` (Node) as identical to `Uint8Array` | Extra methods, pooling quirks | Convert with `new Uint8Array(buf)` when needed |
| Default `sort()` mental model | Typed arrays sort numerically | Fine, but know arrays differ |

## Key takeaways

- `ArrayBuffer` is raw memory; typed arrays and `DataView` are views onto it
- Use `subarray` for views, `slice` for copies, and `DataView` for explicit endianness
- `TextEncoder`/`TextDecoder` convert strings and bytes
- Transfer buffers instead of copying when sending to workers

**Next:** [Proxy and Reflect](./11_proxy-and-reflect.md)
