# Makefile Basics

> A comprehensive deep dive into GNU Make for C projects: dependency DAGs, automatic variables, pattern rules, header dependency auto-generation with -MMD/-MP, recursive variables, and phony target orchestration.

| Metadata | Details |
|---|---|
| **Level** | Beginner → Intermediate |
| **Prerequisites** | [01-toolchain.md](./01-toolchain.md), [03-compilation-process.md](./03-compilation-process.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Standard Baseline** | POSIX Make & GNU Make 4.x Conventions |

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

In non-trivial C applications consisting of dozens or hundreds of translation units (`.c` files), invoking `gcc *.c -o myprog` manually for every code modification is catastrophic:
1. **Redundant Compilation Time**: Recompiling every unchanged `.c` file consumes minutes or hours on large codebases instead of compiling only modified translation units.
2. **Missing Transitive Header Dependencies**: If a shared header file `config.h` changes, developer scripts frequently fail to recompile the dependent `.c` files, leading to silent memory corruption and ABI mismatches.
3. **Inconsistent Build Environments**: Team members may compile with divergent compiler flags, missing warning flags, or disparate include paths.

**GNU Make** solves this through an explicit Directed Acyclic Graph (DAG) of targets, prerequisites, and file modification timestamps (`mtime`). If a target file is newer than all of its prerequisites, Make skips execution, guaranteeing minimal, correct, incremental builds.

---

## 2. Core Concepts

### Target, Prerequisite, and Recipe Structure

The foundational unit of a Makefile is a **Rule**:

```makefile
target: prerequisite1 prerequisite2
	recipe_command_1
	recipe_command_2
```
*CRITICAL SYNTAX NOTE*: Every line in a recipe **must** begin with an ASCII Tab character (`\t`), not four spaces. Spaces cause `Makefile: *** missing separator. Stop.`

```
+-------------------------------------------------------------+
|                     MAKE DIRECTED GRAPH                     |
+-------------------------------------------------------------+
|                        [ myapp ]                            |
|                       /         \                           |
|                  [ main.o ]     [ math.o ]                  |
|                  /       \       /       \                  |
|             [ main.c ]  [ common.h ]   [ math.c ]           |
+-------------------------------------------------------------+
```

### Standard Variables in C Makefiles

Standard Make conventions use specific variable names recognized by implicit rules:

| Variable | Conventional Purpose | Example Value |
|---|---|---|
| `CC` | C compiler driver program | `gcc` or `clang` |
| `CFLAGS` | Flags passed to the compiler during compilation | `-std=c11 -Wall -Wextra -O2` |
| `CPPFLAGS` | Flags passed to the C preprocessor | `-Iinclude -DNDEBUG` |
| `LDFLAGS` | Flags passed to the linker driver | `-Llib -Wl,-rpath=/opt/lib` |
| `LDLIBS` | Library link flags passed at the end of the linker line | `-lm -lpthread` |

### Automatic Variables

Inside rule recipes, GNU Make provides automatic variables representing active targets and prerequisites:

| Variable | Meaning |
|---|---|
| `$@` | The file name of the **target** of the rule |
| `$<` | The file name of the **first prerequisite** |
| `$^` | The file names of **all prerequisites**, with spaces, omitting duplicates |
| `$?` | The file names of all prerequisites that are **newer** than the target |
| `$*` | The stem with which an implicit rule matches (e.g. `foo` in `foo.o`) |

---

## 3. How It Works Internally

### The Timestamp Comparison Algorithm

For every requested target, Make performs a recursive depth-first search on the dependency tree:
1. Make queries the filesystem for the modification timestamp (`st_mtime` from `stat(2)`) of the target.
2. Make recursively traverses all prerequisites. If any prerequisite is itself a target of another rule, Make updates it first.
3. If the target does not exist, or if **any** prerequisite has an `mtime > target.mtime`, Make executes the recipe lines.
4. Each recipe line is executed in a **separate subshell** (`/bin/sh -c` on Linux, `cmd.exe` on Windows). Variables exported in one line do not persist into the next line unless chained with `&&` or escaped with a trailing backslash `\`.

### Automated Header Dependency Generation (`-MMD -MP`)

A classic Makefile flaw: if `math.c` includes `math.h`, and `math.h` is edited, a rule `math.o: math.c` will **not** trigger because Make only inspects the timestamp of `math.c`.

The modern solution uses GCC/Clang preprocessor dependency flags:
- `-MMD`: Instructs the compiler to output a `.d` dependency file listing all user headers included.
- `-MP`: Adds phony targets for each header file to prevent `make` errors if a header is deleted or renamed.

```makefile
CFLAGS += -MMD -MP

-include $(OBJS:.o=.d)
```

---

## 4. Syntax & Usage

### 1. Robust Production Makefile Template
```makefile
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?= -lm

SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

TARGET := $(BIN_DIR)/app
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

run: all
	./$(TARGET)

-include $(DEPS)
```

### 2. Immediate (`:=`) vs. Deferred (`=`) Variable Assignment
- `FOO = $(BAR)`: **Deferred/Recursive**. `$(BAR)` is evaluated every time `$(FOO)` is referenced. If `BAR` changes later in the file, `FOO` changes.
- `FOO := $(BAR)`: **Immediate/Simple**. `$(BAR)` is expanded once at the moment this line is parsed by Make. Highly recommended for build predictability and performance.

---

## 5. Practical Examples

### Target Source Program

```c
/* File: 01-languages/c/00-setup/examples/07-makefile-basics.c */
#include <stdio.h>

static int multiply(int a, int b) {
    return a * b;
}

int main(void) {
    printf("=== Makefile Target Execution ===\n");
    int x = 6;
    int y = 7;
    int product = multiply(x, y);
    printf("Result of %d * %d = %d\n", x, y, product);
    return 0;
}
```

#### Compilation & Execution Output
```bash
gcc -std=c11 -Wall -Wextra -O2 07-makefile-basics.c -o run_07.exe
./run_07.exe
```
Output:
```text
=== Makefile Target Execution ===
Result of 6 * 7 = 42
```

---

## 6. Best Practices

1. **Always Declare Phony Targets (`.PHONY`)**: Rules like `all`, `clean`, and `test` do not produce files named `all` or `clean`. If a file named `clean` is ever created on disk, `make clean` will report `clean is up to date` and refuse to run unless `.PHONY: clean` is specified.
2. **Use Order-Only Prerequisites for Directories**: Syntax: `$(TARGET): $(OBJS) | $(BIN_DIR)`. The pipe `|` indicates that `BIN_DIR` must exist, but updating `BIN_DIR`'s timestamp should not force `$(TARGET)` to re-link.
3. **Include Header Dependencies Dynamically**: Always add `-MMD -MP` and `-include $(DEPS)` to prevent stale header bugs.
4. **Prefer `:=` over `=` for File Lists**: Computing `$(wildcard ...)` recursively on every rule evaluation degrades Make execution speed.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Conforming Fix |
|---|---|---|
| Indenting recipes with 4 spaces instead of a Tab | POSIX and GNU Make require ASCII Tab (`\t`) for recipes. | Configure editor to preserve tabs in Makefiles (`.editorconfig: indent_style = tab`). |
| Multi-line recipe commands expecting shared state | Each line runs in a separate subshell. `cd src` on line 1 does not affect line 2. | Chain with `&&`: `cd src && ./build.sh` or use a single subshell via line continuation `\`. |
| Omitting `.PHONY` on utility targets | If a file or folder named `test` exists, `make test` will never execute. | Add `.PHONY: all clean test run`. |
| Putting libraries in `LDFLAGS` instead of `LDLIBS` | Linkers evaluate arguments left-to-right. Putting `-lm` before `.o` files drops symbols because unresolved symbol tables are empty when `-lm` is seen. | Place libraries at the end of the link line: `$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@`. |
| Writing duplicate rules for `.c` to `.o` | Redundant rules clutter the file and lead to conflicting flag definitions. | Use a single generic pattern rule: `%.o: %.c`. |
| Modifying source timestamps during build | Generating code inside recipes that changes prerequisites can cause infinite Make rebuild loops. | Ensure code generators only touch files when generated output actually changes. |

---

## 8. Performance & Security Notes

- **Parallel Compilation with `-j`**: Always invoke Make with `make -j$(nproc)` to compile independent `.o` targets concurrently across all CPU hardware threads.
- **Recipe Silent Execution (`@`)**: Prefixing a recipe line with `@` hides the command string from terminal output (`@rm -rf $(OBJ_DIR)`). While clean, avoid overusing `@` in compilation rules because hiding flags impedes build debugging.

---

## 9. Comparison / When to Use What

| Build System | Target Scale | Strengths | Trade-offs |
|---|---|---|---|
| **GNU Make** | Small to medium projects (1-100 files), embedded systems | Universal, zero dependencies, lightning fast startup | Complex syntax, fragile whitespace rules, manual dependency handling |
| **CMake** | Large multi-platform libraries, cross-platform enterprise | Generates Makefiles, Ninja, VS Solutions; package finding | Meta-build system (requires another generator); complex DSL |
| **Ninja** | High-performance backend builder | Extreme parallel dispatch speed, minimal overhead | Low-level machine syntax; not designed to be authored by humans |
| **Meson** | Modern Linux desktops, system utilities | Fast Python-based syntax, native Ninja generation | Requires Python and Ninja runtime environments |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must recipes in Makefiles be indented with a Tab character instead of spaces?</strong></summary>

Historical backward compatibility. When Stuart Feldman created Make in 1976 at Bell Labs, he used Tab as the delimiter for command lines. Changing it would have broken existing build scripts, so the Tab requirement remains encoded in the POSIX Make standard.
</details>

<details>
<summary><strong>2. What is the difference between automatic variables `$@`, `$<`, and `$^`?</strong></summary>

- `$@` evaluates to the file name of the rule's **target**.
- `$<` evaluates to the **first prerequisite** in the list (typically the `.c` file in `%.o: %.c`).
- `$^` evaluates to **all prerequisites** separated by spaces, with duplicate file names removed (typically all `.o` files during linking).
</details>

<details>
<summary><strong>3. Why is `.PHONY` required, and what happens if you omit it?</strong></summary>

`.PHONY` informs Make that the specified target does not correspond to a physical file on disk. If a directory or file with the same name (such as `clean` or `test`) exists in the directory, Make would check its modification timestamp, conclude it is up to date, and refuse to execute the recipe.
</details>

<details>
<summary><strong>4. How does GNU Make decide whether a target needs to be rebuilt?</strong></summary>

Make examines the filesystem modification timestamp (`mtime`) of the target and compares it against the modification timestamps of all its prerequisites. If the target does not exist, or if any prerequisite has a timestamp newer than the target, Make re-executes the recipe.
</details>

<details>
<summary><strong>5. What is the difference between `:=` and `=` in GNU Make variable assignment?</strong></summary>

`:=` is **simply expanded** (immediate): the right-hand side is evaluated once when the assignment line is parsed. `=` is **recursively expanded** (deferred): the right-hand side is re-evaluated every time the variable is referenced, reflecting whatever values its sub-variables hold at reference time.
</details>

<details>
<summary><strong>6. Why should library link flags (e.g. `-lpthread`, `-lm`) be placed after object files in the link recipe?</strong></summary>

Single-pass ELF linkers (like GNU `ld`) process input files strictly from left to right. When an object file (`main.o`) is processed, unresolved symbol references (e.g. `sin`) are recorded in an internal table. When the library (`-lm`) is subsequently encountered, the linker resolves those recorded symbols. If `-lm` appears before `main.o`, the unresolved symbol list is empty, and the library symbols are discarded.
</details>

<details>
<summary><strong>7. How do `-MMD` and `-MP` prevent header dependency bugs?</strong></summary>

`-MMD` tells the compiler to output a `.d` file containing the Make-compatible dependency rule `file.o: file.c header1.h header2.h`. `-MP` emits an empty rule for each header (e.g. `header1.h:`), which prevents Make from failing with a missing file error if a header is deleted or renamed.
</details>

<details>
<summary><strong>8. What is an "order-only prerequisite", and what is its syntax?</strong></summary>

An order-only prerequisite (specified after a pipe `|`, e.g. `$(TARGET): $(OBJS) | $(BIN_DIR)`) ensures that the prerequisite is built before the target, but changes to its modification timestamp will **not** trigger a rebuild of the target. This is used for output directory creation.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Two-File Compilation Makefile</strong></summary>

**Goal**: Write a Makefile that compiles `main.c` and `helper.c` into an executable `program` with `-Wall -Wextra`, and includes a `clean` target.

**Solution**:
```makefile
CC = gcc
CFLAGS = -Wall -Wextra

.PHONY: all clean

all: program

program: main.o helper.o
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o program
```
</details>

<details>
<summary><strong>Exercise 2 (Medium): Automatic Dependency Inclusion</strong></summary>

**Goal**: Extend the Makefile from Exercise 1 to automatically track header file changes without hardcoding header names, using `-MMD -MP` and `-include`.

**Solution**:
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -MMD -MP

SRCS = main.c helper.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

.PHONY: all clean

all: program

program: $(OBJS)
	$(CC) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(DEPS) program

-include $(DEPS)
```
</details>

<details>
<summary><strong>Exercise 3 (Hard): Multi-Directory Out-of-Tree Build Setup</strong></summary>

**Goal**: Create a production Makefile that places all generated `.o` and `.d` files in `build/` and the executable in `bin/`, ensuring directories are created dynamically via order-only prerequisites.

**Solution**:
```makefile
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -O2 -MMD -MP
CPPFLAGS := -Iinclude

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin

TARGET := $(BIN_DIR)/engine
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(DEPS)
```
</details>

---

## 12. Summary / Cheat Sheet

- **Tabs mandatory**: Never use spaces for recipe indents.
- `$@`: Target name; `$<`: First prerequisite; `$^`: All prerequisites.
- `.PHONY`: Mandatory for abstract targets (`clean`, `all`, `test`).
- `:=`: Immediate evaluation; `=`: Deferred evaluation.
- `-MMD -MP` + `-include $(DEPS)`: Robust automated header dependency tracking.
- Order-only prerequisites `| dir`: Guarantees directory creation without forcing redundant rebuilds.

---

## 13. Further Reading

- Robert Mecklenburg: *Managing Projects with GNU Make* (O'Reilly).
- GNU Make Official Manual (Free Software Foundation).
- John Graham-Cumming: *The GNU Make Book* (No Starch Press).

---

[← Previous: 06-debugging-with-gdb.md](./06-debugging-with-gdb.md) | [Section Index](./README.md) | [Next: 08-compiler-flags-and-warnings.md](./08-compiler-flags-and-warnings.md) →
