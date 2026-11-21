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

---

## Batch 8: Module 02 Core Concepts (Notes 09–12)
- **`02-core-concepts/09-preprocessor.md`**:
  - Completely restructured into all 13 required sections (424 lines).
  - Detailed preprocessor translation phases 1–4, object-like vs function-like macros, stringification `#`, token pasting `##`, and variadic macros (`__VA_ARGS__`, C23 `__VA_OPT__`).
  - Formalized the `do { ... } while(0)` safe multi-statement macro idiom, standard header include guards vs `#pragma once`, and ISO C23 `#embed` and `__has_include`.
  - Added runnable example `examples/09-preprocessor.c` with verified real output in `real_outputs_batch8.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/10-command-line-arguments.md`**:
  - Completely restructured into all 13 required sections (411 lines).
  - Detailed `main(int argc, char *argv[])` kernel process stack initialization, pointer vector structure, and guaranteed `argv[argc] == NULL` sentinel.
  - Analyzed shell tokenization (quoting, globbing), why `atoi` must be strictly avoided, and safe integer parsing with `strtol` checking `errno` and `endptr`.
  - Contrasted POSIX `getopt`/`getopt_long` with Windows command-line processing, process argument inspection via `/proc`, and argument injection security vulnerabilities.
  - Added runnable example `examples/10-command-line-arguments.c` with verified real output in `real_outputs_batch8.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/11-file-io.md`**:
  - Completely restructured into all 13 required sections (476 lines).
  - Detailed the `FILE *` standard I/O stream abstraction, user-space buffering modes (`_IOFBF`, `_IOLBF`, `_IONBF`) via `setvbuf`, and kernel system call boundaries.
  - Examined open modes (`"r"`, `"w"`, `"a"`, `"r+"`, `"w+"`, `"a+"`, and C11 exclusive `"wx"`), Windows CRLF text vs binary translation mechanics, and formatted line reading via `fgets`.
  - Formalized binary serialization with `fwrite`/`fread`, file seeking (`fseek`/`ftell` vs 64-bit `fgetpos`/`fsetpos`), and why `while(!feof(f))` is a fatal anti-pattern.
  - Added runnable example `examples/11-file-io.c` with verified real output in `real_outputs_batch8.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/12-pointer-arithmetic.md`**:
  - Authored complete 413-line comprehensive reference on C pointer arithmetic mechanics.
  - Detailed address scaling by `sizeof(*p)`, pointer difference yielding signed `ptrdiff_t` in `<stddef.h>`, and the `a[i] == *(a + i)` duality.
  - Formalized ISO C object boundary rules, the one-past-the-end sentinel rule (`arr + N`), interior pointers, negative indexing (`p[-1]`), and 2D row-stride traversal.
  - Added runnable example `examples/12-pointer-arithmetic.c` with verified real output in `real_outputs_batch8.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.

---

## Batch 9: Module 02 Core Concepts (Notes 13–17)
- **`02-core-concepts/13-multi-dimensional-arrays.md`**:
  - Authored complete 431-line deep dive into multi-dimensional arrays.
  - Detailed row-major contiguous memory layout, flat stride index formula `r * COLS + c`, pointer decay to array pointers `int (*)[COLS]` vs illegal `int **`, and VLA function parameters.
  - Analyzed CPU cache impacts: row-major spatial locality vs column-major cache thrashing penalties.
  - Added runnable example `examples/13-multi-dimensional-arrays.c` with verified real output in `real_outputs_batch9.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/14-dynamic-arrays-and-realloc.md`**:
  - Authored complete 447-line comprehensive engineering guide to dynamic arrays (vectors).
  - Formalized size vs capacity, mathematical proof of geometric capacity growth yielding amortized $O(1)$ push time, and the 1.5x vs 2.0x growth factor memory reuse debate.
  - Detailed the defensive temporary pointer `realloc` idiom, pointer invalidation risks, shrink-to-fit considerations, and arithmetic overflow protection during capacity scaling.
  - Added runnable example `examples/14-dynamic-arrays-and-realloc.c` with verified real output in `real_outputs_batch9.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/15-string-h-functions-and-safe-alternatives.md`**:
  - Authored complete 435-line reference on `<string.h>` operations and secure boundaries.
  - Dissected classic pitfalls: `strcpy` buffer overflows, `strncpy` missing null terminators and zero-padding penalties, and quadratic $O(N^2)$ `strlen` inside loop conditions.
  - Detailed bounds-safe alternatives: `snprintf` truncation detection (`ret >= size`), BSD `strlcpy`/`strlcat` reference implementations, constant-time `safe_memcmp_const_time`, and reentrant `strtok_r` vs non-reentrant `strtok`.
  - Added runnable example `examples/15-string-h-functions-and-safe-alternatives.c` with verified real output in `real_outputs_batch9.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/16-bit-fields.md`**:
  - Authored complete 432-line technical guide to C bit-fields.
  - Detailed bit-field syntax, memory packing, signed 1-bit `-1` overflow traps, the address-of `&` compilation restriction, and zero-width boundary aligners (`: 0`).
  - Analyzed endianness and ABI non-portability: why bit-fields must never define network packets or disk formats, hardware MMIO register unions, and portable bitwise shift-and-mask alternatives.
  - Added runnable example `examples/16-bit-fields.c` with verified real output in `real_outputs_batch9.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/17-enums-and-flags.md`**:
  - Authored complete 415-line guide to enumerations and bitmask flags in C.
  - Contrasted discrete state modeling with power-of-two bitmask combinations (`1 << N`), bitwise set (`|=`), clear (`&= ~`), toggle (`^=`), and query (`(& MASK) == MASK`) idioms.
  - Examined ISO C enum type-safety realities (assignment of arbitrary integers), `-Wswitch-enum` exhaustiveness checking, operator precedence pitfalls, and ISO C23 fixed underlying type syntax (`enum Color : uint8_t`).
  - Added runnable example `examples/17-enums-and-flags.c` with verified real output in `real_outputs_batch9.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.

---

