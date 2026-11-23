# JavaScript Engineering Curriculum Decisions Log

> Architectural, pedagogical, and structural decisions recorded during the JavaScript curriculum audit, expansion, and verification.

---

### Decision 1: Platform & Toolchain Baseline
- **Choice:** Oracle Node.js v24.11.1 / Node.js v22 LTS as primary runtime baseline; ECMAScript 2024 / 2025 as language standard.
- **Rationale:** Node.js v22 is the current Active LTS, and Node.js v24 is the latest modern release. Both provide native support for modern ES2024 features (`Object.groupBy`, `Promise.withResolvers`, Set methods, ArrayBuffer transfer) without polyfills or experimental flags.

### Decision 2: Strict Markdown-Only Repository Policy
- **Choice:** Maintain strictly `.md` files (and `tree.txt`) inside `01-languages/javascript/`.
- **Rationale:** Prevents repository clutter, build artifact pollution (`node_modules/`, `.dist/`), and accidental package manager cache bloat. All verification and execution is performed in external scratch directories.

### Decision 3: Language-First vs Framework Separation
- **Choice:** Strictly language and runtime-focused. Server-side web frameworks (Express, NestJS) belong in `03-backend/nodejs/` and `03-backend/nestjs/`; frontend view libraries (React, Next.js) belong in `02-frontend/`; TypeScript-specific typing belongs in `01-languages/typescript/`.
- **Rationale:** Preserves clear pedagogical boundaries, preventing duplication while fostering targeted deep-dives into core language mechanics.

### Decision 4: Authoritative 13-Section Concept Note Standard
- **Choice:** Enforce 13 mandatory numbered sections across every concept note (`_templates/concept-template.md`).
- **Rationale:** Ensures every note delivers senior-to-staff level depth: internals (V8 memory, hidden classes, JIT compiler), 5 verified real-world code examples, common failure modes, interview questions, and exercises with verified solutions.

### Decision 5: Test Runner Selection for Code Verification
- **Choice:** Standard Node.js built-in test runner (`node:test`) and strict assertions (`node:assert/strict`).
- **Rationale:** Eliminates external npm package dependencies during automated verification while providing standard TAP/spec output across all modern Node.js versions.

### Decision 6: Moving Topic Module Cheatsheet to Dedicated Track
- **Choice:** Move `08_modern-javascript/07_modern-syntax-cheatsheet.md` to `27_cheatsheets/07_modern-syntax.md` and preserve relative link in `08_modern-javascript/README.md`.
- **Rationale:** Consolidates all quick-reference and cheat-sheet material into `27_cheatsheets/` for consistency with Java and C curriculum structures.
"""
,Overwrite:true,Path: