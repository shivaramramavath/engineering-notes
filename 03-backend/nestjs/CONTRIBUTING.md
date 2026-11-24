# Contributing to the NestJS Curriculum

> Guidelines and standards for authoring, reviewing, and extending notes within the NestJS learning track (`03-backend/nestjs`).

---

## 1. Architectural Standards

1. **Target Baseline**: All notes and code examples must be compatible with **NestJS 11+** and **Node.js >= 20.0.0** (Node.js 22 LTS reference). Tag features specific to Nest 11 (`ParseDatePipe`, `IntrinsicException`, Keyv caching, named route wildcards).
2. **Authoritative Templates**:
   - Concept notes MUST strictly follow `03-backend/nestjs/_templates/concept-template.md` (13 mandatory sections).
   - Reference projects MUST strictly follow `03-backend/nestjs/_templates/project-template.md` (5 mandatory parts).
3. **Markdown-Only Rule**: ONLY `.md` files (and root `tree.txt`) may exist inside the repository. Absolutely NO `.ts`, `.js`, `.json`, or `node_modules` in the folder tree. All code examples, configs, and test manifests must be placed inside fenced code blocks within markdown documents.
4. **Runnable & Verified Code**: Fenced code blocks must contain real, working, type-safe TypeScript code with explicit imports from `@nestjs/*`.
5. **Cross-Module Boundaries**: Cross-link to related tracks instead of duplicating:
   - Node.js runtime $\rightarrow$ `03-backend/nodejs`
   - TypeScript typing $\rightarrow$ `01-languages/typescript`
   - SQL queries $\rightarrow$ `09-sql`
   - DevOps / Docker $\rightarrow$ `04-devops`

---

## 2. Formatting & File Conventions

- **File Naming**: Strict `NN-kebab-case.md` format (e.g. `01-pipeline-overview.md`).
- **Folder Naming**: Strict `NN-kebab-case` format (e.g. `03-core-concepts`).
- **Module Index**: Every directory must contain a canonical `README.md` file summarizing scope and contents. Never create duplicate `README (N).md` files.
- **Code Fences**: Every code block must declare its language tag (`typescript`, `json`, `sql`, `yaml`, `dockerfile`, `bash`).
- **Links**: Use relative markdown links (`../01-getting-started/README.md`) so documents render properly across GitHub and local editors.

---

## 3. Pull Request Checklist

Before submitting a contribution:
- [ ] Conforms to `concept-template.md` or `project-template.md`.
- [ ] All code blocks use explicit language tags and valid NestJS 11 syntax.
- [ ] Mistakes table has 6+ realistic engineering scenarios.
- [ ] 8 interview questions with `<details>` tags are provided.
- [ ] 6 exercises with solutions are provided.
- [ ] Module `README.md` and root `INDEX.md` are updated.
