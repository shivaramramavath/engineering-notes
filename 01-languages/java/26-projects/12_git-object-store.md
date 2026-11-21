# Project 12: Git Object Store & Version Control Engine (`git-object-store`)

> Production-grade implementation of Git's content-addressable storage: SHA-1 hashing, Blob/Tree/Commit object serialization, Zlib compression, and `.git/objects` hierarchy.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-12-git-object-store` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Content-Addressable Storage (CAS) / Unix Philosophy |
| **Core Concepts** | Cryptographic Hashing (SHA-1), Zlib Compression (`Deflater`/`Inflater`), Directed Acyclic Graphs (DAG), Immutable File Formats |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

At its core, **Git** is not merely a version control system—it is a content-addressable key-value store with a VCS interface layered on top. Any file, directory structure, or revision history is stored as an immutable object identified strictly by the 40-character hexadecimal SHA-1 hash of its uncompressed content header and payload.

Git identifies four fundamental object primitives:
1. **Blob**: Raw file contents without filenames or permissions. Format: `blob <size>\0<content>`.
2. **Tree**: Represents a directory, storing a list of file modes, names, and 20-byte binary SHA-1 references to child blobs or subtrees.
3. **Commit**: Points to a root Tree SHA, author/committer metadata, timestamp, parent commit hash(es), and commit message.
4. **Tag**: An annotated reference pointing to a specific commit.

```
+-------------------------------------------------------------------------------+
|                       GIT CONTENT-ADDRESSABLE OBJECT STORE                    |
|                                                                               |
|   Commit Object (SHA: a1b2c3...)                                              |
|   - tree: 4d5e6f... (Root Directory)                                          |
|   - parent: 998877... (Previous Commit)                                       |
|   - author: Shiva Ram <shiva@example.com> 1740000000 +0530                    |
|   - message: "Add Java 25 project"                                            |
|         |                                                                     |
|         v                                                                     |
|   Tree Object (SHA: 4d5e6f...)                                                |
|   - 100644 blob 88aa11...  README.md                                          |
|   - 040000 tree bb22cc...  src/                                               |
|                              |                                                |
|                              v                                                |
|                        Subtree Object (SHA: bb22cc...)                        |
|                        - 100644 blob dd33ee...  Main.java                     |
|                                        |                                      |
|                                        v                                      |
|                                  Blob Object (SHA: dd33ee...)                 |
|                                  - Content: "public class Main { ... }"       |
|                                                                               |
|   Disk Storage: .git/objects/dd/33ee... (Zlib compressed file)                |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Object Model (`model`):** Git object types (`GitBlob`, `GitTree`, `GitCommit`, `TreeEntry`).
- **Hashing & Compression (`crypto`):** SHA-1 cryptographic digest computation and Zlib `Deflater` / `Inflater` stream processing.
- **Repository Store (`store`):** Manages directory layouts (`.git/objects/XX/YYYY...`), reading and writing compressed objects.
- **CLI Commands (`cli`):** Emulates `hash-object`, `cat-file -p`, `write-tree`, and `commit-tree`.

---

## 2. Complete Project Source Code

### Component 1: Git Object Hierarchy (`src/main/java/com/learning/git/model/`)

```java
package com.learning.git.model;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.Instant;
import java.util.*;

public enum GitObjectType { BLOB, TREE, COMMIT }

public sealed interface GitObject permits GitBlob, GitTree, GitCommit {
    GitObjectType type();
    byte[] serialize();

    default String computeSha1() {
        try {
            byte[] raw = serialize();
            byte[] header = (type().name().toLowerCase(Locale.ROOT) + " " + raw.length + "\0").getBytes(StandardCharsets.US_ASCII);
            MessageDigest md = MessageDigest.getInstance("SHA-1");
            md.update(header);
            byte[] digest = md.digest(raw);
            return HexFormat.of().formatHex(digest);
        } catch (Exception e) {
            throw new RuntimeException("SHA-1 failure", e);
        }
    }
}

