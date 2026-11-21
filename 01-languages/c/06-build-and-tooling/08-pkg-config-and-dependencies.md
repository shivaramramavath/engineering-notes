# pkg-config and Library Dependencies

> Master third-party dependency management in C: the `pkg-config` metadata architecture, querying compiler and linker flags (`--cflags`, `--libs`), search path resolution (`PKG_CONFIG_PATH`), authoring custom `.pc` specification files, and CMake `FindPkgConfig` integration.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-make-in-depth.md](./01-make-in-depth.md), [02-cmake.md](./02-cmake.md), [04-advanced/04-linking-static-vs-shared-libraries.md](../04-advanced/04-linking-static-vs-shared-libraries.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| pkg-config / pkgconf / POSIX.1-2008 / FreeDesktop.org Standard |

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
When linking third-party libraries in C (such as OpenSSL, SQLite, libuv, or zlib), hardcoding paths like `-I/usr/local/include` and `-L/usr/local/lib -lssl -lcrypto` into Makefiles is brittle. On Ubuntu, headers might reside in `/usr/include`; on macOS Homebrew, they live in `/opt/homebrew/include`; and on FreeBSD, they reside in `/usr/local/include`.

Furthermore, libraries often depend transitively on secondary libraries (e.g. OpenSSL needing `libdl` and `pthread`). Hardcoding these dependencies manually is error-prone and breaks across operating systems.

**`pkg-config`** (and modern replacement **`pkgconf`**) is the universal package management helper for C. Installed libraries publish a standardized metadata file with extension **`.pc`**. Applications and build systems query `pkg-config` to discover the exact compiler flags, include directories, linker paths, and library dependencies required, providing clean, cross-platform build automation.

---

## 2. Core Concepts
- **The `.pc` File**: A simple key-value metadata text file installed alongside library headers and binaries (typically in `/usr/lib/pkgconfig` or `/usr/local/lib/pkgconfig`).
- **Core Queries**:
  - `pkg-config --cflags <lib>`: Returns preprocessor and include directory flags (e.g. `-I/usr/include/libuv`).
  - `pkg-config --libs <lib>`: Returns linker flags for dynamic linking (e.g. `-luv`).
  - `pkg-config --libs --static <lib>`: Returns all private transitive dependencies required for **static** linking (e.g. `-luv -lrt -lpthread -lnsl -ldl`).
  - `pkg-config --modversion <lib>`: Returns the library version string (e.g. `1.44.2`).
- **Environment Search Paths**:
  - `PKG_CONFIG_PATH`: Colon-separated list of additional directories where `pkg-config` searches for `.pc` files before consulting default system paths.
  - `PKG_CONFIG_LIBDIR`: Overrides system default search directories (essential for cross-compilation!).
- **CMake Integration**: The `FindPkgConfig` module wraps `pkg-config` to create native modern CMake imported targets (`PkgConfig::LIBNAME`).

---

## 3. How It Works Internally

### Package Metadata Resolution
When a library author installs a library, they install a `.pc` file:

```
File: /usr/lib/pkgconfig/libuv.pc
+-------------------------------------------------------+
| prefix=/usr                                           |
| exec_prefix=${prefix}                                 |
| libdir=${exec_prefix}/lib                             |
| includedir=${prefix}/include                          |
|                                                       |
| Name: libuv                                           |
| Version: 1.44.2                                       |
| Description: Multi-platform asynchronous I/O library  |
| Cflags: -I${includedir}                               |
| Libs: -L${libdir} -luv                                |
| Libs.private: -lpthread -ldl -lrt                     |
+-------------------------------------------------------+
```

When you execute `pkg-config --libs --static libuv`:
1. `pkg-config` scans directories in `PKG_CONFIG_PATH` followed by system paths.
2. It parses `libuv.pc`, performs variable expansion (replacing `${prefix}` with `/usr`), and combines `Libs` and `Libs.private`.
3. It emits the exact consolidated string: `-L/usr/lib -luv -lpthread -ldl -lrt`.

---

## 4. Syntax & Usage

### Authoring a Production `.pc` File Template
```ini
# myengine.pc.in (Processed by CMake or configure)
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: @PROJECT_NAME@
Description: @PROJECT_DESCRIPTION@
Version: @PROJECT_VERSION@
URL: https://github.com/myorg/myengine
Requires: libuv >= 1.40.0
Requires.private: openssl >= 1.1.1
Cflags: -I${includedir}
Libs: -L${libdir} -lmyengine
Libs.private: -lm -lpthread
```

### Integration into GNU Makefiles
```makefile
# Makefile
CC      := gcc
CFLAGS  := -std=c11 -Wall -O2

# Query flags from pkg-config with error checking
DEPS    := libuv sqlite3
CFLAGS  += $(shell pkg-config --cflags $(DEPS))
LDLIBS  += $(shell pkg-config --libs $(DEPS))

all: app

app: src/main.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@
```

### Integration into Modern CMake
```cmake
find_package(PkgConfig REQUIRED)

# Search for libuv >= 1.40.0 and import as a target
pkg_check_modules(UV REQUIRED IMPORTED_TARGET libuv>=1.40.0)

add_executable(my_server src/main.c)
# Links libraries, adds include directories and definitions automatically!
target_link_libraries(my_server PRIVATE PkgConfig::UV)
```

---

## 5. Practical Examples

### Example 1: `.pc` Metadata Package File Generation
```c
#include <stdio.h>

typedef struct {
    const char *prefix;
    const char *name;
    const char *version;
    const char *description;
    const char *cflags;
    const char *libs;
} PkgConfigSpec;

void render_pc_file(const PkgConfigSpec *pkg) {
    printf("prefix=%s\n", pkg->prefix);
    printf("includedir=${prefix}/include\n");
    printf("libdir=${prefix}/lib\n\n");
    printf("Name: %s\n", pkg->name);
    printf("Description: %s\n", pkg->description);
    printf("Version: %s\n", pkg->version);
    printf("Cflags: -I${includedir} %s\n", pkg->cflags);
    printf("Libs: -L${libdir} %s\n", pkg->libs);
}

int main(void) {
    printf("Generating .pc Metadata Package File...\n\n");
    PkgConfigSpec spec = {
        .prefix = "/usr/local",
        .name = "libmyengine",
        .version = "1.4.0",
        .description = "High Performance C Networking Engine",
        .cflags = "-D_GNU_SOURCE",
        .libs = "-lmyengine -lpthread"
    };
    render_pc_file(&spec);
    printf("\npkg-config --cflags --libs libmyengine verification successful.\n");
    return 0;
}
```

```text
Generating .pc Metadata Package File...

prefix=/usr/local
includedir=${prefix}/include
libdir=${prefix}/lib

Name: libmyengine
Description: High Performance C Networking Engine
Version: 1.4.0
Cflags: -I${includedir} -D_GNU_SOURCE
Libs: -L${libdir} -lmyengine -lpthread

pkg-config --cflags --libs libmyengine verification successful.
```

### Example 2: Checking Library Version in Shell Build Scripts
```bash
# Verify minimum required version of OpenSSL
if ! pkg-config --atleast-version=1.1.1 openssl; then
    echo "Error: OpenSSL >= 1.1.1 is required to build this project!"
    echo "Installed version: $(pkg-config --modversion openssl)"
    exit 1
fi
```

---

## 6. Best Practices
1. **Always Supply `Libs.private` in `.pc` Files**: When authoring libraries, put secondary dependencies (like `-lpthread` or internal sub-libraries) in `Libs.private` and `Requires.private`. This keeps dynamic linking clean while ensuring static linking (`--static`) works seamlessly.
2. **Use `IMPORTED_TARGET` in CMake**: In CMake, always pass `IMPORTED_TARGET` to `pkg_check_modules()`. This creates a standard modern target (`PkgConfig::NAME`) that encapsulates include paths and link options.
3. **Configure `PKG_CONFIG_PATH` for Custom Installs**: If libraries are installed in non-standard prefixes (e.g. `/opt/custom` or `$HOME/.local`), export `PKG_CONFIG_PATH=/opt/custom/lib/pkgconfig:$PKG_CONFIG_PATH`.
4. **Always Install `.pc` Files with C Libraries**: If you distribute a shared or static C library, always generate and install a matching `.pc` file so other developers can link against your library easily.
5. **Use `pkgconf` When Available**: `pkgconf` is a modern, drop-in C reimplementation of `pkg-config` that resolves circular dependencies faster and conforms strictly to FreeDesktop specifications.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Hardcoding `-I/usr/include/foo` in Makefile | Fails on macOS, FreeBSD, or custom installations where paths differ | Use `$(shell pkg-config --cflags foo)` |
| Omitting `--static` when building static binaries | Static linking fails with undefined symbol errors because private dependencies are omitted | Use `pkg-config --libs --static foo` |
| Using host `pkg-config` during cross-compilation | Returns host paths (`/usr/lib/x86_64...`), breaking cross-compiler linking | Set `PKG_CONFIG_LIBDIR` pointing exclusively to target sysroot pkgconfig |
| Hardcoding absolute paths in `.pc` file | Moving the installation directory breaks the `.pc` metadata | Use `${prefix}` variable expansion |
| Assuming `pkg-config` is always installed | Minimal Docker images or embedded buildroots may omit `pkg-config` | Provide informative build failure message if `pkg-config` command is missing |
| Putting private dependencies in `Libs:` | Forces consumers of dynamic library to link against internal dependencies unnecessarily | Put internal dependencies in `Libs.private:` |

---

## 8. Performance & Security Notes
- **Cross-Compilation Isolation with `PKG_CONFIG_LIBDIR`**: In cross-compilation environments, host `pkg-config` will cheerfully return x86-64 library flags for an ARM build if `PKG_CONFIG_LIBDIR` is not overridden. Always isolate sysroot paths using:
  ```bash
  export PKG_CONFIG_DIR=
  export PKG_CONFIG_LIBDIR=/opt/sysroots/aarch64/usr/lib/pkgconfig
  export PKG_CONFIG_SYSROOT_DIR=/opt/sysroots/aarch64
  ```
- **Supply Chain Integrity**: Ensure `.pc` files are installed in directories writable only by `root` (`/usr/lib/pkgconfig`), as malicious `.pc` files can inject arbitrary compiler flags into builds.

---

## 9. Comparison / When to Use What

| Mechanism | Scope | Portability | Best Use Case |
| :--- | :--- | :--- | :--- |
| **`pkg-config`** | Standard Unix / Linux | Universal Unix / MSYS2 | C library metadata, Makefiles, meson, autotools |
| **CMake `find_package()`**| CMake-based projects | Universal (Windows MSVC, Unix) | Modern CMake target integration |
| **Direct `-l` Flags** | Hardcoded | Very poor | Quick 1-file test programs |
| **Vcpkg / Conan** | Full package manager | Cross-platform | Full binary dependency package management |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `Libs` and `Libs.private` in a `.pc` file?</summary>
- `Libs`: Lists the linker flags required to link against the library dynamically. Because dynamic linkers resolve internal dependencies automatically via `DT_NEEDED` tags in shared objects, only the library itself (e.g. `-lfoo`) is needed.
- `Libs.private`: Lists secondary internal dependencies required *only* when linking **statically** (`--static`). When building a static binary, the linker must resolve all internal references explicitly, so flags from both `Libs` and `Libs.private` are concatenated.
</details>

<details>
<summary>2. What is the difference between `PKG_CONFIG_PATH` and `PKG_CONFIG_LIBDIR`?</summary>
- `PKG_CONFIG_PATH`: Appends additional search directories *before* the default system directories (`/usr/lib/pkgconfig`, `/usr/share/pkgconfig`).
- `PKG_CONFIG_LIBDIR`: Completely **replaces and overrides** the default system search directories. This is critical in cross-compilation to prevent the tool from searching host system libraries.
</details>

<details>
<summary>3. What does `pkg-config --cflags libuv` output?</summary>
It prints the preprocessor and compiler flags required to compile source files including `libuv` headers, typically `-I/usr/include/libuv` (or nothing if headers reside in the standard default system include path `/usr/include`).
</details>

<details>
<summary>4. How does `Requires` differ from `Requires.private`?</summary>
- `Requires`: Specifies other packages whose flags must be unconditionally added to both dynamic and static linking queries.
- `Requires.private`: Specifies other packages whose flags are needed *only* during static linking (`--static`).
</details>

<details>
<summary>5. How can you verify that a library meets a minimum version requirement in a script?</summary>
Using `pkg-config --atleast-version=<ver> <package>`. It returns exit status 0 if the condition is met, and a non-zero exit status if the installed version is too old or the package is not installed.
</details>

<details>
<summary>6. How does Modern CMake integrate with `pkg-config`?</summary>
Via the `FindPkgConfig` module:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
target_link_libraries(app PRIVATE PkgConfig::GLIB)
```
</details>

<details>
<summary>7. What does the `PKG_CONFIG_SYSROOT_DIR` environment variable do?</summary>
It instructs `pkg-config` to prepend the specified sysroot path prefix to all `-I` include and `-L` library directory paths returned by `.pc` queries, ensuring proper cross-compilation path resolution.
</details>

<details>
<summary>8. Where does `pkg-config` search for `.pc` files by default on standard Linux systems?</summary>
Typically in:
1. `/usr/lib/pkgconfig` and `/usr/lib64/pkgconfig`
2. `/usr/share/pkgconfig` (for architecture-independent packages)
3. `/usr/local/lib/pkgconfig`
</details>

---


<details>
<summary><strong>7. How does <code>pkg-config</code> determine whether to emit static archive dependencies?</strong></summary>

By default, <code>pkg-config --libs <lib></code> emits only direct dynamic shared library flags (e.g., <code>-lfoo</code>). When <code>--static</code> is passed, <code>pkg-config</code> parses the <code>Libs.private</code> field in the <code>.pc</code> file and recursively resolves all transitive dependencies required for linking a static archive (e.g., <code>-lfoo -lbar -lpthread -lm</code>).
</details>

<details>
<summary><strong>8. What should a developer do if a third-party dependency does not provide a <code>.pc</code> file?</strong></summary>

You can either write a custom CMake find module (<code>Find<Lib>.cmake</code>), construct an automated pkg-config file generator in your build system, or write a minimal <code><lib>.pc</code> file and place it in a local directory included in <code>PKG_CONFIG_PATH</code>. This preserves a uniform dependency resolution interface across your entire build pipeline.
</details>

## 11. Practice Exercises

### Easy
1. **Query System Packages**: Use `pkg-config --list-all` to inspect all packages installed on your system.
2. **Version Query**: Check the version of `zlib` or `glib` using `pkg-config --modversion zlib`.
3. **Flags Printer**: Write a shell command that prints the compiler and linker flags for `sqlite3`.

<details>
<summary>Solutions (Easy)</summary>

```bash
# Exercise 2 & 3
pkg-config --modversion zlib
pkg-config --cflags --libs sqlite3
```
</details>

### Medium
4. **Custom .pc File Author**: Write a valid `libfastmath.pc` file that specifies prefix `/opt/fastmath`, version 2.0.0, and private dependency `-lm`.
5. **Makefile with pkg-config Discovery**: Author a Makefile that uses `pkg-config` to find `openssl` and aborts with a clean error message if OpenSSL is not installed.
6. **PKG_CONFIG_PATH Custom Test**: Place `libfastmath.pc` in `/tmp/my_pkg/` and verify that running `PKG_CONFIG_PATH=/tmp/my_pkg pkg-config --libs libfastmath` resolves correctly.

<details>
<summary>Solutions (Medium)</summary>

```makefile
# Exercise 5: Makefile with pkg-config check
DEPS := openssl

CHECK := $(shell pkg-config --exists $(DEPS) && echo "OK" || echo "FAIL")
ifeq ($(CHECK),FAIL)
  $(error "Missing dependency: openssl is required to build this project!")
endif

CFLAGS += $(shell pkg-config --cflags $(DEPS))
LDLIBS += $(shell pkg-config --libs $(DEPS))

app: main.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@
```
</details>

### Hard
7. **CMake Automated .pc File Generator**: In a CMake project that builds a library `myengine`, use `configure_file(myengine.pc.in myengine.pc @ONLY)` and `install(FILES ...)` to generate and install a valid `.pc` file.
8. **Cross-Compilation Sysroot Wrapper**: Write a shell script `arm-pkg-config` that sets `PKG_CONFIG_LIBDIR` and `PKG_CONFIG_SYSROOT_DIR` to wrap `pkgconf` for an ARM sysroot.
9. **C Language .pc File Parser**: Implement a lightweight C parser that reads a `.pc` file, expands `${prefix}` variables, and extracts the `Cflags` and `Libs` strings.

<details>
<summary>Solutions (Hard)</summary>

```bash
#!/bin/bash
# Exercise 8: arm-pkg-config Cross-Compilation Wrapper
SYSROOT="/opt/sysroots/aarch64"

export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR="${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="${SYSROOT}"

exec pkg-config "$@"
```
</details>

---

## 12. Summary / Cheat Sheet
- **Command**: `pkg-config --cflags --libs <package>`.
- **Static Link**: `pkg-config --libs --static <package>`.
- **Version Check**: `pkg-config --atleast-version=1.2.0 <package>`.
- **Search Path**: `export PKG_CONFIG_PATH=/custom/dir/pkgconfig:$PKG_CONFIG_PATH`.
- **Cross Build**: Use `PKG_CONFIG_LIBDIR` and `PKG_CONFIG_SYSROOT_DIR`.
- **CMake**: `find_package(PkgConfig REQUIRED)` + `pkg_check_modules(... IMPORTED_TARGET)`.

---

## 13. Further Reading
- FreeDesktop.org pkg-config Specification and Guide.
- pkgconf Documentation: Modern pkg-config Replacement.
- CMake Documentation: FindPkgConfig Module.

---

[← Previous Note: Cross-Compilation Intro](./07-cross-compilation-intro.md) | [Module Index](README.md) | [Next Note: Continuous Integration for C](./09-continuous-integration-for-c.md)
