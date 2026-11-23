# Editor Integration & TypeScript Playground

> Comprehensive guide to configuring Language Server Protocol (LSP), VS Code settings, inlay hints, TypeScript Playground reproduction links, and AST debugging tools.

| Metadata | Value |
| :--- | :--- |
| **Level** | Beginner / Intermediate |
| **Prerequisites** | [00-toolchain.md](file:///e:/Learning/engineering-notes/01-languages/typescript/00-setup/00-toolchain.md) |
| **Time to Read** | 20 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 5.0+ / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `target: ES2022`, `moduleResolution: NodeNext` |

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

### Problem Statement
Writing TypeScript without rich editor integration eliminates the primary developer experience benefit of the language: real-time feedback, auto-completion, refactoring, and inline diagnostic error highlighting. Furthermore, when reporting compiler bugs or collaborating, engineers need isolated, reproducible environments (Playground links, reproduction repositories) rather than multi-file code snippets.

### Historical Context
Historically, IDEs ran separate proprietary background compilers. TypeScript pioneered the **Language Server Protocol (LSP)** and the **TypeScript Server (`tsserver`)**, exposing a JSON-RPC interface that any modern text editor (VS Code, Neovim, Zed, JetBrains, Emacs) can consume for uniform type intelligence.

### Real-World Relevance
Configuring editor inlay hints, workspace SDK selection, and Playground sharing URLs reduces debugging latency by orders of magnitude and allows frictionless communication across distributed engineering teams.

---

## 2. Core Concepts

### The TypeScript Language Service (`tsserver`)
`tsserver` is an out-of-process Node.js daemon started automatically by your editor. It reads `tsconfig.json`, maintains an in-memory project graph, parses open files, and pushes diagnostics to the editor.

```
       Editor (VS Code, Neovim, Zed)
                   │
                   ▼  JSON-RPC / stdio
       ┌───────────────────────────────┐
       │   tsserver / LSP Server       │
       ├───────────────────────────────┤
       │  - Auto-completions           │
       │  - Inlay hints (return types) │
       │  - Hover type information     │
       │  - Code actions / Quick fixes │
       └───────────────────────────────┘
```

### Inlay Hints
Inlay hints display implicit type annotations directly inside the editor canvas without modifying source code on disk:
- Parameter name hints: `calcArea(/* width: */ 10, /* height: */ 20)`
- Variable type hints: `const count/*: number*/ = 42;`
- Function return hints: `function getStatus()/*: boolean*/ { ... }`

---

## 3. How It Works Internally

### Workspace Version vs Built-In Editor Version
VS Code bundles a specific TypeScript version internally. However, your project may depend on a newer or older version installed in `node_modules/typescript`.
- **VS Code Built-in**: `C:\Users\...\AppData\Local\Programs\Microsoft VS Code\resources\app\extensions\node_modules\typescript`
- **Workspace Version**: `./node_modules/typescript/lib`

To ensure consistency across CI and development, always configure VS Code to use the **Workspace Version**:
`F1` $\to$ `TypeScript: Select TypeScript Version` $\to$ `Use Workspace Version`.

---

## 4. Syntax & API Reference

### Recommended `.vscode/settings.json`

```json
{
  "typescript.tsdk": "node_modules/typescript/lib",
  "typescript.enablePromptUseWorkspaceTsdk": true,
  
  /* Inlay Hints Configuration */
  "typescript.inlayHints.parameterNames.enabled": "all",
  "typescript.inlayHints.parameterTypes.enabled": true,
  "typescript.inlayHints.variableTypes.enabled": true,
  "typescript.inlayHints.propertyDeclarationTypes.enabled": true,
  "typescript.inlayHints.functionLikeReturnTypes.enabled": true,
  "typescript.inlayHints.enumMemberValues.enabled": true,
  
  /* Formatting & Auto-Import Behavior */
  "typescript.preferences.importModuleSpecifier": "non-relative",
  "typescript.preferences.includePackageJsonAutoImports": "on",
  "typescript.suggest.completeFunctionCalls": true,
  "editor.codeActionsOnSave": {
    "source.organizeImports": "explicit",
    "source.fixAll": "explicit"
  }
}
```

---

## 5. Practical Examples

### Example 1: Inlay Hints for Implicit Return Types
- **Goal:** Observe how inlay hints clarify inferred return types.
- **Code:**
```typescript
export function computeMetrics(samples: readonly number[]) {
  const sum = samples.reduce((acc, curr) => acc + curr, 0);
  const avg = sum / (samples.length || 1);
  return { sum, avg, count: samples.length };
}
```
- **Editor Inlay Visualization:**
```typescript
export function computeMetrics(samples: readonly number[])/*: { sum: number; avg: number; count: number }*/ {
  const sum/*: number*/ = samples.reduce((acc, curr) => acc + curr, 0);
  const avg/*: number*/ = sum / (samples.length || 1);
  return { sum, avg, count: samples.length };
}
```
- **Explanation:** Inlay hints display the exact inferred type without requiring the developer to add explicit return type boilerplate.

### Example 2: Generating a Sharable TypeScript Playground URL
- **Goal:** Share code snippets with exact compiler flags embedded in the URL.
- **URL Structure:**
```text
https://www.typescriptlang.org/play?strict=true&noUncheckedIndexedAccess=true&ts=5.7.3#code/JYOwLgpgTgZghgYwgAg...
```
- **Explanation:** The `#code/<base64>` hash contains the LZ-string compressed source code and all active compiler flags, allowing instant reproduction across the team.

### Example 3: Editor Quick-Fix / Code Action
- **Goal:** Automatically implement missing interface properties via IDE code actions.
- **Code:**
```typescript
interface Logger {
  info(msg: string): void;
  error(msg: string, err: Error): void;
}

// Pressing Ctrl + . (Cmd + .) on ConsoleLogger triggers "Implement interface 'Logger'"
class ConsoleLogger implements Logger {
  info(msg: string): void {
    console.log(`[INFO]: ${msg}`);
  }
  error(msg: string, err: Error): void {
    console.error(`[ERROR]: ${msg}`, err);
  }
}
```

### Example 4: Negative Test — Diagnostic Invalidation
- **Goal:** Trigger type-checker diagnostic in editor and inspect error code.
- **Code:**
```typescript
const ids: string[] = ['a', 'b', 'c'];
// @ts-expect-error TS2322
ids.push(42);
```
- **Real Diagnostic in VS Code Problems Tab:**
```
Argument of type 'number' is not assignable to parameter of type 'string'. (TS2345) [Ln 3, Col 10]
```

### Example 5: AST Explorer Inspection for TypeScript Tokens
- **Goal:** Understand AST representation of a typed function.
- **Source Snippet:**
```typescript
type ID = string | number;
```
- **AST Node Breakdown (via AST Explorer with `@typescript-eslint/parser`):**
```json
{
  "type": "TSTypeAliasDeclaration",
  "id": { "type": "Identifier", "name": "ID" },
  "typeAnnotation": {
    "type": "TSUnionType",
    "types": [
      { "type": "TSStringKeyword" },
      { "type": "TSNumberKeyword" }
    ]
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Version Mismatch**: Using features introduced in TS 5.4 (`NoInfer`) while your editor's built-in TypeScript is set to TS 5.2 causes false syntax error squiggles in the editor that pass in CI. Always lock `typescript.tsdk` to the workspace version.
2. **Memory Leak in Long-Running `tsserver`**: Large monorepos can cause `tsserver` to exceed 4GB memory usage, causing editor freezes. Run `TypeScript: Restart TS Server` in the command palette to flush the in-memory cache.
3. **Multi-Root Workspace Conflicts**: In monorepos with multiple `tsconfig.json` files, opening a file not included in any project's `"include"` list falls back to an untracked single-file context (`tsconfig.json` ignored).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Real Compiler / Editor Issue | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Relying on editor built-in TS version | Type checks pass locally but fail in CI | Select "Use Workspace Version" | Eliminates version discrepancies |
| Ignoring `tsserver` memory warnings | Editor typing lag exceeds 2 seconds | Restart TS Server or allocate `--max-old-space-size` | Restores instantaneous auto-completion |
| Sharing code via screenshots in chat | Cannot be tested or compiled by peers | Share via TypeScript Playground URL | Embeds exact compiler options and types |
| Missing `.vscode/settings.json` in repo | Inconsistent inlay hints across team | Check `.vscode/settings.json` into git | Enforces team-wide editor standardization |

---

## 8. Performance & Type-Checker Complexity

- **LSP Debounce Time**: `tsserver` queues diagnostic updates on keystrokes. In deep conditional types, recalculating diagnostics takes $> 500\text{ms}$.
- **Background Semantic Diagnostics**: Configure `"typescript.tsserver.experimental.enableProjectDiagnostics": true` if you want background type errors reported across unopened files in the workspace.

---

## 9. Compiler Internals

`tsserver` wraps the standard TypeScript `CompilerHost` inside a `LanguageServiceHost`. When a file is edited:
1. An incremental edit event updates the in-memory `SourceFile`.
2. The language service updates the AST incrementally without re-parsing untouched functions.
3. The type checker caches symbol types in a weak map, invalidating only symbols in affected dependency files.

---

## 10. Best Practices & Design Guidelines

1. **Commit `.vscode/settings.json` and `.vscode/extensions.json`**: Recommend standard extensions (ESLint, Prettier, Biome, Vitest).
2. **Enable Inlay Hints**: Greatly reduces the cognitive overhead of reading inferred complex generic types.
3. **Use TypeScript Playground for PR Explanations**: When changing complex types, link a Playground URL showing the before and after assignability tests.

---

## 11. Real-World Case Study

A core platform team at a fintech company frequently dealt with developer complaints that "VS Code shows no errors, but `npm run build` fails in GitHub Actions."

### Remediation:
1. Created `.vscode/settings.json` enforcing `"typescript.tsdk": "node_modules/typescript/lib"`.
2. Added `"typescript.enablePromptUseWorkspaceTsdk": true`.
3. Added a pre-commit check verifying that developers used workspace dependencies.

**Result**: Eliminated 100% of local-vs-CI false positive discrepancy tickets.

---

## 12. Interview Questions

### Q1: What is the role of `tsserver` in modern development environments?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

`tsserver` is the headless language server daemon packaged with TypeScript. It exposes an RPC API allowing editors (VS Code, Neovim, JetBrains) to query semantic information about code: completions, signature help, find references, rename symbols, organize imports, and fetch diagnostic errors. It keeps an incremental in-memory AST and Symbol table synchronized with disk edits.
</details>

### Q2: How does VS Code decide which TypeScript version to execute?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By default, VS Code uses its internal bundled TypeScript version located in its application directory. If a project has `typescript` installed in `node_modules`, VS Code prompts the user to switch to the workspace version, or reads `"typescript.tsdk": "node_modules/typescript/lib"` from `.vscode/settings.json`.
</details>

### Q3: Why is sharing a TypeScript Playground link superior to sharing a GitHub gist for type questions?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

The TypeScript Playground encodes the exact compiler flags (`strict`, `noUncheckedIndexedAccess`, `exactOptionalPropertyTypes`), target version (e.g. `5.7.3`, `Nightly`), and code into a single URL. The recipient can immediately inspect hover types, view emitted JavaScript, and test edge cases in an isolated environment without cloning repositories or configuring dependencies.
</details>

### Q4: How do you increase memory allocation for `tsserver` in large monorepos?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In VS Code settings, configure:
```json
"typescript.tsserver.maxTsServerMemory": 8192
```
This passes `--max-old-space-size=8192` to the underlying Node.js process executing `tsserver`, preventing out-of-memory crashes on million-line codebases.
</details>

---

## 13. Exercises & Challenges

### Exercise 1 (Easy): Enable Inlay Hints in VS Code
- **Problem Statement**: Write the JSON snippet to enable parameter names and variable types inlay hints in `.vscode/settings.json`.
- **Solution**:
```json
{
  "typescript.inlayHints.parameterNames.enabled": "all",
  "typescript.inlayHints.variableTypes.enabled": true
}
```

### Exercise 2 (Medium): Author an AST Explorer Transformation Mock
- **Problem Statement**: Write a function that accepts a TypeScript AST node and returns `true` if it represents a type alias declaration (`type X = ...`).
- **Solution**:
```typescript
import ts from 'typescript';

export function isTypeAlias(node: ts.Node): node is ts.TypeAliasDeclaration {
  return node.kind === ts.SyntaxKind.TypeAliasDeclaration;
}
```

### Exercise 3 (Hard): Language Service Plugin Scaffold
- **Problem Statement**: Sketch a minimal TypeScript Language Service Plugin that intercepts completions to add a custom prefix.
- **Solution**:
```typescript
import type ts from 'typescript/lib/tsserverlibrary';

function init(modules: { typescript: typeof ts }) {
  function create(info: ts.server.PluginCreateInfo) {
    const proxy: ts.LanguageService = Object.create(null);
    for (const k of Object.keys(info.languageService) as Array<keyof ts.LanguageService>) {
      const x = info.languageService[k]!;
      // @ts-expect-error proxy indexing
      proxy[k] = (...args: any[]) => x.apply(info.languageService, args);
    }

    proxy.getCompletionsAtPosition = (fileName, position, options) => {
      const prior = info.languageService.getCompletionsAtPosition(fileName, position, options);
      if (!prior) return prior;
      prior.entries.forEach(e => {
        e.name = `✨_${e.name}`;
      });
      return prior;
    };

    return proxy;
  }

  return { create };
}

export = init;
```
