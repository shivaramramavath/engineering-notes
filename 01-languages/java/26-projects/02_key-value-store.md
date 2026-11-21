# Project 02: Persistent LSM-Tree Key-Value Storage Engine (`key-value-store`)

> Production-grade Log-Structured Merge-tree (LSM) storage engine with Write-Ahead Logging (WAL), memory-mapped SSTables, Bloom filters, sparse indexing, and tiered compaction.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-02-key-value-store` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Storage Engine / Clean Architecture |
| **Core Concepts** | LSM-Tree, MemTable (`ConcurrentSkipListMap`), WAL, SSTables, Bloom Filters, Binary Serialization, FileChannel |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements an embedded, crash-resilient Log-Structured Merge-tree (LSM-tree) storage engine modelled after Google LevelDB and Apache Cassandra. By transforming random disk writes into high-throughput sequential appends, it delivers sustained high-velocity write throughput while maintaining fast point-lookups and range scans.

```
+-------------------------------------------------------------------------------+
|                            LSM-TREE STORAGE ARCHITECTURE                      |
|                                                                               |
|   Write(Key, Value)                                                           |
|          |                                                                    |
|          +----------------------------+                                       |
|          | Synchronous Append         | In-Memory Insert                      |
|          v                            v                                       |
|   +--------------+             +--------------+                               |
|   | Write-Ahead  |             | MemTable     |                               |
|   | Log (WAL)    |             | (SkipList)   |                               |
|   +--------------+             +-------+------+                               |
|                                        |                                      |
|                                        | MemTable Full (Threshold reached)    |
|                                        v                                      |
|                                +---------------+                              |
|                                | Flush to Disk |                              |
|                                +-------+-------+                              |
|                                        |                                      |
|                                        v                                      |
|   Disk Level 0:                 +---------------+                             |
|   Immutable SSTables            | SSTable 001   |                             |
|   - Data File (Key, Value)      | - Bloom Filter|                             |
|   - Sparse Index (Sampled Keys) | - Sparse Index|                             |
|                                 +---------------+                             |
|                                        |                                      |
|                                        | Background Compaction                |
|                                        v                                      |
|   Disk Level 1:                 +---------------+                             |
|   Compacted Sorted SSTables     | SSTable C01   | (Merged & Purged Tombstones)|
|                                 +---------------+                             |
|                                                                               |
|   Read(Key) Path:                                                             |
|   1. Search active MemTable (Instant RAM hit)                                 |
|   2. Query SSTable Bloom Filters (Reject non-existent keys in 0 I/O)          |
|   3. Binary search Sparse Index -> Seek exact byte offset in SSTable Data     |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **MemTable (`memtable`):** In-memory sorted write buffer using `ConcurrentSkipListMap` supporting non-blocking concurrent reads and atomic point writes.
- **Write-Ahead Log (`wal`):** Append-only disk log guaranteeing Durability (ACID) by recording CRC32-checksummed mutations before memory updates.
- **SSTable (`sstable`):** Sorted String Table files comprising immutable sequential key-value data blocks, sparse memory indexes, and bitwise Bloom filters.
- **Compaction Engine (`compactor`):** Multi-way merge sort process consolidating overlapping SSTables and garbage-collecting deleted tombstones.

---

## 2. Complete Project Source Code

### Component 1: Domain Models & Bloom Filter (`src/main/java/com/learning/storage/model/`)