## Batch 10: Module 02 Core Concepts Complete (Notes 18–25 + Module README)
- **`02-core-concepts/README.md`**: Created comprehensive module roadmap, index table of all 25 notes, learning outcomes, and prerequisites.
- **`02-core-concepts/18-designated-initializers-and-compound-literals.md`**:
  - Authored complete 414-line reference on C99/C11/C23 modernization idioms.
  - Detailed designated initialization for structs/unions, sparse array indexing, compound literals as modifiable lvalues with automatic/static storage duration, and ISO C23 empty braces `{}`.
  - Added runnable example `examples/18-designated-initializers-and-compound-literals.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/19-struct-padding-and-alignment.md`**:
  - Authored complete 420-line guide to hardware memory alignment and struct packing.
  - Detailed natural alignment boundaries, internal member padding, trailing padding calculations, member reordering optimization (saving 33% memory footprint), `offsetof` mechanics, C11 `<stdalign.h>` / C23 `alignas`, and packed attribute hazards.
  - Added runnable example `examples/19-struct-padding-and-alignment.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/20-memory-layout-stack-heap-data-bss.md`**:
  - Authored complete 428-line architectural breakdown of process virtual address spaces.
  - Detailed `.text` (RX), `.rodata` (R), `.data` (RW initialized), `.bss` (RW zeroed, 0 bytes on disk), Heap (upward growth via `brk`/`mmap`), and Stack (downward growth via RSP).
  - Added live Linux virtual page table inspection via `/proc/self/maps`, stack canaries, ASLR, and memory tool commands (`size`, `readelf`, `nm`).
  - Added runnable example `examples/20-memory-layout-stack-heap-data-bss.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/21-flexible-array-members.md`**:
  - Authored complete 407-line guide to variable-length single-allocation structures.
  - Detailed ISO C99 flexible array member syntax (`type member[]`), single `malloc()` allocation for header + payload, cache prefetch benefits, and why FAM structs cannot be assigned by value or stored in flat arrays.
  - Contrasted with historic C89 struct hack (`[1]`) and GCC zero-length arrays (`[0]`).
  - Added runnable example `examples/21-flexible-array-members.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/22-restrict-and-aliasing-intro.md`**:
  - Authored complete 403-line guide to pointer aliasing and optimization boundaries.
  - Detailed the Strict Aliasing Rule (§6.5), Type-Based Alias Analysis (TBAA), the character pointer aliasing exception (`char *`), and the `restrict` non-aliasing contract.
  - Analyzed SIMD AVX2 auto-vectorization, why `memcpy` forbids overlap while `memmove` permits it, and standard-compliant type-punning via union or `memcpy`.
  - Added runnable example `examples/22-restrict-and-aliasing-intro.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 10 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/23-pointers-to-pointers-and-arrays-of-pointers.md`**:
  - Authored complete 416-line guide to multi-level pointer indirection.
  - Detailed pointer-to-pointer (`T**`) out parameters for dynamic memory allocation and linked list head mutation.
  - Contrasted ragged arrays of pointers (`char *argv[]`) with contiguous 2D matrices, indirect pointer sorting for large struct records, and universal pointer-nullifying free routines.
  - Added runnable example `examples/23-pointers-to-pointers-and-arrays-of-pointers.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/24-complex-declarations-reading-guide.md`**:
  - Authored complete 425-line guide to deciphering C declaration syntax.
  - Detailed declarator operator precedence (`()` and `[]` over `*`), the step-by-step Clockwise/Spiral reading algorithm, signal handler prototypes, 2D state machine function pointer transition matrices, and C23 `typeof`.
  - Added runnable example `examples/24-complex-declarations-reading-guide.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`02-core-concepts/25-linked-structures-with-pointers-intro.md`**:
  - Authored complete 416-line introduction to self-referential structures.
  - Formalized why structures cannot contain instances of themselves by value but can contain pointers to themselves, heap node linking, iterative list traversal, and safe leak-free deallocation.
  - Added Floyd's Cycle-Finding Algorithm (Tortoise and Hare), sorted list insertion via double pointers, and cache penalty comparisons between linked lists and arrays.
  - Added runnable example `examples/25-linked-structures-with-pointers-intro.c` with verified real output in `real_outputs_batch10.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.

---

