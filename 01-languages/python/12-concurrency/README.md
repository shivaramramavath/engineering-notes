# Module 12: Concurrency, AsyncIO & Free-Threading (`12-concurrency`)

> **Mastering Python concurrency architectures from foundational multithreading and multiprocessing to cooperative AsyncIO, structured concurrency TaskGroups (PEP 654), subinterpreters with per-interpreter GILs (PEP 684), and modern Python 3.13 free-threaded no-GIL execution (PEP 703).**

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/12-concurrency` |
| **Status** | 100% Complete (15 Notes + Module README) |
| **Total Lines Authored** | ~10,900+ lines of comprehensive technical documentation |
| **Target Python Version** | Python 3.12 / 3.13+ (`python3.13t` No-GIL, PEP 684, PEP 654 TaskGroups) |
| **Previous Module** | [11-type-hints-and-static-typing](../11-type-hints-and-static-typing/README.md) |
| **Next Module** | [13-python-internals](../13-python-internals/README.md) |

---

## 🎯 Learning Outcomes

By completing this module, you will:
1. **Choose the Optimal Concurrency Model**: Accurately evaluate I/O-bound vs CPU-bound bottlenecks and select between `threading`, `multiprocessing`, `asyncio`, subinterpreters, and free-threaded execution.
2. **Master Preemptive Multithreading**: Understand kernel thread mapping, lifecycle management, thread-local storage (`threading.local`), and exception interception (`threading.excepthook`).
3. **Enforce Absolute Thread Safety**: Dissect why the GIL does not protect against race conditions, master synchronization primitives (`Lock`, `RLock`, `BoundedSemaphore`, `Condition`, `Barrier`), and break the Coffman deadlock conditions.
4. **Scale Multi-Process Parallelism**: Harness `multiprocessing.Pool`, handle platform start methods (`spawn`, `fork`, `forkserver`), tune chunking strategies, and protect against Windows/macOS fork bombs.
5. **Implement Zero-Copy Shared Memory**: Build ultra-low-latency inter-process pipelines using `multiprocessing.shared_memory` to transfer gigabytes of tensors and video frames without `pickle` overhead.
6. **Orchestrate with `concurrent.futures`**: Unify thread and process pools behind the `Executor` and `Future` lifecycle, stream completion with `as_completed()`, and implement hedged requests.
7. **Build Scalable AsyncIO Services**: Master cooperative coroutines, the `__await__` protocol, the single-threaded event loop, and timeouts with modern `async with asyncio.timeout()`.
8. **Adopt Structured Concurrency**: Prevent background task leaks and handle compound failures with `asyncio.TaskGroup` and `ExceptionGroup` (`except*` pattern matching).
9. **Stream Sockets with Flow Control**: Implement backpressure on async queues and TCP streams with `await writer.drain()`, eliminating buffer bloat and OOM crashes.
10. **Bridge the Sync/Async Divide**: Safely invoke synchronous code from coroutines with `asyncio.to_thread()` and dispatch coroutines from sync threads via `run_coroutine_threadsafe()`.
11. **Harness Free-Threaded No-GIL Python**: Understand PEP 703 mimalloc memory arenas, biased reference counting, immortal objects, and multi-threaded CPU scaling on `python3.13t`.
12. **Deploy Subinterpreters (PEP 684)**: Isolate multi-tenant plugin runtimes in single-process hermetic sandboxes with private GILs and channel communication.
13. **Apply Resilient Production Patterns**: Deploy token bucket rate limiters, automated circuit breakers, single-flight request coalescers, and async streaming generators with `aclosing()`.
14. **Diagnose Event Loop Internals**: Dissect `_run_once()`, timer heaps, OS selectors (`epoll`/`kqueue`), activate debug profiling, and supercharge throughput with `uvloop`.

---

## 📑 Syllabus & Notes Directory

All 15 concept notes strictly follow the 13-section technical specification (`_templates/concept-template.md`), including Mermaid architecture diagrams, verified progressive code examples, edge cases, 6+ row pitfall tables, CPython internals, production case studies, 8 interview questions with `<details>`, and 6 practical exercises with verified solutions in `<details>`.

| # | Note | Primary Topics Covered | Lines | Difficulty |
|:---:|:---|:---|:---:|:---:|
| 00 | [Concurrency Models Overview](00-concurrency-models-overview.md) | Concurrency vs parallelism, I/O vs CPU bound, the 5 Python concurrency paradigms, decision matrix | 717 | Advanced |
| 01 | [Threading Basics](01-threading-basics.md) | OS thread mapping, thread lifecycle, daemon vs non-daemon rules, `threading.local()`, `threading.excepthook` | 694 | Intermediate |
| 02 | [Thread Safety, Locks & Semaphores](02-thread-safety-locks-and-semaphores.md) | Race conditions under GIL, Lock, RLock, BoundedSemaphore, Condition, Barrier, Coffman deadlock conditions | 829 | Advanced |
| 03 | [Multiprocessing Basics](03-multiprocessing-basics.md) | Multi-core parallelism, `spawn`/`fork`/`forkserver`, `multiprocessing.Pool`, chunking, pickle boundaries | 681 | Advanced |
| 04 | [Shared Memory and IPC](04-shared-memory-and-ipc.md) | `multiprocessing.Queue`, `Pipe`, `Value`, `Array`, `Manager`, Python 3.8+ zero-copy `SharedMemory` | 717 | Advanced |
| 05 | [Concurrent Futures & Executors](05-concurrent-futures-executors.md) | `ThreadPoolExecutor`, `ProcessPoolExecutor`, `Future` lifecycle, `as_completed()`, `wait()`, exception propagation | 655 | Advanced |
| 06 | [AsyncIO Fundamentals](06-asyncio-fundamentals.md) | Cooperative multitasking, `__await__` protocol, Event Loop, `asyncio.run()`, modern `asyncio.timeout()`, cancellation | 774 | Advanced |
| 07 | [Tasks, TaskGroups & Structured Concurrency](07-asyncio-tasks-and-taskgroups.md) | Structured concurrency, `asyncio.TaskGroup` (PEP 654), `ExceptionGroup` and `except*`, task leakage prevention | 726 | Advanced |
| 08 | [AsyncIO Queues & Network Streams](08-asyncio-queues-and-streams.md) | `asyncio.Queue`, `PriorityQueue`, backpressure, `StreamReader`/`StreamWriter`, `writer.drain()` flow control | 790 | Advanced |
| 09 | [Mixing Sync and Async Code](09-mixing-sync-and-async.md) | Function coloring problem, `asyncio.to_thread()`, `run_coroutine_threadsafe()`, background loop hosting, `contextvars` | 698 | Advanced |
| 10 | [Free-Threaded Python 3.13 (No-GIL)](10-free-threading-python313-no-gil.md) | PEP 703, `python3.13t`, mimalloc, biased reference counting, immortal objects, `sys._is_gil_enabled()`, multi-core scaling | 622 | Advanced |
| 11 | [Subinterpreters & Per-Interpreter GIL](11-subinterpreters.md) | PEP 684 per-interpreter GIL, PEP 554, `_interpreters`, memory isolation, inter-interpreter channels, plugin sandboxes | 678 | Advanced |
| 12 | [Concurrency Patterns & Pitfalls](12-concurrency-patterns-and-pitfalls.md) | Token bucket rate limiting, circuit breaker pattern, single-flight cache, thundering herd mitigation, hedged requests | 737 | Advanced |
| 13 | [Async Context Managers & Iterators](13-async-context-managers-and-iterators.md) | `__aenter__`/`__aexit__`, `@asynccontextmanager`, `__aiter__`/`__anext__`, async comprehensions, `asyncio.aclosing()` | 825 | Advanced |
| 14 | [Event Loop Internals & uvloop](14-event-loop-internals.md) | `BaseEventLoop._run_once()`, ready deque, timer min-heap, OS selectors (`epoll`/`kqueue`), `uvloop` engine, latency profiling | 774 | Advanced |

---

## 🛠️ Verification & Execution

To test the examples in this module:

```bash
# Run standard asynchronous verification
python -m unittest discover 01-languages/python/12-concurrency/

# Test free-threaded multi-core execution (requires Python 3.13t)
python3.13t -X gil=0 01-languages/python/12-concurrency/10-free-threading-python313-no-gil.md
```
