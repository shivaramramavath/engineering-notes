# Linting and Formatting in Modern TypeScript: Biome, Oxlint, and typescript-eslint v8

> Comprehensive guide to static analysis, linting, formatting, and AST-based rule enforcement in TypeScript using typescript-eslint v8 (Flat Config), Biome, and Oxlint.

| Metadata | Details |
| :--- | :--- |
| **Category** | Production Tooling |
| **Difficulty** | Intermediate to Advanced |
| **Prerequisites** | [14-compiler-internals/00-compiler-architecture.md](../14-compiler-internals/00-compiler-architecture.md), [13-compiler-options/00-tsconfig-json.md](../13-compiler-options/00-tsconfig-json.md) |
| **TypeScript Baseline** | TS 5.0+, Node.js 20/22 LTS |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

While the TypeScript compiler (`tsc`) guarantees type correctness across your codebase, it intentionally limits its scope to type checking and emission. It does not enforce:
- Code style and formatting consistency (tabs vs. spaces, semicolon policies).
- Idiomatic language practices (avoiding floating promises, unused parameters, or redundant type assertions).
- Security and safety lint rules (detecting regex injection, unsafe DOM manipulation, or unsafe casting).

Historically, the ecosystem relied on TSLint, which was deprecated in 2019 in favor of `typescript-eslint`. Today, production toolchains are undergoing a major architectural evolution:
1. **typescript-eslint v8**: The industry standard for deep type-aware linting utilizing ESLint's new Flat Configuration system (`eslint.config.js`). It connects ESLint rules directly to the TypeScript compiler's type checker (`ts.TypeChecker`).
2. **Biome**: A unified, high-performance toolchain written in Rust (formerly Rome) providing formatting and linting at sub-millisecond speeds.
3. **Oxlint**: An ultra-fast JavaScript/TypeScript linter written in Rust designed as a drop-in front-end filter to catch common syntax and semantic errors before triggering heavy type-aware ESLint passes.

---

## 2. Core Concepts

### Syntax-Only vs. Type-Aware Linting
- **Syntax-Only Linting**: Analyzes the raw Abstract Syntax Tree (AST) produced by the parser. It evaluates node structure without resolving types (e.g., verifying whether an `if` block uses braces or detecting `console.log`). Runs at ~10,000+ files per second.
- **Type-Aware Linting**: Requires a full compilation program (`ts.Program`). The linter asks TypeScript to type-check nodes before evaluating rules (e.g., checking if an expression inside `await` actually returns a `PromiseLike`, or if a method call on an unknown object is safe). This is 10x–50x slower but catches subtle logic errors.

```
+-------------------------------------------------------------------------+
|                          Linter Architecture                            |
+-------------------------------------------------------------------------+
|                                                                         |
| Source Code ---> [ Parser (TSESTree) ] ---> AST Node Stream             |
|                                                    |                    |
|                                                    v                    |
|                        +---------------------------+                    |
|                        | Fast Syntax Linter        |                    |
|                        | (Biome / Oxlint / ESLint) |                    |
|                        +---------------------------+                    |
|                                                    |                    |
|   +------------------------------------------------+                    |
|   | Need Types?                                                         |
|   v                                                                     |
| [ ts.createProgram() ] ---> [ ts.TypeChecker ]                          |
|                                    |                                    |
|                                    v                                    |
|                   +----------------------------------+                  |
|                   | Type-Aware Rules                 |                  |
|                   | (@typescript-eslint/recommended- |                  |
|                   |  type-checked)                   |                  |
|                   +----------------------------------+                  |
+-------------------------------------------------------------------------+
```

### ESLint Flat Config (`eslint.config.mjs`)
Since ESLint v9, the legacy `.eslintrc.*` format is superseded by modular, declarative flat configs exporting an array of configuration objects.

---

## 3. How It Works Internally

