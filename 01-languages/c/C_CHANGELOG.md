# C Section Changelog (C_CHANGELOG.md)

All modifications, error fixes, technical corrections, and structural extensions applied during the Phase 3 curriculum upgrade are documented here.

## Baseline & Standards
- Baseline standard: **ISO C11 / C17** (`-std=c11` / `-std=c17`).
- Modernization: **ISO C23** callout sections highlighting newly standardized features, attributes, and removed/deprecated behaviors.
- Compiler Verification: Verified locally with `gcc -std=c11 -Wall -Wextra -Wpedantic -Werror`.
- Code Examples: Isolated, self-contained, runnable C programs stored in `examples/` subfolders under each module, with real output captured and verified.

---

## Batch 1: Module 00 Setup (Notes 01–04 + Module README)
- **`00-setup/README.md`**: Created module index, learning outcomes, roadmap, and prerequisites.
- **`00-setup/01-toolchain.md`**:
  - Restructured into all 13 required sections.
  - Added ASCII toolchain pipeline diagram (preprocessor → compiler cc1 → assembler as → linker ld).
  - Preserved original package manager install recipes and added modern C23 compiler flags (`-std=c2x`, `-std=c23`).
  - Added LLVM/Clang ecosystem integration (`clangd`, `clang-tidy`, `clang-format`, `lld`).
  - Added MinGW-w64 vs MSYS2 vs WSL2 comparison on Windows.
  - Added `<details>` interview questions and easy/medium/hard practice exercises with verified solutions.
  - Linked to compilable example `examples/01-toolchain.c`.
- **`00-setup/02-editor-setup.md`**:
  - Restructured into all 13 required sections.
  - Added `.clangd` configuration recipes supporting C17/C23 and standard warning flags.
  - Added `compile_commands.json` compilation database generation via CMake and `bear`.
  - Added `.clang-format` configuration rules.
  - Added GDB/LLDB debug launch configs for VS Code (`launch.json`) and Neovim (`nvim-dap`).
  - Added `<details>` interview questions and hands-on editor diagnostics exercises.
  - Linked to compilable example `examples/02-editor-setup.c`.
- **`00-setup/03-compilation-process.md`**:
  - Restructured into all 13 required sections.
  - Added complete compilation transformation pipeline diagram.
  - Detailed the 4 phases: preprocessing (`.i`), compilation (`.s`), assembly (`.o`), and linking (executable).
  - Detailed ELF binary structure: `.text`, `.rodata`, `.data`, `.bss`, symbol tables, and relocation tables.
  - Added binutils inspection commands (`readelf -S`, `objdump -d -M intel`, `nm -C`).
  - Added `<details>` interview questions (static vs dynamic linking, symbol collision) and exercises with solutions.
  - Linked to compilable example `examples/03-compilation-process.c`.
- **`00-setup/04-first-program.md`**:
  - Fixed Block #3 syntax error where markdown prose was wrapped inside a C code fence.
  - Restructured into all 13 required sections.
  - Added C runtime startup sequence diagram (`_start` → `__libc_start_main` → `main` → `exit`).
  - Detailed standard `main` signatures: `int main(void)` and `int main(int argc, char *argv[])`.
  - Highlighted C23 change: empty parameter list `int main()` is now strictly equivalent to `(void)`.
  - Added `<details>` interview questions (program exit codes, `atexit()`) and exercises with solutions.
  - Linked to compilable example `examples/04-first-program.c`.

---

