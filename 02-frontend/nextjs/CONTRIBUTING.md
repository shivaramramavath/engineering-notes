# Next.js Curriculum Contribution Guidelines

> Standards, conventions, verification rules, and authoring guidelines for extending or maintaining the Next.js engineering learning repository.

---

## 1. Core Editorial Principles

1. **Production-Grade Rigor**: Every code snippet, architecture pattern, and configuration must reflect enterprise standards. No toy code, no artificial simplifications.
2. **Zero Incomplete Content**: Never leave `TODO`, `TBD`, `FIXME`, or placeholder ellipses (`...`) in code blocks or text.
3. **Markdown-Only in Learning Track**: All files inside `02-frontend/nextjs/` must be pure Markdown (`.md`). Benchmarks, scratch scripts, or temporary test code must reside exclusively in external scratch directories.
4. **Exhaustive Template Compliance**: Every new concept note must adhere strictly to `_templates/concept.md` (13 sections, complete metadata, diagrams, and code). Every project note must adhere to `_templates/project-template.md`.

---

## 2. Note Authoring Checklist

Before adding or updating a concept note, verify:
- [ ] **Title**: Single `#` matching module naming (`# Module XX: Topic Name (NN_slug)`).
- [ ] **Summary Blockquote**: 2-3 sentences explaining architectural intent.
- [ ] **Metadata Table**: Complete table including:
  - Next.js Baseline (`Next.js 15.2+ / 16.0`)
  - React Baseline (`React 19.0.0`)
  - Node.js Baseline (`Node.js 20+ LTS`)
  - Router (`App Router`)
  - Tested With profile
- [ ] **Table of Contents**: Exact anchor links to all 13 numbered sections.
- [ ] **Section 3 (How It Works Internally)**: Includes a clear ASCII or Mermaid diagram explaining RSC/compiler/streaming mechanics.
- [ ] **Section 5 (Code Examples)**: Exactly 5 complete examples (Basic, Real-World, Edge Case, Failure Case with `@ts-expect-error` or runtime error code, and Idiomatic).
- [ ] **Section 7 (Common Mistakes)**: Markdown table with 6+ rows detailing Mistake, Why It Fails, Error Message / Symptom, and Correct Pattern.
- [ ] **Section 12 (Interview Questions)**: Exactly 8 progressive questions in collapsible `<details>` tags.
- [ ] **Section 13 (Challenges)**: Exactly 6 coding challenges in collapsible `<details>` tags.

---

## 3. Scope Boundaries & Anti-Duplication Rules

To maintain tight architectural focus:
- **Client-Side React**: Cross-link to `02-frontend/reactjs/` for standard React hooks, concurrent features, or client-side context patterns.
- **TypeScript Core**: Cross-link to `01-languages/typescript/` for generic utility types, type narrowing, and tsconfig fundamentals.
- **Backend Infrastructure**: Cross-link to `03-backend/` for raw database setup, Redis administration, and microservice architectures.
- **Security & Auth Theory**: Cross-link to `06-project-patterns/auth/` and `07-system-design/` for broad authentication theory and system design fundamentals.
- **DevOps & Containers**: Cross-link to `04-devops/` for Kubernetes cluster provisioning and CI/CD pipelines.

---

## 4. Modern Next.js Conventions

- **Always Await Request APIs**: In Next.js 15+, `cookies()`, `headers()`, `params`, and `searchParams` are asynchronous `Promise`s.
- **Explicit Caching**: Use the `"use cache"` directive and `cacheLife()` rather than relying on legacy `fetch` auto-caching.
- **Defense in Depth**: Always verify sessions in the Data Access Layer (`lib/dal.ts`) with `import 'server-only'`. Never rely solely on edge middleware for data security.