The bridge between ESLint and TypeScript is `@typescript-eslint/parser`.
1. **Parsing**: `@typescript-eslint/parser` invokes the TypeScript compiler's internal scanner and parser to generate a standard TypeScript AST (`ts.SourceFile`).
2. **AST Transformation (TSESTree)**: ESLint expects ASTs complying with ESTree specifications. The parser converts TypeScript AST nodes into TSESTree format (mapping `ts.SyntaxKind` to ESTree node types like `TSInterfaceDeclaration`, `TSTypeAnnotation`).
3. **Program Binding**: When `project: true` or `projectService: true` is configured, the parser creates or reuses a `ts.Program`. It attaches the `ts.TypeChecker` instance to ESLint's `parserServices`.
4. **Rule Execution**: Lint rules subscribe to AST selector queries (e.g., `CallExpression:exit`). A type-aware rule extracts the TypeScript node via `parserServices.esTreeNodeToTSNodeMap.get(node)` and queries `typeChecker.getTypeAtLocation(tsNode)`.

---

## 4. Syntax & API Reference

### typescript-eslint v8 Flat Config (`eslint.config.mjs`)
```javascript
import eslint from '@eslint/js';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  eslint.configs.recommended,
  ...tseslint.configs.recommendedTypeChecked,
  ...tseslint.configs.stylisticTypeChecked,
  {
    languageOptions: {
      parserOptions: {
        projectService: true,
        tsconfigRootDir: import.meta.dirname,
      },
    },
    rules: {
      '@typescript-eslint/no-floating-promises': 'error',
      '@typescript-eslint/await-thenable': 'error',
      '@typescript-eslint/no-explicit-any': 'warn',
    },
  }
);
```

### Biome Configuration (`biome.json`)
```json
{
  "$schema": "https://biomejs.dev/schemas/1.8.0/schema.json",
  "formatter": {
    "enabled": true,
    "formatWithErrors": false,
    "indentStyle": "space",
    "indentWidth": 2,
    "lineWidth": 100
  },
  "linter": {
    "enabled": true,
    "rules": {
      "recommended": true,
      "correctness": {
        "noUnusedVariables": "error"
      },
      "style": {
        "useConst": "error"
      }
    }
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Type-Aware Rule - Floating Promise Detection
```typescript
// Floating promises can fail silently without triggering unhandledRejection handlers.
async function fetchAccountData(userId: string): Promise<{ id: string; balance: number }> {
  return { id: userId, balance: 1000 };
}

// INCORRECT: Promise ignored without await or void
// @typescript-eslint/no-floating-promises will flag this:
function syncNotification(userId: string): void {
  // lint error: Promises must be awaited, end with a call to .catch, or an explicit void operator.
  void fetchAccountData(userId).catch((err) => {
    console.error("Failed to fetch balance:", err);
  });
}
```

### Example 2: Real-World Strict Type Filtering
```typescript
// Enforcing exact exhaustiveness checking through lint rules
type PaymentStatus = "pending" | "authorized" | "captured" | "refunded";

interface PaymentEvent {
  status: PaymentStatus;
  amount: number;
}

export function handlePayment(event: PaymentEvent): string {
  switch (event.status) {
    case "pending":
      return "Hold payment";
    case "authorized":
      return "Settle transaction";
    case "captured":
      return "Receipt issued";
    case "refunded":
      return "Funds returned";
    default: {
      // @typescript-eslint/switch-exhaustiveness-check ensures all cases are handled
      const _exhaustiveCheck: never = event.status;
      throw new Error(`Unhandled payment status: ${_exhaustiveCheck}`);
    }
  }
}
```

### Example 3: Edge Case - Awaiting Non-Thenable Expressions
```typescript
// Rule: @typescript-eslint/await-thenable
async function computeTotal(subtotal: number, taxRate: number): Promise<number> {
  const tax = subtotal * taxRate;
  
  // INCORRECT: Awaiting a non-Promise primitive
  // const total = await (subtotal + tax); // Lint error: 'await' has no effect on the type 'number'.
  
  const total = subtotal + tax; // Correct
  return total;
}
```

### Example 4: Compiler Error & Type Assertion Linter Detection
```typescript
// Rule: @typescript-eslint/consistent-type-assertions
interface ServerConfig {
  host: string;
  port: number;
}

// Bad practice: Type assertion bypasses type checking
const badConfig = {} as ServerConfig; 

// Lint warning / error: Do not use object assertion to empty object.
// Correct approach:
const goodConfig: ServerConfig = {
  host: "0.0.0.0",
  port: 8080
};

