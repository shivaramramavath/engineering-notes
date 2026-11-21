# C Curriculum Completion & Expansion Plan (C_COMPLETION_PLAN.md)

> **Location:** `01-languages/c/`  
> **Status:** Plan Stage — Awaiting User Approval before Phase 3 Execution.  
> **Standards Baseline:** ISO C11 / C17 with explicit C23 modernization boxes (`nullptr`, `constexpr`, `typeof`, `#embed`, attributes, `<stdckdint.h>`, `<stdbit.h>`).

---

## 1. Concrete Plan for Each of the 40 Existing Notes

Every note will be expanded to the **300–800 line** master standard, restructured into the **13 numbered sections**, equipped with metadata tables, Table of Contents, ASCII/mermaid memory diagrams, real compiled examples in an `examples/` subfolder, common pitfalls tables, `<details>` interview questions, and practice exercises with full verified solutions.

### Module 00: Setup
- **`00-setup/01-toolchain.md`**:
  - Restructure into 13 sections. Expand from 89 lines to ~350 lines.
  - Add toolchain architecture diagram (preprocessor → compiler cc1 → assembler as → linker ld).
  - Add detailed MinGW vs MinGW-w64 vs MSYS2 vs WSL2 breakdown for Windows.
  - Add C11/C17/C23 standard selection flags (`-std=c11`, `-std=c17`, `-std=c2x`, `-std=c23`).
  - Add LLVM Clang toolchain and linker alternatives (`lld`, `mold`).
  - Add interview Qs on how toolchains differ from interpreters, cross-compilation basics.
  - Add exercises: configuring dual GCC/Clang builds with static analysis flags.

- **`00-setup/02-editor-setup.md`**:
  - Restructure into 13 sections. Expand from 84 lines to ~320 lines.
  - Add `compile_commands.json` generation via CMake and `bear`.
  - Add `.clangd` configuration file syntax with `-std=c23` and warning flags.
  - Add `.clang-format` baseline configuration (LLVM / Google / WebKit styles).
  - Add VS Code / Neovim debug launch configurations for GDB and LLDB.
  - Add interview Qs on language server protocol (LSP) mechanics and AST parsing.

- **`00-setup/03-compilation-process.md`**:
  - Restructure into 13 sections. Expand from 104 lines to ~450 lines.
  - Add complete ASCII memory and file transformation pipeline diagram.
  - Add deep dive into intermediate files: `.i` (preprocessed), `.s` (assembly), `.o` (relocatable object), binary executable.
  - Add ELF format inspection using `readelf -h`, `readelf -S`, `objdump -d`, `nm`.
  - Explain `.text`, `.rodata`, `.data`, `.bss`, symbol tables, and relocation entries.
  - Add interview Qs: static vs dynamic linking resolution, symbol collision (`extern` vs `static`).

- **`00-setup/04-first-program.md`**:
  - Restructure into 13 sections. Expand from 115 lines to ~380 lines.
  - Fix broken code block #3 where markdown was wrapped inside a C code fence.
  - Contrast `int main(void)` vs `int main(int argc, char *argv[])` vs freestanding `void main()`.
  - Add C23 update: empty parameter list `int main()` is now strictly equivalent to `(void)`.
  - Add C runtime (CRT) entry point diagram (`_start` → `__libc_start_main` → `main` → `exit`).
  - Add interview Qs on program termination, `atexit()` handlers, and exit status conventions.

- **`00-setup/05-c-standards-history.md`**:
  - Restructure into 13 sections. Expand from 94 lines to ~420 lines.
  - Add comprehensive comparative matrix across K&R, C89/C90, C99, C11, C17, and C23.
  - Detail C23 additions: `nullptr`, `constexpr`, `typeof`, `#embed`, `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`, `<stdckdint.h>`, `<stdbit.h>`, zero-size realloc deprecation.
  - Detail removed features: `gets()` (C11), `K&R` function declarators (C23).
  - Add interview Qs on why C evolves conservatively and ABI stability consequences.

---

### Module 01: Fundamentals
- **`01-fundamentals/01-program-structure.md`**:
  - Fix syntax error in block #2 (unclosed `main`). Expand to ~400 lines.
  - Add visual diagram of translation units, headers, source files, and linkage scopes.
  - Detail forward declarations, prototypes, definition vs declaration.
  - Add interview Qs: One Definition Rule (ODR) in C, multiple inclusion guard mechanisms.

- **`01-fundamentals/02-data-types.md`**:
  - Expand to ~500 lines. Add ILP32 vs LP64 vs LLP64 memory model comparison diagram.
  - Add C23 `nullptr_t`, native `bool`, binary literals (`0b...`), digit separators (`'`).
  - Add `<stdint.h>` exact-width, least-width, and fast types with format specifiers (`PRId32`, `PRIu64`).
  - Add interview Qs: why `sizeof(char)` is defined as 1 byte regardless of bit width, `ptrdiff_t` vs `size_t`.

- **`01-fundamentals/03-integer-representation.md`**:
  - Expand to ~450 lines. Add two's complement bitwheel diagram, sign extension, and arithmetic shifts.
  - Add C23 requirement: two's complement is now universally mandated (one's complement and sign-magnitude officially removed).
  - Detail safe overflow checking: manual pre-condition checks vs C23 `<stdckdint.h>` (`ckd_add`, `ckd_mul`).
  - Add interview Qs: why signed overflow is UB while unsigned arithmetic wraps modulo $2^N$.

