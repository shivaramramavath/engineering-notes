# Node.js Curriculum Contribution Guidelines

> Standards, conventions, verification rules, and authoring guidelines for extending or maintaining the Node.js engineering learning repository.

---

## 1. Core Editorial Principles

1. **Production-Grade Rigor**: Every code snippet, architecture pattern, and configuration must reflect enterprise standards. No toy code, no artificial simplifications.
2. **Zero Incomplete Content**: Never leave `TODO`, `TBD`, `FIXME`, or placeholder ellipses (`...`) in code blocks or text.
3. **Markdown-Only in Learning Track**: All files inside `03-backend/nodejs/` must be pure Markdown (`.md`). Benchmarks, scratch scripts, or temporary test code must reside exclusively in external scratch directories.
4. **Exhaustive Template Compliance**: Every new concept note must adhere strictly to `_templates/concept-template.md` (13 sections, complete metadata, diagrams, and code). Every project note must adhere to `_templates/project-template.md`.

---

## 2. Note Authoring Checklist

Before adding or updating a concept note, verify:
- [ ] **Title**: Single `#` matching module naming (`# Module XX: Topic Name (NN_slug)`).
- [ ] **Summary Blockquote**: 2-3 sentences explaining architectural intent.
- [ ] **Metadata Table**: Complete table including:
  - Node.js Baseline (`Node.js 22 LTS / 24+`)
  - Module System (`ESM / CJS`)
  - Framework / Driver
  - Tested With profile
- [ ] **Table of Contents**: Exact anchor links to all 13 numbered sections.
- [ ] **Section 1.2 (System Diagram)**: ASCII or Mermaid diagram explaining event loop / network / data flow.
- [ ] **Section 3 (Five Examples)**: Exactly 5 complete examples (Basic, Real-World, Edge Case, Failure Case, and Idiomatic).
- [ ] **Section 6 (Common Mistakes)**: Markdown table with 6+ rows detailing Anti-Pattern, Root Cause, Observable Symptom, and Correct Architecture.
- [ ] **Section 11 (Interview Questions)**: Exactly 8 progressive questions in collapsible `<details>` tags.
- [ ] **Section 12 (Exercises)**: Exactly 6 coding exercises (2 easy, 2 medium, 2 hard) with verified solutions in collapsible `<details>` tags.

---

## 3. Scope Boundaries & Anti-Duplication Rules

To maintain tight architectural focus:
- **Language-Level JS**: Cross-link to `01-languages/javascript/` for standard syntax, closures, and prototypes.
- **NestJS Framework**: Cross-link to `03-backend/nestjs/`.
- **TypeScript Language**: Cross-link to `01-languages/typescript/` for generic utility types, narrowing, and compiler mechanics.
- **SQL Fundamentals**: Cross-link to `09-sql/`.
- **System Design Core**: Cross-link to `07-system-design/`.
- **DevOps & Cloud Ops**: Cross-link to `04-devops/` for Kubernetes and CI/CD pipelines.

---

## 4. Modern Node.js Baseline Standards

- **Use Native Node 22+ APIs**: Prefer built-in `node:test`, `node:sqlite`, `--env-file`, and native type stripping over third-party polyfills where applicable.
- **Express 5 Alignment**: Use `app.delete()` (not `app.del()`), avoid `req.param(name)`, and leverage automatic promise rejection forwarding.
- **Defense in Depth**: Always use parameterized queries for database calls, validate payloads with Zod, and use Argon2 for password hashing.
