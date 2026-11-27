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

### 2026-11-27 (Batch 2) - Module 03: General Design Principles Completion
- **Module 03: General Design Principles (`03-design-principles/`)**:
  - Authored 22 complete files: `README.md`, `00-overview.md`, and `01-dry-and-rule-of-three.md` through `20-principles-cheat-sheet.md`.
  - Covered DRY & Rule of Three, KISS, YAGNI, Law of Demeter, Separation of Concerns, Tell-Don't-Ask, Principle of Least Astonishment, Fail-Fast, Boy Scout Rule, High Cohesion & Low Coupling, Orthogonality, Postel's Law (including RFC 9413 security nuances), Hollywood Principle / IoC, Craig Larman's 9 GRASP patterns, Robert C. Martin's 6 Package Principles (REP, CCP, CRP, ADP, SDP, SAP), Premature Optimisation & Measuring, Idempotency Principle, Conway's Law & Team Topologies, Principles Conflicts & Trade-offs, and Operational Cheat Sheet.
  - Verified runnable multi-language examples (Java 25 LTS, TypeScript 5.8, Python 3.14) via scratch runner scripts `test_m3_batch1.js`, `test_kiss_yagni.js`, `test_m3_batch2.js`, and `test_idempotency.js`.

### 2026-11-27 (Batch 3) - Module 04 & Module 05 Completion
- **Module 04: Patterns Introduction (`04-patterns-intro/`)**:
  - Authored `README.md`, `00-overview.md`, `01-what-are-patterns.md`, `02-how-to-read-and-choose-patterns.md`, `03-dependency-injection.md` (renamed from legacy `02-dependency-injection.md`), `04-inversion-of-control-containers.md`, and `05-pattern-languages-and-catalogues-history.md`.
- **Module 05: Creational Patterns (`05-creational-patterns/`)**:
  - Authored `README.md`, `00-overview.md`, `01-singleton.md` through `11-creational-patterns-comparison.md`.
  - Verified runnable multi-language examples (Java 25, TypeScript 5.8, Python 3.14) via `test_creational_gof.js` and `test_creational_ext.js`.

### 2026-11-27 (Batch 4) - Module 06: Structural Patterns Completion
- **Module 06: Structural Patterns (`06-structural-patterns/`)**:
  - Authored `README.md`, `00-overview.md`, `01-adapter.md` through `11-structural-patterns-comparison.md` (13 files).
  - Verified runnable multi-language examples (Java 25, TypeScript 5.8, Python 3.14) via `test_struct_1.js` and `test_struct_2.js`.

### 2026-11-27 (Batch 5) - Module 07: Behavioral Patterns Completion
- **Module 07: Behavioral Patterns (`07-behavioral-patterns/`)**:
  - Authored `README.md`, `00-overview.md`, `01-strategy.md` through `15-behavioural-patterns-comparison.md` (17 files).
  - Verified runnable multi-language examples (Java 25, TypeScript 5.8, Python 3.14) via `test_behav_1.js`, `test_behav_2.js`, and `test_behav_3.js`.

## Batch 6 — Module 08 (Concurrency Patterns) & Module 12 (Functional & Modern Idioms)
- Authored all 15 files in `08-concurrency-patterns/` (`README.md`, `00-overview.md`, `01`–`13`), including Java 25 Virtual Threads & Structured Concurrency (`StructuredTaskScope`), TypeScript async/await & backpressure, and Python 3.14 `asyncio.TaskGroup` & `asyncio.Queue(maxsize)`.
- Authored all 12 files in `12-functional-and-modern-idioms/` (`README.md`, `00-overview.md`, `01`–`10`), covering functional Strategy/Decorator/Command, onion middleware, `Result`/`Either` railway error handling, currying/partial application, memoisation, persistent data, `sealed` ADTs + exhaustive pattern matching, functional DI, lazy/reactive streams, and FP vs OOP decision criteria.
- Verified Java 25, TypeScript 5.8, and Python 3.14 code examples via `test_conc_1.js`, `test_conc_2.js`, `test_conc_3.js`, and `test_fp_1.js`.

## Batch 7 — Module 09 (Enterprise Application Patterns) & Module 10 (Software Architecture)
- Rewrote & extended `09-enterprise-application-patterns/` to 18 files (`README.md`, `00-overview.md`, `01`–`16`). Renumbered legacy `03-specification.md` → `04-specification.md`, `04-service-layer.md` → `05-service-layer.md`, `05-data-transfer-object.md` → `06-data-transfer-object.md`, and added `03-data-mapper-vs-active-record.md` plus `07`–`16`.
- Rewrote & extended `10-architecture/` to 16 files (`README.md`, `00-overview.md`, `01`–`14`), adding `06-microservices-and-when-not-to.md` through `14-evolutionary-architecture.md`.
- Verified Java 25, TypeScript 5.8, and Python 3.14 enterprise examples via `test_ent_1.js`.

## Batch 8 — Module 11 (Distributed & Cloud Patterns)
- Authored all 22 files in `11-distributed-and-cloud-patterns/` (`README.md`, `00-overview.md`, `01`–`20`), covering resilience (Circuit Breaker, Retry+Jitter, Timeout/Deadline, Bulkhead, Rate Limiter), consistency (Idempotency, Saga, Transactional Outbox, CQRS Read Models, Cache-Aside), topology (API Gateway/BFF, Sidecar/Ambassador, Strangler Fig, ACL, Discovery/LB, Leader Election/Fencing Tokens, Sharding), and progressive delivery (Health Checks, Feature Flags, Blue–Green & Canary).
- Verified Java 25, TypeScript 5.8, and Python 3.14 examples via `test_dist_1.js`.
