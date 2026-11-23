# JavaScript TypedArrays & ArrayBuffer: Raw Binary, DataView, Endianness & Shared Memory

> Deep technical guide to ECMAScript binary memory management, `ArrayBuffer`, Typed Array views, `DataView` endianness handling, `Uint8ClampedArray`, ES2024 buffer transfer, and `SharedArrayBuffer` Atomics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Systems Programming |
| **Prerequisites** | [09_built-in-objects/02_number-and-bigint.md](02_number-and-bigint.md), [01_fundamentals/05_operators.md](../01_fundamentals/05_operators.md) |
| **Time to Read** | 27 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (ArrayBuffer, TypedArrays, DataView) / ES2017 (SharedArrayBuffer, Atomics) / ES2024 (ArrayBuffer transfer & resize) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Buffers, Views & Endianness](#2-core-concepts-buffers-views--endianness)
3. [How It Works: Engine Memory Layout & BackingStore](#3-how-it-works-engine-memory-layout--backingstore)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: TypedArrays vs Standard Arrays](#8-performance-characteristics-typedarrays-vs-standard-arrays)
9. [Engine Internals: V8 JSTypedArray & Zero-Copy Slicing](#9-engine-internals-v8-jstypedarray--zero-copy-slicing)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Binary Telemetry Protocol Parser for High-Frequency WebSockets](#11-real-world-case-study-binary-telemetry-protocol-parser-for-high-frequency-websockets)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Standard JavaScript arrays are dynamic, high-level abstractions: they can grow or shrink dynamically, hold heterogeneous data types, and contain sparse holes. Under the hood, V8 optimizes them into various ElementsKinds (PACKED_SMI, HOLEY_DOUBLE, etc.).

However, when JavaScript expanded into WebGL 3D graphics, audio synthesis (Web Audio API), WebAssembly, networking (WebSockets/WebRTC), and file systems, engines needed a way to manipulate **raw, contiguous, fixed-size chunks of binary memory directly**:
- Standard arrays incur pointer dereferencing and GC tracking overhead.
- Passing a 1080p canvas image (8.3 million bytes) through standard arrays would consume over 100 MB of heap memory and crash the browser frame rate.

To deliver true systems-level binary throughput, ECMAScript standardized the **Binary Data Architecture**:
1. **`ArrayBuffer`:** A raw, fixed-length allocation of bytes.
2. **`TypedArray` Views:** Homogeneous typed index windows (`Uint8Array`, `Float32Array`, `BigInt64Array`).
3. **`DataView`:** A flexible, heterogeneous byte reader/writer that supports explicit endianness (little-endian vs big-endian).
4. **`SharedArrayBuffer` & `Atomics`:** Concurrency primitives enabling lock-free shared memory across worker threads.

---

## 2. Core Concepts: Buffers, Views & Endianness

### 2.1 The Buffer-View Architecture
An `ArrayBuffer` cannot be read or written directly. It represents raw memory. To interact with it, you must wrap it in a **View**:

```
+------------------------------------------------------------------------------------+
|                         ArrayBuffer (16 Raw Bytes)                                 |
| 0x00 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 | 0x07 | 0x08 | 0x09 | ... | 0x0F |
+------------------------------------------------------------------------------------+
                                         |
     +-----------------------------------+-----------------------------------+
     |                                                                       |
     v (Uint8Array view: 1 byte per element)                                 v (Int32Array view: 4 bytes per element)
+---------------------------------------------+                         +---------------------------------------------+
| [0]  | [1]  | [2]  | [3]  | [4]  | ... |[15]|                         |   Index [0]   |   Index [1]   |  ... [3]    |
+---------------------------------------------+                         +---------------------------------------------+
```

### 2.2 The Complete Family of TypedArrays
| TypedArray | Bytes / Elem | Value Range | Common Use Cases |
| :--- | :--- | :--- | :--- |
| `Int8Array` | 1 | $-128$ to $127$ | Audio samples, 8-bit sensor data |
| `Uint8Array` | 1 | $0$ to $255$ | File I/O, Node.js Buffers, network packets |
| `Uint8ClampedArray` | 1 | $0$ to $255$ (Clamped) | Canvas 2D image pixel processing (RGBA) |
| `Int16Array` | 2 | $-32,768$ to $32,767$ | 16-bit PCM audio |
| `Uint16Array` | 2 | $0$ to $65,535$ | UTF-16 strings, 16-bit indices |
| `Int32Array` | 4 | $-2^{31}$ to $2^{31}-1$ | Mathematical coordinates, integer IDs |
| `Uint32Array` | 4 | $0$ to $2^{32}-1$ | WebGL indices, 32-bit bitmasks |
| `Float32Array` | 4 | IEEE-754 Single Float | WebGL vertices, 3D matrices, neural weights |
| `Float64Array` | 8 | IEEE-754 Double Float | High-precision scientific computing |
| `BigInt64Array` | 8 | $-2^{63}$ to $2^{63}-1$ | 64-bit database keys, hardware timestamps |
| `BigUint64Array` | 8 | $0$ to $2^{64}-1$ | 64-bit memory addresses, crypto hashes |

### 2.3 Endianness (Byte Ordering)
When multi-byte numbers (such as 16-bit or 32-bit integers) are stored in memory:
- **Little-Endian:** The least significant byte is stored at the lowest address (Standard on x86, x86_64, and ARM processors).
- **Big-Endian (Network Byte Order):** The most significant byte is stored at the lowest address (Standard in TCP/IP network protocols).

`DataView` allows you to explicitly enforce endianness via a boolean flag `isLittleEndian`.

---

## 3. How It Works: Engine Memory Layout & BackingStore

In V8, TypedArrays separate the JavaScript wrapper object from the underlying memory:

```
  JavaScript Heap (V8 Managed GC)             Native C++ Heap (BackingStore)
+-----------------------------------+        +-----------------------------------+
|      v8::internal::JSTypedArray   |        |        v8::internal::BackingStore |
+-----------------------------------+        +-----------------------------------+
| Shape / Hidden Class              |        | Raw contiguous byte buffer        |
| length: 1024                      |        | Allocated via malloc/mmap         |
| byte_offset: 0                    | ------>| 0x7FFF8900: [00 00 80 3F ...]     |
| buffer: Pointer to JSArrayBuffer  |        +-----------------------------------+
+-----------------------------------+
```
1. The `BackingStore` is allocated in native C++ memory using system `malloc()` or virtual memory mapping.
2. In-place operations on TypedArrays map directly to raw CPU load/store instructions, bypassing JavaScript garbage collection overhead.

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Creating Buffers and Views
```javascript
// Allocate 16 bytes of zero-initialized memory:
const buffer = new ArrayBuffer(16);
console.log(buffer.byteLength); // 16

// Create TypedArray views:
const uint8 = new Uint8Array(buffer); // 16 elements
const int32 = new Int32Array(buffer);  // 4 elements (16 / 4)

uint8[0] = 0x12;
uint8[1] = 0x34;

// Subarray (Zero-copy slice sharing underlying memory):
const subView = uint8.subarray(0, 2);
console.log(subView.buffer === buffer); // true (Shared memory!)
```

### 4.2 Heterogeneous Access via `DataView`
```javascript
const dv = new DataView(buffer);

// Write 16-bit int at offset 0 as Big-Endian (false = Big-Endian):
dv.setInt16(0, 0x1234, false);

// Read back as Little-Endian (true = Little-Endian):
console.log(dv.getInt16(0, true).toString(16)); // "3412" (Bytes swapped!)
```

### 4.3 ES2024 Resizable ArrayBuffer & `transfer()`
Standardized in ECMAScript 2024, buffers can be resized or their memory ownership transferred to another thread/buffer without copying:
```javascript
// Resizable buffer:
const resizable = new ArrayBuffer(16, { maxByteLength: 1024 });
resizable.resize(64);
console.log(resizable.byteLength); // 64

// Detaching and transferring memory:
const transferred = resizable.transfer(128); // Old buffer is detached!
console.log(resizable.detached);    // true
console.log(transferred.byteLength); // 128
```

---

## 5. Practical Examples

### Example 1: Binary BMP Image Header Parser via `DataView`
```javascript
function parseBmpHeader(arrayBuffer) {
  const dv = new DataView(arrayBuffer);

  // BMP magic number "BM" (0x42, 0x4D):
  const magic = String.fromCharCode(dv.getUint8(0), dv.getUint8(1));
  if (magic !== "BM") throw new Error("Invalid BMP file");

  const fileSize = dv.getUint32(2, true); // Little-endian
  const pixelOffset = dv.getUint32(10, true);
  const width = dv.getInt32(18, true);
  const height = dv.getInt32(22, true);
  const bitsPerPixel = dv.getUint16(28, true);

  return { magic, fileSize, pixelOffset, width, height, bitsPerPixel };
}
```

### Example 2: Canvas Inversion using `Uint8ClampedArray`
`Uint8ClampedArray` automatically clamps numbers outside $[0, 255]$ rather than wrapping them around via modulo:
```javascript
function invertCanvasPixels(pixelArray) {
  // pixelArray is a Uint8ClampedArray representing [R, G, B, A, R, G, B, A, ...]
  for (let i = 0; i < pixelArray.length; i += 4) {
    pixelArray[i]     = 255 - pixelArray[i];     // Red
    pixelArray[i + 1] = 255 - pixelArray[i + 1]; // Green
    pixelArray[i + 2] = 255 - pixelArray[i + 2]; // Blue
    // pixelArray[i + 3] is Alpha (left untouched)
  }
}

// Clamping demonstration:
const clamped = new Uint8ClampedArray(2);
clamped[0] = 300;  // Automatically clamped to 255 (NOT 44!)
clamped[1] = -50;  // Automatically clamped to 0
console.log(clamped); // Uint8ClampedArray(2) [ 255, 0 ]
```

### Example 3: Shared Memory Concurrency with `SharedArrayBuffer` & `Atomics`
```javascript
// Shared across Worker Threads
const sharedBuffer = new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4);
const sharedArray = new Int32Array(sharedBuffer);

// Thread 1 (Producer):
Atomics.store(sharedArray, 0, 100);
Atomics.add(sharedArray, 0, 50); // Atomic addition

// Thread 2 (Consumer):
const currentValue = Atomics.load(sharedArray, 0);
console.log("Atomic value read:", currentValue); // 150

// Compare-And-Swap (CAS):
const old = Atomics.compareExchange(sharedArray, 0, 150, 200);
console.log("Updated to 200 via CAS. Previous:", old); // 150
```

### Example 4: Converting UTF-8 Strings to `Uint8Array` via `TextEncoder`
```javascript
const encoder = new TextEncoder(); // Encodes to UTF-8 Uint8Array
const decoder = new TextDecoder("utf-8");

const text = "Hello 🚀 World";
const bytes = encoder.encode(text);

console.log(bytes instanceof Uint8Array); // true
console.log("Byte length:", bytes.byteLength); // 16 bytes (Emoji took 4 bytes)
console.log("Decoded text:", decoder.decode(bytes)); // "Hello 🚀 World"
```

### Example 5: High-Speed WebGL Vector Buffer Layout
```javascript
// Pack 3D coordinates [x, y, z] and UV textures [u, v] into a single contiguous vertex buffer
const vertexCount = 3; // 1 triangle
const strideBytes = 5 * Float32Array.BYTES_PER_ELEMENT; // 20 bytes per vertex
const vertexBuffer = new Float32Array(vertexCount * 5);

// Vertex 0: (x, y, z, u, v)
vertexBuffer.set([-1.0, -1.0, 0.0, 0.0, 0.0], 0);
// Vertex 1:
vertexBuffer.set([ 1.0, -1.0, 0.0, 1.0, 0.0], 5);
// Vertex 2:
vertexBuffer.set([ 0.0,  1.0, 0.0, 0.5, 1.0], 10);

console.log("Total Vertex Buffer Bytes:", vertexBuffer.byteLength); // 60 bytes
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `slice()` Copies; `subarray()` Shares Memory
This is one of the most common sources of memory corruption in binary JavaScript programming:
```javascript
const orig = new Uint8Array([10, 20, 30]);

// slice() allocates a brand new ArrayBuffer:
const copy = orig.slice(0, 2);
copy[0] = 99;
console.log(orig[0]); // 10 (Original unaffected)

// subarray() creates a view over the SAME ArrayBuffer:
const view = orig.subarray(0, 2);
view[0] = 99;
console.log(orig[0]); // 99 (Original MUTATED!)
```

### 6.2 Modulo Wrapping vs Saturation Clamping
- `Uint8Array` performs bitwise modulo arithmetic ($x \pmod{256}$):
  `new Uint8Array([300])[0] === 44` ($300 - 256 = 44$).
- `Uint8ClampedArray` performs saturation clamping:
  `new Uint8ClampedArray([300])[0] === 255`.

### 6.3 Alignment Faults
When creating TypedArrays over an existing `ArrayBuffer`, the `byteOffset` argument **must be a multiple of the element's byte size**:
```javascript
const buf = new ArrayBuffer(16);
// Throws RangeError: byte offset must be a multiple of 4:
// const invalid = new Int32Array(buf, 3);

// Valid alignment:
const valid = new Int32Array(buf, 4);
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `slice()` when sharing is intended** | `const chunk = buf.slice(0, 10);` | `const chunk = buf.subarray(0, 10);` | `slice()` allocates a new buffer and copies bytes; `subarray()` is zero-copy. |
| **Assuming Big-Endian on TypedArrays** | Reading 32-bit ints directly from network | Use `DataView.prototype.getInt32(offset, false)` | Native TypedArray indexing always uses the host CPU's endianness (Little-Endian). |
| **Storing non-integers in Int arrays** | `int32Array[0] = 3.99;` | `float32Array[0] = 3.99;` | `Int32Array` silently truncates fractional numbers (`3.99` becomes `3`). |
| **Manual UTF-8 String Encoding** | Custom byte-shifting loops | Use native `TextEncoder` / `TextDecoder` | Standard built-ins are compiled to hardware SIMD instructions. |
| **Unclamped Canvas Image Math** | Manipulating pixel arrays with `Uint8Array` | Use `Uint8ClampedArray` | Values $> 255$ wrap to $0$, causing inverted/corrupted color artifacts. |
| **Sharing memory without Atomics** | `sharedArr[0]++` across workers | `Atomics.add(sharedArr, 0, 1)` | Non-atomic writes cause memory tear data races. |

---

## 8. Performance Characteristics: TypedArrays vs Standard Arrays

1. **Memory Density:**
   - A standard JavaScript array of 1,000,000 integers takes $\approx 8\text{ MB}$ to $16\text{ MB}$ of V8 heap memory (due to object headers, capacity padding, and pointer tags).
   - A `Uint8Array(1_000_000)` occupies **exactly 1,000,000 bytes (~0.95 MB)** in contiguous physical RAM.
2. **SIMD & CPU Vectorization:**
   - Because TypedArrays guarantee contiguous physical memory with identical data types, modern JIT engines (TurboFan) auto-vectorize loops into 128-bit/256-bit SIMD instructions (AVX2/NEON), processing 8–16 values per CPU clock cycle.
3. **Zero Garbage Collection:**
   - Mutating elements in a TypedArray creates zero garbage collection churn.

---

## 9. Engine Internals: V8 JSTypedArray & Zero-Copy Slicing

Inside V8, TypedArray instances are represented by `v8::internal::JSTypedArray`:

```
                  +-----------------------------------+
                  |   v8::internal::JSTypedArray      |
                  +-----------------------------------+
                  | - Shape / Map pointer             |
                  | - elements: FixedTypedArrayBase*  |
                  | - buffer: JSArrayBuffer*          |
                  | - byte_offset: size_t             |
                  | - byte_length: size_t             |
                  | - data_ptr: void*                 | ---> Direct hardware pointer!
                  +-----------------------------------+
```

### JIT Fast Path:
When TurboFan compiles `uint8[i] = val`:
1. It computes the address as `data_ptr + i`.
2. It emits a single assembly instruction: `MOV byte ptr [rax + rbx], cl`.
3. It bypasses all property descriptors, prototypes, and dynamic method dispatches.

---

## 10. Best Practices & Design Guidelines

1. **Use `DataView` for Binary Network Protocols:** Always use `DataView` when decoding binary packets (like WebSocket binary frames or file formats) to explicitly handle endianness.
2. **Use `subarray()` for Windowing:** When parsing streaming packets, use `.subarray(offset, length)` to create zero-copy packet slices without memory allocations.
3. **Use `TextEncoder`/`TextDecoder`:** Never implement custom UTF-8 decoding algorithms; rely on the native browser/Node APIs.
4. **Prefer `ArrayBuffer.prototype.transfer()` (ES2024):** When transferring buffers between workers, transfer ownership to prevent accidental shared mutation and memory bloat.

---

## 11. Real-World Case Study: Binary Telemetry Protocol Parser for High-Frequency WebSockets

### Problem
A financial trading terminal received 30,000 market quote updates per second over a WebSocket. The original backend sent JSON messages (`{"ticker": "AAPL", "bid": 150.25, "ask": 150.30, "volume": 5000}`). JSON deserialization consumed 75% of main-thread CPU time, causing UI lag.

### Solution
Migrate the WebSocket protocol to a compact 16-byte fixed binary packet parsed via `DataView`:

```javascript
// Compact 16-byte Binary Quote Packet Layout:
// Offset 0 (4 bytes):  Ticker symbol (4 ASCII characters, e.g. "AAPL")
// Offset 4 (4 bytes):  Bid Price (Float32, Little-Endian)
// Offset 8 (4 bytes):  Ask Price (Float32, Little-Endian)
// Offset 12 (4 bytes): Volume (Uint32, Little-Endian)

class MarketDataParser {
  static parseQuote(arrayBuffer) {
    const dv = new DataView(arrayBuffer);

    // Read 4-character ASCII ticker:
    const ticker = String.fromCharCode(
      dv.getUint8(0),
      dv.getUint8(1),
      dv.getUint8(2),
      dv.getUint8(3)
    ).trim();

    const bid = dv.getFloat32(4, true);
    const ask = dv.getFloat32(8, true);
    const volume = dv.getUint32(12, true);

    return { ticker, bid, ask, volume };
  }

  static serializeQuote(ticker, bid, ask, volume) {
    const buffer = new ArrayBuffer(16);
    const dv = new DataView(buffer);

    // Write Ticker:
    for (let i = 0; i < 4; i++) {
      dv.setUint8(i, ticker.charCodeAt(i) || 32); // Space padded
    }

    dv.setFloat32(4, bid, true);
    dv.setFloat32(8, ask, true);
    dv.setUint32(12, volume, true);

    return buffer;
  }
}

// Verification:
const packetBuffer = MarketDataParser.serializeQuote("AAPL", 150.25, 150.30, 50000);
const parsed = MarketDataParser.parseQuote(packetBuffer);

console.log("Parsed Binary Quote:", parsed);
// Output: { ticker: 'AAPL', bid: 150.25, ask: 150.3000030517578, volume: 50000 }
// Result: 85% reduction in CPU utilization; 70% reduction in network bandwidth!
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the exact difference between an <code>ArrayBuffer</code> and a <code>TypedArray</code>?</strong></summary>

An `ArrayBuffer` represents the physical allocation of raw bytes in memory; it cannot be indexed or manipulated directly. A `TypedArray` is a typed view (lens) that wraps an `ArrayBuffer`, providing indexed read and write access to elements interpreted as specific data types (`Int32`, `Float64`, etc.). Multiple TypedArrays can wrap the same underlying `ArrayBuffer`.
</details>

<details>
<summary><strong>Q2: Why does <code>Uint8Array.prototype.subarray()</code> not allocate new memory, while <code>slice()</code> does?</strong></summary>

`subarray(start, end)` returns a new TypedArray view referencing the **exact same underlying `ArrayBuffer`** with an adjusted `byteOffset` and `byteLength`. `slice(start, end)` allocates a completely new `ArrayBuffer` and copies the byte contents from the source buffer.
</details>

<details>
<summary><strong>Q3: What makes <code>Uint8ClampedArray</code> unique compared to standard <code>Uint8Array</code>?</strong></summary>

When an out-of-range value ($< 0$ or $> 255$) is assigned to `Uint8Array`, it performs modulo wrapping ($300 \pmod{256} = 44$). In contrast, `Uint8ClampedArray` performs **saturation clamping**: numbers $> 255$ become $255$, and numbers $< 0$ become $0$. This is required by HTML5 Canvas pixel rendering to prevent visual color inversion bugs.
</details>

<details>
<summary><strong>Q4: Why is <code>DataView</code> necessary if TypedArrays already exist?</strong></summary>

TypedArrays use the native CPU architecture's endianness (almost always little-endian) and require elements to be homogeneous. `DataView` allows heterogeneous access (e.g., reading a 1-byte flag, followed by a 4-byte integer, followed by a float) and provides explicit control over endianness per method call, making it indispensable for network packet and file format decoding.
</details>

<details>
<summary><strong>Q5: What are the security restrictions on <code>SharedArrayBuffer</code> in modern web browsers?</strong></summary>

Following the discovery of the Spectre CPU vulnerability (which exploited high-precision timers and shared memory), browsers require that sites using `SharedArrayBuffer` be served in a **cross-origin isolated environment**. The server must send two HTTP response headers:
1. `Cross-Origin-Opener-Policy: same-origin`
2. `Cross-Origin-Embedder-Policy: require-corp`
</details>

<details>
<summary><strong>Q6: What is <code>ArrayBuffer.prototype.transfer()</code> introduced in ES2024?</strong></summary>

`transfer(newByteLength)` detaches the source `ArrayBuffer` (setting its `byteLength` to 0 and making it unusable) and transfers its underlying memory allocation to a newly created `ArrayBuffer`. This enables zero-copy buffer resizing and safe memory ownership transfer between threads without duplicating heap memory.
</details>

<details>
<summary><strong>Q7: Why does a TypedArray loop execute faster in V8 than a standard Array loop?</strong></summary>

TypedArrays are contiguous buffers of identical binary elements. TurboFan JIT does not need to perform type feedback checks, prototype chain lookups, or bound checks on every iteration. It computes memory addresses using direct pointer arithmetic (`data_ptr + index * stride`) and can auto-vectorize operations into CPU SIMD registers.
</details>

<details>
<summary><strong>Q8: What happens when you attempt to create a 32-bit TypedArray with an odd byte offset?</strong></summary>

The engine throws a `RangeError`. By ECMAScript specification, the `byteOffset` of a TypedArray view over an `ArrayBuffer` must be aligned to a multiple of its `BYTES_PER_ELEMENT` (e.g., 4 bytes for `Int32Array`, 8 bytes for `Float64Array`).
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Endianness Detection Utility
Write a function `getHostEndianness()` that programmatically detects whether the runtime system CPU is little-endian or big-endian.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function getHostEndianness() {
  const buffer = new ArrayBuffer(2);
  const uint16 = new Uint16Array(buffer);
  const uint8 = new Uint8Array(buffer);

  uint16[0] = 0xAAFF; // Storing multi-byte integer

  // In Little-Endian, least significant byte (0xFF) is stored first at index 0:
  if (uint8[0] === 0xFF) {
    return "LITTLE_ENDIAN";
  }
  return "BIG_ENDIAN";
}

console.log("Host Architecture:", getHostEndianness()); // "LITTLE_ENDIAN" (on x86 / ARM)
```
</details>

### Exercise 2: Fast Concatenation of Multiple Uint8Arrays
Implement a function `concatUint8Arrays(arrays)` that takes an array of `Uint8Array` chunks and merges them into a single contiguous `Uint8Array` in $O(n)$ time with a single allocation.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function concatUint8Arrays(arrays) {
  const totalLength = arrays.reduce((acc, arr) => acc + arr.length, 0);
  const result = new Uint8Array(totalLength);
  
  let offset = 0;
  for (const arr of arrays) {
    result.set(arr, offset); // Low-level memcpy under the hood
    offset += arr.length;
  }
  
  return result;
}

const chunk1 = new Uint8Array([1, 2, 3]);
const chunk2 = new Uint8Array([4, 5]);
const merged = concatUint8Arrays([chunk1, chunk2]);
console.log(merged); // Uint8Array(5) [ 1, 2, 3, 4, 5 ]
```
</details>

### Exercise 3: Hex String to Uint8Array & Vice Versa
Implement high-speed binary converters `hexToBytes(hexString)` and `bytesToHex(uint8Array)`.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function hexToBytes(hex) {
  const cleanHex = hex.startsWith("0x") ? hex.slice(2) : hex;
  const bytes = new Uint8Array(cleanHex.length / 2);
  for (let i = 0; i < cleanHex.length; i += 2) {
    bytes[i / 2] = parseInt(cleanHex.substr(i, 2), 16);
  }
  return bytes;
}

function bytesToHex(bytes) {
  return Array.from(bytes)
    .map(b => b.toString(16).padStart(2, "0"))
    .join("");
}

const originalHex = "deadbeef42";
const bytes = hexToBytes(originalHex);
console.log(bytes); // Uint8Array(5) [ 222, 173, 190, 239, 66 ]
console.log(bytesToHex(bytes)); // "deadbeef42"
```
</details>
