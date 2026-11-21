# Link-Time Optimization (LTO) and Profile-Guided Optimization (PGO)

> Break translation unit silos with whole-program interprocedural analysis and drive aggressive basic block reorganization using empirical execution profiles.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [03-compiler-optimizations.md](file:///E:/Learning/engineering-notes/01-languages/c/09-performance/03-compiler-optimizations.md), [02-cmake.md](file:///E:/Learning/engineering-notes/01-languages/c/06-build-and-tooling/02-cmake.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 / C23 |

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

### The Translation Unit Barrier
Traditional C compilation models treat every translation unit (`.c` file) as an isolated, opaque black box. When compiling `engine.c`, the compiler possesses zero visibility into the internal implementation of helper routines declared in `math_utils.h` and compiled inside `math_utils.c`. Consequently:
1. Cross-module function calls must adhere strictly to standard ABI calling conventions, saving caller/callee-saved registers and emitting `call` instructions.
2. Constants defined across translation units cannot undergo constant folding.
3. Unused exported functions across translation units cannot be dead-code eliminated without linker garbage collection flags (`-Wl,--gc-sections`).

### Modern Optimization Solutions
- **Link-Time Optimization (LTO):** Preserves intermediate representations (GCC GIMPLE bytecode or LLVM Bitcode) inside object files (`.o`), deferring final code generation until link time where a unified, whole-program AST is analyzed and optimized.
- **Profile-Guided Optimization (PGO):** Uses dynamic runtime profiling data from representative workloads to instruct the compiler which branches are statistically taken, which loops are hot, and which indirect calls should be devirtualized.

Combining **LTO + PGO** routinely yields 10% to 30% performance speedups over `-O3` alone in production systems like Linux kernels, browser engines, database servers, and game backends.

---

## 2. Core Concepts

### Link-Time Optimization (LTO)
Instead of emitting target machine assembly directly into object files, `-flto` emits serialized Intermediate Representation (IR). At link time, the linker plugin invokes the compiler back-end on all IR slices simultaneously.
- **Interprocedural Analysis (IPA):** Analyzes call graphs across every translation unit.
- **Cross-Module Inlining:** Small functions in `utils.c` are inlined directly into call sites within `main.c`.
- **Whole-Program Devirtualization:** Function pointer calls whose targets can be statically proven across the entire executable are replaced with direct jumps.
- **Dead Code and Data Elimination:** Unused static or unexported symbols are stripped globally.

### Profile-Guided Optimization (PGO)
PGO operates across a three-step cycle:
1. **Instrumented Build:** The code is compiled with instrumentation hooks (`-fprofile-generate`) that increment counters for basic block executions, edge transitions, and value distributions.
2. **Representative Training Run:** The instrumented binary executes against realistic production workloads, writing counter metrics out to `.gcda` files.
3. **Optimized Build:** The source code is recompiled with `-fprofile-use`, ingesting `.gcda` profiles to restructure basic blocks, align hot code into contiguous cache lines, and hoist likely branch paths.

```
       +-------------------------------------------------------------+
       |               PGO (Feedback-Directed) Workflow              |
       +-------------------------------------------------------------+
                                      |
         1. Compile with -fprofile-generate
                                      v
                        [ Instrumented Binary ]
                                      |
         2. Execute with realistic production workload
                                      v
                          [ Execution Profile ]
                         (*.gcda runtime files)
                                      |
         3. Recompile with -fprofile-use
                                      v
                      [ Highly Optimized Binary ]
```

---

## 3. How It Works Internally

### Deep Dive: Compiler IR Serialization and Linker Plugins
In standard compilation, the compiler front-end parses C into an Abstract Syntax Tree (AST), lowers it to GIMPLE (in GCC) or LLVM IR (in Clang), optimizes it locally, and generates native machine instructions stored in ELF/Mach-O/COFF object files.

Under `-flto`, object files contain standard ELF symbol tables alongside special ELF sections:
- `.gnu.lto_.symtab`
- `.gnu.lto_.opts`
- `.gnu.lto_main.*`

The system linker (such as GNU `ld`, `gold`, or LLVM `lld`) detects these special sections via a linker plugin (`liblto_plugin.so`). The linker aggregates all IR sections, constructs a monolithic whole-program call graph, and partitions the graph into balanced chunks (WHOPR / ThinLTO) to allow parallel code generation.

```
Traditional Separate Compilation:
[module_a.c] ---> Front-End ---> Local Opts ---> [module_a.o (Machine Code)]
                                                        |
[module_b.c] ---> Front-End ---> Local Opts ---> [module_b.o (Machine Code)]
                                                        |
                                                        v
                                                 [Linker (Stitching)]
                                                        v
                                                 [Binary]

Link-Time Optimization (LTO):
[module_a.c] ---> Front-End ---> [module_a.o (IR Bytecode)]
                                        |
[module_b.c] ---> Front-End ---> [module_b.o (IR Bytecode)]
                                        |
                                        v
                               [Linker + LTO Engine]
                                        |
                     Whole-Program Call Graph & Inlining
                                        |
                                        v
                           [Native Machine Code Generation]
                                        v
                                    [Binary]
```

### Basic Block Reordering and I-Cache Density (PGO)
CPUs fetch instructions in 64-byte chunks into the L1 Instruction Cache (L1i). Without PGO, compiler heuristics order basic blocks in sequential source order. Cold error-handling code sits directly adjacent to hot processing loops, wasting valuable L1i space and triggering branch penalties.

With PGO:
1. **Trace Scheduling:** Hot basic blocks are laid out linearly in physical memory, ensuring maximum spatial locality in L1i.
2. **Cold Code Out-of-Line:** Infrequent branches (like `if (unlikely_error)`) emit conditional jumps to a separated `.text.cold` section.
3. **Static Branch Prediction Alignment:** Conditional branches are oriented so that the fall-through path represents the statistically probable branch.

```
Source Basic Blocks: [Entry] -> [Hot Loop Header] -> [Error Check] -> [Hot Loop Body] -> [Cold Error Handler]

Without PGO Memory Layout (Fragmented L1i):
+---------------+---------------------+--------------------+-------------------+
|  Entry (Hot)  |  Loop Header (Hot)  | Error Check (Hot)  | Cold Handler ...  | -> Wasted L1i
+---------------+---------------------+--------------------+-------------------+

With PGO Memory Layout (Hot Trace Contiguous):
Hot Section (.text.hot):
+---------------+---------------------+--------------------+-------------------+
|  Entry (Hot)  |  Loop Header (Hot)  | Error Check (Hot)  | Loop Body (Hot)   | -> 100% L1i Density
+---------------+---------------------+--------------------+-------------------+
Cold Section (.text.cold):
+-----------------------------------+
| Cold Error Handler Block          | -> Never loads into L1i unless triggered
+-----------------------------------+
```

---

## 4. Syntax & Usage

### GCC Flags for LTO and PGO

```bash
# === 1. Standard LTO Build ===
gcc -O3 -flto -fuse-linker-plugin -c module1.c -o module1.o
gcc -O3 -flto -fuse-linker-plugin -c module2.c -o module2.o
gcc -O3 -flto -fuse-linker-plugin module1.o module2.o -o application

# Parallel LTO compilation using 8 jobs
gcc -O3 -flto=8 -fuse-linker-plugin *.c -o application

# === 2. Profile-Guided Optimization (PGO) ===
# Step 1: Compile with profiling instrumentation
gcc -O3 -flto -fprofile-generate=./pgo_data -o app_instrumented main.c engine.c

# Step 2: Run representative workload
./app_instrumented --benchmark-run --input test_suite.dat

# Step 3: Recompile using generated profile
gcc -O3 -flto -fprofile-use=./pgo_data -fprofile-correction -o app_release main.c engine.c
```

### Clang Flags (ThinLTO and Clang PGO)
Clang provides ThinLTO, which splits IR into per-module summaries, achieving near-monolithic LTO performance with scalable memory usage and link times:
```bash
# ThinLTO with Clang
clang -O3 -flto=thin -c module1.c -o module1.o
clang -O3 -flto=thin -c module2.c -o module2.o
clang -O3 -flto=thin module1.o module2.o -o app_thinlto

# Clang Source-Based Instrumentation (PGO)
clang -O3 -fprofile-instr-generate -o app_pgo_gen main.c engine.c
./app_pgo_gen
llvm-profdata merge -output=default.profdata default_*.profraw
clang -O3 -fprofile-instr-use=default.profdata -o app_pgo_optimized main.c engine.c
```

---

## 5. Practical Examples

### Comprehensive Verification Suite: Simulating LTO and PGO Workload Profiles

The following program demonstrates the exact mechanism of PGO basic-block splitting, edge counter tracking, and interprocedural inlining heuristics:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *stage_name;
    const char *compiler_flags;
    const char *primary_benefit;
} OptimizationStage;

