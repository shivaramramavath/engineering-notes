# Module 11: Production Projects in C

> Build 14 end-to-end, production-grade systems in C: UNIX shells, event-driven HTTP servers, persistent KV stores, custom heap allocators, recursive-descent JSON parsers, grep engines, work-stealing thread pools, LRU caches, bytecode virtual machines, and Huffman compression utilities.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `11-projects` |
| **Prerequisites** | Modules 00 through 10 (Full Curriculum Foundation) |
| **Duration** | ~80 hours |
| **Status** | Complete |
| **Standard Baseline**| C11 / C17 / POSIX.1-2008 / GCC / Clang / Make / CMake |

---

## Overview & Philosophy

True mastery of C cannot be achieved through isolated syntax exercises alone. Writing robust systems requires orchestrating memory management, OS system calls, concurrency primitives, data structures, and defensive error handling into coherent, production-grade applications.

Every project in this module is structured as a **complete, self-contained reference implementation** embedded in each project's dedicated `README.md`:
1. **Zero Toy Implementations:** Full error handling, boundary validation, and resource cleanup.
2. **Complete Source Code:** Full header interfaces, modular implementations, and test suites embedded directly in fenced code blocks.
3. **Build Specifications:** Fully functional `Makefile` and `CMakeLists.txt` scripts.
4. **Interactive Verification:** Real execution sessions, inputs, outputs, and edge cases.

---

## The 14 Production Projects Catalog

| # | Project Directory | Domain | Key Concepts & Architecture |
| :--- | :--- | :--- | :--- |
| **01** | [01-tiny-shell/](./01-tiny-shell/README.md) | Systems / OS | Fork/execvp process lifecycle, piping (`pipe`), I/O redirection (`dup2`), job control, signal handling |
| **02** | [02-http-server/](./02-http-server/README.md) | Networking | Non-blocking event-driven TCP socket server, HTTP/1.1 parsing, static file streaming, mime types |
| **03** | [03-key-value-store/](./03-key-value-store/README.md) | Storage / DB | In-memory hash table, binary protocol framing, append-only log (AOF) disk persistence, snapshotting |
| **04** | [04-memory-allocator/](./04-memory-allocator/README.md) | Systems / Runtime | Custom `malloc`/`free`/`realloc`, boundary tags, segregated free lists, coalesce on free, `sbrk`/`mmap` |
| **05** | [05-json-parser/](./05-json-parser/README.md) | Compilers / AST | Recursive-descent AST parser, tokenizer/lexer, dynamic object/array models, UTF-8 escape handling |
| **06** | [06-text-editor-or-grep-clone/](./06-text-editor-or-grep-clone/README.md) | Systems / Tooling | Multi-threaded pattern search, memory-mapped files (`mmap`), worker queues, colored ANSI output |
| **07** | [07-thread-pool/](./07-thread-pool/README.md) | Concurrency | Worker thread pool, task queue synchronization, condition variables, work completion barriers |
| **08** | [08-lru-cache/](./08-lru-cache/README.md) | Data Structures | $O(1)$ Least Recently Used cache combining hash table indexing with intrusive doubly linked lists |
| **09** | [09-tiny-vm-or-interpreter/](./09-tiny-vm-or-interpreter/README.md) | Compilers / Runtime | Stack-based bytecode virtual machine, instruction decoder loop, operand stack, disassembly |
| **10** | [10-dynamic-string-library/](./10-dynamic-string-library/README.md) | Core Foundations | ABI-stable dynamic string buffer (`sds`-style), length prefixing, binary-safe concatenation |
| **11** | [11-tcp-chat-server/](./11-tcp-chat-server/README.md) | Networking / IPC | Multi-client broadcast chat daemon, `poll`/`epoll` multiplexing, non-blocking sockets, client tracking |
| **12** | [12-file-compression-huffman/](./12-file-compression-huffman/README.md) | Algorithms | Huffman frequency tree, priority queue min-heap, bit-level stream I/O writer/reader, file packing |
| **13** | [13-ring-buffer-logger/](./13-ring-buffer-logger/README.md) | High Performance | Lock-free single-producer single-consumer (SPSC) ring buffer, memory barriers, async disk flusher |
| **14** | [14-mini-regex-engine/](./14-mini-regex-engine/README.md) | Theory / Compilers | Non-deterministic Finite Automaton (NFA) Thompson's construction, state graph matcher (`*`, `+`, `?`, `\|`) |

---

## Suggested Path

1. **Foundational Tooling & Data Structures**: Start with [10-dynamic-string-library](./10-dynamic-string-library/README.md), [08-lru-cache](./08-lru-cache/README.md), and [05-json-parser](./05-json-parser/README.md).
2. **Low-Level Systems & Memory**: Build [04-memory-allocator](./04-memory-allocator/README.md), [01-tiny-shell](./01-tiny-shell/README.md), and [06-text-editor-or-grep-clone](./06-text-editor-or-grep-clone/README.md).
3. **Concurrency & Storage**: Implement [07-thread-pool](./07-thread-pool/README.md), [13-ring-buffer-logger](./13-ring-buffer-logger/README.md), and [03-key-value-store](./03-key-value-store/README.md).
4. **Networking & Compilers**: Complete [02-http-server](./02-http-server/README.md), [11-tcp-chat-server](./11-tcp-chat-server/README.md), [09-tiny-vm-or-interpreter](./09-tiny-vm-or-interpreter/README.md), [12-file-compression-huffman](./12-file-compression-huffman/README.md), and [14-mini-regex-engine](./14-mini-regex-engine/README.md).

---

[← Previous Module: Design & Patterns](../10-design-and-patterns/README.md) | [C Language Track Index](../README.md) | [Next Module: Technical Interview Guide](../12-interview/README.md)