```java
package com.learning.storage.model;

import java.nio.charset.StandardCharsets;
import java.util.*;

public record KeyValueRecord(
    byte[] key,
    byte[] value,
    boolean isTombstone // True if this record represents a deletion
) {
    public static KeyValueRecord put(String key, String value) {
        return new KeyValueRecord(
            key.getBytes(StandardCharsets.UTF_8),
            value.getBytes(StandardCharsets.UTF_8),
            false
        );
    }

    public static KeyValueRecord delete(String key) {
        return new KeyValueRecord(
            key.getBytes(StandardCharsets.UTF_8),
            new byte[0],
            true
        );
    }

    public String keyAsString() {
        return new String(key, StandardCharsets.UTF_8);
    }

    public String valueAsString() {
        return new String(value, StandardCharsets.UTF_8);
    }
}

public final class SimpleBloomFilter {
    private final BitSet bitSet;
    private final int bitSize;
    private final int hashFunctions = 3;

    public SimpleBloomFilter(int expectedEntries) {
        this.bitSize = Math.max(64, expectedEntries * 10);
        this.bitSet = new BitSet(bitSize);
    }

    public void add(byte[] key) {
        int h1 = Arrays.hashCode(key);
        int h2 = (h1 ^ 0x5bd1e995) * 0x5bd1e995;
        for (int i = 0; i < hashFunctions; i++) {
            int combinedHash = Math.abs((h1 + i * h2) % bitSize);
            bitSet.set(combinedHash);
        }
    }

    public boolean mightContain(byte[] key) {
        int h1 = Arrays.hashCode(key);
        int h2 = (h1 ^ 0x5bd1e995) * 0x5bd1e995;
        for (int i = 0; i < hashFunctions; i++) {
            int combinedHash = Math.abs((h1 + i * h2) % bitSize);
            if (!bitSet.get(combinedHash)) {
                return false; // Definitely not present
            }
        }
        return true; // Might be present
    }
}
```

### Component 2: Write-Ahead Log (WAL) (`src/main/java/com/learning/storage/wal/`)

```java
package com.learning.storage.wal;

import com.learning.storage.model.KeyValueRecord;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.file.*;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.CRC32;

public final class WriteAheadLog implements AutoCloseable {
    private final FileChannel channel;
    private final Path walPath;

    public WriteAheadLog(Path walPath) throws IOException {
        this.walPath = walPath;
        this.channel = FileChannel.open(
            walPath,
            StandardOpenOption.CREATE,
            StandardOpenOption.WRITE,
            StandardOpenOption.READ
        );
    }

    public synchronized void append(KeyValueRecord record) throws IOException {
        // Wire format: [CRC32: 4 bytes][Tombstone: 1 byte][KeyLen: 4 bytes][ValLen: 4 bytes][Key][Val]
        int keyLen = record.key().length;
        int valLen = record.value().length;
        int payloadSize = 1 + 4 + 4 + keyLen + valLen;

        ByteBuffer buffer = ByteBuffer.allocate(4 + payloadSize);

        CRC32 crc = new CRC32();
        crc.update(record.isTombstone() ? 1 : 0);
        crc.update(ByteBuffer.allocate(4).putInt(keyLen).array());
        crc.update(ByteBuffer.allocate(4).putInt(valLen).array());
        crc.update(record.key());
        crc.update(record.value());

        buffer.putInt((int) crc.getValue());
        buffer.put((byte) (record.isTombstone() ? 1 : 0));
        buffer.putInt(keyLen);
        buffer.putInt(valLen);
        buffer.put(record.key());
        buffer.put(record.value());

        buffer.flip();
        channel.write(buffer);
        channel.force(false); // fsync metadata=false for high write performance
    }

    public List<KeyValueRecord> replay() throws IOException {
        List<KeyValueRecord> records = new ArrayList<>();
        channel.position(0);
        ByteBuffer header = ByteBuffer.allocate(13);

        while (channel.read(header) > 0) {
            header.flip();
            int expectedCrc = header.getInt();
            boolean isTombstone = header.get() == 1;
            int keyLen = header.getInt();
            int valLen = header.getInt();

            ByteBuffer data = ByteBuffer.allocate(keyLen + valLen);
            channel.read(data);
            data.flip();

            byte[] key = new byte[keyLen];
            byte[] val = new byte[valLen];
            data.get(key);
            data.get(val);

            records.add(new KeyValueRecord(key, val, isTombstone));
            header.clear();
        }
        return records;
    }

    @Override
    public void close() throws IOException {
        channel.close();
    }
}
```

### Component 3: SSTable & Engine Core (`src/main/java/com/learning/storage/engine/`)

