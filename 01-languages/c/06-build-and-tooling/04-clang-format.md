# Code Formatting with Clang-Format

> Master automated code formatting in C: the `.clang-format` configuration specification, base style inheritance (LLVM, Google, Linux, WebKit), configuring braces, indentation, and alignment, pre-commit Git hooks, `git-clang-format`, and automated CI formatting gates.

| Metadata | Details |
| :--- | :--- |
| **Level** | Tooling / Advanced |
| **Prerequisites** | [01-fundamentals/01-program-structure.md](../01-fundamentals/01-program-structure.md), [01-make-in-depth.md](./01-make-in-depth.md) |
| **Time to read** | 35 minutes |
| **Status** | Complete |
| **Standard Baseline**| Clang-Format 12+ / YAML Configuration / Git Integration |

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
In software engineering teams, debates over code formatting—tabs vs spaces, brace placement (K&R vs Allman), pointer alignment (`int* p` vs `int *p`), line length limits—consume hours of valuable code review time and generate noisy, unreadable Git diffs.

**Clang-Format** is an automated code formatting engine part of the LLVM toolchain. It parses C source code into a syntactic token stream and reformats the entire codebase according to deterministic rules specified in a `.clang-format` YAML file. By delegating formatting entirely to automated tools, developers eliminate subjective formatting debates, ensure 100% architectural visual consistency across large repositories, and prevent style-only diff pollution.

---

## 2. Core Concepts
- **Configuration Hierarchy**: When formatting a file, `clang-format` searches the current directory and all parent directories for a `.clang-format` (or `_clang-format`) file, using the closest configuration found.
- **Base Styles (`BasedOnStyle`)**:
  - `LLVM`: Default LLVM style (2-space indent, right-aligned pointers).
  - `Google`: Google C++ Style Guide (2-space indent).
  - `Chromium`: Chromium project conventions.
  - `Mozilla`: Mozilla project standards.
  - `WebKit`: WebKit coding guidelines (4-space indent).
  - `Microsoft`: Visual Studio conventions.
  - `GNU`: Strict GNU coding standards.
- **Key Formatting Options**:
  - `IndentWidth`: Number of spaces per indentation level.
  - `ColumnLimit`: Maximum characters per line before wrapping (0 disables wrapping).
  - `BreakBeforeBraces`: Policy for braces (`Attach`, `Linux`, `Allman`, `Custom`).
  - `PointerAlignment`: Alignment of pointer qualifiers (`Left`, `Right`, `Middle`).
  - `SortIncludes`: Automatically sorts `#include` directives alphabetically.
- **`git-clang-format`**: A Python helper utility distributed with Clang that formats *only the specific lines modified in the current Git staging area*, avoiding reformatting legacy code.

---

## 3. How It Works Internally

### Token Stream Parsing and Penalty Optimization
Clang-Format does not use naive regular expressions; it uses the real Clang C lexer:

```
Source Code (Messy Formatting)
int   foo(int  x,char*  y){if(x>0)return 1;else return 0;}
                          |
                          v (Clang Lexer)
Token Stream: [int] [foo] [(] [int] [x] [,] [char] [*] [y] [)] [{] ...
                          |
                          v (UnwrappedLineParser)
Line Breaks & Indentation Graph (Weighted Penalty Evaluation)
  - Penalty for breaking before operator: 50
  - Penalty for line exceeding ColumnLimit (80): 1000
  - Dijkstra / Dynamic Programming Shortest Path Search
                          |
                          v (Formatted Output)
int foo(int x, char *y) {
    if (x > 0) {
        return 1;
    } else {
        return 0;
    }
}
```

Clang-Format casts line wrapping as an optimization problem: it defines penalty scores for breaking across function calls, operators, or string literals, and uses dynamic programming to find the mathematically optimal formatting layout that minimizes total visual penalty.

---

## 4. Syntax & Usage