## Batch 11: Module 03 Intermediate (Notes 01–04 + Module README)
- **`03-intermediate/README.md`**: Created module roadmap, comprehensive table of all 24 intermediate notes and 19 data structure notes, learning outcomes, and prerequisites.
- **`03-intermediate/01-function-pointers.md`**:
  - Completely restructured into all 13 required sections (444 lines).
  - Detailed function pointer declaration syntax, automatic address decay, invocation mechanics, and the universal `void *user_data` callback pattern.
  - Analyzed standard `qsort` and `bsearch` comparators (highlighting the `a - b` overflow vulnerability and the safe `(a > b) - (a < b)` idiom), dynamic dispatch tables, and object-oriented polymorphism/vtable emulation in C.
  - Added runnable example `examples/01-function-pointers.c` with verified real output in `real_outputs_batch11.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/02-multi-file-programs.md`**:
  - Completely restructured into all 13 required sections (473 lines).
  - Detailed translation units, header interfaces vs source implementations, standard include guards vs `#pragma once`, circular dependency breaking via forward declarations (`typedef struct B B;`), and `extern` variables.
  - Analyzed internal linkage (`static`) for translation-unit encapsulation, `static inline` header functions, and incremental compilation pipelines.
  - Added fully working, compilable companion code in `examples/02-multi-file/` (`mathutil.h`, `mathutil.c`, `main.c`) with verified real output in `real_outputs_batch11.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/03-error-handling.md`**:
  - Completely restructured into all 13 required sections (461 lines).
  - Formalized the 4 C error reporting strategies (sentinels, status codes + out parameters, thread-local `errno`, and error enums).
  - Detailed thread-local `errno` rules, `strerror_r` thread safety, cascading cleanup rollback architectures, and the canonical `goto cleanup;` resource management idiom.
  - Added runnable example `examples/03-error-handling.c` with verified real output in `real_outputs_batch11.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/04-assertions-defensive-coding.md`**:
  - Completely restructured into all 13 required sections (407 lines).
  - Formalized the boundary between programmer bugs (invariant violations caught by assertions) and expected runtime errors (handled by status codes).
  - Detailed runtime `assert()`, release build disabling via `-DNDEBUG`, the strict prohibition against side-effects in assertions, and compile-time static assertions (`_Static_assert` / C23 `static_assert`).
  - Added runnable example `examples/04-assertions-defensive-coding.c` with verified real output in `real_outputs_batch11.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.

---

## Batch 12: Module 03 Intermediate (Notes 05–10)
- **`03-intermediate/05-bit-manipulation.md`**:
  - Completely rewritten into all 13 required sections (459 lines).
  - Detailed the six bitwise operators, unsigned vs signed rules, shift UB when count $\ge$ width, logical vs arithmetic right shifts, and the 4 fundamental mask idioms (set, clear, toggle, test).
  - Detailed bit-twiddling hacks: power-of-two check `x & (x - 1)`, isolating lowest set bit `x & (-x)`, and Brian Kernighan's population count.
  - Added C23 `<stdbit.h>` catalog (`stdc_count_ones`, `stdc_leading_zeros`, `stdc_has_single_bit`).
  - Verified runnable code in `examples/05-bit-manipulation.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/06-variadic-functions.md`**:
  - Completely rewritten into all 13 required sections (500 lines).
  - Detailed `<stdarg.h>` primitives (`va_list`, `va_start`, `va_arg`, `va_end`, `va_copy`).
  - Detailed default argument promotions (`char`/`short` -> `int`, `float` -> `double`) and ABI calling conventions (System V AMD64 register save area vs 32-bit stack).
  - Added custom logging framework and safe `vsnprintf` dynamic string duplicator with `va_copy`.
  - Covered C23 parameterless ellipsis (`void f(...)`) and GCC `__attribute__((format(printf, ...)))`.
  - Verified runnable code in `examples/06-variadic-functions.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/07-text-processing-parsing.md`**:
  - Completely rewritten into all 13 required sections (559 lines).
  - Analyzed string scanning (`strchr`, `strstr`), span functions (`strspn`, `strcspn`), and non-destructive pointer walking.
  - Explaining why `strtok` is inherently thread-unsafe and comparing with `strtok_r` / `strtok_s`.
  - Detailed strict numeric conversion using `strtol`/`strtod` with `errno == ERANGE` and `endptr` validation, explaining why `atoi`/`atof` must be banned.
  - Implemented non-destructive CSV tokenizer and full recursive-descent math parser.
  - Verified runnable code in `examples/07-text-processing-parsing.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/08-unicode-locale.md`**:
  - Completely rewritten into all 13 required sections (446 lines).
  - Contrasted code points, code units, grapheme clusters, string byte length vs terminal display width.
  - Detailed UTF-8 variable-length byte layout (1 to 4 bytes), self-synchronization, and defensive decoding.
  - Addressed critical security risks: rejecting overlong UTF-8 encodings and UTF-16 surrogate halves (`U+D800`–`U+DFFF`).
  - Analyzed `<locale.h>`, the hazards of `setlocale(LC_ALL, "")` on decimal numbers in serialization, and C11 `<uchar.h>` (`char16_t`, `char32_t`).
  - Verified runnable code in `examples/08-unicode-locale.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/09-time-and-random.md`**:
  - Completely rewritten into all 13 required sections (458 lines).
  - Contrasted calendar wall-clock time (`time_t`, `struct tm`, `strftime`) with monotonic time for profiling.
  - Detailed C11 high-precision `timespec_get(TIME_UTC)` and CPU profiling with `clock()`.
  - Addressed the Year 2038 problem (Y2038) with 32-bit `time_t` overflow.
  - Deconstructed flaws of `rand()` and implemented high-performance Xoroshiro64* PRNG and rejection sampling to eliminate modulo bias.
  - Verified runnable code in `examples/09-time-and-random.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/10-undefined-behavior-and-sanitizers.md`**:
  - **Authored new comprehensive guide filling the Missing Slot 10 gap!** (435 lines).
  - Formalized the 4 ISO C behavior classes: Defined, Implementation-Defined, Unspecified, and Undefined Behavior.
  - Cataloged major UB categories: signed overflow, out-of-bounds, use-after-free, double-free, uninitialized reads, strict aliasing violations, bad shifts.
  - Analyzed how modern optimizing compilers exploit UB axioms (dead code removal, "time-travel" null check elimination).
  - Detailed the modern sanitizer suite: AddressSanitizer (ASan shadow memory architecture), UBSan, TSan, and Valgrind Memcheck.
  - Introduced C23 checked arithmetic (`<stdckdint.h>`).
  - Verified runnable code in `examples/10-undefined-behavior-and-sanitizers.c` with real output in `real_outputs_batch12.json`.
  - Added 6-row common pitfalls table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.

---

## Batch 13: Module 03 Intermediate (Notes 11–23)
- **`03-intermediate/11-standard-library.md`**:
  - Rewritten into all 13 required sections (404 lines).
  - Detailed hosted vs freestanding environments, standard header taxonomy, C runtime startup (`_start`), and CRT layers.
  - Demonstrated standard sorting/searching (`qsort`/`bsearch`), `<math.h>`, and exact-width integer formatting.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/12-generic-programming.md`**:
  - Authored new comprehensive guide (404 lines).
  - Detailed C11 `_Generic` selection keyword, compile-time AST resolution, type matching rules, and type-generic macros.
  - Contrasted `_Generic` with `void *` type erasure and token-pasting macro templates.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/13-opaque-pointers-and-adts.md`**:
  - Authored new comprehensive guide (542 lines).
  - Formalized incomplete types vs complete types, header/implementation compilation boundaries, and ABI stability.
  - Implemented dynamic stack ADT with encapsulated lifecycle management (`create`, `push`, `pop`, `destroy`).
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/14-callbacks-and-function-pointer-tables.md`**:
  - Authored new comprehensive guide (505 lines).
  - Formalized the context carrier pattern (`void *user_data`), table-driven command dispatch, and finite state machines.
  - Detailed CPU indirect branch prediction (Branch Target Buffer) and placing tables in `.rodata` for security.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/15-setjmp-longjmp.md`**:
  - Authored new comprehensive guide (426 lines).
  - Detailed non-local stack unwinding, CPU register preservation in `jmp_buf`, and the mandatory `volatile` variable rule.
  - Constructed structured exception handling (`TRY`, `CATCH`, `THROW`) and analyzed POSIX `sigsetjmp`/`siglongjmp`.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/16-inline-restrict-volatile.md`**:
  - Authored new comprehensive guide (404 lines).
  - Analyzed `inline` function semantics (`static inline` in headers), `restrict` pointer aliasing contract, and `volatile` hardware semantics.
  - Demonstrated SIMD auto-vectorization enablement via `restrict` and deconstructed the myth of `volatile` for multithreading.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/17-x-macros-and-advanced-preprocessor.md`**:
  - Authored new comprehensive guide (447 lines).
  - Detailed X-Macros for single-source-of-truth code generation, multi-pass expansions, stringification (`#`), and token pasting (`##`).
  - Demonstrated synchronizing enums, hex tables, string lookup arrays, and switch statements.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/18-strict-aliasing-and-type-punning.md`**:
  - Authored new comprehensive guide (403 lines).
  - Analyzed Type-Based Alias Analysis (TBAA) rules and demonstrated why casting `*(int *)&float` causes undefined behavior.
  - Demonstrated safe, standard-compliant type punning using `memcpy` and `union` reinterpretation.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/19-integer-overflow-and-checked-arithmetic.md`**:
  - Authored new comprehensive guide (403 lines).
  - Contrasted defined unsigned modulo wrapping with undefined signed integer overflow.
  - Detailed two's complement asymmetry (`-INT_MIN`, `INT_MIN / -1` traps), defensive pre-condition boundary checks, and C23 `<stdckdint.h>`.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/20-floating-point-pitfalls.md`**:
  - Authored new comprehensive guide (412 lines).
  - Analyzed IEEE-754 representation (sign, biased exponent, mantissa), binary representation error, and catastrophic cancellation.
  - Implemented safe relative epsilon comparisons, sentinels (`NaN`, `INFINITY`), and examined `<fenv.h>` rounding modes.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/21-signals-basics-portable.md`**:
  - Authored new comprehensive guide (418 lines).
  - Analyzed kernel software interrupts, signal frame redirection, and the strict Async-Signal-Safe principle.
  - Implemented graceful shutdown coordination using `volatile sig_atomic_t` flags.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/22-stdint-inttypes-and-limits.md`**:
  - Authored new comprehensive guide (405 lines).
  - Detailed platform data models (ILP32, LP64, LLP64), exact-width integers, pointer-sized integers (`uintptr_t`, `size_t`, `ptrdiff_t`), and `<inttypes.h>` format macros.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.
