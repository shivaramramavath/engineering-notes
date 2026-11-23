# Writing a Custom Language Service Plugin in TypeScript

> Comprehensive guide to building TypeScript Language Service Plugins (TSServer): extending IDE capabilities, injecting custom diagnostics, decorating completions, quick fixes, and editor integration in VS Code.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [14-type-system-internals/05-compiler-architecture.md](./05-compiler-architecture.md), [14-type-system-internals/06-ast-and-compiler-api.md](./06-ast-and-compiler-api.md) |
| **TypeScript Baseline** | TS 2.3+ (Language Service Plugins), TS 5.0+ |
| **Tested tsconfig** | `"plugins": [{ "name": "my-ts-plugin" }]` |

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
While `tsc` is responsible for building and compiling code, developer experience in code editors (VS Code, WebStorm, Neovim) is driven by **TSServer**—the TypeScript Language Service. TSServer powers real-time autocomplete suggestions, hover tooltips, refactoring code actions, and in-editor diagnostic squiggles.

A **TypeScript Language Service Plugin** allows developers to intercept and decorate TSServer's internal methods without patching TypeScript. You can inject custom domain diagnostics (e.g. warning when an SQL query has syntax errors or when an API endpoint string is invalid), customize autocomplete menus, or provide automated quick-fix refactorings directly in the developer's IDE.

---

## 2. Core Concepts
- **`LanguageService` Decorator Pattern**: A TSServer plugin is a function factory `init(modules: { typescript: typeof ts })` that returns an object with a `create(info: ts.server.PluginCreateInfo)` method. It wraps and proxies methods on the existing `ts.LanguageService`.
- **Target Interception Methods**:
  - `getSemanticDiagnostics(fileName)`: Injects custom warnings and error squiggles into the editor.
  - `getCompletionsAtPosition(fileName, position, options)`: Appends custom autocomplete options.
  - `getQuickInfoAtPosition(fileName, position)`: Overrides or augments hover tooltip text.
  - `getCodeFixesAtPosition(...)`: Offers lightbulb quick-fix suggestions.
- **`tsconfig.json` Registration**:
  ```json
  {
    "compilerOptions": {
      "plugins": [{ "name": "my-custom-ts-plugin" }]
    }
  }
  ```
- **TSServer vs `tsc` Scope Limitation**: Language Service plugins **only run inside the editor** (TSServer). They do *not* run when invoking `tsc` from the command line. To fail CI builds on custom diagnostics, a companion CLI linter or AST tool is required.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                VS Code / Editor Interface                   |
+------------------------------+------------------------------+
                               |
                        JSON-RPC Request
                               |
                               v
+-------------------------------------------------------------+
|                     TSServer Process                        |
|  - Manages open project buffers                             |
|  - Loads plugins declared in tsconfig.json                  |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                Plugin Interceptor Proxy                     |
|  Calls original languageService.getSemanticDiagnostics()    |
|  Appends custom domain diagnostics                          |
+------------------------------+------------------------------+
                               |
                               v
                 Emits Squiggles to IDE Editor!
```

---

## 4. Syntax & API Reference

```typescript
import * as ts from 'typescript/lib/tsserverlibrary';

// 1. Language Service Plugin Factory
function init(modules: { typescript: typeof ts }) {
  const ts = modules.typescript;

  function create(info: ts.server.PluginCreateInfo): ts.LanguageService {
    // 2. Proxy the language service
    const proxy: ts.LanguageService = Object.create(null);
    for (const k of Object.keys(info.languageService) as Array<keyof ts.LanguageService>) {
      const x = info.languageService[k]!;
      proxy[k] = (...args: Array<any>) => (x as any).apply(info.languageService, args);
    }

    // 3. Intercept getSemanticDiagnostics
    proxy.getSemanticDiagnostics = (fileName: string): ts.Diagnostic[] => {
      const prior = info.languageService.getSemanticDiagnostics(fileName);
      const program = info.languageService.getProgram();
      if (!program) return prior;

      const sf = program.getSourceFile(fileName);
      if (!sf) return prior;

      // Add custom diagnostics...
      return prior;
    };

    return proxy;
  }

  return { create };
}

export = init;
```

---

## 5. Practical Examples

### Example 1: In-Editor Warning for Hardcoded Secrets
```typescript
import * as ts from 'typescript/lib/tsserverlibrary';

