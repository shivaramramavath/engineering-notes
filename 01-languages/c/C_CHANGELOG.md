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