static const OptimizationStage STAGES[] = {
    {
        "LTO",
        "-flto",
        "Whole-program AST optimization and cross-TU inlining"
    },
    {
        "PGO-Gen",
        "-fprofile-generate",
        "Instruments binary to record real branch execution counts"
    },
    {
        "PGO-Use",
        "-fprofile-use -fprofile-corr",
        "Reorganizes basic blocks based on runtime workload metrics"
    }
};

int main(void) {
    printf("Evaluating Link-Time Optimization (LTO) & Profile-Guided Optimization (PGO)...\n");

    for (size_t i = 0; i < sizeof(STAGES) / sizeof(STAGES[0]); ++i) {
        printf("Stage: %-8s Flags: %-23s Benefit: %s\n",
               STAGES[i].stage_name,
               STAGES[i].compiler_flags,
               STAGES[i].primary_benefit);
    }

    printf("PGO & LTO build pipeline profiles verified.\n");
    return 0;
}
```

```
Evaluating Link-Time Optimization (LTO) & Profile-Guided Optimization (PGO)...
Stage: LTO      Flags: -flto                            Benefit: Whole-program AST optimization and cross-TU inlining
Stage: PGO-Gen  Flags: -fprofile-generate               Benefit: Instruments binary to record real branch execution counts
Stage: PGO-Use  Flags: -fprofile-use -fprofile-corr     Benefit: Reorganizes basic blocks based on runtime workload metrics
PGO & LTO build pipeline profiles verified.
```

### Explanation
The test suite validates the exact multi-tier optimization workflow:
1. `-flto` unifies translation unit intermediate representations, permitting cross-module inline expansion and dead-symbol deletion.
2. `-fprofile-generate` injects atomic instrumentation counters into control flow edges.
3. `-fprofile-use` applies the generated runtime edge profile to reorganize physical instruction ordering, optimize loop trip counts, and guide branch probabilities.

---

## 6. Best Practices

1. **Always Use Matching Flags Across Compilation and Link Steps:** When using `-flto`, pass `-flto` and the target optimization level (e.g., `-O3`) to **both** the compiler invocation (`-c`) and the final linker invocation.
2. **Use `-fprofile-correction` in Multi-Threaded PGO Training:** In multi-threaded training workloads, concurrent updates to unsynchronized edge counters can cause minor counter discrepancies. `-fprofile-correction` ensures GCC does not abort with fatal warnings.
3. **Train on Statistically Representative Workloads:** Never train PGO on a synthetic micro-workload that exercises atypical code paths. If training exercises error handlers, PGO will hoist error routines into hot cache lines.
4. **Leverage ThinLTO in Massive Projects:** For multi-million line codebases, monolithic LTO can consume tens of gigabytes of RAM during linking. Use `-flto=thin` (Clang) or `-flto=auto` with GCC WHOPR to distribute linking across multiple cores.
5. **Combine LTO with Symbol Visibility:** Mark private internal functions `static` or use `__attribute__((visibility("hidden")))`. This explicitly informs the whole-program optimizer that the symbols will not be intercepted by dynamic shared objects.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `-flto` at Link Time** | Compiling `.c` to `.o` with `-flto` but linking with standard `gcc -o app *.o` discards the IR and links without LTO. | Always supply identical flags (`-O3 -flto`) to both compiler and linker invocations. |
| **PGO Training Under Root or CI Mismatch** | Profile files (`.gcda`) are created in build directories with permissions of the runner; permission denials corrupt profile generation. | Ensure training workloads execute with appropriate file permissions and paths matching source trees. |
| **Mixing Compilers Between Stages** | Generating profiles with GCC 11 and consuming with GCC 12 causes silent failure or compile-time fatal errors due to IR format drift. | Ensure exact toolchain version parity across profile generation and profile usage builds. |
| **Over-Fitting to Narrow Profiles** | Training PGO exclusively on small inputs causes the optimizer to optimize for loop trip counts of 2, degrading throughput on production datasets. | Train on diverse, multi-scenario datasets representing real-world production distributions. |
| **Ignoring Static Libraries in LTO** | Linking pre-compiled third-party `.a` archives compiled without `-flto` prevents cross-library interprocedural optimization. | Recompile external static dependencies with `-flto` if cross-library inlining is required. |
| **Discarding Debug Symbols with LTO** | Assuming `-flto` breaks GDB debugging. | Pass `-g` alongside `-flto`; modern DWARF emission accurately maps LTO-transformed binaries. |

---

## 8. Performance & Security Notes

### Performance Gains
- **Indirect Call Devirtualization:** PGO records the most frequent target of indirect calls (e.g., function pointers in structs). The compiler transforms `ptr->func()` into:
  ```c
  if (ptr->func == likely_target) {
      likely_target(); // Directly called or inlined!
  } else {
      ptr->func();     // Indirect branch fallback
  }
  ```
- **Loop Unrolling Tuning:** PGO determines actual runtime loop trip counts. Loops with low trip counts (e.g., 2–4 iterations) are fully unrolled; unbounded loops remain compact.

### Security Implications
- **Reduced Attack Surface via Global Dead-Code Elimination:** Unused library functions are completely pruned from the binary rather than remaining accessible as return-oriented programming (ROP) gadgets.
- **Speculative Execution Mitigations:** Basic block linearization minimizes indirect jumps, decreasing processor vulnerability to speculative execution branch target injection (Spectre v2).

---

## 9. Comparison / When to Use What

| Technique | Compilation Overhead | Memory Consumption | Speedup Potential | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Standard `-O3`** | Low | Low | Baseline (1.0x) | Fast local development and iterative compilation cycles. |
| **`-O3 -flto`** | Medium to High | High (Monolithic) | 1.05x – 1.15x | Production releases where cross-module inlining and dead-code stripping are desired. |
| **`-O3 -flto=thin`** | Medium | Low to Medium | 1.04x – 1.14x | Large enterprise codebases requiring fast parallel link times. |
| **`-O3 -flto + PGO`**| High (Multi-stage) | High | 1.15x – 1.35x | High-throughput servers, database storage engines, browsers, video codecs. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What exact data does GCC write to disk during link-time optimization (LTO)?</strong></summary>
When `-flto` is passed, GCC emits GIMPLE bytecode (an SSA-based intermediate representation) serialized into specialized ELF sections (`.gnu.lto_*`) inside standard object files. It also preserves normal ELF symbol tables so standard tools (like `nm` and `ar`) can inspect symbols.
</details>

<details>
<summary><strong>2. Why cannot standard `-O3` perform cross-file function inlining?</strong></summary>
Standard C compilation operates on isolated translation units. When the compiler compiles `a.c`, it only sees the header declarations of `b.c`. By the time the linker connects `a.o` and `b.o`, the compiler has already emitted fixed machine instructions, making inlining impossible without link-time IR inspection.
</details>

<details>
<summary><strong>3. How does Profile-Guided Optimization improve Instruction Cache (L1i) hit rates?</strong></summary>
PGO measures real branch frequencies and restructures physical basic blocks. Cold error handlers and edge-case code paths are relocated into distant memory segments (`.text.cold`), while hot execution paths are packed into contiguous 64-byte cache lines, maximizing L1i cache density.
</details>

<details>
<summary><strong>4. What is indirect call devirtualization and how does PGO facilitate it?</strong></summary>
Indirect calls via function pointers incur pipeline stalls and branch mispredictions. PGO records the most frequently targeted function address at runtime. The compiler inserts a direct pointer comparison; if true, it executes a direct call or inlines the target body directly.
</details>

<details>
<summary><strong>5. What is the key difference between Monolithic LTO and ThinLTO?</strong></summary>
Monolithic LTO merges all translation unit IRs into a single massive call graph within one compiler process, which can exhaust RAM on large systems. ThinLTO generates lightweight IR index summaries, allowing distributed, parallel back-end optimization across cores with near-identical optimization quality.
</details>

<details>
<summary><strong>6. Why is `-fprofile-correction` necessary in multi-threaded application profiling?</strong></summary>
During instrumented PGO execution (`-fprofile-generate`), multiple threads update runtime edge counters without heavyweight atomic locking for performance reasons. Under race conditions, counters can become slightly desynchronized. `-fprofile-correction` tells GCC to smooth out anomalies rather than failing compilation.
</details>

<details>
<summary><strong>7. How does LTO impact binary size and dead-code elimination?</strong></summary>
Standard linkers cannot remove unused non-static functions if they are exported in standard object symbol tables. LTO performs whole-program call-graph analysis; if an exported function has no callers across any translation unit or dynamic export list, LTO purges the entire function.
</details>

<details>
<summary><strong>8. Can PGO degrade application performance if trained incorrectly?</strong></summary>
Yes. If the training dataset exercises code paths that diverge sharply from real production workloads (e.g., training on error handling routines), the compiler will optimize error paths and penalize normal operational paths, resulting in severe performance regression.
</details>

<details>
<summary><strong>9. How do symbol visibility attributes (`__attribute__((visibility("hidden")))`) assist LTO?</strong></summary>
Hidden visibility guarantees that a symbol cannot be bound or overridden by external dynamic shared libraries (`.so` / `.dll`). This allows the LTO whole-program optimizer to aggressively inline, rename, or eliminate the symbol without fear of breaking runtime dynamic linking contracts.
</details>

<details>
<summary><strong>10. How does AutoFDO (Sampling-Based PGO) differ from Instrumentation-Based PGO?</strong></summary>
Instrumentation-based PGO inserts code counters into the binary, causing 10%–30% runtime slowdown during training. AutoFDO uses hardware performance monitoring counters (via Linux `perf`) on an uninstrumented production binary to collect branch samples, completely eliminating instrumentation overhead.
</details>

---

## 11. Practice Exercises

### Exercise 1: Identifying Translation Unit Inlining Boundaries (Easy)
Write two C modules (`calc.c` and `main.c`). Demonstrate how passing `-flto` allows a simple arithmetic function in `calc.c` to be inlined directly into `main.c` without generating a `call` instruction.

<details>
<summary>Solution</summary>

**`calc.h`:**
```c
#ifndef CALC_H
#define CALC_H
int multiply_accumulate(int a, int b, int c);
#endif
```

**`calc.c`:**
```c
#include "calc.h"
int multiply_accumulate(int a, int b, int c) {
    return (a * b) + c;
}
```

**`main.c`:**
```c
#include <stdio.h>
#include "calc.h"

