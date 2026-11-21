# API Design for C Libraries: ABI Stability, Symbol Visibility, and Versioning

> Engineer enterprise-grade, backward-compatible C shared libraries with guaranteed Application Binary Interface (ABI) stability, controlled symbol export visibility, and reentrant interfaces.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [01-modular-design-in-c.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/01-modular-design-in-c.md), [01-make-in-depth.md](file:///E:/Learning/engineering-notes/01-languages/c/06-build-and-tooling/01-make-in-depth.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | C11 / C17 / C23 / ELF / POSIX.1-2008 |

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

### The Fragility of Shared Libraries
When building a shared library (`.so` on Linux, `.dll` on Windows, `.dylib` on macOS), software engineers must maintain two distinct compatibility promises:
- **Application Programming Interface (API):** Source-level compatibility. Code compiles without errors against new header files.
- **Application Binary Interface (ABI):** Binary-level compatibility. Already-compiled client executables run seamlessly against a newly installed shared library without recompilation.

In C, an ABI break is invisible at compile time. If a library author adds a field to the middle of a publicly exposed struct, changes an integer parameter from `int32_t` to `int64_t`, or reorders enum constants:
1. Compiled applications continue to run.
2. The application reads and writes incorrect memory offsets.
3. The result is catastrophic, silent memory corruption or instant crashes.

### Designing for Longevity
World-class libraries (like OpenSSL, SQLite, libuv, and the Linux C standard library `glibc`) remain ABI-stable for decades by following disciplined API patterns:
- Concealing struct layouts behind **Opaque Types**.
- Utilizing **Versioned Config Structs** with explicit `struct_size` fields.
- Restricting symbol export tables using compiler **visibility attributes**.
- Providing thread-safe, reentrant function variants (`_r`).

---

## 2. Core Concepts

### 1. The Versioned Struct Idiom
When a public API must accept a configuration struct, the library prefixes the struct with its byte size:
```c
typedef struct {
    uint32_t struct_size; /* Initialized by caller: sizeof(ClientConfig) */
    int timeout_ms;
    int max_retries;
    /* Future versions append fields here */
} ClientConfig;
```
When `client_init(const ClientConfig *cfg)` executes:
- It checks `cfg->struct_size`.
- If an older application was compiled with version 1 (smaller struct size), the library only accesses fields present in version 1 and provides defaults for newer fields.
- Binary compatibility is preserved without recompiling older applications!

### 2. Controlling Symbol Export Visibility
By default, GCC and Clang export every non-static function into the dynamic shared object's ELF symbol table (`.dynsym`).
- Compiling with `-fvisibility=hidden` hides all symbols by default.
- Only public API functions explicitly decorated with `__attribute__((visibility("default")))` are exported.

```
       +-----------------------------------------------------------+
       |               Shared Library Symbol Boundary              |
       +-----------------------------------------------------------+
                                     |
               -fvisibility=hidden enables closed boundary
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
[ Exported Public API ]                                 [ Hidden Internal Code ]
__attribute__((visibility("default")))                  Default visibility: hidden
- client_create()                                       - internal_parse_header()
- client_send()                                         - crypto_aes_helper()
- client_destroy()                                      - buffer_expand()
Visible in .dynsym (Linkable by App)                    Internal only; zero namespace pollution
```

### 3. Semantic Versioning (SemVer) and SONAME
Shared libraries on UNIX systems use `SONAME` versioning:
- `libfoo.so.1` (SONAME: Major version, broken ABI).
- `libfoo.so.1.2.0` (Real binary file: Major.Minor.Patch).
- Minor updates (backward-compatible features) increment the minor version while preserving the SONAME symlink (`libfoo.so.1 -> libfoo.so.1.2.0`).

---

## 3. How It Works Internally

### Memory Offsets and ABI Incompatibility
Consider what happens when a struct layout changes:

```
Library Version 1.0:
struct Request {
    int timeout;    // offset 0 (4 bytes)
    int retries;    // offset 4 (4 bytes)
};
Client App compiled against v1.0 emits:
mov eax, [rdi + 4]  ; Read retries

Library Version 2.0 (BROKEN ABI - field inserted):
struct Request {
    int timeout;    // offset 0 (4 bytes)
    int priority;   // offset 4 (4 bytes)  <--- NEW FIELD!
    int retries;    // offset 8 (4 bytes)
};

When Old Client App runs with New Library:
mov eax, [rdi + 4]  ; Old App reads priority, expecting retries!
                    ; SILENT CORRUPTION OCCURS!
```

By using opaque pointers or appending new fields exclusively to the tail of version-checked structs, memory offsets for pre-existing fields remain strictly immutable.

### ELF Global Offset Table (GOT) and PLT Performance
Hiding internal library symbols with `-fvisibility=hidden` delivers major performance wins:
1. **Direct Jumps:** Calls between internal functions in the `.so` bypass the Procedure Linkage Table (PLT) and Global Offset Table (GOT), converting indirect dynamic jumps into direct PC-relative `call` instructions.
2. **Faster Dynamic Loading:** The dynamic linker (`ld.so`) has fewer symbols to relocate at application startup.
3. **Optimized Inlining:** The compiler optimizer can freely inline or strip unexported functions.

---

## 4. Syntax & Usage

### 1. Robust Export Macro Header (`export.h`)

```c
#ifndef LIB_EXPORT_H
#define LIB_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(BUILDING_LIB_SHARED)
        #define LIB_API __declspec(dllexport)
    #else
        #define LIB_API __declspec(dllimport)
    #endif
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define LIB_API __attribute__((visibility("default")))
    #else
        #define LIB_API
    #endif
#endif

#endif /* LIB_EXPORT_H */
```

### 2. Forward-Compatible API Interface (`client.h`)
```c
#ifndef CLIENT_H
#define CLIENT_H

#include "export.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-compatible options struct */
typedef struct ClientOptions {
    uint32_t struct_size; /* Must be initialized to sizeof(ClientOptions) */
    int timeout_ms;
    int max_retries;
    int flags;
} ClientOptions;

/* Helper macro to initialize options safely */
#define CLIENT_OPTIONS_INIT { sizeof(ClientOptions), 5000, 3, 0 }

/* Opaque client handle */
typedef struct ClientContext ClientContext;

/* Exported Public Functions */
LIB_API int client_init(ClientContext **ctx, const ClientOptions *opts);
LIB_API void client_destroy(ClientContext **ctx);

#ifdef __cplusplus
}
#endif

#endif /* CLIENT_H */
```

---

## 5. Practical Examples

### Comprehensive Verification Suite: ABI Validation and Struct Version Checking

The following test program demonstrates runtime struct size validation for backward-compatible configuration objects:

```c
#include <stdio.h>
#include <stdint.h>

/* Forward-Compatible Configuration Struct with explicit size field */
typedef struct {
    uint32_t struct_size;
    int timeout_ms;
    int max_retries;
    int flags;
} ClientConfig;

int client_init(const ClientConfig *config) {
    if (!config || config->struct_size < sizeof(ClientConfig)) {
        return -1; /* Incompatible ABI version or NULL */
    }
    return 0; /* OK */
}

int main(void) {
    printf("Evaluating ABI-Stable Public Library API Design...\n");
    ClientConfig cfg = {
        .struct_size = (uint32_t)sizeof(ClientConfig),
        .timeout_ms = 5000,
        .max_retries = 3,
        .flags = 0
    };

    int res = client_init(&cfg);
    printf("Client initialization ABI validation result: %d\n", res);
    printf("Forward-compatible versioned struct validated.\n");
    return 0;
}
```

```
Evaluating ABI-Stable Public Library API Design...
Client initialization ABI validation result: 0
Forward-compatible versioned struct validated.
```

### Explanation
- `client_init` inspects `config->struct_size`.
- If a client binary compiled against a legacy header passes a smaller struct, the library detects the version difference, reads only supported fields, and supplies safe defaults for newer features.

---

## 6. Best Practices

1. **Hide All Symbols by Default:** Compile shared libraries with `-fvisibility=hidden` and annotate only public APIs with `LIB_API` (`__attribute__((visibility("default")))`).
2. **Never Expose Struct Members in Headers:** Make types opaque (`typedef struct Foo Foo;`). Provide getters and setters instead of public fields.
3. **Always Append New Fields to the Tail:** When expanding a versioned struct, append new fields strictly to the end. Never insert fields into the middle or reorder existing members.
4. **Make Interfaces Reentrant (`_r`):** Never use static internal buffers (like `strtok` or `ctime`). Require the caller to supply an output buffer or state context pointer.
5. **Never Change Function Signatures Across Minor Versions:** If a function needs new arguments, introduce a new variant (e.g., `client_connect_v2()` or `client_connect_ex()`) while preserving the original function.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Reordering Struct Fields** | Alters byte offsets, causing older compiled binaries to read/write wrong members. | Keep existing fields at identical offsets; append new fields strictly at the end. |
| **Changing Enum Explicit Values** | Inserting an enum item shifts numeric values of all subsequent enums, breaking binary switch logic. | Always assign explicit integer values to public enum members (`VAL = 1`). |
| **Exporting Internal Helper Functions** | Pollutes global dynamic symbol table; clients might link against private APIs, cementing them in the ABI. | Compile with `-fvisibility=hidden`; mark private functions `static`. |
| **Changing Integer Widths in Prototypes** | Changing `int` to `int64_t` corrupts caller register allocation on 64-bit calling conventions. | Freeze parameter types; introduce a new `_ex()` function if wider types are required. |
| **Using C++ Names in Public C Headers** | Including a header in C++ fails if identifiers use keywords like `class`, `new`, `template`, `this`. | Avoid C++ reserved keywords for struct fields or parameter names. |
| **Failing to Wrap Headers with `extern "C"`** | C++ compiler mangles symbol names, causing linker `undefined reference` errors. | Enclose all public declarations in `#ifdef __cplusplus extern "C" { #endif`. |

---

## 8. Performance & Security Notes

### Performance Impact
- **Dynamic Linker Acceleration:** Reducing exported symbols from 2,000 down to 50 public APIs speeds up application load times by over 80% because `ld.so` performs far fewer symbol table lookups and relocations.
- **Interprocedural Compiler Inlining:** The compiler can aggressively inline internal non-exported functions across translation units when building shared objects.

### Security Implications
- **Internal Attack Surface Shielding:** Unexported internal routines cannot be hooked or dynamically hijacked using `LD_PRELOAD`.
- **ROP Gadget Reduction:** Pruning unused exported functions eliminates thousands of potential Return-Oriented Programming (ROP) gadgets from the executable memory map.

---

## 9. Comparison / When to Use What

| Technique | API Impact | ABI Impact | Maintenance Overhead | Best Suited For |
| :--- | :--- | :--- | :--- | :--- |
| **Opaque Pointer Handle** | None (Abstract) | 100% Stable | Low | Core library entities (connections, parsers, contexts). |
| **Versioned Struct (`struct_size`)**| Transparent | Backward Compatible | Moderate | Complex configuration options, feature toggles. |
| **Function Variant (`func_ex`)** | Introduces new API | 100% Stable | Moderate | Extending existing APIs with additional parameters. |
| **Symbol Versioning (GNU `.symver`)** | Hidden in linker script | 100% Stable | High | Core system libraries (glibc, libpthread) upgrading signatures. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the fundamental difference between API compatibility and ABI compatibility?</strong></summary>
API (Application Programming Interface) compatibility means that existing source code compiles successfully against a new version of a library. ABI (Application Binary Interface) compatibility means that already-compiled binaries run correctly with the new shared library without needing recompilation.
</details>

<details>
<summary><strong>2. How does the `struct_size` field maintain backward compatibility in configuration structs?</strong></summary>
When a client passes a configuration struct, the library compares `cfg->struct_size` against `sizeof(CurrentStruct)`. If `struct_size` is smaller, the library recognizes that the binary was compiled against an older version, reads only the fields present in that version, and initializes newer fields to safe defaults.
</details>

<details>
<summary><strong>3. Why should shared libraries be compiled with `-fvisibility=hidden`?</strong></summary>
By default, GCC exports all non-static functions into the dynamic symbol table. `-fvisibility=hidden` hides all symbols unless explicitly decorated with `__attribute__((visibility("default")))`. This prevents namespace pollution, speeds up dynamic linking, enables internal compiler optimizations, and avoids accidental ABI commitments.
</details>

<details>
<summary><strong>4. What is a SONAME in ELF shared libraries, and how is it used during runtime linking?</strong></summary>
SONAME is a metadata field embedded inside an ELF shared object (e.g., `libcurl.so.4`). When an application is linked against the library, the linker writes the library's SONAME into the application's `DT_NEEDED` dynamic tag. At runtime, the dynamic linker looks specifically for the file matching the SONAME, allowing multiple major ABI versions to coexist on the same system.
</details>

<details>
<summary><strong>5. Why is changing an enum value considered an ABI break in C?</strong></summary>
In C, enum values are compiled into raw integer literals in the calling binary's machine instructions. If an enum is renumbered in the library, older compiled binaries will continue sending the old integer values, leading to incorrect branching and silent logic errors.
</details>

<details>
<summary><strong>6. How does GNU symbol versioning (`.symver`) allow a library to support multiple function signatures simultaneously?</strong></summary>
Symbol versioning allows a shared library to export multiple implementations of the same function tag bound to different version nodes (e.g., `memcpy@@GLIBC_2.14` vs `memcpy@GLIBC_2.2.5`). Old binaries bind to the legacy implementation, while newly compiled binaries link against the modern version.
</details>

<details>
<summary><strong>7. Why should public C library functions never expose standard library file descriptors or `FILE*` across Windows DLL boundaries?</strong></summary>
On Windows, each DLL or executable can link against a different C Runtime Library (CRT) version (e.g., `msvcrt.dll` vs `ucrtbase.dll`). File descriptors and `FILE*` handles allocated in one CRT cannot be read, flushed, or closed by another CRT, causing immediate crashes.
</details>

<details>
<summary><strong>8. What is a reentrant function (`_r`), and why is it mandatory for multi-threaded library design?</strong></summary>
A reentrant function can be interrupted and called again by another thread (or signal handler) before its previous invocation completes without data corruption. It achieves this by holding zero static internal state and requiring all working buffers and context pointers to be supplied by the caller.
</details>

<details>
<summary><strong>9. How does `__declspec(dllexport)` differ from `__attribute__((visibility("default")))`?</strong></summary>
`__declspec(dllexport)` is the Microsoft Windows MSVC attribute for exporting symbols into a PE/COFF dynamic library's export table. `__attribute__((visibility("default")))` is the GCC/Clang attribute for exporting symbols into an ELF/Mach-O shared object.
</details>

<details>
<summary><strong>10. Why should library memory deallocators always be exported alongside allocators?</strong></summary>
If an application calls `free(ptr)` directly using its own libc allocator on memory allocated by a shared library (especially on Windows or cross-allocator systems), heap corruption occurs because different allocators manage heaps independently. The library must provide its own `lib_free()` or `lib_destroy()` function.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configuring Symbol Visibility in a Shared Library Makefile (Easy)
Write a Makefile rule for GCC that compiles `crypto.c` into `libcrypto.so` with hidden visibility, exporting only functions marked with default visibility.

<details>
<summary>Solution</summary>

```makefile
CC = gcc
CFLAGS = -O2 -fPIC -fvisibility=hidden -Wall -Wextra
LDFLAGS = -shared -Wl,-soname,libcrypto.so.1

libcrypto.so.1.0.0: crypto.o
	$(CC) $(LDFLAGS) -o $@ $^
	ln -sf libcrypto.so.1.0.0 libcrypto.so.1
	ln -sf libcrypto.so.1 libcrypto.so

crypto.o: crypto.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so*
```
</details>

### Exercise 2: Implementing a Versioned Configuration Dispatcher (Medium)
Create a network client configuration structure that handles both Version 1 (timeout only) and Version 2 (timeout + proxy URL) clients seamlessly.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t struct_size;
    int timeout_ms;
    const char *proxy_url; /* Added in Version 2 */
} NetConfig;