## Batch 2: Module 00 Setup Complete (Notes 05–11 + Updated Module README)
- **`00-setup/README.md`**: Updated with full 11-note roadmap, outcomes, and cross-module navigation to Module 01.
- **`00-setup/05-c-standards-history.md`**:
  - Rewritten from 94 lines to 453 lines, adhering strictly to the 13 required sections.
  - Added standards timeline from K&R, C89/C90, C99, C11, C17, to C23 (`ISO/IEC 9899:2024`).
  - Added preprocessor detection macros (`__STDC_VERSION__`), C11 `_Generic`, C23 native keywords (`nullptr`, `bool`, `constexpr`, `typeof`, `[[attributes]]`).
  - Added compilation example and real output verification from `examples/05-c-standards-history.c`.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/06-debugging-with-gdb.md`**:
  - Authored complete 433-line deep dive into GDB debugging.
  - Detailed internal mechanism of software breakpoints (`INT 3` opcode injection) vs hardware watchpoints (x86 `DR0`-`DR7` registers).
  - Detailed DWARF symbol formats, core dump generation and post-mortem backtracing (`bt full`).
  - Added compilation example and real output verification from `examples/06-debugging-with-gdb.c`.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/07-makefile-basics.md`**:
  - Authored complete 415-line guide to GNU Make in C environments.
  - Detailed dependency DAGs, timestamp comparison algorithm, subshell execution rules, and automatic variables (`$@`, `$<`, `$^`).
  - Detailed automated header dependency tracking using `-MMD -MP` and `-include $(DEPS)`.
  - Added compilation example and real output verification from `examples/07-makefile-basics.c`.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/08-compiler-flags-and-warnings.md`**:
  - Authored complete 402-line reference for GCC/Clang warning hygiene and compiler flags.
  - Detailed warning tiers (`-Wall`, `-Wextra`, `-Wpedantic`, `-Wconversion`, `-Wsign-conversion`, `-Wshadow`, `-Werror`).
  - Detailed optimizer transformations (`-O0` to `-O3`, `-Os`, `-Og`), strict aliasing analysis (TBAA), SSA form, and security hardening flags (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-fPIE -pie`, `-Wl,-z,relro,-z,now`).
  - Added compilation example and real output verification from `examples/08-compiler-flags-and-warnings.c`.
  - Added 7-row common pitfalls table, 10 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/09-reading-assembly-godbolt.md`**:
  - Authored complete 400-line engineer's guide to x86-64 assembly and Compiler Explorer.
  - Detailed x86-64 register hierarchy, System V AMD64 ABI vs Microsoft x64 ABI, stack frame anatomy, and the 128-byte Red Zone.
  - Detailed SIMD vector extensions (SSE, AVX, AVX-512), `lea` arithmetic decomposition, `cmov` branchless idioms, and Tail Call Optimization.
  - Added compilation example and real output verification from `examples/09-reading-assembly-godbolt.c`.
  - Added 7-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/10-c-project-layout.md`**:
  - Authored complete 468-line guide to scalable modular C repository architecture.
  - Detailed separation of public API headers (`include/<project>/`) from private implementation headers (`src/`).
  - Detailed the Opaque Pointer (PIMPL) encapsulation pattern for ABI stability, out-of-tree object builds (`build/obj/`), static archives (`.a`), and shared objects (`.so`) with symbol visibility hardening (`-fvisibility=hidden`).
  - Added compilation example and real output verification from `examples/10-c-project-layout.c`.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`00-setup/11-headers-and-include-guards-intro.md`**:
  - Authored complete 448-line guide to C header hygiene and idempotency.
  - Detailed preprocessor text substitution, the One Definition Rule (ODR), and include guard mechanics.
  - Compared `#ifndef` guards vs `#pragma once` (portability, symlinks, network drives) and the Include Guard Optimization (IGO) in GCC/Clang.
  - Detailed forward declarations to prevent circular dependencies, and `extern "C"` wrapper guards for C++ ABI compatibility.
  - Added compilation example and real output verification from `examples/11-headers-and-include-guards-intro.c`.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.

---

