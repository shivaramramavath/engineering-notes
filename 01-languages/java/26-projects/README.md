# Module 26: Production-Grade Projects in Java

> 15 complete, standalone, production-grade applications and systems implemented from scratch in pure modern Java (SE 21 / 25 LTS).

---

## Module Overview

Module 26 demonstrates how the principles, patterns, and language features mastered throughout the Java track coalesce into complete, runnable, production-quality systems. Every project in this module adheres strictly to the authoritative [`project-template.md`](../_templates/project-template.md):

- **Zero Third-Party Runtime Bloat**: Implemented with pure Java standard library capabilities (NIO, Virtual Threads, Concurrency, Class-File API, Cryptography).
- **Architectural Diagrams & Component Decoupling**: Clean separation of Domain Models, Engine/Service logic, Infrastructure/Storage, and runnable CLI harnesses.
- **Complete Maven `pom.xml` & JUnit 5 Test Suites**: Automated tests with mock environments, assertions, and verification outputs.
- **Target Line Count**: 400–600 lines per project note with zero placeholders or omissions.

---

## Complete Project Catalog

| # | Project Name | Slug | Complexity | Key Architectural Patterns & Technologies | Note Link |
| :-: | :--- | :--- | :--- | :--- | :--- |
| **01** | **High-Throughput HTTP/1.1 Server** | `http-server` | Advanced | Virtual-Thread-Per-Task, SocketChannel, RFC 9112 Parser, Routing Trie | [`01_http-server.md`](01_http-server.md) |
| **02** | **Persistent LSM-Tree Key-Value Store** | `key-value-store` | Advanced | LSM-Tree, MemTable (`ConcurrentSkipListMap`), WAL, SSTable, Bloom Filter | [`02_key-value-store.md`](02_key-value-store.md) |
| **03** | **Distributed Rate Limiting Gateway** | `rate-limiter` | Advanced | Lock-Free Token Bucket, Sliding Window Counter, RFC 6585 Headers | [`03_rate-limiter.md`](03_rate-limiter.md) |
| **04** | **RFC 8259 JSON Parser & Serializer** | `json-parser` | Advanced | Compiler Lexer, Recursive-Descent Parser, Sealed AST Interfaces | [`04_json-parser.md`](04_json-parser.md) |
| **05** | **In-Memory Search Engine with BM25** | `search-engine` | Advanced | Inverted Index, Okapi BM25 Ranking, Tokenizer, Stopword Pruning | [`05_search-engine.md`](05_search-engine.md) |
| **06** | **Distributed Task Scheduler & Worker Pool** | `task-scheduler` | Advanced | `DelayQueue`, Priority Scheduling, Cron Parsing, Exponential Backoff, DLQ | [`06_task-scheduler.md`](06_task-scheduler.md) |
| **07** | **Bytecode Disassembler & Analyzer** | `classfile-analyzer` | Advanced | Java 24/25 Class-File API (`java.lang.classfile`), Constant Pool, Opcodes | [`07_classfile-analyzer.md`](07_classfile-analyzer.md) |
| **08** | **Mini Relational Database Engine** | `database-engine` | Advanced | SQL Lexer/Parser, Table Catalog, B+Tree Index, Slotted-Page Storage | [`08_database-engine.md`](08_database-engine.md) |
| **09** | **Event-Driven Partitioned Message Broker** | `message-broker` | Advanced | Partitioned Commit Log, Topics, Consumer Group Offsets, Replayability | [`09_message-broker.md`](09_message-broker.md) |
| **10** | **Limit Order Book (LOB) Matching Engine** | `order-matching-engine`| Advanced | High-Frequency Trading, Price-Time Priority (FIFO), Bid/Ask Depth Ladders | [`10_order-matching-engine.md`](10_order-matching-engine.md) |
| **11** | **Raft Distributed Consensus Protocol** | `raft-consensus` | Advanced | Raft Leader Election, Heartbeats, Term Monotonicity, Quorum Consensus | [`11_raft-consensus.md`](11_raft-consensus.md) |
| **12** | **Git Content-Addressable Object Store** | `git-object-store` | Advanced | SHA-1 Hashing, Blobs, Trees, Commits, Zlib Deflate/Inflate Compression | [`12_git-object-store.md`](12_git-object-store.md) |
| **13** | **High-Throughput RESTful API Gateway** | `api-gateway` | Advanced | Path Routing, Cryptographic JWT (HMAC-SHA256), Reverse Proxy Forwarding | [`13_api-gateway.md`](13_api-gateway.md) |
| **14** | **Reactive File Streaming & Compression**| `file-streaming-service`| Advanced | Zero-Copy DMA, Java Flow API Reactive Backpressure, Streaming Gzip | [`14_file-streaming-service.md`](14_file-streaming-service.md) |
| **15** | **Real-Time WebSocket Chat Server** | `realtime-chat-server`| Advanced | RFC 6455 Handshake, Frame Masking, Structured Concurrency, Room Pub/Sub | [`15_realtime-chat-server.md`](15_realtime-chat-server.md) |

---

## Technology Matrix & Architectural Highlights

```
                                    Virtual  Zero-Copy  Sealed    Custom   Crypto /
Project                             Threads  / Buffers  Records   Parser   Hashing
-------------------------------------------------------------------------------------
01. High-Throughput HTTP Server        X        X          X        X         -
02. LSM-Tree Key-Value Store           -        X          X        -         X (CRC32)
03. Rate Limiting Gateway              -        -          X        -         -
04. RFC 8259 JSON Parser               -        -          X        X         -
05. In-Memory Search Engine (BM25)     -        -          X        X         -
06. Distributed Task Scheduler         X        -          X        -         -
07. Class-File Bytecode Analyzer       -        X          X        -         -
08. Relational Database Engine         -        X          X        X         -
09. Partitioned Message Broker         -        X          X        -         -
10. Limit Order Book Matching Engine   -        -          X        -         -
11. Raft Distributed Consensus         X        -          X        -         -
12. Git Object Store                   -        X          X        -         X (SHA-1)
13. RESTful API Gateway                X        -          X        -         X (HMAC)
14. Reactive File Streaming Daemon     X        X          -        -         -
15. Real-Time WebSocket Chat Server    X        X          X        X         X (SHA-1)
```
