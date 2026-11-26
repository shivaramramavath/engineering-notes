# SOLID, Design Principles & Design Patterns Changelog (SDP_CHANGELOG.md)

All structural changes, renames, additions, corrections, and verified code runs are documented here.

---

### 2026-11-26 - Track Initialization & Governance Baseline
- Formulated `SDP_FACTS.md` with verified runtime baselines (Java 25 LTS, TypeScript Node 24 native type-stripping, Python 3.14) and verified library citations.
- Built reusable multi-language test runner in scratch directory: `<scratch>/sdp_runner/runner.js`.
- Authored `SDP_AUDIT.md` identifying legacy formatting gaps, 7 stray parenthesis overview files, and missing modern design curriculum modules.
- Formulated `_templates/principle-template.md` and `_templates/pattern-template.md` defining the mandatory 17-section problem-first format.
- Authored `SDP_DECISIONS.md` establishing ADRs 001 through 008.
- Authored `SDP_COMPLETION_PLAN.md` detailing the 21 target modules, directory renumbering map, and stray file merge strategy.
- Initialized `SDP_PROGRESS.md` tracking all modules and notes.
- Backed up initial track to `../SDP-backup-20261126`.

### 2026-11-27 - Module 01 & Module 02 Completion
- **Module 01: Foundations (`01-foundations/`)**:
  - Authored full 17-section notes: `00-overview.md`, `01-oop-pillars.md`, `02-composition-vs-inheritance.md`, `03-coupling-and-cohesion.md`, `04-programming-to-interfaces.md`, `05-abstraction-and-information-hiding.md`, `06-encapsulate-what-varies.md`, `07-immutability-and-value-objects.md`, `08-command-query-separation.md`, `09-design-by-contract-and-defensive-programming.md`, `10-what-is-good-design.md`, and `README.md`.
  - All runnable code examples verified against Java 25, TypeScript 5.8, and Python 3.14 via scratch runner.
- **Module 02: SOLID Principles (`02-solid-principles/`)**:
  - Authored full 17-section notes: `00-overview.md`, `01-single-responsibility.md`, `02-open-closed.md`, `03-liskov-substitution.md`, `04-interface-segregation.md`, `05-dependency-inversion.md`, `06-solid-in-practice.md`, `07-solid-violations-spotting-guide.md`, `08-solid-and-functional-programming.md`, `09-criticisms-and-limits-of-solid.md`, and `README.md`.
  - Verified code runs:
    - SRP: CFO/COO/CTO separation verified in `test_srp.js`.
    - OCP: Payment gateway extensions verified in `test_ocp.js`.
    - LSP: Account capability contracts verified in `test_lsp.js`.
    - ISP: Printer/Scanner/Fax role interfaces verified in `test_isp.js`.
    - DIP: Domain ports with Postgres/SendGrid adapters verified in `test_dip.js`.
    - SOLID in Practice: Full OrderProcessor pipeline verified in `test_practice.js`.
    - Functional SOLID: Pure lambdas, curried DI, and higher-order functions verified in `test_fp.js`.
  - Verified 100% adherence to the 17-section template across all notes (each between 430 and 662 lines).
