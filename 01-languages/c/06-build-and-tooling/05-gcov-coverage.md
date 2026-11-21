# Code Coverage with gcov and lcov

> Master code coverage analysis in C: compiler instrumentation flags (`--coverage`), trace file generation (`.gcno` and `.gcda`), statement and line coverage, branch coverage analysis, and generating visual HTML coverage reports with `lcov` and `genhtml`.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-make-in-depth.md](./01-make-in-depth.md), [02-cmake.md](./02-cmake.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| GCC 9+ / Clang 10+ / gcov / lcov 1.14+ |

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
Writing unit tests is essential for software quality, but how do you know if your test suite actually tests your code? You might have 500 passing tests, but if critical error-handling branches, edge-case validations, or boundary conditions are never executed during tests, catastrophic bugs will remain undiscovered until production.

**Code Coverage Analysis** measures exactly which lines, statements, functions, and conditional branches are executed during test runs. Using **`gcov`** (the GNU coverage testing tool) and **`lcov`** (the graphical front-end), C developers can instrument binaries to record execution frequency counters, identify untested dead code, and generate visual, color-coded HTML dashboards showing exact line-by-line coverage metrics.

---

## 2. Core Concepts
- **Line (Statement) Coverage**: The percentage of source code lines executed at least once during test execution.
- **Branch Coverage**: The percentage of conditional decision branches (e.g. both `true` and `false` paths of an `if` statement) evaluated during execution. Line coverage can be 100% while branch coverage is only 50%!
- **Function Coverage**: The percentage of declared functions called at least once.
- **Instrumentation Flags (`--coverage`)**: Passing `--coverage` to GCC/Clang during compilation and linking enables:
  - `-fprofile-arcs`: Instruments the binary with counters tracking branch transitions.
  - `-ftest-coverage`: Generates the static graph structure file (`.gcno`).
- **Coverage File Types**:
  - `.gcno` (Graph File): Created at **compile time**. Contains Basic Block flow graphs and line number mappings.
  - `.gcda` (Data File): Created at **runtime** when the program terminates. Contains raw execution counts.

---

## 3. How It Works Internally

### Basic Block Counter Instrumentation
When compiling with `--coverage`, the compiler breaks function code into **Basic Blocks** (sequences of instructions with a single entry and exit point) and injects 64-bit integer counter increments:

```
Source Code:
void process(int x) {
    if (x > 0) {
        handle_positive();
    } else {
        handle_non_positive();
    }
}

Compiler Instrumentation Representation:
process:
    __gcov_ctr[0]++;        // Block 0: Function entry
    cmp edi, 0
    jle .L_else
    __gcov_ctr[1]++;        // Block 1: True branch taken
    call handle_positive
    jmp .L_end
.L_else:
    __gcov_ctr[2]++;        // Block 2: False branch taken
    call handle_non_positive
.L_end:
    ret
```

1. **Compilation**: GCC emits machine code with counter increments and writes the control flow graph to `app.gcno`.
2. **Execution**: The test suite runs. When the program calls `exit()` or returns from `main()`, an internal `atexit` handler flushes the accumulated counter array into `app.gcda`.
3. **Report Generation**: `gcov` merges `app.gcno` and `app.gcda` with the original `app.c` source file, producing an annotated `app.c.gcov` showing execution counts for every line.

---

## 4. Syntax & Usage

### End-to-End Workflow with GCC and lcov
```bash
# 1. Compile with coverage instrumentation and no optimization
gcc -O0 --coverage -Wall src/math.c tests/test_math.c -o test_runner

# 2. Run the test suite (Generates .gcda files upon exit)
./test_runner

# 3. Capture coverage data with lcov
lcov --capture --directory . --output-file coverage.info

# 4. Filter out system headers (/usr/include) and test files
lcov --remove coverage.info '/usr/*' 'tests/*' --output-file coverage_filtered.info

# 5. Generate interactive HTML report
genhtml coverage_filtered.info --output-directory coverage_html/
```

### Understanding Annotated `.c.gcov` Output
When `gcov file.c` runs, it prefixes each line with its execution status:
- `-`: Non-executable line (comments, blank lines, type declarations, closing braces).
- `#####`: Executable line that was **never executed** (dead code or missing test case).
- `<number>`: Exact execution count (e.g. `1042` indicates the line was executed 1,042 times).

```text
        -:    0:Source:math.c
        -:    0:Graph:math.gcno
        -:    0:Data:math.gcda
        -:    1:#include "math.h"
        -:    2:
       15:    3:int safe_add(int a, int b) {
       15:    4:    if (a > 0 && b > 1000) {
    #####:    5:        return -1; /* UNTESTED ERROR BRANCH! */
        -:    6:    }
       15:    7:    return a + b;
        -:    8:}
```

---

## 5. Practical Examples

### Example 1: Code Coverage Reporting Simulation
```c
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint32_t total_lines;
    uint32_t executed_lines;
    uint32_t total_branches;
    uint32_t taken_branches;
} CoverageReport;

void print_coverage(const char *module, CoverageReport *cov) {
    double line_pct = (double)cov->executed_lines * 100.0 / cov->total_lines;
    double branch_pct = (double)cov->taken_branches * 100.0 / cov->total_branches;
    printf("Coverage Summary for '%s':\n", module);
    printf("  Line Coverage:   %.1f%% (%u/%u lines)\n", line_pct, cov->executed_lines, cov->total_lines);
    printf("  Branch Coverage: %.1f%% (%u/%u branches)\n", branch_pct, cov->taken_branches, cov->total_branches);
}

int main(void) {
    printf("Evaluating Code Coverage Reporting (gcov / lcov)...\n");
    CoverageReport rep = { 450, 420, 120, 108 };
    print_coverage("core_allocator.c", &rep);
    printf("Threshold Verification: Line coverage > 90%% -> PASS\n");
    return 0;
}
```

```text
Evaluating Code Coverage Reporting (gcov / lcov)...
Coverage Summary for 'core_allocator.c':
  Line Coverage:   93.3% (420/450 lines)
  Branch Coverage: 90.0% (108/120 branches)
Threshold Verification: Line coverage > 90% -> PASS
```

### Example 2: Line vs Branch Coverage Trap
Consider this function:
```c
int authenticate(const char *user, const char *pass) {
    if (user != NULL && pass != NULL) { // 2 conditional branches here!
        return check_credentials(user, pass);
    }
    return 0;
}
```
If your unit test only passes valid strings (`user = "admin", pass = "secret"`), line coverage will report **100%**! However, branch coverage will report only **50%**, because the scenarios where `user == NULL` or `pass == NULL` were never exercised.

---

## 6. Best Practices
1. **Always Compile at `-O0` for Coverage**: Compiler optimizations (`-O2`, inlining, loop unrolling) reorder, merge, or delete lines of code, producing distorted and inaccurate line-number mappings in gcov.
2. **Prioritize Branch Coverage Over Line Coverage**: Line coverage is easily manipulated and gives a false sense of security. Aim for $\ge 80\%$ branch coverage in safety-critical modules.
3. **Filter Out External & Test Code**: Always use `lcov --remove` to filter out `/usr/include/*`, external libraries (`vendor/`), and the unit test files themselves, ensuring the report reflects only actual application code.
4. **Clean Coverage Data Before Test Runs**: `.gcda` files accumulate execution counts across runs. Run `lcov --zerocounters` or `rm -f *.gcda` before each test suite execution to prevent stale data.
5. **Enforce Coverage Thresholds in CI**: Integrate coverage tools into pull request gates (e.g. failing builds if total line coverage drops below 85%).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Compiling with `-O2` or `-O3` | Optimizations eliminate lines and inline functions, corrupting gcov report accuracy | Always compile coverage builds with `-O0 -g` |
| Forgetting to link with `--coverage` | Runtime counter symbols (`__gcov_*`) fail to resolve during link step | Pass `--coverage` to both compiler and linker |
| Aborting via `_exit()` or crash | Standard `atexit` handler does not run; `.gcda` files are not flushed to disk | Ensure test process exits via standard `return` or `exit()`, or call `__gcov_dump()` |
| Not resetting counters between runs | New test runs add to existing `.gcda` numbers, showing inflated historical coverage | Clean `.gcda` files before running tests |
| 100% line coverage complacency | Error branches or NULL checks remain unexercised | Check and enforce branch coverage (`lcov --rc lcov_branch_coverage=1`) |
| Measuring test files in coverage metrics | Including test assertions inflates total coverage percentage artificially | Exclude test directories using `lcov --remove` |

---

## 8. Performance & Security Notes
- **Runtime Instrumentation Overhead**: Binaries compiled with `--coverage` run approximately $20\%	ext{--}40\%$ slower and consume additional memory for counter buffers. **Never deploy coverage-instrumented binaries to production environments!**
- **Manual Counter Flushing (`__gcov_dump`)**: In daemons or embedded systems that never terminate normally (infinite event loops), call `__gcov_dump()` explicitly inside a `SIGUSR1` signal handler to flush current coverage data to disk on demand.

---

## 9. Comparison / When to Use What

| Tool | Focus | Output Format | Best Use Case |
| :--- | :--- | :--- | :--- |
| **gcov** | Core command-line tool | Annotated text (`.c.gcov`) | Quick terminal inspection of single files |
| **lcov** | Front-end aggregator | Structured `.info` and HTML | Full-repository dashboards, CI summaries |
| **llvm-cov / gcovr**| LLVM native / Python | Cobertura XML, JSON, HTML | Cloud CI dashboards (Codecov, Coveralls) |

---

## 10. Interview Questions

<details>
<summary>1. What is the difference between line coverage and branch coverage?</summary>
Line (statement) coverage measures whether each line of code was executed at least once. Branch coverage measures whether every possible decision path in control-flow structures (such as `if`, `else`, `switch`, `while`) was traversed. It is possible to achieve 100% line coverage while testing only 50% of the branches if conditional statements with multiple boolean expressions (`if (A && B)`) are never evaluated in both true and false states.
</details>

<details>
<summary>2. What do `.gcno` and `.gcda` files contain?</summary>
- `.gcno` (Graph Note): Generated at compile-time when `-ftest-coverage` is specified. It contains the static Control Flow Graph (CFG) of basic blocks and maps basic blocks to source file line numbers.
- `.gcda` (Graph Data): Generated at runtime when the instrumented binary executes and terminates. It contains the actual transition counts for every arc in the basic block graph.
</details>

<details>
<summary>3. Why must code coverage builds be compiled with optimization disabled (`-O0`)?</summary>
Compilers performing optimizations (`-O2`) reorder instructions, inline functions, eliminate dead code branches, and merge common subexpressions. Because these optimizations dissociate machine code from the original source lines, running coverage on optimized code causes gcov to report confusing or impossible line execution numbers.
</details>

<details>
<summary>4. How does `gcov` record coverage in programs that do not exit normally (e.g. daemons)?</summary>
By default, coverage counters are flushed to disk by an internal `atexit` function registered by the C runtime library. If a daemon runs in an infinite loop or terminates via `_exit()` or `kill -9`, the counters are never written. Applications can solve this by declaring `extern void __gcov_dump(void);` and calling it periodically or inside a signal handler (e.g. `SIGUSR1`).
</details>

<details>
<summary>5. How can you enable branch coverage in `lcov`?</summary>
By default, `lcov` disables branch coverage to reduce clutter. It can be enabled by passing `--rc lcov_branch_coverage=1` to both `lcov` and `genhtml`.
</details>

<details>
<summary>6. What does `lcov --zerocounters` do?</summary>
It resets all `.gcda` counter files in the specified directory to zero without deleting the static `.gcno` graph files, preparing the workspace for a fresh test run.
</details>

<details>
<summary>7. What is Modified Condition/Decision Coverage (MC/DC)?</summary>
MC/DC is a rigorous coverage standard required in aviation and safety-critical systems (DO-178C). It requires demonstrating that every condition in a decision can independently affect the decision outcome, going beyond standard branch coverage.
</details>

<details>
<summary>8. How do you integrate coverage generation into CMake?</summary>
```cmake
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()
```
</details>

---

## 11. Practice Exercises

### Easy
1. **Compile with Coverage**: Compile a 20-line C program with `gcc --coverage -O0` and verify that `.gcno` is generated.
2. **Execute and Verify .gcda**: Run the compiled binary and verify that `.gcda` appears on disk.
3. **Run gcov**: Run `gcov main.c` and inspect the generated `main.c.gcov` text file.

<details>
<summary>Solutions (Easy)</summary>

```bash
# Exercise 1, 2, 3
gcc -O0 --coverage main.c -o test_app
./test_app
gcov main.c
cat main.c.gcov
```
</details>

### Medium
4. **Branch Coverage Inspection**: Write a function with nested `if-else` branches. Write tests that achieve 100% line coverage but only 50% branch coverage, verifying the difference with `gcov -b`.
5. **lcov HTML Generation Script**: Write a shell script `generate_coverage.sh` that cleans counters, runs tests, captures data with `lcov`, filters out `/usr/*`, and runs `genhtml`.
6. **Coverage Threshold Checker**: Write a Python or shell script that parses `coverage.info` and fails with exit code 1 if total coverage is $< 80\%$.

<details>
<summary>Solutions (Medium)</summary>

```bash
#!/bin/bash
# Exercise 5: generate_coverage.sh
set -e

# Zero old counters
lcov --directory . --zerocounters

# Run test binary
./build/tests/unit_tests

# Capture
lcov --capture --directory . --output-file coverage.raw.info --rc lcov_branch_coverage=1

# Filter
lcov --remove coverage.raw.info '/usr/*' 'tests/*' --output-file coverage.info --rc lcov_branch_coverage=1

# Generate HTML
genhtml coverage.info --output-directory coverage_html --branch-coverage

echo "HTML report generated at coverage_html/index.html"
```
</details>

### Hard
7. **Complete Makefile with Coverage Target**: Write a production Makefile featuring a `make coverage` target that builds with `--coverage`, runs unit tests, generates an HTML report, and opens the index page in a browser.
8. **Daemon On-Demand Coverage Flusher**: Implement a long-running daemon that catches `SIGUSR2`, calls `__gcov_dump()`, and resets counters with `__gcov_reset()` to permit taking live coverage snapshots without restarting the daemon.
9. **GitHub Actions Coverage Gate**: Author a GitHub Actions workflow that runs `lcov`, extracts the line coverage percentage, posts a coverage summary table to pull request comments, and fails the build if coverage drops.

<details>
<summary>Solutions (Hard)</summary>

```c
/* Exercise 8: Live Coverage Dumper for Long-Running Process */
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

extern void __gcov_dump(void);
extern void __gcov_reset(void);

void sigusr2_handler(int sig) {
    (void)sig;
    const char msg[] = "Flushing coverage counters to disk...\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    __gcov_dump();
    __gcov_reset();
}

void setup_coverage_signal(void) {
    struct sigaction sa = {0};
    sa.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);
}
```
```yaml
# Exercise 9: GitHub Actions Automated Coverage Gate (.github/workflows/coverage.yml)
name: Coverage Gate
on: [push, pull_request]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install Tools
        run: sudo apt-get update && sudo apt-get install -y lcov gcc
      - name: Build & Run Tests
        run: |
          gcc -O0 --coverage -Wall src/*.c tests/*.c -o runner
          ./runner
      - name: Generate Coverage Report
        run: |
          lcov --capture --directory . --output-file coverage.info --rc lcov_branch_coverage=1
          lcov --remove coverage.info '/usr/*' 'tests/*' --output-file coverage.filtered.info --rc lcov_branch_coverage=1
          genhtml coverage.filtered.info --output-directory coverage_html
      - name: Verify Threshold (> 85%)
        run: |
          COVERAGE=$(lcov --summary coverage.filtered.info | grep lines | awk '{print $2}' | tr -d '%')
          echo "Line Coverage: $COVERAGE%"
          awk -v cov="$COVERAGE" 'BEGIN { if (cov < 85.0) exit 1; else exit 0; }'
```
</details>

---

## 12. Summary / Cheat Sheet
- **Flags**: `gcc -O0 --coverage src.c -o app` (both compile and link).
- **Files**: `.gcno` (compile graph), `.gcda` (runtime data).
- **gcov**: `gcov -b src.c` (generates `src.c.gcov` with branch stats).
- **lcov capture**: `lcov --capture --directory . -o cov.info`.
- **lcov filter**: `lcov --remove cov.info '/usr/*' 'tests/*' -o cov.info`.
- **genhtml**: `genhtml cov.info -o html_report/`.

---

## 13. Further Reading
- GCC Manual: Section on Gcov and Coverage Testing Options.
- Linux Test Project: LCOV Readme and User Guide.
- IEEE Standard 1008: Software Unit Testing.

---

[← Previous Note: Code Formatting with Clang-Format](./04-clang-format.md) | [Module Index](README.md) | [Next Note: Git Workflow for C Projects](./06-git-workflow-for-c-projects.md)
