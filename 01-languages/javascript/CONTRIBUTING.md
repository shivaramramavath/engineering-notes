# Contributing to the JavaScript Engineering Curriculum

> Standards, authoring conventions, verification protocols, and style guidelines for contributors and technical reviewers.

---

## 1. Core Principles & Philosophy

The JavaScript curriculum is designed as an authoritative, production-grade technical repository. All contributions must adhere to the following non-negotiable principles:
1. **Zero External Runtime Dependencies**: Code examples, projects, and test suites must run using native ECMAScript and Node.js standard libraries (`node:*`).
2. **Exhaustive Completeness**: Zero placeholders (`// TODO: implement`, `...`), zero truncated functions, and zero pseudo-code.
3. **Specification Accuracy**: Validate language behaviors against the ECMAScript 2024+ specifications and Node.js 22 LTS runtime semantics.
4. **Markdown-Only Repository Boundary**: Inside `01-languages/javascript/`, only `.md` files may be committed. Test runner scratch scripts or temporary files must reside strictly outside the track directory.

---

## 2. Note Authoring Conventions & Templates

Every new note must follow one of the two authoritative templates located in `_templates/`:

### A. Conceptual Notes (`_templates/concept-template.md`)
Every conceptual note must include:
- Single `#` Title and one-line summary blockquote.
- Metadata table (Level, Prerequisites with relative markdown links, Time to read, Status).
- Table of Contents.
- Exactly **13 numbered sections**:
  1. What & Why
  2. Core Concepts
  3. How It Works Internally (with ASCII/mermaid architectural diagram)
  4. Syntax & API Reference
  5. Practical Examples (5 complete, progressive, runnable examples)
  6. Edge Cases, Pitfalls & Gotchas
  7. Common Mistakes & Anti-Patterns (comparison table with 6+ rows)
  8. Performance Characteristics (Big-O time/space, memory allocation, cache locality)
  9. Engine Internals (V8 Hidden Classes, Shapes, Ignition Bytecode, libuv)
  10. Best Practices & Design Guidelines
  11. Real-World Case Study
  12. Interview Questions (8 deep technical Q&A in collapsible `<details>`)
  13. Exercises & Challenges (2–6 challenges with verified solutions in `<details>`)
- Target length: **350–700 lines** per note.

### B. Engineering Project Notes (`_templates/project-template.md`)
Every project note must include:
- Title: `# Project [XX]: [Project Name] ([slug])`
- Metadata table (Project Code, Node Baseline, Complexity, Architectural Style, Core Concepts).
- Section 1: Architectural Overview & Component Breakdown (ASCII flow diagram + responsibilities).
- Section 2: Complete Project Source Code (Component 1 through Component 4 with zero placeholders).
- Section 3: Build & Runtime Configuration (`package.json`).
- Section 4: Comprehensive Test Suite (`tests/app.test.js` using `node:test` and `node:assert/strict`).
- Section 5: Verification & Sample Execution (CLI test pass log).
- Target length: **300–600 lines** per project note.

---

## 3. Code Style & Verification

### Code Formatting
- Modern ES2024 syntax: `const`/`let`, arrow functions, `?.`, `??`, object grouping.
- Clean formatting: 2-space indentation, semicolons, single quotes.
- Asynchronous patterns: Use `async/await` and Promises over legacy error-first callbacks.

### Verification Protocol
Before submitting any changes, verify all code blocks natively:

```bash
# Verify code snippets via Node.js
node --test <test-file>.js

# Verify with syntax checking
node --check <script>.js
```

---

## 4. Git Commit & Documentation Standards

- **Commit Message Format**: Follow Conventional Commits:
  - `js: batch X - <brief description of completed modules>`
  - `docs(js): update roadmap and glossary for ES2025`
  - `fix(js): correct prototype receiver lookup in module 05`
- **Changelog Updates**: Every commit introducing structural additions or fixes must update both `JS_PROGRESS.md` and `JS_CHANGELOG.md`.
