# Buffers

A `Buffer` is Node's fixed-size container for **raw bytes**. It is a subclass of `Uint8Array` (see [Typed Arrays and ArrayBuffer](../09_built-in-objects/10_typed-arrays-and-arraybuffer.md)) with extra methods for encodings and binary parsing. File reads without an encoding, socket data, and crypto output are all buffers. `Buffer` is a global; you can also `import { Buffer } from 'node:buffer'`.

## Creating buffers

```js
Buffer.from('héllo');                   // from a string (UTF-8 by default)
Buffer.from('68656c6c6f', 'hex');       // from hex
Buffer.from('aGVsbG8=', 'base64');      // from base64
Buffer.from([104, 105]);                // from an array of bytes
Buffer.from(otherBuffer);               // copy
Buffer.alloc(8);                        // 8 zero-filled bytes (safe)
Buffer.alloc(8, 0xff);                  // filled with 0xff
Buffer.allocUnsafe(8);                  // uninitialized memory: faster, may contain old data
```

| Method | Use |
|--------|-----|
| `Buffer.alloc(n)` | Safe default |
| `Buffer.allocUnsafe(n)` | Performance-critical code where you overwrite every byte |
| `Buffer.from(...)` | Convert from strings, arrays, buffers, `ArrayBuffer` |
| `new Buffer(...)` | **Deprecated.** Never use |

`allocUnsafe` can expose bytes from previously freed memory. If you do not fill the whole buffer, you may leak data. Use `alloc` unless profiling says otherwise.

## Encodings

```js
const buf = Buffer.from('héllo', 'utf8');
buf.length;                  // 6 (bytes, not characters: é takes 2 bytes)
'héllo'.length;              // 5

buf.toString();              // 'héllo' (utf8)
buf.toString('hex');         // '68c3a96c6c6f'
buf.toString('base64');      // 'aMOpbGxv'
buf.toString('base64url');   // URL-safe base64 (no +, /, padding)
buf.toString('latin1');
buf.toString('utf8', 0, 3);  // decode a byte range
```

| Encoding | Notes |
|----------|-------|
| `utf8` | Default; variable-length (1 to 4 bytes per character) |
| `utf16le` / `ucs2` | 2 bytes per code unit |
| `latin1` / `binary` | One byte per character |
| `ascii` | 7-bit; non-ASCII bytes get masked |
| `hex` | Two hex characters per byte |
| `base64` / `base64url` | Text-safe binary encoding |

Byte length of a string without allocating:

```js
Buffer.byteLength('héllo');       // 6
Buffer.byteLength('héllo', 'utf16le');
```

## Reading and writing bytes

```js
const buf = Buffer.from([0x41, 0x42, 0x43]);

buf[0];                 // 65 (indexing returns numbers 0 to 255)
buf[1] = 0x5a;          // writes a byte (values wrap modulo 256)
buf.at(-1);             // 67
buf.length;             // 3

for (const byte of buf) console.log(byte);
```

Writing text into a buffer:

```js
const b = Buffer.alloc(10);
const written = b.write('hi', 2, 'utf8');   // write at offset 2, returns bytes written
```

## Binary numbers

```js
const b = Buffer.alloc(8);

b.writeUInt8(255, 0);
b.writeUInt16BE(0x1234, 1);       // big-endian
b.writeUInt32LE(0xdeadbeef, 3);   // little-endian
b.writeInt16BE(-2, 7 - 1);

b.readUInt16BE(1);                // 0x1234
b.readUInt32LE(3);

b.writeDoubleBE(3.14, 0);         // 8-byte float
b.readDoubleBE(0);

b.writeBigUInt64BE(2n ** 63n, 0); // BigInt for 64-bit integers
b.readBigUInt64BE(0);
```

| Suffix | Meaning |
|--------|---------|
| `BE` | Big-endian: most significant byte first (network order) |
| `LE` | Little-endian: least significant byte first (most CPUs) |
| `UInt` / `Int` | Unsigned / signed |
| `8`, `16`, `32` | Bit size |
| `Float`, `Double` | 32-bit and 64-bit floats |
| `BigInt64`, `BigUInt64` | 64-bit integers as `BigInt` |

Out-of-range offsets or values throw `ERR_OUT_OF_RANGE`.

A tiny binary protocol example:

```js
// Header: 2-byte type, 4-byte payload length, then the payload
function encode(type, payload) {
  const body = Buffer.from(payload, 'utf8');
  const header = Buffer.alloc(6);
  header.writeUInt16BE(type, 0);
  header.writeUInt32BE(body.length, 2);
  return Buffer.concat([header, body]);
}

function decode(buf) {
  const type = buf.readUInt16BE(0);
  const length = buf.readUInt32BE(2);
  const payload = buf.toString('utf8', 6, 6 + length);
  return { type, payload };
}

decode(encode(1, 'hello'));   // { type: 1, payload: 'hello' }
```

