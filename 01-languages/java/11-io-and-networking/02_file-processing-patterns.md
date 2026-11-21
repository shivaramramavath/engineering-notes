# File Processing Patterns and Large File Streaming

> Processing massive files without memory exhaustion: stream pipelines, chunked binary processing, temp file patterns, and FileVisitor directory traversal.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_io-streams-readers-writers.md](00_io-streams-readers-writers.md), [01_nio-paths-and-files.md](01_nio-paths-and-files.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 8+ |

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

In production environments, applications frequently ingest, process, and generate massive files:
- Multi-gigabyte web server access logs
- Large CSV or JSON export dumps (10 GB+)
- Video, audio, or database backup archives
- Deep directory structures containing millions of documents

A common novice mistake is calling `Files.readAllBytes(path)` or `Files.readAllLines(path)` on these files. Attempting to load a 4 GB file into memory on a JVM with a 2 GB heap immediately crashes the service with an unrecoverable `OutOfMemoryError`.

Scalable file processing requires **streaming and chunking architectures**: processing files incrementally, in small constant-memory chunks, so that memory consumption remains fixed ($O(1)$ space) regardless of whether the file is 10 megabytes or 50 gigabytes.

---

## 2. Core Concepts

### 1. Lazy Stream Processing with `Files.lines()`
`Files.lines(path)` returns a `Stream<String>` that lazily reads lines from the file on demand.
- Lines are fetched as the stream pipeline pulls them.
- Memory consumption is proportional to the length of a single line, **not** the size of the file.
- Must be wrapped in try-with-resources because it holds an underlying OS file channel open.

### 2. Chunked Binary Processing
For binary files, reading via fixed-size byte buffers (e.g., 64 KB) ensures constant memory footprint while minimizing system calls.

### 3. Safe Temporary File Handling
When generating or transforming files:
- Write intermediate data to a temporary file created via `Files.createTempFile()`.
- Set JVM shutdown delete hooks or delete in `finally` blocks.
- On success, rename or move atomically to the final destination.

### 4. Recursive Traversal via `FileVisitor`
For complex directory structures, `Files.walkFileTree(path, FileVisitor)` provides fine-grained control:
- `preVisitDirectory`: Before entering directory.
- `visitFile`: On each file encountered.
- `visitFileFailed`: Handles permission errors without aborting the entire traversal.
- `postVisitDirectory`: After leaving directory (ideal for deleting directories bottom-up).

---

## 3. How It Works Internally

### `Files.lines()` Spliterator Mechanics
Inside OpenJDK, `Files.lines()` creates a custom `BufferedReaderSpliterator`:
```java
// Simplified OpenJDK internal representation
static class BufferedReaderSpliterator implements Spliterator<String> {
    private final BufferedReader reader;
    public boolean tryAdvance(Consumer<? super String> action) {
        String line = reader.readLine();
        if (line != null) {
            action.accept(line);
            return true;
        }
        return false;
    }
}
```
Each call to `tryAdvance()` reads exactly one line from the underlying OS channel. Once processed, the line string becomes eligible for garbage collection immediately.

```
Disk Storage (50 GB Log File)
        |
        v  One line at a time (O(1) Memory!)
[ BufferedReader: 8 KB buffer ] ---> tryAdvance() ---> Line Consumer ---> GC!
```

---

## 4. Syntax & Usage

### 1. Processing a Large File Line by Line
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;

public class LargeLineStreamDemo {
    public static void main(String[] args) throws IOException {
        Path hugeLog = Path.of("server.log");

        // Processes 50GB file with only ~1MB JVM heap memory!
        try (Stream<String> lines = Files.lines(hugeLog)) {
            long errorCount = lines
                .filter(line -> line.contains("ERROR"))
                .count();

            System.out.println("Total ERROR lines: " + errorCount);
        }
    }
}
```

### 2. Chunked Binary Streaming
```java
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

public class ChunkedBinaryProcessing {
    public static void processInChunks(String src, String dest) throws IOException {
        byte[] buffer = new byte[64 * 1024]; // 64 KB buffer

        try (var in = new FileInputStream(src);
             var out = new FileOutputStream(dest)) {

            int bytesRead;
            while ((bytesRead = in.read(buffer)) != -1) {
                // Process bytes in buffer[0 .. bytesRead]
                out.write(buffer, 0, bytesRead);
            }
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Recursive Directory Deletion via `SimpleFileVisitor` (Basic to Intermediate)
```java
import java.io.IOException;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;

public class SafeDirectoryCleaner {
    public static void deleteDirectoryRecursively(Path dir) throws IOException {
        Files.walkFileTree(dir, new SimpleFileVisitor<>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
                Files.delete(file); // Delete file first
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc) throws IOException {
                if (exc != null) throw exc;
                Files.delete(dir); // Delete directory after its contents are gone!
                return FileVisitResult.CONTINUE;
            }
        });
    }

    public static void main(String[] args) throws IOException {
        Path tempDir = Files.createTempDirectory("cleanup-demo");
        Files.writeString(tempDir.resolve("temp.txt"), "data");
        deleteDirectoryRecursively(tempDir);
        System.out.println("Cleaned directory successfully!");
    }
}
```

### Example 2: External Merge Sort for Multi-Gigabyte Files (Real-World)
```java
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ExternalMergeSortChunker {
    // Splits large file into sorted temporary chunk files
    public static List<Path> splitAndSortChunks(Path largeFile, int chunkSize) throws IOException {
        List<Path> chunkFiles = new ArrayList<>();
        List<String> memoryBuffer = new ArrayList<>(chunkSize);

        try (BufferedReader reader = Files.newBufferedReader(largeFile)) {
            String line;
            while ((line = reader.readLine()) != null) {
                memoryBuffer.add(line);
                if (memoryBuffer.size() >= chunkSize) {
                    Collections.sort(memoryBuffer);
                    Path chunkPath = Files.createTempFile("sort-chunk-", ".tmp");
                    Files.write(chunkPath, memoryBuffer);
                    chunkFiles.add(chunkPath);
                    memoryBuffer.clear();
                }
            }
            if (!memoryBuffer.isEmpty()) {
                Collections.sort(memoryBuffer);
                Path chunkPath = Files.createTempFile("sort-chunk-", ".tmp");
                Files.write(chunkPath, memoryBuffer);
                chunkFiles.add(chunkPath);
            }
        }
        return chunkFiles;
    }
}
```

### Example 3: Edge Case - UTF-8 Malformed Input in `Files.lines()`
```java
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

public class MalformedEncodingHandler {
    public static void readCorruptedFile(Path path) throws Exception {
        // Files.lines() throws MalformedInputException if non-UTF-8 bytes are found!
        // To handle corrupted files gracefully, configure CharsetDecoder explicitly:
        var decoder = StandardCharsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPLACE)
            .onUnmappableCharacter(CodingErrorAction.REPLACE);

        try (var reader = new BufferedReader(new InputStreamReader(Files.newInputStream(path), decoder))) {
            String line;
            while ((line = reader.readLine()) != null) {
                // Malformed characters replaced with '?' instead of throwing exception!
            }
        }
    }
}
```

### Example 4: Failure Case - Memory Leak via `Files.readAllLines()`
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

public class ReadAllLinesOomFailure {
    public static void main(String[] args) throws IOException {
        // FATAL MISTAKE on a 10 GB file:
        // List<String> allLines = Files.readAllLines(Path.of("huge.log"));
        // Allocates millions of String objects simultaneously -> java.lang.OutOfMemoryError: Java heap space!
    }
}
```

---

## 6. Best Practices

1. **Never Call `Files.readAllLines()` or `Files.readAllBytes()` on Arbitrary Files**: Only use them when file size is guaranteed to be small (< 10 MB).
2. **Always Process Large Text Files via `Files.lines()` in Try-With-Resources**: Guarantees constant $O(1)$ memory consumption and proper resource closing.
3. **Use Buffers Sized to Power of Two (64 KB - 128 KB) for Binary Processing**: Aligns with OS virtual memory paging to maximize I/O throughput.
4. **Delete Temporary Files in `finally` Blocks or Use Shutdown Hooks**: Ensure temporary files generated during transformations do not clutter the host disk.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Calling `readAllLines()` on large files | Loads entire file into heap simultaneously, causing `OutOfMemoryError`. | Use `Files.lines()` with streaming. |
| Forgetting to close `Files.lines()` | Leaks OS file channel handles. | Wrap in try-with-resources. |
| Deleting non-empty directory with `Files.delete()` | Throws `DirectoryNotEmptyException`. | Use `Files.walkFileTree` with bottom-up deletion. |
| Crashing on malformed UTF-8 bytes | `Files.lines()` throws `MalformedInputException`. | Use custom `CharsetDecoder` with `CodingErrorAction.REPLACE`. |
| Writing partial files directly to production path | Readers encounter corrupt, incomplete data if writer crashes. | Write to temp file and move atomically. |
| Unbounded `Files.walk()` traversal on deep symlink loops | Loops indefinitely across cyclic links. | Pass `maxDepth` or `FileVisitOption.FOLLOW_LINKS` restrictions. |

---

## 8. Performance & Security Notes

### Performance
- **Garbage Collection Efficiency**: Streaming lines with `Files.lines()` keeps object lifecycles extremely short (Young Gen allocation), allowing the G1 or ZGC garbage collector to reclaim string memory with near-zero pause times.

### Security
- **Zip Slip Vulnerability**: When extracting archives, paths inside zip entries can contain `../../`. Always verify that `targetPath.normalize().startsWith(destinationDir)`.

---

## 9. Comparison / When to Use What

| Pattern | Memory Usage | Best For |
|---|---|---|
| `Files.readString()` | $O(N)$ (Entire file in heap) | Small files (< 10 MB), configs |
| `Files.lines()` | $O(1)$ (One line at a time) | Streaming large text logs, CSVs |
| Chunked `InputStream` | $O(1)$ (64 KB buffer) | Large binary downloads, copying |
| Memory-Mapped (`mmap`) | $O(1)$ JVM heap (OS Page Cache) | Extremely high-throughput random access |
| `Files.walkFileTree()` | $O(\text{depth})$ (Call stack only) | Deep directory search, recursive delete |

---

## 10. Interview Questions

<details>
<summary>1. Why does `Files.lines()` consume significantly less memory than `Files.readAllLines()`?</summary>
`Files.readAllLines()` eagerly reads all lines and stores them simultaneously in a `List<String>`, consuming memory proportional to the entire file size ($O(N)$). `Files.lines()` returns a lazy `Stream<String>` backed by an underlying reader; it only loads one line into memory at a time as downstream operations pull it, consuming constant memory ($O(1)$).
</details>

<details>
<summary>2. Why does `Files.delete(directoryPath)` fail on a non-empty directory?</summary>
The underlying operating system `rmdir` system call strictly requires a directory to be empty before it can be unlinked. To delete a non-empty directory in Java, you must walk the directory tree and delete child files first, deleting the directory in `postVisitDirectory`.
</details>

<details>
<summary>3. What is the Zip Slip vulnerability and how do you prevent it?</summary>
Zip Slip is a security vulnerability where a malicious archive contains file names with directory traversal characters (e.g. `../../../../etc/cron.d/malicious`). When unzipped, it overwrites sensitive files outside the target directory. Prevention: check `targetPath.normalize().startsWith(destinationFolder)`.
</details>

<details>
<summary>4. How does external merge sort allow sorting a 50GB file on a machine with 2GB RAM?</summary>
It divides the large file into small chunks (e.g. 50MB) that fit into RAM, sorts each chunk in memory, and writes them to temporary files on disk. Then, it uses a $K$-way merge with a `PriorityQueue` to read the smallest elements from each chunk file and stream the globally sorted result to disk in $O(1)$ RAM.
</details>

<details>
<summary>5. What exception is thrown if `Files.lines()` encounters an invalid byte sequence?</summary>
`java.nio.charset.MalformedInputException`.
</details>

<details>
<summary>6. What does `FileVisitResult.SKIP_SUBTREE` do in a `FileVisitor`?</summary>
When returned from `preVisitDirectory`, it instructs the file walker to skip the current directory and all of its descendants, continuing traversal with the next sibling.
</details>

<details>
<summary>7. What buffer size is typically recommended for chunked binary I/O?</summary>
Between 64 KB and 128 KB. This size aligns with modern OS file cache readahead buffers while maintaining a minimal memory footprint.
</details>

<details>
<summary>8. How can you ensure temporary files are deleted when the JVM exits?</summary>
By calling `tempPath.toFile().deleteOnExit()` or managing cleanup in a registered JVM shutdown hook (`Runtime.getRuntime().addShutdownHook(...)`).
</details>

---

## 11. Practice Exercises

### Easy
1. Count the total lines in a file using `Files.lines().count()`.
2. Create a temporary file with prefix `"test-"` and suffix `".tmp"` using `Files.createTempFile()`.
3. Filter lines in a log file that contain "WARN" and write them to a new file.

### Medium
4. Implement a chunked file copy utility using a 64 KB byte buffer.
5. Write a recursive directory file deleter using `Files.walkFileTree`.
6. Implement a streaming CSV parser that computes the average value of a specific column without loading the entire CSV into memory.

### Hard
7. Implement an external merge sort algorithm that sorts a 1GB file containing random numbers using at most 50MB of heap memory.
8. Build a safe ZIP archive extraction utility that defends against Zip Slip and Zip Bomb (compression ratio expansion) attacks.
9. Benchmark line processing throughput of `BufferedReader.readLine()` vs `Files.lines()` vs `Scanner` using JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: Line count
long count;
try (Stream<String> lines = Files.lines(Path.of("test.txt"))) {
    count = lines.count();
}

// Medium 5: Recursive delete
public static void deleteTree(Path root) throws IOException {
    Files.walkFileTree(root, new SimpleFileVisitor<>() {
        public FileVisitResult visitFile(Path file, BasicFileAttributes a) throws IOException {
            Files.delete(file); return FileVisitResult.CONTINUE;
        }
        public FileVisitResult postVisitDirectory(Path dir, IOException e) throws IOException {
            Files.delete(dir); return FileVisitResult.CONTINUE;
        }
    });
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Large Files**: Always use `Files.lines(path)` in try-with-resources.
- **Never**: Call `readAllLines()` on multi-gigabyte files.
- **Binary Chunking**: Process in 64 KB byte buffers.
- **Directory Walking**: Use `Files.walkFileTree` for fine-grained traversal control.
- **Atomic Writes**: Write to temp file first, then `ATOMIC_MOVE`.
- **Zip Slip Defense**: Check `path.normalize().startsWith(dest)`.

---

## 13. Further Reading
- Oracle Java Documentation: *Walking the File Tree*.
- Snyk: *Zip Slip Vulnerability Research*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 59 (Know and use the libraries).
