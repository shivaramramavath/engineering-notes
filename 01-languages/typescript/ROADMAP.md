# TypeScript Engineering Mastery Roadmap

> Structured career learning paths for engineering progression: from JavaScript transition to Staff / Principal Software Architect.

---

## 🗺️ Visual Learning Trajectories

```text
  Level 1: Language Core & Modern Setup
  [00-setup] ──> [01-basics] ──> [02-functions] ──> [03-objects] ──> [04-unions]
                                                                        │
  Level 2: Generics & Metaprogramming                                   ▼
  [05-classes] <── [06-generics] <── [07-utility-types] <── [10-advanced-types]
        │
        ▼
  Level 3: Full-Stack & Systems Architecture
  [08-modules] ──> [13-tsconfig] ──> [15-zod] ──> [19-react] ──> [20-node]
                                                                     │
  Level 4: Staff Architecture & Compiler Engineering                 ▼
  [26-projects] <── [29-typed-dsa] <── [22-performance] <── [14-internals]
```

---

## Track 1: Modern Foundations (Junior / Transitioning from JS)
*Goal: Eliminate `any`, configure strict tsconfig, and write type-safe functions, objects, and unions.*

1. **Environment Setup**: Master Node 22 type-stripping, Vite bundler resolution, and tsconfig strict baseline ([00-setup](./00-setup/README.md)).
2. **Primitive Types & Invariants**: Understand top and bottom types (`unknown`, `never`), literal types, and tuples ([01-basics-and-primitive-types](./01-basics-and-primitive-types/README.md)).
3. **Function Signatures**: Overloads, optional/default parameters, and type predicates ([02-functions](./02-functions/README.md)).
4. **Structural Objects**: Structural subtyping (duck typing), interfaces vs types, and index signatures ([03-objects-and-interfaces](./03-objects-and-interfaces/README.md)).
5. **Control Flow Analysis**: Discriminated unions and exhaustive pattern matching ([04-union-and-intersection-types](./04-union-and-intersection-types/README.md)).

---

## Track 2: Advanced Type Systems & Metaprogramming (Mid -> Senior)
*Goal: Author reusable generic libraries, mapped transformations, and type-level assertions.*

1. **Generic Abstractions**: Type parameter constraints (`T extends HasId`), default parameters, and the `infer` keyword ([06-generics](./06-generics/README.md)).
2. **Standard Utilities**: Master standard homomorphic mapped types (`Partial`, `Pick`, `Omit`, `Awaited`) ([07-utility-types](./07-utility-types/README.md)).
3. **Advanced Type-Level Computation**: Conditional types, distributive evaluations, and template literal type parsing ([10-advanced-types](./10-advanced-types/README.md)).
4. **Error Handling Architecture**: The typed `Result<T, E>` pattern vs exception throwing ([11-error-handling](./11-error-handling/README.md)).
5. **Testing Type Spaces**: Vitest `expectTypeOf`, assignability testing, and negative unit tests with `@ts-expect-error` ([18-testing](./18-testing/README.md)).

---

## Track 3: Full-Stack Enterprise Architecture (Senior -> Lead)
*Goal: Architect scalable monorepos, end-to-end type-safe contracts, and robust frontend/backend systems.*

1. **Project References**: Subproject compilation caching with `composite: true` and `.tsbuildinfo` ([13-compiler-and-configuration](./13-compiler-and-configuration/README.md)).
2. **Runtime Contract Boundaries**: Zod schema validation, branded types, and boot environment validation ([15-runtime-type-checking](./15-runtime-type-checking/README.md)).
3. **Type-Safe Fullstack APIs**: tRPC architecture, type-safe Fetch clients, and OpenAPI sync ([16-type-safe-apis](./16-type-safe-apis/README.md)).
4. **React Component Systems**: Polymorphic components, custom hooks, and context typing without `React.FC` ([19-react-and-frontend](./19-react-and-frontend/README.md)).
5. **Backend Data Layers**: Express/Fastify request augmentation and Drizzle ORM query typing ([20-nodejs-backend](./20-nodejs-backend/README.md)).
6. **Production Tooling**: typescript-eslint v8 Flat Config and Biome linting/formatting ([21-production-tooling](./21-production-tooling/README.md)).

---

## Track 4: Compiler Architecture & Performance Optimization (Lead -> Principal / Staff)
*Goal: Optimize compiler check times, navigate compiler internals, and master advanced algorithms.*

1. **Compiler Pipeline Internals**: Scanner, Parser (AST), Binder (Symbols), Checker (Types), and Emitter ([14-compiler-internals](./14-compiler-internals/README.md)).
2. **Compilation Profiling**: Diagnosing slow type checks with `tsc --extendedDiagnostics` and `@typescript/analyze-trace` ([22-performance](./22-performance/README.md)).
3. **Typed Data Structures & Algorithms**: Generic heaps, topological dependency sorting, and type-space complexity ([29-typed-data-structures-and-algorithms](./29-typed-data-structures-and-algorithms/README.md)).
4. **Hands-on Production Projects**: Build and verify all 14 end-to-end reference applications ([26-projects](./26-projects/README.md)).
5. **Interview & Live Coding Mastery**: Tackle system design scenarios, tricky compiler questions, and type challenges ([27-interview](./27-interview/README.md)).