- **`03-intermediate/23-locale-and-wide-chars-deep-dive.md`**:
  - Authored new comprehensive guide (400 lines).
  - Detailed stateful multibyte-to-wide character streaming with `mbstate_t`, `mbrtowc`, and `wcrtomb`.
  - Analyzed shift states, POSIX thread-safe locale objects (`uselocale`), and cross-platform wide character pitfalls.
  - Added 6-row common mistakes table, 8 interview questions with `<details>`, and 3 practice exercises with full solutions.









## Batch 14 - Sub-Module Data Structures Core (2026-11-21)
- Authored sub-module overview `03-intermediate/data-structures/README.md`.
- Rewrote and extended `01-linked-lists.md` (675 lines): singly, doubly, circular, sentinel nodes, intrusive lists, Tortoise and Hare cycle detection, reverse in-place, 8 interview Qs, 9 exercises with full solutions.
- Rewrote and extended `02-stacks-queues.md` (583 lines): LIFO stack, FIFO queue, circular ring buffers, bitwise capacity masks, 8 interview Qs, 9 exercises with full solutions.
- Rewrote and extended `03-trees.md` (548 lines): binary trees, binary search trees, in-order, pre-order, post-order, BFS level-order, 8 interview Qs, 9 exercises with full solutions.
- Authored `04-hash-tables.md` (633 lines): hash functions (djb2, FNV-1a), separate chaining, linear probing, tombstones, load factor rehashing, HashDoS mitigations, 8 interview Qs, 9 exercises with full solutions.
- Authored `05-graphs.md` (595 lines): adjacency list vs matrix, DFS, BFS, shortest hop, 3-color directed cycle detection, Kahn's topological sort, 8 interview Qs, 9 exercises with full solutions.
- Authored `06-dynamic-array.md` (513 lines): capacity growth factors (1.5x vs 2.0x), amortized O(1), safe realloc idiom, memory compaction, 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout recorded in notes.

## Batch 15 - Sub-Module Data Structures Core Expansion (2026-11-21)
- Authored `07-queues-circular-buffer.md` (526 lines): power-of-two ring buffers, bitwise index masking, lock-free SPSC mechanics, cache line alignment, 8 interview Qs, 9 exercises with full solutions.
- Authored `08-heaps-and-priority-queues.md` (508 lines): binary min/max heaps in flat arrays, sift-up, sift-down, O(N) heapify, priority queues, 8 interview Qs, 9 exercises with full solutions.
- Authored `09-binary-search-trees.md` (533 lines): balanced BSTs, AVL tree rotations (LL/RR/LR/RL), Red-Black properties, range queries, 8 interview Qs, 9 exercises with full solutions.
- Authored `10-tries.md` (556 lines): prefix trees, radix compression, IP routing longest prefix match, wildcard search, 8 interview Qs, 9 exercises with full solutions.
- Authored `11-union-find.md` (536 lines): Disjoint Set Union, path compression, union by rank and size, inverse Ackermann O(alpha(N)) bounds, cycle detection, Kruskal's MST, 8 interview Qs, 9 exercises with full solutions.
- Authored `12-sorting-algorithms.md` (506 lines): Quicksort (Lomuto vs Hoare), Mergesort, Heapsort, Insertion sort, standard library qsort() comparator contracts, 8 interview Qs, 9 exercises with full solutions.
- Authored `13-searching-algorithms.md` (472 lines): overflow-safe binary search `low + (high - low)/2`, branch-free binary search, interpolation search, exponential search, standard bsearch(), 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout captured in notes.

