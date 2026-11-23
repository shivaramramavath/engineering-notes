# TypeScript Curriculum Audit Report (TS_AUDIT.md)

> Automated baseline structural audit of the TypeScript curriculum track (`01-languages/typescript/`).

## 1. Executive Summary

- **Total Markdown Files Audited**: 220
- **Duplicate / Stray Files Identified**: 4
- **Empty Files (0 Bytes)**: 67
- **Thin Notes (< 150 Lines)**: 104

## 2. Identified Stray & Duplicate Files

| Path | Lines | Recommendation |
| :--- | :--- | :--- |
| `12-async-and-iteration/README (2).md` | 66 | Delete / Consolidate into canonical README.md |
| `13-compiler-and-tsconfig/README (3).md` | 76 | Delete / Consolidate into canonical README.md |
| `17-design-patterns/README (4).md` | 71 | Delete / Consolidate into canonical README.md |
| `18-testing-and-debugging/README (1).md` | 74 | Delete / Consolidate into canonical README.md |

## 3. Structural Gaps & Module Analysis

1. **Stray Files**: `README (2).md` in `12-async-and-iteration`, `README (3).md` in `13-compiler-and-tsconfig`, `README (4).md` in `17-design-patterns`, `README (1).md` in `18-testing-and-debugging`.
2. **Templates Directory**: `templates/` needs renaming/symlinking to `_templates/` to match Java/JavaScript track standards, and needs `concept-template.md` and `project-template.md`.
3. **Empty Track Root Files**: `README.md`, `INDEX.md`, `ROADMAP.md`, `GLOSSARY.md`, `CONTRIBUTING.md` are empty 0-byte stubs.
4. **Project Module Scaffolding**: `26-projects` contains 7 project directories each with only an empty/stub README, and `exercises/` with only `00-type-challenges.md`.
5. **Missing Advanced Modules**: Module 29 (`29-typed-data-structures-and-algorithms`) does not exist yet.

## 4. Complete File Inventory & Section Conformance

