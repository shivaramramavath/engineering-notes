# I/O Streams, Readers and Writers

Java's classic I/O (`java.io`) models data movement as **streams**: you read from a source or write to a destination one piece at a time, without caring whether it is a file, a socket, or memory. Even if you mostly use `Files` ([NIO](01_nio-paths-and-files.md)) today, streams are everywhere: HTTP bodies, sockets, zip entries, and library APIs all hand you an `InputStream` or `OutputStream`.

**Prerequisites:** [try-with-resources](../06-exceptions-and-debugging/03_try-with-resources.md), [Unicode and Encoding](../03-strings-and-text/07_unicode-and-encoding.md).

---

## 1. Two families: bytes and characters

```text
                 bytes                          characters
  read   ┌─────────────────┐              ┌─────────────────┐
         │  InputStream    │              │     Reader      │
  write  ├─────────────────┤              ├─────────────────┤
         │  OutputStream   │              │     Writer      │
         └─────────────────┘              └─────────────────┘
          images, zip, network,            text: needs a CHARSET to turn
          anything binary                  bytes into chars and back
```

- **`InputStream` / `OutputStream`**: raw bytes (`0-255`). Use for anything that isn't text, and whenever you don't control the encoding.
- **`Reader` / `Writer`**: UTF-16 `char`s. They sit on top of a byte stream and apply a **charset** ([Unicode and Encoding](../03-strings-and-text/07_unicode-and-encoding.md)).

The bridge between the two is `InputStreamReader` / `OutputStreamWriter`, which takes the charset:

```java
Reader r = new InputStreamReader(inputStream, StandardCharsets.UTF_8);
Writer w = new OutputStreamWriter(outputStream, StandardCharsets.UTF_8);
```

---

## 2. Reading and writing bytes

```java
try (InputStream in = new FileInputStream("photo.jpg");
     OutputStream out = new FileOutputStream("copy.jpg")) {

    byte[] buffer = new byte[8192];
    int n;
    while ((n = in.read(buffer)) != -1) {
        out.write(buffer, 0, n);     // write only the n bytes actually read
    }
}
```

The same loop as a one-liner (Java 9+):

```java
in.transferTo(out);
```

### How `read` behaves

| Call | Returns |
|---|---|
| `int read()` | One byte as `0-255`, or `-1` at end of stream |
| `int read(byte[] b)` | Number of bytes actually read (**may be less than `b.length`**), or `-1` at EOF |
| `readNBytes(int n)` (9+) | Up to `n` bytes, blocking until `n` bytes or EOF |
| `readAllBytes()` (9+) | Everything remaining, **into memory** |

The classic bug is assuming `read(buffer)` fills the buffer. It doesn't have to, especially on sockets. Always use the returned count `n`.

Appending instead of overwriting: `new FileOutputStream("log.bin", true)`. By default the file is truncated.

---

## 3. Reading and writing text

```java
// Read line by line
try (BufferedReader reader = new BufferedReader(new FileReader("notes.txt", StandardCharsets.UTF_8))) {
    String line;
    while ((line = reader.readLine()) != null) {      // null at EOF; line terminators stripped
        process(line);
    }
}

// Write text
try (BufferedWriter writer = new BufferedWriter(new FileWriter("out.txt", StandardCharsets.UTF_8))) {
    writer.write("first line");
    writer.newLine();                                 // platform line separator
}

// PrintWriter: println/printf convenience
try (PrintWriter pw = new PrintWriter(new BufferedWriter(new FileWriter("report.txt", StandardCharsets.UTF_8)))) {
    pw.printf("Total: %d%n", 42);
}
```

`FileReader`/`FileWriter` constructors that take a `Charset` exist since Java 11. For file work, the NIO equivalents (`Files.newBufferedReader`, `Files.readString`) are shorter and always default to UTF-8 ([NIO](01_nio-paths-and-files.md)).

### `PrintWriter` hides errors

`PrintWriter` (and `PrintStream`, including `System.out`) **never throws `IOException`**. Failures are recorded and exposed through `checkError()`. For data you can't afford to lose, use `BufferedWriter`, or check `pw.checkError()` after writing.

---

## 4. Decorators: stacking behavior

Streams use the decorator pattern ([Decorator](../24-design-patterns/02-structural/01_decorator.md)): each wrapper adds one capability on top of another stream.

```text
FileInputStream  →  BufferedInputStream  →  DataInputStream
 (reads file)        (adds buffering)        (readInt, readUTF...)

FileInputStream  →  InputStreamReader  →  BufferedReader
 (bytes)              (bytes → chars)       (buffering + readLine)
```

| Class | Adds |
|---|---|
| `BufferedInputStream` / `BufferedOutputStream` | In-memory buffer (default 8 KiB) → far fewer system calls |
| `BufferedReader` / `BufferedWriter` | Same for characters; `readLine()`, `newLine()` |
| `DataInputStream` / `DataOutputStream` | Primitives and `String` in a fixed binary format (big-endian) |
| `ByteArrayInputStream` / `ByteArrayOutputStream` | In-memory source/sink (great for tests) |
| `GZIPInputStream` / `GZIPOutputStream` | Compression ([File Processing Patterns](02_file-processing-patterns.md)) |
| `ObjectInputStream` / `ObjectOutputStream` | Java serialization ([Serialization](03_java-serialization.md)) |