## Batch 16 - Sub-Module Data Structures Advanced & Algorithms (2026-11-21)
- Authored `14-algorithm-complexity.md` (490 lines): asymptotic bounds, amortized analysis, potential method, ideal cache model, spatial locality, 8 interview Qs, 9 exercises with full solutions.
- Authored `15-skip-lists.md` (515 lines): probabilistic balance, multi-level express lanes, coin-flip height determination, update arrays, Redis zset mechanics, 8 interview Qs, 9 exercises with full solutions.
- Authored `16-bloom-filters.md` (499 lines): space-efficient probabilistic set membership, Kirsch-Mitzenmacher 2-hash optimization, false positive math, counting bloom filters, 8 interview Qs, 9 exercises with full solutions.
- Authored `17-lru-cache-structure.md` (521 lines): hash table + doubly linked list with dummy sentinels, O(1) recency promotion and eviction, in-place node recycling, 8 interview Qs, 9 exercises with full solutions.
- Authored `18-graph-algorithms.md` (467 lines): Dijkstra's shortest path, Bellman-Ford negative cycle detection, Floyd-Warshall all-pairs, Tarjan's strongly connected components, 8 interview Qs, 9 exercises with full solutions.
- Authored `19-string-algorithms.md` (489 lines): Knuth-Morris-Pratt (KMP) failure table, Rabin-Karp polynomial rolling hash, Boyer-Moore bad character heuristic, 8 interview Qs, 9 exercises with full solutions.
- Completed full Data Structures sub-module: 19 concept notes + README, 100% complete with verified companion code outputs.

## Batch 17 - Module 04 Advanced Systems Foundations (2026-11-21)
- Created module directory `04-advanced/` and authored `04-advanced/README.md`.
- Authored `01-custom-allocators-and-arenas.md` (461 lines): bump/linear allocators, power-of-two alignment arithmetic, temp/scratch arenas, mmap virtual memory backing, 8 interview Qs, 9 exercises with full solutions.
- Authored `02-memory-pools.md` (477 lines): fixed-size block pools, union embedded freelists, zero fragmentation, O(1) alloc/dealloc, ABA defense with tagged pointers, 8 interview Qs, 9 exercises with full solutions.
- Authored `03-memory-debugging-valgrind-asan.md` (429 lines): AddressSanitizer shadow memory mapping, Valgrind Memcheck binary instrumentation, UBsan, leak suppressions, 8 interview Qs, 9 exercises with full solutions.
- Authored `04-linking-static-vs-shared-libraries.md` (423 lines): static archives (.a), shared objects (.so/.dll), Position Independent Code (-fPIC), GOT/PLT lazy dynamic binding, Full RELRO, 8 interview Qs, 9 exercises with full solutions.
- Authored `05-abi-and-calling-conventions.md` (427 lines): System V AMD64 vs Microsoft x64, register parameter passing, 16-byte stack alignment rule, 32-byte shadow space, struct return in registers, 8 interview Qs, 9 exercises with full solutions.
- Authored `06-endianness-and-serialization.md` (489 lines): Big-Endian network byte order vs Little-Endian host order, explicit bit-shift packing/unpacking, struct padding traps, hardware bswap intrinsics, 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout captured in notes.

## Batch 18 - Module 04 Advanced Systems Programming Completion (2026-11-21)
- Authored `07-interfacing-with-assembly.md` (465 lines): NASM/GAS calling conventions, C symbol mangling and leading underscore rules, stack frame pointer alignment, SIMD scalar calling, 8 interview Qs, 9 exercises with full solutions.
- Authored `08-writing-a-small-library.md` (488 lines): Public vs private header separation, opaque struct handles, dynamic library export macros (`DLL_EXPORT`/`__attribute__((visibility))`), semantic versioning soname policies, 8 interview Qs, 9 exercises with full solutions.
- Authored `09-dynamic-loading-dlopen.md` (460 lines): POSIX `dlopen`/`dlsym`/`dlerror` vs Win32 `LoadLibrary`/`GetProcAddress`, runtime plugin architecture, symbol resolution modes (`RTLD_NOW` vs `RTLD_LAZY`), 8 interview Qs, 9 exercises with full solutions.
- Authored `10-symbol-visibility-and-elf-basics.md` (409 lines): ELF binary layout (`.text`, `.data`, `.rodata`, `.bss`), `-fvisibility=hidden`, GNU visibility attributes, version scripts, stripping local symbols, 8 interview Qs, 9 exercises with full solutions.
- Authored `11-inline-assembly-and-intrinsics-intro.md` (447 lines): Extended GCC inline assembly syntax, register/memory/immediate constraints, clobber lists, volatile side-effects, compiler barrier vs CPU fence, 8 interview Qs, 9 exercises with full solutions.
- Authored `12-object-files-and-the-linker-in-depth.md` (434 lines): Relocation entries (`R_X86_64_PC32`, `R_X86_64_64`), global symbol tables, weak symbols (`__attribute__((weak))`), Link-Time Optimization (`-flto`), linker scripts, 8 interview Qs, 9 exercises with full solutions.
- Updated `04-advanced/README.md` to 100% Complete status (all 12 notes verified).

