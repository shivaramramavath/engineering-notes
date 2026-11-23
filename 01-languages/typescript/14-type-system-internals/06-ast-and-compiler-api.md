# TypeScript Compiler API and AST Manipulation

> Complete programmatic guide to the TypeScript Compiler API: `ts.createProgram`, `ts.createSourceFile`, AST visitors (`ts.forEachChild`, `ts.visitEachChild`), node factories (`ts.factory`), and AST transformation pipelines.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [14-type-system-internals/05-compiler-architecture.md](./05-compiler-architecture.md) |
| **TypeScript Baseline** | TS 4.0+ (`ts.factory`), TS 5.0+ |
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
While most developers interact with TypeScript through the `tsc` CLI or VS Code, TypeScript is also a comprehensive, programmatic JavaScript/TypeScript library. The **TypeScript Compiler API** exposes every internal component of the compiler—parser, AST tree, symbol table, type checker, and emitter—as a public programmatic API.

Mastering the Compiler API allows you to build custom static analysis tools, code generation engines, automated refactorings (codemods), custom ESLint rules, and compile-time schema analyzers that have complete access to the full semantic type graph.

---

## 2. Core Concepts
- **`ts.SourceFile` and `ts.Node`**: The fundamental data structures of the AST. A `SourceFile` is the root node representing an entire file.
- **AST Navigation**:
  - `node.forEachChild(cb)`: Lightweight read-only traversal.
  - `ts.visitEachChild(node, visitor, context)`: Transformational visitor that replaces nodes.
- **Node Factories (`ts.factory`)**: Standardized API for constructing and updating immutable AST nodes (`ts.factory.createIdentifier`, `ts.factory.createCallExpression`).
- **`ts.Program` and `ts.TypeChecker`**: Orchestrates multiple source files, resolves imports via a `CompilerHost`, and provides methods like `checker.getTypeAtLocation(node)` and `checker.getSymbolAtLocation(node)`.
- **Custom Transformers (`ts.TransformationContext`)**: Pluggable visitor functions that rewrite AST nodes before code generation.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|               ts.createProgram(rootFiles, options)          |
+------------------------------+------------------------------+
                               |
               +---------------+---------------+
               |                               |
               v                               v
    program.getSourceFiles()         program.getTypeChecker()
               |                               |
               v                               v
       AST Nodes (Syntax)              Semantic Types & Symbols
               |                               |
               +---------------+---------------+
                               |
                               v
                     ts.transform(sourceFile, [transformer])
                               |
                               v
                    ts.createPrinter().printFile()
```

---

## 4. Syntax & API Reference

```typescript
import * as ts from 'typescript';

// 1. Parsing text directly into an AST
const sourceFile = ts.createSourceFile(
  'sample.ts',
  'const answer = 42;',
  ts.ScriptTarget.Latest,
  /* setParentNodes */ true
);

// 2. Creating AST nodes with ts.factory
const id = ts.factory.createIdentifier('myVariable');
const literal = ts.factory.createNumericLiteral(100);
const varDecl = ts.factory.createVariableDeclaration(id, undefined, undefined, literal);

// 3. Printing an AST node to code string
const printer = ts.createPrinter({ newLine: ts.NewLineKind.LineFeed });
const outputCode = printer.printNode(ts.EmitHint.Unspecified, varDecl, sourceFile);
```

---

## 5. Practical Examples

### Example 1: Finding All Functions in a File
```typescript
import * as ts from 'typescript';

function listFunctions(code: string): string[] {
  const sf = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
  const funcNames: string[] = [];

  function visit(node: ts.Node) {
    if (ts.isFunctionDeclaration(node) && node.name) {
      funcNames.push(node.name.text);
    } else if (ts.isVariableDeclaration(node) && node.initializer && ts.isArrowFunction(node.initializer)) {
      if (ts.isIdentifier(node.name)) {
        funcNames.push(node.name.text);
      }
    }
    ts.forEachChild(node, visit);
  }

  visit(sf);
  return funcNames;
}
```

### Example 2: Codemod - Renaming an Identifier
```typescript
import * as ts from 'typescript';

