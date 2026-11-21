# Classic I/O Streams, Readers, and Writers

> The foundational I/O architecture of Java: byte streams vs character streams, buffering mechanics, decorator composition, and charset encoding.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [06-exceptions-and-debugging/03_try-with-resources.md](../06-exceptions-and-debugging/03_try-with-resources.md), [03-strings-and-text/07_unicode-and-encoding.md](../03-strings-and-text/07_unicode-and-encoding.md) |
| **Time to Read** | 24 minutes |
| **Status** | Active |
| **Java Version** | Java 1.0+ (UTF-8 default Java 18+ JEP 400) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Input and output (I/O) operations allow a program to communicate with external data sources and sinks: disk files, network sockets, memory buffers, and hardware peripherals. In Java, classic I/O (`java.io`) models all data transfer as a continuous **stream of data**.

The Java I/O framework is architecturally divided into two distinct hierarchies:
1. **Byte Streams (`InputStream` / `OutputStream`)**: Process raw 8-bit binary bytes. Ideal for images, audio, compiled classes, and binary protocols.
2. **Character Streams (`Reader` / `Writer`)**: Process 16-bit Unicode characters. Handle text files, internationalization, and character encoding/decoding.

In legacy Java versions, character streams defaulted to the host operating system's platform encoding (e.g. `Windows-1252` on English Windows, `Shift_JIS` on Japanese Windows), causing silent data corruption when files moved across operating systems. In modern Java (**Java 18+ JEP 400**), the default charset is standardized across all platforms to **UTF-8**.

---

## 2. Core Concepts

### The Four Root Abstract Classes
```
                        java.io Root Hierarchies
                                   |
         +-------------------------+-------------------------+
         |                                                   |
Byte Streams (8-bit bytes)                       Character Streams (16-bit chars)
   |-- InputStream (read bytes)                     |-- Reader (read chars)
   |-- OutputStream (write bytes)                   |-- Writer (write chars)
```

### Decorator Pattern Architecture
`java.io` makes heavy use of the **Decorator Pattern**. Primitive streams provide raw data access, while wrapper streams add buffering, filtering, data conversion, or compression by wrapping an existing stream:

```java
// Layered Decorator Composition:
InputStream raw = new FileInputStream("data.gz");        // 1. Raw disk access
InputStream buffered = new BufferedInputStream(raw);     // 2. High-speed 8KB memory buffer
InputStream decompress = new GZIPInputStream(buffered); // 3. Decompression filter
Reader reader = new InputStreamReader(decompress, StandardCharsets.UTF_8); // 4. Char decoding
BufferedReader lineReader = new BufferedReader(reader);  // 5. Line-buffering (readLine)
```

### Buffering Mechanics
Reading or writing one byte at a time directly to a disk or network device triggers an expensive OS kernel context switch for every byte.
- `BufferedInputStream` and `BufferedReader` maintain an internal memory buffer (default 8192 bytes = 8 KB).
- Read operations fetch an entire 8KB block from the OS in a single system call (`read(2)`), serving subsequent reads directly from fast RAM.
- `BufferedOutputStream` and `BufferedWriter` accumulate writes in memory and flush to the OS only when full or when `.flush()` is explicitly called.

---

## 3. How It Works Internally

### Byte to Character Decoding (`InputStreamReader`)
Inside `InputStreamReader`, bytes read from the underlying `InputStream` are converted into 16-bit Java `char`s using a `java.nio.charset.CharsetDecoder`:
```
Disk / Socket (Raw Bytes: [0xE2, 0x82, 0xAC])
                 |
                 v InputStream.read()
         [ 0xE2, 0x82, 0xAC ]
                 |
                 v CharsetDecoder (UTF-8)
         [ Unicode: \u20AC ('€') ]
                 |
                 v Reader.read()
Java char variable: '€'
```

### The Return Value `-1` Sentinel
The fundamental `read()` methods return an `int`, not a `byte`:
```java
int b = in.read();
```
- If data is available, it returns an unsigned byte value ($0$ to $255$).
- If the end of the stream (EOF) has been reached, it returns **`-1`**.
Returning an `int` allows distinguishing between the valid data byte `0xFF` (which represents decimal 255) and the EOF indicator `-1`.

