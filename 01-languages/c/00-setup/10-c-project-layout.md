# C Project Layout and Modular Architecture

> The definitive engineering architecture for scalable C repositories: separation of public API headers (`include/`) from private implementation headers (`src/`), out-of-tree builds, static/shared library artifact generation, and automated test harnesses.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-toolchain.md](./01-toolchain.md), [07-makefile-basics.md](./07-makefile-basics.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | POSIX / Unix Filesystem Hierarchy Standard (FHS) Conventions |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

Novice C developers frequently place all `.c` and `.h` files into a single flat directory alongside compiler output binaries. While workable for toy programs, this flat structure collapses rapidly as systems expand:
1. **Accidental Symbol Leakage**: When header files containing internal data structures are placed in the same directory as consumer headers, consumers couple to private implementation details, breaking library encapsulation.
2. **Directory Pollution & Build Artifact Clutter**: Interleaving intermediate object files (`.o`), dependency files (`.d`), and executables with source files ruins source control tracking and makes clean rebuilds error-prone.
3. **Circular Dependencies**: Without clear topological hierarchy between subsystems, modules quickly develop cyclic `#include` references that stall compilation.

Adopting the standard POSIX modular C project architecture establishes clear visibility boundaries, separates consumer APIs from implementation internals, and guarantees clean, reproducible out-of-tree builds.

---

## 2. Core Concepts

### The Standard Production Directory Layout

```
my_project/
├── Makefile                # Master root build orchestration
├── CMakeLists.txt          # Modern meta-build definition (optional/alternative)
├── .editorconfig           # Formatting rules (tabs/spaces, line endings)
├── .gitignore              # Ignores build/, bin/, obj/
├── README.md               # Architecture overview and build instructions
├── include/                # PUBLIC headers exposed to external library consumers
│   └── my_project/         # Namespaced subfolder to avoid filename collisions
│       ├── my_project.h    # Umbrella header
│       ├── engine.h        # Public subsystem header
│       └── types.h         # Public types and error codes
├── src/                    # PRIVATE implementation sources and internal headers
│   ├── engine.c            # Subsystem implementation
│   ├── internal_buffer.h   # PRIVATE header (hidden from consumers!)
│   ├── internal_buffer.c   # Private buffer helper
│   └── main.c              # Application entry point (if building executable)
├── tests/                  # Unit and integration test suites
│   ├── test_engine.c       # Subsystem test runner
│   └── unity/              # Minimal embedded test framework (or vendor submodule)
├── docs/                   # Architecture decision records and API documentation
└── build/                  # OUT-OF-TREE build artifacts (wiped on 'make clean')
    ├── obj/                # Compiled object files (.o) and dependency files (.d)
    └── bin/                # Linked target binaries and libraries (.a / .so)
```

### Public vs. Private Visibility Boundaries

```
+-------------------------------------------------------------------------+
|                       VISIBILITY BOUNDARY                               |
+-------------------------------------------------------------------------+
|  CONSUMER APPLICATION                                                   |
|       |                                                                 |
|       | #include <my_project/engine.h>  [PUBLIC API]                    |
|       v                                                                 |
|  +-------------------------------------------------------------------+  |
|  | [include/my_project/] (Exported, installed to /usr/local/include) |  |
|  +-------------------------------------------------------------------+  |
|       |                                                                 |
|       | Implementation linkage                                          |
|       v                                                                 |
|  +-------------------------------------------------------------------+  |
|  | [src/] (Private internals, opaque structs, internal_buffer.h)     |  |
|  +-------------------------------------------------------------------+  |
+-------------------------------------------------------------------------+
```

- **Public Headers (`include/my_project/`)**: Installed into system include paths (e.g. `/usr/local/include/my_project`). They must expose only opaque struct pointers, function prototypes, and public constants. Never `#include` internal headers here.
- **Private Headers (`src/`)**: Contain concrete struct member definitions, internal macros, and private function prototypes. They are never installed or distributed to consumers.

---

## 3. How It Works Internally

### The Preprocessor Search Path Mechanism (`-I` vs `-iquote`)

When the compiler resolves `#include` directives, its search algorithm depends on the delimiters and flags:
- `#include "header.h"`: The preprocessor searches the directory of the *current file* first, then paths specified by `-iquote`, then `-I` paths, then system default directories (`/usr/include`).
- `#include <my_project/header.h>`: The preprocessor bypasses the local directory, searching exclusively along the `-I` include search paths and system directories.

By structuring the include tree as `include/my_project/engine.h` and passing `-Iinclude` to the compiler driver:
1. Consumer source files write `#include <my_project/engine.h>`.
2. This eliminates namespace clashes: if another library also has an `engine.h`, the prefix `my_project/` unambiguously identifies the target.

### Out-of-Tree Object Translation

During compilation, object files are directed into `build/obj/` rather than the source directory:
```makefile
$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
```
This isolates the source tree completely from generated artifacts, making `git clean` or `rm -rf build` a safe, instantaneous 100% clean operation.

---

## 4. Syntax & Usage

### 1. The Opaque Pointer (PIMPL) Encapsulation Pattern

In `include/my_project/engine.h` (Public Header):
```c
#ifndef MY_PROJECT_ENGINE_H
#define MY_PROJECT_ENGINE_H

#include <stddef.h>

/* Opaque struct declaration: Consumer cannot see inside struct engine_state */
typedef struct engine_state engine_state_t;

engine_state_t *engine_create(int id, const char *name);
void engine_destroy(engine_state_t *engine);
int engine_get_id(const engine_state_t *engine);

#endif /* MY_PROJECT_ENGINE_H */
```

In `src/engine.c` (Private Implementation):
```c
#include "my_project/engine.h"
#include <stdlib.h>
#include <string.h>

/* Full struct definition visible ONLY inside the implementation translation unit */
struct engine_state {
    int id;
    char name[64];
    void *internal_cache; /* Private field */
};

engine_state_t *engine_create(int id, const char *name) {
    engine_state_t *state = malloc(sizeof(*state));
    if (!state) return NULL;
    state->id = id;
    strncpy(state->name, name, sizeof(state->name) - 1);
    state->name[sizeof(state->name) - 1] = '\0';
    state->internal_cache = NULL;
    return state;
}

void engine_destroy(engine_state_t *engine) {
    free(engine);
}

int engine_get_id(const engine_state_t *engine) {
    return engine ? engine->id : -1;
}
```

---

## 5. Practical Examples

### Modular Architecture Demonstration Program

```c
/* File: 01-languages/c/00-setup/examples/10-c-project-layout.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Simulated Public Header Interface */
typedef struct subsystem subsystem_t;

subsystem_t *subsystem_init(int id, const char *tag);
void subsystem_print(const subsystem_t *sys);
void subsystem_free(subsystem_t *sys);

/* Simulated Private Implementation */
struct subsystem {
    int subsystem_id;
    char tag_name[32];
};

subsystem_t *subsystem_init(int id, const char *tag) {
    subsystem_t *s = malloc(sizeof(*s));
    if (!s) return NULL;
    s->subsystem_id = id;
    strncpy(s->tag_name, tag, sizeof(s->tag_name) - 1);
    s->tag_name[sizeof(s->tag_name) - 1] = '\0';
    return s;
}

void subsystem_print(const subsystem_t *sys) {
    if (sys) {
        printf("Subsystem [ID: %d, Module: %s] initialized.\n", 
               sys->subsystem_id, sys->tag_name);
    }
}

void subsystem_free(subsystem_t *sys) {
    free(sys);
}

int main(void) {
    printf("=== Modular Project Architecture Demo ===\n");
    subsystem_t *engine = subsystem_init(1, "NetworkingEngine");
    subsystem_print(engine);
    subsystem_free(engine);
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 10-c-project-layout.c -o run_10.exe
./run_10.exe
```
Output:
```text
=== Modular Project Architecture Demo ===
Subsystem [ID: 1, Module: NetworkingEngine] initialized.
```

---

## 6. Best Practices

1. **Namespace Public Headers Subfolder**: Always place public headers under `include/<project_name>/`. Never place loose `.h` files directly into `include/`.
2. **Never Expose Struct Layouts Unless Required for Inlining**: Use opaque struct pointers (`typedef struct foo foo_t;`) in public headers. This maintains strict ABI compatibility: you can add or reorder struct fields in `src/foo.c` without forcing consumers to recompile!
3. **Mirror Source Layout in Test Directory**: If you have `src/net/socket.c`, create `tests/net/test_socket.c`.
4. **Use Out-of-Tree Builds Exclusively**: All `.o`, `.d`, and executable files must go into `build/` or `target/`. Keep `src/` completely clean of compilation artifacts.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Placing private internal headers in `include/` | External consumers will `#include` internal headers, creating brittle dependencies that break during internal refactorings. | Move internal headers to `src/`. |
| Putting function definitions in `.h` files | Including that header in multiple `.c` files causes multiple definition linker errors (`duplicate symbol`). | Only declare prototypes in `.h`; put definitions in `.c`. (Unless marked `static inline`). |
| `#include "../include/foo.h"` relative path walking | Fragile; breaks if source files are moved across directory tiers. | Pass `-Iinclude` in `CPPFLAGS` and write `#include "foo.h"` or `#include <project/foo.h>`. |
| Storing compiled object files in `src/` | Pollutes git status, complicates clean rebuilds, and risks accidental commits of binaries. | Configure build system to output to `build/obj/`. |
| Exposing concrete structs in public APIs | Modifying struct size changes ABI; every consumer binary breaks unless recompiled. | Use opaque pointers (`typedef struct foo foo_t;`). |
| Forgetting to install license and documentation files | Breaks open-source distribution and package manager compliance. | Include root `LICENSE`, `README.md`, and `docs/`. |

---

## 8. Performance & Security Notes

- **Static vs. Shared Libraries**:
  - **Static Library (`.a`)**: Bundles compiled object files into an archive (`ar rcs libengine.a $(OBJS)`). The linker copies only referenced functions directly into the final executable. Fast, zero runtime dependency, but increases binary size.
  - **Shared Library (`.so` / `.dll`)**: Compiled with Position Independent Code (`-fPIC`). Loaded into memory once by the OS dynamic linker and shared across multiple processes. Reduces disk and RAM usage, but incurs minor dynamic resolution overhead.
- **Symbol Visibility Hardening (`-fvisibility=hidden`)**: By default, all global functions in a `.so` are exported to the dynamic symbol table. Pass `-fvisibility=hidden` and use `__attribute__((visibility("default")))` explicitly on public API functions. This prevents reverse engineering of internal symbols and speeds up dynamic linking.

---

## 9. Comparison / When to Use What

| Architecture Tier | Executable Project | Static Library (`.a`) | Shared Library (`.so`) |
|---|---|---|---|
| **Entry Point** | Contains `main()` | No `main()` | No `main()` |
| **Output Directory** | `build/bin/my_app` | `build/lib/libmy_app.a` | `build/lib/libmy_app.so` |
| **Compiler Flags** | Standard flags | `-O2` | `-fPIC -O2` |
| **Header Exposure** | Private only (in `src/`) | Public headers in `include/` | Public headers in `include/` |
| **Consumer Linkage**| N/A (Standalone) | `gcc app.c -Llib -lmy_app` | Runtime loader (`LD_LIBRARY_PATH`) |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why is the `include/<project_name>/` directory layout considered best practice over a flat `include/` directory?</strong></summary>

It prevents header name collisions. If a library has a generic header like `types.h` or `utils.h` placed directly in `include/`, a consumer passing `-Iinclude` risks shadowing system headers or other libraries' headers. Placing them in `include/my_project/` forces the consumer to write `#include <my_project/types.h>`, which guarantees a unique namespace.
</details>

<details>
<summary><strong>2. What is an opaque pointer (also known as the PIMPL or handle idiom) in C?</strong></summary>

An opaque pointer is a pointer to an incomplete struct type declared in a public header (e.g. `typedef struct database db_t;`). The full struct layout is defined exclusively within the private `.c` implementation file. This hides memory layout from the consumer, enforces strict encapsulation, and allows changing the struct definition without breaking the binary ABI.
</details>

<details>
<summary><strong>3. What is the difference between compiling with `-fPIC` and compiling without it?</strong></summary>

`-fPIC` generates Position-Independent Code. In PIC, all memory references to global variables and functions use PC-relative addressing via a Global Offset Table (GOT) and Procedure Linkage Table (PLT). This is mandatory for shared libraries (`.so`) so the OS can map the library code to any arbitrary virtual address across different processes.
</details>

<details>
<summary><strong>4. Why should object files be written to a dedicated `build/` directory instead of next to source files?</strong></summary>

It guarantees clean separation between immutable source code and transient build artifacts. It avoids polluting version control (`.gitignore`), prevents stale object files from interfering with builds, and allows a full project clean by simply deleting the `build/` directory (`rm -rf build`).
</details>

<details>
<summary><strong>5. What is the difference between `#include "..."` and `#include <...>`?</strong></summary>

`#include "..."` tells the preprocessor to look in the directory containing the currently executing file first before searching `-I` paths. `#include <...>` skips the current file's directory and begins searching immediately along the include paths specified by `-I` and system directories.
</details>

<details>
<summary><strong>6. How do you create a static library (`.a`) from compiled `.o` object files on Linux?</strong></summary>

Using the `ar` (archiver) tool: `ar rcs libmy_project.a file1.o file2.o`. The `r` flag replaces existing members, `c` creates the archive if it does not exist, and `s` creates an internal object-file symbol index.
</details>

<details>
<summary><strong>7. What compiler flag hides internal library symbols in a shared library?</strong></summary>

`-fvisibility=hidden`. When enabled, all functions and variables are marked as local to the shared object unless explicitly tagged with `__attribute__((visibility("default")))`.
</details>

<details>
<summary><strong>8. Why should you avoid putting global variable definitions in header files?</strong></summary>

If a header containing `int global_counter = 0;` is included in more than one `.c` file, each translation unit will emit a strong definition of `global_counter`. When the linker attempts to combine the object files, it will fail with a duplicate symbol error (`multiple definition of 'global_counter'`).
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Setting up Project Directory Tree</strong></summary>

**Goal**: Write a bash script that generates the canonical C project folder layout with a single command.

**Solution**:
```bash
#!/bin/bash
PROJECT_NAME="core_engine"
mkdir -p ${PROJECT_NAME}/{include/${PROJECT_NAME},src,tests,docs,build/{obj,bin}}
touch ${PROJECT_NAME}/Makefile
touch ${PROJECT_NAME}/README.md
touch ${PROJECT_NAME}/.gitignore
echo "build/" > ${PROJECT_NAME}/.gitignore
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Implementing the Opaque Pointer Idiom</strong></summary>

**Goal**: Write a public header `counter.h` and implementation `counter.c` that exposes an opaque counter without revealing the internal integer value.

**Solution**:
In `include/counter.h`:
```c
#ifndef COUNTER_H
#define COUNTER_H

typedef struct counter counter_t;

counter_t *counter_create(void);
void counter_increment(counter_t *c);
int counter_get(const counter_t *c);
void counter_destroy(counter_t *c);

#endif
```

In `src/counter.c`:
```c
#include "counter.h"
#include <stdlib.h>

struct counter {
    int value;
};

counter_t *counter_create(void) {
    counter_t *c = malloc(sizeof(*c));
    if (c) c->value = 0;
    return c;
}

void counter_increment(counter_t *c) {
    if (c) c->value++;
}

int counter_get(const counter_t *c) {
    return c ? c->value : 0;
}

void counter_destroy(counter_t *c) {
    free(c);
}
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Makefile with Public/Private Header Separation</strong></summary>

**Goal**: Author a Makefile that compiles a static library `libcore.a` from `src/` using `include/` for public headers and outputs everything to `build/`.

**Solution**:
```makefile
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2 -MMD -MP
CPPFLAGS := -Iinclude -Isrc
AR := ar
ARFLAGS := rcs

BUILD_DIR := build
SRC_DIR := src
TARGET_LIB := $(BUILD_DIR)/libcore.a

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/obj/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET_LIB)

$(TARGET_LIB): $(OBJS) | $(BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)/obj
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(BUILD_DIR)/obj:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
```
</details>

---

## 12. Summary / Cheat Sheet

- `include/<project>/`: Public headers; installed to system; namespaced subfolder.
- `src/`: Private `.c` files and internal `.h` files; never installed.
- `build/`: Out-of-tree artifacts (`obj/`, `bin/`); easily wiped.
- **Opaque Pointers**: Keep struct definitions in `src/` to guarantee binary ABI stability.
- `-Iinclude`: Always pass to preprocessor; write `#include <my_project/header.h>`.

---

## 13. Further Reading

- Eric S. Raymond: *The Art of Unix Programming* (Modular Architecture).
- Linux Foundation: *Filesystem Hierarchy Standard (FHS)*.
- Ulrich Drepper: *How To Write Shared Libraries*.

---

[← Previous: 09-reading-assembly-godbolt.md](./09-reading-assembly-godbolt.md) | [Section Index](./README.md) | [Next: 11-headers-and-include-guards-intro.md](./11-headers-and-include-guards-intro.md) →
