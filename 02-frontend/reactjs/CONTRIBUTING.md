# Contributing to the ReactJS Curriculum (CONTRIBUTING.md)

> Engineering guidelines, verification requirements, and formatting rules for all authors and technical reviewers.

---

## 1. Core Principles

1. **Markdown-Only Policy**:
   - The repository contains **ONLY `.md` files** (and `tree.txt`).
   - Never commit `.tsx`, `.ts`, `.js`, `.json`, `.css`, or `node_modules` into `02-frontend/reactjs/`.
   - All source code, configs, and tests must reside inside fenced code blocks within markdown documents.
2. **Real Verification Requirement**:
   - Never invent or fabricate console outputs, render counts, bundle sizes, or benchmark figures.
   - All code snippets must be compiled and tested in the scratch workspace (`scratch/react_verify/`) before pasting exact outputs into notes.
3. **Canonical 13-Section Concept Template**:
   - Every concept note must follow `_templates/concept.md` with all 13 numbered sections.
   - Must contain 5 complete examples (Basic $\to$ Production $\to$ Edge Case $\to$ Failure Mode $\to$ Modern React 19).
   - Must include an ASCII/flowchart diagram, 6+ mistake rows, 8 interview Q&As in `<details>`, and 6 exercises with verified solutions in `<details>`.
4. **Target Line Length**:
   - Concept notes must target **400 to 700 lines**.
   - Expand any note below 150 lines without padding; split any note exceeding 900 lines.

---

## 2. Technical Standards & Version Baselines

- **Baseline Runtime**: React 19.0.0+, TypeScript 5.7+ (strict mode), Vite 6.0+, Vitest 3.0+.
- **No Legacy Deprecations**:
  - Do NOT teach Create React App (CRA) as current practice.
  - Do NOT use `forwardRef()` for React 19 notes (pass `ref` as a prop).
  - Do NOT recommend manual `useMemo`/`useCallback` without explaining the React Compiler.
  - Do NOT use `{ suspense: true }` in TanStack Query (use `useSuspenseQuery`).
- **Accessibility by Default**:
  - Always provide accessible HTML (proper button elements, semantic headings, `aria-expanded`, `aria-label`).
  - Never teach non-interactive element buttons (e.g. `<div onClick={...}>`).

---

## 3. Resumable Workflow & Git Commits

- Update `REACT_PROGRESS.md` after completing each module batch.
- Append all structural corrections, migrations, and bug fixes to `REACT_CHANGELOG.md`.
- Group changes into **one commit per completed module**:
  ```bash
  git add 02-frontend/reactjs/<module-dir>/
  git commit -m "feat(react): complete Module XX - [Topic Name] (N notes + README)"
  ```