## Batch 3: Module 01 Fundamentals Part 1 (Notes 01–04)
- **`01-fundamentals/01-program-structure.md`**:
  - Rewritten from 79 lines to 402 lines, adhering strictly to the 13 required sections.
  - Fixed syntax error from Block #2 in original note (unclosed `main` fragment).
  - Detailed translation unit generation, linkage types (`static` vs `extern`), object file symbol tables (`.symtab`), ELF bindings (`STB_LOCAL` vs `STB_GLOBAL`), and hosted vs freestanding execution environments.
  - Added runnable example `examples/01-program-structure.c` and verified real output.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/02-data-types.md`**:
  - Rewritten from 115 lines to 414 lines, adhering strictly to the 13 required sections.
  - Added standards minimum widths, LP64 (Linux/macOS) vs LLP64 (Windows) data models, IEEE 754 floating-point internal representations.
  - Added `<stdint.h>` exact-width types, `<inttypes.h>` portable format specifiers (`PRIu32`, `PRId64`), and C23 native keywords (`nullptr_t`, `bool`, `0b...` literals, `1'000'000` digit separators).
  - Added runnable example `examples/02-data-types.c` and verified real output demonstrating 32-bit vs 64-bit platform width divergence.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/03-integer-representation.md`**:
  - Rewritten from 131 lines to 402 lines, adhering strictly to the 13 required sections.
  - Detailed two's complement arithmetic (mandated as sole representation in ISO C23), signed overflow undefined behavior vs unsigned modular wrap-around ($2^N$).
  - Detailed endianness (Little-Endian vs Big-Endian/Network order), byte-swapping, sign vs zero extension (`movsx` vs `movzx`), C23 bit-precise integers (`_BitInt(N)`), and `<stdckdint.h>` checked arithmetic.
  - Added runnable example `examples/03-integer-representation.c` and verified real memory bytes and endianness output.
  - Added 6-row common pitfalls table, 10 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/04-variables-constants.md`**:
  - Rewritten from 124 lines to 410 lines, adhering strictly to the 13 required sections.
  - Fixed Block #2 compilation error from original note (uninitialized variable read under `-Werror`).
  - Detailed the 4 storage durations (automatic, static, allocated, thread), the 4 scopes (block, file, function, function-prototype), and ELF memory segment placement (`.text`, `.rodata`, `.data`, `.bss`, heap, stack).
  - Clarified why `const` variables are not compile-time constants in C11/C17 (creating VLAs if used in array bounds), demonstrated enum constant idiom, and detailed C23 `constexpr` objects.
  - Added runnable example `examples/04-variables-constants.c` and verified real output.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.

---

## Batch 4: Module 01 Fundamentals Part 2 (Notes 05–08)
- **`01-fundamentals/05-operators.md`**:
  - Rewritten from 126 lines to 406 lines, adhering strictly to the 13 required sections.
  - Detailed the complete 15-tier operator precedence and associativity table.
  - Clarified sequence points vs modern ISO C11/C17/C23 "sequenced-before" partial-order relations, detailing why expressions like `i = i++`, `a[i] = i++`, and `f(v++, v++)` invoke undefined behavior.
  - Detailed short-circuit evaluation guarantees in `&&` and `||`, safe bitwise manipulation macros, and the sequenced comma operator.
  - Added C23 `typeof` and `typeof_unqual` operators and their application to type-safe generic macro design.
  - Added runnable example `examples/05-operators.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/06-type-conversions.md`**:
  - Rewritten from 130 lines to 400 lines, adhering strictly to the 13 required sections.
  - Detailed integer promotion rank rules and the 7-step Usual Arithmetic Conversions algorithm.
  - Explained why `(unsigned short)a + (unsigned short)b` produces a signed `int`.
  - Detailed the sign-conversion trap (`-1 < 1U` evaluating to `false`), float-to-integer truncation toward zero, and the strict aliasing rule with `memcpy` bit reinterpretation.
  - Added C23 `<stdckdint.h>` checked conversion functions (`ckd_add`, `ckd_mul`).
  - Added runnable example `examples/06-type-conversions.c` and verified real output.
  - Added 7-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/07-control-flow.md`**:
  - Rewritten from 147 lines to 452 lines, adhering strictly to the 13 required sections.
  - Detailed branch prediction architecture (BTB), pipeline flush penalties, and compiler branch hints (`likely`/`unlikely`).
  - Detailed `switch` statement compilation strategies ($O(1)$ jump tables vs binary search trees), and the C23 `[[fallthrough]]` attribute.
  - Detailed the canonical `goto cleanup` single-exit resource teardown pattern used in the Linux kernel.
  - Dissected Tom Duff's Device for loop unrolling and its syntactic validity under the ISO C grammar.
  - Added runnable example `examples/07-control-flow.c` and verified real output.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/08-functions.md`**:
  - Rewritten from 155 lines to 419 lines, adhering strictly to the 13 required sections.
  - Detailed strict pass-by-value semantics, simulating pass-by-reference via pointers, stack frame activation records, prologue/epilogue sequences, and caller-saved vs callee-saved registers.
  - Detailed ABI calling conventions (System V AMD64 vs Microsoft x64 32-byte shadow space).
  - Detailed function pointers, clean `typedef` callback idioms, Tail Call Optimization (TCO), and the C23 standard change making `int f()` strictly identical to `int f(void)`.
  - Added runnable example `examples/08-functions.c` and verified real output.
  - Added 6-row common pitfalls table, 8 interview questions, and 3 practice exercises with full solutions.

---

## Batch 5: Module 01 Fundamentals Part 3 (Notes 09–15 + Module README)
- **`01-fundamentals/README.md`**:
  - Authored complete 69-line module index and roadmap covering all 15 fundamental notes with learning outcomes and prerequisites.
- **`01-fundamentals/09-input-output.md`**:
  - Rewritten from 185 lines to 400 lines, adhering strictly to the 13 required sections.
  - Detailed standard stream handles (`stdin`, `stdout`, `stderr`), buffering modes (`_IOFBF`, `_IOLBF`, `_IONBF`), `setvbuf()`, and `fflush()`.
  - Detailed format string vulnerabilities (CWE-134), `snprintf` truncation detection and dynamic allocation patterns, safe input parsing via `fgets` + `sscanf`, and newline stripping idioms.
  - Evaluated C11 Annex K bounds-checking interfaces (`printf_s`, `scanf_s`, `gets_s`).
  - Added runnable example `examples/09-input-output.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/10-undefined-unspecified-implementation-defined.md`**:
  - Authored complete 412-line guide to non-deterministic behaviors in ISO C.
  - Formalized the triad: Undefined Behavior (UB), Unspecified Behavior, and Implementation-Defined Behavior with ISO definitions.
  - Detailed how modern optimizing compilers exploit UB via data-flow analysis to delete security checks and dead code.
  - Detailed time-travel optimizations, `-fwrapv`, `-fno-delete-null-pointer-checks`, and runtime detection via UBSan (`-fsanitize=undefined`).
  - Added runnable example `examples/10-undefined-unspecified-implementation-defined.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/11-sizeof-and-alignment-basics.md`**:
  - Authored complete 402-line guide to memory alignment and `sizeof`.
  - Detailed `sizeof` compile-time evaluation (and the VLA runtime evaluation exception), hardware bus alignment requirements, and `SIGBUS` faults.
  - Detailed struct member padding calculations, trailing struct padding, member reordering optimization (saving 33% memory footprint), and C11 `<stdalign.h>` (`alignof`, `alignas`, `max_align_t`).
  - Added runnable example `examples/11-sizeof-and-alignment-basics.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/12-const-correctness-basics.md`**:
  - Authored complete 405-line guide to `const` qualification and pointer semantics.
  - Detailed pointer-to-const vs const-pointer vs const-pointer-to-const, the clockwise / right-to-left reading rule, and transitive struct constness.
  - Detailed ELF `.rodata` segment mapping with hardware write-protection page faults (`SIGSEGV`) when casting away constness.
  - Explained why multi-level pointer qualification (`char **` to `const char **`) requires explicit casts in C.
  - Added runnable example `examples/12-const-correctness-basics.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/13-integer-promotions-and-usual-arithmetic-conversions.md`**:
  - Authored complete 403-line definitive reference on C conversion ranks and promotion rules.
  - Detailed the conversion rank hierarchy, integer promotion to signed `int`, and the step-by-step Usual Arithmetic Conversions state machine.
  - Detailed the signed-to-unsigned comparison vulnerability, bitwise shift undefined behavior on promoted narrow types, and C23 `_BitInt(N)` promotion semantics.
  - Added runnable example `examples/13-integer-promotions-and-usual-arithmetic-conversions.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`01-fundamentals/14-sequence-points-and-evaluation-order.md`**:
  - Authored complete 409-line guide to execution ordering in C.
  - Formalized ISO C11/C17/C23 sequenced-before, unsequenced, and indeterminately sequenced relations.
  - Cataloged all standard sequence points (`;`, `&&`, `||`, `?:`, `,`, function entry, full expressions).
  - Detailed why `i = i++`, `a[i] = i++`, and `f(v++, v++)` invoke undefined behavior, and compiler instruction reordering boundaries.
  - Added runnable example `examples/14-sequence-points-and-evaluation-order.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 4 practice exercises with full solutions.
- **`01-fundamentals/15-lvalues-rvalues-and-expressions.md`**:
  - Authored complete 403-line deep dive into C value categories.
  - Formalized lvalues (locator values designating memory storage) vs rvalues (transient expression values).
  - Detailed modifiable vs non-modifiable lvalues (const-qualified and array types), and lvalue-to-rvalue assembly conversions.
  - Dissected array decay into rvalue pointers and the three strict exceptions (`sizeof`, `&`, string literal initializer).
  - Detailed C99 compound literals as mutable lvalues with block-scope automatic or file-scope static storage duration.
  - Added runnable example `examples/15-lvalues-rvalues-and-expressions.c` and verified real output.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.

---

## Batch 6: Module 02 Core Concepts (Notes 01–04)
- **`02-core-concepts/01-arrays.md`**:
  - Completely restructured into all 13 required sections (401 lines).
  - Detailed contiguous memory layout, base address offset calculations, and duality of `a[i]` and `*(a + i)`.
  - Clarified array-to-pointer decay mechanisms and the three ISO exceptions (`sizeof`, address-of `&`, string literal initialization).
  - Analyzed Variable Length Arrays (VLAs): stack overflow vulnerabilities, C11 optional status (`__STDC_NO_VLA__`), and safe dynamic alternatives.
  - Added runnable example `examples/01-arrays.c` with verified real output in `real_outputs_batch6.json`.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`02-core-concepts/02-strings.md`**:
  - Completely restructured into all 13 required sections (406 lines).
  - Detailed Null-Terminated Byte Strings (NTBS), memory layouts, string literal placement in read-only `.rodata`, and `SIGSEGV` on mutation.
  - Analyzed standard string functions: `strlen`, `strcpy`, `strncpy`, `strlcpy`, and bounds-safe `snprintf`.
  - Examined UTF-8 encoding considerations, multi-byte character traversal, and ASCII vs code point byte counts.
  - Added runnable example `examples/02-strings.c` with verified real output in `real_outputs_batch6.json`.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`02-core-concepts/03-pointers.md`**:
  - Completely restructured into all 13 required sections (420 lines).
  - Detailed virtual address spaces, byte addressing, pointer types, alignment requirements, and dereference mechanics.
  - Examined multi-level indirection (pointer-to-pointer `T**`) for dynamic reallocation and out-parameter mutations.
  - Covered ISO C pointer rules: pointer subtraction (`ptrdiff_t`), object boundary rules, strict aliasing, pointer provenance, and C23 `nullptr` / `nullptr_t`.
  - Added runnable example `examples/03-pointers.c` with verified real output in `real_outputs_batch6.json`.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.
- **`02-core-concepts/04-declarations-typedef.md`**:
  - Completely restructured into all 13 required sections (415 lines).
  - Detailed C declaration grammar, declarator priority, and the Clockwise/Spiral reading algorithm for complex types.
  - Dissected complex function pointer types, arrays of function pointers, and `signal()` handler signatures.
  - Contrasted `typedef` type aliasing with `#define` textual substitution, opaque handle patterns, and C23 `typeof` / `typeof_unqual`.
  - Added runnable example `examples/04-declarations-typedef.c` with verified real output in `real_outputs_batch6.json`.
  - Added 6-row common pitfalls table, 12 interview questions, and 3 practice exercises with full solutions.