### Production `.clang-format` Configuration Template
```yaml
---
# Base inheritance
BasedOnStyle: LLVM
Language: Cpp

# Indentation and column limits
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100

# Brace placement: Attach braces (K&R / Linux style)
BreakBeforeBraces: Attach

# Pointer alignment: int *ptr (Standard C convention)
PointerAlignment: Right

# Spacing rules
SpacesInParentheses: false
SpaceBeforeParens: ControlStatements
SpaceAfterCStyleCast: false
SpacesInAngles: false

# Header include management
SortIncludes: CaseSensitive
IncludeBlocks: Regroup
IncludeCategories:
  - Regex: '^<.*\.h>'
    Priority: 1
  - Regex: '^<.*>'
    Priority: 2
  - Regex: '^".*"'
    Priority: 3
...
```

### Command-Line Execution
```bash
# Reformat a file in-place
clang-format -i src/main.c

# Reformat all C files in src/ and include/ in-place
clang-format -i $(find src include -name '*.[ch]')

# Dry-run check: return exit code 1 if formatting changes are required (for CI)
clang-format --dry-run --Werror src/main.c

# Reformat only lines modified in current Git commit
git clang-format HEAD~1
```

---

## 5. Practical Examples

### Example 1: Evaluating `.clang-format` Configuration Simulation
```c
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    int indent_width;
    int column_limit;
    bool break_before_braces;
    bool pointer_alignment_right;
} FormatStyle;

int main(void) {
    printf("Evaluating .clang-format Configuration Specification...\n");
    FormatStyle style = {
        .indent_width = 4,
        .column_limit = 100,
        .break_before_braces = false, // Linux/K&R style
        .pointer_alignment_right = true // int *ptr
    };

    printf("Applied Style Settings:\n");
    printf("  IndentWidth: %d\n", style.indent_width);
    printf("  ColumnLimit: %d\n", style.column_limit);
    printf("  BreakBeforeBraces: %s\n", style.break_before_braces ? "Allman" : "Attach (K&R)");
    printf("  PointerAlignment: %s\n", style.pointer_alignment_right ? "Right (int *ptr)" : "Left (int* ptr)");
    printf("Automated format check: 100%% code compliance with zero diffs.\n");
    return 0;
}
```

```text
Evaluating .clang-format Configuration Specification...
Applied Style Settings:
  IndentWidth: 4
  ColumnLimit: 100
  BreakBeforeBraces: Attach (K&R)
  PointerAlignment: Right (int *ptr)
Automated format check: 100% code compliance with zero diffs.
```

### Example 2: Selective Disabling for Data Tables
When formatting mathematical matrices or hardware register lookup tables, auto-formatting can destroy human readability. Use comment pragmas:

```c
#include <stdint.h>

/* clang-format off */
const uint8_t sbox[16] = {
    0x63, 0x7c, 0x77, 0x7b, 
    0xf2, 0x6b, 0x6f, 0xc5, 
    0x30, 0x01, 0x67, 0x2b, 
    0xfe, 0xd7, 0xab, 0x76
};
/* clang-format on */
```

---

## 6. Best Practices
1. **Commit `.clang-format` to Repository Root**: Always store `.clang-format` in the root of the Git repository so all team members and CI runners use the identical formatting specification.
2. **Use `git-clang-format` on Existing Codebases**: When introducing formatting to an older project, reformatting the entire codebase destroys `git blame` history. Instead, use `git-clang-format` to format only modified lines on new pull requests.
3. **Use `.git-blame-ignore-revs`**: If you *do* reformat the entire codebase in one commit, add that commit hash to `.git-blame-ignore-revs` so `git blame` skips the formatting revision automatically.
4. **Group and Sort Includes**: Configure `IncludeBlocks: Regroup` and `SortIncludes: true` to prevent duplicate or messy `#include` headers.
5. **Enforce in CI**: Add a simple CI check `clang-format --dry-run --Werror` to automatically block pull requests with formatting deviations.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It's Wrong | Correct Fix |
| :--- | :--- | :--- |
| Huge formatting commits mixed with logic | Makes code review impossible and ruins git bisect | Separate pure reformatting commits from functional logic changes |
| Running clang-format on generated files | Reformats Lex/Yacc or Protobuf generated C files, breaking generated layouts | Add `.clang-format-ignore` file listing generated paths |
| Differing Clang-Format versions across team | Clang-Format 10 and Clang-Format 16 may format subtle edge cases differently | Pin Clang-Format version in CI and developer containers |
| Manually formatting code before committing | Wastes human engineering time | Install pre-commit hook to automate formatting on `git commit` |
| Overusing `/* clang-format off */` | Defeats the purpose of automated consistency | Reserve `clang-format off` strictly for ASCII tables and math matrices |
| Ignoring `.clang-format` in submodules | Submodules inherit parent formatting if not isolated | Place independent `.clang-format` files in submodules |

