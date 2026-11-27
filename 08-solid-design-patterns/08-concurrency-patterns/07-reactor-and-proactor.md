# Reactor & Proactor Patterns

> **One-line summary:** Handle tens of thousands of concurrent network connections on a single thread (or small core-count loop) by demultiplexing OS I/O readiness events (`Reactor`) or OS async completion events (`Proactor`)—eliminating 1-thread-per-connection memory overhead.

| Attribute | Specification |
|---|---|
| **Level** | Intermediate / Systems & Networking |
| **Prerequisites** | [01-thread-pool-and-executor.md](01-thread-pool-and-executor.md), [06-future-and-promise.md](06-future-and-promise.md) |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Complete |
| **Category** | Concurrency |
| **Origin** | Douglas C. Schmidt (POSA Vol. 2, 1995/2000) |
| **Also Known As** | Event Loop (`epoll`/`kqueue`), Non-Blocking I/O Demultiplexer |
| **Languages Tested** | Java 25 (LTS) / TypeScript 5.8 (Node 24 native) / Python 3.14 |

---

## Table of Contents
- [1 The Problem](#1-the-problem)
- [2 Why It Exists](#2-why-it-exists)
- [3 What It Is](#3-what-it-is)
- [4 The Problem Without It](#4-the-problem-without-it)
- [5 How It Solves It](#5-how-it-solves-it)
- [6 How It Works](#6-how-it-works)
- [7 Real-World Examples](#7-real-world-examples)
- [8 When to Use and When NOT to Use](#8-when-to-use-and-when-not-to-use)
- [9 Variations and Language Idioms](#9-variations-and-language-idioms)
- [10 Relationship to Other Principles and Patterns](#10-relationship-to-other-principles-and-patterns)
- [11 Common Mistakes and Anti-Patterns](#11-common-mistakes-and-anti-patterns)
- [12 Testing and Design Verification](#12-testing-and-design-verification)
- [13 Performance, Concurrency and Security Notes](#13-performance-concurrency-and-security-notes)
- [14 Interview Questions](#14-interview-questions)
- [15 Practice Exercises](#15-practice-exercises)
- [16 Summary / Cheat Sheet](#16-summary--cheat-sheet)
- [17 Further Reading](#17-further-reading)

---

## 1 The Problem

Classic blocking servers (`1 OS Thread = 1 Socket`) hit the **C10K Problem** (Dan Kegel, 1999): 10,000 idle keep-alive HTTP connections waste 10GB of thread stacks and thrash the OS scheduler, even though 99% of sockets are just sitting idle waiting for bytes!

---

## 2 Why It Exists

Documented by **Douglas C. Schmidt** (POSA Vol. 2):
- **Reactor (Readiness-driven: `epoll`, `kqueue`, Java NIO `Selector`, Node `libuv`, Python `asyncio`):** OS notifies the event loop *"Socket 42 is ready to read!"*; the handler reads non-blockingly and processes.
- **Proactor (Completion-driven: Windows IOCP, Linux `io_uring`):** App initiates async read into a buffer; the OS executes the read in the kernel and notifies *"Read into buffer X completed!"*.

---

## 3 What It Is

```mermaid
graph LR
    Socks[10,000 Sockets] -->|OS epoll / kqueue| Demux[Event Demultiplexer]
    Demux -->|Ready Event| Loop[Reactor Event Loop]
    Loop -->|dispatches fast non-blocking| Handler[Event Handler]
    Handler -.->|Offloads heavy CPU| Pool[Worker Thread Pool]
```

---

## 4 The Problem Without It

Blocking `socket.accept()` + `new Thread()` per client exhausts RAM and OS scheduler capacity at scale.

---

## 5 How It Solves It

### Java Implementation (Java 25 LTS — NIO `Selector` Reactor)

```java
package com.design.patterns.concurrency.reactor;

import java.nio.channels.Selector;

// Conceptual Reactor loop structure using Java NIO Selector:
public class NioReactorSketch {
    public static void runLoopOnce(Selector selector) throws Exception {
        selector.select(100); // Blocks in OS epoll/kqueue until at least 1 channel is ready!
        var keys = selector.selectedKeys().iterator();
        while (keys.hasNext()) {
            var key = keys.next();
            keys.remove(); // Crucial: remove handled key!
            if (key.isReadable()) { /* non-blocking read */ }
        }
    }
}
```

### TypeScript Implementation (Node 24 Native — `libuv` Reactor)

```typescript
// Every Node.js server (http, net, fs.promises) runs on libuv's Reactor loop!
import * as http from "node:http";

const server = http.createServer((_req, res) => {
    res.end("Handled non-blockingly on Reactor event loop!");
});
// server.listen(8080); // 1 thread handles 10,000+ concurrent keep-alive sockets!
```

### Python Implementation (Python 3.14 — `asyncio` Event Loop Reactor)

```python
import asyncio

async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter) -> None:
    data = await reader.read(1024)
    writer.write(data)
    await writer.drain()
    writer.close()
```

---

## 6 How It Works: The Golden Rule of Event Loops

**"Never Block the Event Loop!"**
Because 1 Reactor thread services 10,000 clients, executing a synchronous `Thread.sleep(2000)`, `fs.readFileSync()`, or heavy CPU loop (`bcrypt` / huge JSON parse) freezes **all 10,000 connected users simultaneously!** Always offload CPU-heavy work to a Worker Pool (`worker_threads` / `ProcessPoolExecutor` / `EventExecutorGroup`).

---

## 7 Real-World Examples

1. **Nginx** (master + worker Reactor processes handling C10K/C100K effortlessly).
2. **Node.js `libuv` & Netty (Java)** (Powering gRPC, Redis clients, Discord, WebFlux).
3. **Python `asyncio` / `uvicorn` (`uvloop`)** & **Redis single-threaded Reactor**.

---

## 8 When to Use and When NOT to Use

| Workload | Reactor (`asyncio`/Netty/Node) vs Virtual Threads (Java 25) |
|---|---|
| **Node.js / Python / Nginx / Netty** | **Reactor** is the native high-concurrency I/O engine. |
| **Java 21/25+** | **Virtual Threads** give Reactor-level scalability while letting you write clean synchronous-looking blocking code! |

---

## 9 Variations and Language Idioms

- **Multi-Reactor (Netty `bossGroup` + `workerGroup`):** 1 acceptor Reactor accepts new TCP connections and hands them round-robin to $N_{\text{cores}}$ I/O Reactor threads—saturating all CPU cores cleanly!

---

## 10 Relationship to Other Principles and Patterns

- **Hollywood Principle:** Reactor calls your registered `onRead`/`onData` handlers when OS readiness fires.
- **Observer & Future:** Reactors feed Promises/Futures upon I/O completion.

---

## 11 Common Mistakes and Anti-Patterns

| Anti-Pattern | Description | Production Consequence | Correct Remedy |
|---|---|---|---|
| **Blocking the Event Loop** | Calling `fs.readFileSync`, `time.sleep()`, or heavy CPU math inside an async handler. | Freezes *all* concurrent requests on that worker! | Use `fs.promises`, `asyncio.sleep`, or offload CPU to worker pool. |
| **Forgetting `keys.remove()` in Java NIO** | Not removing processed `SelectionKey` from `selectedKeys()`. | Busy-spin 100% CPU loop! | Always `iterator.remove()`. |
| **ReDoS (Catastrophic Regex Backtracking)** | Running complex regex on untrusted input on Node/Python event loop. | Single malicious string freezes entire server! | Use linear-time regex (`re2`) or strict length limits. |
| **Massive `JSON.parse()` on Event Loop** | Parsing 50MB JSON payloads synchronously in Node.js. | Event loop latency spike! | Stream-parse or offload to `worker_threads`. |
| **Ignoring Backpressure on Slow Socket Writes** | Calling `socket.write()` blindly without checking drain buffer. | Unbounded write buffer OOM! | Honor `write()` return `false` / `await writer.drain()`. |
| **Confusing Reactor with Proactor** | Calling `epoll` a Proactor. | Conceptual confusion. | `epoll`/`kqueue` = Reactor (readiness); `IOCP`/`io_uring` = Proactor (completion). |

---

## 12 Testing and Design Verification

Monitor **Event Loop Lag** (`monitorEventLoopDelay` in Node, slow-loop warnings in `asyncio(debug=True)`) under load tests to catch accidental blocking calls!

---

## 13 Performance, Concurrency and Security Notes

- **Security:** Event-loop servers are uniquely vulnerable to **CPU-starvation DoS** (e.g., ReDoS or algorithmic hash collisions) because one stalled request blocks all neighbors.

---

## 14 Interview Questions

<details>
<summary><strong>1. Distinguish Reactor vs Proactor patterns.</strong></summary>

**Reactor** (`epoll`, `kqueue`, Java NIO `Selector`, Node `libuv` on Unix) is **readiness-based**: the OS notifies the app when a socket is *ready* to read/write, and the app performs the non-blocking syscall. **Proactor** (Windows IOCP, Linux `io_uring`) is **completion-based**: the kernel executes the async I/O into a buffer and notifies the app only after completion.
</details>

<details>
<summary><strong>2. Why does blocking the event loop in Node.js or Python `asyncio` affect every user?</strong></summary>

Because a single Reactor thread multiplexes thousands of concurrent connections sequentially between `await` points—any synchronous CPU loop or blocking syscall (`time.sleep`, `readFileSync`) prevents all other queued callbacks from running until it finishes.
</details>

<details>
<summary><strong>3. How does Netty scale Reactor across multiple CPU cores?</strong></summary>

Netty uses the **Multi-Reactor** pattern: a `bossGroup` Reactor accepts incoming connections and distributes active sockets across a `workerGroup` of $N_{\text{cores}}$ I/O Reactor threads, plus separate business thread pools for slow work.
</details>

<details>
<summary><strong>4. How do Java 25 Virtual Threads compare to the Reactor pattern?</strong></summary>

Under the hood, Java Virtual Threads actually *use* an internal ForkJoin scheduler + OS non-blocking poller (Reactor-style!) when a virtual thread parks on I/O—giving developers Reactor-level C100K scalability with clean sequential imperative code!
</details>

<details>
<summary><strong>5. Why is `iterator.remove()` mandatory when iterating `selector.selectedKeys()` in Java NIO?</strong></summary>

`Selector` adds ready keys to `selectedKeys()` but never removes them automatically; if you forget `remove()`, the next `select()` returns immediately with stale keys, burning 100% CPU.
</details>

<details>
<summary><strong>6. What is Event Loop Lag and how do you diagnose it?</strong></summary>

Event Loop Lag measures how late a scheduled 1ms timer fires due to synchronous blocking work monopolizing the loop. Diagnose using `perf_hooks.monitorEventLoopDelay()` (Node) or `asyncio` debug mode.
</details>

<details>
<summary><strong>7. Why is ReDoS (Regular Expression Denial of Service) extra dangerous on Reactor servers?</strong></summary>

One maliciously crafted input string triggering exponential regex backtracking locks the single Reactor thread for seconds—taking down all 10,000 concurrent users with a single HTTP request.
</details>

<details>
<summary><strong>8. Give 3 production servers built on the Reactor pattern.</strong></summary>

**Nginx**, **Node.js**, **Redis**, and **Netty** / **Vert.x**.
</details>

---

## 15 Practice Exercises

### Exercise 1: Spot the Event-Loop Blocker (Easy)
`app.post('/hash', (req, res) => res.send(crypto.pbkdf2Sync(req.body.pw, ...)));` — why does latency spike for all users?

<details>
<summary><strong>Solution</strong></summary>

`pbkdf2Sync` runs synchronously on the main Node Reactor thread! Replace with async `crypto.pbkdf2(...)` which offloads to libuv's worker thread pool.
</details>

---

### Exercise 2: Offload CPU Work in Python `asyncio` (Medium)
Use `await asyncio.to_thread(heavy_cpu_or_blocking_fn, arg)` inside an `async def` endpoint so the event loop stays responsive.

<details>
<summary><strong>Solution</strong></summary>

`result = await asyncio.to_thread(heavy_fn, arg)` runs `heavy_fn` on a worker thread cleanly.
</details>

---

### Exercise 3: Event Loop Lag Detector in TypeScript (Hard)
Write a 5-line interval check using `performance.now()` that logs a warning whenever Node's event loop stalls $> 50\text{ms}$.

<details>
<summary><strong>Solution</strong></summary>

Track `last = performance.now()` inside `setInterval(..., 100)` and warn if `(now - last - 100) > 50`.
</details>

---

## 16 Summary / Cheat Sheet

```
+--------------------------------------------------------------------------+
|                       REACTOR & PROACTOR PATTERNS                        |
+--------------------------------------------------------------------------+
| REACTOR    : OS readiness events (epoll/kqueue) -> Node, Nginx, Netty.   |
| PROACTOR   : OS completion events (IOCP, io_uring).                      |
| GOLDEN RULE: NEVER block the event loop with sync I/O or heavy CPU!      |
+--------------------------------------------------------------------------+
```

---

## 17 Further Reading

- Douglas C. Schmidt et al., *Pattern-Oriented Software Architecture Vol. 2 (POSA2)*, Wiley, 2000.

### In Your Language
| Language Track | Deep Dive Reference |
|---|---|
| Java | [Java OOP & Design Patterns](file:///e:/Learning/engineering-notes/01-languages/java/24-design-patterns/README.md) |
| TypeScript | [TypeScript Design Patterns](file:///e:/Learning/engineering-notes/01-languages/typescript/17-design-patterns/README.md) |
| Python | [Python Design Patterns](file:///e:/Learning/engineering-notes/01-languages/python/23-design-patterns/README.md) |

---

[← Previous: 06 - Future & Promise](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/06-future-and-promise.md) | [Module 08 Index](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/README.md) | [Next: 08 - Immutability & Thread Confinement →](file:///e:/Learning/engineering-notes/08-solid-design-patterns/08-concurrency-patterns/08-immutability-and-thread-confinement.md)