## Batch 19 Part 1 - Module 05 Systems Programming Foundations (2026-11-21)
- Created module directory `05-systems-programming/` and authored `05-systems-programming/README.md`.
- Authored `01-file-descriptors-and-posix-io.md` (461 lines): Kernel file descriptor tables, open file descriptions, inodes, `open`/`read`/`write`/`close`/`lseek`, descriptor duplication via `dup2()`, non-blocking I/O (`O_NONBLOCK`), atomic appending, 8 interview Qs, 9 exercises with full solutions.
- Authored `02-processes-fork-exec-wait.md` (462 lines): Unix process model, parent/child relationships, Copy-on-Write (COW) memory mechanics, program image replacement with `execve()`, child reaping with `waitpid()`, zombie and orphan lifecycles, 8 interview Qs, 9 exercises with full solutions.
- Authored `03-signals.md` (419 lines): Kernel signal delivery and stack frames, reliable handling with `sigaction`, signal masks with `sigprocmask`, asynchronous signal safety rules, clean shutdown with `volatile sig_atomic_t`, self-pipe trick, 8 interview Qs, 9 exercises with full solutions.
- Authored `04-pipes-and-ipc.md` (433 lines): Anonymous pipes (`pipe()`), kernel ring buffer mechanics, multi-stage pipeline construction, named FIFOs (`mkfifo`), atomic writes with `PIPE_BUF`, `SIGPIPE` and `EPIPE` defense, 8 interview Qs, 9 exercises with full solutions.
- Authored `05-pthreads.md` (417 lines): POSIX multi-threading (`pthread_create`, `pthread_join`, `pthread_detach`), thread stack sizing and attributes, thread-local storage, cancellation cleanup handlers (`pthread_cleanup_push`), 8 interview Qs, 9 exercises with full solutions.
- Authored `06-mutex-condvar-semaphores.md` (500 lines): Mutual exclusion (`pthread_mutex_t`), fast userspace mutexes (futex), spurious wakeups and condition variable predicate loops (`pthread_cond_wait`), POSIX counting semaphores (`sem_t`), deadlock prevention, 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout captured in notes.

## Batch 19 Part 2 - Module 05 Advanced Concurrency & Systems Networking (2026-11-21)
- Authored `07-c11-atomics-and-memory-order.md` (424 lines): ISO C11 `<stdatomic.h>`, Compare-And-Swap (CAS) loops, 6 memory orderings (relaxed, acquire, release, seq_cst), hardware memory fences, spinlocks, ABA defense with tagged pointers, 8 interview Qs, 9 exercises with full solutions.
- Authored `08-c11-threads-h.md` (404 lines): Standard C11 `<threads.h>`, thread creation (`thrd_create`), mutexes (`mtx_t`), condition variables (`cnd_t`), thread-specific storage (`tss_t`), `_Thread_local`, 8 interview Qs, 9 exercises with full solutions.
- Authored `09-sockets-tcp-udp.md` (416 lines): Berkeley sockets API, IPv4/IPv6 (`AF_INET`/`AF_INET6`), stream sockets (`SOCK_STREAM`), datagram sockets (`SOCK_DGRAM`), TCP handshake, backlog queues, network byte order conversions (`htons`/`ntohl`), length-prefixed framing, 8 interview Qs, 9 exercises with full solutions.
- Authored `10-io-multiplexing-select-poll-epoll.md` (447 lines): Synchronous I/O multiplexing, `select()` and `FD_SETSIZE` limits, `poll()` array of `struct pollfd`, Linux `epoll` Red-Black tree and ready list architecture, Level-Triggered vs Edge-Triggered (`EPOLLET`), Reactor pattern, 8 interview Qs, 9 exercises with full solutions.
- Authored `11-mmap-and-shared-memory.md` (401 lines): Virtual memory page mapping, `mmap()`, `munmap()`, `msync()`, `MAP_SHARED` vs `MAP_PRIVATE` Copy-on-Write, anonymous memory mapping, zero-copy IPC via POSIX shared memory (`shm_open`), page faults, 8 interview Qs, 9 exercises with full solutions.
- Authored `12-daemons-and-system-calls.md` (429 lines): Unix daemon architecture, double-fork technique, session detachment (`setsid`), `/dev/null` standard descriptor redirection, syslog logging, direct Linux kernel system calls via `syscall()`, 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout captured in notes.

## Batch 19 Part 3 - Module 05 Systems Programming Completion (2026-11-21)
- Authored `13-terminals-and-termios.md` (408 lines): POSIX TTY line discipline, canonical (cooked) vs raw non-canonical mode, `struct termios`, disabling echo, `VMIN`/`VTIME` controls, ANSI VT100 escape codes, 8 interview Qs, 9 exercises with full solutions.
- Authored `14-filesystem-apis-stat-dirent.md` (407 lines): Filesystem inode architecture, `stat()` vs `lstat()` vs `fstat()`, directory streaming with `opendir()`/`readdir()`/`closedir()`, TOCTOU race condition mitigations, 8 interview Qs, 9 exercises with full solutions.
- Authored `15-errno-and-system-error-handling.md` (400 lines): Thread-local `errno` implementation, `strerror` vs thread-safe `strerror_r`, `EINTR` interrupted system call retry loops, preserving `errno` across cleanups, 8 interview Qs, 9 exercises with full solutions.
- Authored `16-time-and-timers-posix.md` (410 lines): Monotonic vs wall-clock time (`CLOCK_MONOTONIC` vs `CLOCK_REALTIME`), vDSO fast userspace clock calls, `nanosleep()` and drift-free `clock_nanosleep()`, Linux `timerfd` event loop integration, 8 interview Qs, 9 exercises with full solutions.
- Authored `17-resource-limits-and-process-control.md` (400 lines): POSIX `getrlimit()`/`setrlimit()`, file descriptor tuning (`RLIMIT_NOFILE`), virtual memory caps (`RLIMIT_AS`), core dumps (`RLIMIT_CORE`), CPU nice priorities, CPU affinity pinning, Linux cgroups v2 overview, 8 interview Qs, 9 exercises with full solutions.
- Completed Module 05 Systems Programming: 17 comprehensive concept notes + README, 100% complete with verified real outputs.

