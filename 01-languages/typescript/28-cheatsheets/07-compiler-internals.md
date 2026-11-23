# Module 28: TypeScript Compiler Internals Cheatsheet

> **Quick Reference**: Pipeline stages, AST node flags, Symbol and Type tables, and Checker internals reference.

---

## 1. Five-Stage Compiler Pipeline

```text
Source Code (.ts)
       │
       ▼  [Scanner]
Token Stream
       │
       ▼  [Parser]
Abstract Syntax Tree (AST / SourceFile)
       │
       ▼  [Binder]
Symbol Table (Identifiers linked to Declarations)
       │
       ▼  [Checker]
Type Checking, Type Computation & Semantic Diagnostics
       │
       ▼  [Emitter]
JavaScript (.js), Source Maps (.map), Declarations (.d.ts)
```

---

## 2. Compiler Internal Data Structures

| Structure | Ownership | Description |
|---|---|---|
| `SourceFile` | Parser | AST root representing a single source module |
| `Node` | Parser | Syntax element in AST with `pos`, `end`, and `flags` |
| `Symbol` | Binder | Semantic name container linking multiple AST declarations |
| `Type` | Checker | Abstract type object calculated during semantic analysis |
| `Program` | Root Orchestrator | Graph containing all `SourceFile`s and referenced libraries |

---

## 3. Useful Compiler API Methods

```typescript
import ts from "typescript";

// Parse AST from string
const sourceFile = ts.createSourceFile("temp.ts", "const x: number = 42;", ts.ScriptTarget.Latest, true);

// Create Program & Checker
const program = ts.createProgram(["src/index.ts"], { strict: true });
const checker = program.getTypeChecker();

// Inspect Node Type
ts.forEachChild(sourceFile, (node) => {
  if (ts.isVariableStatement(node)) {
    console.log("Found variable declaration statement");
  }
});
```\n