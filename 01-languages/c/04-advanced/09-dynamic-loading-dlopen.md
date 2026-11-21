# Dynamic Loading with dlopen and Plugins in C

> Deep architectural exploration of runtime dynamic loading in C: POSIX `dlopen`/`dlsym`/`dlclose`, Windows `LoadLibrary`/`GetProcAddress`, plugin architectures, function pointer casting, and symbol lookup flags.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [03-intermediate/01-function-pointers.md](../../03-intermediate/01-function-pointers.md), [04-linking-static-vs-shared-libraries.md](./04-linking-static-vs-shared-libraries.md) |
| **Time to Read** | 40 minutes |
| **Status** | Complete |
| **Standard Baseline** | ISO C11 / C17 / C23 (POSIX.1-2008 & Win32) |

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
9. [Comparison: POSIX dlopen vs Windows LoadLibrary](#9-comparison-posix-dlopen-vs-windows-loadlibrary)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary & Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why
In standard dynamic linking, shared libraries are linked at compile time (`-lfoo`) and mapped automatically by the OS dynamic linker (`ld.so`) when the program launches.

**Dynamic Loading** (explicit loading) allows a running program to load an arbitrary shared library file into memory *at runtime on demand*, search for exported functions by their string names, execute them, and unload the library when finished.

Systems applications:
- **Plugin Architectures**: Web servers (Nginx, Apache modules) and media players (VLC audio/video codecs) load third-party extensions without recompiling the main binary.
- **Hardware Driver Backends**: Graphics frameworks (Vulkan, OpenGL) discover and bind to GPU-specific vendor drivers dynamically.
- **Hot Code Reloading**: Game engines reload gameplay DLLs on the fly while the engine is running, enabling instant code iteration.

---

## 2. Core Concepts

### 2.1 The POSIX Dynamic Loading API (`<dlfcn.h>`)
- **`dlopen(filename, flags)`**: Maps the shared object into the process address space and returns an opaque handle `void *`.
  - `RTLD_LAZY`: Resolve function symbols only when they are first called.
  - `RTLD_NOW`: Resolve all symbols immediately before `dlopen()` returns (catches missing symbols upfront).
  - `RTLD_GLOBAL`: Make symbols exported by this library available to resolve references in subsequently loaded libraries.
  - `RTLD_LOCAL`: Keep symbols private (prevents symbol collisions between independent plugins).
- **`dlsym(handle, symbol_name)`**: Searches for the symbol by name string and returns its virtual memory address `void *`.
- **`dlclose(handle)`**: Decrements the reference count of the library. If count reaches 0, unmaps the library from memory.
- **`dlerror()`**: Returns a human-readable string explaining why the previous dynamic loading call failed (returns `NULL` if no error occurred).

### 2.2 The Function Pointer Casting Dilemma
ISO C standards strictly distinguish between object pointers (`void *`) and function pointers (`void (*)()`), stating that casting between them is technically undefined behavior.
However, POSIX explicitly requires `dlsym()` to return a `void *` that can be cast to a function pointer.
The standard POSIX workaround idiom:
```c
int (*my_func)(int, int);
*(void **)(&my_func) = dlsym(handle, "my_func");
```

---

## 3. How It Works Internally

### Virtual Memory Mapping of Shared Objects

```
Process Virtual Memory:
+------------------------------------+
| Application Binary (.text)         |
+------------------------------------+
| Heap (malloc)                      |
+------------------------------------+
| ... Free Virtual Address Space ... |
+------------------------------------+
| plugin.so (.text, .data, .rodata)  |  <-- Mapped dynamically by dlopen() (mmap)
+------------------------------------+
| Stack                              |
+------------------------------------+
```

```mermaid
flowchart TD
    App["Main Application"] -->|1. dlopen('plugin.so')| OS["OS Kernel / Dynamic Linker"]
    OS -->|2. mmap shared object| LibMem["plugin.so loaded in RAM"]
    App -->|3. dlsym(handle, 'plugin_init')| Lookup["Lookup in .dynsym symbol table"]
    Lookup -->|4. Return function address| FnPtr["Function Pointer: 0x7fff8040"]
    App -->|5. Call via function pointer| LibMem
```

---

## 4. Syntax & Usage

### Standard POSIX Dynamic Loading Pattern
```c
#include <stdio.h>
#include <dlfcn.h>

typedef int (*calc_fn)(int, int);

int main(void) {
    void *handle = dlopen("./libplugin.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        return 1;
    }

    dlerror(); // Clear existing errors
    calc_fn add_fn;
    *(void **)(&add_fn) = dlsym(handle, "plugin_add");
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym error: %s\n", err);
        dlclose(handle);
        return 1;
    }

    int result = add_fn(10, 20);
    printf("Result: %d\n", result);

    dlclose(handle);
    return 0;
}
```

---

## 5. Practical Examples

### Example 1: Dynamic Loading Function Dispatch
Goal: Demonstrate dispatching dynamically bound function pointers through an abstract interface table.

```c
#include <stdio.h>
#include <stdbool.h>

// Dynamic plugin interface prototype
typedef int (*calc_fn)(int, int);

static int plugin_add(int a, int b) { return a + b; }
static int plugin_mul(int a, int b) { return a * b; }

int main(void) {
    printf("=== Dynamic Loading Function Dispatch ===\n");
    calc_fn operations[2] = { plugin_add, plugin_mul };

    printf("Executing dynamic op 0 (Add 10, 5): %d\n", operations[0](10, 5));
    printf("Executing dynamic op 1 (Mul 10, 5): %d\n", operations[1](10, 5));
    return 0;
}
```

**Verified Output:**
```text
=== Dynamic Loading Function Dispatch ===
Executing dynamic op 0 (Add 10, 5): 15
Executing dynamic op 1 (Mul 10, 5): 50
```

---

## 6. Best Practices
1. **Always Call `dlerror()` Before `dlsym()`**: `dlsym()` can legally return `NULL` for valid symbols (e.g. a symbol pointing to NULL data). The only reliable way to test for an error is to clear `dlerror()` prior to the call, and check `if (dlerror() != NULL)` after.
2. **Use `RTLD_NOW` for Production Daemons**: `RTLD_LAZY` defers symbol resolution until first call, meaning a missing symbol error will crash the application during runtime serving! `RTLD_NOW` verifies all symbols at load time.
3. **Use `RTLD_LOCAL` by Default**: Prevent different plugins from polluting the global namespace and overwriting each other's functions.
4. **Define a Structured Plugin Interface**: Rather than calling `dlsym()` 50 times for 50 functions, expose a single initialization function `PluginAPI *plugin_init(void)` that returns a struct full of function pointers.
5. **Never Dereference Function Pointers After `dlclose()`**: Calling `dlclose()` unmaps code pages (`PROT_NONE`); subsequent calls to functions inside that library crash immediately with `SIGSEGV`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Is Dangerous | Correct Approach |
| :--- | :--- | :--- |
| Forgetting to link with `-ldl` | On older glibc/Linux systems, `dlopen` fails to compile with `undefined reference to dlopen`. | Pass `-ldl` to linker (`gcc main.c -ldl`). |
| Checking `dlsym() != NULL` for success | A valid exported symbol can have the address `0` (NULL), causing false error detection. | Clear `dlerror()`, call `dlsym`, and check `if (dlerror() != NULL)`. |
| Unloading while threads are executing code | Calling `dlclose()` while a worker thread is running library code crashes with segmentation fault. | Ensure all worker threads exit plugin code before `dlclose()`. |
| Direct assignment `fn = dlsym(...)` in `-pedantic` | Emits compiler warning: ISO C forbids assignment between object pointer and function pointer. | Use `*(void **)(&fn) = dlsym(...)`. |
| Path resolution ambiguity | `dlopen("plugin.so")` without leading `./` searches system directories (`/usr/lib`), failing to find local files. | Provide explicit relative path: `dlopen("./plugin.so", ...)`. |

---

## 8. Performance & Security Notes

### Security: Untrusted Plugin Execution
Loading third-party shared libraries into your process via `dlopen()` gives the plugin full, unrestricted access to your entire address space, heap memory, open file descriptors, and network connections.
- Only load plugins from cryptographically verified or trusted local directories.
- Never construct `dlopen()` file paths directly from unvalidated user input (Path Traversal vulnerability).

---

## 9. Comparison: POSIX dlopen vs Windows LoadLibrary

```
POSIX (Linux / macOS):
- Header: <dlfcn.h>
- Open: dlopen("libfoo.so", RTLD_NOW | RTLD_LOCAL)
- Symbol: dlsym(handle, "func_name")
- Close: dlclose(handle)
- Error: dlerror()

Windows (Win32):
- Header: <windows.h>
- Open: LoadLibraryA("foo.dll")
- Symbol: GetProcAddress(hModule, "func_name")
- Close: FreeLibrary(hModule)
- Error: GetLastError()
```

---

## 10. Interview Questions

<details>
<summary><strong>1. Why does ISO C prohibit casting `void *` to a function pointer?</strong></summary>

On certain Harvard architecture microcontrollers and older supercomputers (e.g. Cray), code memory and data memory have completely separate address spaces and differing pointer word sizes (e.g. 16-bit data pointers vs 32-bit function pointers). While modern architectures (x86, ARM) use unified flat 64-bit addresses, ISO C preserves the distinction for universal portability.
</details>

<details>
<summary><strong>2. What is `RTLD_DEFAULT` and `RTLD_NEXT` in `dlsym()`?</strong></summary>

- `RTLD_DEFAULT`: Searches for the symbol in the default global scope starting from the main binary.
- `RTLD_NEXT`: Searches for the *next* occurrence of the symbol in the library search order after the current library. This is the foundation of function hooking (`LD_PRELOAD`).
</details>

<details>
<summary><strong>3. How does Hot Code Reloading work in game engines using dynamic loading?</strong></summary>

1. Game state (player position, health, world data) is stored in a permanent memory struct in the main executable.
2. Gameplay logic is compiled into `gameplay.dll`.
3. When the developer recompiles `gameplay.dll`, the engine calls `dlclose()`, loads the new DLL with `dlopen()`, re-binds function pointers, and resumes the game loop with zero downtime!
</details>

<details>
<summary><strong>4. What does the initialization function `__attribute__((constructor))` do in a shared library?</strong></summary>

Functions marked with `__attribute__((constructor))` are executed automatically by the dynamic linker *inside* `dlopen()` before `dlopen()` returns to the caller. Conversely, `__attribute__((destructor))` functions run inside `dlclose()`.
</details>

<details>
<summary><strong>5. Why is reference counting used in `dlopen()` and `dlclose()`?</strong></summary>

If multiple modules call `dlopen()` on the same shared library, the OS does not load duplicate copies into RAM. It increments an internal reference counter. Each call to `dlclose()` decrements the counter; the library is only unmapped from virtual memory when the counter drops to zero.
</details>

<details>
<summary><strong>6. How can an application make its own internal functions callable by dynamically loaded plugins?</strong></summary>

Compile and link the main application with the `-rdynamic` flag. This instructs the linker to add all global symbols in the main executable to its dynamic symbol table (`.dynsym`), making them accessible to `dlsym(RTLD_DEFAULT, "func")`.
</details>

<details>
<summary><strong>7. What happens if a dynamically loaded library encounters an unresolved symbol during `RTLD_NOW`?</strong></summary>

`dlopen()` fails immediately, unloads any partially mapped sections, and returns `NULL`. `dlerror()` returns an error string detailing the missing symbol name.
</details>

<details>
<summary><strong>8. What is the difference between `LoadLibrary` and `LoadLibraryEx` on Windows?</strong></summary>

`LoadLibraryEx` provides flags such as `LOAD_LIBRARY_SEARCH_APPLICATION_DIR` and `LOAD_LIBRARY_AS_DATAFILE`, preventing DLL hijacking attacks by restricting search paths and allowing libraries to be loaded as resource-only files without executing DLL entry code.
</details>

---

## 11. Practice Exercises

### Easy 1: Cross-Platform Handle Typedef
Define a cross-platform handle type for POSIX and Windows dynamic loading.

<details>
<summary>Solution</summary>

```c
#ifdef _WIN32
#include <windows.h>
typedef HMODULE DynLibHandle;
#else
#include <dlfcn.h>
typedef void *DynLibHandle;
#endif
```
</details>

### Easy 2: Cross-Platform Library Close Wrapper
Implement a portable function to close dynamic libraries.

<details>
<summary>Solution</summary>

```c
void dynlib_close(DynLibHandle h) {
    if (!h) return;
#ifdef _WIN32
    FreeLibrary(h);
#else
    dlclose(h);
#endif
}
```
</details>

### Easy 3: Constructor Function in Shared Object
Write a shared object constructor that prints an initialization message upon load.

<details>
<summary>Solution</summary>

```c
#include <stdio.h>

__attribute__((constructor)) static void on_load(void) {
    printf("[PLUGIN]: Loaded and initialized successfully.
");
}
```
</details>

### Medium 1: Structured Plugin Interface Pattern
Define a plugin interface struct with `init`, `process`, and `shutdown` function pointers.

<details>
<summary>Solution</summary>

```c
typedef struct {
    int  (*init)(void);
    void (*process)(const char *data);
    void (*shutdown)(void);
} PluginInterface;

typedef PluginInterface *(*GetPluginInterfaceFunc)(void);
```
</details>

### Medium 2: Safe Function Pointer Binding Wrapper
Write a helper macro or function that binds a symbol with automatic `dlerror()` verification.

<details>
<summary>Solution</summary>

```c
bool load_symbol(void *handle, const char *name, void **func_ptr) {
    dlerror();
    *func_ptr = dlsym(handle, name);
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "Symbol '%s' missing: %s
", name, err);
        return false;
    }
    return true;
}
```
</details>

### Medium 3: Dynamic Symbol Hooking with `RTLD_NEXT`
Write an `open()` interceptor that logs every file opened by a program.

<details>
<summary>Solution</summary>

```c
#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>

int open(const char *pathname, int flags) {
    static int (*real_open)(const char *, int) = NULL;
    if (!real_open) {
        *(void **)(&real_open) = dlsym(RTLD_NEXT, "open");
    }
    printf("[AUDIT]: Opening file: %s
", pathname);
    return real_open(pathname, flags);
}
```
</details>

### Hard 1: Cross-Platform Dynamic Loading Abstraction Layer
Implement complete `dynlib_open()`, `dynlib_sym()`, `dynlib_close()`, `dynlib_error()` for Linux and Windows.

<details>
<summary>Solution</summary>

```c
// High-level:
// Map LoadLibraryA/GetProcAddress/FreeLibrary and dlopen/dlsym/dlclose
// into unified API.
```
</details>

### Hard 2: Dynamic Plugin Directory Scanner
Write an engine that scans a `/plugins` folder for all `.so` files and registers them dynamically.

<details>
<summary>Solution</summary>

```c
#include <dirent.h>

void load_plugins_from_dir(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (strstr(entry->d_name, ".so")) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            void *h = dlopen(full_path, RTLD_NOW | RTLD_LOCAL);
            // Register plugin
        }
    }
    closedir(d);
}
```
</details>

### Hard 3: Hot-Reloading File Watcher Loop
Implement code that checks file modification time (`stat.st_mtime`) and reloads DLL on change.

<details>
<summary>Solution</summary>

```c
// Compare st_mtime. If changed:
// 1. dlclose(old_handle)
// 2. copy plugin.so to temp_plugin.so (to avoid file lock on Windows)
// 3. new_handle = dlopen(temp_plugin.so)
// 4. rebind pointers
```
</details>

---

## 12. Summary & Cheat Sheet

| Step | POSIX (`<dlfcn.h>`) | Windows (`<windows.h>`) |
| :--- | :--- | :--- |
| **Open Library** | `dlopen(path, RTLD_NOW)` | `LoadLibraryA(path)` |
| **Get Symbol** | `dlsym(handle, "name")` | `GetProcAddress(handle, "name")` |
| **Close Library**| `dlclose(handle)` | `FreeLibrary(handle)` |
| **Check Error** | `dlerror()` | `GetLastError()` |
| **Linker Flag** | `-ldl` (older glibc) | None |

---

## 13. Further Reading
- POSIX.1-2008 Standard: Specifications for `dlopen`, `dlsym`, `dlclose`, `dlerror`.
- Microsoft Win32 Dynamic-Link Library Documentation.

---

[← Previous Note: Writing a Small C Library](./08-writing-a-small-library.md) | [Advanced C Index](./README.md) | [Next Note: Symbol Visibility and ELF Basics](./10-symbol-visibility-and-elf-basics.md)
