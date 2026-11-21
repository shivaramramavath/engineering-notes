# Java Track Architectural Decisions Record (ADR)

> **Status:** Active  
> **Last Updated:** 2026-11-21  

---

## Decision 1: Java LTS Baseline and Target Version
- **Context:** The environment has Oracle JDK 25.0.1 LTS (`build 25.0.1+8-LTS-27`). Previous industry LTS releases are Java 8, 11, 17, and 21.
- **Decision:** Use **Java 25 LTS** as the authoritative baseline for all compilation, syntax, idioms, and standard libraries.
- **Consequences:**
  - Modern features finalized in Java 21–25 (Virtual Threads, Sequenced Collections, Pattern Matching, Stream Gatherers, Class-File API, Scoped Values, Flexible Constructor Bodies, Compact Source Files, Module Imports) are presented as standard idiomatic Java.
  - Every note includes an explicit migration matrix highlighting historical differences across Java 8, 11, 17, 21, and 25.
  - Preview features (e.g., Structured Concurrency JEP 505) are explicitly marked with `--enable-preview`.

---

## Decision 2: Prohibition of Withdrawn Features (String Templates)
- **Context:** String Templates were introduced as a preview feature in Java 21 (JEP 430) and 22 (JEP 459) using syntax `STR."Hello \{name}"`. However, JEP 430/459 was officially **withdrawn in Java 23** and does NOT exist in Java 24 or 25.
- **Decision:** Do NOT present `STR."..."` as current or standard Java syntax. Any mention of String Templates must clearly state that the feature was withdrawn in JDK 23 for comprehensive redesign.
- **Consequences:** Prevents invalid code snippets that fail to compile under Java 25.

---

## Decision 3: Removal of SecurityManager
- **Context:** JEP 486 in Java 24 permanently removed the `SecurityManager` and disabled `System.setSecurityManager()`. Calling it throws `UnsupportedOperationException`.
- **Decision:** Document `SecurityManager` strictly in the context of its deprecation and permanent removal in Java 24+, emphasizing OS-level process isolation, containers, and modern JVM boundary flags.
- **Consequences:** Notes in `21-security` and `15-jvm-internals` will not instruct students to use legacy SecurityManager policies.

---

## Decision 4: Markdown-Only Repository Policy
- **Context:** The curriculum repository must remain a pure Markdown reference system. Extraneous `.class` files, compiled bytecode, `.jar` binaries, and temporary project builds pollute git diffs and repository search.
- **Decision:** Under `01-languages/java/`, **only `.md` files** and documentation files are allowed. No `.java`, `.class`, `.jar`, `.xml`, `.gradle`, or executable build directories are committed directly to the repo.
- **Consequences:**
  - All test compilation and execution are conducted in the out-of-repo scratch directory:
    `C:\Users\RAMAVATH SHIVA RAM\.gemini\antigravity\brain\a9bfc182-e985-4a5d-9df0-ebf568360240\scratch\verification\`.
  - Project source codes in `26-projects` are presented as complete, copy-pasteable, verified Markdown code blocks with embedded `pom.xml` and test suites.

---

## Decision 5: Standard 13-Section Template Enforcement
- **Context:** Notes in the repository had varying lengths and inconsistent formats.
- **Decision:** All concept notes must strictly conform to `_templates/concept-template.md` (13 sections), targeting 400–900 lines per note.
- **Consequences:** High consistency, clear navigation, deep coverage including memory internals, 5 runnable verified examples, 6+ common pitfalls, 8+ interview questions, and 9+ exercises with solutions.

---

## Decision 6: Restoration of Misnamed Stray Module READMEs
- **Context:** Modules 10, 11, 12, 16, and 17 contained files named `README (1).md`, `README (6).md`, etc., while their canonical `README.md` files were missing.
- **Decision:** Rename the stray files directly to `README.md` within their respective module directories.
- **Consequences:** Clean directory tree without duplicate files or broken relative links.