## Batch 20 Part 1 - Module 06 Build & Tooling Core (2026-11-21)
- Created module directory `06-build-and-tooling/` and authored `06-build-and-tooling/README.md`.
- Authored `01-make-in-depth.md` (418 lines): GNU Make DAG resolution, automatic header dependency tracking (`-MMD -MP`), pattern rules, automatic variables (`$@`, `$<`, `$^`), order-only prerequisites, non-recursive Makefiles, 8 interview Qs, 9 exercises with full solutions.
- Authored `02-cmake.md` (411 lines): Modern CMake 3.x target-based architecture, `PUBLIC`/`PRIVATE`/`INTERFACE` visibility propagation, generator expressions, CTest suite integration, out-of-source builds, 8 interview Qs, 9 exercises with full solutions.
- Authored `03-static-analysis-clang-tidy-cppcheck.md` (406 lines): AST matchers and symbolic execution, `clang-tidy` rules (`bugprone-*`, `cert-*`), `cppcheck` pipelines, `compile_commands.json` compilation database generation, 8 interview Qs, 9 exercises with full solutions.
- Authored `04-clang-format.md` (400 lines): `.clang-format` specification, style inheritance (LLVM/Google/Linux), brace placement and pointer alignment, `git-clang-format` staged formatting, pre-commit hooks, 8 interview Qs, 9 exercises with full solutions.
- Authored `05-gcov-coverage.md` (402 lines): Code coverage instrumentation (`--coverage`), `.gcno` and `.gcda` files, line vs branch coverage traps, HTML reports with `lcov` and `genhtml`, 8 interview Qs, 9 exercises with full solutions.
- Authored `06-git-workflow-for-c-projects.md` (411 lines): C-specific `.gitignore` rules, Git submodules for deterministic dependency management, Semantic Versioning (SemVer), GPG-signed release tags, 8 interview Qs, 9 exercises with full solutions.
- Compiled and executed companion programs in scratch directory; verified stdout captured in notes.

