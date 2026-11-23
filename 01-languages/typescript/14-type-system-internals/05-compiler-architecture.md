# Compiler Architecture and The Five Compilation Phases in TypeScript

> Comprehensive exploration of TypeScript compiler internals: the five pipeline stages (Scanner -> Parser -> Binder -> Type Checker -> Emitter), AST representations, symbol tables, and control-flow graphs.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-and-tsconfig/01-compiler-architecture.md](../13-compiler-and-tsconfig/01-compiler-architecture.md), [14-type-system-internals/00-type-erasure-and-runtime.md](./00-type-erasure-and-runtime.md) |
| **TypeScript Baseline** | TS 1.0+, TS 5.0+ |
| **Tested tsconfig** | `"strict": true, "target": "ES2022", "module": "NodeNext"` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-Common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
The TypeScript compiler (`tsc`) is one of the most sophisticated software engineering artifacts in modern computing. It processes millions of lines of complex code, performs type checking on Turing-complete type programs, and emits standard JavaScript in milliseconds.

Understanding the five distinct pipeline phases of the compiler—**Scanner, Parser, Binder, Type Checker, and Emitter**—demystifies how TypeScript processes code, resolves symbols, performs control-flow narrowing, and generates output. This knowledge is essential for writing custom language tools, linters, AST codemods, and debugging complex compiler behaviors.

---

## 2. Core Concepts
- **Phase 1: Scanner (`scanner.ts`)**: Lexical analysis. Converts raw source UTF-8 text into a stream of tokens (`SyntaxKind.Identifier`, `SyntaxKind.ConstKeyword`, etc.).
- **Phase 2: Parser (`parser.ts`)**: Syntactic analysis. Consumes tokens and constructs the **Abstract Syntax Tree (AST)** represented by `SourceFile` and `Node` objects.
- **Phase 3: Binder (`binder.ts`)**: Semantic linking. Walks the AST and creates **Symbols** (`ts.Symbol`). Links identifier names to declarations, establishes lexical scopes, and constructs the **Control Flow Graph (CFG)** for flow analysis.
- **Phase 4: Type Checker (`checker.ts`)**: Semantic verification. The largest file in the codebase ($\approx 45,000$ lines). Verifies assignability, evaluates conditionals and generics, calculates types, and emits diagnostics.
- **Phase 5: Emitter (`emitter.ts`)**: Code generation. Transforms AST nodes into JavaScript files (`.js`), declaration files (`.d.ts`), and sourcemaps (`.js.map`).

---

## 3. How It Works Internally

```
      Source Code Text (".ts" file)
                   |
                   v
         +-------------------+
         | 1. Scanner        | Lexical Analysis: Produces Token Stream
         +---------+---------+
                   |
                   v
         +-------------------+
         | 2. Parser         | Syntactic Analysis: Constructs AST (SourceFile)
         +---------+---------+
                   |
                   v
         +-------------------+
         | 3. Binder         | Builds Symbol Table, Scopes, and Flow Graph
         +---------+---------+
                   |
                   v
         +-------------------+
         | 4. Type Checker   | Resolves Types, Assignability, Emits Diagnostics
         +---------+---------+
                   |
                   v
         +-------------------+
         | 5. Emitter        | Generates .js, .d.ts, and .map files
         +-------------------+
```

Key Architectural Principles:
1. **Lazy Evaluation**: The Parser and Binder are fast and eagerly run over all files. The Type Checker evaluates types **lazily on demand**: a function's body is only type-checked when queried or during program emission.
2. **Immutable AST**: Once created by the parser, AST nodes are generally treated as immutable.
3. **Symbol Table**: Symbols bridge the syntax world (AST nodes) with the semantic world (Types). Multiple declarations (e.g. declaration merging between an interface and a namespace) point to a single `Symbol`.

---

## 4. Syntax & API Reference

```typescript
import * as ts from 'typescript';

// 1. Compiler Program Orchestration
const program = ts.createProgram(['src/index.ts'], {
  strict: true,
  target: ts.ScriptTarget.ES2022,
});

// 2. Accessing the Type Checker (Phase 4)
const checker = program.getTypeChecker();

// 3. Emitting output files (Phase 5)
const emitResult = program.emit();

// 4. Inspecting Diagnostics
const diagnostics = ts.getPreEmitDiagnostics(program);
diagnostics.forEach(d => {
  console.log(ts.flattenDiagnosticMessageText(d.messageText, '\n'));
});
```