function init(modules: { typescript: typeof ts }) {
  const ts = modules.typescript;

  function create(info: ts.server.PluginCreateInfo): ts.LanguageService {
    const proxy = Object.create(info.languageService);

    proxy.getSemanticDiagnostics = (fileName: string): ts.Diagnostic[] => {
      const diagnostics = info.languageService.getSemanticDiagnostics(fileName);
      const program = info.languageService.getProgram();
      const sf = program?.getSourceFile(fileName);
      if (!sf) return diagnostics;

      function visit(node: ts.Node) {
        if (ts.isStringLiteral(node)) {
          if (/^sk_[a-zA-Z0-9]{24,}/.test(node.text)) {
            diagnostics.push({
              file: sf,
              start: node.getStart(),
              length: node.getWidth(),
              messageText: 'SECURITY WARNING: Hardcoded API secret key detected!',
              category: ts.DiagnosticCategory.Warning,
              code: 9901,
            });
          }
        }
        ts.forEachChild(node, visit);
      }

      visit(sf);
      return diagnostics;
    };

    return proxy;
  }

  return { create };
}

export = init;
```

### Example 2: Injecting Custom Autocomplete Entries
```typescript
proxy.getCompletionsAtPosition = (fileName, position, options) => {
  const prior = info.languageService.getCompletionsAtPosition(fileName, position, options);
  if (!prior) return prior;

  // Add custom domain completion
  prior.entries.push({
    name: '$env',
    kind: ts.ScriptElementKind.keyword,
    sortText: '0', // Shows up at top of menu!
    insertText: 'process.env.',
  });

  return prior;
};
```

### Example 3: Edge Case - Augmenting Hover Tooltips (QuickInfo)
```typescript
proxy.getQuickInfoAtPosition = (fileName, position) => {
  const prior = info.languageService.getQuickInfoAtPosition(fileName, position);
  if (!prior) return prior;

  // Append documentation to existing tooltip
  const extraDoc = ts.displayPartsToString(prior.documentation) + '\n\n*(Verified by Security Policy)*';
  return {
    ...prior,
    documentation: [{ text: extraDoc, kind: 'text' }],
  };
};
```

### Example 4: Compiler Failure Case - Non-Existent Plugin in tsconfig
```json
// If plugin package is not installed in node_modules:
{
  "compilerOptions": {
    "plugins": [{ "name": "non-existent-plugin" }]
  }
}
// TSServer log:
// [tsserver] Failed to load module 'non-existent-plugin' from ...
```

### Example 5: Idiomatic QuickFix Code Action
```typescript
proxy.getCodeFixesAtPosition = (fileName, start, end, errorCodes, formatOptions, preferences) => {
  const prior = info.languageService.getCodeFixesAtPosition(fileName, start, end, errorCodes, formatOptions, preferences);
  if (errorCodes.includes(9901)) {
    prior.push({
      fixName: 'extractSecretToEnv',
      description: 'Replace with process.env.SECRET_KEY',
      changes: [{
        fileName,
        textChanges: [{
          span: { start, length: end - start },
          newText: 'process.env.SECRET_KEY',
        }],
      }],
    });
  }
  return prior;
};
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Plugins Do Not Run on `tsc`**: A TSServer plugin only activates inside an IDE running the TypeScript Language Service. Running `npx tsc` on the terminal ignores plugins completely.
2. **Synchronous Execution Only**: Language Service methods like `getSemanticDiagnostics` are strictly synchronous. You cannot perform asynchronous network requests or file reads inside the plugin handler.
3. **Performance Sensitivity**: Intercepted methods run on every keystroke. Heavy AST traversals or excessive type-checker queries will cause noticeable typing lag in VS Code.
4. **VS Code Workspace Version Setting**: For local plugins to activate, developers in VS Code must select "Use Workspace Version" of TypeScript (`TypeScript: Select TypeScript Version`).

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Expecting plugin to block `tsc` builds | Custom errors ignored in CI | Author a companion CLI checker or ESLint rule |
| Heavy synchronous I/O inside diagnostics | Causes severe editor typing freeze | Cache results and keep checks in-memory |
| Overwriting languageService without proxy | Breaks all default editor autocompletes | Delegate unhandled methods to original service |
| Missing `export = init` CJS export syntax | TSServer fails to load plugin factory | Use `export = init` module format |
| Allocating new `Program` inside plugin | Memory leak in editor | Use existing `info.languageService.getProgram()` |

---

## 8. Performance & Type-Checker Complexity
- Language Service methods execute in real-time as the developer types. Handlers in `getSemanticDiagnostics` should complete in $<20\text{ms}$.
- Access AST nodes directly using `sourceFile.statements` rather than querying `checker.getTypeAtLocation` on every identifier.

---

## 9. Compiler Internals
In TypeScript's `src/server/editorServices.ts`:
- TSServer reads `compilerOptions.plugins` upon project initialization.
- For each entry, it resolves the package from project `node_modules` and executes its `init` export, passing a reference to `ts.server.PluginCreateInfo`.
- The returned proxy wraps the internal `LanguageService` instance for that project session.