### Batch 20 Part 2: Module 06 Completion (Build & Tooling Notes 07-11)
- **Completed Notes**:
  - `07-cross-compilation-intro.md` (402 lines)
  - `08-pkg-config-and-dependencies.md` (400 lines)
  - `09-continuous-integration-for-c.md` (400 lines)
  - `10-doxygen-documentation.md` (460 lines)
  - `11-debug-vs-release-builds-and-sanitizer-ci.md` (438 lines)
  - `README.md` marked Complete (58 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured. All 11 notes in Module 06 meet >= 400 line standard with 13 standard sections.

### Batch 20 Part 3: Module 07 Completion (Testing in C Notes 01-07 + README)
- **Completed Module**: `07-testing/` (100% Complete)
- **Completed Notes**:
  - `README.md` (52 lines)
  - `01-unit-testing-with-unity-or-cmocka.md` (494 lines)
  - `02-test-doubles-and-mocking-in-c.md` (486 lines)
  - `03-fuzzing-with-libfuzzer-afl.md` (466 lines)
  - `04-test-driven-c.md` (442 lines)
  - `05-property-testing-ideas-in-c.md` (416 lines)
  - `06-testing-with-sanitizers-and-valgrind.md` (401 lines)
  - `07-code-coverage-targets.md` (413 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b20_p3.json`. All 7 notes meet >= 400 line standard with 13 standard sections.

### Batch 21 Part 1: Module 08 Security & Safety (Notes 01-06 + README)
- **Completed Notes**:
  - `README.md` (56 lines)
  - `01-buffer-overflows.md` (429 lines)
  - `02-format-string-vulnerabilities.md` (405 lines)
  - `03-integer-overflow-and-wraparound.md` (426 lines)
  - `04-use-after-free-and-double-free.md` (439 lines)
  - `05-secure-coding-cert-c.md` (416 lines)
  - `06-misra-c-overview.md` (424 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b21_p1.json`. All 6 notes meet >= 400 line standard with 13 standard sections.

### Batch 21 Part 2: Module 08 Completion (Security & Safety Notes 07-11)
- **Completed Module**: `08-security-and-safety/` (100% Complete)
- **Completed Notes**:
  - `07-hardening-flags.md` (409 lines)
  - `08-race-conditions-and-toctou.md` (412 lines)
  - `09-safe-string-handling-patterns.md` (404 lines)
  - `10-secure-random-and-crypto-pitfalls.md` (471 lines)
  - `11-exploit-mitigations-explained.md` (408 lines)
  - `README.md` marked Complete (56 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b21_p2.json`. All 11 notes in Module 08 meet >= 400 line standard with 13 standard sections.

### Batch 21 Part 3: Module 09 Performance & Optimization (Notes 01-05 + README)
- **Completed Notes**:
  - `README.md` (56 lines)
  - `01-cache-and-memory-hierarchy.md` (405 lines)
  - `02-profiling-perf-gprof-valgrind-callgrind.md` (402 lines)
  - `03-compiler-optimizations.md` (407 lines)
  - `04-branch-prediction-and-data-layout.md` (419 lines)
  - `05-simd-intrinsics-intro.md` (401 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b21_p3.json`. All 5 notes meet >= 400 line standard with 13 standard sections.


### Batch 21 Part 4: Module 09 Completion (Performance & Optimization Notes 06-10)
- **Completed Module**: `09-performance/` (100% Complete)
- **Completed Notes**:
  - `06-benchmarking-correctly.md` (411 lines): Monotonic timers, warm-up iterations, statistical rigor, compiler optimization clobbers (`benchmark_clobber`), 10 interview Qs, 3 exercises with full solutions.
  - `07-struct-of-arrays-vs-array-of-structs.md` (473 lines): AoS vs SoA memory layout, cache line utilization, SIMD vectorization friendly structures, 10 interview Qs, 3 exercises with full solutions.
  - `08-false-sharing-and-concurrency-performance.md` (418 lines): Multi-core MESI/MOESI cache coherence protocols, cache line bouncing, `alignas(64)` padding, 10 interview Qs, 3 exercises with full solutions.
  - `09-link-time-optimization-and-pgo.md` (508 lines): Whole-program interprocedural analysis, intermediate representation serialization, PGO feedback cycle (`-fprofile-generate`/`-fprofile-use`), trace scheduling, 10 interview Qs, 3 exercises with full solutions.
  - `10-inlining-and-code-size.md` (493 lines): Inlining cost-benefit models, stack frame elimination, L1 instruction cache thrashing, `static inline` vs `always_inline` vs `cold`, 10 interview Qs, 3 exercises with full solutions.
  - `README.md` marked Complete (56 lines)
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b21_p4.json`. All 10 notes in Module 09 meet >= 400 line standard with 13 standard sections.


### Batch 22: Module 10 Completion (Design and Architectural Patterns in C)
- **Completed Module**: `10-design-and-patterns/` (100% Complete)
- **Completed Notes**:
  - `README.md` (51 lines)
  - `01-modular-design-in-c.md` (560 lines): Single-responsibility modules, interface headers (`.h`), opaque pointer encapsulation (`typedef struct Foo Foo;`), loose coupling, ABI stability, 10 interview Qs, 3 exercises with full solutions.
  - `02-design-patterns-in-c.md` (523 lines): GoF patterns in procedural C: Strategy, Observer, Factory, Iterator, State Machine tables, runtime polymorphism via static `const` Vtables in `.rodata`, single inheritance via first-member struct embedding, 10 interview Qs, 3 exercises with full solutions.
  - `03-error-handling-patterns.md` (540 lines): Status code returns with dedicated out-parameters, centralized `goto cleanup` stack unwinding idiom, structured error context objects (`Error**`), thread-local `errno` conventions, 10 interview Qs, 3 exercises with full solutions.
  - `04-coding-style-guide.md` (446 lines): Systematic identifier prefixing (`lib_module_action`), strict const-correctness, header hygiene (`extern "C"` and guards), exact-width types (`<stdint.h>`), defensive macros (`do { ... } while(0)`), 10 interview Qs, 3 exercises with full solutions.
  - `05-resource-management-patterns.md` (528 lines): Scope-bound RAII via compiler cleanup attributes (`__attribute__((cleanup))`), high-speed $O(1)$ bump arena allocators, atomic reference counting retain/release semantics, paired init/destroy contracts, 10 interview Qs, 3 exercises with full solutions.
  - `06-api-design-for-c-libraries.md` (532 lines): Long-term ABI stability, forward-compatible configuration structs (`struct_size` field), symbol export control (`-fvisibility=hidden`, `visibility("default")`), reentrant interfaces, 10 interview Qs, 3 exercises with full solutions.
  - `07-ownership-and-lifetime-conventions.md` (554 lines): Caller-allocates (borrowing) vs Callee-allocates (ownership transfer) paradigms, stack vs heap lifetime boundaries, documenting ownership annotations, preventing UAF and double free, 10 interview Qs, 3 exercises with full solutions.
- **Verification**: All companion C test code executed in scratch; verified real outputs captured in `real_outputs_b22_p1.json` and `real_outputs_b22_p2.json`. All 7 notes in Module 10 meet >= 400 line standard with all 13 standard sections.


### Batch 23: Module 11 Completion (14 Production Projects in C)
- **Completed Module**: `11-projects/` (100% Complete)
- **Completed Projects (Each with complete architecture, self-contained source code, Makefile, and verification)**:
  - `README.md` (57 lines): Project matrix, domain breakdown, architectural philosophy, suggested paths.
  - `01-tiny-shell/README.md` (413 lines): UNIX interactive command shell, fork/execvp lifecycle, piping (`pipe`), redirection (`dup2`), job control, signal handling (`SIGINT`).
  - `02-http-server/README.md` (400 lines): Event-driven non-blocking TCP HTTP/1.1 web server, `poll()` multiplexing, HTTP request line parsing, MIME type resolution, static file streaming.
  - `03-key-value-store/README.md` (393 lines): In-memory key-value database, djb2 hash table with separate chaining, crash-resilient Append-Only File (AOF) disk logging and recovery engine.
  - `04-memory-allocator/README.md` (323 lines): Custom heap allocator (`my_malloc`, `my_free`, `my_realloc`), boundary tag architecture (headers/footers), constant-time $O(1)$ bidirectional coalescing, explicit free list.
  - `05-json-parser/README.md` (407 lines): RFC 8259 compliant recursive-descent JSON parser, lexical scanner, Abstract Syntax Tree (AST), serializer, safe deep memory reclamation.
  - `06-text-editor-or-grep-clone/README.md` (222 lines): High-speed parallel grep utility, memory-mapped file I/O (`mmap`), zero-copy pointer scanning, ANSI colored terminal match highlighting.
  - `07-thread-pool/README.md` (282 lines): Production concurrent worker thread pool, POSIX threads (`pthreads`), mutex-protected circular task queue, condition variables, completion barrier (`thread_pool_wait`).
  - `08-lru-cache/README.md` (282 lines): High-performance Least Recently Used (LRU) cache, hash table indexing coupled with intrusive doubly linked list for guaranteed $O(1)$ get, put, and eviction.
  - `09-tiny-vm-or-interpreter/README.md` (228 lines): Stack-based bytecode virtual machine, instruction set architecture (ISA), operand stack evaluation, fetch-decode-execute interpreter loop, bytecode disassembler.
  - `10-dynamic-string-library/README.md` (198 lines): ABI-stable binary-safe dynamic string library (`sds`-style), prefix header with length and capacity, $O(1)$ length inspection, amortized exponential growth.
  - `11-tcp-chat-server/README.md` (259 lines): Multi-client broadcast chat daemon, `poll()` I/O multiplexing, client tracking array, nickname management (`/nick`), broadcast protocol.
  - `12-file-compression-huffman/README.md` (266 lines): Huffman coding lossless file compression utility, byte frequency analysis, min-heap priority queue, prefix-free binary tree construction, header serialization.
  - `13-ring-buffer-logger/README.md` (243 lines): Ultra-low-latency asynchronous logger, lock-free Single-Producer Single-Consumer (SPSC) circular ring buffer, C11 atomics, cache line alignment (`_Alignas(64)`), background disk flusher.
  - `14-mini-regex-engine/README.md` (207 lines): Regular expression compiler and matcher implementing Thompson's NFA construction, state graph simulation, linear $O(M \times N)$ execution without exponential backtracking.
- **Verification**: Clean directory structure with markdown-only documentation and verified self-contained implementations.