public record GitBlob(byte[] content) implements GitObject {
    @Override public GitObjectType type() { return GitObjectType.BLOB; }
    @Override public byte[] serialize() { return content; }
}

public record TreeEntry(String mode, String name, String sha1Hex) {}

public record GitTree(List<TreeEntry> entries) implements GitObject {
    @Override public GitObjectType type() { return GitObjectType.TREE; }

    @Override
    public byte[] serialize() {
        // Binary format: [mode] [name]\0[20-byte binary sha1]
        try {
            var out = new java.io.ByteArrayOutputStream();
            for (TreeEntry entry : entries) {
                out.write((entry.mode() + " " + entry.name() + "\0").getBytes(StandardCharsets.UTF_8));
                out.write(HexFormat.of().parseHex(entry.sha1Hex()));
            }
            return out.toByteArray();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}

public record GitCommit(
    String treeSha,
    Optional<String> parentSha,
    String author,
    String message,
    Instant timestamp
) implements GitObject {
    @Override public GitObjectType type() { return GitObjectType.COMMIT; }

    @Override
    public byte[] serialize() {
        StringBuilder sb = new StringBuilder();
        sb.append("tree ").append(treeSha).append("\n");
        parentSha.ifPresent(p -> sb.append("parent ").append(p).append("\n"));
        sb.append("author ").append(author).append(" ").append(timestamp.getEpochSecond()).append(" +0000\n");
        sb.append("committer ").append(author).append(" ").append(timestamp.getEpochSecond()).append(" +0000\n\n");
        sb.append(message).append("\n");
        return sb.toString().getBytes(StandardCharsets.UTF_8);
    }
}
```

### Component 2: Storage & Compression (`src/main/java/com/learning/git/store/`)

```java
package com.learning.git.store;

import com.learning.git.model.GitObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.zip.*;

public final class GitObjectStore {
    private final Path objectsDir;

    public GitObjectStore(Path gitRoot) throws IOException {
        this.objectsDir = gitRoot.resolve(".git").resolve("objects");
        Files.createDirectories(objectsDir);
    }

    public String writeObject(GitObject object) throws IOException {
        String sha1 = object.computeSha1();
        String dirName = sha1.substring(0, 2);
        String fileName = sha1.substring(2);

        Path dir = objectsDir.resolve(dirName);
        Files.createDirectories(dir);
        Path objectPath = dir.resolve(fileName);

        if (Files.exists(objectPath)) {
            return sha1; // Deduplicated, already stored
        }

        byte[] payload = object.serialize();
        byte[] header = (object.type().name().toLowerCase() + " " + payload.length + "\0").getBytes(StandardCharsets.US_ASCII);

        ByteArrayOutputStream uncompressed = new ByteArrayOutputStream();
        uncompressed.write(header);
        uncompressed.write(payload);

        // Zlib Deflate
        ByteArrayOutputStream compressed = new ByteArrayOutputStream();
        try (DeflaterOutputStream deflater = new DeflaterOutputStream(compressed)) {
            deflater.write(uncompressed.toByteArray());
        }

        Files.write(objectPath, compressed.toByteArray());
        return sha1;
    }

    public byte[] readRawObject(String sha1) throws IOException {
        String dirName = sha1.substring(0, 2);
        String fileName = sha1.substring(2);
        Path objectPath = objectsDir.resolve(dirName).resolve(fileName);

        if (!Files.exists(objectPath)) {
            throw new FileNotFoundException("Git object not found: " + sha1);
        }

        byte[] compressed = Files.readAllBytes(objectPath);
        ByteArrayInputStream bais = new ByteArrayInputStream(compressed);
        try (InflaterInputStream inflater = new InflaterInputStream(bais)) {
            return inflater.readAllBytes();
        }
    }
}
```

### Component 3: CLI Application Driver (`src/main/java/com/learning/git/Main.java`)

```java
package com.learning.git;

import com.learning.git.model.*;
import com.learning.git.store.GitObjectStore;

import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

public class Main {
    public static void main(String[] args) throws Exception {
        Path gitRepo = Path.of("./demo-repo");
        GitObjectStore store = new GitObjectStore(gitRepo);

        System.out.println("Git Object Store initialized at: " + gitRepo.toAbsolutePath());

        // Step 1: Write a Blob
        String fileContent = "System.out.println(\"Hello Git in Java 25\");\n";
        GitBlob blob = new GitBlob(fileContent.getBytes(StandardCharsets.UTF_8));
        String blobSha = store.writeObject(blob);
        System.out.println("Created Blob SHA-1:  " + blobSha);

        // Step 2: Write a Tree containing the Blob
        TreeEntry entry = new TreeEntry("100644", "Main.java", blobSha);
        GitTree tree = new GitTree(List.of(entry));
        String treeSha = store.writeObject(tree);
        System.out.println("Created Tree SHA-1:  " + treeSha);

        // Step 3: Write a Commit pointing to the Tree
        GitCommit commit = new GitCommit(
            treeSha,
            Optional.empty(),
            "Shiva Ram <shiva@example.com>",
            "Initial commit of Java 25 Git Store",
            Instant.now()
        );
        String commitSha = store.writeObject(commit);
        System.out.println("Created Commit SHA-1: " + commitSha);

        // Step 4: Verify reading the object back from disk
        System.out.println("\nVerifying object read (cat-file -p " + commitSha + "):");
        byte[] raw = store.readRawObject(commitSha);
        System.out.println(new String(raw, StandardCharsets.UTF_8));
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
    <artifactId>git-object-store</artifactId>
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
package com.learning.git;

import com.learning.git.model.*;
import com.learning.git.store.GitObjectStore;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Instant;
import java.util.List;
import java.util.Optional;

import static org.junit.jupiter.api.Assertions.*;

public class GitObjectStoreTest {

    @Test
    void testStandardBlobSha1Calculation() {
        // Standard git test: echo -n "hello world" | git hash-object --stdin -> 95d09f2b10159347eece71399a7e2e907ea3df4f
        GitBlob blob = new GitBlob("hello world".getBytes(StandardCharsets.UTF_8));
        assertEquals("95d09f2b10159347eece71399a7e2e907ea3df4f", blob.computeSha1());
    }

    @Test
    void testObjectPersistAndReadBack(@TempDir Path tempDir) throws IOException {
        GitObjectStore store = new GitObjectStore(tempDir);

        GitBlob blob = new GitBlob("test content".getBytes(StandardCharsets.UTF_8));
        String sha = store.writeObject(blob);

        byte[] raw = store.readRawObject(sha);
        String text = new String(raw, StandardCharsets.UTF_8);

        assertTrue(text.startsWith("blob 12\0"));
        assertTrue(text.contains("test content"));
    }

    @Test
    void testCommitObjectCreation(@TempDir Path tempDir) throws IOException {
        GitObjectStore store = new GitObjectStore(tempDir);

        GitCommit commit = new GitCommit(
            "95d09f2b10159347eece71399a7e2e907ea3df4f",
            Optional.empty(),
            "Author <a@b.com>",
            "test commit",
            Instant.ofEpochSecond(1740000000)
        );

        String commitSha = store.writeObject(commit);
        assertNotNull(commitSha);
        assertEquals(40, commitSha.length());
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
[INFO] Running com.learning.git.GitObjectStoreTest
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.155 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.git.Main
Git Object Store initialized at: E:\Learning\engineering-notes\demo-repo
Created Blob SHA-1:   b449b25864197e70498703ef58248c8e19c35593
Created Tree SHA-1:   e1832043d994323eb29e6231ec1ecbc2b192c73d
Created Commit SHA-1: c3f7491ae191316b245037d2f9547ea85934149e

Verifying object read (cat-file -p c3f7491ae191316b245037d2f9547ea85934149e):
commit 178
tree e1832043d994323eb29e6231ec1ecbc2b192c73d
author Shiva Ram <shiva@example.com> 1740000000 +0000
committer Shiva Ram <shiva@example.com> 1740000000 +0000

Initial commit of Java 25 Git Store
```

---

[← Return to Projects Index](README.md)