---

## 5. Practical Examples

### Example 1: Inspecting the Scanner (Tokenization)
```typescript
import * as ts from 'typescript';

const code = 'const total: number = 42;';
const scanner = ts.createScanner(ts.ScriptTarget.Latest, true, ts.LanguageVariant.Standard, code);

let token = scanner.scan();
while (token !== ts.SyntaxKind.EndOfFileToken) {
  console.log(`Token: ${ts.SyntaxKind[token]} -> '${scanner.getTokenText()}'`);
  token = scanner.scan();
}
// Outputs: ConstKeyword, WhitespaceTrivia, Identifier, ColonToken, NumberKeyword, EqualsToken, NumericLiteral, SemicolonToken
```

### Example 2: Inspecting AST Nodes with the Parser
```typescript
import * as ts from 'typescript';

const sourceFile = ts.createSourceFile('demo.ts', 'let x = 10;', ts.ScriptTarget.Latest, true);

function traverse(node: ts.Node, indent = 0) {
  console.log(`${' '.repeat(indent)}${ts.SyntaxKind[node.kind]} [${node.pos}-${node.end}]`);
  ts.forEachChild(node, child => traverse(child, indent + 2));
}

traverse(sourceFile);
```

### Example 3: Edge Case - Declaration Merging in the Binder
```typescript
// File: merge.ts
interface Box {
  width: number;
}
interface Box {
  height: number;
}

// In the Binder:
// A single Symbol 'Box' is created with SymbolFlags.Interface.
// It contains two declarations in its .declarations array!
```

### Example 4: Compiler Failure Case - Syntax Error vs Semantic Error
```typescript
// 1. Syntactic Error (Caught by Phase 2: Parser)
// const = 10;
// Error TS1005: Identifier expected.

// 2. Semantic Error (Caught by Phase 4: Type Checker)
const num: number = "hello";
// Error TS2322: Type 'string' is not assignable to type 'number'.
```

