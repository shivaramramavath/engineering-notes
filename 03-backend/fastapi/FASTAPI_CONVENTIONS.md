# FastAPI Curriculum Conventions & Authoring Standards (FASTAPI_CONVENTIONS.md)

> Formal standard defining the organization, metadata schemas, code testing rules, and architectural blueprints for the FastAPI curriculum (`03-backend/fastapi/`).
> Adapted from the Python, NestJS, and Spring Boot tracks.

---

## 1. Directory & File Organization Standards

1. **NN-kebab-case Hierarchy**:
   - Module directories use zero-padded two-digit prefixes: `00-setup`, `01-fundamentals`, `02-pydantic-v2`, ..., `20-cheatsheets`.
   - Notes inside modules follow zero-padded two-digit numbering: `01-topic-name.md`, `02-second-topic.md`.
   - Every module directory contains a dedicated `README.md` serving as an introductory syllabus and navigation hub.
2. **Repository Track Boundaries (Markdown-Only)**:
   - Inside `03-backend/fastapi/`, ONLY `.md` files are permitted (alongside root `tree.txt`).
   - Zero `.py`, `.toml`, `.json`, `.lock`, `.yaml`, `.sql`, or temporary test files may be checked into this directory.
   - All source code, tests, configuration manifests (`pyproject.toml`), and Dockerfiles must be embedded as fenced Markdown code blocks with explicit language tags (`python`, `toml`, `bash`, `dockerfile`, `sql`, etc.).
   - All scratch execution, linting, type-checking, and test validation must occur in external scratch directories (`<artifacts>/scratch/`).
3. **Canonical Root Files**:
   - `README.md`: Master curriculum index and platform baseline.
   - `INDEX.md`: Complete alphabetical and categorical directory of every note and concept.
   - `ROADMAP.md`: Career learning trajectories (Python Dev $\to$ API Dev, Production Backend, High-Performance Async, Microservices, AI Serving).
   - `GLOSSARY.md`: Precise definitions for 70+ FastAPI, Starlette, Pydantic, and ASGI terms.
   - `CONTRIBUTING.md`: Standards for note authors and technical reviewers.
   - `LICENSE`: Educational curriculum license.
   - Tracking files: `FASTAPI_FACTS.md`, `FASTAPI_CONVENTIONS.md`, `FASTAPI_DECISIONS.md`, `FASTAPI_AUDIT.md`, `FASTAPI_COMPLETION_PLAN.md`, `FASTAPI_PROGRESS.md`, `FASTAPI_CHANGELOG.md`, `FASTAPI_FINAL_REPORT.md`.

---

## 2. Concept Note Architectural Standard (13 Sections)

Every concept note in modules 00-16 strictly adheres to the 13-section architectural model defined in `_templates/concept-template.md`:

| Section | Title | Mandatory Contents |
|---|---|---|
| **Header** | Title & Summary | `# Module XX: Topic Name (NN-slug)` followed by 2-3 sentence blockquote |
| **Metadata** | Metadata Table | Category, Difficulty, FastAPI Version, Starlette Version, Pydantic Version, Python Version, Tested With |
| **TOC** | Table of Contents | Anchor links to all 13 numbered sections |
| **Section 1** | Conceptual Overview | Industry motivation, problem solved, mental model, when to use / when NOT to use |
| **Section 2** | Core Concepts & Architecture | Formal definitions, ASGI request flow, data models, invariants, ASCII/Mermaid diagram |
| **Section 3** | How It Works Internally | Starlette ASGI scope, Pydantic core validation, dependency graph resolution, FastAPI routing internals |
| **Section 4** | Syntax & API Reference | Exhaustive signatures, parameters, flags, return types, OpenAPI schema mapping |
| **Section 5** | Code Examples (5 Verified) | 1. Basic / Minimal<br>2. Real-World Production<br>3. Edge Case / Boundary<br>4. Failure Case (422 validation payload or 500 error)<br>5. Idiomatic Modern Pattern (Annotated, Pydantic v2) |
| **Section 6** | Edge Cases & Pitfalls | Subtle traps, async/sync threadpool blocking, mutable default arguments, lifecycle gotchas |
| **Section 7** | Common Mistakes Table | Markdown table (6+ rows): Mistake, Why It Fails, Exception / Diagnostic, Correct Fix |
| **Section 8** | Performance & Scale | Throughput (RPS), serialization overhead, memory profiling, event loop impact |
| **Section 9** | Security & Failure Modes | OWASP Top 10 mapping, input validation, SQL injection prevention, authentication guards |
| **Section 10** | Testing Strategies | Unit testing with `TestClient` / `httpx.AsyncClient`, dependency overrides, pytest fixtures |
| **Section 11** | Real-World Case Study | Production architecture walkthrough from high-scale FastAPI codebases |
| **Section 12** | Technical Interview Questions | Exactly 8 progressive questions with collapsible `<details>` answers |
| **Section 13** | Practical Exercises & Challenges | Exactly 6 challenges (2 Easy, 2 Medium, 2 Hard) with collapsible `<details>` solutions |

---

## 3. Metadata Table Schema for FastAPI Notes

Each concept note includes the following standardized metadata table immediately under the summary blockquote:

```markdown
| Field | Value |
|---|---|
| **Category** | Core Routing / Pydantic v2 / Dependency Injection / Databases / etc. |
| **Target Level** | Beginner / Intermediate / Advanced / Staff Architect |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | Uvicorn 0.54.0, uv 0.13+, httpx 0.28+, pytest 9.1+, ruff 0.17+ |
```

---

## 4. Production Project Architectural Standard

Every project in `17-projects/` conforms to `_templates/project-template.md`:
1. Title: `# Project [XX]: [Project Title] ([slug])`
2. Architectural design and ASCII sequence/topology diagrams.
3. Complete runnable source code organized across modular layers (Domain Schemas, Database Models, Repositories, Services, Routers/Controllers, Config/Lifespan). Zero placeholders, zero ellipses (`...`), zero `pass` stubs.
4. `pyproject.toml` configuration with dependency declarations, build tool settings (`uv`), ruff, and mypy configs.
5. Database migrations using Alembic (`env.py`, initial revision).
6. Comprehensive `pytest` test suite (`tests/test_api.py`) covering unit, integration, and auth edge cases.
7. Real CLI execution logs showing commands run and passing test outputs.

---

## 5. Verification & Tooling Protocol

1. **No Untested Claims**: All example code is executed in external scratch directories (`<artifacts>/scratch/`).
2. **Linter & Formatter Quality**: All Python code blocks must pass `ruff check` and `ruff format --check`.
3. **Type Checking Quality**: All typed code blocks must pass `mypy --strict` or document expected type diagnostic codes with `# type: ignore[...]`.
4. **Real Execution Outputs**: Outputs in code blocks must reflect real outputs captured from CPython runs.