```java
// Binary format example
try (DataOutputStream out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream("data.bin")))) {
    out.writeInt(42);
    out.writeDouble(3.14);
    out.writeUTF("hello");
}
try (DataInputStream in = new DataInputStream(new BufferedInputStream(new FileInputStream("data.bin")))) {
    int i = in.readInt();           // must read in the SAME order as written
    double d = in.readDouble();
    String s = in.readUTF();
}
```

---

## 5. Buffering, flushing, and closing

### Buffering

Unbuffered `FileInputStream.read()` is one OS call per byte, which is extremely slow. Wrap raw file and socket streams in a buffered stream, or read into your own `byte[]` buffer (as in the copy loop). Wrapping an already-buffered stream again is harmless but pointless.

### Flushing

A buffered writer holds data in memory until its buffer fills, `flush()` is called, or it is closed. If a program exits without flushing or closing, **data can be lost**.

- `close()` flushes first. This is why try-with-resources is the correct default.
- Call `flush()` explicitly only when the other side needs data *now* (interactive protocols, sockets).

### Closing

```java
try (BufferedReader r = new BufferedReader(new FileReader("a.txt", StandardCharsets.UTF_8))) {
    ...
}   // closes r, which closes the FileReader underneath
```

- **Closing the outermost wrapper closes everything inside it.** You only need to declare the outermost one (declaring each is also fine).
- Always use try-with-resources. Unclosed streams leak file descriptors and, on Windows, keep files locked.
- Closing a stream twice is safe; using it after closing throws `IOException`.

---

## 6. Charsets: the usual source of garbled text

Bytes only become text through a charset:

```java
byte[] bytes = "café".getBytes(StandardCharsets.UTF_8);          // 5 bytes: é is 2 bytes
String s = new String(bytes, StandardCharsets.UTF_8);            // "café"
String broken = new String(bytes, StandardCharsets.ISO_8859_1);  // "cafÃ©"  ← wrong charset
```

Rules:

- **Always pass a charset explicitly** (`StandardCharsets.UTF_8`) to `InputStreamReader`, `OutputStreamWriter`, `String.getBytes`, `new String(bytes, cs)`.
- Before Java 18, the default charset came from the OS and varied between machines. Since Java 18 ([JEP 400](https://openjdk.org/jeps/400)) the default is UTF-8, but code that states its charset is still clearer and works on older JVMs.
- The encoding of `System.out` and the console is a **separate** setting from the file default charset, which is why `é` may print wrongly even when file reading works.
- Java does not strip a UTF-8 **BOM**. A file saved with a BOM will start with `\uFEFF` in your first string, which breaks key lookups and parsing of the first header column.
- You cannot reliably *detect* an unknown encoding. Agree on it (UTF-8) with whoever produces the data.

---

## 7. Standard streams

`System.in` is an `InputStream`; `System.out` and `System.err` are `PrintStream`s. To read console text line by line:

```java
BufferedReader console = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
```

`Scanner` is convenient for parsing tokens but slower and easy to misuse with mixed `nextInt()` / `nextLine()` calls. See [Console Input and Output](../01-fundamentals/09_console-input-output.md). Don't close `System.in`/`System.out` unless you intend to lose them for the rest of the program.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Ignoring the count returned by `read(buf)` | `out.write(buf, 0, n)` |
| No buffering on file/socket streams | Wrap in a `Buffered*` stream or use a `byte[]` buffer |
| Relying on the default charset | Pass `StandardCharsets.UTF_8` |
| Not closing streams / closing in a plain `finally` with nested try | try-with-resources |
| Writing without flush/close, then reading the file back | Close (or flush) the writer first |
| Reading text with `InputStream` and casting bytes to `char` | Use a `Reader` with a charset |
| Using `PrintWriter` and assuming failures throw | Check `checkError()` or use `BufferedWriter` |
| `readAllBytes()` / reading everything on huge inputs | Stream in chunks; see [File Processing Patterns](02_file-processing-patterns.md) |
| Reading data with `DataInputStream` in a different order than written | Keep reader/writer symmetric, and prefer a real format for anything long-lived |

### Debugging

- Strange characters (`Ã©`, `?`, `�`) → a charset mismatch: compare the writer's encoding to the reader's.
- `FileNotFoundException` that says "(Access is denied)" or "(Is a directory)" → check the path type and permissions, not just existence. Print `new File(path).getAbsolutePath()` to see what the relative path resolves against (the process working directory).
- `IOException: Stream closed` → you closed an outer wrapper and then kept using the inner stream, or closed it twice via a different owner.
- Output file empty or truncated → an unflushed/unclosed writer, or the process ended before `close()`.
- A read call that never returns on a socket → blocking is normal. Set timeouts ([HTTP Client and Networking](04_http-client-and-networking.md)).

---

## Quick Summary

- **Bytes** → `InputStream`/`OutputStream`. **Text** → `Reader`/`Writer` + an explicit charset.
- Streams are decorators: file stream → buffer → data/text layer.
- `read(byte[])` returns *how many* bytes it read. Use that count.
- Buffer file and network I/O. `close()` flushes. Use try-with-resources always.
- `PrintWriter`/`PrintStream` swallow exceptions.
- Specify UTF-8 explicitly, and watch for BOMs and console-encoding differences.

**Next:** [NIO: Paths and Files](01_nio-paths-and-files.md)
