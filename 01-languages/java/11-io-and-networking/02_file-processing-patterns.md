# File Processing Patterns

Knowing the APIs ([streams](00_io-streams-readers-writers.md), [NIO](01_nio-paths-and-files.md)) is the easy part. Real file code has to survive **big inputs, bad data, crashes mid-write, concurrent writers, and untrusted archives**. This note collects the patterns that handle those cases.

**Prerequisites:** [I/O Streams, Readers and Writers](00_io-streams-readers-writers.md), [NIO: Paths and Files](01_nio-paths-and-files.md).

---

## 1. Pick a strategy by input size

| Input | Approach |
|---|---|
| Small (config, a few KB-MB) | `Files.readString` / `readAllLines` |
| Large text, record-by-record | Stream: `newBufferedReader` or `Files.lines` |
| Large binary | `InputStream` + `byte[]` buffer, or `FileChannel` |
| Structured (JSON/XML/CSV) and big | A **streaming parser**, not "load the whole document" |
| Output of unknown size | Write as you go through a `BufferedWriter`/`OutputStream`; don't build one giant `String` |

The mistake to avoid is `readAllLines()` on a file that is "usually small". It works until the day someone uploads 2 GB, and then fails with `OutOfMemoryError` in production.

---

## 2. Streaming line processing

### Count or aggregate with constant memory

```java
try (Stream<String> lines = Files.lines(logFile)) {
    Map<String, Long> byLevel = lines
        .map(l -> l.split(" ", 3))
        .filter(parts -> parts.length >= 2)
        .collect(Collectors.groupingBy(parts -> parts[1], Collectors.counting()));
}
```

Memory stays flat for the file, but the **result map** still grows with the number of distinct keys. Aggregating "word → count" across a huge file can still be large.

### Batch and tolerate bad lines

Real input contains malformed records. Decide the policy up front: fail fast, skip and report, or quarantine.

```java
int lineNo = 0, bad = 0;
List<Order> batch = new ArrayList<>(1_000);

try (BufferedReader r = Files.newBufferedReader(input);
     BufferedWriter rejects = Files.newBufferedWriter(rejectFile)) {

    r.readLine();                                 // skip header
    String line;
    while ((line = r.readLine()) != null) {
        lineNo++;
        try {
            batch.add(parse(line));
        } catch (IllegalArgumentException e) {
            bad++;
            rejects.write(lineNo + "\t" + e.getMessage() + "\t" + line);
            rejects.newLine();
            continue;
        }
        if (batch.size() == 1_000) { save(batch); batch.clear(); }   // bounded memory, bulk writes
    }
    if (!batch.isEmpty()) save(batch);
}
```

Keep the **line number** in errors, since "bad row somewhere in 5 million" is not debuggable. Batching suits DB inserts (see [JDBC Patterns](../16-jdbc-and-databases/06_jdbc-patterns.md)).

### CSV caution

`line.split(",")` breaks on quoted fields (`"Smith, John"`), embedded newlines, and escaped quotes. For anything beyond trivial machine-generated CSV, use a real CSV library (Apache Commons CSV, OpenCSV, Jackson CSV) and stream records from it.

### Parallelism

`BufferedReader.lines().parallel()` splits poorly because the source is sequential. If per-line work is expensive, read lines sequentially and hand batches to an executor ([Executors](../14-concurrency/10_executors-and-thread-pools.md)). Measure first ([Parallel Streams](../09-functional-java/07_parallel-streams.md)). Disk speed is often the bottleneck, not the CPU.

---

## 3. Writing safely: atomic replace

A plain `Files.write(target, data)` truncates `target` first. If the process crashes halfway (or the disk fills), readers see a **partial or empty file**. The standard fix is write-to-temp-then-rename:

```java
static void writeAtomically(Path target, String content) throws IOException {
    Path dir = target.toAbsolutePath().getParent();
    Path tmp = Files.createTempFile(dir, "tmp-", ".part");     // same directory → same file system
    try {
        Files.writeString(tmp, content);
        Files.move(tmp, target, StandardCopyOption.ATOMIC_MOVE);
    } catch (IOException | RuntimeException e) {
        Files.deleteIfExists(tmp);
        throw e;
    }
}
```