export function renameIdentifierTransformer(
  oldName: string,
  newName: string
): ts.TransformerFactory<ts.SourceFile> {
  return context => rootNode => {
    function visit(node: ts.Node): ts.Node {
      if (ts.isIdentifier(node) && node.text === oldName) {
        return ts.factory.createIdentifier(newName);
      }
      return ts.visitEachChild(node, visit, context);
    }
    return ts.visitNode(rootNode, visit) as ts.SourceFile;
  };
}
```

### Example 3: Edge Case - Preserving Comments and Trivia
```typescript
// Synthetic nodes created via ts.factory do NOT have comments by default!
// To attach comments to generated nodes:
const newId = ts.factory.createIdentifier('calculatedValue');
ts.addSyntheticLeadingComment(
  newId,
  ts.SyntaxKind.MultiLineCommentTrivia,
  '* Generated by codemod *',
  true
);
```

### Example 4: Compiler Failure Case - Mutating Read-Only AST Nodes
```typescript
const sf = ts.createSourceFile('test.ts', 'let x = 1;', ts.ScriptTarget.Latest, true);
const firstNode = sf.statements[0];

// In modern TypeScript, directly mutating properties is illegal and corrupts compiler caches:
// @ts-expect-error TS2540: Cannot assign to 'kind' because it is a read-only property.
firstNode.kind = ts.SyntaxKind.EmptyStatement;
```

### Example 5: Idiomatic Type Extraction from Symbol Table
```typescript
export function inspectVariableType(filePath: string, varName: string): string | undefined {
  const program = ts.createProgram([filePath], { strict: true });
  const checker = program.getTypeChecker();
  const sourceFile = program.getSourceFile(filePath);
  if (!sourceFile) return undefined;

  let resolvedType: string | undefined;

  ts.forEachChild(sourceFile, node => {
    if (ts.isVariableStatement(node)) {
      for (const decl of node.declarationList.declarations) {
        if (ts.isIdentifier(decl.name) && decl.name.text === varName) {
          const type = checker.getTypeAtLocation(decl);
          resolvedType = checker.typeToString(type);
        }
      }
    }
  });

  return resolvedType;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **`setParentNodes` Parameter**: In `ts.createSourceFile`, the fourth parameter (`setParentNodes`) defaults to `false`. Without it, `node.parent` is `undefined`, breaking upward tree navigation. Always set `true` when analyzing code.
2. **Missing TypeChecker on Standalone SourceFile**: Nodes obtained via `ts.createSourceFile` do not have a `TypeChecker` attached. To query types or symbols, you *must* create a `ts.Program`.
3. **Updating vs Creating Nodes**: When writing transformers, prefer `ts.factory.update*` methods over `ts.factory.create*` to preserve original sourcemap source spans.
4. **Synthetic Node Diagnostics**: Synthetic nodes generated via `ts.factory` have position `-1`. Passing them to diagnostic reporters will crash unless synthetic spans are handled.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Forgetting `setParentNodes: true` | `node.parent` is `undefined` | Pass `true` as 4th arg to `createSourceFile` |
| Using string regex to refactor code | Corrupts strings, comments, and nested scopes | Use AST transformation (`ts.transform`) |
| Creating `ts.createProgram` in a tight loop | Massive memory and CPU starvation | Reuse existing Program instance |
| Directly mutating AST node properties | Silent compiler bugs, broken print writer | Use `ts.factory.update*` APIs |
| Calling `node.getText()` without sourceFile | Throws runtime error on synthetic nodes | Pass `sourceFile`: `node.getText(sourceFile)` |

---

## 8. Performance & Type-Checker Complexity
- AST traversal with `ts.forEachChild` is extremely fast ($\sim 500,000$ nodes per second).
- Instantiating a `ts.Program` requires I/O to read standard libraries (`lib.d.ts`). Caching `CompilerHost` and `SourceFile` instances across runs is mandatory for high-performance CLI tools.

---

## 9. Compiler Internals
In TypeScript's `src/compiler/factory/nodeFactory.ts`:
- Every factory function allocates an immutable `Node` object with a designated `SyntaxKind` and `NodeFlags.Synthesized`.
- Transformers run inside `ts.transform()`, passing ASTs through pipeline stages where updated nodes are substituted before being rendered by `ts.createPrinter()`.

---

## 10. Best Practices & Design Guidelines
- Pass `/* setParentNodes */ true` when creating source files.
- Always use `ts.is*` type guards (`ts.isIdentifier`, `ts.isCallExpression`) for type narrowing.
- Use `ts.factory` for generating AST nodes.
- When printing ASTs, configure `ts.createPrinter` with consistent newline settings.
- In tests, compare generated ASTs against formatted source strings.

---

## 11. Real-World Case Study
An enterprise engineering team had to migrate 1,400 files from a legacy deprecated authentication API `auth.loginWithToken(token)` to a new async method `auth.authenticateSession({ token })`. Performing manual refactoring or using regex was deemed too risky due to edge cases and comments.

**Automated Migration with Compiler API:**
The team wrote a 40-line AST transformer using `ts.transform` and `ts.factory`:
```typescript
function authMigrator(ctx: ts.TransformationContext) {
  return (sf: ts.SourceFile) => {
    function visit(node: ts.Node): ts.Node {
      if (
        ts.isCallExpression(node) &&
        ts.isPropertyAccessExpression(node.expression) &&
        node.expression.name.text === 'loginWithToken'
      ) {
        const tokenArg = node.arguments[0];
        const newObj = ts.factory.createObjectLiteralExpression([
          ts.factory.createPropertyAssignment('token', tokenArg),
        ]);
        const newMethod = ts.factory.createPropertyAccessExpression(
          node.expression.expression,
          'authenticateSession'
        );
        return ts.factory.updateCallExpression(node, newMethod, node.typeArguments, [newObj]);
      }
      return ts.visitEachChild(node, visit, ctx);
    }
    return ts.visitNode(sf, visit) as ts.SourceFile;
  };
}
```
All 1,400 files were migrated in 12 seconds with 100% syntactic precision and zero regressions.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `ts.forEachChild` and `ts.visitEachChild`?</summary>

- `ts.forEachChild`: A read-only traversal function that calls a callback for each child node. It cannot replace or transform nodes.
- `ts.visitEachChild`: A transformational traversal function used inside AST transformers that returns a new or updated node to replace the current child node.
</details>

<details>
<summary>2. Why does `node.parent` often evaluate to `undefined` in standalone ASTs?</summary>

Because by default, `ts.createSourceFile` does not attach parent pointers to save memory. You must pass `true` as the fourth parameter (`setParentNodes`) to populate `node.parent`.
</details>

<details>
<summary>3. What is the role of `ts.factory` in TypeScript?</summary>

`ts.factory` is the official modern API for constructing and updating immutable AST nodes, replacing older deprecated standalone creation functions (like `ts.createIdentifier`).
</details>

<details>
<summary>4. How can you convert an AST back into source code text?</summary>

Using `ts.createPrinter().printFile(sourceFile)` for an entire file, or `ts.createPrinter().printNode(ts.EmitHint.Unspecified, node, sourceFile)` for an individual node.
</details>

<details>
<summary>5. What is `ts.CompilerHost`?</summary>

It is an abstraction interface that mediates all compiler interactions with the underlying runtime environment (reading files, checking file existence, getting current directory, resolving module names).
</details>

<details>
<summary>6. How do you query the resolved type of an AST expression programmatically?</summary>

Obtain a `TypeChecker` from a `ts.Program`, then call `checker.getTypeAtLocation(node)`.
</details>

<details>
<summary>7. What is a "synthetic" node in the TypeScript Compiler API?</summary>

A node created programmatically in memory via `ts.factory` that was not parsed from original source text. It has position `-1` and no original source spans.
</details>

<details>
<summary>8. How do you check if an AST node is an exported function declaration?</summary>

Check `ts.isFunctionDeclaration(node)` and verify that `node.modifiers` contains a modifier with `kind === ts.SyntaxKind.ExportKeyword`.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Find Variable Names**: Write a function that prints the names of all `const` variable declarations in a code string.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function printConstNames(code: string): string[] {
     const sf = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     const names: string[] = [];

     ts.forEachChild(sf, node => {
       if (ts.isVariableStatement(node) && (node.declarationList.flags & ts.NodeFlags.Const)) {
         for (const decl of node.declarationList.declarations) {
           if (ts.isIdentifier(decl.name)) names.push(decl.name.text);
         }
       }
     });

     return names;
   }
   ```
   </details>

2. **Generate Return Statement**: Use `ts.factory` to generate a `return true;` AST node and print it as a string.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   const retNode = ts.factory.createReturnStatement(ts.factory.createTrue());
   const printer = ts.createPrinter();
   const sf = ts.createSourceFile('temp.ts', '', ts.ScriptTarget.Latest);
   const code = printer.printNode(ts.EmitHint.Unspecified, retNode, sf);
   console.log(code); // "return true;"
   ```
   </details>

### Medium
3. **Strip Console Logs Transformer**: Write a custom transformer that removes all `console.log(...)` expression statements from an AST.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function stripConsoleLogs(ctx: ts.TransformationContext) {
     return (root: ts.SourceFile) => {
       function visit(node: ts.Node): ts.Node | undefined {
         if (
           ts.isExpressionStatement(node) &&
           ts.isCallExpression(node.expression) &&
           ts.isPropertyAccessExpression(node.expression.expression) &&
           node.expression.expression.name.text === 'log'
         ) {
           return undefined; // Erases the node!
         }
         return ts.visitEachChild(node, visit, ctx);
       }
       return ts.visitNode(root, visit) as ts.SourceFile;
     };
   }
   ```
   </details>

4. **Extract JSDoc Comments**: Write a function using the Compiler API that extracts the JSDoc description for a target class in a file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function getJSDocDescription(code: string, className: string): string | undefined {
     const sf = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     let doc: string | undefined;

     ts.forEachChild(sf, node => {
       if (ts.isClassDeclaration(node) && node.name?.text === className) {
         const jsDocs = (node as any).jsDoc as ts.JSDoc[] | undefined;
         if (jsDocs && jsDocs.length > 0) {
           doc = jsDocs[0].comment?.toString();
         }
       }
     });

     return doc;
   }
   ```
   </details>

### Hard
5. **Full AST Codemod Runner**: Build an automated codemod CLI that accepts a file path, parses the file, runs an AST transformer that wraps all bare `throw new Error(...)` statements into `throw new AppError(...)`, and writes the formatted file back to disk.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';
   import * as fs from 'fs';

   export function runCodemod(filePath: string): void {
     const sourceText = fs.readFileSync(filePath, 'utf8');
     const sourceFile = ts.createSourceFile(filePath, sourceText, ts.ScriptTarget.Latest, true);

     const transformer = (ctx: ts.TransformationContext) => (root: ts.SourceFile) => {
       function visit(node: ts.Node): ts.Node {
         if (
           ts.isThrowStatement(node) &&
           ts.isNewExpression(node.expression) &&
           ts.isIdentifier(node.expression.expression) &&
           node.expression.expression.text === 'Error'
         ) {
           const newExpr = ts.factory.updateNewExpression(
             node.expression,
             ts.factory.createIdentifier('AppError'),
             node.expression.typeArguments,
             node.expression.arguments
           );
           return ts.factory.updateThrowStatement(node, newExpr);
         }
         return ts.visitEachChild(node, visit, ctx);
       }
       return ts.visitNode(root, visit) as ts.SourceFile;
     };

     const result = ts.transform(sourceFile, [transformer]);
     const printer = ts.createPrinter();
     const output = printer.printFile(result.transformed[0]);
     fs.writeFileSync(filePath, output, 'utf8');
     console.log(`Codemod successfully applied to ${filePath}`);
   }
   ```
   </details>

6. **Automatic API Route Contract Generator**: Write a utility using the `TypeChecker` that inspects a source file containing route handlers, extracts their return types, and generates a typed TypeScript client SDK file automatically.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function generateClientSdk(routeFilePath: string): string {
     const program = ts.createProgram([routeFilePath], { strict: true });
     const checker = program.getTypeChecker();
     const sourceFile = program.getSourceFile(routeFilePath);
     if (!sourceFile) return '';

     const endpoints: Array<{ name: string; returnType: string }> = [];

     ts.forEachChild(sourceFile, node => {
       if (ts.isFunctionDeclaration(node) && node.name && node.modifiers?.some(m => m.kind === ts.SyntaxKind.ExportKeyword)) {
         const type = checker.getTypeAtLocation(node);
         const callSignatures = type.getCallSignatures();
         if (callSignatures.length > 0) {
           const returnType = checker.getReturnTypeOfSignature(callSignatures[0]);
           endpoints.push({
             name: node.name.text,
             returnType: checker.typeToString(returnType),
           });
         }
       }
     });

     return `// Auto-generated API Client\nexport interface ApiClient {\n` +
       endpoints.map(e => `  ${e.name}(): Promise<${e.returnType}>;`).join('\n') +
       `\n}\n`;
   }
   ```
   </details>
