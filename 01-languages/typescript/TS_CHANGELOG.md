# TypeScript Engineering Curriculum Changelog (TS_CHANGELOG.md)

> Detailed chronological record of all structural modifications, corrections, note expansions, and audit remediations across the TypeScript curriculum.

---

## [Batch 6] - 2026-11-24 - Runtime Validation, APIs, Patterns & Testing (Modules 15, 16, 17, 18)
### Added / Completed
- **15-runtime-validation/**:
  - `00-trust-boundaries.md` (454 lines)
  - `01-schema-validation.md` (408 lines)
  - `02-zod.md` (428 lines)
  - `03-other-validation-libraries.md` (423 lines - Valibot, ArkType, TypeBox)
  - `04-validation-recipes.md` (460 lines)
  - `README.md` (57 lines)
- **16-type-safe-apis/**:
  - `00-api-contracts.md` (389 lines)
  - `01-request-response-types.md` (436 lines)
  - `02-dto-pattern.md` (559 lines)
  - `03-pagination-types.md` (581 lines)
  - `04-error-response-types.md` (620 lines - RFC 7807/9457 Problem Details)
  - `05-typed-fetch-and-api-client.md` (562 lines)
  - `06-contract-first-apis.md` (541 lines - OpenAPI, tRPC)
  - `README.md` (22 lines)
- **17-design-patterns/**:
  - `00-factory.md` (578 lines)
  - `01-builder.md` (617 lines)
  - `02-strategy.md` (494 lines)
  - `03-adapter.md` (517 lines)
  - `04-repository.md` (580 lines)
  - `05-dependency-injection.md` (541 lines)
  - `06-state-machines.md` (507 lines)
  - `07-typed-event-emitter.md` (535 lines)
  - `README.md` (23 lines)
- **18-testing-and-debugging/**:
  - `00-unit-testing.md` (490 lines - Vitest, AAA pattern)
  - `01-integration-testing.md` (485 lines - Supertest, Testcontainers)
  - `02-mocking.md` (487 lines - Vitest spies, MSW)
  - `03-test-runners.md` (424 lines - Vitest, Jest, node:test)
  - `04-type-testing.md` (428 lines - expectTypeOf, @ts-expect-error)
  - `05-debugging.md` (457 lines - Source maps, VS Code launch, V8 inspector)
  - `06-reading-type-errors.md` (398 lines - TS2322, TS2345, TS7053)
  - `README.md` (22 lines)

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


### Batch 3: Classes, Generics & Utility Types (Modules 05, 06, 07)
- **Module 05: Classes (11 files, 100% Complete)**:
  - `00-classes.md` (533 lines): Class field declarations, definite assignment assertions (`!`), constructor initialization order.
  - `01-access-modifiers.md` (571 lines): TypeScript `private`/`protected` compile-time checks vs ECMAScript `#private` runtime hard privacy.
  - `02-inheritance-and-abstract-classes.md` (624 lines): Abstract class definitions, abstract member constraints, Liskov substitution verification.
  - `03-implements.md` (594 lines): Interface implementation contracts, multiple interface conformance, separating static construct signatures from instance types.
  - `04-static-members.md` (535 lines): Static properties/methods, static initialization blocks (`static {}`), inheritance of static members.
  - `05-getters-and-setters.md` (561 lines): Accessor pairs, asymmetric getter/setter types (TS 4.3+), prototype property descriptors.
  - `06-mixins.md` (597 lines): Constructor constraint pattern (`Constructor<T>`), composable behavioral pipelines, avoiding multiple inheritance limits.
  - `07-decorators.md` (616 lines): Stage 3 ECMAScript decorators (TS 5.0+), decorator context (`ClassMethodDecoratorContext`, `addInitializer`, `metadata` TS 5.2+), migration from legacy `experimentalDecorators`.
  - `08-constructor-parameter-properties-and-erasable-syntax.md` (563 lines - NEW): Parameter properties vs Node 22 native type stripping, syntax incompatibility, and TS 5.8+ `--erasableSyntaxOnly` boundary.
  - `09-class-expressions-and-structural-typing.md` (522 lines - NEW): Anonymous class expressions, instance type duck typing, nominal branding techniques.
  - Authored comprehensive `05-classes/README.md` (56 lines).

- **Module 06: Generics (10 files, 100% Complete)**:
  - `00-generic-functions.md` (494 lines): Type parameters, contextual inference, generic constraints, multi-parameter dependencies.
  - `01-generic-types.md` (557 lines): Generic interfaces, type aliases, generic classes, static member type isolation.
  - `02-generic-constraints-and-defaults.md` (512 lines): `extends` constraints, default type arguments, interdependent generic defaults.
  - `03-keyof-and-typeof.md` (490 lines): Type-space `typeof`, `keyof` property unions, dictionary types, `as const` integration.
  - `04-indexed-access-types.md` (460 lines): `T[K]` lookup types, tuple indexing, array element lookup with `T[number]`, chained access.
  - `05-generic-inference.md` (417 lines): Conditional types, `infer` pattern matching, distributive conditional rules, distributive `never` handling.
  - `06-const-type-parameters.md` (489 lines - NEW): TS 5.0+ `<const T>` modifier, deep literal tuple and object inference without caller `as const`.
  - `07-type-parameter-variance-annotations.md` (459 lines - NEW): TS 4.7+ `in` and `out` variance annotations, covariance, contravariance, invariance, compiler subtyping acceleration.
  - `08-generic-instantiation-limits-and-compiler-perf.md` (413 lines - NEW): `TS2589` error, tail-call recursion optimization (TS 4.5+), accumulator tuples, `--extendedDiagnostics` profiling.
  - Authored comprehensive `06-generics/README.md` (42 lines).

- **Module 07: Utility Types (9 files, 100% Complete)**:
  - `00-partial-required-readonly.md` (499 lines): Property modifiers, `+` and `-` token modifiers, `exactOptionalPropertyTypes` interaction.
  - `01-pick-omit-record.md` (434 lines): Whitelist/blacklist projections, `StrictOmit` implementation, DTO architectural design.
  - `02-exclude-extract-nonnullable.md` (389 lines): Set operations on unions, distributive conditional types, TS 4.8+ `T & {}` definition of `NonNullable`.
  - `03-function-and-class-utilities.md` (470 lines): `Parameters`, `ReturnType`, `ConstructorParameters`, `InstanceType`, `ThisParameterType`, `OmitThisParameter`.
  - `04-building-custom-utility-types.md` (490 lines): Key remapping with `as`, filtering keys by value type, homomorphic mapping, `Prettify` type helper.
  - `05-awaited-and-promise-unwrapping.md` (420 lines - NEW): TS 4.5+ `Awaited<T>`, recursive unwrapping of Promises and Promises/A+ Thenables.
  - `06-string-manipulation-types.md` (398 lines - NEW): Intrinsic string types (`Uppercase`, `Lowercase`, `Capitalize`, `Uncapitalize`), template literal types, string pattern extraction.
  - `07-deep-utility-types.md` (512 lines - NEW): Recursive `DeepPartial`, `DeepReadonly`, `DeepRequired`, `DeepMutable`, guards for `Function`, `Date`, and collections.
  - Authored comprehensive `07-utility-types/README.md` (42 lines).


### Batch 4: Modules, Declarations & Advanced Types (Modules 08, 09, 10)
- **Module 08: Modules (9 files, 100% Complete)**:
  - `00-imports-and-exports.md` (384 lines): Named/default exports, type elision mechanisms, re-export pipelines, dynamic imports.
  - `01-es-modules-and-commonjs.md` (379 lines): `esModuleInterop`, default import synthesis, synthetic module records, dual-package hazards.
  - `02-barrel-files-and-module-organization.md` (374 lines): Facade patterns, tree-shaking impacts, circular dependency avoidance, subpaths.
  - `03-namespaces.md` (431 lines): Legacy namespaces, IIFE emission, declaration merging with functions/enums, `isolatedModules` constraints.
  - `04-verbatim-module-syntax-and-type-modifiers.md` (364 lines - NEW): TS 5.0+ `verbatimModuleSyntax`, Node 22 native type stripping (`--experimental-strip-types`) compatibility, `import type`.
  - `05-package-json-exports-and-resolution-modes.md` (458 lines - NEW): Modern resolution modes (`bundler`, `node16`, `nodenext`), subpath exports, condition matching order (`types` first).
  - `06-import-attributes-and-json-modules.md` (383 lines - NEW): TS 5.3+ Stage 3 `with { type: "json" }`, dynamic asset import security, MIME-type confusion defenses.
  - `07-isolated-declarations.md` (398 lines - NEW): TS 5.5+ `--isolatedDeclarations`, explicit export boundaries, unblocking parallel compilers (`tsgo`, Oxlint).
  - Authored comprehensive `08-modules/README.md` (42 lines).

- **Module 09: Declaration Files (9 files, 100% Complete)**:
  - `00-declaration-files.md` (384 lines): `.d.ts` anatomy, `declarationMap`, `emitDeclarationOnly`, project references optimization.
  - `01-ambient-declarations.md` (388 lines): `declare` keyword, global variables, window extensions, `NodeJS.ProcessEnv` typing.
  - `02-global-and-module-augmentation.md` (396 lines): Non-invasive third-party extensions (`declare module "pkg"`, `declare global`), Express, Fastify, Vitest matchers.
  - `03-declaration-merging.md` (422 lines): Interface merging, method overload ordering rules, namespace companion merging with functions and enums.
  - `04-third-party-types.md` (356 lines): DefinitelyTyped ecosystem (`@types/`), `skipLibCheck`, `typeRoots`, scoping with `types` array.
  - `05-authoring-declarations-for-javascript-libraries.md` (401 lines - NEW): Step-by-step authoring for untyped npm packages, UMD `export as namespace`, `export =` CommonJS signatures.
  - `06-types-versions-and-downleveling.md` (391 lines - NEW): `typesVersions` in `package.json`, automated `downlevel-dts`, multi-version compiler support.
  - `07-definitely-typed-workflow-and-best-practices.md` (408 lines - NEW): DefinitelyTyped monorepo architecture, `dtslint` verification, PR approval bots, types-publisher automated release.
  - Authored comprehensive `09-declaration-files/README.md` (42 lines).

- **Module 10: Advanced Types (13 files, 100% Complete)**:
  - `00-conditional-types.md` (397 lines): Ternary type logic, deferred evaluation, assignability checking, type equality checkers.
  - `01-distributive-conditional-types.md` (374 lines): Bare type parameter rule, union filtering, tuple wrapping to disable distribution, the `never` trap.
  - `02-infer.md` (351 lines): Pattern matching, covariant vs contravariant inference positions, TS 4.7+ constrained `infer`.
  - `03-mapped-types.md` (393 lines): Homomorphic mapped types, `+` and `-` modifier tokens, key remapping with `as`, filtering with `never`.
  - `04-template-literal-types.md` (391 lines): String interpolation in types, Cartesian product unions, string pattern extraction.
  - `05-recursive-types.md` (429 lines): Self-referential type aliases, JSON models, tail-call recursion optimization (TS 4.5+), depth guards.
  - `06-variadic-tuple-types.md` (391 lines): Generic tuple spreads (`...T`), flexible rest element positioning, labeled tuple elements.
  - `07-branded-types.md` (394 lines): Nominal typing via phantom brands, unique symbols, zero runtime overhead, smart constructors.
  - `08-type-level-programming.md` (379 lines): Turing completeness, type-space functional programming, boolean logic, accumulator pattern.
  - `09-type-level-parsers-and-state-machines.md` (431 lines - NEW): Compile-time string DSL parsing, state transition matrices, query string tokenizers, SQL column extraction.
  - `10-deep-keys-and-path-navigation.md` (455 lines - NEW): Exhaustive nested dotted paths (`DeepPaths`), path value extraction (`DeepPathValue`), type-safe lens setters.
  - `11-type-arithmetic-and-tuple-manipulation.md` (354 lines - NEW): Peano number simulation, tuple length arithmetic (`Add`, `Subtract`, `GreaterThan`), tensor dimension verification.
  - Authored comprehensive `10-advanced-types/README.md` (50 lines).

## [Batch 5] - 2026-11-24 - Modules 11, 12, 13, 14 (Error Handling, Async & Iteration, Compiler & tsconfig, Type System Internals)

### Added & Expanded
- **Module 11: Error Handling** (6 concept notes + README, 3,090 total lines):
  - `00-catching-and-narrowing-errors.md` (519 lines, 13 sections): `catch (e: unknown)`, `useUnknownInCatchVariables`, prototype narrowing, cross-realm error guards, `Error.cause` chaining.
  - `01-custom-errors.md` (567 lines, 13 sections): Subclassing `Error`, prototype downleveling (`Object.setPrototypeOf`), nominal branding, `.toJSON()` serialization, V8 stack frame pruning.
  - `02-result-pattern.md` (473 lines, 13 sections): Discriminated unions, `ok`/`err` constructors, functor `map`, monad `flatMap`, eliminating runtime exceptions, sub-millisecond trading case study.
  - `03-error-handling-strategies.md` (520 lines, 13 sections): Operational errors vs programmer bugs, global boundaries, fail-fast vs graceful degradation, structured logging, PII sanitization.
  - `04-neverthrow-and-functional-error-handling.md` (499 lines, 13 sections): `neverthrow` library patterns, `ResultAsync`, `fromPromise`, `fromThrowable`, `safeTry` coroutines, parallel combining.
  - `05-effect-ts-error-management-and-defects.md` (456 lines, 13 sections): `Effect<A, E, R>`, expected failures vs defects (`die`), `Data.TaggedError`, `catchTag`, `Cause<E>`, fiber recovery.
  - `README.md` (63 lines): Comprehensive module roadmap, relationship diagram, and architectural principles.

- **Module 12: Asynchronous Programming and Iteration** (8 concept notes + README, 3,964 total lines):
  - `00-event-loop.md` (505 lines, 13 sections): Event loop mechanics, microtasks vs macrotasks, event loop starvation, cross-environment timer handles, queueMicrotask batching.
  - `01-promises.md` (459 lines, 13 sections): `Promise<T>`, executor typing, `Promise.all`, `Promise.allSettled`, `Promise.race`, `Promise.any`, AggregateError handling.
  - `02-async-await.md` (398 lines, 13 sections): `async`/`await`, `Awaited<T>` recursive unwrapping, top-level `await`, floating promises prevention, transactional execution.
  - `03-async-generics.md` (567 lines, 13 sections): Higher-order async wrappers, in-flight deduplication (thundering-herd prevention), async semaphores, batched queues.
  - `04-iterators-and-generators.md` (480 lines, 13 sections): `Generator<TYield, TReturn, TNext>`, `AsyncIterable<T>`, `for-await-of`, coroutine state machines, S3 audit streaming.
  - `05-concurrency-patterns.md` (494 lines, 13 sections): Node.js `worker_threads`, structured cloning, thread pools, `SharedArrayBuffer` with `Atomics` spinlocks.
  - `06-explicit-resource-management-and-using.md` (484 lines, 13 sections): TypeScript 5.2+ `using` and `await using`, `Disposable`, `AsyncDisposable`, `Symbol.dispose`, `DisposableStack`, `SuppressedError`.
  - `07-abortsignal-and-cancellation-tokens.md` (491 lines, 13 sections): `AbortController`, `AbortSignal.timeout`, `AbortSignal.any`, `signal.throwIfAborted()`, autocomplete race prevention.
  - `README.md` (75 lines): Module navigation, flowcharts, and concurrent programming best practices.

- **Module 13: Compiler Internals, tsconfig, and Tooling** (8 concept notes + README, 3,524 total lines):
  - `00-compiler-options.md` (436 lines, 13 sections): Schema anatomy, `include`/`exclude`, `extends`, emit controls (`noEmit`, `emitDeclarationOnly`), sourcemaps, JSX modes.
  - `01-strict-mode.md` (399 lines, 13 sections): `"strict": true` family breakdown, pedantic flags (`noUncheckedIndexedAccess`, `exactOptionalPropertyTypes`), portfolio crash case study.
  - `02-target-module-and-lib.md` (400 lines, 13 sections): Syntax downleveling (`ES2022`), module formats (`NodeNext`, `bundler`, `preserve`), declaration header sets (DOM vs Node).
  - `03-module-resolution-and-paths.md` (417 lines, 13 sections): `NodeNext` mandatory `.js` extensions, `moduleResolution: "bundler"`, `package.json` `"exports"` and subpath imports (`#`).
  - `04-project-references-and-incremental-builds.md` (494 lines, 13 sections): `composite: true`, `references: [{ path }]`, `tsc --build`, `.tsbuildinfo` caching, DAG cycle detection.
  - `05-tsconfig-recipes.md` (481 lines, 13 sections): Production recipes for Node 22 ESM, Vite React 19, Next.js 14/15 App Router, dual-package libraries, monorepo base.
  - `06-extended-diagnostics-and-generate-trace.md` (395 lines, 13 sections): `--extendedDiagnostics`, `--generateTrace`, Chromium Perfetto flamegraphs, `@typescript/analyze-trace`, Cartesian explosion fixes.
  - `07-typescript-7-native-go-compiler-tsgo.md` (423 lines, 13 sections): TypeScript 7.0 native Go port (`tsgo`), parallel multi-threaded type checking, 10x-15x performance gains, isolated declarations synergy.
  - `README.md` (79 lines): Complete compiler module reference, architecture diagrams, and optimization rules.

- **Module 14: Type System Internals and Extensibility** (9 concept notes + README, 4,142 total lines):
  - `00-type-erasure-and-runtime.md` (438 lines, 13 sections): Type erasure mechanics, compile-time vs runtime, Node 22 `--experimental-strip-types`, TC39 type annotations proposal.
  - `01-assignability-and-subtyping.md` (423 lines, 13 sections): Structural subtyping, assignability relation, identity, fresh literal excess property checks, weak type rules.
  - `02-variance.md` (411 lines, 13 sections): Covariance, contravariance, invariance, method bivariance, TS 4.7 explicit variance annotations (`in`/`out`).
  - `03-type-widening-and-inference.md` (400 lines, 13 sections): Literal widening, `let` vs `const`, property widening, `as const` assertions, top-down contextual typing.
  - `04-soundness-and-escape-hatches.md` (394 lines, 13 sections): Deliberate non-sound design goals (Non-Goal 3), array covariance holes, escape hatches (`any`, `unknown`, `as`, `!`, `@ts-expect-error`).
  - `05-compiler-architecture.md` (450 lines, 13 sections): The 5 compilation phases: Scanner -> Parser -> Binder -> Type Checker -> Emitter, AST, Symbols, FlowGraphs.
  - `06-ast-and-compiler-api.md` (498 lines, 13 sections): Programmatic Compiler API, `ts.createProgram`, `ts.factory` immutable nodes, AST visitors, automated codemod runners.
  - `07-ts-morph-and-programmatic-ast-manipulation.md` (469 lines, 13 sections): `ts-morph` Project, wrapped nodes, symbol navigation, automated migrations, in-memory testing.
  - `08-writing-a-custom-language-service-plugin.md` (563 lines, 13 sections): TSServer plugins, intercepting diagnostics, custom autocompletions, hover quick info, quick fixes in VS Code.
  - `README.md` (86 lines): Module architecture, learning paths, and compiler extensibility best practices.


## [Batch 7] - 2026-11-24 - Ecosystem, Tooling & Real-World (Modules 19, 20, 21, 22, 23, 24, 25)
### Added / Completed
- **19-react-and-frontend/** (10 concept notes + README, 5,147 total lines):
  - `00-component-props-and-children.md` (547 lines) - React 19 function components, `ReactNode` vs `ReactElement`, children typing, polymorphic props
  - `01-event-types.md` (486 lines) - Synthetic events, change handlers, keyboard/mouse events, generic event wrappers
  - `02-hooks.md` (548 lines) - useState, useReducer, useEffect, useCallback, useMemo, custom hook return types
  - `03-context.md` (548 lines) - Generic context factories, null checks, provider typing, splitting state and dispatch
  - `04-refs.md` (526 lines) - RefObject vs MutableRefObject, React 19 prop refs, callback refs, imperative handles
  - `05-forms.md` (522 lines) - React Hook Form, Zod resolver integration, type-safe error messages, nested field arrays
  - `06-generic-and-polymorphic-components.md` (499 lines) - `as` prop, ComponentPropsWithoutRef, slot patterns, forwardRef generics
  - `07-server-state-tanstack-query.md` (499 lines) - QueryKey factory, useQuery, useMutation, optimistic updates, query cancellation
  - `08-client-state-zustand.md` (557 lines) - Curried create, slices pattern, immutable updates, selectors, middleware typing
  - `09-nextjs.md` (518 lines) - App Router, async Server Components, Next.js 15 dynamic params promises, Server Actions
  - `README.md` (25 lines)
- **20-nodejs-backend/** (9 concept notes + README, 4,378 total lines):
  - `00-node-types.md` (431 lines) - @types/node, Buffer, Streams, EventEmitter, global declarations, Node 22 type stripping
  - `01-config-and-environment.md` (435 lines) - ProcessEnv typing, Zod schema validation, dotenv-safe, typed config services
  - `02-express.md` (492 lines) - Typed Request, Response, ParamsDictionary, typed query and body, async error handling
  - `03-middleware.md` (513 lines) - Augmenting express namespace, auth middleware, rate limiting, error middleware
  - `04-service-and-repository-layers.md` (468 lines) - Interface-driven architecture, dependency injection, Result types, repository pattern
  - `05-databases.md` (476 lines) - Prisma client types, Drizzle ORM `$inferSelect` / `$inferInsert`, Kysely SQL query builder
  - `06-redis-and-caching.md` (503 lines) - ioredis typing, cache serialization, cache-aside pattern, distributed locks with redlock
  - `07-authentication-and-authorization.md` (445 lines) - JWT verification, Argon2 password hashing, RBAC, session typing
  - `08-nestjs.md` (491 lines) - Decorators, modules, dependency injection containers, DTO validation with class-validator
  - `README.md` (24 lines)
- **21-production-tooling/** (7 concept notes + README, 3,506 total lines):
  - `00-linting-and-formatting.md` (546 lines) - Biome, Oxlint, typescript-eslint v8 Flat Config (`eslint.config.mjs`)
  - `01-build-and-bundling.md` (527 lines) - tsup, esbuild, swc, tsc dual-track, TS 5.5 `isolatedDeclarations`
  - `02-monorepos.md` (489 lines) - pnpm workspaces, TypeScript Project References (`composite: true`), Turborepo caching
  - `03-publishing-packages.md` (499 lines) - package.json conditional exports, publint, @arethetypeswrong/cli, npm provenance
  - `04-js-to-ts-migration.md` (506 lines) - allowJs, checkJs, JSDoc typing, gradual migration roadmap
  - `05-ci-and-deployment.md` (482 lines) - GitHub Actions matrix, .tsbuildinfo caching, multi-stage Docker builds
  - `06-logging-and-observability.md` (531 lines) - Pino structured logging, OpenTelemetry distributed tracing, AsyncLocalStorage
  - `README.md` (17 lines)
- **22-performance/** (3 concept notes + README, 1,330 total lines):
  - `00-type-checking-performance.md` (447 lines) - Profiling with `--generateTrace`, `@typescript/analyze-trace`, union explosion, interface vs type caching
  - `01-build-performance.md` (391 lines) - Incremental builds, .tsbuildinfo serialization, signature hashing, Project References
  - `02-runtime-performance.md` (479 lines) - V8 hidden classes, monomorphic call sites, avoid delete operator, enum overhead vs `as const`
  - `README.md` (13 lines)
- **23-security/** (4 concept notes + README, 1,785 total lines):
  - `00-unsafe-types-and-assertions.md` (431 lines) - Dangers of `any`, double assertions (`as unknown as T`), non-null assertion `!`, satisfies
  - `01-input-validation.md` (461 lines) - Zero-trust boundaries, Zod `.strict()`, ArkType, TypeBox, prototype pollution defense
  - `02-dependency-security.md` (409 lines) - Ambient declaration poisoning, lockfile pinning, `ignore-scripts=true`, pnpm isolation
  - `03-security-checklist.md` (470 lines) - 5-pillar production readiness checklist, `noUncheckedIndexedAccess`, non-root Docker user
  - `README.md` (14 lines)
- **24-best-practices/** (4 concept notes + README, 1,766 total lines):
  - `00-type-design-principles.md` (475 lines) - Parse Don't Validate, making illegal states unrepresentable, nominal branding, Result types
  - `01-common-mistakes-and-anti-patterns.md` (417 lines) - Excess property checks traps, shallow readonly mutations, `{}` anti-pattern
  - `02-project-structure.md` (444 lines) - Feature-sliced design, barrel file performance traps, circular import prevention via madge
  - `03-code-style-and-conventions.md` (416 lines) - Naming conventions, no `I` interface prefix, explicit boundary return types
  - `README.md` (14 lines)
- **25-real-world-patterns/** (3 concept notes + README, 1,579 total lines):
  - `00-domain-modeling.md` (550 lines) - Domain-Driven Design (DDD), Value Objects, Entities, Aggregates, domain invariants
  - `01-filtering-and-sorting.md` (493 lines) - Type-safe query criteria, dynamic operator mapping, cursor vs offset pagination
  - `02-state-management.md` (523 lines) - Finite state machines, discriminated union reducers, Zustand stores, optimistic updates
  - `README.md` (13 lines)