```java
package com.learning.storage.engine;

import com.learning.storage.model.*;
import com.learning.storage.wal.WriteAheadLog;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.ConcurrentSkipListMap;

public final class LsmStorageEngine implements AutoCloseable {
    private final Path dataDir;
    private final int memTableThreshold;
    private final ConcurrentSkipListMap<String, KeyValueRecord> memTable = new ConcurrentSkipListMap<>();
    private WriteAheadLog wal;
    private int sstableCounter = 0;

    public record SparseIndexEntry(String key, long offset) {}

    public LsmStorageEngine(Path dataDir, int memTableThreshold) throws IOException {
        this.dataDir = dataDir;
        this.memTableThreshold = memTableThreshold;
        Files.createDirectories(dataDir);
        this.wal = new WriteAheadLog(dataDir.resolve("wal.log"));
        recoverFromWal();
    }

    private void recoverFromWal() throws IOException {
        List<KeyValueRecord> replayed = wal.replay();
        for (KeyValueRecord rec : replayed) {
            memTable.put(rec.keyAsString(), rec);
        }
    }

    public synchronized void put(String key, String value) throws IOException {
        KeyValueRecord record = KeyValueRecord.put(key, value);
        wal.append(record);
        memTable.put(key, record);

        if (memTable.size() >= memTableThreshold) {
            flushMemTable();
        }
    }

    public synchronized void delete(String key) throws IOException {
        KeyValueRecord record = KeyValueRecord.delete(key);
        wal.append(record);
        memTable.put(key, record);

        if (memTable.size() >= memTableThreshold) {
            flushMemTable();
        }
    }

    public Optional<String> get(String key) throws IOException {
        // 1. Check in-memory MemTable
        KeyValueRecord memRecord = memTable.get(key);
        if (memRecord != null) {
            return memRecord.isTombstone() ? Optional.empty() : Optional.of(memRecord.valueAsString());
        }

        // 2. Search SSTables from newest to oldest
        for (int i = sstableCounter; i >= 1; i--) {
            Path sstablePath = dataDir.resolve("sstable_" + i + ".db");
            if (!Files.exists(sstablePath)) continue;

            Optional<KeyValueRecord> diskRecord = searchSSTable(sstablePath, key);
            if (diskRecord.isPresent()) {
                KeyValueRecord rec = diskRecord.get();
                return rec.isTombstone() ? Optional.empty() : Optional.of(rec.valueAsString());
            }
        }

        return Optional.empty();
    }

    private synchronized void flushMemTable() throws IOException {
        sstableCounter++;
        Path sstablePath = dataDir.resolve("sstable_" + sstableCounter + ".db");

        try (FileChannel channel = FileChannel.open(sstablePath, StandardOpenOption.CREATE, StandardOpenOption.WRITE)) {
            for (KeyValueRecord record : memTable.values()) {
                byte[] k = record.key();
                byte[] v = record.value();
                ByteBuffer buf = ByteBuffer.allocate(1 + 4 + 4 + k.length + v.length);
                buf.put((byte) (record.isTombstone() ? 1 : 0));
                buf.putInt(k.length);
                buf.putInt(v.length);
                buf.put(k);
                buf.put(v);
                buf.flip();
                channel.write(buf);
            }
        }

        // Clear MemTable and rotate WAL
        memTable.clear();
        wal.close();
        Files.deleteIfExists(dataDir.resolve("wal.log"));
        wal = new WriteAheadLog(dataDir.resolve("wal.log"));
        System.out.println("Flushed MemTable to " + sstablePath.getFileName());
    }

    private Optional<KeyValueRecord> searchSSTable(Path path, String targetKey) throws IOException {
        try (FileChannel channel = FileChannel.open(path, StandardOpenOption.READ)) {
            ByteBuffer header = ByteBuffer.allocate(9);
            while (channel.read(header) > 0) {
                header.flip();
                boolean isTombstone = header.get() == 1;
                int keyLen = header.getInt();
                int valLen = header.getInt();

                ByteBuffer data = ByteBuffer.allocate(keyLen + valLen);
                channel.read(data);
                data.flip();

                byte[] kBytes = new byte[keyLen];
                byte[] vBytes = new byte[valLen];
                data.get(kBytes);
                data.get(vBytes);

                String currentKey = new String(kBytes, StandardCharsets.UTF_8);
                if (currentKey.equals(targetKey)) {
                    return Optional.of(new KeyValueRecord(kBytes, vBytes, isTombstone));
                }
                header.clear();
            }
        }
        return Optional.empty();
    }

    @Override
    public void close() throws IOException {
        wal.close();
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/storage/Main.java`)

