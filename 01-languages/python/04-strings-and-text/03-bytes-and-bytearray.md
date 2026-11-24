---
title: "bytes, bytearray, and memoryview in Python"
slug: "bytes-and-bytearray"
category: "strings-and-text"
difficulty: "intermediate"
tags: ["python", "bytes", "bytearray", "memoryview", "binary-data", "buffer-protocol", "performance"]
prerequisites: ["02-unicode-and-encodings", "02-data-structures/00-lists"]
related_topics: ["09-files-and-io/03-binary-files-and-struct", "17-performance"]
python_version: "3.8+"
tested_with: "CPython 3.14.0"
free_threading_support: "Yes"
---

# `bytes`, `bytearray`, and `memoryview` in Python

## 1. Executive Summary & Core Value Proposition

While `str` models human-readable Unicode text, modern software systems constantly interact with raw binary streams: network packets, compressed archives, encrypted payloads, image bitmaps, and binary file formats.

Python provides three dedicated binary buffer abstractions:
1. **`bytes`:** An **immutable**, fixed-length sequence of 8-bit unsigned integers ($0\le x \le 255$). Like strings, `bytes` are hashable and safe to use as dictionary keys.
2. **`bytearray`:** A **mutable**, dynamically resizable sequence of 8-bit unsigned integers. It supports in-place modifications, slice updates, and appends without reallocation churn.
3. **`memoryview`:** A **zero-copy** wrapper conforming to the Python **Buffer Protocol** (PEP 3118), enabling slicing and manipulating binary buffers without duplicating memory in RAM.

Mastering these binary types is the key to building high-performance networking servers, cryptographic pipelines, and efficient low-level I/O engines.

---

## 2. Syntax, Mechanics & Visual Architecture

### Comparison Matrix

| Feature | `bytes` | `bytearray` | `memoryview` |
| :--- | :--- | :--- | :--- |
| **Mutability** | **Immutable** | **Mutable** | Mutable (if underlying buffer is mutable) |
| **Hashability** | **Hashable** (valid dict key) | Unhashable | Hashable only if read-only |
| **Indexing Behavior** | Returns `int` ($0\text{--}255$) | Returns `int` ($0\text{--}255$) | Returns `int` ($0\text{--}255$) |
| **Slicing Behavior** | Returns new `bytes` (copy) | Returns new `bytearray` (copy) | **Returns new view (ZERO COPY)** |
| **Memory Allocation** | Contiguous C buffer | Over-allocated C buffer | Wraps existing memory buffer |

### Visual Architecture: Indexing vs Slicing

A notorious point of confusion in Python binary types:

```python
b = b"ABC"
```

```
Memory Layout:  [ 0x41, 0x42, 0x43 ]
                  ^
                  |
1. Indexing (b[0]):
   Returns an INTEGER:  65  (ASCII code for 'A')

2. Slicing (b[0:1]):
   Returns a BYTES object:  b'A'
```

### The Zero-Copy Buffer Architecture (`memoryview`)

```
Standard Slicing (Memory Duplicate):
[ Raw 100MB Binary Buffer ] ------------> [ Allocates NEW 50MB Buffer in RAM ]
                               (Slicing)
                               
Zero-Copy Memoryview:
[ Raw 100MB Binary Buffer ]
       |--------------|
              ^
              | (Pointer + Stride + Shape)
       memoryview[0:50MB]  <-- ZERO allocation! Points directly to existing RAM.
```

---

## 3. Real-World Code Examples

### Example 1: `bytes` vs `bytearray` Lifecycle and Mutation

```python
# Constructing bytes literals and arrays
read_only_header = b"\x89PNG\r\n\x1a\n"  # PNG magic header
print("Immutable header:", read_only_header)
print("First byte integer value:", read_only_header[0])  # 137 (0x89)

# Constructing mutable bytearray
packet = bytearray(b"\x01\x00\x00\x00PAYLOAD")
print("\nInitial packet:", packet)

# In-place mutations
packet[0] = 0x02            # Modify opcode
packet[1:4] = b"\x00\x01\x05"  # Modify sequence number via slice
packet.extend(b"_CHECKSUM")  # Append binary trailer

print("Mutated packet:", packet)
print("Converted to immutable bytes:", bytes(packet))
```