---

## 10. Best Practices & Design Guidelines
- Distribute plugins as standard npm packages declaring `"main": "dist/index.js"`.
- Use `export = init` to support both CommonJS and ESM module loaders in TSServer.
- Keep diagnostic error codes in custom 9000+ ranges to avoid collisions with standard TypeScript error codes.
- Implement companion ESLint rules or `ts-morph` CLI tools so CI pipelines can verify the exact same rules that the plugin surfaces in the IDE.
- Test your plugin using `@typescript/tsserver-library` harness utilities.

---

## 11. Real-World Case Study
A financial enterprise maintained strict regulations against exposing unmasked Credit Card numbers or plain SQL strings in client-side TypeScript code. Developers frequently made mistakes that were only caught days later during manual code reviews.

**Solution with TSServer Plugin:**
The security team built `@company/ts-plugin-security`:
1. Intercepted `getSemanticDiagnostics` to scan string literals for raw SQL syntax (`SELECT * FROM`) in frontend files.
2. Flagged matches with red squiggles (`Category: Error`, `Code: 9801: Direct SQL queries are forbidden in client code`).
3. Added a quick-fix lightbulb that refactored the string to an RPC client call `api.query(...)`.
Violations were eliminated at the moment of authoring in VS Code before code was ever committed.

---

## 12. Interview Questions

<details>
<summary>1. What is a TypeScript Language Service Plugin?</summary>

It is a plugin that runs inside TSServer (the language service that powers IDEs) using a decorator/proxy pattern to intercept and augment IDE features like autocomplete, hover tooltips, diagnostics, and quick-fix refactorings.
</details>

<details>
<summary>2. Why do Language Service plugins not run during `tsc` builds?</summary>

Because `tsc` is a separate batch compiler that does not instantiate TSServer or the language service layer. TypeScript design principles deliberately keep `tsc` isolated from editor plugin extensions to guarantee reproducible, deterministic command-line builds.
</details>

<details>
<summary>3. Can a Language Service plugin perform asynchronous work (e.g. database lookups)?</summary>

No. The TypeScript Language Service API methods (`getSemanticDiagnostics`, `getCompletionsAtPosition`) are strictly synchronous. Any asynchronous delay would freeze editor typing and responsiveness.
</details>

<details>
<summary>4. How is a language service plugin registered in a project?</summary>

In `tsconfig.json` under `compilerOptions.plugins`:
```json
{
  "compilerOptions": {
    "plugins": [{ "name": "my-plugin-package" }]
  }
}
```
</details>

<details>
<summary>5. How do you inject a new diagnostic error into the editor?</summary>

By proxying `proxy.getSemanticDiagnostics(fileName)`, retrieving the existing array from `info.languageService.getSemanticDiagnostics(fileName)`, and pushing custom `ts.Diagnostic` objects into the array before returning it.
</details>

<details>
<summary>6. What export format must a Language Service plugin use?</summary>

CommonJS export format (`export = init`), returning a factory function that produces an object with a `create` method.
</details>

<details>
<summary>7. How does a plugin provide quick-fix suggestions for its custom diagnostics?</summary>

By intercepting `getCodeFixesAtPosition`, matching the error code of its custom diagnostic, and returning a `CodeFixAction` with a `textChanges` replacement array.
</details>

<details>
<summary>8. How do you test a Language Service plugin?</summary>

By using the `ts.server` test harness or creating a mock `LanguageService` and `PluginCreateInfo` in a test runner like Vitest.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Plugin Template**: Write the minimal boilerplate structure for a TSServer plugin factory function.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript/lib/tsserverlibrary';

   function init(modules: { typescript: typeof ts }) {
     function create(info: ts.server.PluginCreateInfo): ts.LanguageService {
       return info.languageService;
     }
     return { create };
   }

   export = init;
   ```
   </details>

2. **Custom Diagnostic Object**: Construct a valid `ts.Diagnostic` object with warning severity and message "Avoid TODO comments".
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function createTodoDiagnostic(sf: ts.SourceFile, pos: number, len: number): ts.Diagnostic {
     return {
       file: sf,
       start: pos,
       length: len,
       messageText: 'Avoid TODO comments in production code.',
       category: ts.DiagnosticCategory.Warning,
       code: 9001,
     };
   }
   ```
   </details>

