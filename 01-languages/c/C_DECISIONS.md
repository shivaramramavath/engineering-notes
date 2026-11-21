# C Curriculum Technical Decisions Log (C_DECISIONS.md)

> This document records architectural, curriculum, and implementation decisions made during the autonomous overhaul of `01-languages/c/`.

---

## Decision 1: Host Compiler & Verification Strategy
- **Context:** The local host environment provides MinGW.org GCC 6.3.0 (32-bit Windows). It supports `-std=c11` but lacks `-std=c17`/`-std=c23` flags and ASan runtime libraries (`-lasan`).
- **Decision:** 
  1. Compile and run every code example locally using `gcc -std=c11 -Wall -Wextra -Wpedantic -Werror` to guarantee real, uninvented program outputs.
  2. Write all code to be strictly conforming to ISO C11/C17/C23 standards.
  3. Where C23-specific syntax (like `#embed` or `<stdckdint.h>`) is introduced, encapsulate it in dedicated C23 callouts with standard `#if __STDC_VERSION__ >= 202311L` feature-test guards so code remains 100% portable across compilers.
- **Alternatives Considered:** Refusing to compile locally without GCC 14+; rejected because local automated execution is necessary to verify real outputs.

---

## Decision 2: Resolution of Missing Slot `03-intermediate/10`
- **Context:** `03-intermediate/` contained notes 01–09, jumped over 10, and continued at `11-standard-library.md`. Note 09 had a broken link pointing forward to 10.
- **Decision:** Populate slot `10` with **`10-undefined-behavior-and-sanitizers.md`**.
- **Alternatives Considered:** Renumbering `11-standard-library.md` to `10-standard-library.md`; rejected because renumbering breaks existing references and leaves UB/sanitizers uncovered in the intermediate track.

---

## Decision 3: Preservation of Existing File Names
- **Context:** The repository-wide organisation plan (`ORGANISATION_PLAN.md`) proposed broad renames across the whole repo, but has not yet been applied to the rest of the repository.
- **Decision:** Preserve all 40 existing C note file names in place. Do not rename existing notes. All new topics are appended with clean sequential two-digit prefixes (`NN-topic-name.md`).
- **Alternatives Considered:** Renaming all existing C notes; rejected to avoid breaking references before the global organisation plan is applied.

---

## Decision 4: Self-Contained Project Architecture (`11-projects/`)
- **Context:** The prompt specifies 14 real-world C projects (e.g. tiny-shell, http-server, memory-allocator, json-parser, etc.).
- **Decision:** Each project will be constructed as a complete, self-contained module containing:
  - `README.md` (Architecture, design trade-offs, usage, API contract)
  - `include/` (Public interface header files)
  - `src/` (Clean, modular C implementation without global state)
  - `tests/` (Automated unit/integration tests)
  - `Makefile` (Automated build, test, clean targets)
- **Alternatives Considered:** Providing monolithic single-file projects; rejected because multi-file modularity is essential for production C engineering.