### Example 5: Idiomatic Custom AST Traversal
```typescript
export function findExportedFunctionNames(sourceCode: string): string[] {
  const sourceFile = ts.createSourceFile('temp.ts', sourceCode, ts.ScriptTarget.Latest, true);
  const names: string[] = [];

  ts.forEachChild(sourceFile, node => {
    if (ts.isFunctionDeclaration(node) && node.name) {
      const isExported = node.modifiers?.some(m => m.kind === ts.SyntaxKind.ExportKeyword);
      if (isExported) {
        names.push(node.name.text);
      }
    }
  });

  return names;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Node Position Spans (`pos` vs `getStart()`)**: An AST node's `.pos` property includes leading trivia (whitespace, comments). To get the actual token start position without whitespace, always invoke `node.getStart(sourceFile)`.
2. **Symbols vs Types**: A `Symbol` represents a named declaration in the source code (e.g. identifier `Person`). A `Type` represents the semantic shape or contract. A single symbol can have multiple types across control-flow branches.
3. **Syntactic vs Semantic Diagnostics**: Running `sourceFile.getSyntacticDiagnostics()` only checks for grammar and syntax errors (Parser). Type errors (like `TS2322`) only exist in `program.getSemanticDiagnostics()` (Checker).
4. **Memory Retention in Programs**: Holding a reference to a `ts.Program` instance keeps all ASTs, Symbols, and SourceFiles in memory. Explicitly dispose or nullify programs in long-running watcher services.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Using `node.pos` for string slicing | Slices leading comments and whitespace | Use `node.getStart()` and `node.getEnd()` |
| Running `checker` on code with syntax errors | Checker crashes or yields bizarre cascaded errors | Check `getSyntacticDiagnostics()` first |
| Mutating AST nodes directly in memory | Corrupts compiler caches and sourcemaps | Use `ts.factory` and `ts.transform` |
| Calling `getTypeAtLocation` on thousands of trivial nodes | Extreme memory and CPU overhead | Filter nodes by `SyntaxKind` before querying checker |
| Assuming a Symbol has only one Declaration | Misses declaration merging (interfaces, namespaces) | Always iterate `symbol.declarations` |

---

## 8. Performance & Type-Checker Complexity
- Phase 1 (Scanner) and Phase 2 (Parser) account for $\approx 15\%$ of total compilation time.
- Phase 3 (Binder) is extremely fast, taking $<5\%$ of time.
- Phase 4 (Type Checker) represents **$70\% - 80\%$ of total compilation time**, as it evaluates complex type relations and conditional evaluations.
- Phase 5 (Emitter) accounts for $\approx 10\%$ of time.

---

## 9. Compiler Internals
- **FlowGraph**: Inside `binder.ts`, the compiler creates a `FlowLabel` and `FlowCondition` graph that models branches (`if`, `switch`, `while`). During type checking, `checker.ts` uses this graph to narrow types along each branch.
- **Node Factory (`ts.factory`)**: In modern TypeScript, creating or transforming AST nodes is performed through `ts.factory.createIdentifier(...)`, ensuring AST invariants are preserved.

---

## 10. Best Practices & Design Guidelines
- When building AST tools, decouple parsing from type checking: if you only need AST structure, avoid instantiating the heavy `TypeChecker`.
- Use `ts.forEachChild` rather than recursive function calls to prevent call stack exhaustion on deep ASTs.
- Always pass `sourceFile` to `node.getText(sourceFile)` for correct token text retrieval.
- Use `ts.transform` with custom transformers when modifying code rather than string regex replacements.
- Leverage `ts.getPreEmitDiagnostics(program)` to collect both syntactic and semantic errors in one unified call.

---

## 11. Real-World Case Study
A developer tooling team built a documentation generator that parsed 500 TypeScript API files. The initial implementation instantiated `ts.createProgram` and queried `checker.getTypeAtLocation` on every single AST token. Execution took 120 seconds and consumed 3.5GB of RAM.

**Optimization:**
1. Separated the pipeline: used `ts.createSourceFile` (Parser only) to extract JSDoc comments and method signatures without invoking the `TypeChecker`.
2. Only invoked `checker.getTypeAtLocation` on public exported interfaces requiring deep type expansion.
Documentation generation time plummeted from **120 seconds to 1.8 seconds**, with memory consumption dropping to 85MB.

---

## 12. Interview Questions

<details>
<summary>1. What are the five main phases of the TypeScript compiler?</summary>

1. **Scanner**: Converts source text into lexical tokens.
2. **Parser**: Constructs the Abstract Syntax Tree (AST) from tokens.
3. **Binder**: Links AST nodes to Symbols, establishes scopes, and builds the Control Flow Graph.
4. **Type Checker**: Performs type analysis, validates assignability, and produces semantic diagnostics.
5. **Emitter**: Translates AST nodes into JavaScript files, declaration files, and sourcemaps.
</details>

<details>
<summary>2. What is the difference between a `Node`, a `Symbol`, and a `Type`?</summary>

- `Node`: A syntactic element in the AST representing source code structure (e.g. a variable declaration).
- `Symbol`: A semantic entity representing a named declaration (or merged declarations) in the symbol table.
- `Type`: The semantic type assigned to an expression or symbol by the Type Checker.
</details>

<details>
<summary>3. Why is type checking in TypeScript described as "lazy"?</summary>

Because the compiler does not eagerly compute the types of every function body or variable during parsing or binding. Types are only calculated when an expression is explicitly referenced, queried by the language service, or during the emit phase.
</details>

<details>
<summary>4. What does the Binder do that the Parser does not?</summary>

The Parser only builds the AST tree structure. The Binder walks the AST to connect declarations with the symbol table, establishes lexical parent/child scopes, and builds the control-flow graph for flow analysis.
</details>

<details>
<summary>5. What is the difference between `node.pos` and `node.getStart()`?</summary>

`node.pos` is the position including leading trivia (whitespace and comments) before the node. `node.getStart()` computes the start position of the actual first token of the node, skipping trivia.
</details>

<details>
<summary>6. Which phase of the compiler consumes the most CPU time?</summary>

Phase 4: The Type Checker (`checker.ts`), which typically accounts for 70% to 80% of total compilation time due to solving generic constraints, recursive types, and union assignability.
</details>

<details>
<summary>7. How does TypeScript handle declaration merging in the compiler phases?</summary>

In Phase 3 (Binder), when the binder encounters a declaration with an existing name in the same scope that supports merging (like two interfaces), it attaches the new AST declaration to the existing `Symbol`'s `declarations` array rather than creating a duplicate symbol.
</details>

<details>
<summary>8. How do you create an AST programmatically without parsing source text?</summary>

Using the `ts.factory` API (e.g. `ts.factory.createVariableStatement(...)`, `ts.factory.createIdentifier(...)`).
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Count AST Nodes**: Write a function using `ts.forEachChild` that counts the total number of AST nodes in a TypeScript code string.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function countNodes(code: string): number {
     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     let count = 0;

     function visit(node: ts.Node) {
       count++;
       ts.forEachChild(node, visit);
     }

     visit(sourceFile);
     return count;
   }
   ```
   </details>

