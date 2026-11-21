# Module 04: Advanced C Systems Programming

> Master production systems engineering in C: custom memory allocators and arenas, fixed-size memory pools, ELF symbol visibility, ABIs and calling conventions, cross-platform endianness serialization, dynamic loading (`dlopen`/`LoadLibrary`), inline assembly, and link-time mechanics.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `04-advanced` |
| **Prerequisites** | [02-core-concepts/08-memory-management.md](../02-core-concepts/08-memory-management.md), [03-intermediate/13-opaque-pointers-and-adts.md](../03-intermediate/13-opaque-pointers-and-adts.md), [03-intermediate/18-strict-aliasing-and-type-punning.md](../03-intermediate/18-strict-aliasing-and-type-punning.md) |
| **Duration** | ~25 hours |
| **Status** | In Progress (Batches 17–18) |
| **Standard Baseline**| ISO C11 / C17 / C23 (POSIX.1-2008 & Win32 compatibility) |

---

## Overview & Learning Outcomes
Moving beyond standard C libraries into low-level systems programming requires peeling back the runtime abstractions to interact directly with the operating system kernel, executable file formats (ELF / PE-COFF), hardware memory management units (MMUs), and the CPU instruction pipeline.

In this module, you will master:
1. **Custom Memory Architecture**: Building high-speed bump allocators, region arenas, and fixed-block object pools that outperform general-purpose `malloc()` by $10\times$ while completely eliminating memory fragmentation.
2. **Binary Linking & Executable Formats**: Decoupling compilation from linking, inspecting symbol tables (`nm`, `objdump`, `readelf`), managing symbol visibility (`__attribute__((visibility("hidden")))`), and resolving link-time collisions.
3. **ABI & Calling Conventions**: How parameters are passed in machine registers vs the call stack (System V AMD64 ABI vs Microsoft x64 vs cdecl), stack alignment constraints, and struct return protocols.
4. **Binary Serialization & Network Wire Formats**: Endian-independent byte packing, zero-padding serialization, and hardware-accelerated byte swapping (`htons`, `ntohl`, `bswap`).
5. **Dynamic Loading & Plugins**: Loading shared libraries at runtime using `dlopen()` / `dlsym()` on POSIX and `LoadLibrary()` / `GetProcAddress()` on Windows.
6. **Assembly Integration**: Interfacing pure C code with assembly subroutines and utilizing GCC/Clang inline assembly (`__asm__ __volatile__`) with explicit input, output, and clobber constraints.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Custom Allocators and Arenas](./01-custom-allocators-and-arenas.md) | Advanced | Complete | Bump/linear allocators, region lifetimes, alignment padding, resetting arenas |
| **02** | [Memory Pools](./02-memory-pools.md) | Advanced | Complete | Fixed-size block allocation, embedded freelists, $O(1)$ zero-fragmentation |
| **03** | [Memory Debugging: Valgrind & ASan](./03-memory-debugging-valgrind-asan.md) | Advanced | Complete | AddressSanitizer (`-fsanitize=address`), Valgrind Memcheck, UBsan, leak detection |
| **04** | [Linking: Static vs Shared Libraries](./04-linking-static-vs-shared-libraries.md) | Advanced | Complete | Archive `.a` vs shared `.so`/`.dll`, PIC (`-fPIC`), runtime dynamic linking |
| **05** | [ABI and Calling Conventions](./05-abi-and-calling-conventions.md) | Advanced | Complete | System V vs MS x64, caller vs callee saved registers, stack frames, 16-byte alignment |
| **06** | [Endianness and Serialization](./06-endianness-and-serialization.md) | Advanced | Complete | Big-endian vs little-endian, byte swapping, safe network packet serialization |
| **07** | [Interfacing with Assembly](./07-interfacing-with-assembly.md) | Advanced | Planned | Calling NASM/GAS assembly from C, symbol naming, stack setup, return values |
| **08** | [Writing a Small C Library](./08-writing-a-small-library.md) | Advanced | Planned | Header design, public vs private symbols, semantic versioning, distribution |
| **09** | [Dynamic Loading with dlopen](./09-dynamic-loading-dlopen.md) | Advanced | Planned | Runtime plugin architecture, `dlopen`, `dlsym`, `dlerror`, Win32 `LoadLibrary` |
| **10** | [Symbol Visibility and ELF Basics](./10-symbol-visibility-and-elf-basics.md) | Advanced | Planned | ELF sections (`.text`, `.data`, `.bss`, `.rodata`), `-fvisibility=hidden` |
| **11** | [Inline Assembly and Intrinsics Intro](./11-inline-assembly-and-intrinsics-intro.md) | Advanced | Planned | GCC extended asm (`asm volatile`), register constraints, clobbers, CPUID |
| **12** | [Object Files and the Linker In-Depth](./12-object-files-and-the-linker-in-depth.md) | Advanced | Planned | Relocations, symbol resolution, weak symbols, link-time optimization (LTO) |

---

## Suggested Study Order
1. **Memory Foundations**: Start with [Custom Allocators & Arenas](./01-custom-allocators-and-arenas.md) and [Memory Pools](./02-memory-pools.md) to revolutionize how you design system lifecycles.
2. **Defensive Systems**: Master [Memory Debugging](./03-memory-debugging-valgrind-asan.md) with AddressSanitizer and Valgrind.
3. **Binary Formats & ABI**: Study [Linking](./04-linking-static-vs-shared-libraries.md), [ABI & Calling Conventions](./05-abi-and-calling-conventions.md), and [Object Files & Linkers](./12-object-files-and-the-linker-in-depth.md).
4. **Wire Formats & Interoperability**: Learn [Endianness & Serialization](./06-endianness-and-serialization.md) and [Interfacing with Assembly](./07-interfacing-with-assembly.md).
5. **Production Architecture**: Conclude with [Writing a Small Library](./08-writing-a-small-library.md) and [Dynamic Loading with dlopen](./09-dynamic-loading-dlopen.md).

---

[← Previous Sub-Module: Data Structures](../03-intermediate/data-structures/README.md) | [C Language Track Index](../README.md) | [Next Module: Systems Programming](../05-systems-programming/README.md)