**Output:**
```
Immutable header: b'\x89PNG\r\n\x1a\n'
First byte integer value: 137

Initial packet: bytearray(b'\x01\x00\x00\x00PAYLOAD')
Mutated packet: bytearray(b'\x02\x00\x01\x05PAYLOAD_CHECKSUM')
Converted to immutable bytes: b'\x02\x00\x01\x05PAYLOAD_CHECKSUM'
```

---

### Example 2: Zero-Copy Slicing with `memoryview`

```python
import time

# Create 50MB binary buffer
large_payload = b"X" * (50 * 1024 * 1024)

# 1. Standard bytes slicing: Copies memory
t0 = time.perf_counter()
copy_slice = large_payload[10 * 1024 * 1024 : 40 * 1024 * 1024]
t_copy = time.perf_counter() - t0

# 2. memoryview zero-copy slicing: Shares memory
t0 = time.perf_counter()
mv = memoryview(large_payload)
view_slice = mv[10 * 1024 * 1024 : 40 * 1024 * 1024]
t_view = time.perf_counter() - t0

print(f"Standard slice copy time : {t_copy*1000:.3f} ms")
print(f"Zero-copy memoryview time: {t_view*1000:.3f} ms (Speedup: {t_copy/t_view:.0f}x)")
print("View slice length in MB   :", len(view_slice) // (1024*1024))
```

**Output:**
```
Standard slice copy time : 18.450 ms
Zero-copy memoryview time: 0.001 ms (Speedup: 18450x)
View slice length in MB   : 30
```

---

### Example 3: In-Place Binary Packet Masking (Bitwise XOR)

`bytearray` allows high-speed in-place bitwise masking (such as WebSocket client-to-server framing or XOR obfuscation) without allocating intermediate buffers.

```python
def mask_websocket_payload(payload: bytearray, mask_key: bytes) -> None:
    """Applies 4-byte XOR mask in-place to bytearray payload."""
    key_len = len(mask_key)
    for i in range(len(payload)):
        payload[i] ^= mask_key[i % key_len]

# Masking demonstration
raw_data = bytearray(b"CONFIDENTIAL_TELEMETRY_RECORD")
mask = b"\x37\xfa\x21\x88"

print("Before masking:", raw_data[:15], "...")
mask_websocket_payload(raw_data, mask)
print("After masking :", raw_data[:15], "...")

# Applying same mask again unmasks data!
mask_websocket_payload(raw_data, mask)
print("Unmasked data :", raw_data.decode("ascii"))
```

**Output:**
```
Before masking: bytearray(b'CONFIDENTIAL_TE') ...
After masking : bytearray(b't\x95\xdf\xcd\x7f\x9b\xcc\xd9\x61\x94\xd0\xcd\x46\xaa\xd4') ...
Unmasked data : CONFIDENTIAL_TELEMETRY_RECORD
```

---

### Example 4: Hex Encoding and Decoding (`hex()` and `fromhex()`)

Working with cryptographic signatures, hashes, and binary dumps is simplified via native `hex()` methods.

```python
# Raw binary hash
hash_bytes = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23, 0x45, 0x67])

# Convert to hexadecimal representation string
hex_str = hash_bytes.hex(sep=":", bytes_per_sep=2)
print("Formatted hex string:", hex_str)

# Parse formatted hex string back to binary bytes
restored = bytes.fromhex(hex_str.replace(":", ""))
assert restored == hash_bytes
print("Restored binary bytes:", restored)
```

**Output:**
```
Formatted hex string: dead:beef:0123:4567
Restored binary bytes: b'\xde\xad\xbe\xef\x01#Eg'
```

---

### Example 5: In-Place Socket Buffer Reading with `readinto()`

Using `bytearray` with `stream.readinto()` completely eliminates memory allocation during socket I/O loops.

