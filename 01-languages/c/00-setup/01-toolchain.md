# Toolchain

A C toolchain is the set of programs that turn `.c` files into a running executable and help you debug it. C has no runtime or package manager of its own, so what you install decides how you build, debug, and diagnose problems.

## What you need

| Tool | Role | Common choices |
|---|---|---|
| Compiler | Translates C to machine code | `gcc`, `clang` |
| Build tool | Rebuilds only what changed | `make`, `cmake` + `ninja` |
| Debugger | Step through and inspect a running program | `gdb`, `lldb` |
| Binutils | Inspect and manipulate object files | `ld`, `as`, `objdump`, `nm`, `readelf` |
| Memory tools | Find leaks and invalid access | AddressSanitizer, `valgrind` (Linux) |

You can start with only a compiler. The rest becomes necessary within the first few weeks.

## Installing

```bash
# Debian / Ubuntu
sudo apt update
sudo apt install build-essential gdb valgrind clang

# Fedora
sudo dnf install gcc clang make gdb valgrind

# macOS (installs Apple clang, make, lldb)
xcode-select --install
```

**Windows:** the least painful option is **WSL2** with Ubuntu, then follow the Debian steps. Native alternatives are MSYS2 (MinGW-w64 GCC) or Visual Studio's MSVC. MSVC's C support differs from GCC/Clang in places, and most tutorials and the rest of this repository assume a GCC/Clang environment.

> **macOS gotcha:** `gcc` on macOS is usually a wrapper around Apple's `clang`. Run `gcc --version` and you'll see "Apple clang". That is fine for learning, but it is not GNU GCC.

## Verify

```bash
gcc --version
clang --version
make --version
gdb --version      # lldb --version on macOS
```

Every command should print a version. If one says `command not found`, the install or your `PATH` is wrong.

## GCC or Clang?

Both implement the standard well and accept almost the same flags. Pick one and learn it, then try the other occasionally, because each reports different warnings and errors.

- **GCC** is the default on most Linux distributions and has a long track record in embedded and systems work.
- **Clang** often gives clearer diagnostics and powers `clangd`, `clang-tidy` and `clang-format` (see [editor setup](./02-editor-setup.md)).

Compiling the same code with both is a cheap way to catch portability problems early.

## A sensible default command

```bash
gcc -std=c17 -g -Wall -Wextra -Wpedantic hello.c -o hello
```

| Flag | Why |
|---|---|
| `-std=c17` | Pin the language standard instead of relying on compiler defaults |
| `-g` | Include debug symbols for gdb |
| `-Wall -Wextra` | Enable the warnings that catch real bugs |
| `-Wpedantic` | Warn about non-standard extensions |
| `-o hello` | Name the output (default is `a.out`) |

While learning, add sanitizers:

```bash
gcc -std=c17 -g -Wall -Wextra -fsanitize=address,undefined hello.c -o hello
```

This catches out-of-bounds access, use-after-free and many cases of undefined behavior at runtime. See [sanitizers](../06-debugging-testing/04-sanitizers-valgrind.md).

## Common problems

- **`command not found: gcc`:** the package isn't installed, or on macOS the command-line tools aren't.
- **`fatal error: stdio.h: No such file or directory`:** the C library headers are missing. On Debian-based systems install `build-essential`.
- **Works in the terminal, not in the editor:** the editor is using a different compiler or configuration. See [editor setup](./02-editor-setup.md).

## Summary

- A minimal setup is compiler + `make` + debugger; add sanitizers early.
- Always set `-std=` and enable `-Wall -Wextra`. Warnings are free bug reports.
- On macOS `gcc` is Apple clang. On Windows, prefer WSL2.

**Next:** [Editor setup](./02-editor-setup.md)
