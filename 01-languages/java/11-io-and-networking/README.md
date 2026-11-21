# Module 11: I/O and Networking

> Comprehensive guide to input/output, file processing, object serialization, modern HTTP networking, and sockets in Java.

---

## Module Overview

This module covers foundational and modern I/O and networking mechanisms across the Java ecosystem, from classic streams and character readers to modern high-throughput NIO.2 path channels, memory-safe file transformations, object serialization defenses, and standardized HTTP/2 client networking.

---

## Notes Index

| # | Note | Description | Status |
|---|---|---|---|
| 00 | [00_io-streams-readers-writers.md](00_io-streams-readers-writers.md) | Byte streams, character streams, buffering, decorator pattern, charset encodings | Complete (400+ lines) |
| 01 | [01_nio-paths-and-files.md](01_nio-paths-and-files.md) | NIO.2 Path, Files, directory streams, walkFileTree, memory mapping | Complete (400+ lines) |
| 02 | [02_file-processing-patterns.md](02_file-processing-patterns.md) | Streaming file transformations, zero-allocation line reading, atomic operations | Complete (410+ lines) |
| 03 | [03_java-serialization.md](03_java-serialization.md) | Serializable, Externalizable, serialVersionUID, JEP 290 filters, RCE gadgets | Complete (440+ lines) |
| 04 | [04_http-client-and-networking.md](04_http-client-and-networking.md) | Modern java.net.http.HttpClient, HTTP/2, CompletableFuture, BodyHandlers | Complete (450+ lines) |

---

## Recommended Learning Path
1. Master classic streams and readers in `00_io-streams-readers-writers.md`.
2. Adopt modern NIO.2 APIs in `01_nio-paths-and-files.md` and enterprise processing patterns in `02_file-processing-patterns.md`.
3. Understand security risks and serialization defenses in `03_java-serialization.md`.
4. Build scalable network integrations with `04_http-client-and-networking.md`.