---

## Batch 7: Module 02 Core Concepts (Notes 05–08)
- **`02-core-concepts/05-type-qualifiers.md`**:
  - Completely restructured into all 13 required sections (473 lines).
  - Detailed the four ISO qualifiers (`const`, `volatile`, `restrict`, `_Atomic`) and modern C23 additions (`constexpr`, `typeof_unqual`).
  - Explained virtual memory mapping (`.rodata` write faults vs mutable stack constness), MMIO register polling, and non-aliasing SIMD optimizations.
  - Added runnable example `examples/05-type-qualifiers.c` with verified real output in `real_outputs_batch7.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/06-structs-unions-enums.md`**:
  - Completely restructured into all 13 required sections (507 lines).
  - Detailed member alignment padding, trailing padding calculations, `offsetof` introspection, and designated initializers.
  - Analyzed discriminated (tagged) unions, ISO C type-punning semantics vs C++ UB, flexible array members (FAMs), and C23 fixed-underlying-type enums.
  - Added runnable example `examples/06-structs-unions-enums.c` with verified real output in `real_outputs_batch7.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/07-scope-storage-classes.md`**:
  - Completely restructured into all 13 required sections (407 lines).
  - Dissected the 4 lexical scopes (block, file, prototype, function), the 4 storage durations (automatic, static, allocated, thread), and linkages (external, internal, none).
  - Detailed static local state persistence in `.data`/`.bss`, `static inline` header functions, tentative definitions, and C23 `auto` type inference.
  - Added runnable example `examples/07-scope-storage-classes.c` with verified real output in `real_outputs_batch7.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/08-memory-management.md`**:
  - Completely restructured into all 13 required sections (516 lines).
  - Detailed system heap architecture (glibc chunk headers, alignment padding, `brk`/`mmap`), `malloc`, `calloc`, `realloc`, and `free`.
  - Emphasized defensive patterns: temporary pointer with `realloc`, pointer nullification, integer multiplication wrap-around checks, C11 `aligned_alloc`, and C23 sized deallocations.
  - Added runnable example `examples/08-memory-management.c` with verified real output in `real_outputs_batch7.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.