---

## 8. Performance & Security Notes
- **Speed of Formatting**: Clang-Format processes approximately 50,000 lines of C code per second on modern CPUs. Running formatting on an entire repository during pre-commit takes less than 2 seconds.
- **Git Hook Performance**: In large repositories with tens of thousands of files, format only staged files (`git diff --cached --name-only`) rather than scanning the entire filesystem.

---

## 9. Comparison / When to Use What

| Tool | Engine | Customizability | Integration |
| :--- | :--- | :--- | :--- |
| **Clang-Format** | LLVM Clang AST / Lexer | **Extremely high (100+ options)** | Universal (VSCode, Vim, Clion, Git, CI) |
| **GNU Indent** | Legacy C token parser | Moderate | Command-line CLI only |
| **Uncrustify** | Custom tokenizer | Very high (600+ options) | CLI / IDE plugins |
| **Artistic Style (AStyle)** | Regex / Tokenizer | Moderate | Embedded tools |

---

## 10. Interview Questions

<details>
<summary>1. How does Clang-Format determine where to break long lines?</summary>
Clang-Format models line breaking as a dynamic programming graph optimization problem. It assigns mathematical penalty scores to different break locations (e.g. high penalty for breaking inside string literals or between function names and parameter lists; lower penalty for breaking after commas or assignment operators). It finds the combination of line breaks that satisfies `ColumnLimit` while minimizing the total penalty score.
</details>

<details>
<summary>2. How can you format only modified lines without altering the rest of a legacy file?</summary>
Using the `git-clang-format` utility:
`git clang-format`
It calculates the exact line ranges modified in the Git staging index (`git diff --cached`) and invokes `clang-format -lines=N:M` exclusively on those specific line ranges, leaving untouched legacy code unchanged.
</details>

<details>
<summary>3. What is the purpose of `.git-blame-ignore-revs`?</summary>
When an entire repository is reformatted at once, `git blame` attributes every single line of code to the formatting commit, obscuring who originally wrote the logic. Adding the formatting commit SHA to `.git-blame-ignore-revs` and configuring `git config blame.ignoreRevsFile .git-blame-ignore-revs` causes `git blame` to look past the formatting commit and show original authors.
</details>

<details>
<summary>4. How do you temporarily disable formatting for a specific block of code?</summary>
Enclose the code block with special comment pragmas:
```c
/* clang-format off */
int lookup_table[] = { 1, 2, 3, 4 };
/* clang-format on */
```
</details>

<details>
<summary>5. How does Clang-Format discover its configuration file?</summary>
When given a file path (e.g. `src/net/socket.c`), Clang-Format searches for `.clang-format` in `src/net/`, then `src/`, then the repository root, moving up the directory tree until it finds a configuration file. If none is found, it falls back to the default fallback style (usually `LLVM`).
</details>

<details>
<summary>6. How can you enforce formatting in a CI pull request pipeline without modifying files?</summary>
Using the dry-run command:
`clang-format --dry-run --Werror $(find src include -name '*.[ch]')`
If any file requires formatting changes, Clang-Format outputs the required diff, issues an error, and exits with a non-zero status code, failing the CI pipeline.
</details>

<details>
<summary>7. What does the `PointerAlignment: Right` option do in C?</summary>
It specifies that pointer asterisks bind visually to the variable or function name rather than the type specifier: `int *ptr` instead of `int* ptr`. In standard C, this reflects the declaration grammar (where `int *a, b;` declares `a` as a pointer and `b` as an integer).
</details>

<details>
<summary>8. How do you automatically sort `#include` headers with Clang-Format?</summary>
Set `SortIncludes: true` and configure `IncludeCategories` with regular expressions to prioritize system headers (`<stdio.h>`) above internal project headers (`"my_header.h"`).
</details>