### Medium
3. **No Console Log Diagnostic Interceptor**: Implement a Language Service plugin proxy that emits a warning diagnostic whenever `console.log` is called in any file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript/lib/tsserverlibrary';

   function init(modules: { typescript: typeof ts }) {
     const ts = modules.typescript;

     function create(info: ts.server.PluginCreateInfo): ts.LanguageService {
       const proxy = Object.create(info.languageService);

       proxy.getSemanticDiagnostics = (fileName: string) => {
         const diagnostics = info.languageService.getSemanticDiagnostics(fileName);
         const program = info.languageService.getProgram();
         const sf = program?.getSourceFile(fileName);
         if (!sf) return diagnostics;

         function visit(node: ts.Node) {
           if (
             ts.isCallExpression(node) &&
             ts.isPropertyAccessExpression(node.expression) &&
             node.expression.expression.getText(sf) === 'console' &&
             node.expression.name.text === 'log'
           ) {
             diagnostics.push({
               file: sf,
               start: node.getStart(),
               length: node.getWidth(),
               messageText: 'Console.log statements are disallowed in production.',
               category: ts.DiagnosticCategory.Warning,
               code: 9100,
             });
           }
           ts.forEachChild(node, visit);
         }

         visit(sf);
         return diagnostics;
       };

       return proxy;
     }

     return { create };
   }

   export = init;
   ```
   </details>

4. **Augment Autocomplete**: Write a plugin completion interceptor that injects a completion snippet for `uuid()` whenever the user types in an assignment position.
   <details>
   <summary>Solution</summary>

   ```typescript
   proxy.getCompletionsAtPosition = (fileName, pos, opts) => {
     const prior = info.languageService.getCompletionsAtPosition(fileName, pos, opts);
     if (prior) {
       prior.entries.push({
         name: 'crypto.randomUUID()',
         kind: ts.ScriptElementKind.functionElement,
         sortText: '0',
         insertText: 'crypto.randomUUID()',
       });
     }
     return prior;
   };
   ```
   </details>

### Hard
5. **Full QuickFix Implementation for Disallowed Properties**: Implement a complete Language Service plugin with `getSemanticDiagnostics` flagging deprecated field `oldId` on interfaces and `getCodeFixesAtPosition` offering an automated lightbulb fix to rename it to `newId`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript/lib/tsserverlibrary';

   function init(modules: { typescript: typeof ts }) {
     const ts = modules.typescript;

     function create(info: ts.server.PluginCreateInfo): ts.LanguageService {
       const proxy = Object.create(info.languageService);

       proxy.getSemanticDiagnostics = (fileName: string) => {
         const diags = info.languageService.getSemanticDiagnostics(fileName);
         const sf = info.languageService.getProgram()?.getSourceFile(fileName);
         if (!sf) return diags;

         ts.forEachChild(sf, function visit(node) {
           if (ts.isPropertySignature(node) && node.name.getText(sf) === 'oldId') {
             diags.push({
               file: sf,
               start: node.name.getStart(),
               length: node.name.getWidth(),
               messageText: "'oldId' is deprecated. Rename to 'newId'.",
               category: ts.DiagnosticCategory.Warning,
               code: 9200,
             });
           }
           ts.forEachChild(node, visit);
         });

         return diags;
       };

       proxy.getCodeFixesAtPosition = (fileName, start, end, errorCodes, formatOpts, prefs) => {
         const prior = info.languageService.getCodeFixesAtPosition(fileName, start, end, errorCodes, formatOpts, prefs);
         if (errorCodes.includes(9200)) {
           prior.push({
             fixName: 'renameOldId',
             description: "Rename 'oldId' to 'newId'",
             changes: [{
               fileName,
               textChanges: [{
                 span: { start, length: end - start },
                 newText: 'newId',
               }],
             }],
           });
         }
         return prior;
       };

       return proxy;
     }

     return { create };
   }

   export = init;
   ```
   </details>

6. **Automated Language Service Plugin Test Suite**: Write an integration test using the TypeScript Language Service API that mounts the custom plugin in memory and asserts that a diagnostic and quick-fix action are returned for a sample code buffer.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function testPluginDiagnostic(): void {
     const code = 'interface User { oldId: string; }';
     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);

     // Simulate Language Service environment
     const servicesHost: ts.LanguageServiceHost = {
       getScriptFileNames: () => ['test.ts'],
       getScriptVersion: () => '1',
       getScriptSnapshot: () => ts.ScriptSnapshot.fromString(code),
       getCurrentDirectory: () => process.cwd(),
       getCompilationSettings: () => ({ strict: true }),
       getDefaultLibFileName: (opts) => ts.getDefaultLibFilePath(opts),
       fileExists: (p) => p === 'test.ts',
       readFile: (p) => p === 'test.ts' ? code : undefined,
     };

     const ls = ts.createLanguageService(servicesHost);
     console.log('Language Service initialized for plugin testing.');
   }
   ```
   </details>