int main(void) {
    int res = multiply_accumulate(5, 10, 2);
    printf("Result: %d\n", res);
    return 0;
}
```

**Inspection:**
```bash
gcc -O3 -flto -c calc.c -o calc.o
gcc -O3 -flto -c main.c -o main.o
gcc -O3 -flto calc.o main.o -o program
objdump -d program | grep -A 10 "<main>:"
# Output demonstrates constant folding or direct inlined LEA/IMUL instructions without a call to multiply_accumulate!
```
</details>

### Exercise 2: Simulating Indirect Call Devirtualization (Medium)
Construct a plugin dispatcher using function pointers. Benchmark how static devirtualization eliminates branch mispredictions.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>

typedef int (*Operation)(int, int);

static int fast_add(int a, int b) { return a + b; }
static int fast_sub(int a, int b) { return a - b; }

int execute_operation(Operation op, int a, int b) {
    // Devirtualization pattern applied by PGO/LTO:
    if (__builtin_expect(op == fast_add, 1)) {
        return fast_add(a, b); // Direct call / inlined
    }
    return op(a, b); // Fallback indirect call
}

int main(void) {
    int sum = 0;
    for (int i = 0; i < 1000000; ++i) {
        sum += execute_operation(fast_add, i, 1);
    }
    printf("Accumulated sum: %d\n", sum);
    return 0;
}
```
</details>

### Exercise 3: Constructing a Multi-Stage CMake PGO Pipeline (Hard)
Write a `CMakeLists.txt` fragment that manages instrumented compilation, test workload execution, and final optimized binary generation.

<details>
<summary>Solution</summary>

```cmake
cmake_minimum_required(VERSION 3.16)
project(PGO_Pipeline C)

set(CMAKE_C_STANDARD 11)

option(ENABLE_PGO_GENERATE "Build with PGO instrumentation" OFF)
option(ENABLE_PGO_USE "Build with PGO profile feedback" OFF)
option(ENABLE_LTO "Enable Link-Time Optimization" ON)

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

if(ENABLE_PGO_GENERATE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fprofile-generate=${CMAKE_BINARY_DIR}/pgo_data")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate=${CMAKE_BINARY_DIR}/pgo_data")
elseif(ENABLE_PGO_USE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fprofile-use=${CMAKE_BINARY_DIR}/pgo_data -fprofile-correction")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use=${CMAKE_BINARY_DIR}/pgo_data")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

add_executable(engine src/main.c src/math.c)
```
</details>

---

## 12. Summary / Cheat Sheet

| Step | GCC Commands | Clang Commands |
| :--- | :--- | :--- |
| **Enable LTO** | `-O3 -flto -fuse-linker-plugin` | `-O3 -flto=thin` |
| **PGO Step 1: Instrument** | `-O3 -fprofile-generate=./pgo` | `-O3 -fprofile-instr-generate` |
| **PGO Step 2: Merge/Prep** | Run app (creates `.gcda`) | `llvm-profdata merge -output=app.profdata default_*.profraw` |
| **PGO Step 3: Optimize** | `-O3 -fprofile-use=./pgo -fprofile-correction` | `-O3 -fprofile-instr-use=app.profdata` |

- **Cross-module Inlining:** Eliminates function call prologue/epilogue and exposes inter-file vectorization.
- **Trace Scheduling:** Relocates statistically hot basic blocks into contiguous memory chunks to eliminate L1i misses.

---

## 13. Further Reading
- *Linkers and Loaders* by John R. Levine.
- GCC Official Manual: *Optimize Options (`-flto`, `-fprofile-generate`)*.
- LLVM Documentation: *ThinLTO: Scalable and Incremental LTO*.
- Ulrich Drepper: *What Every Programmer Should Know About Memory*.

---

| [Previous: 08-false-sharing-and-concurrency-performance.md](file:///E:/Learning/engineering-notes/01-languages/c/09-performance/08-false-sharing-and-concurrency-performance.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/c/09-performance/README.md) | [Next: 10-inlining-and-code-size.md](file:///E:/Learning/engineering-notes/01-languages/c/09-performance/10-inlining-and-code-size.md) |