- **`01-fundamentals/04-variables-constants.md`**:
  - Fix block #2 (missing header, uninitialized variable warning). Expand to ~400 lines.
  - Add storage duration (automatic, static, allocated, thread) vs scope (block, file, function).
  - Add C23 `constexpr` objects vs `const` qualifiers vs `#define` constants.
  - Add interview Qs: string literal placement in `.rodata` vs stack array initialization.

- **`01-fundamentals/05-operators.md`**:
  - Expand to ~450 lines. Add operator precedence and associativity table.
  - Modernize sequence points to C11/C17/C23 "sequenced before", "sequenced after", and unsequenced modifications (UB).
  - Add short-circuit evaluation semantics and comma operator mechanics.
  - Add interview Qs on order of evaluation of function arguments (unspecified).

- **`01-fundamentals/06-type-conversions.md`**:
  - Expand to ~480 lines. Add visual ladder of usual arithmetic conversions.
  - Detail the subtle integer promotion trap: `unsigned short` promoted to `signed int` on 32-bit platforms.
  - Add sign-conversion vulnerabilities and safe casting patterns.
  - Add interview Qs: comparing signed and unsigned integers (`-1 < 1U` evaluates to false).

- **`01-fundamentals/07-control-flow.md`**:
  - Expand to ~400 lines. Add jump table assembly representation for `switch` statements.
  - Add C23 `[[fallthrough]]` attribute and Duff's Device unrolling mechanics.
  - Add interview Qs: `goto` in Linux kernel error handling cleanup patterns.

- **`01-fundamentals/08-functions.md`**:
  - Expand to ~450 lines. Add stack frame layout diagram (return address, saved base pointer, local variables).
  - Detail pass-by-value semantics, simulating pass-by-reference with pointers.
  - Detail C23 transition: empty parameter list `void f()` is now strictly zero arguments.
  - Add interview Qs: tail-call optimization, inline functions (`extern inline` vs `static inline`).

- **`01-fundamentals/09-input-output.md`**:
  - Expand to ~500 lines. Add stream buffering diagram (unbuffered `stderr`, line-buffered `stdout` on TTY, block-buffered).
  - Detail safe input recipes: `fgets` + `sscanf`, robust newline stripping, flushing leftover input buffer.
  - Add format string security risks (`%n` exploit vectors).
  - Add interview Qs: `printf` return value, `fflush(stdin)` undefined behavior vs `fflush(stdout)`.

---

### Module 02: Core Concepts
- **`02-core-concepts/01-arrays.md`**:
  - Expand to ~500 lines. Add contiguous memory layout diagram.
  - Detail array decay to pointer, `sizeof(array)` vs `sizeof(pointer)`.
  - Add Variable Length Arrays (VLAs) discussion: optional in C11/C17/C23, security hazards (stack overflow).
  - Add interview Qs: why arrays cannot be assigned directly or returned by value.

- **`02-core-concepts/02-strings.md`**:
  - Expand to ~550 lines. Add null-terminated byte string memory representation diagram.
  - Replace unchecked `strcpy` examples with bounded, secure patterns (`snprintf`, `strlcpy`, bounds checking).
  - Detail string literal immutability (`char *s = "text"` vs `char s[] = "text"`).
  - Add C23 `#embed` feature for embedding static string/binary assets.
  - Add interview Qs: `strlen` time complexity $O(N)$, off-by-one null terminator bugs.

- **`02-core-concepts/03-pointers.md`**:
  - Expand to ~600 lines. Add comprehensive pointer memory address and indirection diagrams.
  - Add C23 `nullptr` vs `NULL` vs `(void*)0`.
  - Detail strict aliasing rule, `char *` aliasing exception, and `uintptr_t` conversions.
  - Add interview Qs: pointer arithmetic scaling by `sizeof(*p)`, wild pointers vs dangling pointers.

- **`02-core-concepts/04-declarations-typedef.md`**:
  - Expand to ~420 lines. Add the "Clockwise/Spiral Rule" with visual walkthrough.
  - Detail complex declarators: arrays of function pointers, pointers to arrays.
  - Add C23 `typeof` and `typeof_unqual` usage.
  - Add interview Qs: `typedef` vs `#define` for types, opaque types in header files.

- **`02-core-concepts/05-type-qualifiers.md`**:
  - Expand to ~480 lines. Add assembly code comparison showing optimization with `restrict`.
  - Detail `const` pointer permutations (`const int *`, `int * const`, `const int * const`).
  - Detail `volatile` semantics: memory-mapped I/O, signal handlers, preventing compiler elision.
  - Add interview Qs: why `volatile` does NOT provide thread synchronization or memory ordering.

- **`02-core-concepts/06-structs-unions-enums.md`**:
  - Expand to ~550 lines. Add visual struct memory alignment and padding offset diagram.
  - Add `alignas`, `alignof` (`<stdalign.h>` in C11, native keywords in C23).
  - Add anonymous structs/unions (C11), type-punning rules, and C23 enum underlying types.
  - Add interview Qs: calculating struct padding, tagged unions.