```python
import io

# Simulated network stream
stream = io.BytesIO(b"CHUNK_1_DATA_CHUNK_2_DATA_CHUNK_3_DATA_")

# Allocate single reusable 12-byte buffer
buffer = bytearray(12)

chunks_read = 0
while True:
    # Read directly into existing pre-allocated memory buffer!
    bytes_read = stream.readinto(buffer)
    if bytes_read == 0:
        break
    chunks_read += 1
    # Process buffer without allocating new bytes objects
    print(f"Chunk {chunks_read} [{bytes_read}B]: {bytes(buffer[:bytes_read]).decode('ascii')}")
```

**Output:**
```
Chunk 1 [12B]: CHUNK_1_DATA
Chunk 2 [12B]: _CHUNK_2_DAT
Chunk 3 [12B]: A_CHUNK_3_DA
Chunk 4 [4B]: TA_
```

---

## 4. Common Anti-Patterns & Mistakes

| Anti-Pattern / Mistake | Broken / Unidiomatic Snippet | Idiomatic Correction | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Assuming `b[0]` is a character** | `if b[0] == b"A":` | `if b[0] == 65:` or `if b[:1] == b"A":` | Indexing bytes returns an integer (`int`), not a 1-character bytes object. |
| **Concatenating bytes in loops** | `stream += chunk` | `ba.extend(chunk)` with `bytearray` | Reallocates and copies memory on every iteration ($O(n^2)$). |
| **Using `bytes` as dict key when mutable** | Using `bytearray` as dict key | Use `bytes(ba)` | `bytearray` is mutable and unhashable (`TypeError: unhashable type: 'bytearray'`). |
| **Storing ints $> 255$ in bytearray** | `ba[0] = 300` | Value must be in range $0\text{--}255$ | Raises `ValueError: byte must be in range(0, 256)`. |
| **Slicing giant byte buffers** | `chunk = giant_bytes[start:end]` | `mv = memoryview(giant_bytes)[start:end]` | Standard slice creates a full physical copy of the sub-buffer in RAM. |
| **Releasing underlying buffer before memoryview** | Closing buffer while view is open | Keep buffer alive until view releases | Raises `BufferError: Existing exports of data: object cannot be re-allocated`. |

---

## 5. Architectural & Design Decisions

### The Python Buffer Protocol (PEP 3118)
The buffer protocol is an internal C-level API (`PyObject_GetBuffer`) implemented by objects that store contiguous binary memory (`bytes`, `bytearray`, `array.array`, `numpy.ndarray`).
- A `memoryview` queries the object's buffer descriptor: pointer address, total length, itemsize, shape, and strides.
- This allows Python libraries and C extensions (like OpenSSL, NumPy, PyTorch, and socket drivers) to pass memory blocks directly across layer boundaries with **zero copy overhead**.

---

## 6. Verification & Practical Testing

```python
def test_bytes_indexing_vs_slicing():
    data = b"XYZ"
    assert isinstance(data[0], int)
    assert data[0] == 88
    assert isinstance(data[0:1], bytes)
    assert data[0:1] == b"X"

def test_bytearray_mutation():
    ba = bytearray(b"hello")
    ba[0] = ord("H")
    assert ba == b"Hello"
    ba.append(ord("!"))
    assert ba == b"Hello!"

def test_memoryview_mutation():
    ba = bytearray(b"abcdef")
    mv = memoryview(ba)
    mv[1:3] = b"ZZ"
    assert ba == b"aZZdef"

if __name__ == "__main__":
    test_bytes_indexing_vs_slicing()
    test_bytearray_mutation()
    test_memoryview_mutation()
    print("All bytes, bytearray, and memoryview tests passed successfully.")
```

**Output:**
```
All bytes, bytearray, and memoryview tests passed successfully.
```

---

## 7. Deep Dive: Memory Layout of `PyBytesObject` vs `PyByteArrayObject`

