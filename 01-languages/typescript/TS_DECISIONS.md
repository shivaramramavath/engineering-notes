# TypeScript Architecture & Decisions Log (TS_DECISIONS.md)

> Architectural Decision Records (ADRs) and structural conventions governing the audit, remediation, and expansion of the TypeScript curriculum track (`01-languages/typescript/`).

---

## Decision Index

| ADR | Date | Title | Status |
| :--- | :--- | :--- | :--- |
| `ADR-01` | 2026-11-23 | Adoption of `_templates/` Directory with Version / tsconfig Tested Field | **Accepted** |
| `ADR-02` | 2026-11-23 | Retention of `NN-kebab-case` Naming Convention | **Accepted** |
| `ADR-03` | 2026-11-23 | Purge of Stray Numbered Duplicates (`README (N).md`) | **Accepted** |
| `ADR-04` | 2026-11-23 | Creation of Module 29: Typed Data Structures & Algorithms | **Accepted** |
| `ADR-05` | 2026-11-23 | Baseline Calibration: TS 5.x–7.0 Native, Node.js 22 LTS, and Type Stripping | **Accepted** |
| `ADR-06` | 2026-11-23 | Markdown-Only Invariant and External Scratch Verification | **Accepted** |

---

## ADR Details

### ADR-01: Adoption of `_templates/` Directory with Version / tsconfig Field
- **Context**: The existing `templates/` folder contained an empty stub. The prompt allows normalizing to `_templates/` (matching the Java and JavaScript tracks) and requires adding `project-template.md` and a `"TypeScript version / tsconfig tested"` metadata field to the concept template.
- **Decision**: Rename/populate `_templates/` with `concept-template.md` and `project-template.md`. The metadata header includes:
  `| **TypeScript Baseline** | TS 5.x / TS 7.0 Native |`
  `| **Tested tsconfig** | strict: true, exactOptionalPropertyTypes: true, noUncheckedIndexedAccess: true |`

### ADR-02: Retention of `NN-kebab-case` Naming Convention
- **Context**: The TypeScript folder structure historically uses `NN-kebab-case` (e.g. `00-setup`, `01-fundamentals`, `01-compiler-overview.md`).
- **Decision**: Retain `NN-kebab-case` for all folders and markdown files across all 30 modules (`00-setup` through `29-typed-data-structures-and-algorithms`).

### ADR-03: Purge of Stray Numbered Duplicates
- **Context**: Stray files from earlier copy operations (`README (2).md` in `12-async-and-iteration`, `README (3).md` in `13-compiler-and-tsconfig`, `README (4).md` in `17-design-patterns`, `README (1).md` in `18-testing-and-debugging`) clutter the track.
- **Decision**: Purge these duplicate files after verifying their content against the canonical `README.md` in each respective folder.

### ADR-04: Creation of Module 29: Typed Data Structures & Algorithms
- **Context**: The curriculum requires a dedicated typed DSA module (`29-typed-data-structures-and-algorithms`) covering generic collections with type-level invariants.
- **Decision**: Author 5 comprehensive notes in `29-typed-data-structures-and-algorithms`:
  1. `01-generic-stack-queue-linked-list.md`
  2. `02-heap-and-priority-queue.md`
  3. `03-graph-and-trie.md`
  4. `04-typed-iterators-and-collections.md`
  5. `05-algorithm-complexity-with-ts.md`
  plus `README.md`.

### ADR-05: Baseline Calibration: TS 5.x–7.0 Native, Node.js 22 LTS, and Type Stripping
- **Context**: Modern TypeScript spans 5.x features (`NoInfer`, Stage 3 Decorators, `using`, inferred type predicates, `--erasableSyntaxOnly`) through the Go-based native compiler in TypeScript 7.0, along with Node.js 22 LTS type stripping.
- **Decision**: Every note explicitly documents compatibility: `"Since TS 5.0"`, `"Since TS 5.4"`, `"Since TS 5.8"`, `"TypeScript 7 Native"`, and Node.js native type stripping constraints.

### ADR-06: Markdown-Only Invariant and External Scratch Verification
- **Context**: The repository must contain strictly markdown files inside `01-languages/typescript/`.
- **Decision**: Code verification occurs in external scratch directories (`<appDataDir>/brain/<id>/scratch/`). Zero non-markdown files are committed to the track.