- **`02-core-concepts/07-scope-storage-classes.md`**:
  - Expand to ~450 lines. Add linkage matrix (external, internal, none) and symbol visibility diagram.
  - Detail `static` for file-scope encapsulation, `extern` declarations, `register`.
  - Detail C11 `_Thread_local` / C23 `thread_local`.
  - Add interview Qs: tentative definitions in C, static variables in header files anti-pattern.

- **`02-core-concepts/08-memory-management.md`**:
  - Expand to ~600 lines. Add heap chunk allocation diagram (chunk header, size, payload, boundary tags).
  - Detail `malloc`, `calloc` (overflow protection), `realloc`, `free`.
  - Address C23 deprecation: `realloc(ptr, 0)` behavior is now undefined/implementation-defined.
  - Add common bugs: memory leaks, double free, use-after-free (UAF), heap buffer overflow.
  - Add interview Qs: how `free()` knows how many bytes to release, fragmentation.

- **`02-core-concepts/09-preprocessor.md`**:
  - Expand to ~500 lines. Add preprocessor execution phase diagram.
  - Detail macro pitfalls: operator precedence in macros, double evaluation of arguments.
  - Add stringification `#` and token concatenation `##` operators with robust multi-pass macros.
  - Add C23 `#embed`, `__has_include`, `__has_c_attribute`.
  - Add interview Qs: `do { ... } while(0)` idiom in multi-statement macros.

- **`02-core-concepts/10-command-line-arguments.md`**:
  - Expand to ~400 lines. Add memory layout diagram for `argv` array of pointers and string buffers.
  - Detail POSIX `getopt` and GNU `getopt_long` option parsing.
  - Add environment variables access via `extern char **environ` and `getenv()`.
  - Add interview Qs: memory lifecycle of `argv` strings, handling untrusted command-line inputs.

- **`02-core-concepts/11-file-io.md`**:
  - Expand to ~550 lines. Add FILE structure and OS kernel file table relationship diagram.
  - Detail text vs binary mode across platforms (CRLF vs LF translation on Windows).
  - Detail robust block I/O with `fread`/`fwrite`, `fseeko`/`ftello` for 64-bit file offsets.
  - Add error checking with `ferror`, `feof`, and `clearerr`.
  - Add interview Qs: why `while (!feof(f))` is a bug, atomic file replacement patterns.

---

### Module 03: Intermediate
- **`03-intermediate/01-function-pointers.md`**:
  - Expand to ~500 lines. Add function pointer dispatch table and callback diagram.
  - Detail callback context pattern (`void *user_data`) essential for reentrant libraries.
  - Detail `qsort` and `bsearch` comparators and type safety casting idioms.
  - Add interview Qs: function pointer syntax, implementing object-oriented polymorphism/vtables in C.

- **`03-intermediate/02-multi-file-programs.md`**:
  - Fix multi-file compilation example. Expand to ~450 lines.
  - Provide complete, compilable companion code files in `examples/02-multi-file/`.
  - Detail header design: idempotency (`#ifndef` guards / `#pragma once`), minimizing include dependencies.
  - Detail circular dependency resolution and forward declarations.
  - Add interview Qs: internal vs external linkage, `inline` functions in multi-file projects.

- **`03-intermediate/03-error-handling.md`**:
  - Expand to ~450 lines. Add error propagation architecture diagram.
  - Detail `errno` mechanics, thread-safety with `strerror_r` (POSIX) / `strerror_s` (C11).
  - Detail Result/Status return pattern vs output pointer parameters.
  - Add interview Qs: checking `errno` only when the return code signals failure, setjmp/longjmp error handling trade-offs.

- **`03-intermediate/04-assertions-defensive-coding.md`**:
  - Expand to ~420 lines. Add defensive programming lifecycle diagram.
  - Detail runtime `assert()` and the impact of `-DNDEBUG`.
  - Detail compile-time assertions: C11 `_Static_assert`, C23 single-argument `static_assert(expr)`.
  - Add interview Qs: avoiding side-effects inside assertions (`assert(n++ > 0)` bug).

- **`03-intermediate/05-bit-manipulation.md`**:
  - Expand to ~500 lines. Add bitwise logic, masks, and bit shifting diagrams.
  - Detail bit twiddling hacks: popcount, power-of-two check, isolating lowest set bit.
  - Detail C23 `<stdbit.h>` standard library functions (`stdc_leading_zeros`, `stdc_count_ones`, `stdc_has_single_bit`).
  - Add interview Qs: bitwise endianness independence, signed right-shift implementation divergence.

- **`03-intermediate/06-variadic-functions.md`**:
  - Expand to ~450 lines. Add `<stdarg.h>` stack argument consumption diagram.
  - Detail `va_start`, `va_arg`, `va_end`, and mandatory `va_copy`.
  - Detail C23 modernization: removing the requirement for a named parameter before the ellipsis (`void f(...)`).
  - Add `vprintf`, `vsnprintf` forwarding wrappers and format specifier validation.
  - Add interview Qs: type promotion in variadic arguments (`float` → `double`, `char`/`short` → `int`).

- **`03-intermediate/07-text-processing-parsing.md`**:
  - Expand to ~550 lines. Add state machine parsing diagram.
  - Highlight `strtok()` non-reentrancy and explain thread-safe `strtok_r` (POSIX) / `strtok_s` (C11).
  - Detail robust numeric parsing with `strtol`, `strtod` (handling overflow, underflow, trailing garbage).
  - Build a real recursive-descent expression parser or CSV parser.
  - Add interview Qs: why `atoi` and `atof` should never be used in production code.

- **`03-intermediate/08-unicode-locale.md`**:
  - Expand to ~480 lines. Add UTF-8 variable-length byte sequence encoding diagram.
  - Detail C11 `<uchar.h>` (`char16_t`, `char32_t`, `c16rtomb`, `mbrtoc32`).
  - Add C23 `u8` character and string literals (type changed to `const unsigned char[]`).
  - Add interview Qs: Unicode code points vs UTF-8 code units, string length vs display column width.

- **`03-intermediate/09-time-and-random.md`**:
  - Expand to ~450 lines. Add calendar time, epoch, and monotonic clock comparison diagram.
  - Detail high-resolution time with C11 `timespec_get` and POSIX `clock_gettime(CLOCK_MONOTONIC)`.
  - Detail PRNG vs CSPRNG: why `rand()` is flawed (linear congruential, 15-bit `RAND_MAX` on Windows); introduce PCG / Xoroshiro algorithms.
  - Add interview Qs: generating uniform random numbers within a range without modulo bias.

- **`03-intermediate/11-standard-library.md`**:
  - Expand to ~550 lines. Add comprehensive roadmap of ISO C standard headers.
  - Group headers logically: language support, diagnostics, math, strings, I/O, localization, threading (`<threads.h>`), atomics (`<stdatomic.h>`), C23 additions (`<stdckdint.h>`, `<stdbit.h>`).
  - Add interview Qs: hosted vs freestanding implementations, POSIX extensions vs ISO standard.

---

