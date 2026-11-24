# Python Curriculum Conventions & Authoring Standards (PY_CONVENTIONS.md)

> Formal document defining the organizational conventions, metadata schemas, code testing standards, and architectural blueprints adapted from the Java and TypeScript tracks for the Python curriculum (`01-languages/python/`).

---

## 1. Directory & File Organization Standards

1. **NN-kebab-case Hierarchy**:
   - Module directories use zero-padded two-digit prefixes: `00-setup`, `01-fundamentals`, `02-data-structures`, ..., `28-cheatsheets`.
   - Notes inside modules follow zero-padded two-digit numbering: `00-topic-name.md`, `01-second-topic.md`.
   - Every module directory contains a dedicated `README.md` serving as an introductory syllabus and navigation hub.
2. **Repository Track Boundaries (Markdown-Only)**:
   - Inside `01-languages/python/`, ONLY `.md` files are permitted.
   - Zero `.py`, `.toml`, `.json`, `.lock`, `.yaml`, or temporary test files may be checked into this directory.
   - All source code, tests, configuration manifests (`pyproject.toml`), and Dockerfiles must be embedded as fenced Markdown code blocks with explicit language tags (````python`, ````toml`, ````bash`, etc.).
   - All test compilation, execution, linting, and benchmarking must occur in external scratch directories (`<artifacts>/scratch/`).
3. **Canonical Root Files**:
   - `README.md`: Master curriculum index and platform baseline.
   - `INDEX.md`: Complete alphabetical and categorical directory of every note and concept.
   - `ROADMAP.md`: Career learning trajectories (Beginner, Backend, Data/ML, DevOps, Staff Architect).
   - `GLOSSARY.md`: Precise definitions for 60+ Python runtime, type theoretical, and architectural terms.
   - `CONTRIBUTING.md`: Standards for note authors and PR reviewers.
   - `LICENSE`: Open learning curriculum license.
   - Tracking files: `PY_FACTS.md`, `PY_CONVENTIONS.md`, `PY_DECISIONS.md`, `PY_AUDIT.md`, `PY_COMPLETION_PLAN.md`, `PY_PROGRESS.md`, `PY_CHANGELOG.md`, `PY_FINAL_REPORT.md`.

---

## 2. Concept Note Architectural Standard (13 Sections)

Every concept note strictly adheres to the 13-section architectural model defined in `_templates/concept-template.md`:

| Section | Title | Mandatory Contents |
|---|---|---|
| **Header** | Title & Summary | `# Module XX: Topic Name (NN-slug)` followed by 2-3 sentence blockquote |
| **Metadata** | Metadata Table | Category, Difficulty, Python Version, Tested With, Free-Threading / Platform Notes |
| **TOC** | Table of Contents | Anchor links to all 13 numbered sections |
| **Section 1** | What & Why | Industry motivation, problem solved, mental model, when to use / when NOT to use |
| **Section 2** | Core Concepts | Formal definitions, data model axioms, memory layout, invariants |
| **Section 3** | How It Works Internally | CPython runtime mechanics, bytecode (`dis`), frame objects, ASCII/Mermaid diagram |
| **Section 4** | Syntax & API Reference | Exhaustive signatures, parameters, flags, return types, PEP references |
| **Section 5** | Code Examples (5 Verified) | 1. Basic / Minimal<br>2. Real-World Production<br>3. Edge Case / Boundary<br>4. Failure Case (syntax/runtime error + traceback)<br>5. Idiomatic Pythonic Pattern |
| **Section 6** | Edge Cases & Pitfalls | Subtle traps, mutability gotchas, closure late-binding, GIL implications |
| **Section 7** | Common Mistakes Table | Markdown table (6+ rows): Mistake, Why It Fails, Exception / Diagnostic, Correct Fix |
| **Section 8** | Performance & Complexity | Big-O time/space, CPython bytecode efficiency, memory allocation profile |
| **Section 9** | CPython Internals & Bytecode | Bytecode instructions, C structures (`PyObject`, `PyTupleObject`), memory model |
| **Section 10** | Best Practices & Style | PEP 8 / PEP 257 rules, Ruff rules, typing rules, maintainability guidelines |
| **Section 11** | Real-World Case Study | Production architecture walkthrough from high-scale Python codebases |
| **Section 12** | Technical Interview Questions | Exactly 8 progressive questions with collapsible `<details>` answers |
| **Section 13** | Practical Exercises & Challenges | Exactly 6 challenges (2 Easy, 2 Medium, 2 Hard) with collapsible `<details>` solutions |

---

## 3. Metadata Table Schema for Python Notes

Each concept note includes the following standardized metadata table immediately under the summary blockquote:

```markdown
| Field | Value |
|---|---|
| **Category** | Core Language / Data Structures / Concurrency / etc. |
| **Target Level** | Beginner / Intermediate / Advanced / Staff |
| **Python Version** | Python 3.10+ (with 3.12 / 3.13 / 3.14 specific notes where applicable) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Requires GIL / Windows AMD64 |
```

---

## 4. Production Project Architectural Standard

Every project note in `26-projects/` conforms to `_templates/project-template.md`:
1. Title: `# Project [XX]: [Project Name] ([slug])`
2. Architectural design and ASCII sequence/component diagrams.
3. Complete runnable source code organized across modular layers (Domain, Service, Repository, CLI/Entrypoint). Zero placeholders, zero ellipses (`...`), zero `pass` stubs.
4. `pyproject.toml` configuration with dependency declarations, build tool settings (`hatchling` or `flit`), ruff, and mypy configs.
5. Exhaustive `pytest` test suite (`tests/test_app.py`) with 100% path coverage.
6. Real CLI execution logs showing commands run and passing test outputs.

---

## 5. Verification & Tooling Protocol

1. **No Untested Claims**: All example code must be executed in external scratch directory `<artifacts>/scratch/`.
2. **Linter & Formatter Quality**: All Python code blocks must pass `ruff check` and `ruff format --check`.
3. **Type Checking Quality**: All typed code blocks must pass `mypy --strict` or document expected type diagnostic codes with `# type: ignore[...]`.
4. **Real Execution Outputs**: Outputs in code blocks must reflect real outputs captured from CPython runs.