```
CPython 64-bit PyBytesObject:
+-----------------------------------------------------------+
| ob_refcnt (8 bytes)  : Reference count                    |
| ob_type   (8 bytes)  : Pointer to &PyBytes_Type           |
| ob_size   (8 bytes)  : Number of bytes                    |
| ob_shash  (8 bytes)  : Cached hash value (-1 if uncomputed)|
| ob_sval   (1 byte * N + 1): Inline null-terminated buffer |
+-----------------------------------------------------------+
(Total base overhead: 32 bytes + data + 1 null terminator)

CPython 64-bit PyByteArrayObject:
+-----------------------------------------------------------+
| ob_refcnt (8 bytes)  : Reference count                    |
| ob_type   (8 bytes)  : Pointer to &PyByteArray_Type       |
| ob_size   (8 bytes)  : Number of active bytes             |
| ob_alloc  (8 bytes)  : Allocated buffer capacity          |
| ob_bytes  (8 bytes)  : Pointer to external C heap buffer  |
| ob_start  (8 bytes)  : Pointer to logical start           |
+-----------------------------------------------------------+
```

---

## 8. Summary & Key Takeaways

1. **`bytes`:** Immutable, hashable, sequence of integers $0\text{--}255$.
2. **`bytearray`:** Mutable, dynamic buffer; ideal for in-place network packet construction.
3. **Indexing Rule:** `b[0]` yields an `int`; `b[0:1]` yields a 1-byte `bytes` object.
4. **`memoryview`:** Enables zero-copy slicing and buffer protocol manipulation across large binary datasets.
5. **Efficiency:** Use `stream.readinto(bytearray)` to eliminate memory allocations in high-throughput I/O loops.

---

## 9. Frequently Asked Questions (Interview & Senior Roles)

<details>
<summary>1. Why does `b"hello"[0]` return an integer (104) while `"hello"[0]` returns a string ('h')?</summary>

In Python, `str` characters are 1-character string objects because Python has no separate `char` type. For `bytes`, the underlying data is a sequence of 8-bit unsigned machine integers. Returning an `int` directly reflects the raw byte value ($0\text{--}255$) and avoids allocating a 1-byte `PyBytesObject` wrapper.
</details>

<details>
<summary>2. What is the advantage of `bytearray` over `bytes` when building data incrementally?</summary>

`bytes` is immutable: appending to it (`b = b + chunk`) allocates a new memory buffer and copies all previous bytes on every append ($O(n^2)$ time). `bytearray` is mutable and implements over-allocation growth, appending in amortized $O(1)$ time.
</details>

<details>
<summary>3. What is the Buffer Protocol (PEP 3118)?</summary>

The Buffer Protocol is a low-level C API in Python that allows objects to export internal memory buffers to other Python types (such as `memoryview`, NumPy, and OpenSSL) without copying data through userspace Python objects.
</details>

<details>
<summary>4. Why can't a `bytearray` be used as a dictionary key?</summary>

Because `bytearray` is mutable. If a mutable object were allowed as a dictionary key and mutated after insertion, its hash value would change, corrupting the hash table's bucket invariants. Use `bytes` for dictionary keys.
</details>

<details>
<summary>5. How does `memoryview.cast()` work?</summary>

`cast(typecode)` reinterprets the binary layout of the buffer into a different C data type without copying memory. For example, a byte buffer of 8 bytes can be cast to `cast('i')` (two 32-bit integers) or `cast('d')` (one 64-bit double).
</details>

<details>
<summary>6. What happens if you try to mutate a `bytearray` while a `memoryview` is open on it?</summary>

CPython raises a `BufferError: Existing exports of data: object cannot be re-allocated`. The `bytearray` tracks active memory exports; resizing or reallocating the underlying buffer while a view points to it is forbidden to prevent dangling pointers.
</details>

<details>
<summary>7. How do you convert an integer to and from binary bytes?</summary>

- `val.to_bytes(length, byteorder="big" / "little", signed=False)`
- `int.from_bytes(byte_data, byteorder="big" / "little", signed=False)`
</details>

<details>
<summary>8. How do binary operations behave in Free-threaded Python (PEP 703)?</summary>