---

## 4. Syntax & Usage

### 1. High-Performance Buffered Text Reading
```java
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class BufferedReadDemo {
    public static void main(String[] args) {
        // Always wrap in try-with-resources!
        try (BufferedReader reader = new BufferedReader(
                new FileReader("notes.txt", StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println(line);
            }
        } catch (IOException e) {
            System.err.println("I/O Error: " + e.getMessage());
        }
    }
}
```

### 2. High-Performance Buffered Binary Copying
```java
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public class BinaryCopyDemo {
    public static void copyFile(String src, String dest) throws IOException {
        try (var in = new BufferedInputStream(new FileInputStream(src));
             var out = new BufferedOutputStream(new FileOutputStream(dest))) {

            // In Java 9+: in.transferTo(out) copies all bytes efficiently!
            long bytesCopied = in.transferTo(out);
            System.out.println("Transferred: " + bytesCopied + " bytes");
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: In-Memory Byte Buffers with `ByteArrayOutputStream` (Basic)
```java
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.util.Arrays;

public class MemoryStreamDemo {
    public static void main(String[] args) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try (DataOutputStream dos = new DataOutputStream(baos)) {
            dos.writeInt(42);
            dos.writeDouble(3.14159);
            dos.writeUTF("Antigravity");
        }

        byte[] binaryPayload = baos.toByteArray();
        System.out.println("Encoded bytes: " + Arrays.toString(binaryPayload));
    }
}
```

### Example 2: Stream Tokenizer and Line Counter (Real-World)
```java
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class LineStatistics {
    public record Stats(long lines, long words, long chars) {}

    public static Stats analyzeFile(String filePath) throws IOException {
        long lines = 0, words = 0, chars = 0;
        try (BufferedReader reader = new BufferedReader(new FileReader(filePath, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                lines++;
                chars += line.length();
                String[] tokens = line.trim().split("\\s+");
                if (tokens.length > 0 && !tokens[0].isEmpty()) {
                    words += tokens.length;
                }
            }
        }
        return new Stats(lines, words, chars);
    }
}
```

### Example 3: Edge Case - Forgetting to Flush BufferedOutputStream
```java
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class FlushHazardDemo {
    public static void main(String[] args) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        BufferedOutputStream bos = new BufferedOutputStream(baos);

        bos.write("Hello World".getBytes());

        // WITHOUT FLUSH: Bytes sit in internal 8KB buffer!
        System.out.println("Before flush size: " + baos.size()); // 0!

        bos.flush(); // Flush internal buffer to destination
        System.out.println("After flush size:  " + baos.size()); // 11!
    }
}
```

### Example 4: Failure Case - Reading Byte Without Checking for `-1` Correctly
```java
import java.io.ByteArrayInputStream;