// Failure Case Demonstration:
// @ts-expect-error - TS2322: Type 'string' is not assignable to type 'number'.
goodConfig.port = "8080";
```

### Example 5: Idiomatic Production Setup with Biome CLI
```typescript
// File checked with: biome check --apply ./src
export interface WorkerTask<TPayload, TResult> {
  readonly id: string;
  readonly payload: TPayload;
  execute(data: TPayload): Promise<TResult>;
}

export class TaskRunner {
  private readonly tasks = new Map<string, WorkerTask<unknown, unknown>>();

  public register<P, R>(task: WorkerTask<P, R>): void {
    this.tasks.set(task.id, task as WorkerTask<unknown, unknown>);
  }

  public async run<P, R>(taskId: string, payload: P): Promise<R> {
    const task = this.tasks.get(taskId);
    if (!task) {
      throw new Error(`Task ${taskId} not found`);
    }
    return (await task.execute(payload)) as R;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`projectService: true` vs. `project: true`**:
   In older typescript-eslint versions, configuring `project: ['./tsconfig.json']` caused massive memory leaks and slow reloads because the parser rebuilt the entire `ts.Program` on every lint invocation. typescript-eslint v8 introduced `projectService: true`, which leverages the TypeScript Language Service Project Service to reuse isolated type caches seamlessly.
2. **Out-of-Project Files**:
   Files not included in your `tsconfig.json` (such as `vite.config.ts`, `eslint.config.mjs`, or deployment scripts) will crash type-aware linting with the error `Parsing error: "parserOptions.project" has been set for @typescript-eslint/parser`. Always set `allowDefaultProject: ['eslint.config.mjs']` or configure a dedicated `tsconfig.node.json`.
3. **AST Drift with Nightly TypeScript**:
   When using TypeScript bleeding-edge or pre-release versions, newly introduced syntax (e.g., `using` declarations or decorator metadata) may crash older ESTree converters until `@typescript-eslint/parser` is updated.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Diagnostic / Lint Rule |
| :--- | :--- | :--- | :--- |
| **Floating Promise** | `fetch(url);` | `void fetch(url);` or `await fetch(url);` | `@typescript-eslint/no-floating-promises` |
| **Awaiting Non-Promise** | `await getValue();` (where value is `number`) | `getValue();` | `@typescript-eslint/await-thenable` |
| **Unsafe Type Assertion** | `const u = data as User;` | `const u = userSchema.parse(data);` | `@typescript-eslint/consistent-type-assertions` |
| **Loose Array Constructor** | `new Array(10);` | `Array.from({ length: 10 });` | `no-array-constructor` |
| **Unchecked Indexed Access** | `const val = arr[0].trim();` | `const item = arr[0]; if (item) item.trim();` | `TS2532: Object is possibly 'undefined'` |
| **Any Return in Promise** | `async function f(): Promise<any>` | `async function f(): Promise<UserDTO>` | `@typescript-eslint/explicit-module-boundary-types` |

---

## 8. Performance & Type-Checker Complexity

Type-aware linting performance scales proportionally with the complexity of your TypeScript type hierarchy:
- **Syntax Rules**: $O(N)$ where $N$ is the number of AST nodes. Parsing takes ~1ms per file.
- **Type-Aware Rules**: $O(N \times M)$ where $M$ is the depth of type instantiation and conditional type evaluation performed by `ts.TypeChecker`.
- **Optimization Strategy**: Use a tiered linting pipeline in CI:
  1. Tier 1: Biome or Oxlint on pre-commit hooks (50ms execution across entire workspace).
  2. Tier 2: `eslint . --max-warnings 0` with type-aware rules running in parallel CI jobs caching TypeScript `.tsbuildinfo`.

---

## 9. Compiler Internals

When `@typescript-eslint/parser` operates:
1. `ts.createSourceFile`: Generates the AST from source text.
2. `ts.createProgram`: Constructs the compiler host, builds the file dependency graph, and parses all imported source files.
3. `ts.TypeChecker`: Resolves identifiers into `ts.Symbol` records and creates `ts.Type` objects.
4. `parserServices.program.getTypeChecker()`: Exposes internal methods like `checker.getTypeAtLocation(node)` and `checker.getSignaturesOfType(type, ts.SignatureKind.Call)`.
5. Rules query `type.flags` against bitmasks such as `ts.TypeFlags.Any`, `ts.TypeFlags.Union`, or `ts.TypeFlags.TypeParameter`.

---

## 10. Best Practices & Design Guidelines

1. **Adopt Flat Config**: Migrate away from `.eslintrc` to `eslint.config.mjs` using `typescript-eslint.config()`.
2. **Enable Strict Type-Checked Presets**: Use `tseslint.configs.strictTypeChecked` for maximum safety in new greenfield projects.
3. **Format with Biome or Prettier**: Let linters inspect semantics and dedicated formatters handle indentation and layout.
4. **Isolate Config Files**: Ensure utility scripts and configs have their own tsconfig references to prevent parser project errors.

---

## 11. Real-World Case Study

A high-frequency financial trading dashboard experienced silent transaction dropouts in production. The root cause was identified as an unhandled promise rejection in an asynchronous telemetry dispatch loop:

```typescript
// The Bug:
function logAudit(txnId: string) {
  auditService.record(txnId); // Promise rejected silently on network blips
}

// The Fix via typescript-eslint:
// Enabling @typescript-eslint/no-floating-promises instantly flagged 47 instances
// across the legacy backend.
function logAuditFixed(txnId: string): void {
  auditService.record(txnId).catch((error) => {
    metrics.increment("audit.failure");
    fallbackLogger.error("Failed to record audit", { txnId, error });
  });
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the fundamental difference between standard ESLint rules and typescript-eslint type-aware rules?</summary>

Standard ESLint rules operate solely on the Abstract Syntax Tree (AST) generated by the parser without knowledge of variable types, inheritance hierarchies, or import resolutions. Type-aware rules instantiate a `ts.Program` and query the TypeScript `TypeChecker` to inspect runtime types, resolving whether a variable is a `Promise`, an `any`, or a specific union.
</details>

<details>
<summary>2. Why did typescript-eslint v8 introduce `projectService: true` over `project: true`?</summary>

`project: true` required ESLint to parse and hold multiple entire compiler programs in memory simultaneously, often re-parsing identical files across multiple tsconfigs. `projectService: true` delegates project tracking to TypeScript's internal language service daemon, eliminating duplicate program generation, speeding up lint runs by 2x–5x, and handling out-of-project files gracefully.
</details>

<details>
<summary>3. What is Biome and how does it compare to ESLint + Prettier?</summary>

Biome is an integrated Rust-based toolchain that provides both formatting and linting. It executes 25x–50x faster than ESLint and Prettier by parsing files once into a unified CST/AST in Rust. However, Biome does not currently provide full type-aware linting equivalent to `@typescript-eslint/recommended-type-checked`.
</details>

<details>
<summary>4. What does `@typescript-eslint/no-floating-promises` protect against?</summary>

It flags promises that are initiated without `await`, `void`, or `.catch()`. In JavaScript/Node.js, floating promises can throw asynchronous errors that crash processes (`unhandledRejection`) or fail silently in fire-and-forget contexts.
</details>

<details>
<summary>5. How can you exclude specific test files from type-aware linting performance penalties?</summary>

By defining multiple config blocks in `eslint.config.mjs` and disabling `projectService` or specific type-checked rules for files matching `**/*.test.ts` or `**/*.spec.ts`.
</details>

<details>
<summary>6. What causes the ESLint error "The file must be included in at least one of the projects provided"?</summary>

The file being linted is not matched by the `include` glob patterns of any `tsconfig.json` specified in `parserOptions.project`. Solutions include updating `tsconfig.json` `include` paths or using `projectService.allowDefaultProject`.
</details>

<details>
<summary>7. What is Oxlint and where does it fit in modern pipelines?</summary>

Oxlint is an extremely fast Rust-based linter focused on catching logic bugs without configuration overhead. It is commonly placed as a fast pre-commit check to provide instant feedback before heavier ESLint or TypeScript validation passes run.
</details>

<details>
<summary>8. Why should you avoid `any` even in lint suppression comments?</summary>

Suppressing lint errors with `/* eslint-disable @typescript-eslint/no-explicit-any */` spreads untyped infection throughout downstream consumers. Instead, prefer `unknown` accompanied by runtime type guards or Zod schemas.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Configure a minimal eslint.config.mjs with typescript-eslint</summary>

**Task**: Write an `eslint.config.mjs` exporting a valid Flat Config using `typescript-eslint` that enables recommended rules.

**Solution**:
```javascript
import eslint from '@eslint/js';
import tseslint from 'typescript-eslint';

export default tseslint.config(
  eslint.configs.recommended,
  ...tseslint.configs.recommended,
  {
    rules: {
      'no-console': 'warn',
    },
  }
);
```
</details>

<details>
<summary>Challenge 2 (Easy): Fix floating promise violations in async handlers</summary>

**Task**: Fix the following code so that it satisfies `@typescript-eslint/no-floating-promises`:
```typescript
function startTimer(callback: () => Promise<void>) {
  setInterval(() => {
    callback();
  }, 1000);
}
```

**Solution**:
```typescript
function startTimer(callback: () => Promise<void>) {
  setInterval(() => {
    void callback().catch((err) => {
      console.error("Timer callback error:", err);
    });
  }, 1000);
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Enforce switch statement exhaustiveness via types and linting</summary>

**Task**: Implement a discriminated union handler where omitting a case triggers both a TypeScript compiler error and an exhaustive check pattern.

**Solution**:
```typescript
type Direction = "North" | "South" | "East" | "West";

function navigate(dir: Direction): { x: number; y: number } {
  switch (dir) {
    case "North": return { x: 0, y: 1 };
    case "South": return { x: 0, y: -1 };
    case "East": return { x: 1, y: 0 };
    case "West": return { x: -1, y: 0 };
    default: {
      const _unreachable: never = dir;
      throw new Error(`Invalid direction: ${_unreachable}`);
    }
  }
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Custom ESLint rule selector for catching dangerous casts</summary>

**Task**: Write an ESLint rule configuration snippet that forbids casting to `any` while permitting casting to `unknown`.

**Solution**:
```javascript
export default [
  {
    rules: {
      '@typescript-eslint/no-explicit-any': ['error', { fixToUnknown: true }],
      '@typescript-eslint/consistent-type-assertions': [
        'error',
        {
          assertionStyle: 'as',
          objectLiteralTypeAssertions: 'never'
        }
      ]
    }
  }
];
```
</details>

<details>
<summary>Challenge 5 (Hard): Multi-tier Linting Script in package.json</summary>

**Task**: Construct a `package.json` scripts section that orchestrates Oxlint for instant syntax checking, Biome for formatting, and typescript-eslint for deep type-aware auditing.

**Solution**:
```json
{
  "scripts": {
    "format:check": "biome format ./src",
    "format:fix": "biome format --write ./src",
    "lint:fast": "oxlint --deny-warnings",
    "lint:types": "eslint src/ --max-warnings 0",
    "check": "npm run format:check && npm run lint:fast && npm run lint:types"
  }
}
```
</details>

<details>
<summary>Challenge 6 (Hard): Custom TypeScript AST Rule using ESLint RuleBuilder</summary>

**Task**: Sketch the structure of an ESLint rule that inspects `CallExpression` nodes to verify that any call to `JSON.parse` is immediately asserted or guarded rather than left as implicit `any`.

**Solution**:
```typescript
import { ESLintUtils } from '@typescript-eslint/utils';

export const rule = ESLintUtils.RuleCreator.withoutDocs({
  meta: {
    type: 'problem',
    docs: { description: 'Disallow unchecked JSON.parse results' },
    messages: {
      uncheckedJsonParse: 'JSON.parse returns any. Validate output with a schema validator.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    return {
      CallExpression(node) {
        if (
          node.callee.type === 'MemberExpression' &&
          node.callee.object.type === 'Identifier' &&
          node.callee.object.name === 'JSON' &&
          node.callee.property.type === 'Identifier' &&
          node.callee.property.name === 'parse'
        ) {
          const parent = node.parent;
          if (!parent || parent.type !== 'TSAsExpression') {
            context.report({ node, messageId: 'uncheckedJsonParse' });
          }
        }
      },
    };
  },
});
```
</details>
