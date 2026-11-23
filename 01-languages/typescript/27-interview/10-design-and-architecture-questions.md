# Module 27: TypeScript System Design & Architecture Questions

> **Topic Overview**: High-level architectural interview challenges for Principal & Lead Engineers: designing type-safe SDKs, API contract sharing, monorepo type strategies, and micro-frontend type orchestration.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Interview & Preparation |
| **Target Level** | Lead / Principal / Staff Architect |
| **TypeScript Baseline** | TypeScript 5.0 - 5.8+ |
| **Focus Areas** | SDK Architecture, API Contracts, Monorepo Type Boundaries, Performance |

---

## Architectural Challenges

### Design 1: End-to-End Type-Safe RPC Protocol without Code Generation
**Question**: How would you design a type-safe client-server RPC framework in TypeScript (similar to tRPC) using pure type inference without running build-time code generators like OpenAPI or Protobuf?
<details>
<summary><strong>Solution & Architectural Design</strong></summary>

**Core Strategy**:
1. Server defines router procedures as pure typed functions with input/output validation (e.g. Zod).
2. Export ONLY the server router's type definition (`export type AppRouter = typeof appRouter;`).
3. Client imports `type AppRouter` (zero runtime JavaScript leakage).
4. Client uses recursive mapped proxy types to dynamically provide autocomplete for routes.

```typescript
// Shared Contract
export type Procedure<TInput, TOutput> = {
  _input: TInput;
  _output: TOutput;
  (input: TInput): Promise<TOutput>;
};

export type Router = {
  [key: string]: Procedure<any, any> | Router;
};

// Client Proxy Typing:
export type CreateClient<R extends Router> = {
  [K in keyof R]: R[K] extends Procedure<infer I, infer O>
    ? (input: I) => Promise<O>
    : R[K] extends Router
    ? CreateClient<R[K]>
    : never;
};
```
</details>

---

### Design 2: Managing Shared Types in a Multi-Team Polyglot Monorepo
**Question**: How should type definitions be distributed across a large monorepo with 50+ packages, balancing compilation speed against dependency coupling?
<details>
<summary><strong>Solution & Architectural Design</strong></summary>

**Architectural Blueprint**:
1. **Never import source `.ts` across package boundaries**: Direct source imports force the consuming package to re-parse and re-typecheck the dependency's AST repeatedly.
2. **Project References with Emit on Change**: Use `tsc -b` where internal packages emit `.d.ts` and `.d.ts.map`. Consuming apps depend on declaration files.
3. **Contract First Boundary**: Shared schemas reside in an isolated `contracts` package with zero heavy runtime dependencies (e.g., zero DB drivers, zero UI libs).
4. **Isolated Modules**: Ensure all packages pass `"isolatedModules": true` and `"verbatimModuleSyntax": true` to allow independent esbuild/Vite/swc bundling.
</details>\n