# Cross-Compilation Foundations

> Master cross-compilation in C: target machine triples (`arch-vendor-os-abi`), cross-compiler toolchains (`gcc`, `binutils`), sysroots, cross-compiling from Linux to Windows with MinGW, CMake toolchain files, and testing binaries with QEMU user emulation.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-make-in-depth.md](./01-make-in-depth.md), [02-cmake.md](./02-cmake.md), [04-advanced/05-abi-and-calling-conventions.md](../04-advanced/05-abi-and-calling-conventions.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| GCC 9+ / Clang 10+ / GNU Binutils / POSIX.1-2008 / Win32 |

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
In standard native compilation, the computer running the compiler (the **Host**) is identical to the target machine executing the resulting binary (the **Target**). For example, compiling on an x86-64 Ubuntu laptop to produce an x86-64 Ubuntu binary.

However, embedded devices (Raspberry Pi, ARM Cortex-M, automotive ECUs, routers) frequently lack the CPU power, RAM, or disk storage required to run heavy compilers and build suites. Furthermore, developers working on Linux cloud servers often need to build Windows `.exe` installers or macOS binaries.

**Cross-Compilation** is the technique of building executable code on a fast host development machine for an entirely different target architecture, operating system, or C library ABI. Mastering cross-compilation enables rapid firmware development, embedded systems deployment, and cross-platform release engineering from a single centralized build server.

---

## 2. Core Concepts
- **Machine Roles**:
  - **Build**: The machine where the compiler is built (only relevant when building the compiler itself).
  - **Host**: The machine executing the compiler to build software (e.g. your x86-64 Linux workstation).
  - **Target**: The machine that will execute the generated binary (e.g. an ARM64 Linux server or a 32-bit Windows machine).
- **Target Triples (Tuples)**: A standardized string identifying the target platform: `architecture-vendor-os-abi`:
  - `x86_64-w64-mingw32`: 64-bit Windows via MinGW.
  - `arm-linux-gnueabihf`: 32-bit ARM Linux with hardware floating point.
  - `aarch64-linux-gnu`: 64-bit ARM Linux (glibc).
  - `riscv64-unknown-elf`: Bare-metal 64-bit RISC-V.
- **Sysroot (`--sysroot`)**: A directory on the host machine containing the target system's headers (`/usr/include`) and libraries (`/usr/lib`). It acts as a miniature replica of the target operating system's root filesystem.
- **Toolchain Prefix**: Cross-compilation tools prefix their executables with the target triple: `x86_64-w64-mingw32-gcc`, `aarch64-linux-gnu-ld`, `arm-none-eabi-objcopy`.
- **QEMU User Emulation (`qemu-user`)**: Runs foreign-architecture Linux binaries transparently on the host CPU using dynamic binary translation (e.g. running an ARM binary directly on x86-64 Linux via `qemu-aarch64 ./app`).

---

## 3. How It Works Internally

### Cross-Toolchain Compilation Flow
When invoking a cross-compiler on an x86-64 Host targeting AArch64:

```
Host Workstation (x86-64 Linux)
+--------------------------------------------------------------------+
| Invocation: aarch64-linux-gnu-gcc --sysroot=/opt/sysroot main.c    |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 1. Cross-Preprocessor (Runs on x86-64)                             |
|    - Includes headers from /opt/sysroot/usr/include (AArch64 types)|
|    - Defines __aarch64__, __LP64__, __ARM_ARCH_8A__               |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 2. Cross-Compiler (cc1 running on x86-64)                          |
|    - Generates AArch64 machine instructions (LDR, STR, ADD X0, X1) |
+--------------------------------------------------------------------+
                                |
                                v
+--------------------------------------------------------------------+
| 3. Cross-Linker (aarch64-linux-gnu-ld running on x86-64)           |
|    - Resolves symbols against /opt/sysroot/lib/libc.so (AArch64)   |
|    - Produces ELF 64-bit LSB executable, ARM aarch64               |
+--------------------------------------------------------------------+
                                |
       [Deploy binary to physical target or test via QEMU]
                                v
Target Board: Raspberry Pi 4 (AArch64 Native Execution)
```

---

## 4. Syntax & Usage

### CMake Toolchain File Template (`aarch64-toolchain.cmake`)
Modern CMake abstracts cross-compilation cleanly using toolchain files passed via `-DCMAKE_TOOLCHAIN_FILE=...`:

```cmake
# Target System Configuration
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-Compiler Binary Paths
set(TOOLCHAIN_PREFIX aarch64-linux-gnu)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib)

# Sysroot Configuration
set(CMAKE_SYSROOT /opt/sysroots/aarch64)

# Search Path Policy: Never search host for libraries/headers!
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

### Invoking CMake Cross-Build
```bash
# Configure and build for AArch64 using the toolchain file
cmake -B build-arm64 -S . -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-toolchain.cmake
cmake --build build-arm64 -j$(nproc)

# Verify the binary architecture
file build-arm64/bin/my_app
# Output: build-arm64/bin/my_app: ELF 64-bit LSB executable, ARM aarch64...
```

---

## 5. Practical Examples

### Example 1: Target Architecture and Tuple Detection
```c
#include <stdio.h>

const char *get_target_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "i686";
#elif defined(__aarch64__)
    return "aarch64";
#elif defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}

const char *get_target_os(void) {
#if defined(__linux__)
    return "linux-gnu";
#elif defined(_WIN32)
    return "w64-mingw32";
#elif defined(__APPLE__)
    return "apple-darwin";
#else
    return "unknown";
#endif
}

int main(void) {
    printf("Evaluating Cross-Compilation Target Architecture Tuple...\n");
    printf("Host/Target Architecture: %s\n", get_target_arch());
    printf("Host/Target OS Environment: %s\n", get_target_os());
    printf("Configured Target Triple: %s-%s\n", get_target_arch(), get_target_os());
    printf("Cross-compiler prefix: %s-%s-gcc\n", get_target_arch(), get_target_os());
    return 0;
}
```

```text
Evaluating Cross-Compilation Target Architecture Tuple...
Host/Target Architecture: i686
Host/Target OS Environment: w64-mingw32
Configured Target Triple: i686-w64-mingw32
Cross-compiler prefix: i686-w64-mingw32-gcc
```

### Example 2: Cross-Compiling from Linux to Windows using MinGW-w64
Building Windows binaries from a Linux CI runner is a standard practice:

```bash
# Install MinGW-w64 on Ubuntu
sudo apt-get install -y gcc-mingw-w64

# Cross-compile Windows 64-bit executable with static linking
x86_64-w64-mingw32-gcc -O2 -Wall src/main.c -static -o build/app.exe

# Verify with file utility
file build/app.exe
# Output: PE32+ executable (console) x86-64, for MS Windows
```

---

## 6. Best Practices
1. **Never Let Host Headers Leak Into Target Builds**: Accidentally including host `/usr/include/stdio.h` instead of target sysroot headers leads to subtle ABI and struct alignment mismatches. Use `CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY`.
2. **Always Statically Link System Runtimes When Appropriate**: When cross-compiling to Windows or bare-metal targets, use `-static` or link static runtime libraries (`-static-libgcc`) to eliminate missing DLL or shared library runtime crashes on target machines.
3. **Use QEMU User Emulation for Automated CI Testing**: Install `qemu-user-static` and `binfmt-support` so your x86-64 CI server can execute cross-compiled ARM or RISC-V test suites directly with `ctest`.
4. **Beware Endianness and Structure Packing**: Never assume the target has the same endianness or pointer width as the host. Write endian-independent serialization and avoid raw binary struct dumping.
5. **Set `CC` and `AR` Explicitly in Makefiles**: Makefiles must use `$(CC)` and `$(AR)` so passing `make CC=arm-linux-gnueabihf-gcc` cross-compiles cleanly without modifying source files.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Linking against host `/usr/lib/libc.so` | Linker fails with "incompatible architecture" or produces crash on execution | Supply `--sysroot=/path/to/target/sysroot` |
| Running cross-compiled binary on host | Host CPU cannot execute foreign machine code; fails with "Exec format error" | Use QEMU user emulation: `qemu-aarch64 ./app` |
| Executing build-time code generators on target | If your build compiles a helper tool (`make_tables.c`) and runs it during build, cross-compiling makes it unrunnable on host | Build generator tool with host compiler (`gcc`), and application with cross-compiler |
| Hardcoding `sizeof(long)` or pointer sizes | `long` is 32-bit on Windows x64 and 32-bit ARM, but 64-bit on Linux x86-64 | Use `<stdint.h>` types: `uint32_t`, `uint64_t` |
| Assuming same endianness | Target might be Big-Endian (MIPS, PowerPC) while host is Little-Endian (x86) | Use explicit bit-shifting or byte swapping functions (`htons`) |
| Forgetting `CMAKE_SYSTEM_NAME` in toolchain | CMake assumes native build if `CMAKE_SYSTEM_NAME` is omitted | Always declare `set(CMAKE_SYSTEM_NAME Linux)` or `Windows` |

---

## 8. Performance & Security Notes
- **Speed of Cross-Compilation**: Building on a 64-core AMD EPYC host server can compile a complete Linux kernel or embedded firmware image in 45 seconds, whereas natively compiling on a Raspberry Pi 4 takes over 45 minutes.
- **Reproducible Cross-Builds with Docker**: Encapsulate cross-compilers, sysroots, and CMake toolchain files inside a dedicated Docker container image (e.g. `dockcross/linux-arm64`) to eliminate "works on my machine" toolchain drift.

---

## 9. Comparison / When to Use What

| Environment | Host CPU | Target CPU | Execution Speed | Setup Complexity |
| :--- | :--- | :--- | :--- | :--- |
| **Native Build** | x86-64 | x86-64 | Native | Zero |
| **Cross-Compilation** | x86-64 | ARM64 / RISC-V | **Blazing Fast (Host CPU)** | Moderate (Requires sysroot) |
| **QEMU Full System Emulation**| x86-64 | ARM64 VM | Slow ($5	ext{--}10	imes$ slowdown) | High |
| **Physical Target Native Build**| ARM64 | ARM64 | Slow (Limited by board CPU/RAM) | Zero |

---

## 10. Interview Questions

<details>
<summary>1. What are the three machine definitions in cross-compilation terminology (Build, Host, Target)?</summary>
- **Build**: The machine where the compiler itself was compiled and built.
- **Host**: The machine where the compiler runs to compile software (e.g. your development workstation).
- **Target**: The machine that will execute the generated binary produced by the compiler.
In native compilation, Build = Host = Target. In standard cross-compilation, Build = Host $
eq$ Target. In Canadian Cross compilation, Build $
eq$ Host $
eq$ Target.
</details>

<details>
<summary>2. What is a "sysroot" and why is it necessary?</summary>
A sysroot is a directory on the host filesystem that mirrors the target operating system's root hierarchy, containing the target's specific C library headers (`/usr/include`), shared libraries (`/usr/lib`, `/lib`), and linker scripts. The cross-compiler references the sysroot via `--sysroot` to resolve header declarations and link against target-compatible libraries rather than contaminating the build with host libraries.
</details>

<details>
<summary>3. What causes an "Exec format error" when running a newly compiled binary?</summary>
The kernel returns "Exec format error" (error `ENOEXEC`) when an executable binary was compiled for an architecture or operating system incompatible with the current machine's CPU and kernel (e.g. attempting to run an ARM64 ELF binary on an x86-64 Linux kernel without QEMU binfmt emulation).
</details>

<details>
<summary>4. What is a target triple (or tuple) in GCC and Clang?</summary>
A target triple is a standardized string following the format `architecture-vendor-os-abi` that uniquely identifies the target machine. Examples include `x86_64-pc-linux-gnu`, `arm-linux-gnueabihf` (ARM with hard float), and `x86_64-w64-mingw32` (Windows x64).
</details>

<details>
<summary>5. How do you handle build-time code generators during a cross-compilation build?</summary>
In many C projects, a helper program is compiled and executed during the build process to generate lookup tables or source headers. In a cross-compilation build, the application cannot run the cross-compiled helper tool on the host. The solution is to compile the helper tool using the host's native compiler (`gcc`), run it to generate the code, and then compile the main application using the cross-compiler (`aarch64-linux-gnu-gcc`).
</details>

<details>
<summary>6. How does QEMU User Emulation (`qemu-user-static`) work?</summary>
QEMU user emulation executes foreign CPU machine instructions by dynamically translating them at runtime into host CPU instructions. When the foreign binary issues a system call (like `sys_write`), QEMU intercepts the system call, translates the memory structures across endianness and pointer boundaries, and forwards the system call to the host Linux kernel.
</details>

<details>
<summary>7. What is the purpose of `CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY`?</summary>
It instructs CMake's `find_file` and `find_path` commands to search exclusively inside the target's sysroot directory, strictly forbidding CMake from searching host system directories (like `/usr/include`). This prevents host headers from contaminating cross-compiled builds.
</details>

<details>
<summary>8. How do you cross-compile a C program using Clang?</summary>
Unlike GCC (which uses separate binaries like `arm-linux-gnueabihf-gcc`), a single Clang binary can target all supported architectures natively using the `-target` flag:
`clang -target aarch64-linux-gnu --sysroot=/path/to/sysroot main.c -o app`
</details>

---


<details>
<summary><strong>7. How do you handle architecture-specific endianness and struct padding when cross-compiling?</strong></summary>

When building for a target with different endianness (e.g. big-endian ARM or MIPS from an x86_64 host), never assume native host byte order. Use standard endian conversion macros (such as <code>htobe32()</code>, <code>be32toh()</code> from <code><endian.h></code> or POSIX <code>htonl()</code>). Additionally, structure packing and alignment rules differ across 32-bit and 64-bit architectures; use explicit fixed-width integer types (<code>uint32_t</code>, <code>uint64_t</code>) and explicit padding or <code>__attribute__((packed))</code> where wire or hardware registers require strict layout.
</details>

<details>
<summary><strong>8. What is a sysroot and why can't you just link against host libraries with <code>-L/usr/lib</code>?</strong></summary>

A sysroot represents the target root directory containing headers and libraries compiled strictly for the target CPU architecture and ABI. If you accidentally point the linker to host paths like <code>/usr/lib</code> or <code>/lib/x86_64-linux-gnu</code>, the cross-linker encounters elf architecture mismatches (e.g., trying to link 32-bit ARM object files with 64-bit x86_64 shared libraries), causing immediate linker termination with <code>architecture of input file is incompatible with target output</code>.
</details>


<details>
<summary><strong>9. How does QEMU user-mode emulation simplify cross-compilation testing?</strong></summary>

QEMU user-mode emulation (<code>qemu-user-static</code>) allows a host Linux system to execute target-architecture binaries seamlessly via Linux <code>binfmt_misc</code> kernel support. When a cross-compiled ARM or RISC-V binary is launched on an x86_64 host, the kernel intercepts the ELF magic bytes and invokes QEMU, which translates target CPU instructions dynamically into host instructions, allowing test suites to run directly in CI without physical target hardware.
</details>

## 11. Practice Exercises

### Easy
1. **Target Tuple Inspector**: Compile and run the architecture detection program to inspect your current host tuple.
2. **Cross-Compiler Prefix Check**: Write a small shell script that verifies whether `x86_64-w64-mingw32-gcc` or `arm-linux-gnueabihf-gcc` is installed.
3. **Clang Target Query**: Run `clang --version` and inspect the default registered target.

<details>
<summary>Solutions (Easy)</summary>

```bash
# Exercise 2
if which x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "MinGW-w64 cross-compiler is installed."
else
    echo "MinGW-w64 cross-compiler not found."
fi
```
</details>

### Medium
4. **CMake Toolchain for Windows**: Author a complete `windows-mingw.cmake` toolchain file for cross-compiling to Windows from Linux.
5. **Cross-Compiled Static Windows Binary**: Write a Makefile that builds a standalone, zero-dependency Windows `.exe` using `x86_64-w64-mingw32-gcc -static`.
6. **Dual-Compiler Makefile**: Author a Makefile that compiles a code generator tool `gen_table` with `HOST_CC ?= gcc`, executes it to create `table.h`, and compiles `app` with `CROSS_CC ?= $(CC)`.

<details>
<summary>Solutions (Medium)</summary>

```makefile
# Exercise 6: Dual-Compiler Makefile
HOST_CC  ?= gcc
CROSS_CC ?= $(CC)

.PHONY: all clean

all: app

table.h: gen_table
	./gen_table > $@

gen_table: tools/gen_table.c
	$(HOST_CC) -O2 $< -o $@

app: src/main.c table.h
	$(CROSS_CC) -O2 -I. src/main.c -o $@

clean:
	rm -f gen_table table.h app
```
</details>

### Hard
7. **Complete Dockerized Cross-Build Environment**: Author a `Dockerfile` based on Ubuntu that installs `gcc-aarch64-linux-gnu`, `qemu-user-static`, CMake, and builds an ARM64 test binary running under QEMU.
8. **Automated Cross-Architecture CI Matrix**: Write a GitHub Actions YAML workflow that builds and runs unit tests for both x86-64 and ARM64 (via QEMU) on every pull request.
9. **Universal CMake Cross-Platform Toolchain Suite**: Create a clean repository directory structure with reusable CMake toolchain files for `arm64-linux`, `arm-cortex-m4-baremetal`, and `windows-x64`.

<details>
<summary>Solutions (Hard)</summary>

```dockerfile
# Exercise 7: Dockerfile for AArch64 Cross-Compilation with QEMU
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y     gcc-aarch64-linux-gnu     g++-aarch64-linux-gnu     qemu-user-static     binfmt-support     cmake     ninja-build     file

WORKDIR /workspace
CMD ["/bin/bash"]
```
</details>

---

## 12. Summary / Cheat Sheet
- **Tuple**: `arch-vendor-os-abi` (e.g. `aarch64-linux-gnu`, `x86_64-w64-mingw32`).
- **Sysroot**: Target filesystem replica containing target headers and libraries.
- **Prefix**: Tools named `<tuple>-gcc`, `<tuple>-ld`, `<tuple>-ar`.
- **CMake Toolchain**: `set(CMAKE_SYSTEM_NAME Linux)` + `set(CMAKE_C_COMPILER ...)`.
- **Clang Flag**: `clang -target aarch64-linux-gnu --sysroot=...`.
- **Testing**: Use `qemu-user` (`qemu-aarch64 ./app`) to run foreign binaries on host.

---

## 13. Further Reading
- Karim Yaghmour, *Building Embedded Linux Systems* (O'Reilly).
- CMake Official Documentation: Cross Compiling with CMake.
- GNU GCC Documentation: Specifying Target Machine Triples.

---

[← Previous Note: Git Workflow for C Projects](./06-git-workflow-for-c-projects.md) | [Module Index](README.md) | [Next Note: pkg-config and Library Dependencies](./08-pkg-config-and-dependencies.md)