Why it works: a rename within one file system is atomic, so readers see either the old complete file or the new complete file, never a mixture.

Details:

- The temp file must be on the **same file system** as the target (hence same directory), or `ATOMIC_MOVE` fails.
- When the target already exists, the `Files.move` javadoc says whether it is replaced with `ATOMIC_MOVE` is *implementation-specific*. On common platforms it is replaced, but test on yours.
- **Durability** (surviving power loss) needs more. `Files.write` returns after the OS accepted the data, not after it hit disk. To force it:

```java
try (FileChannel ch = FileChannel.open(tmp, StandardOpenOption.WRITE)) {
    ch.force(true);        // fsync the file's content and metadata before the rename
}
```

Use this for data you can't regenerate (state files, checkpoints). It costs real latency, so don't do it for logs.

For a "create only if it doesn't exist" guarantee (idempotent job markers, lock-ish files), use `Files.createFile` or `CREATE_NEW`. Both fail atomically with `FileAlreadyExistsException`.

---

## 4. File locking

To stop two processes from writing the same file, or to ensure one instance of a job runs:

```java
try (FileChannel ch = FileChannel.open(Path.of("job.lock"),
                                       StandardOpenOption.CREATE, StandardOpenOption.WRITE);
     FileLock lock = ch.tryLock()) {                 // null if another process holds it

    if (lock == null) {
        System.out.println("Another instance is running");
        return;
    }
    runJob();
}   // closing releases the lock
```

What to know:

- Locks are held **on behalf of the whole JVM**, not a thread. A second lock request for an overlapping region in the same JVM throws `OverlappingFileLockException`.
- On some platforms locks are **advisory**: they only stop other processes that *also* ask for the lock. They are unreliable on network file systems.
- If you only need "one instance", a lock file works, but a database row or distributed lock is sturdier in multi-host deployments.

---

## 5. Compression and archives

### GZIP

```java
// Write
try (OutputStream out = new GZIPOutputStream(Files.newOutputStream(Path.of("data.txt.gz")));
     Writer w = new OutputStreamWriter(out, StandardCharsets.UTF_8)) {
    w.write(bigText);
}

// Read line by line
try (BufferedReader r = new BufferedReader(new InputStreamReader(
         new GZIPInputStream(Files.newInputStream(Path.of("data.txt.gz"))), StandardCharsets.UTF_8))) {
    r.lines().forEach(this::process);
}
```