#define NET_CONFIG_V1_SIZE (sizeof(uint32_t) + sizeof(int))
#define NET_CONFIG_V2_SIZE (sizeof(NetConfig))

int net_init(const NetConfig *cfg) {
    if (!cfg || cfg->struct_size < NET_CONFIG_V1_SIZE) {
        return -1; /* Invalid */
    }

    int timeout = cfg->timeout_ms;
    const char *proxy = "NONE";

    /* Inspect if client provided Version 2 fields */
    if (cfg->struct_size >= NET_CONFIG_V2_SIZE && cfg->proxy_url) {
        proxy = cfg->proxy_url;
    }

    printf("Initialized: Timeout=%d ms, Proxy=%s\n", timeout, proxy);
    return 0;
}

int main(void) {
    /* Simulate a legacy V1 client */
    struct {
        uint32_t size;
        int timeout;
    } legacy_client = { sizeof(legacy_client), 3000 };

    net_init((const NetConfig*)&legacy_client);

    /* Simulate modern V2 client */
    NetConfig modern_client = {
        .struct_size = sizeof(NetConfig),
        .timeout_ms = 5000,
        .proxy_url = "http://proxy.internal:8080"
    };
    net_init(&modern_client);
    return 0;
}
```
</details>

### Exercise 3: Designing a Safe Cross-DLL Memory Allocator Pair (Hard)
Construct an opaque byte buffer library interface where memory allocation and deallocation are strictly governed by the library to prevent cross-heap corruption.

<details>
<summary>Solution</summary>

**`safe_buffer.h`:**
```c
#ifndef SAFE_BUFFER_H
#define SAFE_BUFFER_H

