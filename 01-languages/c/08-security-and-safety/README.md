# Module 08: Security & Safety in C

> Master low-level vulnerability analysis, binary exploit mitigations, secure coding standards (CERT C, MISRA C), and defensive systems engineering: buffer overflows (stack, heap, global), format string exploits, integer overflows/truncation, use-after-free/double-free, compiler hardening flags (`-fstack-protector`, `_FORTIFY_SOURCE`, RELRO, PIE), filesystem TOCTOU race conditions, bounds-checked strings, cryptography pitfalls (timing leaks, secret zeroing), and modern runtime exploit mitigations (ASLR, DEP/NX, CFI, CET).

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `08-security-and-safety` |
| **Prerequisites** | [02-core-concepts/03-pointers.md](../02-core-concepts/03-pointers.md), [02-core-concepts/08-memory-management.md](../02-core-concepts/08-memory-management.md), [04-advanced/10-symbol-visibility-and-elf-basics.md](../04-advanced/10-symbol-visibility-and-elf-basics.md) |
| **Duration** | ~30 hours |
| **Status** | Complete (Batch 21 Part 2) |
| **Standard Baseline**| C99 / C11 / C17 / C23 / CERT C / MISRA C:2012 / POSIX.1-2008 |

---

## Overview & Learning Outcomes

C provides developers with direct, unmediated access to physical memory addresses, raw pointers, and operating system system calls. However, because the language runtime enforces zero automatic bounds checking, no automatic memory reclamation, and no runtime type safety, minor programming oversights frequently manifest as severe remote code execution (RCE) vulnerabilities, privilege escalation exploits, or catastrophic safety failures.

In this module, you will master:
1. **Memory Corruption Mechanics**: The exact assembly and stack-frame mechanics behind stack buffer overflows, return-oriented programming (ROP), heap metadata corruption, off-by-one errors, and global buffer overruns.
2. **Format String & Variable Argument Exploits**: How `%n` and positional specifiers (`%k$x`) enable arbitrary memory reads and writes, and how to configure static analysis to eliminate format vulnerabilities.
3. **Integer Arithmetic Pitfalls**: Signed integer undefined behavior vs unsigned modulo arithmetic, integer promotion rules, truncation during narrowing conversions, and checked arithmetic intrinsics.
4. **Dynamic Lifecycle Safety**: Use-After-Free (UAF), double-free attacks, allocator bin manipulation, dangling pointer eradication, and handle-based reference tracking.
5. **Industry Standards Compliance**: Enforcing the **SEI CERT C Secure Coding Standard** for general enterprise systems and **MISRA C:2012** for safety-critical automotive (ISO 26262) and medical software.
6. **Compiler Hardening & Binary Defenses**: Deploying compiler flags (`-fstack-protector-strong`, `-D_FORTIFY_SOURCE=3`, Full RELRO, PIE/PIC) and hardware exploit mitigations (ASLR, Non-Executable Stacks NX/DEP, Control Flow Integrity CFI, Intel CET shadow stacks).
7. **Concurrency & Cryptography Pitfalls**: Preventing Time-of-Check to Time-of-Use (TOCTOU) file race conditions with POSIX `openat()`, preventing side-channel timing leaks with constant-time memory comparisons, and zeroing sensitive secrets with `explicit_bzero()`.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [Buffer Overflows: Stack, Heap & Global](./01-buffer-overflows.md) | Advanced | Complete | Stack frame layout, return address overwrite, heap chunks, bounds defense |
| **02** | [Format String Vulnerabilities](./02-format-string-vulnerabilities.md) | Advanced | Complete | `%n` specifier, arbitrary memory read/write, compiler warning `-Wformat-security` |
| **03** | [Integer Overflows, Underflows & Truncation](./03-integer-overflow-and-wraparound.md) | Advanced | Complete | Signed vs unsigned, promotion rules, checked arithmetic `__builtin_add_overflow` |
| **04** | [Use-After-Free and Double-Free](./04-use-after-free-and-double-free.md) | Advanced | Complete | Heap allocator internals, dangling pointers, pointer nullification idioms |
| **05** | [CERT C Coding Standard](./05-secure-coding-cert-c.md) | Advanced | Complete | Key rules (MEM, INT, STR, ENV), automated compliance with static analysis |
| **06** | [MISRA C Overview](./06-misra-c-overview.md) | Advanced | Complete | Embedded safety, mandatory vs required rules, deviation tracking, ISO 26262 |
| **07** | [Compiler Hardening Flags](./07-hardening-flags.md) | Advanced | Complete | Stack canaries, `_FORTIFY_SOURCE`, Full RELRO, PIE, ASLR integration |
| **08** | [Race Conditions and TOCTOU](./08-race-conditions-and-toctou.md) | Advanced | Complete | Time-of-check to time-of-use, symlink attacks, POSIX `openat()` and `O_NOFOLLOW` |
| **09** | [Safe String Handling](./09-safe-string-handling-patterns.md) | Intermediate | Complete | `strlcpy`/`strlcat`, C11 Annex K (`strcpy_s`), dynamic bounded string libraries |
| **10** | [Cryptography Pitfalls in C](./10-secure-random-and-crypto-pitfalls.md) | Advanced | Complete | Side-channel timing attacks, `explicit_bzero`, CSPRNG `/dev/urandom` vs `rand()` |
| **11** | [Modern Exploit Mitigations](./11-exploit-mitigations-explained.md) | Advanced | Complete | ASLR, DEP/NX, Control Flow Integrity (CFI), Intel CET shadow stacks |

---

## Suggested Study Order
1. **Core Vulnerability Mechanics**: Master [Buffer Overflows](./01-buffer-overflows.md), [Format Strings](./02-format-string-vulnerabilities.md), and [Integer Pitfalls](./03-integer-overflow-and-wraparound.md).
2. **Heap & Dynamic Safety**: Study [Use-After-Free and Double-Free](./04-use-after-free-and-double-free.md) and [Safe String Handling](./09-safe-string-handling-patterns.md).
3. **Formal Standards**: Learn [CERT C](./05-secure-coding-cert-c.md) for enterprise servers and [MISRA C](./06-misra-c-overview.md) for safety-critical systems.
4. **Platform & Hardware Hardening**: Apply [Compiler Hardening Flags](./07-hardening-flags.md), [TOCTOU Race Prevention](./08-race-conditions-and-toctou.md), [Cryptography Pitfalls](./10-secure-random-and-crypto-pitfalls.md), and [Modern Exploit Mitigations](./11-exploit-mitigations-explained.md).

---

[← Previous Module: Testing](../07-testing/README.md) | [C Language Track Index](../README.md) | [Next Module: Performance & Optimization](../09-performance/README.md)