It is just another decorator in the stack ([streams](00_io-streams-readers-writers.md#4-decorators-stacking-behavior)).

### ZIP extraction: guard against zip-slip

An archive entry named `../../etc/cron.d/evil` will escape your target directory if you naively `resolve` it ("zip slip"). Always validate:

```java
Path destDir = Path.of("/srv/extract").toAbsolutePath().normalize();

try (ZipInputStream zin = new ZipInputStream(Files.newInputStream(zipFile))) {
    ZipEntry entry;
    while ((entry = zin.getNextEntry()) != null) {
        Path out = destDir.resolve(entry.getName()).normalize();
        if (!out.startsWith(destDir)) {
            throw new IOException("Blocked path traversal in entry: " + entry.getName());
        }
        if (entry.isDirectory()) {
            Files.createDirectories(out);
        } else {
            Files.createDirectories(out.getParent());
            Files.copy(zin, out, StandardCopyOption.REPLACE_EXISTING);
        }
    }
}
```

For untrusted archives also cap the **total extracted size and entry count**: a tiny "zip bomb" can expand to terabytes. See [Input Validation and Injection](../21-security/00_input-validation-and-injection.md).

---

## 6. Resources inside your application (classpath)

Files packaged with your code (default config, templates, test data) live on the **classpath** and, once packaged, inside a JAR. They are not files on disk, so `new File("src/main/resources/x")` or `Path.of(...)` breaks outside the IDE. Use the class loader:

```java
try (InputStream in = MyService.class.getResourceAsStream("/config/defaults.properties")) {
    if (in == null) throw new FileNotFoundException("config/defaults.properties not on classpath");

    Properties props = new Properties();
    props.load(new InputStreamReader(in, StandardCharsets.UTF_8));   // Reader overload: you control the charset
}
```

- `Class.getResourceAsStream("/a/b.txt")` with a leading `/` is relative to the classpath root. Without it, relative to the class's package.
- `ClassLoader.getResourceAsStream("a/b.txt")` is always root-relative and has **no** leading slash.
- `Properties.load(InputStream)` decodes ISO-8859-1; load through a `Reader` to use UTF-8.
- Maven/Gradle put `src/main/resources` at the classpath root ([Maven](../18-build-and-dependencies/00_maven.md)).

Runtime configuration that operators change belongs *outside* the JAR ([Configuration Management](../22-production-engineering/01_configuration-management.md)).

---

## 7. Operational patterns

### Process-and-move (pick up files from a directory)

```text
incoming/  ──process──►  processed/        (success)
                    └──►  failed/           (error, with the reason logged)
```

Move with `Files.move(..., ATOMIC_MOVE)` after successful processing so a restart doesn't reprocess or lose files. Producers should write to a temporary name and rename into `incoming/` when complete (the same atomic-rename idea), so your consumer never reads half-written files. Design processing to be safe to repeat: see [Idempotency](../25-real-world-patterns/04_idempotency.md).

### Temp files and cleanup

Create temp files in a known directory, delete in `finally` (or on a schedule). `deleteOnExit()` only runs at normal JVM shutdown and accumulates entries in memory in long-running services.

### Line endings and encoding at boundaries

`readLine()` accepts `\n`, `\r`, and `\r\n`. When *writing* files for other systems, use an explicit separator (`"\r\n"`) instead of `newLine()`, which uses the platform's. Declare and document the charset (UTF-8) in every file contract.

---

## Common mistakes

| Mistake | Fix |
|---|---|
| Loading whole large files into memory | Stream; use bounded batches |
| Truncating the live file while writing it | Write temp → `ATOMIC_MOVE` |
| Assuming `Files.write` means data is on disk | `FileChannel.force(true)` where durability matters |
| `split(",")` for real-world CSV | CSV library |
| Skipping bad rows silently | Count, log with line number, quarantine |
| Resolving zip entry names without validation | `normalize()` + `startsWith(destDir)`; cap size |
| Using file paths for resources inside the JAR | `getResourceAsStream` |
| Assuming file locks work across NFS / protect against non-cooperating processes | Treat as advisory; use a DB/distributed lock if needed |
| Consumer reading files while producer is still writing them | Write-to-temp-then-rename convention |
| Leaving `.tmp` files behind after failures | `finally` cleanup |

### Debugging

- `OutOfMemoryError` while reading → find the call that materializes the file (`readAllBytes`, `readAllLines`, `collect(toList())` of lines) and stream instead.
- Corrupt or half-written output after a crash → switch to atomic replace; check whether anything reads the file concurrently.
- `AtomicMoveNotSupportedException` → temp file is on another file system or volume; create it beside the target.
- `OverlappingFileLockException` → the same JVM already holds the lock. Look for a second code path or thread.
- First CSV column "missing" from a map lookup → a UTF-8 BOM prefixed the header ([streams](00_io-streams-readers-writers.md#6-charsets-the-usual-source-of-garbled-text)).

---

## Quick Summary

- Choose streaming vs read-all by the **worst-case** input size, not the typical one.
- Process large files record-by-record, in bounded batches, with a clear bad-record policy and line numbers in errors.
- **Atomic replace** (temp file in the same directory + `ATOMIC_MOVE`) prevents partial files. Add `force(true)` when durability matters.
- File locks coordinate cooperating processes only. They are advisory and unreliable over network file systems.
- Validate archive entry paths (zip-slip) and limit extracted size.
- Resources inside the JAR come from `getResourceAsStream`, not file paths.

**Next:** [Java Serialization](03_java-serialization.md)