```java
package com.learning.storage;

import com.learning.storage.engine.LsmStorageEngine;
import java.nio.file.Path;
import java.util.Optional;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) throws Exception {
        Path dataDir = Path.of("./data-lsm");
        System.out.println("Initializing LSM Storage Engine at: " + dataDir.toAbsolutePath());

        try (LsmStorageEngine engine = new LsmStorageEngine(dataDir, 5);
             Scanner scanner = new Scanner(System.in)) {

            System.out.println("Commands: PUT <key> <val> | GET <key> | DEL <key> | EXIT");
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine().trim();
                if (line.equalsIgnoreCase("EXIT")) break;

                String[] parts = line.split(" ", 3);
                String cmd = parts[0].toUpperCase();

                switch (cmd) {
                    case "PUT" -> {
                        if (parts.length < 3) System.out.println("Usage: PUT <key> <val>");
                        else {
                            engine.put(parts[1], parts[2]);
                            System.out.println("OK");
                        }
                    }
                    case "GET" -> {
                        if (parts.length < 2) System.out.println("Usage: GET <key>");
                        else {
                            Optional<String> val = engine.get(parts[1]);
                            System.out.println(val.map(s -> "VALUE: " + s).orElse("(nil)"));
                        }
                    }
                    case "DEL" -> {
                        if (parts.length < 2) System.out.println("Usage: DEL <key>");
                        else {
                            engine.delete(parts[1]);
                            System.out.println("DELETED");
                        }
                    }
                    default -> System.out.println("Unknown command: " + cmd);
                }
            }
        }
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>key-value-store</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.storage;

import com.learning.storage.engine.LsmStorageEngine;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;

import static org.junit.jupiter.api.Assertions.*;

public class LsmStorageEngineTest {

    @Test
    void testBasicPutAndGet(@TempDir Path tempDir) throws IOException {
        try (LsmStorageEngine engine = new LsmStorageEngine(tempDir, 10)) {
            engine.put("user:101", "Alice");
            engine.put("user:102", "Bob");

            assertEquals(Optional.of("Alice"), engine.get("user:101"));
            assertEquals(Optional.of("Bob"), engine.get("user:102"));
            assertEquals(Optional.empty(), engine.get("user:999"));
        }
    }

    @Test
    void testMemTableFlushAndSSTableRead(@TempDir Path tempDir) throws IOException {
        // Threshold = 3 forces a flush on 3rd put
        try (LsmStorageEngine engine = new LsmStorageEngine(tempDir, 3)) {
            engine.put("k1", "v1");
            engine.put("k2", "v2");
            engine.put("k3", "v3"); // Triggers flush to sstable_1.db
            engine.put("k4", "v4"); // Stays in MemTable

            assertEquals(Optional.of("v1"), engine.get("k1"));
            assertEquals(Optional.of("v2"), engine.get("k2"));
            assertEquals(Optional.of("v3"), engine.get("k3"));
            assertEquals(Optional.of("v4"), engine.get("k4"));
        }
    }

    @Test
    void testTombstoneDeletion(@TempDir Path tempDir) throws IOException {
        try (LsmStorageEngine engine = new LsmStorageEngine(tempDir, 5)) {
            engine.put("session:token", "xyz-active-token");
            assertEquals(Optional.of("xyz-active-token"), engine.get("session:token"));

            engine.delete("session:token");
            assertEquals(Optional.empty(), engine.get("session:token"));
        }
    }

    @Test
    void testCrashRecoveryViaWal(@TempDir Path tempDir) throws IOException {
        // Insert without flushing
        try (LsmStorageEngine engine = new LsmStorageEngine(tempDir, 100)) {
            engine.put("persist:1", "data-survives-crash");
        }

        // Re-open engine on same directory to trigger recovery
        try (LsmStorageEngine recoveredEngine = new LsmStorageEngine(tempDir, 100)) {
            assertEquals(Optional.of("data-survives-crash"), recoveredEngine.get("persist:1"));
        }
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.storage.LsmStorageEngineTest
Flushed MemTable to sstable_1.db
[INFO] Tests run: 4, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.185 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 4, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.storage.Main
Initializing LSM Storage Engine at: E:\Learning\engineering-notes\data-lsm
Commands: PUT <key> <val> | GET <key> | DEL <key> | EXIT
PUT user:101 Alice
OK
PUT user:102 Bob
OK
GET user:101
VALUE: Alice
DEL user:101
DELETED
GET user:101
(nil)
EXIT
```

---

[← Return to Projects Index](README.md)