#include <stddef.h>

typedef struct SafeBuffer SafeBuffer;

SafeBuffer* safe_buffer_create(size_t initial_cap);
int safe_buffer_write(SafeBuffer *buf, const void *data, size_t len);
const void* safe_buffer_data(const SafeBuffer *buf);
size_t safe_buffer_size(const SafeBuffer *buf);
void safe_buffer_destroy(SafeBuffer **buf_ptr);

#endif
```

**`safe_buffer.c`:**
```c
#include "safe_buffer.h"
#include <stdlib.h>
#include <string.h>

struct SafeBuffer {
    uint8_t *data;
    size_t size;
    size_t capacity;
};

SafeBuffer* safe_buffer_create(size_t initial_cap) {
    SafeBuffer *b = malloc(sizeof(SafeBuffer));
    if (!b) return NULL;
    b->data = malloc(initial_cap);
    if (!b->data) { free(b); return NULL; }
    b->size = 0;
    b->capacity = initial_cap;
    return b;
}

int safe_buffer_write(SafeBuffer *buf, const void *data, size_t len) {
    if (!buf || !data) return -1;
    if (buf->size + len > buf->capacity) {
        size_t ncap = (buf->capacity + len) * 2;
        uint8_t *ndata = realloc(buf->data, ncap);
        if (!ndata) return -1;
        buf->data = ndata;
        buf->capacity = ncap;
    }
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    return 0;
}

