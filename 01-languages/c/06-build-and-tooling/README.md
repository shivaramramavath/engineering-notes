# Module 06: Build & Tooling

> Master modern C build engineering, automation, and developer tooling: multi-target GNU Makefiles with automatic header dependency generation (`-MMD`), modern CMake 3.x target-based architectures, automated static analysis (`clang-tidy`, `cppcheck`), code formatting pipelines (`clang-format`), code coverage metrics (`gcov`, `lcov`), Git branching and release versioning for C, cross-compilation toolchains, `pkg-config` dependency resolution, Continuous Integration (CI) sanitizer matrix pipelines, Doxygen API documentation, and Debug vs Release binary optimization profiles.

| Metadata | Details |
| :--- | :--- |
| **Module Code** | `06-build-and-tooling` |
| **Prerequisites** | [00-setup/07-makefile-basics.md](../00-setup/07-makefile-basics.md), [04-advanced/04-linking-static-vs-shared-libraries.md](../04-advanced/04-linking-static-vs-shared-libraries.md) |
| **Duration** | ~25 hours |
| **Status** | Complete |
| **Standard Baseline**| POSIX.1-2008 / GNU Make 4.x / CMake 3.20+ / Clang 12+ / GCC 9+ |

---

## Overview & Learning Outcomes

Professional C software development extends far beyond writing source code: scalable software engineering requires industrial-grade build systems, automated quality assurance gates, deterministic dependency resolution, and repeatable cross-platform release pipelines.

In this module, you will master:
1. **Advanced GNU Make Architecture**: Dependency graphs, automatic header dependency tracking (`-MMD -MP`), recursive vs non-recursive Makefiles, pattern rules, and phony target contracts.
2. **Modern CMake 3.x**: Target-centric CMake (`target_include_directories`, `target_link_libraries`, `target_compile_definitions`), interface visibility (`PUBLIC`, `PRIVATE`, `INTERFACE`), generator expressions, and package configuration export.
3. **Automated Static Analysis**: Integrating `clang-tidy` and `cppcheck` into local pre-commit hooks and CI pipelines to enforce CERT C and MISRA guidelines and detect subtle memory bugs before compilation.
4. **Code Quality & Formatting Automation**: Enforcing organizational style standards automatically with `.clang-format`, automating pre-commit verification, and eliminating style debates in code reviews.
5. **Code Coverage Analysis**: Instrumenting binaries with `gcc --coverage`, generating execution trace files (`.gcno`, `.gcda`), and rendering HTML line/branch coverage dashboards with `gcov` and `lcov`.
6. **Cross-Compilation & Toolchains**: Building binaries for embedded ARM/MIPS/RISC-V targets and cross-compiling from Linux to Windows with MinGW toolchains (`sysroot`, cross-compilers, tuple prefixes).
7. **Package Management & Dependencies**: Discovering and linking system libraries using `pkg-config` and authoring `.pc` specification files for distributed C libraries.
8. **Continuous Integration & Sanitizer Matrix**: Designing GitHub Actions and GitLab CI workflows that run automated test matrices across GCC/Clang with ASan, UBSan, and TSan enabled on every commit.

---

## Module Table of Contents

| Note | Title | Level | Status | Focus Areas |
| :--- | :--- | :--- | :--- | :--- |
| **01** | [GNU Make in Depth](./01-make-in-depth.md) | Tooling | Complete | Dependency graphs, `-MMD -MP` auto-dependencies, pattern rules, multi-target Makefiles |
| **02** | [Modern CMake](./02-cmake.md) | Tooling | Complete | Target-based architecture, `PUBLIC`/`PRIVATE`/`INTERFACE`, generator expressions, ctest |
| **03** | [Static Analysis: Clang-Tidy & Cppcheck](./03-static-analysis-clang-tidy-cppcheck.md) | Tooling | Complete | Bug-prone patterns, CERT C checks, compile_commands.json, automated CI linting |
| **04** | [Code Formatting with Clang-Format](./04-clang-format.md) | Tooling | Complete | `.clang-format` specification, style inheritance, pre-commit hooks, CI style gates |
| **05** | [Code Coverage: gcov & lcov](./05-gcov-coverage.md) | Tooling | Complete | Coverage instrumentation (`--coverage`), line/branch coverage metrics, lcov HTML reports |
| **06** | [Git Workflow for C Projects](./06-git-workflow-for-c-projects.md) | Tooling | Complete | `.gitignore` for C artifacts, submodules, Semantic Versioning (SemVer), Git tags |
| **07** | [Cross-Compilation Intro](./07-cross-compilation-intro.md) | Tooling | Complete | Target tuples (`x86_64-w64-mingw32`, `arm-linux-gnueabihf`), sysroots, CMake toolchains |
| **08** | [pkg-config and Library Dependencies](./08-pkg-config-and-dependencies.md) | Tooling | Planned | Querying `cflags` and `libs`, authoring custom `.pc` metadata files, CMake `find_package` |
| **09** | [Continuous Integration for C](./09-continuous-integration-for-c.md) | Tooling | Complete | GitHub Actions / GitLab CI matrices, multi-compiler testing, caching build artifacts |
| **10** | [Doxygen API Documentation](./10-doxygen-documentation.md) | Tooling | Complete | Javadoc/Doxygen comment syntax (`@param`, `@return`), Doxyfile, generating HTML/PDF docs |
| **11** | [Debug vs Release Builds & Sanitizer CI](./11-debug-vs-release-builds-and-sanitizer-ci.md) | Tooling | Planned | `-O0 -g3` vs `-O3 -DNDEBUG`, LTO, ASan/UBSan/TSan multi-target build matrices |

---

## Suggested Study Order
1. **Core Build Automation**: Master [Make in Depth](./01-make-in-depth.md) and [Modern CMake](./02-cmake.md) to build scalable, incremental compilation pipelines.
2. **Quality Gates & Code Cleanliness**: Integrate [Clang-Format](./04-clang-format.md) and [Static Analysis](./03-static-analysis-clang-tidy-cppcheck.md).
3. **Verification & Coverage**: Instrument test suites with [Code Coverage: gcov & lcov](./05-gcov-coverage.md).
4. **Distribution & External Toolchains**: Learn [Cross-Compilation](./07-cross-compilation-intro.md) and [pkg-config](./08-pkg-config-and-dependencies.md).
5. **Production Pipelines**: Finish with [Git Workflow](./06-git-workflow-for-c-projects.md), [Continuous Integration](./09-continuous-integration-for-c.md), and [Debug vs Release Builds](./11-debug-vs-release-builds-and-sanitizer-ci.md).

---

[← Previous Module: Systems Programming](../05-systems-programming/README.md) | [C Language Track Index](../README.md) | [Next Module: Testing](../07-testing/README.md)