public class ByteCastFailure {
    public static void main(String[] args) {
        byte[] data = {(byte) 0xFF}; // 255 in unsigned, -1 in signed byte
        ByteArrayInputStream in = new ByteArrayInputStream(data);

        // FATAL MISTAKE: Casting to byte before checking -1!
        // byte b = (byte) in.read(); // 0xFF cast to byte becomes -1!
        // if (b == -1) -> False EOF detected on first byte!

        // CORRECT FIX:
        int value = in.read();
        if (value != -1) {
            byte actualByte = (byte) value;
            System.out.println("Successfully read byte: " + (value & 0xFF)); // 255
        }
    }
}
```

---

### Additional Deep Dive: Piped Streams and Inter-Thread Communication
Java also provides `PipedInputStream` and `PipedOutputStream` (along with `PipedReader` and `PipedWriter`). These streams allow one thread to write data that is concurrently read by another thread, acting as an in-memory unidirectional pipe. If the reading thread terminates while data is still in the buffer, or if the writing thread dies, an `IOException: Read end dead` or `Write end dead` is raised. In modern concurrent programming, blocking queues (`ArrayBlockingQueue`) are preferred, but piped streams remain relevant for wrapping legacy APIs that strictly accept `InputStream` without materializing buffers in RAM.

### Zero-Copy Stream Transfer via `InputStream.transferTo()`
Introduced in Java 9, `InputStream.transferTo(OutputStream out)` transfers all remaining bytes directly from this stream to the given output stream using a pre-allocated internal transfer buffer (default 8192 bytes):
```java
try (InputStream in = Files.newInputStream(sourcePath);
     OutputStream out = Files.newOutputStream(destPath)) {
    long bytesCopied = in.transferTo(out);
    System.out.println("Bytes copied: " + bytesCopied);
}
```
This eliminates boilerplate `while ((read = in.read(buf)) != -1)` transfer loops.

## 6. Best Practices

1. **Always Use Try-With-Resources**: Enforce automatic closing of streams to prevent OS file descriptor leaks.
2. **Always Wrap in Buffers**: Never read or write unbuffered streams (`FileInputStream`, `FileOutputStream`) directly in byte loops; wrap with `BufferedInputStream` or `BufferedReader`.
3. **Always Specify `StandardCharsets.UTF_8`**: Never rely on default platform charsets when instantiating `InputStreamReader` or `FileReader`.
4. **Use `InputStream.transferTo(out)` (Java 9+)**: Simplifies copying streams without manual byte loop buffers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Casting `read()` directly to `byte` before checking `-1` | A valid `0xFF` data byte casts to `-1`, triggering premature EOF. | Check `if (intVal == -1)` before casting. |
| Forgetting to flush `BufferedOutputStream` | Buffered data is lost if stream is not properly closed or flushed. | Always close or explicitly call `flush()`. |
| Reading text files with `FileInputStream` | Binary byte stream cannot handle multi-byte Unicode characters properly. | Use `BufferedReader` with `FileReader`. |
| Omitting Charset parameter | Uses platform default encoding, causing encoding corruption across systems. | Pass `StandardCharsets.UTF_8`. |
| Catching generic `Exception` instead of `IOException` | Masks runtime exceptions; hinders specific error recovery. | Catch `IOException`. |
| Closing outer stream manually in finally block | Redundant and error-prone; closing outer stream closes wrapped streams. | Use try-with-resources. |

---

## 8. Performance & Security Notes

### Performance
- **Buffer Size Optimization**: The default buffer size (8 KB) matches the virtual memory page size of modern operating systems, ensuring optimal OS kernel page cache read-ahead efficiency.
- **`transferTo` Zero-Copy Potential**: On modern platforms, `InputStream.transferTo()` can leverage kernel-level zero-copy primitives (`sendfile(2)`) when supported.

### Security
- **Path Traversal Vulnerability**: Never construct file paths by concatenating untrusted user input directly into `FileInputStream("uploads/" + filename)`. Validate against `..` directory traversal attacks.

---

## 9. Comparison / When to Use What

| Task | Recommended Class | Type |
|---|---|---|
| Reading text line-by-line | `BufferedReader` + `FileReader` | Character Stream |
| Writing formatted text | `PrintWriter` or `BufferedWriter` | Character Stream |
| Reading raw binary data | `BufferedInputStream` + `FileInputStream` | Byte Stream |
| Writing raw binary data | `BufferedOutputStream` + `FileOutputStream` | Byte Stream |
| Structured primitives | `DataInputStream` / `DataOutputStream` | Binary Decorator |
| In-memory byte buffer | `ByteArrayInputStream` / `ByteArrayOutputStream` | Memory Stream |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between Byte Streams and Character Streams?</summary>
Byte streams (`InputStream`, `OutputStream`) process raw 8-bit binary bytes with no encoding awareness. Character streams (`Reader`, `Writer`) process 16-bit Unicode characters, automatically handling character encoding and decoding (e.g. converting UTF-8 bytes to Java `char`s).
</details>

<details>
<summary>2. Why does `InputStream.read()` return an `int` instead of a `byte`?</summary>
To represent both all 256 possible unsigned byte values ($0$ to $255$) and the special end-of-file sentinel value ($-1$). If it returned a `byte`, the valid byte value `0xFF` (signed $-1$) would be indistinguishable from EOF.
</details>

<details>
<summary>3. How does buffering improve I/O performance?</summary>
Direct unbuffered I/O triggers an expensive OS system call and disk/network interrupt for every single byte. Buffering loads large chunks of data (typically 8 KB) into a RAM buffer in a single system call, serving subsequent read/write requests directly from memory.
</details>

<details>
<summary>4. What design pattern does `java.io` heavily rely on?</summary>
The **Decorator Pattern**. It allows functionality (such as buffering, data conversion, or compression) to be added dynamically to an existing stream by wrapping it inside decorator streams (`new BufferedReader(new InputStreamReader(new FileInputStream(...)))`).
</details>

<details>
<summary>5. What happens if you close a wrapper stream like `BufferedReader`?</summary>
Closing the outer wrapper stream automatically closes all underlying wrapped streams (`InputStreamReader`, `FileInputStream`).
</details>

<details>
<summary>6. What was the impact of JEP 400 in Java 18?</summary>
JEP 400 standardized the default charset of the Java runtime to **UTF-8** across all platforms and operating systems, eliminating platform-dependent encoding bugs when Charset is omitted.
</details>

<details>
<summary>7. What does `OutputStream.flush()` do?</summary>
It forces any buffered output bytes currently held in user-space memory buffers to be written out to the underlying operating system stream.
</details>

<details>
<summary>8. How does `InputStream.transferTo(OutputStream out)` optimize stream copying?</summary>
Introduced in Java 9, it reads all bytes from the input stream and writes them directly to the output stream using an internal optimized 8KB buffer, eliminating the need to write manual while-loops.
</details>

---

## 11. Practice Exercises

### Easy
1. Write a program that reads a text file and prints it line by line using `BufferedReader`.
2. Copy a binary image file from source to destination using `BufferedInputStream` and `BufferedOutputStream`.
3. Write an integer and a string to a byte array using `DataOutputStream`.

### Medium
4. Implement a line-numbering reader that prints line numbers before each line in a file.
5. Write a method that calculates the SHA-256 hash of a file using `DigestInputStream`.
6. Implement a file search utility that counts occurrences of a specific word across a text file using `BufferedReader`.

### Hard
7. Implement a custom `FilterInputStream` that dynamically decrypts AES-encrypted bytes on the fly.
8. Construct a circular memory buffer stream (`CircularByteBufferStream`) that allows non-blocking byte piping between threads.
9. Measure throughput differences between unbuffered `FileInputStream` vs `BufferedInputStream` vs `FileChannel` on a 100MB file using JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Line reading
try (BufferedReader br = new BufferedReader(new FileReader("test.txt", StandardCharsets.UTF_8))) {
    String line;
    while ((line = br.readLine()) != null) System.out.println(line);
}

// Medium 5: DigestInputStream
public static byte[] hashFile(String path) throws Exception {
    MessageDigest md = MessageDigest.getInstance("SHA-256");
    try (var dis = new DigestInputStream(new FileInputStream(path), md)) {
        dis.transferTo(OutputStream.nullOutputStream());
    }
    return md.digest();
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Byte Streams**: `InputStream` / `OutputStream` (Raw 8-bit binary).
- **Char Streams**: `Reader` / `Writer` (16-bit Unicode characters).
- **EOF**: `read()` returns `-1` on end of stream.
- **Buffering**: Always wrap with `BufferedInputStream` / `BufferedReader`.
- **Charset**: Explicitly pass `StandardCharsets.UTF_8`.
- **Resource Cleanup**: Always use try-with-resources.

---

## 13. Further Reading
- JEP 400: *UTF-8 by Default* (OpenJDK).
- Gamma, Helm, Johnson, Vlissides. *Design Patterns* (Decorator Pattern in I/O).
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 9 (Prefer try-with-resources to try-finally).