| File Path | Lines | Sections | Status | Audit Findings |
| :--- | :--- | :--- | :--- | :--- |
| `00-setup/00-toolchain.md` | 110 | 0/13 | Unset | Thin note (<150 lines) |
| `00-setup/01-first-project.md` | 124 | 0/13 | Unset | Thin note (<150 lines) |
| `00-setup/02-editor-and-playground.md` | 90 | 0/13 | Unset | Thin note (<150 lines) |
| `00-setup/README.md` | 19 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/00-types-and-inference.md` | 102 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/01-primitive-types.md` | 108 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/02-arrays-and-tuples.md` | 117 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/03-objects.md` | 121 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/04-type-aliases.md` | 116 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/05-enums-and-const-objects.md` | 129 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/06-any-and-unknown.md` | 134 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/07-never-and-void.md` | 141 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/08-null-and-undefined.md` | 138 | 0/13 | Unset | Thin note (<150 lines) |
| `01-fundamentals/README.md` | 28 | 0/13 | Unset | Thin note (<150 lines) |
| `02-functions/00-function-types.md` | 156 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `02-functions/01-parameters-and-return-types.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `02-functions/02-callbacks.md` | 147 | 0/13 | Unset | Thin note (<150 lines) |
| `02-functions/03-function-overloads.md` | 141 | 0/13 | Unset | Thin note (<150 lines) |
| `02-functions/04-this-parameters.md` | 156 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `02-functions/README.md` | 23 | 0/13 | Unset | Thin note (<150 lines) |
| `03-unions-and-narrowing/00-union-types.md` | 118 | 0/13 | Unset | Thin note (<150 lines) |
| `03-unions-and-narrowing/01-intersection-types.md` | 141 | 0/13 | Unset | Thin note (<150 lines) |
| `03-unions-and-narrowing/02-literal-types-and-const-assertions.md` | 155 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `03-unions-and-narrowing/03-type-narrowing.md` | 212 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `03-unions-and-narrowing/04-discriminated-unions.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `03-unions-and-narrowing/05-type-guards-and-assertion-functions.md` | 169 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `03-unions-and-narrowing/06-exhaustiveness-checking.md` | 124 | 0/13 | Unset | Thin note (<150 lines) |
| `03-unions-and-narrowing/07-type-assertions-and-satisfies.md` | 166 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `03-unions-and-narrowing/README.md` | 27 | 0/13 | Unset | Thin note (<150 lines) |
| `04-objects-and-interfaces/00-interfaces.md` | 153 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `04-objects-and-interfaces/01-interface-vs-type.md` | 131 | 0/13 | Unset | Thin note (<150 lines) |
| `04-objects-and-interfaces/02-readonly-and-optional-properties.md` | 182 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `04-objects-and-interfaces/03-index-signatures.md` | 170 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `04-objects-and-interfaces/04-structural-typing.md` | 171 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `04-objects-and-interfaces/README.md` | 24 | 0/13 | Unset | Thin note (<150 lines) |
| `05-classes/00-classes.md` | 182 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/01-access-modifiers.md` | 157 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/02-inheritance-and-abstract-classes.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/03-implements.md` | 143 | 0/13 | Unset | Thin note (<150 lines) |
| `05-classes/04-static-members.md` | 163 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/05-getters-and-setters.md` | 160 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/06-mixins.md` | 150 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/07-decorators.md` | 182 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `05-classes/README.md` | 28 | 0/13 | Unset | Thin note (<150 lines) |
| `06-generics/00-generic-functions.md` | 166 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/01-generic-types.md` | 179 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/02-generic-constraints-and-defaults.md` | 189 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/03-keyof-and-typeof.md` | 182 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/04-indexed-access-types.md` | 153 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/05-generic-inference.md` | 197 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `06-generics/README.md` | 26 | 0/13 | Unset | Thin note (<150 lines) |
| `07-utility-types/00-partial-required-readonly.md` | 195 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `07-utility-types/01-pick-omit-record.md` | 197 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `07-utility-types/02-exclude-extract-nonnullable.md` | 151 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `07-utility-types/03-function-and-class-utilities.md` | 217 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `07-utility-types/04-building-custom-utility-types.md` | 207 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `07-utility-types/README.md` | 63 | 0/13 | Unset | Thin note (<150 lines) |
| `08-modules/00-imports-and-exports.md` | 166 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `08-modules/01-es-modules-and-commonjs.md` | 186 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `08-modules/02-barrel-files-and-module-organization.md` | 148 | 0/13 | Unset | Thin note (<150 lines) |
| `08-modules/03-namespaces.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `08-modules/README.md` | 51 | 0/13 | Unset | Thin note (<150 lines) |
| `09-declaration-files/00-declaration-files.md` | 205 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `09-declaration-files/01-ambient-declarations.md` | 205 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `09-declaration-files/02-global-and-module-augmentation.md` | 162 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `09-declaration-files/03-declaration-merging.md` | 204 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `09-declaration-files/04-third-party-types.md` | 171 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `09-declaration-files/README.md` | 67 | 0/13 | Unset | Thin note (<150 lines) |
| `10-advanced-types/00-conditional-types.md` | 192 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/01-distributive-conditional-types.md` | 193 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/02-infer.md` | 194 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/03-mapped-types.md` | 203 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/04-template-literal-types.md` | 209 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/05-recursive-types.md` | 173 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/06-variadic-tuple-types.md` | 170 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/07-branded-types.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/08-type-level-programming.md` | 170 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `10-advanced-types/README.md` | 64 | 0/13 | Unset | Thin note (<150 lines) |
| `11-error-handling/00-catching-and-narrowing-errors.md` | 59 | 0/13 | Unset | Thin note (<150 lines) |
| `11-error-handling/01-custom-errors.md` | 206 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `11-error-handling/02-result-pattern.md` | 205 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `11-error-handling/03-error-handling-strategies.md` | 208 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `11-error-handling/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `12-async-and-iteration/00-event-loop.md` | 171 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/01-promises.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/02-async-await.md` | 222 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/03-async-generics.md` | 233 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/04-iterators-and-generators.md` | 285 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/05-concurrency-patterns.md` | 264 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `12-async-and-iteration/README (2).md` | 66 | 0/13 | Unset | Duplicate/Stray misnamed file |
| `13-compiler-and-tsconfig/00-compiler-options.md` | 158 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `13-compiler-and-tsconfig/01-strict-mode.md` | 201 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `13-compiler-and-tsconfig/02-target-module-and-lib.md` | 189 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `13-compiler-and-tsconfig/03-module-resolution-and-paths.md` | 183 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `13-compiler-and-tsconfig/04-project-references-and-incremental-builds.md` | 215 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `13-compiler-and-tsconfig/05-tsconfig-recipes.md` | 296 | 9/13 | Unset | Incomplete template (9/13 sections) |
| `13-compiler-and-tsconfig/README (3).md` | 76 | 0/13 | Unset | Duplicate/Stray misnamed file |
| `14-type-system-internals/00-type-erasure-and-runtime.md` | 221 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/01-assignability-and-subtyping.md` | 220 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/02-variance.md` | 203 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/03-type-widening-and-inference.md` | 242 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/04-soundness-and-escape-hatches.md` | 207 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/05-compiler-architecture.md` | 184 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `14-type-system-internals/README.md` | 66 | 0/13 | Unset | Thin note (<150 lines) |
| `15-runtime-validation/00-trust-boundaries.md` | 168 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `15-runtime-validation/01-schema-validation.md` | 201 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `15-runtime-validation/02-zod.md` | 323 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `15-runtime-validation/03-other-validation-libraries.md` | 205 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `15-runtime-validation/04-validation-recipes.md` | 347 | 10/13 | Unset | Incomplete template (10/13 sections) |
| `15-runtime-validation/README.md` | 77 | 0/13 | Unset | Thin note (<150 lines) |
| `16-type-safe-apis/00-api-contracts.md` | 163 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/01-request-response-types.md` | 218 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/02-dto-pattern.md` | 236 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/03-pagination-types.md` | 271 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/04-error-response-types.md` | 261 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/05-typed-fetch-and-api-client.md` | 343 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/06-contract-first-apis.md` | 262 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `16-type-safe-apis/README.md` | 72 | 0/13 | Unset | Thin note (<150 lines) |
| `17-design-patterns/00-factory.md` | 238 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/01-builder.md` | 224 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/02-strategy.md` | 175 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/03-adapter.md` | 244 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/04-repository.md` | 287 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/05-dependency-injection.md` | 238 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/06-state-machines.md` | 252 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/07-typed-event-emitter.md` | 276 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `17-design-patterns/README (4).md` | 71 | 0/13 | Unset | Duplicate/Stray misnamed file |
| `18-testing-and-debugging/00-unit-testing.md` | 289 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/01-integration-testing.md` | 252 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/02-mocking.md` | 235 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/03-test-runners.md` | 224 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/04-type-testing.md` | 212 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/05-debugging.md` | 220 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/06-reading-type-errors.md` | 253 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `18-testing-and-debugging/README (1).md` | 74 | 0/13 | Unset | Duplicate/Stray misnamed file |
| `19-react-and-frontend/00-component-props-and-children.md` | 274 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/01-event-types.md` | 226 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/02-hooks.md` | 285 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/03-context.md` | 224 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/04-refs.md` | 267 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/05-forms.md` | 294 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/06-generic-and-polymorphic-components.md` | 265 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/07-server-state-tanstack-query.md` | 302 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/08-client-state-zustand.md` | 321 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/09-nextjs.md` | 317 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `19-react-and-frontend/README.md` | 92 | 0/13 | Unset | Thin note (<150 lines) |
| `20-nodejs-backend/00-node-types.md` | 254 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/01-config-and-environment.md` | 216 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/02-express.md` | 295 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/03-middleware.md` | 300 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/04-service-and-repository-layers.md` | 293 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/05-databases.md` | 260 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/06-redis-and-caching.md` | 289 | 0/13 | Unset | Incomplete template (0/13 sections) |
| `20-nodejs-backend/07-authentication-and-authorization.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `20-nodejs-backend/08-nestjs.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `20-nodejs-backend/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/00-linting-and-formatting.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/01-build-and-bundling.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/02-monorepos.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/03-publishing-packages.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/04-js-to-ts-migration.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/05-ci-and-deployment.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/06-logging-and-observability.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `21-production-tooling/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `22-performance/00-type-checking-performance.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `22-performance/01-build-performance.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `22-performance/02-runtime-performance.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `22-performance/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `23-security/00-unsafe-types-and-assertions.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `23-security/01-input-validation.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `23-security/02-dependency-security.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `23-security/03-security-checklist.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `23-security/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `24-best-practices/00-type-design-principles.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `24-best-practices/01-common-mistakes-and-anti-patterns.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `24-best-practices/02-project-structure.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `24-best-practices/03-code-style-and-conventions.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `24-best-practices/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `25-real-world-patterns/00-domain-modeling.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `25-real-world-patterns/01-filtering-and-sorting.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `25-real-world-patterns/02-state-management.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `25-real-world-patterns/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/01-cli-tool/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/02-type-safe-api-client/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/03-rest-api/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/04-authentication-system/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/05-react-dashboard/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/06-nestjs-api/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/07-fullstack-application/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/exercises/00-type-challenges.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/exercises/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `26-projects/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/00-typescript-basics.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/01-type-system-and-runtime.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/02-generics.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/03-utility-types.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/04-advanced-types.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/05-narrowing.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/06-compiler-and-tsconfig.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/07-react-typescript.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/08-node-typescript.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/09-output-based-questions.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/10-design-and-architecture-questions.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/11-coding-challenges.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `27-interview/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/00-syntax-and-types.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/01-utility-types.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/02-generics.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/03-narrowing.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/04-tsconfig.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/05-react-typescript.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/06-node-typescript.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `28-cheatsheets/README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `CONTRIBUTING.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `GLOSSARY.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `INDEX.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `README.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `ROADMAP.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `templates/concept-template.md` | 1 | 0/13 | Empty (0 B) | Empty file (0 bytes) |
| `TS_FACTS.md` | 116 | 3/13 | Unset | Thin note (<150 lines) |