2. **Extract Identifiers**: Write a function that returns an array of all identifier names in a code string.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function getIdentifiers(code: string): string[] {
     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     const identifiers: string[] = [];

     function visit(node: ts.Node) {
       if (ts.isIdentifier(node)) {
         identifiers.push(node.text);
       }
       ts.forEachChild(node, visit);
     }

     visit(sourceFile);
     return identifiers;
   }
   ```
   </details>

### Medium
3. **Syntax Error Extractor**: Write a script that checks a code string for syntax errors (Parser phase only) without running the full type checker.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function getSyntaxErrors(code: string): string[] {
     const sourceFile = ts.createSourceFile('test.ts', code, ts.ScriptTarget.Latest, true);
     const diagnostics = (sourceFile as any).parseDiagnostics || [];

     return diagnostics.map((d: ts.Diagnostic) =>
       ts.flattenDiagnosticMessageText(d.messageText, '\n')
     );
   }
   ```
   </details>

4. **Symbol Inspector**: Write a script using `ts.createProgram` that prints the names of all exported symbols in a source file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function printExportedSymbols(filePath: string): void {
     const program = ts.createProgram([filePath], { strict: true });
     const checker = program.getTypeChecker();
     const sourceFile = program.getSourceFile(filePath);

     if (sourceFile) {
       const moduleSymbol = checker.getSymbolAtLocation(sourceFile);
       if (moduleSymbol) {
         const exports = checker.getExportsOfModule(moduleSymbol);
         console.log('Exported symbols:', exports.map(e => e.name));
       }
     }
   }
   ```
   </details>

### Hard
5. **Custom AST Transformer for Console Logging**: Author a custom AST transformer function using `ts.transform` and `ts.factory` that prepends a file name string literal argument to every `console.log(...)` call in a file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function createLoggerTransformer(fileName: string): ts.TransformerFactory<ts.SourceFile> {
     return context => {
       return rootNode => {
         function visit(node: ts.Node): ts.Node {
           if (
             ts.isCallExpression(node) &&
             ts.isPropertyAccessExpression(node.expression) &&
             node.expression.expression.getText(rootNode) === 'console' &&
             node.expression.name.text === 'log'
           ) {
             const fileArg = ts.factory.createStringLiteral(`[${fileName}]`);
             return ts.factory.updateCallExpression(
               node,
               node.expression,
               node.typeArguments,
               [fileArg, ...node.arguments]
             );
           }
           return ts.visitEachChild(node, visit, context);
         }
         return ts.visitNode(rootNode, visit) as ts.SourceFile;
       };
     };
   }
   ```
   </details>

6. **Control Flow Graph Visualizer**: Write a tool that inspects the binder's control flow container on an AST node and outputs a trace of flow conditions and labels leading to a target return statement.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as ts from 'typescript';

   export function traceControlFlow(sourceCode: string): void {
     const sourceFile = ts.createSourceFile('flow.ts', sourceCode, ts.ScriptTarget.Latest, true);

     ts.forEachChild(sourceFile, node => {
       if (ts.isFunctionDeclaration(node) && node.body) {
         ts.forEachChild(node.body, statement => {
           if (ts.isReturnStatement(statement)) {
             const flowNode = (statement as any).flowNode;
             console.log('Flow node associated with return:', flowNode ? flowNode.flags : 'None');
           }
         });
       }
     });
   }
   ```
   </details>