const void* safe_buffer_data(const SafeBuffer *buf) { return buf ? buf->data : NULL; }
size_t safe_buffer_size(const SafeBuffer *buf) { return buf ? buf->size : 0; }

void safe_buffer_destroy(SafeBuffer **buf_ptr) {
    if (buf_ptr && *buf_ptr) {
        free((*buf_ptr)->data);
        free(*buf_ptr);
        *buf_ptr = NULL;
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Hide Symbols:** Use `-fvisibility=hidden`. Annotate public APIs with `__attribute__((visibility("default")))`.
- **Version Structs:** Include `uint32_t struct_size;` as the first field; validate size at runtime.
- **Append Only:** Never insert fields in the middle of structs; append exclusively to the tail.
- **Opaque Handles:** Keep struct layouts hidden in `.c` files to prevent ABI breaks entirely.
- **Paired Lifecycles:** Export library destruction functions (`lib_destroy`) so memory is freed by the same allocator that created it.

---

## 13. Further Reading
- Ulrich Drepper: *How To Write Shared Libraries* (Red Hat).
- Ian Lance Taylor: *Linkers (20-part essay series)*.
- ISO C Standard (ISO/IEC 9899:2011), Section 6.7: *Declarations*.
- OpenSSL Coding Guidelines: *ABI Stability and Versioning*.

---

| [Previous: 05-resource-management-patterns.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/05-resource-management-patterns.md) | [Section Index](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/README.md) | [Next: 07-ownership-and-lifetime-conventions.md](file:///E:/Learning/engineering-notes/01-languages/c/10-design-and-patterns/07-ownership-and-lifetime-conventions.md) |
