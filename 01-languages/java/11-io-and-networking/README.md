# 11 · I/O and Networking

Almost every real program reads or writes something outside the JVM: files, sockets, HTTP APIs. This module covers how Java does that, from the classic byte/character stream model, through the modern `java.nio.file` API, to serialization and the built-in HTTP client.

## Contents

| # | Note | What you get |
|---|------|--------------|
| 00 | [I/O Streams, Readers and Writers](00_io-streams-readers-writers.md) | Byte vs character streams, decorators, buffering, charsets, closing and flushing |
| 01 | [NIO: Paths and Files](01_nio-paths-and-files.md) | `Path`, the `Files` utility class, directory walking, attributes, channels in brief |
| 02 | [File Processing Patterns](02_file-processing-patterns.md) | Streaming large files, atomic writes, locking, temp files, zip/gzip, classpath resources |
| 03 | [Java Serialization](03_java-serialization.md) | `Serializable`, `serialVersionUID`, `transient`, custom hooks, the security problem, alternatives |
| 04 | [HTTP Client and Networking](04_http-client-and-networking.md) | `java.net.http.HttpClient`, sync/async calls, timeouts, `URI`, plain TCP sockets |

## Suggested path

Read **00 → 01 → 02** in order. Note 03 can be read independently, and **always read its security section**. Note 04 stands alone, but uses `Duration` ([Date and Time](../10-date-and-time/03_duration-and-period.md)) and `CompletableFuture` ([CompletableFuture](../14-concurrency/12_completablefuture.md)).

## Which API do I reach for?

```text
Read/write a small text file                      → Files.readString / Files.writeString
Read a big text file line by line                 → Files.newBufferedReader or Files.lines (in try-with-resources)
Read/write bytes with a library or network API    → InputStream / OutputStream
Work with paths, directories, copy/move/delete    → Path + Files
Call an HTTP API                                  → java.net.http.HttpClient
Raw TCP connection                                → Socket / ServerSocket
Persist objects                                   → JSON/Protobuf, not Java serialization
```

## Prerequisites

[Exceptions](../06-exceptions-and-debugging/README.md) (especially [try-with-resources](../06-exceptions-and-debugging/03_try-with-resources.md)), and [Strings and Text](../03-strings-and-text/README.md), since encoding errors are the most common I/O bug.

**Next module:** [Modern Java](../12-modern-java/README.md)
