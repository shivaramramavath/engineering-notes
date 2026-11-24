# TypeScript Curriculum Contribution Guidelines

> Standards, conventions, verification rules, and authoring guidelines for extending or maintaining the TypeScript engineering repository.

---

## 📜 Core Editorial Principles

1. **Production-Grade Rigor**: Every code snippet, architecture pattern, and configuration must reflect enterprise standards. No toy code, no artificial simplifications.
2. **Zero Incomplete Content**: Never leave `TODO`, `TBD`, `FIXME`, or placeholder ellipses (`...`) in code blocks or text.
3. **Markdown-Only in Learning Track**: All files inside `01-languages/typescript/` must be pure Markdown (`.md`). Benchmarks, scratch scripts, or temporary test code must reside exclusively in external scratch directories.
4. **Exhaustive Template Compliance**: Every new concept note must adhere strictly to `_templates/concept-template.md` (13 sections, 350-700 lines). Every new project must follow `_templates/project-template.md`.

---

## 🛠️ Note Authoring Checklist

Before adding or updating a concept note, verify:
- [ ] **Title**: Single `#` matching module naming (`# Module XX: Topic Name (NN_slug)`).
- [ ] **Summary Blockquote**: 2-3 sentences explaining architectural intent.
- [ ] **Metadata Table**: Complete table including "TypeScript Baseline" (`TypeScript 5.0 - 5.8+ / 7.0 Native`) and "Tested tsconfig" profile.
- [ ] **Table of Contents**: Exact anchor links to all 13 numbered sections.
- [ ] **Section 3 (How It Works Internally)**: Includes a clear ASCII or Mermaid diagram explaining AST/compiler/memory mechanics.
- [ ] **Section 5 (Code Examples)**: Exactly 5 complete examples (Basic, Real-World, Edge Case, Failure Case with `@ts-expect-error` and compiler error code, and Idiomatic).
- [ ] **Section 7 (Common Mistakes)**: Markdown table with 6+ rows detailing Mistake, Why It Fails, TS Error Code, and Correct Pattern.
- [ ] **Section 12 (Interview Questions)**: Exactly 8 progressive questions in collapsible `<details>` tags.
- [ ] **Section 13 (Challenges)**: Exactly 6 coding challenges in collapsible `<details>` tags.

---

## 🧪 Project Authoring Checklist

Before adding a new production project to `26-projects/`, verify:
- [ ] Adheres strictly to `_templates/project-template.md`.
- [ ] Complete architecture diagram in ASCII.
- [ ] Zero placeholders: Complete source code for all components.
- [ ] Complete `package.json` with explicit dependencies and scripts.
- [ ] Complete `tsconfig.json` with strict flags enabled.
- [ ] Full Vitest / Jest test suite in `tests/app.test.ts`.
- [ ] Reproduction CLI log showing test run pass output.

---

## 🚀 Compiler Compatibility Standards
- Baseline: **TypeScript 5.0 - 5.8+**
- Strict Mode: Always enabled (`"strict": true`)
- Modern Options: Ensure examples conform with `"verbatimModuleSyntax": true` and `"isolatedModules": true`.
