# C11 Atomics and Memory Model

> Master lock-free concurrent programming in C: ISO C11 `<stdatomic.h>`, atomic types, Compare-And-Swap (CAS) primitives, sequential consistency vs relaxed memory ordering, and hardware memory barriers.

| Metadata | Details |
| :--- | :--- |
| **Level** | Systems / Advanced |
| **Prerequisites** | [05-pthreads.md](./05-pthreads.md), [06-mutex-condvar-semaphores.md](./06-mutex-condvar-semaphores.md), [04-advanced/05-abi-and-calling-conventions.md](../04-advanced/05-abi-and-calling-conventions.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| ISO C11 / C17 / C23 (`<stdatomic.h>`, GCC/Clang builtins) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
Traditional concurrency using mutexes incurs significant runtime overhead: contending threads must transition into the operating system kernel via system calls (such as Linux `futex`), descheduling threads and polluting CPU hardware caches. For low-latency data structures (such as lock-free ring buffers, reference counters, and metrics counters), mutex synchronization can dominate execution time.

Introduced in **ISO C11**, `<stdatomic.h>` provides a formal multi-threaded memory model for C. It exposes lock-free hardware instructions (such as `LOCK CMPXCHG` on x86-64) directly to the language, providing atomic loads, stores, fetch-and-add, and Compare-And-Swap (CAS) operations with fine-grained control over compiler instruction reordering and CPU hardware memory barriers.

---

## 2. Core Concepts
- **Atomic Operations**: Operations that appear to occur instantaneously to all other threads in the system. They cannot be observed in an intermediate, partially written, or torn state.
- **Lock-Free vs Wait-Free**:
  - *Lock-Free*: Guarantees that at least one thread in the system makes progress in a finite number of steps (system-wide progress).
  - *Wait-Free*: Guarantees that every individual thread makes progress in a finite number of steps, eliminating starvation.
- **Compare-And-Swap (CAS)**: The foundational building block of lock-free algorithms:
  - `atomic_compare_exchange_strong(object, expected, desired)`: Compares `*object` with `*expected`. If equal, writes `desired` into `*object` and returns `true`. If unequal, copies current `*object` into `*expected` and returns `false`.
  - `weak` variant: May fail spuriously (e.g. on ARM/LL-SC architectures) even if values match, but is faster inside retry loops.
- **The 6 Memory Orderings**:
  1. `memory_order_relaxed`: Guarantees atomicity only; no ordering constraints on surrounding memory accesses.
  2. `memory_order_consume`: Data-dependency ordering (rarely used; compilers treat as acquire).
  3. `memory_order_acquire`: Prevents reads/writes following the barrier from being reordered before this read.
  4. `memory_order_release`: Prevents reads/writes preceding the barrier from being reordered after this write.
  5. `memory_order_acq_rel`: Combines acquire and release semantics (for read-modify-write operations).
  6. `memory_order_seq_cst`: Full sequential consistency; establishes a globally identical total order of operations across all CPU cores (safe default).

---

## 3. How It Works Internally

### Hardware Cache Coherence and Reordering
Modern multi-core processors execute instructions out-of-order and utilize store buffers to hide memory latency. Two threads writing to independent addresses may observe their counterpart's writes in a completely different order than specified in source code unless synchronized by memory barriers.

```
Core 0 (Producer)                                Core 1 (Consumer)
+-----------------------------------+            +-----------------------------------+
| data = 42;                        |            | while (!atomic_load(&ready,       |
| atomic_store_explicit(&ready, 1,  |            |                     acquire));    |
|                       release);   |            | printf("%d\n", data); // Prints 42|
+-----------------------------------+            +-----------------------------------+
                 |                                                 ^
                 | [Release Barrier: Flushes Store Buffer]         | [Acquire Barrier: Invalidates Inbound Caches]
                 +=================================================+
```

- **Release Barrier**: Guarantees all prior memory writes (both atomic and non-atomic) are committed to cache coherence before the atomic write becomes visible.
- **Acquire Barrier**: Guarantees subsequent memory reads will see the updates published before the corresponding release store.
- On x86-64, hardware is strongly ordered (Total Store Order, TSO): loads already have acquire semantics and stores have release semantics for free. On weakly ordered architectures (ARM, POWER, RISC-V), the compiler emits explicit fence instructions (`DMB ISHLD`, `DMB ISHST`).

---

## 4. Syntax & Usage

### Primary Header Inclusions
```c
#include <stdatomic.h>
#include <stdbool.h>
```

### Signature Overview
```c
void atomic_init(volatile A *obj, C value);
void atomic_store(volatile A *obj, C desired);
void atomic_store_explicit(volatile A *obj, C desired, memory_order order);
C atomic_load(const volatile A *obj);
C atomic_load_explicit(const volatile A *obj, memory_order order);
C atomic_fetch_add(volatile A *obj, M operand);
bool atomic_compare_exchange_strong(volatile A *obj, C *expected, C desired);
bool atomic_compare_exchange_weak(volatile A *obj, C *expected, C desired);
void atomic_thread_fence(memory_order order);
```

---

## 5. Practical Examples

### Example 1: Atomic Counter, Compare-And-Swap (CAS), and Spinlock
```c
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
    atomic_int counter;
    atomic_flag spinlock;
} SharedAtomicState;

void spinlock_lock(atomic_flag *lock) {
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        /* Spin / Busy wait */
    }
}

void spinlock_unlock(atomic_flag *lock) {
    atomic_flag_clear_explicit(lock, memory_order_release);
}

int main(void) {
    SharedAtomicState state;
    atomic_init(&state.counter, 100);
    atomic_flag_clear(&state.spinlock);

    printf("Initial atomic counter: %d\n", atomic_load_explicit(&state.counter, memory_order_relaxed));

    // Test Compare-And-Swap (CAS)
    int expected = 100;
    int desired = 250;
    bool success = atomic_compare_exchange_strong_explicit(
        &state.counter, &expected, desired,
        memory_order_acq_rel, memory_order_relaxed);

    printf("CAS (100 -> 250): %s, New Value: %d\n", 
           success ? "SUCCESS" : "FAILED", 
           atomic_load(&state.counter));

    // Test Atomic Spinlock Critical Section
    spinlock_lock(&state.spinlock);
    atomic_fetch_add_explicit(&state.counter, 50, memory_order_relaxed);
    spinlock_unlock(&state.spinlock);

    printf("Counter after spinlock critical section (+50): %d\n", atomic_load(&state.counter));
    return 0;
}
```

```text
Initial atomic counter: 100
CAS (100 -> 250): SUCCESS, New Value: 250
Counter after spinlock critical section (+50): 300
```

### Example 2: Lock-Free Single-Producer Single-Consumer (SPSC) Queue Flag
```c
#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
    int data[1024];
    atomic_size_t head; /* Read index */
    atomic_size_t tail; /* Write index */
} SPSCQueue;

void spsc_init(SPSCQueue *q) {
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
}

bool spsc_push(SPSCQueue *q, int item) {
    size_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    size_t current_head = atomic_load_explicit(&q->head, memory_order_acquire);
    
    if ((current_tail + 1) % 1024 == current_head) {
        return false; /* Queue full */
    }

    q->data[current_tail] = item;
    atomic_store_explicit(&q->tail, (current_tail + 1) % 1024, memory_order_release);
    return true;
}

bool spsc_pop(SPSCQueue *q, int *out) {
    size_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);
    size_t current_tail = atomic_load_explicit(&q->tail, memory_order_acquire);

    if (current_head == current_tail) {
        return false; /* Queue empty */
    }

    *out = q->data[current_head];
    atomic_store_explicit(&q->head, (current_head + 1) % 1024, memory_order_release);
    return true;
}
```

---

## 6. Best Practices
1. **Default to `memory_order_seq_cst` Unless Benchmarked**: Sequential consistency eliminates reordering bugs. Only optimize to acquire-release or relaxed after profiling proves the atomic operation is a bottleneck.
2. **Use `atomic_compare_exchange_weak` in Loops**: In `while (!atomic_compare_exchange_weak(...))` loops, the weak variant generates faster instructions on RISC/ARM architectures (e.g. `LDREX`/`STREX`) because loop repetition handles spurious failures.
3. **Always Check Lock-Free Availability**: Query `atomic_is_lock_free(&obj)` to verify whether operations use genuine hardware instructions or fall back to internal libc locks.
4. **Beware the ABA Problem**: In lock-free stacks and linked lists, a node pointer changing from $A 	o B 	o A$ tricks CAS into assuming state never changed. Use tagged pointers (combining pointer with monotonic generation count) to mitigate.
5. **Pair Acquire with Release**: Every publication of shared state requires `memory_order_release` at the producer and `memory_order_acquire` at the consumer.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Using `memory_order_relaxed` for flag synchronization | Relaxed does not establish happens-before; consumer can see flag set while data writes are still pending | Use `release` on store and `acquire` on load |
| Assuming `atomic_int` makes whole struct atomic | Only the individual `atomic_int` field is atomic; concurrent access to other fields remains a data race | Protect struct with mutex or use atomic pointer swap |
| Using plain `volatile` for thread synchronization | In standard C, `volatile` prevents compiler optimizations but emits **zero** CPU memory fences or atomic instructions | Use `<stdatomic.h>` types and functions |
| Falling victim to the ABA problem | CAS succeeds because pointer matched $A$, but the underlying node at $A$ was modified or freed | Implement double-word CAS with version counter or hazard pointers |
| Passing uninitialized variable as `expected` to CAS | CAS compares against garbage, fails immediately, and copies random value into `expected` | Initialize `expected` with current value: `int expected = atomic_load(&val);` |
| Spin-waiting on `memory_order_seq_cst` without CPU pause | Burns 100% CPU power and starves neighboring hyper-threads | Use `_mm_pause()` (x86) or `__builtin_arm_yield()` (ARM) in spin loops |

---

## 8. Performance & Security Notes
- **Hardware Cost of `seq_cst` vs `relaxed`**: On x86-64, an atomic store with `memory_order_seq_cst` compiles to `XCHG` or `MOV` + `MFENCE`, which drains the CPU store buffer and costs $pprox 30$ CPU cycles. In contrast, `memory_order_relaxed` or `release` stores compile to a standard `MOV` instruction costing $pprox 1$ cycle.
- **Cache Line Bouncing**: When multiple threads aggressively update an atomic counter via `atomic_fetch_add()`, the cache line bounces between CPU L1/L2 caches, causing bus saturation. For high-throughput metrics, use thread-local accumulators that aggregate only upon read.
- **Compiler Optimization Barriers**: Compilers assume standard C variables are not modified across atomic operations unless sequenced by appropriate memory orderings. Missing acquire/release fences can cause GCC/Clang to hoist loads out of loops.

---

## 9. Comparison / When to Use What

| Primitive | Synchronization Scope | Overhead | Best Use Case |
| :--- | :--- | :--- | :--- |
| `pthread_mutex_t` | Blocks thread via OS kernel (futex) | High on contention (~1000 ns) | Complex critical sections, multiple variables |
| `stdatomic.h` (`seq_cst`) | Hardware lock prefix / full memory barrier | Low (~10-30 ns) | Reference counting, thread-safe booleans |
| `stdatomic.h` (`relaxed`) | Hardware atomic fetch/add, no barriers | Minimal (~1-5 ns) | Metrics counters, statistical telemetry |
| `atomic_flag` | Guaranteed lock-free single-bit flag | Minimal | Lightweight spinlocks, run-once initialization |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `memory_order_relaxed` and `memory_order_seq_cst`?</summary>
`memory_order_relaxed` guarantees only that the read or write operation itself is atomic (no torn reads or writes). It imposes zero synchronization or ordering constraints on surrounding memory accesses; the compiler and CPU are free to reorder surrounding reads and writes across the operation. `memory_order_seq_cst` guarantees total sequential consistency: it enforces a globally identical execution order across all CPU cores, establishing strict happens-before relationships.
</details>

<details>
<summary>2. Why does `volatile` in C not provide thread safety?</summary>
In ISO C, `volatile` only informs the compiler that a variable can be changed by something outside the program's control (such as hardware registers or signal handlers), preventing the compiler from caching it in a CPU register or eliding reads/writes. However, `volatile` does not generate atomic CPU bus instructions (like `LOCK CMPXCHG`) and does not emit hardware memory fences, leaving it vulnerable to hardware out-of-order execution and torn reads/writes across multiple cores.
</details>

<details>
<summary>3. What is the ABA problem, and how is it resolved?</summary>
The ABA problem occurs when Thread 1 reads value $A$ from an atomic variable. Thread 2 preempts, changes the variable to $B$, and then changes it back to $A$ before Thread 1 resumes. When Thread 1 executes Compare-And-Swap (CAS), it observes $A$ and concludes that nothing changed, even though the internal state of the referenced object may have been altered or recycled. It is solved using tagged pointers (combining a pointer with a 64-bit monotonically incrementing sequence counter) or hazard pointers.
</details>

<details>
<summary>4. When should you use `atomic_compare_exchange_weak` instead of `strong`?</summary>
`weak` should be used inside retry loops (`while (!atomic_compare_exchange_weak(...))`). On Load-Linked/Store-Conditional (LL/SC) CPU architectures (such as ARM and RISC-V), `weak` compiles to a single store-conditional instruction that may fail spuriously due to context switches or cache evictions. `strong` must implement a loop to eliminate spurious failures; thus, using `strong` inside a user loop creates an unnecessary nested loop.
</details>

<details>
<summary>5. How does acquire-release synchronization establish a "happens-before" relationship?</summary>
When Thread A stores data to a shared location using `memory_order_release`, and Thread B subsequently loads that same atomic location using `memory_order_acquire`, all memory writes (atomic and non-atomic) made by Thread A prior to the release store become visible to Thread B after the acquire load.
</details>

<details>
<summary>6. What does `atomic_is_lock_free()` tell you?</summary>
It returns `true` if atomic operations on the specified object are implemented using native, hardware-level CPU atomic instructions without relying on internal mutex locks inside the C runtime library.
</details>

<details>
<summary>7. What is `atomic_flag` and what is unique about it in C11?</summary>
`atomic_flag` is the only atomic type in C11 guaranteed by the standard to be lock-free on **all** hardware platforms. It supports only two operations: `atomic_flag_test_and_set()` and `atomic_flag_clear()`, making it the universal foundation for building custom spinlocks.
</details>

<details>
<summary>8. How do you implement a lock-free reference counting decrement in C11?</summary>
Use `atomic_fetch_sub_explicit(&ref_count, 1, memory_order_acq_rel)`. If the returned previous value was 1 (meaning the counter is now 0), the current thread has exclusive ownership and must safely deallocate the underlying resource. The `acq_rel` ordering ensures all prior reads/writes to the object complete before destruction.
</details>

---

## 11. Practice Exercises

### Easy
1. **Atomic Counter**: Write a program where 4 threads increment an `atomic_int` 25,000 times using `atomic_fetch_add_explicit(..., memory_order_relaxed)`. Verify final count is 100,000.
2. **Boolean Shutdown Flag**: Implement a worker loop that runs while `atomic_load_explicit(&running, memory_order_relaxed)` is true; main thread sets it to false to stop.
3. **Atomic Max**: Write a function `void atomic_max(atomic_int *target, int value)` using a CAS loop.

<details>
<summary>Solutions (Easy)</summary>

```c
/* Exercise 3: Atomic Max via CAS Loop */
#include <stdatomic.h>

void atomic_max(atomic_int *target, int value) {
    int cur = atomic_load_explicit(target, memory_order_relaxed);
    while (cur < value && !atomic_compare_exchange_weak_explicit(
               target, &cur, value, memory_order_relaxed, memory_order_relaxed)) {
        /* Loop continues until cur >= value or CAS succeeds */
    }
}
```
</details>

### Medium
4. **Lock-Free Spinlock**: Implement a complete spinlock using `atomic_flag` with `spinlock_lock()` (acquire) and `spinlock_unlock()` (release).
5. **Thread-Safe Reference Counter**: Implement a shared resource handle with `retain()` and `release()` using `atomic_int` and an acquire-release destruction barrier.
6. **Hardware Lock-Free Checker**: Write a program that queries and prints `atomic_is_lock_free()` for `atomic_char`, `atomic_int`, `atomic_long`, and `atomic_intmax_t`.

<details>
<summary>Solutions (Medium)</summary>

```c
/* Exercise 5: Thread-Safe Reference Counter */
#include <stdatomic.h>
#include <stdlib.h>

typedef struct {
    atomic_int ref_count;
    void *payload;
} RefCountedObj;

RefCountedObj *obj_create(void *data) {
    RefCountedObj *o = malloc(sizeof(RefCountedObj));
    atomic_init(&o->ref_count, 1);
    o->payload = data;
    return o;
}

void obj_retain(RefCountedObj *o) {
    atomic_fetch_add_explicit(&o->ref_count, 1, memory_order_relaxed);
}

void obj_release(RefCountedObj *o) {
    if (atomic_fetch_sub_explicit(&o->ref_count, 1, memory_order_acq_rel) == 1) {
        free(o->payload);
        free(o);
    }
}
```
</details>

### Hard
7. **Lock-Free Treiber Stack**: Implement a complete lock-free LIFO stack using `atomic_compare_exchange_weak` for `push` and `pop` operations.
8. **Lock-Free Ring Buffer (SPSC)**: Implement a high-performance single-producer single-consumer circular buffer with power-of-two capacity and bitwise masking using acquire/release atomics.
9. **Tagged Pointer ABA Defense**: Implement a 64-bit tagged pointer packing a 48-bit address with a 16-bit sequence tag, verified with 64-bit CAS.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 7: Lock-Free Treiber Stack */
#include <stdatomic.h>
#include <stdlib.h>

typedef struct StackNode {
    int data;
    struct StackNode *next;
} StackNode;

typedef struct {
    _Atomic(StackNode *) head;
} LockFreeStack;

void stack_init(LockFreeStack *s) {
    atomic_init(&s->head, NULL);
}

void stack_push(LockFreeStack *s, int val) {
    StackNode *node = malloc(sizeof(StackNode));
    node->data = val;
    node->next = atomic_load_explicit(&s->head, memory_order_relaxed);
    while (!atomic_compare_exchange_weak_explicit(
               &s->head, &node->next, node,
               memory_order_release, memory_order_relaxed)) {
        /* node->next updated with current head automatically */
    }
}

int stack_pop(LockFreeStack *s, int *out_val) {
    StackNode *old_head = atomic_load_explicit(&s->head, memory_order_acquire);
    while (old_head != NULL && !atomic_compare_exchange_weak_explicit(
               &s->head, &old_head, old_head->next,
               memory_order_acquire, memory_order_acquire)) {
        /* old_head updated automatically */
    }
    if (old_head == NULL) return 0; /* Empty */
    *out_val = old_head->data;
    free(old_head);
    return 1;
}
```
</details>

---

## 12. Summary / Cheat Sheet
- **Header**: `#include <stdatomic.h>`.
- **Memory Orders**:
  - `relaxed`: Atomicity only, zero order guarantees.
  - `acquire`: Read barrier (no subsequent reads hoisted above).
  - `release`: Write barrier (no prior writes sunk below).
  - `seq_cst`: Full sequential consistency.
- **CAS**: `atomic_compare_exchange_weak(obj, &exp, des)` updates `*obj` if equal to `*exp`.
- **Spinlock**: `while (atomic_flag_test_and_set_explicit(&flag, memory_order_acquire));`.

---

## 13. Further Reading
- ISO/IEC 9899:2011 (C11 Standard), Section 7.17 (Atomics `<stdatomic.h>`).
- Paul E. McKenney, *Is Parallel Programming Hard, And, If So, What Can You Do About It?*
- Anthony Williams, *C++ Concurrency in Action*, Chapter 5 (The memory model and operations on atomic types).

---

[← Previous Note: Mutexes, Condition Variables & Semaphores](./06-mutex-condvar-semaphores.md) | [Module Index](README.md) | [Next Note: C11 Standard Threads](./08-c11-threads-h.md)
