# Modern CMake for C Projects

> Master target-centric build engineering with Modern CMake 3.x: `CMakeLists.txt` architecture, targets and properties, interface visibility (`PUBLIC`, `PRIVATE`, `INTERFACE`), generator expressions, out-of-source builds, and automated testing with CTest.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-make-in-depth.md](./01-make-in-depth.md), [04-advanced/04-linking-static-vs-shared-libraries.md](../04-advanced/04-linking-static-vs-shared-libraries.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| CMake 3.20+ / ISO C11 / C17 / C23 (Ninja, Make, MSVC generators) |

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
While Makefiles are exceptional for Unix-centric development, they are platform-specific: a GNU Makefile targeting GCC on Linux does not work natively with Microsoft Visual Studio (MSVC) on Windows or Xcode on macOS. Furthermore, managing transitive library dependencies across large modular repositories manually in Makefiles is complex and prone to errors.

**CMake** is a cross-platform **meta-build system**. You write declarative, target-oriented build specifications in `CMakeLists.txt`, and CMake generates the native build scripts for your platform: GNU Makefiles on Linux, Ninja on CI servers, Visual Studio solutions (`.sln`) on Windows, or Xcode projects on macOS. **Modern CMake** (version 3.0+) treats build engineering as an object-oriented dependency graph of *targets* and *properties*, completely eliminating fragile global directory variables.

---

## 2. Core Concepts
- **Targets**: The primary citizens of Modern CMake. Created with `add_executable(name ...)` or `add_library(name [STATIC|SHARED|INTERFACE] ...)`.
- **Target Properties & Transitive Usage Requirements**:
  - `target_include_directories(tgt SCOPE dirs...)`
  - `target_compile_definitions(tgt SCOPE defs...)`
  - `target_compile_options(tgt SCOPE flags...)`
  - `target_link_libraries(tgt SCOPE libs...)`
- **Visibility Scopes**:
  - `PRIVATE`: Requirement applies *only* to building this target; not passed to consumers.
  - `INTERFACE`: Requirement applies *only* to consumers of this target; not used to build the target itself (used for header-only libraries).
  - `PUBLIC`: Requirement applies to *both* building this target and any consumer linking against it.
- **Out-of-Source Builds**: CMake mandates building in a dedicated separate directory (e.g. `mkdir build && cd build && cmake ..`). Source directories remain completely pristine with zero build clutter.
- **Generator Expressions (`$<...>`)**: Evaluated at build-system generation time rather than configuration time (e.g. `$<BUILD_INTERFACE:...>` vs `$<INSTALL_INTERFACE:...>`).

---

## 3. How It Works Internally

### Target Dependency Graph Propagation
When you link Target A to Target B with `target_link_libraries(App PRIVATE CoreLib)`, Modern CMake propagates transitive usage requirements automatically:

```
+-------------------------------------------------------------------+
| Target: CoreLib (STATIC Library)                                  |
|   - Sources: src/core.c                                           |
|   - PUBLIC Include Dirs: ${CMAKE_CURRENT_SOURCE_DIR}/include       |
|   - PUBLIC Defines: USE_CORE_V2=1                                 |
+-------------------------------------------------------------------+
                                  ^
                                  | Transitive Propagation!
                                  | (Inherits include dirs & definitions automatically)
+-------------------------------------------------------------------+
| Target: App (Executable)                                          |
|   - Sources: src/main.c                                           |
|   - target_link_libraries(App PRIVATE CoreLib)                    |
+-------------------------------------------------------------------+
```

During the configuration step (`cmake -B build`), CMake parses `CMakeLists.txt`, resolves the target dependency graph, and verifies compiler capabilities. During the generation step, it outputs native Ninja (`build.ninja`) or Make (`Makefile`) files with all include paths and compiler flags automatically expanded.

---

## 4. Syntax & Usage

### Modern Production `CMakeLists.txt` Template
```cmake
cmake_minimum_required(VERSION 3.20)
project(HighPerfEngine
    VERSION 1.2.0
    DESCRIPTION "Industrial C Systems Engine"
    LANGUAGES C
)

# Enforce standard C11 across all targets
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Core Static Library
add_library(core_engine STATIC
    src/allocator.c
    src/buffer.c
)

target_include_directories(core_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
)

target_compile_options(core_engine PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -O2>
    $<$<C_COMPILER_ID:MSVC>:/W4 /O2>
)

# Application Executable
add_executable(engine_server src/main.c)
target_link_libraries(engine_server PRIVATE core_engine)
```

---

## 5. Practical Examples

### Example 1: Modern CMake Target Architecture Simulation
```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    const char *target_name;
    const char *type; // EXECUTABLE, STATIC, SHARED
    const char *c_std;
    bool pic;
} CMakeTarget;

void print_cmake_target(const CMakeTarget *t) {
    printf("CMake Target: %s\n", t->target_name);
    printf("  Type: %s\n", t->type);
    printf("  C Standard: %s (C_STANDARD_REQUIRED=ON)\n", t->c_std);
    printf("  Position Independent Code (PIC): %s\n", t->pic ? "ON" : "OFF");
}

int main(void) {
    printf("Evaluating Modern CMake 3.x Target-Based Architecture...\n");
    CMakeTarget core_lib = { "core_engine", "STATIC", "11", true };
    CMakeTarget server_app = { "network_server", "EXECUTABLE", "11", false };

    print_cmake_target(&core_lib);
    print_cmake_target(&server_app);
    printf("target_link_libraries(network_server PRIVATE core_engine) resolved.\n");
    return 0;
}
```

```text
Evaluating Modern CMake 3.x Target-Based Architecture...
CMake Target: core_engine
  Type: STATIC
  C Standard: 11 (C_STANDARD_REQUIRED=ON)
  Position Independent Code (PIC): ON
CMake Target: network_server
  Type: EXECUTABLE
  C Standard: 11 (C_STANDARD_REQUIRED=ON)
  Position Independent Code (PIC): OFF
target_link_libraries(network_server PRIVATE core_engine) resolved.
```

### Example 2: Automated Testing with CTest Integration
```cmake
# Enable CTest suite
enable_testing()

add_executable(test_allocator tests/test_allocator.c)
target_link_libraries(test_allocator PRIVATE core_engine)

add_test(NAME AllocatorUnitTest COMMAND test_allocator)
set_tests_properties(AllocatorUnitTest PROPERTIES TIMEOUT 10)
```

Running `ctest --output-on-failure` inside the build directory discovers and runs all registered tests automatically with execution time reporting.

---

## 6. Best Practices
1. **Never Use Legacy Global Commands**: Avoid `include_directories()`, `link_libraries()`, `add_definitions()`. Always use their target-specific modern equivalents: `target_include_directories()`, `target_link_libraries()`, and `target_compile_definitions()`.
2. **Explicitly Set Visibility Scopes**: Always declare `PUBLIC`, `PRIVATE`, or `INTERFACE` for all target requirements. Private headers belong in `PRIVATE`; public API headers belong in `PUBLIC`.
3. **Always Build Out-of-Source**: Never run `cmake .` in the root repository. Use `cmake -B build -S .` to keep all generated files contained inside `build/`.
4. **Use Ninja for Blazing Fast Compilation**: On multi-core systems, configure builds with `-G Ninja` (`cmake -B build -G Ninja`). Ninja executes parallel builds significantly faster than Make.
5. **Use Generator Expressions for Multi-Compiler Warnings**: Never set raw `CMAKE_C_FLAGS = "-Wall"`. Use `$<C_COMPILER_ID:...>` to configure warning flags safely across GCC, Clang, and MSVC.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Using `include_directories()` globally | Pollutes the include path of every subsequent target in the project, masking missing include bugs | Use `target_include_directories(tgt PRIVATE/PUBLIC)` |
| In-source builds (`cmake .`) | Pollutes git source tree with hundreds of cache files and build artifacts that cannot be cleaned cleanly | Build in separate directory: `cmake -B build` |
| Using `file(GLOB SRCS *.c)` for source lists | CMake does not detect newly added source files automatically; ninja/make will not reconfigure | Explicitly enumerate source files in `add_executable/library` |
| Hardcoding compiler flags into `CMAKE_C_FLAGS` | Overrides user and CI environment variables; breaks when changing compilers (e.g. to MSVC) | Use `target_compile_options()` with generator expressions |
| Missing `find_package()` check | Build fails cryptically during compile time if required system libraries are missing | Use `find_package(Lib REQUIRED)` with error diagnostics |
| Forgetting `C_STANDARD_REQUIRED ON` | If compiler lacks C11 support, CMake quietly degrades to C99 without error | Set `set(CMAKE_C_STANDARD_REQUIRED ON)` |

---

## 8. Performance & Security Notes
- **Link-Time Optimization (LTO) via CMake**: Enable whole-program optimization portably using CMake's built-in feature:
  ```cmake
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported)
  if(ipo_supported)
      set_target_properties(my_app PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
  ```
- **Exporting Compile Commands (`compile_commands.json`)**: Modern language servers (Clangd, Neovim, VSCode) and static analyzers require a compilation database. Enable it with:
  ```cmake
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  ```

---

## 9. Comparison / When to Use What

| Feature | Legacy CMake (Pre-3.0) | Modern CMake (3.x) |
| :--- | :--- | :--- |
| **Model** | Directory-based global state | **Target-based property graph** |
| **Includes** | `include_directories()` | `target_include_directories()` |
| **Transitivity**| Manual propagation | **Automatic via PUBLIC/INTERFACE** |
| **Standard** | Flags in `CMAKE_C_FLAGS` | `set(CMAKE_C_STANDARD 11)` |
| **Safety** | High risk of flag pollution | Strict isolation between modules |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between `PUBLIC`, `PRIVATE`, and `INTERFACE` in Modern CMake?</summary>
- `PRIVATE`: The property (include directory, compile flag, or linked library) applies *only* to the target itself when being compiled. It is not inherited by targets that link against it.
- `INTERFACE`: The property is *not* used to compile the target itself (common for header-only libraries or interface targets), but is automatically propagated to any downstream consumer target that links against it.
- `PUBLIC`: The property is used to compile the target itself *and* is automatically propagated to all downstream consumers.
</details>

<details>
<summary>2. Why is `file(GLOB ...)` discouraged for collecting source files in CMake?</summary>
CMake generates native build files (Makefiles or Ninja files) based on the files present at configuration time. If a developer adds or deletes a `.c` file in the directory, the build system (Ninja or Make) has no mechanism to know that the directory contents changed, and will not automatically re-run CMake. The new file will be omitted from the build. Explicitly listing source files ensures the build system re-runs CMake when `CMakeLists.txt` is edited.
</details>

<details>
<summary>3. What is the difference between configuration time and generation time in CMake?</summary>
- **Configuration Time**: CMake reads `CMakeLists.txt`, executes CMake scripting commands, evaluates variables and functions, checks compiler capabilities, and builds the internal target dependency graph.
- **Generation Time**: CMake evaluates Generator Expressions (`$<...>`) and writes the physical native build files (e.g. `Makefile`, `build.ninja`, `.sln`).
</details>

<details>
<summary>4. What does `target_include_directories(tgt PUBLIC $<BUILD_INTERFACE:...> $<INSTALL_INTERFACE:...>)` accomplish?</summary>
It separates include directories used during internal project compilation from include directories used after the library is installed on a customer's machine:
- `BUILD_INTERFACE`: Specifies local repository header paths (e.g. `${CMAKE_CURRENT_SOURCE_DIR}/include`) used when building within the source tree.
- `INSTALL_INTERFACE`: Specifies relative paths (e.g. `include`) where headers will reside after `make install` or `cmake --install`.
</details>

<details>
<summary>5. How does CMake support automated unit testing?</summary>
Through the built-in **CTest** module: invoking `enable_testing()` enables test registration. Each unit test binary is added using `add_test(NAME TestName COMMAND binary_target)`. Tests can be executed across the entire repository by running `ctest` or `cmake --build build --target test`.
</details>

<details>
<summary>6. How do you pass preprocessor definitions to a specific target in CMake?</summary>
Using `target_compile_definitions(target_name PRIVATE DEBUG_LEVEL=2 LOG_ENABLED)`. This sets the definitions specifically for that target without affecting any other targets in the project.
</details>

<details>
<summary>7. What is `CMAKE_EXPORT_COMPILE_COMMANDS` and why is it useful?</summary>
Setting `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` instructs CMake to generate a `compile_commands.json` file in the build directory. This file describes the exact compiler flags, include paths, and definitions used for every source file, allowing tools like Clangd, clang-tidy, and IDEs to provide accurate syntax highlighting and static analysis.
</details>

<details>
<summary>8. How do you build a CMake project using Ninja from the command line?</summary>
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```
</details>

---

## 11. Practice Exercises

### Easy
1. **Basic Executable CMakeLists**: Write a minimal `CMakeLists.txt` that compiles `main.c` with C11 standard enforced.
2. **Static Library Link**: Write a `CMakeLists.txt` that creates a static library `math_lib` from `src/math.c` and links it to `app`.
3. **Compile Definitions**: Use `target_compile_definitions()` to define `VERSION_STRING="1.0.0"` for an executable.

<details>
<summary>Solutions (Easy)</summary>

```cmake
# Exercise 1 & 2
cmake_minimum_required(VERSION 3.20)
project(EasyApp LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(math_lib STATIC src/math.c)
target_include_directories(math_lib PUBLIC include)

add_executable(app src/main.c)
target_link_libraries(app PRIVATE math_lib)
target_compile_definitions(app PRIVATE VERSION_STRING="1.0.0")
```
</details>

### Medium
4. **Header-Only Interface Library**: Create an `INTERFACE` library target `fast_headers` and link it to two separate executables.
5. **CTest Test Suite**: Configure a project with 2 unit tests, register them with `add_test()`, and verify execution via `ctest`.
6. **Multi-Platform Warnings with Generator Expressions**: Write a `CMakeLists.txt` that enables `-Wall -Wextra` on GCC/Clang and `/W4` on MSVC using generator expressions.

<details>
<summary>Solutions (Medium)</summary>

```cmake
# Exercise 4 & 6
add_library(fast_headers INTERFACE)
target_include_directories(fast_headers INTERFACE include)

target_compile_options(fast_headers INTERFACE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

add_executable(tool1 src/tool1.c)
target_link_libraries(tool1 PRIVATE fast_headers)

add_executable(tool2 src/tool2.c)
target_link_libraries(tool2 PRIVATE fast_headers)
```
</details>

### Hard
7. **Production Multi-Module CMake Project**: Author a modular `CMakeLists.txt` structure featuring a root file, `lib/` subdirectory exporting an installable shared library with public headers, and `cli/` building an executable with CTest suite.
8. **Sanitizer Build Type Option**: Add a CMake option `ENABLE_ASAN` that adds `-fsanitize=address,undefined` to compiler and linker flags for all targets when enabled.
9. **CMake Package Config Export**: Implement `install(TARGETS ... EXPORT ...)` and `install(EXPORT ...)` so external projects can consume the built library via `find_package(MyEngine REQUIRED)`.

<details>
<summary>Solutions (Hard)</summary>

```cmake
# Exercise 7: Modular Multi-Directory CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(ModularApp VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(lib)
add_subdirectory(cli)

# Exercise 8: Sanitizer Build Option
option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling AddressSanitizer and UBSan")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers not supported on current compiler")
    endif()
endif()

# Exercise 9: Package Config Export
include(GNUInstallDirs)
install(TARGETS core_engine
    EXPORT CoreEngineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT CoreEngineTargets
    FILE CoreEngineTargets.cmake
    NAMESPACE CoreEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CoreEngine
)
```
</details>

---

## 12. Summary / Cheat Sheet
- **Out-of-source**: `cmake -B build -S .` and `cmake --build build`.
- **Target model**: `add_executable(app src.c)` / `add_library(lib STATIC src.c)`.
- **Properties**: `target_include_directories()`, `target_link_libraries()`.
- **Visibility**:
  - `PRIVATE`: Target build only.
  - `INTERFACE`: Consumers only.
  - `PUBLIC`: Target + Consumers.
- **CTest**: `enable_testing()` + `add_test(NAME n COMMAND cmd)`.
- **Database**: `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` for Clangd/clang-tidy.

---

## 13. Further Reading
- Craig Scott, *Professional CMake: A Practical Guide* (The definitive resource).
- CMake Official Documentation: Target-Centric Build Design.
- Daniel Pfeifer, *Effective CMake* (CppCon).

---

[← Previous Note: GNU Make in Depth](./01-make-in-depth.md) | [Module Index](README.md) | [Next Note: Static Analysis (Clang-Tidy & Cppcheck)](./03-static-analysis-clang-tidy-cppcheck.md)
