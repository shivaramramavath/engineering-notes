# NIO: Paths and Files

`java.nio.file` (Java 7+) is the modern way to work with the file system. Two types do most of the work:

- **`Path`**: an immutable *name* for a location (it doesn't have to exist).
- **`Files`**: static methods that **act** on paths: read, write, copy, move, delete, list, inspect.

It replaces most uses of `java.io.File`, with real exceptions instead of `false` return values, symbolic-link awareness, atomic operations, and one-line reads and writes.

**Prerequisites:** [I/O Streams, Readers and Writers](00_io-streams-readers-writers.md).

---

## 1. `Path`: building and inspecting

```java
Path p1 = Path.of("data", "reports", "q1.csv");        // Java 11+; data/reports/q1.csv
Path p2 = Path.of("/var/log/app.log");
Path p3 = Paths.get("data/reports");                     // older equivalent of Path.of

p1.getFileName();          // q1.csv
p1.getParent();            // data/reports   (null if there is no parent)
p1.getRoot();              // null for relative, "/" on Unix for absolute
p1.isAbsolute();           // false
p1.toAbsolutePath();       // resolved against the process working directory
p1.getNameCount();         // 3
```

Creating a `Path` never touches the disk, so `Path.of("nope.txt")` is fine even if the file doesn't exist.

### Combining and cleaning

```java
Path base = Path.of("/srv/app");

base.resolve("config/app.yml");              // /srv/app/config/app.yml
base.resolve("/etc/passwd");                 // /etc/passwd   ← absolute argument REPLACES the base
base.resolveSibling("backup");               // /srv/backup

Path.of("/a/b").relativize(Path.of("/a/b/c/d"));   // c/d

Path.of("/srv/app/../app/./x").normalize();  // /srv/app/x   (lexical: removes . and ..)
path.toRealPath();                           // resolves symlinks; the file must exist
```

Important behaviors:

- `resolve` with an **absolute** argument returns that argument: a classic path-traversal ingredient.
- `normalize()` is purely textual. It does not look at the disk or follow symlinks.
- `Path.equals` compares path text, not the file it points to. For "same file?" use `Files.isSameFile(a, b)`.
- `~` is not expanded. Use `System.getProperty("user.home")`.
- Path separators are handled for you. Don't hand-build strings with `/` or `\`.

### Preventing path traversal

If part of a path comes from outside (upload name, API parameter), verify the final location stays inside your base directory:

```java
Path base = Path.of("/srv/uploads").toAbsolutePath().normalize();
Path target = base.resolve(userSuppliedName).normalize();
if (!target.startsWith(base)) {
    throw new SecurityException("Path escapes base directory");
}
```

See [Input Validation and Injection](../21-security/00_input-validation-and-injection.md). For symlink-aware checks, compare `toRealPath()` results.

---

## 2. Reading and writing whole files

```java
Path p = Path.of("notes.txt");

String text            = Files.readString(p);                 // Java 11+, UTF-8
List<String> lines     = Files.readAllLines(p);               // UTF-8
byte[] bytes           = Files.readAllBytes(p);

Files.writeString(p, "hello\n");                              // create or truncate
Files.write(p, List.of("a", "b", "c"));                       // lines
Files.write(p, bytes);
Files.writeString(p, "more\n", StandardOpenOption.APPEND);    // append
```

Rules:

- The `Files` text methods **default to UTF-8**, unlike the pre-Java-18 `FileReader`. You can pass another `Charset`.
- Invalid byte sequences throw `MalformedInputException`. They are **not** silently replaced as they are with `new String(bytes)`.
- Default write options are `CREATE, TRUNCATE_EXISTING, WRITE`: an existing file is **overwritten**. Pass options to change this:

| `StandardOpenOption` | Effect |
|---|---|
| `APPEND` | Add to the end |
| `CREATE_NEW` | Create; **fail** with `FileAlreadyExistsException` if it exists (atomic check) |
| `TRUNCATE_EXISTING` | Empty the file first (default with `WRITE`) |
| `CREATE` | Create if missing |
| `SYNC` / `DSYNC` | Force content (and metadata) to storage on each write: slow, durable |

These "read all" methods load the entire file into memory. Use them for small files only.

---

## 3. Streaming text

```java
// Line by line, constant memory
try (BufferedReader r = Files.newBufferedReader(p)) {          // UTF-8 default
    String line;
    while ((line = r.readLine()) != null) { process(line); }
}

try (BufferedWriter w = Files.newBufferedWriter(p, StandardOpenOption.CREATE, StandardOpenOption.APPEND)) {
    w.write("event");
    w.newLine();
}

// As a Stream<String>: MUST be closed
try (Stream<String> lines = Files.lines(p)) {
    long errors = lines.filter(l -> l.contains("ERROR")).count();
}
```

`Files.lines`, `Files.list`, `Files.walk`, and `Files.find` return streams backed by an open file or directory handle. **Always wrap them in try-with-resources**, or the handle leaks. A decoding or I/O failure *during* the stream surfaces as an unchecked `UncheckedIOException`.

For raw bytes: `Files.newInputStream(p)` and `Files.newOutputStream(p, options...)`.

---

## 4. Managing files and directories

```java
Files.exists(p);                  Files.notExists(p);          // NOT complements: both false if inaccessible
Files.isRegularFile(p);           Files.isDirectory(p);
Files.isReadable(p);              Files.isWritable(p);
Files.size(p);

Files.createDirectory(dir);                    // fails if parent missing or dir exists
Files.createDirectories(dir);                  // like mkdir -p; no error if it already exists
Files.createFile(p);                           // fails if it exists

Files.copy(src, dst);                                            // fails if dst exists
Files.copy(src, dst, StandardCopyOption.REPLACE_EXISTING);
Files.move(src, dst, StandardCopyOption.ATOMIC_MOVE);            // all-or-nothing rename (same file system)
Files.delete(p);                               // throws NoSuchFileException if missing
Files.deleteIfExists(p);                       // returns boolean
```

Things that surprise people:

- **`Files.delete` on a non-empty directory throws `DirectoryNotEmptyException`.** There is no built-in recursive delete (see section 5).
- `Files.copy` of a directory copies **only the directory itself**, not its contents.
- `Files.exists` followed by an action is a race (the file may change in between). Prefer to *try the operation* and handle `NoSuchFileException` / `FileAlreadyExistsException`, or use `CREATE_NEW`.
- `ATOMIC_MOVE` may throw `AtomicMoveNotSupportedException` if source and target are on different file systems.

### Temp files

```java
Path tmp = Files.createTempFile("upload-", ".tmp");
Path tmpDir = Files.createTempDirectory("work-");
tmp.toFile().deleteOnExit();      // or delete explicitly in finally
```

---

## 5. Listing and walking directories

```java
// One level
try (Stream<Path> entries = Files.list(dir)) {
    entries.filter(Files::isRegularFile).forEach(System.out::println);
}

// Whole tree (depth-first)
try (Stream<Path> tree = Files.walk(dir)) {
    List<Path> javaFiles = tree.filter(f -> f.toString().endsWith(".java")).toList();
}

// Limit depth; filter with attributes (can be more efficient)
try (Stream<Path> found = Files.find(dir, 3, (path, attrs) -> attrs.isRegularFile() && attrs.size() > 1_000_000)) {
    found.forEach(System.out::println);
}

// Glob patterns for a single directory
try (DirectoryStream<Path> ds = Files.newDirectoryStream(dir, "*.{csv,txt}")) {
    for (Path f : ds) { ... }
}
```

- `Files.list` is **not** recursive and returns entries in no guaranteed order.
- `Files.walk` does **not follow symlinks** by default (pass `FileVisitOption.FOLLOW_LINKS` to change that, with loop detection).

### Recursive delete

```java
try (Stream<Path> tree = Files.walk(root)) {
    tree.sorted(Comparator.reverseOrder())      // children before parents
        .forEach(p -> {
            try { Files.delete(p); }
            catch (IOException e) { throw new UncheckedIOException(e); }
        });
}
```

When you need per-file error handling, pre/post-visit hooks, or to skip subtrees, use `Files.walkFileTree` with a `SimpleFileVisitor`:

```java
Files.walkFileTree(root, new SimpleFileVisitor<>() {
    @Override public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
        Files.delete(file);
        return FileVisitResult.CONTINUE;
    }
    @Override public FileVisitResult postVisitDirectory(Path dir, IOException e) throws IOException {
        Files.delete(dir);
        return FileVisitResult.CONTINUE;
    }
});
```

---

## 6. Attributes, permissions, symlinks

```java
BasicFileAttributes a = Files.readAttributes(p, BasicFileAttributes.class);
a.size(); a.creationTime(); a.lastModifiedTime(); a.isDirectory(); a.isSymbolicLink();

FileTime modified = Files.getLastModifiedTime(p);
Instant when = modified.toInstant();                    // → java.time ([Date and Time](../10-date-and-time/00_java-time-overview.md))

// POSIX permissions (not available on every file system, e.g. plain Windows NTFS)
Files.setPosixFilePermissions(p, PosixFilePermissions.fromString("rw-------"));

Files.isSymbolicLink(link);
Files.readSymbolicLink(link);
Files.readAttributes(link, BasicFileAttributes.class, LinkOption.NOFOLLOW_LINKS);
```

Most `Files` methods **follow symbolic links** unless you pass `LinkOption.NOFOLLOW_LINKS`. Reading attributes in one call is cheaper than calling `size()`, `isDirectory()`, and so on separately, which each hit the file system.

---

## 7. `java.io.File` interop

```java
Path path = file.toPath();
File file = path.toFile();
```

`File` is still found in older APIs. Its main weaknesses are the ones `Files` fixes: methods like `delete()` and `mkdir()` return `false` without saying why, no symlink support, and weak metadata handling.

---

## 8. Beyond the basics (briefly)

- **Channels and buffers** (`FileChannel`, `ByteBuffer`): block-oriented, supports positional reads/writes, `transferTo`/`transferFrom`, locking ([File Processing Patterns](02_file-processing-patterns.md)), and memory-mapped files via `FileChannel.map`. Worth it for random access or very large binary files, not for ordinary text.
- **`WatchService`**: get notified of create/modify/delete in a directory. Behavior and latency depend on the platform, and events can be coalesced or dropped (`OVERFLOW`), so treat events as hints to rescan rather than as a precise log.
- **`FileSystem` / `FileSystems`**: pluggable file systems; for example a zip file can be opened as a `FileSystem` and navigated with the same `Path`/`Files` API.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Forgetting to close `Files.lines/list/walk/find` streams | try-with-resources |
| `Files.readString`/`readAllLines` on huge files | Stream with `newBufferedReader` or `Files.lines` |
| Assuming `Files.write*` appends | It truncates by default. Use `APPEND` |
| Checking `exists()` then acting | Act and catch the specific exception (or `CREATE_NEW`) |
| `Files.delete` on a non-empty directory | Walk and delete children first |
| `base.resolve(userInput)` without validation | `normalize()` + `startsWith(base)` check |
| Comparing paths with `equals` to test "same file" | `Files.isSameFile` |
| Relying on `~` or relative paths in services | Absolute paths from configuration |
| `Files.copy` replacing silently / failing unexpectedly | Be explicit with `StandardCopyOption` |

### Debugging

- Know your exceptions: `NoSuchFileException` (missing), `FileAlreadyExistsException`, `AccessDeniedException`, `DirectoryNotEmptyException`, `NotDirectoryException`, `FileSystemException` (generic OS reason). The message contains the path.
- "File not found" for a relative path → print `path.toAbsolutePath()`. The working directory of an IDE, a test runner, and a service differ.
- Resources inside a JAR can't be opened as `Path`s on the normal file system. Use `getResourceAsStream` ([File Processing Patterns](02_file-processing-patterns.md)).
- `MalformedInputException` → the file isn't valid UTF-8. Fix the data's encoding, or pass the correct charset.

---

## Quick Summary

- `Path` is a name (immutable, may not exist). `Files` does the work. Use them instead of `File`.
- `Files.readString/writeString/readAllLines` for small files; `newBufferedReader`/`Files.lines` for large ones.
- Streams from `Files.lines/list/walk/find` **must be closed**.
- Writes **truncate by default**. Use `APPEND` or `CREATE_NEW` deliberately.
- `resolve` with an absolute argument replaces the base. Validate external path input with `normalize()` + `startsWith`.
- Prefer attempting an operation and handling its exception over check-then-act.

**Next:** [File Processing Patterns](02_file-processing-patterns.md)
