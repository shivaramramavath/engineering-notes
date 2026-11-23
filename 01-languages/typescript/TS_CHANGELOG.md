# TypeScript Engineering Curriculum Changelog (TS_CHANGELOG.md)

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the TypeScript curriculum.

---

## 2026-11-23

### Batch 2: Setup, Fundamentals & Functions (COMPLETED)
- **Module 00: Setup & Toolchain**
  - Expanded all 6 notes to 420–470 lines adhering strictly to \`_templates/concept-template.md\`:
    - \`00-toolchain.md\`, \`01-first-project.md\`, \`02-editor-and-playground.md\`, \`03-typescript-versions-and-upgrade-strategy.md\`, \`04-running-typescript.md\`, \`05-playground-and-reproduction-repos.md\`.
  - Added module roadmap \`00-setup/README.md\`.
  - Documented TS 7.0 Native Go Compiler architecture, Node 22 native type stripping, modern bundler setups.

- **Module 01: TypeScript Fundamentals**
  - Expanded/authored all 14 notes to 315–543 lines with 13 canonical sections, zero placeholders, complete compiler diagnostics and exercises:
    - \`00-types-and-inference.md\`, \`01-primitive-types.md\`, \`02-arrays-and-tuples.md\`, \`03-objects.md\`.
    - \`04-type-aliases.md\` (539 lines): Recursive types, nominal branding via \`unique symbol\`, AST & Binder internals.
    - \`05-enums-and-const-objects.md\` (543 lines): IIFE reverse mappings, Node 22 type stripping breakage, TS 5.8 \`--erasableSyntaxOnly\` compliance.
    - \`06-any-and-unknown.md\` (525 lines): Type lattice, safe boundary parsing, \`useUnknownInCatchVariables\`, \`IsAny<T>\` detection.
    - \`07-never-and-void.md\` (496 lines): Bottom type theory, union distribution over \`never\`, callback substitutability of \`void\`, \`assertNever\`.
    - \`08-null-and-undefined.md\` (477 lines): \`strictNullChecks\`, TS 4.4 \`exactOptionalPropertyTypes\`, nullish coalescing (\`??\`).
    - \`09-object-vs-empty-curly-vs-object-wrapper.md\` (496 lines - NEW): \`object\` vs \`{}\` vs \`Object\`, TS 4.8 \`T & {}\` NonNullable mechanics.
    - \`10-bigint-symbol-and-unique-symbol.md\` (513 lines - NEW): Arbitrary precision integers, nominal branding, TS 5.2 \`Symbol.dispose\` explicit resource management.
    - \`11-readonly-arrays-and-tuples.md\` (480 lines - NEW): Shallow immutability, mutating methods exclusion, \`as const\` tuple inference, \`DeepReadonly<T>\`.
    - \`12-truthiness-and-equality-narrowing.md\` (470 lines - NEW): CFA flow graphs, loose nullish equality, falsy numerical traps.
    - \`13-typeof-instanceof-in-quick-reference.md\` (522 lines - NEW): Runtime type guards, prototype cross-realm limitations, TS 4.9 unconstrained \`in\` narrowing.
  - Authored comprehensive \`01-fundamentals/README.md\`.

- **Module 02: Functions**
  - Expanded/authored all 8 notes to 478–567 lines adhering to canonical standard:
    - \`00-function-types.md\` (508 lines): Parameter contravariance under \`strictFunctionTypes\`, return covariance, callable objects with properties.
    - \`01-parameters-and-return-types.md\` (487 lines): Optional, default, and tuple rest parameters, \`noImplicitReturns\` CFA analysis.
    - \`02-callbacks.md\` (563 lines): Error-first conventions, \`void\` callback substitutability, type predicate callbacks, async \`forEach\` traps.
    - \`03-function-overloads.md\` (478 lines): Overload signatures vs hidden implementation signature, union argument resolution, conditional type alternatives.
    - \`04-this-parameters.md\` (529 lines): Fake \`this\` parameters, \`noImplicitThis\`, detached method protection, \`ThisType<T>\` contextual options typing.
    - \`05-generic-callbacks-and-contextual-typing.md\` (498 lines - NEW): Multi-phase bidirectional inference, TS 5.4 \`NoInfer<T>\` parameter isolation.
    - \`06-assertion-signatures-and-this-types.md\` (567 lines - NEW): TS 3.7 \`asserts\` linear scope mutation, polymorphic \`this\` returns, \`this is Type\` method guards.
    - \`07-abstract-construct-signatures-and-constructor-types.md\` (559 lines - NEW): TS 4.2 \`abstract new\` constructors, type-safe mixin hierarchies, DI container registries.
  - Authored comprehensive \`02-functions/README.md\`.

### Batch 1: Structural Hygiene & Authoritative Templates
- Initialized \`TS_FACTS.md\`, \`TS_AUDIT.md\`, \`TS_COMPLETION_PLAN.md\`, \`TS_DECISIONS.md\`, and \`TS_PROGRESS.md\`.
- Purged stray duplicate files:
  - Deleted \`12-async-and-iteration/README (2).md\`
  - Deleted \`13-compiler-and-tsconfig/README (3).md\`
  - Deleted \`17-design-patterns/README (4).md\`
  - Deleted \`18-testing-and-debugging/README (1).md\`
- Created \`_templates/\` directory containing:
  - \`_templates/concept-template.md\`: 13-section canonical standard including "TypeScript version / tsconfig tested" metadata field.
  - \`_templates/project-template.md\`: 5-section production project standard with zero placeholders.
