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
