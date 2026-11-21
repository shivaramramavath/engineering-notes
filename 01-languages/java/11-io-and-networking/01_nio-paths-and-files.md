# NIO.2 Paths, Files, and Modern File System APIs

> Modern file systems in Java: Path operations, Files utility methods, atomic file manipulation, memory-mapped files, and directory walking.

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | [00_io-streams-readers-writers.md](00_io-streams-readers-writers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Active |
| **Java Version** | Java 7+ (Modern helper methods Java 11+) |

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

In early Java versions, file manipulation was handled by `java.io.File`. The legacy `File` class suffered from severe architectural limitations:
- **Poor Error Reporting**: Many methods returned a boolean `false` on failure (e.g. `file.delete()` returned `false` without any explanation of whether the file was locked, missing, or permissions were denied).
- **No Symbolic Link Support**: It could not detect or traverse Unix symlinks properly.
- **Inability to Handle File Metadata**: It could not read or modify POSIX file permissions, ACLs, or file owner attributes.
- **Scalability Collapses**: Calling `file.listFiles()` on a directory with 1,000,000 files allocated an array of 1,000,000 `File` objects simultaneously in memory, routinely crashing JVMs with `OutOfMemoryError`.

Introduced in Java 7 (JSR-203) and enriched in Java 11+, **NIO.2** (`java.nio.file`) completely replaced `java.io.File` with:
- **`Path`**: An immutable, hierarchical representation of a file path.
- **`Files`**: A comprehensive static utility class providing atomic operations, stream-based directory walking, and attribute access.
- **`FileSystem`**: Pluggable file system architecture (supporting ZIP file systems and memory-mapped files).

---

## 2. Core Concepts

### 1. `java.nio.file.Path`
Represents a programmatic path in a file system. It is **immutable** and does not require the physical file to exist on disk:
- Creation: `Path.of("docs", "readme.txt")` (Java 11+) or `Paths.get("docs/readme.txt")`.
- Operations: `resolve()`, `relativize()`, `normalize()`, `toAbsolutePath()`.

### 2. `java.nio.file.Files`
The static powerhouse of modern Java file operations:
- **Reading/Writing Small Files (Java 11+)**: `Files.readString(path)`, `Files.writeString(path, content)`.
- **Stream-Based Operations**: `Files.lines(path)`, `Files.list(path)`, `Files.walk(path)`, `Files.find(path, maxDepth, matcher)`.
- **Atomic Operations**: `Files.move(src, dest, StandardCopyOption.ATOMIC_MOVE)`.
- **Attribute Queries**: `Files.exists(path)`, `Files.isDirectory(path)`, `Files.size(path)`.

### 3. Path Path Manipulation Methods
- **`resolve(String other)`**: Joins paths (like Unix path joining).
- **`relativize(Path other)`**: Computes the relative path from this path to another.
- **`normalize()`**: Eliminates redundant name elements like `.` and `..`.

---

## 3. How It Works Internally

### Atomic File Replacement Pattern
In high-concurrency systems, writing directly to an active production configuration file risks readers seeing partially written, corrupt data.
NIO.2 provides **atomic file replacement**:
1. Write the new configuration to a temporary file on the same filesystem.
2. Flush and sync the temp file to disk.
3. Call `Files.move(tempFile, targetFile, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)`.
Under the hood, this compiles to the POSIX `rename(2)` system call, atomically swapping the directory inode pointer in a single kernel cycle with zero reader downtime!

```
Step 1: Write to Temp File:
[ Inode: config.tmp (New Data) ]

Step 2: Atomic Move (POSIX rename):
Directory Inode Entry ("app.conf") ---> Points atomically to New Inode!
Old Inode unlinked by kernel. Zero intermediate corrupt state visible to readers!
```

---

## 4. Syntax & Usage

### 1. Modern One-Liner Reading and Writing (Java 11+)
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

public class ModernFileIoDemo {
    public static void main(String[] args) throws IOException {
        Path path = Path.of("sample.txt");

        // Write entire string with UTF-8
        Files.writeString(path, "Hello from Java 25!\nSecond line.\n");

        // Read entire string
        String content = Files.readString(path);
        System.out.println("Content:\n" + content);

        // Append to file
        Files.writeString(path, "Appended line.\n", StandardOpenOption.APPEND);
    }
}
```

### 2. Path Arithmetic and Normalization
```java
import java.nio.file.Path;

public class PathArithmeticDemo {
    public static void main(String[] args) {
        Path base = Path.of("/app/data");

        // Resolve child
        Path file = base.resolve("logs/app.log");
        System.out.println("Resolved:   " + file); // /app/data/logs/app.log

        // Normalization
        Path messy = Path.of("/app/data/../logs/./app.log");
        System.out.println("Normalized: " + messy.normalize()); // /app/logs/app.log

        // Relativize
        Path p1 = Path.of("/a/b");
        Path p2 = Path.of("/a/b/c/d");
        System.out.println("Relative:   " + p1.relativize(p2)); // c/d
    }
}
```

---

## 5. Practical Examples

### Example 1: Stream-Based Directory Walking and Filtering (Basic to Intermediate)
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.stream.Stream;

public class LogFileScanner {
    public static List<Path> findRecentLogFiles(Path rootDir) throws IOException {
        // Files.walk lazily streams directory entries without loading all into memory!
        try (Stream<Path> stream = Files.walk(rootDir, 3)) {
            return stream
                .filter(Files::isRegularFile)
                .filter(p -> p.getFileName().toString().endsWith(".log"))
                .filter(p -> {
                    try { return Files.size(p) > 1024; } // > 1 KB
                    catch (IOException e) { return false; }
                })
                .toList();
        }
    }

    public static void main(String[] args) throws IOException {
        List<Path> logs = findRecentLogFiles(Path.of("."));
        System.out.println("Found log files: " + logs);
    }
}
```

### Example 2: Memory-Mapped Files for Ultra-Fast Binary Scanning (Real-World)
```java
import java.io.RandomAccessFile;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.Path;

public class MemoryMappedFileDemo {
    public static void scanLargeFile(Path path) throws Exception {
        try (RandomAccessFile file = new RandomAccessFile(path.toFile(), "r");
             FileChannel channel = file.getChannel()) {

            long size = channel.size();
            // Map file directly into virtual memory pages!
            MappedByteBuffer buffer = channel.map(FileChannel.MapMode.READ_ONLY, 0, Math.min(size, 1024 * 1024));

            int countA = 0;
            while (buffer.hasRemaining()) {
                if (buffer.get() == 'A') countA++;
            }
            System.out.println("Count of 'A': " + countA);
        }
    }
}
```

### Example 3: Edge Case - `Files.exists()` vs `Files.notExists()`
```java
import java.nio.file.Files;
import java.nio.file.Path;

public class ExistsTriStateTrap {
    public static void main(String[] args) {
        Path path = Path.of("/restricted/secret.txt");

        // Files.exists() returns false if file doesn't exist OR if access is denied!
        // Files.notExists() returns false if file exists OR if access is denied!
        // TRAP: Both can return false simultaneously if permissions prevent checking!
        boolean exists = Files.exists(path);
        boolean notExists = Files.notExists(path);

        if (!exists && !notExists) {
            System.out.println("File existence status is UNKNOWN (permission denied)!");
        }
    }
}
```

### Example 4: Failure Case - Unclosed Directory Streams Causing File Descriptor Leaks
```java
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.stream.Stream;

public class UnclosedDirectoryStreamFailure {
    public static void main(String[] args) throws IOException {
        // FATAL MISTAKE: Calling Files.list() without try-with-resources!
        // Leaves an open directory file descriptor on the OS!
        /*
        Stream<Path> unclosed = Files.list(Path.of("."));
        unclosed.forEach(System.out::println);
        */

        // CORRECT FIX:
        try (Stream<Path> safe = Files.list(Path.of("."))) {
            safe.forEach(System.out::println);
        }
    }
}
```

---

### Filesystem Stores and POSIX File Attributes
NIO.2 provides direct programmatic inspection of underlying storage devices and POSIX security permissions via `FileStore` and `PosixFileAttributeView`:

```java
import java.nio.file.*;
import java.nio.file.attribute.*;
import java.util.Set;

public class FileStoreAndPosixDemo {
    public static void inspect(Path path) throws Exception {
        // Inspect underlying storage volume
        FileStore store = Files.getFileStore(path);
        System.out.println("Total storage space: " + (store.getTotalSpace() / (1024 * 1024 * 1024)) + " GB");
        System.out.println("Usable storage space: " + (store.getUsableSpace() / (1024 * 1024 * 1024)) + " GB");

        // Read and modify POSIX permissions (on UNIX/Linux systems)
        if (FileSystems.getDefault().supportedFileAttributeViews().contains("posix")) {
            Set<PosixFilePermission> permissions = Files.getPosixFilePermissions(path);
            System.out.println("Current POSIX permissions: " + PosixFilePermissions.toString(permissions));
            
            // Set 0644 (rw-r--r--)
            Files.setPosixFilePermissions(path, PosixFilePermissions.fromString("rw-r--r--"));
        }
    }
}
```

## 6. Best Practices

1. **Always Wrap `Files.lines()`, `Files.list()`, `Files.walk()` in Try-With-Resources**: These methods return streams backed by open operating system directory/file handles.
2. **Use `Path.of()` Instead of Legacy `Paths.get()`**: `Path.of()` (Java 11+) is the modern, idiomatic static factory.
3. **Use `ATOMIC_MOVE` for Reliable Configuration Updates**: Write to a temporary file on the same filesystem, then execute an atomic move to eliminate partial read corruptions.
4. **Use `Files.readString()` and `Files.writeString()` for Text Files**: Eliminates 10 lines of boilerplate reader/writer code for files that fit in memory.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Omitting try-with-resources on `Files.walk()` | Leaks OS directory file descriptors, eventually causing "Too many open files". | Use `try (Stream<Path> s = Files.walk(...))`. |
| Using `ATOMIC_MOVE` across different partitions/drives | OS atomic move only works within the same filesystem. | Copy and delete if cross-partition. |
| Using `Files.readAllBytes()` on multi-gigabyte files | Exhausts JVM heap memory instantly (`OutOfMemoryError`). | Process in chunks using `FileChannel` or streams. |
| Assuming `!Files.exists(path)` means file does not exist | Could mean access is denied by OS permissions. | Use `Files.notExists(path)` or check permissions. |
| Hardcoding path separators (`/` or `\`) | Breaks cross-platform portability between Windows and Linux. | Use `Path.of("dir", "file")` or `resolve()`. |
| Using legacy `java.io.File` in new code | Lacks exception details, symlink handling, and attribute support. | Use `Path` and `Files`. |

---

## 8. Performance & Security Notes

### Performance
- **Memory-Mapped Files (`mmap`)**: Bypasses JVM user-space memory buffers, allowing the OS kernel page cache to page file bytes directly into virtual memory.
- **Directory Walking Scalability**: `Files.walk()` uses lazy spliterators, traversing directories on demand with minimal memory overhead compared to legacy `File.listFiles()`.

### Security
- **Path Traversal Attacks**: Malicious input like `../../etc/passwd` can escape target directories. Always call `path.normalize()` and verify `normalizedPath.startsWith(baseDirectory)`.

---

## 9. Comparison / When to Use What

| Feature | Legacy `java.io.File` | Modern `java.nio.file.Path` |
|---|---|---|
| **Error Handling** | Returns `false` (no details) | Throws detailed `IOException` |
| **Symlinks** | Poor / Broken | Fully supported (`NOFOLLOW_LINKS`) |
| **Directory Scan** | Eager array (`File[]`) | Lazy stream (`Stream<Path>`) |
| **Metadata** | Last modified only | Full attributes (POSIX, ACL, Owner) |
| **Atomic Operations** | Unsupported | Supported (`ATOMIC_MOVE`) |

---

## 10. Interview Questions

<details>
<summary>1. What advantages does NIO.2 (`Path` and `Files`) offer over legacy `java.io.File`?</summary>
NIO.2 provides detailed exceptions instead of boolean failure returns, full symbolic link support, access to OS file attributes (POSIX, ACLs), scalable streaming directory traversal (`Files.walk`), and atomic file operations (`ATOMIC_MOVE`).
</details>

<details>
<summary>2. Why must streams returned by `Files.lines()` and `Files.walk()` be closed in try-with-resources?</summary>
Because these streams hold open underlying operating system file channels or directory handles. If unclosed, file descriptors leak, eventually exhausting the operating system limit ("Too many open files").
</details>

<details>
<summary>3. What is an atomic file move and why is it important?</summary>
An atomic move (`StandardCopyOption.ATOMIC_MOVE`) leverages the OS `rename` system call to update the filesystem directory pointer in a single operation. This ensures concurrent readers never observe partially written or corrupt files during updates.
</details>

<details>
<summary>4. What is the difference between `path.resolve()` and `path.relativize()`?</summary>
`resolve(other)` joins `other` onto `this` path (concatenation). `relativize(other)` calculates the relative navigation path required to reach `other` starting from `this` path.
</details>

<details>
<summary>5. How can `Files.exists(path)` and `Files.notExists(path)` both return `false`?</summary>
If the file's existence cannot be determined due to operating system permission restrictions (e.g. access denied on parent directory), both methods return `false`.
</details>

<details>
<summary>6. How do memory-mapped files improve I/O throughput?</summary>
By mapping a file directly into the application's virtual memory address space via `FileChannel.map()`, read and write operations access OS page cache memory directly, eliminating expensive user-space buffer copies and system calls.
</details>

<details>
<summary>7. What does `path.normalize()` do?</summary>
It eliminates redundant name elements from a path, such as `.` (current directory) and `..` (parent directory), without querying the physical filesystem.
</details>

<details>
<summary>8. How do you defend against path traversal attacks in Java?</summary>
Normalize the incoming path (`path.normalize()`) and verify that it starts with the intended base directory: `path.toAbsolutePath().startsWith(baseDir.toAbsolutePath())`.
</details>

---

## 11. Practice Exercises

### Easy
1. Read the entire content of a file into a string using `Files.readString()`.
2. Check if a directory exists, and if not, create all non-existent parent directories using `Files.createDirectories()`.
3. Normalize a path containing `../` elements and print the result.

### Medium
4. Write a directory cleanup utility that deletes all files ending in `.tmp` older than 7 days using `Files.find()`.
5. Implement an atomic configuration updater that writes to a temp file and replaces the target file using `ATOMIC_MOVE`.
6. Count total lines of code across all `.java` files in a project directory using `Files.walk()` and `Files.lines()`.

### Hard
7. Implement a fast file deduplicator that walks a directory, computes SHA-256 hashes for files of identical size, and reports duplicates.
8. Implement a file watcher service using `java.nio.file.WatchService` that detects and logs file creation and deletion events in real time.
9. Measure throughput differences between `Files.readAllBytes()` vs `FileChannel.map()` on a 500MB file using JMH.

<details>
<summary>Solutions</summary>

```java
// Easy 1: readString
String content = Files.readString(Path.of("sample.txt"));

// Easy 2: createDirectories
Files.createDirectories(Path.of("data/logs/2026"));

// Medium 5: Atomic update
public static void updateConfigAtomically(Path target, String configData) throws IOException {
    Path temp = Files.createTempFile(target.getParent(), "cfg", ".tmp");
    Files.writeString(temp, configData);
    Files.move(temp, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING);
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Path**: `Path.of("a", "b")` (Immutable representation).
- **Read/Write**: `Files.readString(path)`, `Files.writeString(path, text)`.
- **Streams**: `Files.lines()`, `Files.walk()`, `Files.find()` (Always close with try-with-resources!).
- **Atomic Move**: `Files.move(src, dest, ATOMIC_MOVE, REPLACE_EXISTING)`.
- **Normalization**: `path.normalize()` removes `.` and `..`.

---

## 13. Further Reading
- JSR-203: *More New I/O APIs for the Java Platform (NIO.2)*.
- Oracle Java Documentation: *File I/O (Featuring NIO.2)*.
- Bloch, Joshua. *Effective Java (3rd Edition)*: Item 59 (Know and use the libraries).