### Module 03 Data Structures Subfolder
- **`03-intermediate/data-structures/01-linked-lists.md`**:
  - Expand to ~600 lines. Add node memory layout, singly, doubly, and circular list diagrams.
  - Contrast standard pointer lists with Linux kernel style **intrusive linked lists** (`container_of` macro).
  - Add full CRUD operations, reversal, cycle detection (Floyd's Tortoise and Hare).
  - Add interview Qs: cache locality disadvantages vs arrays, merge sort on linked lists.

- **`03-intermediate/data-structures/02-stacks-queues.md`**:
  - Expand to ~550 lines. Add array-based and linked-list-based LIFO/FIFO memory diagrams.
  - Implement dynamic resizing stack and circular ring-buffer queue.
  - Add real-world applications: expression balancing, BFS traversal queue.
  - Add interview Qs: detecting full vs empty condition in circular buffers.

- **`03-intermediate/data-structures/03-trees.md`**:
  - Expand to ~600 lines. Add Binary Search Tree (BST) pointer structure and traversal diagrams.
  - Implement insert, search, in-order/pre-order/post-order, and node deletion (3 cases).
  - Provide conceptual overview of self-balancing trees (AVL rotations, Red-Black color invariants).
  - Add interview Qs: BST degenerate case $O(N)$, finding LCA (Lowest Common Ancestor).

- **`03-intermediate/data-structures/04-hash-tables.md`**:
  - Expand to ~650 lines. Add hash bucket collision resolution diagrams (chaining vs open addressing).
  - Implement hash table with `djb2` hash, dynamic rehashing on load factor threshold (0.75).
  - Detail open addressing with linear and quadratic probing; tombstone deletion.
  - Add interview Qs: collision attack vulnerabilities, SipHash, cache-friendly flat hash maps.

- **`03-intermediate/data-structures/05-graphs.md`**:
  - Expand to ~600 lines. Add Adjacency Matrix vs Adjacency List memory diagrams.
  - Implement BFS (queue-based) and DFS (recursive and iterative) algorithms.
  - Implement cycle detection and Dijkstra's shortest path algorithm.
  - Add interview Qs: topological sort, graph representation memory trade-offs.

---

## 2. Complete Final Tree for `01-languages/c/`

The complete target directory structure incorporates all 40 existing notes (expanded and completed), the new intermediary topics, and the 10 new advanced modules.

```
01-languages/c/
├── README.md                                          # Curriculum roadmap, learning tracks, prerequisites, how to compile
├── C_AUDIT.md                                         # Audit report of repository state
├── C_COMPLETION_PLAN.md                               # This execution plan
├── _templates/
│   └── note-template.md                               # Master 13-section reference template
│
├── 00-setup/
│   ├── README.md                                      # Setup module index and prerequisites
│   ├── 01-toolchain.md
│   ├── 02-editor-setup.md
│   ├── 03-compilation-process.md
│   ├── 04-first-program.md
│   ├── 05-c-standards-history.md
│   ├── 06-debugging-with-gdb.md                       # NEW: GDB breakpoints, stepping, backtrace, watchpoints, TUI
│   ├── 07-makefile-basics.md                          # NEW: Rules, targets, prerequisites, variables, pattern rules
│   ├── 08-compiler-flags-and-warnings.md              # NEW: -Wall -Wextra -Wpedantic -Werror, optimization, debug flags
│   └── 09-reading-assembly-godbolt.md                 # NEW: x86-64 assembly basics, registers, calling conventions, Compiler Explorer
│
├── 01-fundamentals/
│   ├── README.md
│   ├── 01-program-structure.md
│   ├── 02-data-types.md
│   ├── 03-integer-representation.md
│   ├── 04-variables-constants.md
│   ├── 05-operators.md
│   ├── 06-type-conversions.md
│   ├── 07-control-flow.md
│   ├── 08-functions.md
│   ├── 09-input-output.md
│   ├── 10-undefined-unspecified-implementation-defined.md # NEW: Definitions, ISO C citations, compiler exploitation
│   ├── 11-sizeof-alignment-basics.md                  # NEW: sizeof operator, alignof, memory boundaries
│   └── 12-const-correctness-basics.md                 # NEW: Read-only views, pointer qualifiers, API contract design
│
├── 02-core-concepts/
│   ├── README.md
│   ├── 01-arrays.md
│   ├── 02-strings.md
│   ├── 03-pointers.md
│   ├── 04-declarations-typedef.md
│   ├── 05-type-qualifiers.md
│   ├── 06-structs-unions-enums.md
│   ├── 07-scope-storage-classes.md
│   ├── 08-memory-management.md
│   ├── 09-preprocessor.md
│   ├── 10-command-line-arguments.md
│   ├── 11-file-io.md
│   ├── 12-pointer-arithmetic.md                       # NEW: Stepping, scaling, pointer subtraction, array indexing equivalence
│   ├── 13-multi-dimensional-arrays.md                 # NEW: Row-major order, flattened indexing, pointer arrays vs 2D arrays
│   ├── 14-dynamic-arrays-and-realloc.md               # NEW: Geometric resizing vector implementation in C
│   ├── 15-string-h-functions-and-safe-alternatives.md # NEW: Deep dive: memcpy vs memmove, strchr, strstr, safe string libraries
│   ├── 16-bit-fields.md                               # NEW: Syntax, packing, implementation differences, portability traps
│   ├── 17-enums-and-flags.md                          # NEW: Bitmask flags, bitwise operations, C23 fixed underlying types
│   ├── 18-designated-initializers-and-compound-literals.md # NEW: C99/C23 syntax for structs, arrays, inline temporary objects
│   ├── 19-struct-padding-and-alignment.md             # NEW: Offsetof, cache-line splitting, packing directives (#pragma pack)
│   └── 20-memory-layout-stack-heap-data-bss.md        # NEW: Process address space, stack frame mechanics, heap growth
│
├── 03-intermediate/
│   ├── README.md
│   ├── 01-function-pointers.md
│   ├── 02-multi-file-programs.md
│   ├── 03-error-handling.md
│   ├── 04-assertions-defensive-coding.md
│   ├── 05-bit-manipulation.md
│   ├── 06-variadic-functions.md
│   ├── 07-text-processing-parsing.md
│   ├── 08-unicode-locale.md
│   ├── 09-time-and-random.md
│   ├── 10-undefined-behavior-and-sanitizers.md        # Fills Missing Slot 10: ASan, UBSan, Valgrind, UB bug catalog
│   ├── 11-standard-library.md
│   ├── 12-generic-programming.md                      # NEW: void*, _Generic keyword (C11), type-generic macros
│   ├── 13-opaque-pointers-and-adts.md                 # NEW: Incomplete types, encapsulation, information hiding
│   ├── 14-callbacks-and-function-pointer-tables.md    # NEW: State machines, event handlers, dispatch tables
│   ├── 15-setjmp-longjmp.md                           # NEW: Non-local jumps, exception simulation, register preservation hazards
│   ├── 16-inline-restrict-volatile.md                 # NEW: Compiler optimization hints and hardware semantics
│   ├── 17-x-macros-and-advanced-preprocessor.md       # NEW: X-macros for code generation, enum-to-string tables
│   │
│   └── data-structures/
│       ├── README.md
│       ├── 01-linked-lists.md
│       ├── 02-stacks-queues.md
│       ├── 03-trees.md
│       ├── 04-hash-tables.md
│       ├── 05-graphs.md
│       ├── 06-dynamic-array.md                        # NEW: Full generic vector implementation with tests
│       ├── 07-queues-circular-buffer.md               # NEW: Fixed-size ring buffer implementation for embedded/systems
│       ├── 08-heaps-and-priority-queues.md            # NEW: Binary min/max heap, heapify, priority queue interface
│       ├── 09-binary-search-trees.md                  # NEW: Detailed self-balancing trees (AVL rotations, Red-Black theory)
│       ├── 10-tries.md                                # NEW: Prefix tree for dictionary search and IP routing
│       ├── 11-union-find.md                           # NEW: Disjoint-set data structure with path compression & union by rank
│       ├── 12-sorting-algorithms.md                   # NEW: Quicksort, mergesort, heapsort, radix sort implementations & benchmarks
│       ├── 13-searching-algorithms.md                 # NEW: Binary search, interpolation search, exponential search
│       └── 14-algorithm-complexity.md                 # NEW: Big-O, Big-Omega, Big-Theta, cache effects on complexity
│
├── 04-advanced/                                       # NEW MODULE
│   ├── README.md
│   ├── 01-custom-allocators-and-arenas.md             # Linear arena allocator implementation
│   ├── 02-memory-pools.md                             # Fixed-size block pool allocator
│   ├── 03-memory-debugging-valgrind-asan.md           # ASan, MSan, Valgrind Massif memory profiling
│   ├── 04-linking-static-vs-shared-libraries.md       # .a vs .so/.dll, PIC, dlopen/dlsym, symbol visibility
│   ├── 05-abi-and-calling-conventions.md              # System V AMD64 ABI, Microsoft x64, register vs stack passing
│   ├── 06-endianness-and-serialization.md             # Network byte order (htons/ntohs), binary struct packing
│   ├── 07-interfacing-with-assembly.md                # Inline asm (__asm__ volatile), calling asm functions from C
│   └── 08-writing-a-small-library.md                  # Header-only vs compiled, semantic versioning, API stability
│
├── 05-systems-programming/                            # NEW MODULE (POSIX/Linux with Windows Notes)
│   ├── README.md
│   ├── 01-file-descriptors-and-posix-io.md            # open, read, write, close, lseek, O_NONBLOCK
│   ├── 02-processes-fork-exec-wait.md                 # fork, execve, waitpid, process lifecycle, zombie processes
│   ├── 03-signals.md                                  # sigaction, signal masks, async-signal-safe functions
│   ├── 04-pipes-and-ipc.md                            # Anonymous pipes, named pipes (FIFO), UNIX domain sockets
│   ├── 05-pthreads.md                                 # pthread_create, pthread_join, thread attributes, stack size
│   ├── 06-mutex-condvar-semaphores.md                 # Mutex locking, condition variable signaling, POSIX semaphores
│   ├── 07-c11-atomics-and-memory-order.md             # <stdatomic.h>, atomic operations, sequential consistency, relaxed
│   ├── 08-sockets-tcp-udp.md                          # socket, bind, listen, accept, connect, send, recv
│   ├── 09-io-multiplexing-select-poll-epoll.md        # Event-driven I/O, epoll edge-triggered vs level-triggered
│   ├── 10-mmap-and-shared-memory.md                   # mmap, munmap, shared memory (shm_open), memory-mapped files
│   └── 11-daemons-and-system-calls.md                 # Daemonizing processes, nohup, strace tracing system calls
│
├── 06-build-and-tooling/                              # NEW MODULE
│   ├── README.md
│   ├── 01-make-in-depth.md                            # Multi-target Makefiles, automatic dependency generation (-MMD)
│   ├── 02-cmake.md                                    # Modern CMake (CMakeLists.txt, targets, generator expressions)
│   ├── 03-static-analysis-clang-tidy-cppcheck.md      # Automated static analysis rules and CI integration
│   ├── 04-clang-format.md                             # Style configuration and pre-commit hooks
│   ├── 05-gcov-coverage.md                            # Code coverage instrumentation and gcov/lcov reporting
│   ├── 06-git-workflow-for-c-projects.md              # .gitignore for C artifacts, submodules for libraries
│   └── 07-cross-compilation-intro.md                  # Target triplets, sysroots, toolchain files
│
├── 07-testing/                                        # NEW MODULE
│   ├── README.md
│   ├── 01-unit-testing-with-unity-or-cmocka.md        # Unity test framework integration and test runners
│   ├── 02-test-doubles-and-mocking-in-c.md            # Link-time mocking (-Wl,--wrap), function pointer mocking
│   ├── 03-fuzzing-with-libfuzzer-afl.md               # Coverage-guided fuzzing, writing fuzz targets
│   └── 04-test-driven-c.md                            # TDD workflow in systems C code
│
├── 08-security-and-safety/                            # NEW MODULE
│   ├── README.md
│   ├── 01-buffer-overflows.md                         # Stack smashing, return address overwriting, shellcode anatomy
│   ├── 02-format-string-vulnerabilities.md            # %n arbitrary write vulnerability, leak reading
│   ├── 03-integer-overflow-and-wraparound.md          # Exploiting allocator calculations, sign truncation
│   ├── 04-use-after-free-and-double-free.md           # Heap exploitation mechanisms, dangling pointer reuse
│   ├── 05-secure-coding-cert-c.md                     # SEI CERT C coding standard top rules
│   ├── 06-misra-c-overview.md                         # Automotive/aerospace guidelines and compliance
│   └── 07-hardening-flags.md                          # Stack canaries, ASLR, DEP/NX, RELRO, FORTIFY_SOURCE
│
├── 09-performance/                                    # NEW MODULE
│   ├── README.md
│   ├── 01-cache-and-memory-hierarchy.md               # L1/L2/L3 caches, cache lines (64 bytes), false sharing
│   ├── 02-profiling-perf-gprof-valgrind-callgrind.md  # Linux perf profiling, CPU cycles, cache-miss tracking
│   ├── 03-compiler-optimizations.md                   # -O2 vs -O3 vs -Os, inlining, loop unrolling, vectorization
│   ├── 04-branch-prediction-and-data-layout.md        # Branch prediction penalty, __builtin_expect, struct of arrays (SoA)
│   ├── 05-simd-intrinsics-intro.md                    # AVX2/SSE intrinsics (<immintrin.h>), vector math
│   └── 06-benchmarking-correctly.md                   # Measuring wall-clock vs CPU time, escaping dead-code elimination
│
├── 10-design-and-patterns/                            # NEW MODULE
│   ├── README.md
│   ├── 01-modular-design-in-c.md                      # Single-responsibility modules, interface headers
│   ├── 02-design-patterns-in-c.md                     # Strategy, Observer, State Machine, Factory, Iterator in C
│   ├── 03-error-handling-patterns.md                  # Goto cleanup, status codes, error object patterns
│   └── 04-coding-style-guide.md                       # Naming conventions, indentation, error checking discipline
│
├── 11-projects/                                       # NEW MODULE (Each with README, src/, Makefile, tests/)
│   ├── README.md
│   ├── 01-tiny-shell/                                 # Interactive UNIX command shell (fork, exec, piping, redirection)
│   ├── 02-http-server/                                # Event-driven TCP HTTP/1.1 server handling GET/POST
│   ├── 03-key-value-store/                            # In-memory KV store with binary protocol and persistence
│   ├── 04-memory-allocator/                           # Custom malloc, free, realloc with heap boundary tags
│   ├── 05-json-parser/                                # Recursive-descent AST parser for JSON
│   ├── 06-text-editor-or-grep-clone/                  # Multi-threaded regex grep clone with memory-mapped files
│   ├── 07-thread-pool/                                # Work-stealing or FIFO worker thread pool with mutex & condvars
│   └── 08-lru-cache/                                  # O(1) Least Recently Used cache combining hash table + doubly linked list
│
├── 12-interview/                                      # NEW MODULE
│   ├── README.md
│   ├── 01-c-basics-qa.md                              # 50 essential C fundamentals questions with answers
│   ├── 02-pointers-and-memory-qa.md                   # Pointers, dynamic allocation, aliasing questions
│   ├── 03-output-based-questions.md                   # Tricky code snippets, precedence puzzles, promotion quirks
│   ├── 04-bit-manipulation-qa.md                      # Common bit twiddling interview challenges
│   ├── 05-data-structures-coding-problems.md          # Implementing lists, trees, and heaps under interview constraints
│   ├── 06-systems-qa.md                               # Processes, threads, IPC, and POSIX questions
│   └── 07-c-vs-cpp.md                                 # Incompatibilities, ABI, C-linkage (`extern "C"`), idioms
│
└── 13-cheatsheets/                                    # NEW MODULE
    ├── README.md
    ├── 01-syntax.md                                   # Core language syntax and keyword quick reference
    ├── 02-pointers-and-declarations.md                # Declarator cheat sheet, operator precedence chart
    ├── 03-standard-library-quick-ref.md               # Standard library header and function lookup
    ├── 04-printf-scanf-formats.md                     # Complete format specifier and modifier matrix
    ├── 05-gcc-gdb-make.md                             # Essential CLI flags, GDB commands, and Makefile macros
    ├── 06-posix-api.md                                # Common POSIX system call signatures and error codes
    └── 07-ub-checklist.md                             # The definitive Undefined Behavior safety checklist
```

---

## 3. Order of Work in Batches (~5 Notes per Batch)

To enable incremental review and verification, execution is partitioned into **24 discrete batches**. After each batch, all new/modified examples will be compiled, sanitizers executed where applicable, and a progress summary produced before pausing for user review.

- **Batch 1 (00-setup foundation)**:
  `00-setup/README.md`, `00-setup/01-toolchain.md`, `00-setup/02-editor-setup.md`, `00-setup/03-compilation-process.md`, `00-setup/04-first-program.md`.
- **Batch 2 (00-setup extension)**:
  `00-setup/05-c-standards-history.md`, `00-setup/06-debugging-with-gdb.md`, `00-setup/07-makefile-basics.md`, `00-setup/08-compiler-flags-and-warnings.md`, `00-setup/09-reading-assembly-godbolt.md`.
- **Batch 3 (01-fundamentals part 1)**:
  `01-fundamentals/README.md`, `01-fundamentals/01-program-structure.md`, `01-fundamentals/02-data-types.md`, `01-fundamentals/03-integer-representation.md`, `01-fundamentals/04-variables-constants.md`.
- **Batch 4 (01-fundamentals part 2)**:
  `01-fundamentals/05-operators.md`, `01-fundamentals/06-type-conversions.md`, `01-fundamentals/07-control-flow.md`, `01-fundamentals/08-functions.md`.
- **Batch 5 (01-fundamentals part 3 & completion)**:
  `01-fundamentals/09-input-output.md`, `01-fundamentals/10-undefined-unspecified-implementation-defined.md`, `01-fundamentals/11-sizeof-alignment-basics.md`, `01-fundamentals/12-const-correctness-basics.md`.
- **Batch 6 (02-core-concepts part 1)**:
  `02-core-concepts/README.md`, `02-core-concepts/01-arrays.md`, `02-core-concepts/02-strings.md`, `02-core-concepts/03-pointers.md`, `02-core-concepts/04-declarations-typedef.md`.
- **Batch 7 (02-core-concepts part 2)**:
  `02-core-concepts/05-type-qualifiers.md`, `02-core-concepts/06-structs-unions-enums.md`, `02-core-concepts/07-scope-storage-classes.md`, `02-core-concepts/08-memory-management.md`.
- **Batch 8 (02-core-concepts part 3)**:
  `02-core-concepts/09-preprocessor.md`, `02-core-concepts/10-command-line-arguments.md`, `02-core-concepts/11-file-io.md`, `02-core-concepts/12-pointer-arithmetic.md`.
- **Batch 9 (02-core-concepts completion)**:
  `02-core-concepts/13-multi-dimensional-arrays.md`, `02-core-concepts/14-dynamic-arrays-and-realloc.md`, `02-core-concepts/15-string-h-functions-and-safe-alternatives.md`, `02-core-concepts/16-bit-fields.md`, `02-core-concepts/17-enums-and-flags.md`.
- **Batch 10 (02-core-concepts memory & literals)**:
  `02-core-concepts/18-designated-initializers-and-compound-literals.md`, `02-core-concepts/19-struct-padding-and-alignment.md`, `02-core-concepts/20-memory-layout-stack-heap-data-bss.md`.
- **Batch 11 (03-intermediate part 1)**:
  `03-intermediate/README.md`, `03-intermediate/01-function-pointers.md`, `03-intermediate/02-multi-file-programs.md`, `03-intermediate/03-error-handling.md`, `03-intermediate/04-assertions-defensive-coding.md`.
- **Batch 12 (03-intermediate part 2 & Slot 10)**:
  `03-intermediate/05-bit-manipulation.md`, `03-intermediate/06-variadic-functions.md`, `03-intermediate/07-text-processing-parsing.md`, `03-intermediate/08-unicode-locale.md`, `03-intermediate/09-time-and-random.md`, `03-intermediate/10-undefined-behavior-and-sanitizers.md`.
- **Batch 13 (03-intermediate completion)**:
  `03-intermediate/11-standard-library.md`, `03-intermediate/12-generic-programming.md`, `03-intermediate/13-opaque-pointers-and-adts.md`, `03-intermediate/14-callbacks-and-function-pointer-tables.md`, `03-intermediate/15-setjmp-longjmp.md`, `03-intermediate/16-inline-restrict-volatile.md`, `03-intermediate/17-x-macros-and-advanced-preprocessor.md`.
- **Batch 14 (03 Data Structures part 1)**:
  `03-intermediate/data-structures/README.md`, `data-structures/01-linked-lists.md`, `data-structures/02-stacks-queues.md`, `data-structures/03-trees.md`, `data-structures/04-hash-tables.md`.
- **Batch 15 (03 Data Structures part 2)**:
  `data-structures/05-graphs.md`, `data-structures/06-dynamic-array.md`, `data-structures/07-queues-circular-buffer.md`, `data-structures/08-heaps-and-priority-queues.md`, `data-structures/09-binary-search-trees.md`.
- **Batch 16 (03 Data Structures completion)**:
  `data-structures/10-tries.md`, `data-structures/11-union-find.md`, `data-structures/12-sorting-algorithms.md`, `data-structures/13-searching-algorithms.md`, `data-structures/14-algorithm-complexity.md`.
- **Batch 17 (04-advanced)**:
  `04-advanced/README.md`, notes `01` through `08`.
- **Batch 18 (05-systems-programming part 1)**:
  `05-systems-programming/README.md`, notes `01` through `06`.
- **Batch 19 (05-systems-programming part 2)**:
  Notes `07` through `11`.
- **Batch 20 (06-build-and-tooling & 07-testing)**:
  `06-build-and-tooling/` (all 7 notes) + `07-testing/` (all 4 notes).
- **Batch 21 (08-security-and-safety & 09-performance)**:
  `08-security-and-safety/` (all 7 notes) + `09-performance/` (all 6 notes).
- **Batch 22 (10-design-and-patterns)**:
  `10-design-and-patterns/` (all 4 notes) + root `01-languages/c/README.md`.
- **Batch 23 (11-projects)**:
  Projects `01-tiny-shell` through `08-lru-cache` (code, tests, Makefiles, documentation).
- **Batch 24 (12-interview & 13-cheatsheets)**:
  `12-interview/` (7 notes) + `13-cheatsheets/` (7 notes).

---

## 4. Open Questions & Technical Alignment for User Approval

1. **Host Compiler Capabilities for Sanitizers**:
   - The Windows host has `MinGW.org GCC 6.3.0` (32-bit), which supports `-std=c11` but lacks `-std=c17` and ASan (`-fsanitize=address`).
   - *Proposal:* All code examples will be verified for strict compilation with `-std=c11 -Wall -Wextra -Wpedantic -Werror` locally. We will write every example to be 100% portable C11/C17/C23 and annotate how to run with ASan/UBSan on modern Linux/WSL2/macOS.
2. **Project Code Structure in `11-projects/`**:
   - Should each of the 8 projects be a full mini-repository (with its own `src/`, `include/`, `tests/`, `Makefile`, and `README.md`)?
   - *Recommendation:* Yes, creating clean, modular, self-contained directories under `11-projects/` provides maximum real-world learning value.
3. **Execution Approval Gate**:
   - In accordance with prompt instructions, **Phase 3 execution will only start once you reply "approved"**.