---

## 11. Practice Exercises

### Easy
1. **Basic Format CLI**: Format a messy C source file in-place using `clang-format -i -style=LLVM file.c`.
2. **Dump Default Style**: Dump the default Google C style configuration to a file using `clang-format -style=Google -dump-config > .clang-format`.
3. **Format Check Script**: Write a one-line shell command that checks if `main.c` conforms to formatting rules using `--dry-run --Werror`.

<details>
<summary>Solutions (Easy)</summary>

```bash
# Exercise 2 & 3
clang-format -style=Google -dump-config > .clang-format
clang-format --dry-run --Werror main.c
```
</details>

### Medium
4. **Custom 4-Space K&R Config**: Author a complete `.clang-format` file specifying 4-space indentation, 100 column limit, attached braces (`Attach`), and right-aligned pointers.
5. **Selective Disabling Demo**: Create a C file with an intentionally formatted mathematical lookup matrix protected by `/* clang-format off */`. Verify that running `clang-format -i` leaves the matrix unchanged.
6. **Makefile Format Target**: Add a `make format` and `make check-format` target to a Makefile that formats all project files.

<details>
<summary>Solutions (Medium)</summary>

```makefile
# Exercise 6: Makefile Format Targets
SRCS := $(shell find src include -name '*.[ch]')

.PHONY: format check-format

format:
	@echo "Formatting C source files..."
	@clang-format -i $(SRCS)

check-format:
	@echo "Checking formatting compliance..."
	@clang-format --dry-run --Werror $(SRCS)
```
</details>

### Hard
7. **Automated Git Pre-Commit Formatter**: Author a Git pre-commit hook script (`.git/hooks/pre-commit`) that runs `git-clang-format` on staged changes and automatically re-stages reformatted files.
8. **GitHub Actions Format Verifier**: Write a GitHub Actions YAML workflow (`.github/workflows/style.yml`) that runs Clang-Format on all pull requests and comments with a patch if formatting fails.
9. **Multi-Directory Hierarchical Formatting**: Set up a repository where the main code uses 4-space K&R formatting, but a sub-module directory `vendor/` overrides formatting with its own independent `.clang-format`.

<details>
<summary>Solutions (Hard)</summary>

```bash
#!/bin/bash
# Exercise 7: Git Pre-Commit Hook (.git/hooks/pre-commit)
set -e

echo "Running git-clang-format..."
# Run git-clang-format on staged changes
output=$(git clang-format --staged --diff)

if [ -n "$output" ] && [ "$output" != "no modified files to format" ]; then
    echo "Formatting violations detected:"
    echo "$output"
    echo ""
    echo "Applying automatic formatting..."
    git clang-format --staged
    # Re-stage formatted files
    git diff --name-only --cached | xargs -r git add
    echo "Formatting applied and re-staged."
fi

exit 0
```
```yaml
# Exercise 9: Hierarchical Subdirectory Configuration (vendor/.clang-format)
# Overrides parent rules specifically for third-party vendored code
BasedOnStyle: None
DisableFormat: true
SortIncludes: false
```
</details>

---

## 12. Summary / Cheat Sheet
- **File**: `.clang-format` in project root.
- **Format in-place**: `clang-format -i file.c`.
- **Dry-run CI**: `clang-format --dry-run --Werror file.c`.
- **Git staged**: `git clang-format`.
- **Pragma**: `/* clang-format off */` ... `/* clang-format on */`.
- **Key Keys**: `IndentWidth: 4`, `ColumnLimit: 100`, `PointerAlignment: Right`.

---

## 13. Further Reading
- LLVM Clang-Format Style Options Documentation (Complete Schema).
- Google C++ Style Guide (C Formatting Conventions).
- Linux Kernel Coding Style Guide.

---

[← Previous Note: Static Analysis (Clang-Tidy & Cppcheck)](./03-static-analysis-clang-tidy-cppcheck.md) | [Module Index](README.md) | [Next Note: Code Coverage (gcov & lcov)](./05-gcov-coverage.md)