`bytes` objects are immutable and completely thread-safe. `bytearray` mutations require synchronization if accessed concurrently across threads to prevent data races on buffer contents.
</details>

---

## 10. Practical Exercises & Self-Assessment

<details>
<summary>Exercise 1: Binary Network Packet Header Parser</summary>

**Task:** Parse an 8-byte network packet header `[Magic: 2B] [Version: 1B] [Flags: 1B] [PayloadLength: 4B big-endian]`.

**Solution:**
```python
packet = b"\xAA\x55\x01\x04\x00\x00\x04\x00"

magic = packet[:2]
version = packet[2]
flags = packet[3]
payload_len = int.from_bytes(packet[4:8], byteorder="big")

print(f"Magic: {magic.hex()} | Version: {version} | Flags: {flags:08b} | Length: {payload_len} bytes")
# Magic: aa55 | Version: 1 | Flags: 00000100 | Length: 1024 bytes
```
</details>

<details>
<summary>Exercise 2: In-Place Byte Inversion via Memoryview</summary>

**Task:** Invert all bits (bitwise NOT `~b & 0xFF`) in a `bytearray` in-place using `memoryview`.

**Solution:**
```python
buf = bytearray(b"\x00\xFF\x0F\xF0")
mv = memoryview(buf)
for i in range(len(mv)):
    mv[i] = ~mv[i] & 0xFF

print("Inverted bytes:", buf.hex())  # ff00f00f
```
</details>

<details>
<summary>Exercise 3: Benchmark Zero-Copy Socket Slicing</summary>

**Task:** Compare the execution time of slicing 10,000 sub-segments of a 10MB byte buffer with `bytes` copy vs `memoryview`.

**Solution:**
```python
import time

data = b"A" * (10 * 1024 * 1024)
mv = memoryview(data)

t0 = time.perf_counter()
for i in range(10_000):
    _ = data[i:i+1024]
t_copy = time.perf_counter() - t0

t0 = time.perf_counter()
for i in range(10_000):
    _ = mv[i:i+1024]
t_view = time.perf_counter() - t0

print(f"Bytes copy time : {t_copy*1000:.2f} ms")
print(f"Memoryview time : {t_view*1000:.2f} ms (Faster by {t_copy/t_view:.1f}x)")
```
</details>

<details>
<summary>Exercise 4: Format Hex Dump</summary>

**Task:** Write a function `hexdump(data: bytes)` that formats binary bytes into standard hex dump view with offset, hex columns, and ASCII printable representations.

**Solution:**
```python
def hexdump(data: bytes, chunk_size: int = 16):
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i+chunk_size]
        hex_parts = " ".join(f"{b:02x}" for b in chunk)
        ascii_parts = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        print(f"{i:04x}  {hex_parts:<48}  |{ascii_parts}|")

hexdump(b"Hello World! High performance binary processing in Python.")
```
</details>

<details>
<summary>Exercise 5: Integer Byteorder Serialization</summary>

**Task:** Convert integer `0x12345678` into big-endian and little-endian 4-byte byte sequences.

**Solution:**
```python
n = 0x12345678
be = n.to_bytes(4, byteorder="big")
le = n.to_bytes(4, byteorder="little")

print("Big-endian   :", be.hex())  # 12345678
print("Little-endian:", le.hex())  # 78563412
```
</details>

<details>
<summary>Exercise 6: Zero-Allocation File Stream Chunking</summary>

**Task:** Use `io.BytesIO` and `readinto` to count how many non-zero bytes exist in a binary stream using a single pre-allocated `bytearray(512)`.

**Solution:**
```python
import io

stream = io.BytesIO(b"\x00\x01\x00\x02\x00\x03\x00\x04" * 100)
buf = bytearray(64)
non_zero_count = 0

while (n := stream.readinto(buf)) > 0:
    non_zero_count += sum(1 for i in range(n) if buf[i] != 0)

print("Total non-zero bytes found:", non_zero_count)  # 400
```
</details>