## Combining, slicing, copying

```js
Buffer.concat([a, b, c]);                // new buffer with all bytes
Buffer.concat([a, b], totalLength);      // pass the length to skip a pass

const part = buf.subarray(2, 5);         // VIEW: shares memory with buf
part[0] = 0;                             // also changes buf!

const copy = Buffer.from(buf.subarray(2, 5));   // independent copy
buf.copy(target, targetStart, sourceStart, sourceEnd);
```

`buf.slice()` is deprecated; it also returns a view. Use `subarray` (a view) or `Buffer.from(...)` (a copy), and be explicit about which one you want.

## Comparing and searching

```js
buf1.equals(buf2);                       // true if identical bytes
Buffer.compare(buf1, buf2);              // -1, 0, 1 (for sorting)
buf.includes('lo');
buf.indexOf('lo');                       // byte index, -1 if not found
buf.fill(0);                             // overwrite all bytes (e.g. wipe a secret)

// Constant-time comparison for secrets (see node:crypto)
import { timingSafeEqual } from 'node:crypto';
timingSafeEqual(a, b);                   // buffers must have the same length
```

Do not compare secrets (tokens, MACs) with `===` or `.equals`: they can leak timing information.

## Relationship to `Uint8Array` and `ArrayBuffer`

```js
const buf = Buffer.from('hi');
buf instanceof Uint8Array;    // true

new Uint8Array(buf);                                          // copy
new Uint8Array(buf.buffer, buf.byteOffset, buf.length);       // view, no copy

Buffer.from(arrayBuffer);                                     // view over the ArrayBuffer
Buffer.from(arrayBuffer, byteOffset, length);
```

Small buffers created with `Buffer.from` or `allocUnsafe` come from a shared **pool**, so `buf.buffer` can be larger than the buffer and `byteOffset` is not always 0. Always pass `byteOffset` and `length` when building a view.

## JSON and inspection

```js
JSON.stringify(Buffer.from('hi'));
// '{"type":"Buffer","data":[104,105]}'

Buffer.from(JSON.parse(json));        // round-trip from that shape

console.log(Buffer.from('hello'));    // <Buffer 68 65 6c 6c 6f>
buf.toString('hex');                  // easiest way to eyeball bytes
```

## Text decoding with `TextDecoder`

For web-standard code, or streaming decode of multi-byte text:

```js
const dec = new TextDecoder('utf-8');
dec.decode(buf);

// Streaming: handles characters split across chunks
const streamDec = new TextDecoder();
let text = '';
for await (const chunk of stream) text += streamDec.decode(chunk, { stream: true });
text += streamDec.decode();
```

`StringDecoder` from `node:string_decoder` does the same.

## Buffers and streams

Without an encoding, streams emit `Buffer` chunks:

```js
const chunks = [];
for await (const chunk of createReadStream('file.bin')) chunks.push(chunk);
const whole = Buffer.concat(chunks);
```

Do not build text with `str += chunk` on raw buffers; multi-byte characters may be split across chunks. Collect and `concat`, or set an encoding on the stream.

## Constants and limits

```js
import { constants, kMaxLength } from 'node:buffer';
kMaxLength;                  // maximum buffer size (platform dependent, very large)
```

Buffers live **outside the V8 heap** (as external memory), so large buffers do not show up in `heapUsed`. Watch `process.memoryUsage().external` and `arrayBuffers`.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `new Buffer(n)` | Deprecated and potentially unsafe | `Buffer.alloc` / `Buffer.from` |
| `allocUnsafe` without filling | Leaks old memory contents | `alloc`, or overwrite every byte |
| Confusing `.length` with character count | Length is bytes | `Buffer.byteLength`, or count characters on the string |
| Mutating a `subarray` or `slice` | Changes the original | `Buffer.from(sub)` to copy |
| `toString()` on partial UTF-8 chunks | Garbled characters | `StringDecoder`, or `Buffer.concat` first |
| Comparing secrets with `===` | Timing leaks | `crypto.timingSafeEqual` |
| Ignoring endianness | Wrong numbers across systems | Pick `BE` or `LE` by protocol spec |
| Using `buf.buffer` directly | Pool offset bugs | Use `byteOffset` and `length` |
| Assuming `hex`/`base64` round-trip arbitrary text | They are byte encodings | Encode text to bytes first |

## Key takeaways

- `Buffer` holds raw bytes; it is a `Uint8Array` subclass
- Use `Buffer.alloc` and `Buffer.from`; avoid `new Buffer` and be careful with `allocUnsafe`
- `.length` counts bytes; encodings (`utf8`, `hex`, `base64`) convert between bytes and text
- `subarray` shares memory; copy with `Buffer.from` when you need independence
- Use `readUInt*` / `writeUInt*` with explicit endianness for binary protocols
- Compare secrets with `timingSafeEqual`

**Next:** [HTTP Server](./08_http-server.md)
