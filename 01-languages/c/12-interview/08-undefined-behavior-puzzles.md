# Undefined Behavior Traps and Puzzles (`08-undefined-behavior-puzzles`)

> Analyze how modern optimizing compilers exploit ISO C Undefined Behavior (UB) axioms: time-travel null elimination, loop induction truncation, and strict aliasing optimizations.

| Metadata | Details |
| :--- | :--- |
| **Guide** | `08-undefined-behavior-puzzles` |
| **Level** | Advanced |
| **Standard Baseline** | C11 / C17 / C23 / GCC / Clang |

---

## Puzzle 1: The "Time-Travel" Null Check Elimination

```c
void process_packet(Packet *pkt) {
    int id = pkt->header.id; /* Dereference at line 2 */

    if (!pkt) {              /* Null check at line 4 */
        log_error("Null packet received!");
        return;
    }

    handle_id(id);
}
```

<details>
<summary><strong>What does the optimizing compiler do with the null check?</strong></summary>

**Compiler Optimization:** The compiler **completely deletes** the `if (!pkt)` check!

**Why:** In ISO C, dereferencing a NULL pointer is Undefined Behavior. The compiler reasons using proof by contradiction:
1. If `pkt` were NULL, line 2 would have invoked Undefined Behavior.
2. Since the compiler is entitled to assume UB never occurs in correct programs, `pkt` **must** be non-NULL at line 2.
3. Therefore, `pkt` cannot be NULL at line 4.
4. The null check is dead code and is pruned entirely!
If `pkt` is actually NULL at runtime, the program crashes with a segmentation fault without ever executing `log_error`.
</details>

---

## Puzzle 2: Infinite Loop Elimination

```c
int run_infinite_loop(void) {
    while (1) {
        /* Empty loop with no side effects */
    }
    return 42;
}
```

<details>
<summary><strong>What does a conforming C11 compiler emit for this function?</strong></summary>

In C11 (Section 6.8.5#6), an iteration statement whose controlling expression is not a constant true expression, or any loop that contains no I/O, volatile accesses, or synchronization, may be assumed by the implementation to terminate!
Under `-O3`, Clang and GCC can optimize away the loop entirely and return `42` directly, or fall through to whatever code follows in memory.
</details>

---

## Puzzle 3: Signed Integer Overflow Optimization

```c
bool is_overflow(int x) {
    return (x + 10) < x;
}
```

<details>
<summary><strong>What does this function return under `-O2`?</strong></summary>

**Return Value:** Always `false`!

**Why:** Signed integer overflow is Undefined Behavior in C. Because the compiler assumes signed arithmetic never overflows, mathematically $x + 10$ is always strictly greater than $x$. The compiler simplifies the comparison to a constant `0` (`false`).
To check for overflow legally, use unsigned arithmetic or C23 `<stdckdint.h>`.
</details>

---

[← C vs C++](./07-c-vs-cpp.md) | [Interview Index](./README.md) | [Next: Embedded C QA →](./09-embedded-c-qa.md)
