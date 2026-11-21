# Debugging with GDB

> A deep technical guide to the GNU Project Debugger (GDB): symbol generation with DWARF, breakpoint manipulation, call stack backtracing, hardware watchpoints, assembly disassembly, and post-mortem core dump analysis.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-toolchain.md](./01-toolchain.md), [03-compilation-process.md](./03-compilation-process.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 / C23 Debugging Infrastructure |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In C, errors manifest as silent memory corruption, segmented execution faults (`SIGSEGV`), infinite loops, or undefined behavior. Relying exclusively on "printf debugging" has major flaws:
1. **Heisenbugs**: Injecting `printf()` calls modifies stack layout, register allocation, cache timing, and file I/O buffering. A race condition or memory bug may vanish solely because `printf()` was added.
2. **Crash Invisibility**: When a program crashes with a segmentation fault, buffered `printf` output often remains unflushed in `stdout` user-space buffers, misleading developers about the crash line.
3. **Lack of Post-Mortem State**: `printf` cannot reconstruct the program's register states, memory heap, and stack frames after termination.

The **GNU Debugger (GDB)** solves this by executing target programs under direct CPU and kernel supervisory control (`ptrace` on Linux, debug APIs on Windows), allowing developers to pause execution, inspect arbitrary registers and memory, single-step through assembly instructions, and inspect core dump files.

---

## 2. Core Concepts

### Debugging Information: DWARF vs. Symbols

A compiler normally strips symbol names and variable mapping to optimize binary size. To enable source-level debugging, the compiler must embed debug metadata:
- **`-g` / `-g3`**: Emits DWARF (Debugging With Arbitrary Record Formats) sections (`.debug_info`, `.debug_line`, `.debug_frame`, `.debug_loc`). `-g3` includes preprocessor `#define` macros.
- **`-O0`**: Disables optimization passes so that local variables correspond 1:1 to memory locations on the stack rather than being optimized into registers or eliminated.

```
+-------------------------------------------------------------------+
|                        GDB ARCHITECTURE                           |
+-------------------------------------------------------------------+
|  [ User Interface / TUI / VS Code DAP ]                           |
|       |                                                           |
|  [ GDB Core Engine ] <----> [ DWARF Symbol Table Parser ]         |
|       |                                                           |
|  [ OS Debug Interface: ptrace(2) / Win32 Debug API ]             |
|       |                                                           |
|  [ Target Process / Stopped in Kernel by SIGTRAP / Trap Flag ]    |
+-------------------------------------------------------------------+
```

### Essential GDB Workflow Commands

| GDB Command | Shorthand | Purpose |
|---|---|---|
| `run [args]` | `r` | Start program execution with optional command-line arguments |
| `break [func/line]` | `b` | Set a software breakpoint at a function name or file:line |
| `watch [expr]` | `wa` | Set a hardware watchpoint that halts execution when memory changes |
| `step` | `s` | Step execution by one source line, stepping INTO functions |
| `next` | `n` | Step execution by one source line, stepping OVER functions |
| `stepi` / `nexti` | `si` / `ni` | Single-step by single CPU machine instruction |
| `finish` | `fin` | Continue execution until the current function returns |
| `continue` | `c` | Resume program execution until the next breakpoint or signal |
| `backtrace` | `bt` | Print the full call stack frames from main() to the crash point |
| `print [expr]` | `p` | Evaluate and display variable value or C expression |
| `x/[format] [addr]` | `x` | Examine raw memory at an address (`x/16xb $rsp`, `x/s ptr`) |
| `info registers` | `i r` | Display CPU architectural registers (`rax`, `rbx`, `rip`, etc.) |

---

## 3. How It Works Internally

### How Software Breakpoints Work: Software Traps (`INT 3`)

When you execute `break main` in GDB:
1. GDB queries the DWARF symbol table to find the virtual memory address of the first instruction of `main()`.
2. GDB uses the kernel `ptrace(PTRACE_POKETEXT)` system call to read the original machine instruction byte at that address.
3. GDB saves the original byte and overwrites it with an `INT 3` instruction (opcode `0xCC` on x86/x64).
4. When the CPU hits `0xCC`, the CPU triggers an interrupt exception (`Vector 3`).
5. The operating system kernel intercepts this interrupt, stops the target process, and sends a `SIGTRAP` signal to GDB.
6. GDB handles `SIGTRAP`, restores the original instruction byte in memory, decrements the instruction pointer (`%rip`), and prompts the user.

```
Original Memory:       [ 0x55 ] (push %rbp)  [ 0x48 0x89 0xE5 ]
GDB Inserts Trap:      [ 0xCC ] (int 3)      [ 0x48 0x89 0xE5 ]  --> Halts CPU!
GDB Resumes Step:      [ 0x55 ] (restored)   [ 0x48 0x89 0xE5 ]
```

### Hardware Watchpoints: Debug Registers (`DR0` - `DR7`)

Unlike software breakpoints, watchpoints monitor **memory writes or reads** without modifying executable code. On x86-64 processors, the CPU provides 4 hardware debug registers (`DR0` through `DR3`) and a control register (`DR7`). When you run `watch counter`, GDB programs `DR0` with the address of `counter` and `DR7` with condition flags (break on write, 4 bytes). The CPU hardware itself monitors bus accesses; when a write hits that address, the CPU halts immediately without code modification.

---

## 4. Syntax & Usage

### 1. Compiling for GDB
```bash
# Compile with DWARF debug info and zero optimizations
gcc -std=c11 -Wall -Wextra -g3 -O0 main.c -o myprog

# Or compile for release with separate debug symbols
gcc -std=c11 -O2 -g main.c -o myprog
objcopy --only-keep-debug myprog myprog.debug
strip --strip-debug --strip-unneeded myprog
objcopy --add-gnu-debuglink=myprog.debug myprog
```

### 2. Basic GDB Interactive Session
```text
$ gdb ./myprog
(gdb) break main
Breakpoint 1 at 0x1149: file main.c, line 10.
(gdb) run
Starting program: /path/to/myprog
Breakpoint 1, main () at main.c:10
10          int x = 42;
(gdb) next
11          int y = calculate(x);
(gdb) step
calculate (val=42) at main.c:4
4           return val * 2;
(gdb) print val
$1 = 42
(gdb) backtrace
#0  calculate (val=42) at main.c:4
#1  0x000055555555515e in main () at main.c:11
(gdb) continue
```

### 3. Analyzing a Core Dump (Post-Mortem Analysis)
```bash
# Enable core file generation in Linux shell
ulimit -c unlimited

# When a segmentation fault occurs, core is generated
./myprog
# Segmentation fault (core dumped)

# Inspect with GDB
gdb ./myprog core
(gdb) bt full
# Inspects every variable in every frame at the exact moment of crash!
```

---

## 5. Practical Examples

### Interactive Debugging Target: Recursive Call Stack Tracing

```c
/* File: 01-languages/c/00-setup/examples/06-debugging-with-gdb.c */
#include <stdio.h>

long factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main(void) {
    printf("=== GDB Debugging Target Program ===\n");
    printf("Tracing call stack for factorial calculation...\n");
    
    int val = 5;
    long result = factorial(val);
    
    printf("Factorial of %d is %ld\n", val, result);
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -g3 -O0 06-debugging-with-gdb.c -o run_06.exe
./run_06.exe
```
Output:
```text
=== GDB Debugging Target Program ===
Tracing call stack for factorial calculation...
Factorial of 5 is 120
```

#### What GDB Sees in Call Stack (`backtrace` at `n == 1`):
```text
(gdb) break factorial if n == 1
(gdb) run
Breakpoint 1, factorial (n=1) at 06-debugging-with-gdb.c:5
(gdb) bt
#0  factorial (n=1) at 06-debugging-with-gdb.c:5
#1  0x000000000040156d in factorial (n=2) at 06-debugging-with-gdb.c:8
#2  0x000000000040156d in factorial (n=3) at 06-debugging-with-gdb.c:8
#3  0x000000000040156d in factorial (n=4) at 06-debugging-with-gdb.c:8
#4  0x000000000040156d in factorial (n=5) at 06-debugging-with-gdb.c:8
#5  0x00000000004015a8 in main () at 06-debugging-with-gdb.c:16
```

---

## 6. Best Practices

1. **Always Use `-g3 -O0` for Debug Builds**: `-g3` preserves macro definitions (`#define MAX_SIZE 1024`), allowing you to use `info macro MAX_SIZE` directly inside GDB.
2. **Enable GDB TUI (Text User Interface) Mode**: Press `Ctrl+X Ctrl+A` (or run `gdb -tui`) to split your terminal into an interactive source-code viewer above and a command prompt below.
3. **Use Pretty Printers for C Standard Structures**: Install standard library python pretty-printers to format complex nested structs instead of printing raw hex addresses.
4. **Automate Breakpoints with `.gdbinit`**: Place project-specific setup commands (such as setting breakpoints or configuring target paths) in a local `.gdbinit` file.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Debugging an optimized binary (`-O2` or `-O3`) | The compiler inlines functions, unrolls loops, and optimizes out variables, resulting in `<value optimized out>`. | Compile with `-O0 -g` when diagnosing bugs. |
| Forgetting to recompile before debugging | GDB reads source files from disk; if code was modified after compiling, line numbers and breakpoints will point to mismatched lines. | Use automated Make targets that depend on current binaries. |
| Using `step` when you intended `next` | `step` plunges into C standard library functions (like `printf` or `strlen`) if debug symbols for glibc are present. | Use `next` to step over known library calls. |
| Stripping debug symbols before archiving | A stripped crash report cannot be diagnosed. | Retain unstripped debug binaries or use `objcopy --only-keep-debug` to archive symbols. |
| Not checking variable scope in macros | GDB cannot set breakpoints inside multi-statement macros easily. | Prefer `static inline` functions over complex preprocessor macros. |
| Assuming watchpoints have zero overhead | Software watchpoints (when hardware registers are exhausted) single-step the entire process, slowing execution down by orders of magnitude. | Keep watchpoints restricted to fewer than 4 simultaneous hardware registers. |

---

## 8. Performance & Security Notes

- **Never Ship `-g` Binaries to Public Production Without Stripping**: Debug symbols expose internal struct layouts, function names, source paths, and developer filenames, aiding reverse engineers.
- **ASLR (Address Space Layout Randomization)**: GDB disables ASLR by default (`set disable-randomization on`) so that addresses remain predictable across runs. In production, ASLR will randomize addresses; ensure you understand base offsets when inspecting core files.

---

## 9. Comparison / When to Use What

| Tool | Primary Use Case | Overhead | Strengths |
|---|---|---|---|
| **GDB / LLDB** | Interactive execution control, breakpoint stepping, core dump triage | Low (until watchpoint) | Precise state inspection, register control |
| **AddressSanitizer (ASan)** | Automated out-of-bounds, use-after-free, double-free detection | ~2x CPU, ~3x RAM | Pinpoints memory corruption without interactive stepping |
| **Valgrind (Memcheck)** | Dynamic binary instrumentation, memory leaks, uninitialized reads | ~10x-20x slowdown | No recompilation required; deep memory leak analysis |
| **Static Analyzers** | Pre-compile analysis of code patterns (`clang-tidy`, `cppcheck`) | Zero runtime | Detects vulnerabilities before running code |

---

## 10. Interview Questions

<details>
<summary><strong>1. How does GDB implement a software breakpoint at the machine level?</strong></summary>

GDB replaces the byte at the target address with an `INT 3` opcode (`0xCC` on x86). When execution reaches this byte, an architectural interrupt is fired, stopping the process and handing control to GDB via `SIGTRAP`. Upon resuming, GDB restores the original byte, executes it, and reinserts the trap if necessary.
</details>

<details>
<summary><strong>2. What is the difference between GDB commands `step` and `next`?</strong></summary>

`step` executes the current source line and stops at the next line; if the current line contains a function call, it steps **into** that function. `next` steps **over** function calls, continuing execution until the function returns and halting at the next line in the current stack frame.
</details>

<details>
<summary><strong>3. How does a hardware watchpoint differ from a software watchpoint?</strong></summary>

A hardware watchpoint uses dedicated CPU debug registers (`DR0`-`DR3` on x86) to monitor addresses directly on the hardware memory bus without slowing execution. A software watchpoint is used when hardware registers are exhausted or expressions are complex; GDB must single-step every instruction and evaluate the expression, causing massive slowdown.
</details>

<details>
<summary><strong>4. Why do variables show as `<value optimized out>` in GDB?</strong></summary>

When code is compiled with optimizations (`-O1`, `-O2`, `-O3`), the compiler may keep variables purely in registers, unroll them, or eliminate them entirely via dead-code elimination. Use `-O0` to ensure all variables retain dedicated stack locations.
</details>

<details>
<summary><strong>5. What is a core dump, and how do you analyze it with GDB?</strong></summary>

A core dump is an ELF file containing the recorded memory space, thread states, and register contents of a process at the instant it terminated abnormally (e.g. `SIGSEGV`). It is loaded using `gdb <executable> <corefile>`, allowing complete stack backtracing (`bt full`) without rerunning the program.
</details>

<details>
<summary><strong>6. What GDB command examines raw memory in hexadecimal, and what is its syntax?</strong></summary>

The `x` (examine) command. Syntax: `x/[count][format][size] [address]`. For example: `x/16xb 0x7fffffffe000` prints 16 bytes in hexadecimal byte format.
</details>

<details>
<summary><strong>7. How can you set a conditional breakpoint in GDB?</strong></summary>

By specifying `break [location] if [condition]`. For example: `break process_item if item_id == 42`.
</details>

<details>
<summary><strong>8. What does the `-g3` compiler flag provide that `-g` does not?</strong></summary>

`-g3` instructs the compiler to generate extra DWARF debugging information including macro definitions (`#define`). In GDB, this allows inspecting macros with `macro expand MACRO_NAME` and `info macro`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Stepping and Inspection</strong></summary>

**Goal**: Given a program with an integer array, set a breakpoint in the summation loop, print the array pointer, and inspect the accumulator after 3 iterations.

**Solution**:
```text
$ gcc -g -O0 loop.c -o loop
$ gdb ./loop
(gdb) break main
(gdb) run
(gdb) list
(gdb) break 12 if i == 3
(gdb) continue
(gdb) print i
$1 = 3
(gdb) print sum
$2 = 6
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Catching a Null Pointer Dereference with Core Dumps</strong></summary>

**Goal**: Write a C program that dereferences a null pointer. Configure your Linux environment to produce a core dump, and identify the faulty source line using GDB.

**Solution**:
```c
/* crash.c */
#include <stdio.h>

void cause_crash(int *ptr) {
    *ptr = 42; /* Null dereference */
}

int main(void) {
    int *p = NULL;
    cause_crash(p);
    return 0;
}
```
Commands:
```bash
ulimit -c unlimited
gcc -g -O0 crash.c -o crash
./crash
# Segmentation fault (core dumped)
gdb ./crash core
(gdb) bt
# #0  0x0000000000401132 in cause_crash (ptr=0x0) at crash.c:5
# #1  0x0000000000401156 in main () at crash.c:10
(gdb) print ptr
# $1 = (int *) 0x0
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Debugging Memory Corruption with Hardware Watchpoints</strong></summary>

**Goal**: Diagnose an out-of-bounds write from an adjacent array that silently corrupts a neighboring structure's integer field using GDB hardware watchpoints.

**Solution**:
```c
#include <stdio.h>

struct Data {
    char buffer[8];
    int security_flag;
};

void corrupt(struct Data *d) {
    for (int i = 0; i <= 8; ++i) { /* Off-by-one bug! Writes into security_flag */
        d->buffer[i] = 0;
    }
}

int main(void) {
    struct Data d;
    d.security_flag = 1;
    corrupt(&d);
    return 0;
}
```
GDB commands:
```text
(gdb) break main
(gdb) run
(gdb) watch d.security_flag
Hardware watchpoint 2: d.security_flag
(gdb) continue
Continuing.

Hardware watchpoint 2: d.security_flag
Old value = 1
New value = 0
corrupt (d=0x7fffffffe340) at buffer.c:10
10          d->buffer[i] = 0;
(gdb) print i
$1 = 8  <-- Instantly exposes the exact off-by-one iteration index!
```
</details>

---

## 12. Summary / Cheat Sheet

- `gcc -g3 -O0`: Always use to generate full DWARF symbols without aggressive inlining.
- `break file:line if cond`: Set targeted conditional breakpoints.
- `watch *addr`: Hardware watchpoint halting immediately on unauthorized memory writes.
- `bt full`: Print every frame in the call stack with local variables.
- `x/4xw $rsp`: Inspect stack memory as 4 hexadecimal 32-bit words.
- `Ctrl+X Ctrl+A`: Toggle TUI visual source code mode.

---

## 13. Further Reading

- Richard Stallman, Roland Pesch, Stan Shebs: *Debugging with GDB: The GNU Source-Level Debugger*.
- Peter Jay Salzman: *The Art of Debugging with GDB, DDD, and Eclipse*.
- GDB Internals Manual: Breakpoint Handling and Target Architecture Layers.

---

[← Previous: 05-c-standards-history.md](./05-c-standards-history.md) | [Section Index](./README.md) | [Next: 07-makefile-basics.md](./07-makefile-basics.md) →
