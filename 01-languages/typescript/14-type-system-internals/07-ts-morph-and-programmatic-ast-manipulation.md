# Programmatic AST Manipulation with ts-morph

> High-level TypeScript code analysis and transformation with `ts-morph`: project management, AST querying, code generation, refactoring, symbol navigation, and automated migrations.

| Metadata | Details |
| :--- | :--- |
| **Category** | Type System Internals |
| **Difficulty** | Advanced |
| **Prerequisites** | [14-type-system-internals/06-ast-and-compiler-api.md](./06-ast-and-compiler-api.md) |
| **TypeScript Baseline** | TS 4.5+, ts-morph 20.x+ |
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
While the native TypeScript Compiler API is powerful, its lower-level API is notoriously verbose and difficult to work with: AST nodes are immutable, modifying a single statement requires rebuilding parents via `ts.factory`, and manipulating source files requires manual string index calculations and trivia tracking.

`ts-morph` (formerly `ts-simple-ast`) is the premier higher-level wrapper around the TypeScript Compiler API. It provides an intuitive, mutable, object-oriented wrapper around `SourceFile`, `ClassDeclaration`, `InterfaceDeclaration`, and `TypeChecker`. With `ts-morph`, complex code generation, automated large-scale refactorings, and AST queries can be achieved in a fraction of the code required by the raw compiler API.

---

## 2. Core Concepts
- **`Project`**: The central container managing `tsconfig.json`, file systems, source files, and the underlying `ts.Program`.
- **Wrapped AST Nodes**: Every native node is wrapped in a type-safe class (`ClassDeclaration`, `FunctionDeclaration`, `PropertySignature`) exposing fluent mutation methods (`addMethod`, `setReturnType`, `rename`).
- **Synchronized Source Code**: Modifying a node automatically recalculates line numbers, positions, and source maps without manual printer calls.
- **Type Navigation**: Direct methods like `node.getType()` and `node.getDefinitions()` provide seamless access to the type checker.
- **In-Memory File System**: Ability to run transformations purely in memory for blazing-fast unit tests without touching disk.

---

## 3. How It Works Internally

```
+-------------------------------------------------------------+
|                      ts-morph Project                       |
+------------------------------+------------------------------+
                               |
                               v
            Wraps native ts.Program and ts.TypeChecker
                               |
            +------------------+------------------+
            |                                     |
            v                                     v
   Native ts.SourceFile                 ts-morph SourceFile
   (Immutable AST Nodes)                (Mutable Wrappers)
                                                  |
                               .addClass({ name: 'User' })
                                                  |
                                                  v
                                      Regenerates AST Subtree
                                      Updates parent spans
                                      Preserves formatting!
```

---

## 4. Syntax & API Reference

```typescript
import { Project, StructureKind } from 'ts-morph';

// 1. Initialize project from tsconfig.json
const project = new Project({
  tsConfigFilePath: 'tsconfig.json',
});

// 2. Querying source files
const sourceFile = project.getSourceFileOrThrow('src/index.ts');
const classes = sourceFile.getClasses();

// 3. Mutating AST fluently
sourceFile.addInterface({
  name: 'UserProfile',
  isExported: true,
  properties: [
    { name: 'id', type: 'string' },
    { name: 'email', type: 'string' },
  ],
});

// 4. Saving changes to disk
await project.save();
```

---

## 5. Practical Examples

### Example 1: Automated Interface Generation from DB Schema
```typescript
import { Project } from 'ts-morph';

export function generateSchemaInterface(outputPath: string, tableName: string, columns: Record<string, string>) {
  const project = new Project();
  const sf = project.createSourceFile(outputPath, '', { overwrite: true });

  sf.addInterface({
    name: `${tableName}Record`,
    isExported: true,
    properties: Object.entries(columns).map(([name, type]) => ({
      name,
      type,
    })),
  });

  sf.formatText();
  sf.saveSync();
}
```

### Example 2: Refactoring - Renaming All Usages Across Monorepo
```typescript
import { Project } from 'ts-morph';

export function renameInterfaceGlobally(projectPath: string, oldName: string, newName: string) {
  const project = new Project({ tsConfigFilePath: projectPath });

  for (const sf of project.getSourceFiles()) {
    const targetInterface = sf.getInterface(oldName);
    if (targetInterface) {
      // Renames declaration AND all references across all files in the project!
      targetInterface.rename(newName);
    }
  }

  project.saveSync();
}
```

### Example 3: Edge Case - Navigating Inferred Union Types
```typescript
import { Project } from 'ts-morph';

const project = new Project({ useInMemoryFileSystem: true });
const sf = project.createSourceFile('test.ts', 'const x = Math.random() > 0.5 ? "hello" : 42;');

const varDecl = sf.getVariableDeclarationOrThrow('x');
const type = varDecl.getType();

console.log(type.getText()); // 'string | number'
console.log(type.isUnion()); // true
const unionTypes = type.getUnionTypes().map(t => t.getText()); // ['string', 'number']
```

### Example 4: Compiler Failure Case - Stale Node References
```typescript
import { Project } from 'ts-morph';

const project = new Project({ useInMemoryFileSystem: true });
const sf = project.createSourceFile('test.ts', 'function a() {} function b() {}');

const fnA = sf.getFunctionOrThrow('a');
fnA.remove(); // Removes function from AST

// Attempting to access an already deleted node throws:
// Error: Attempted to get information from a node that was removed or forgotten.
fnA.getName();
```

### Example 5: Idiomatic Documentation Generator
```typescript
import { Project } from 'ts-morph';

export function extractApiDocumentation(filePath: string): Array<{ name: string; jsDoc: string }> {
  const project = new Project();
  const sf = project.addSourceFileAtPath(filePath);

  return sf.getFunctions().map(fn => {
    const docs = fn.getJsDocs().map(d => d.getDescription().trim()).join('\n');
    return {
      name: fn.getName() || 'anonymous',
      jsDoc: docs,
    };
  });
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **The "Forgotten Node" Error**: When an AST node is deleted or a parent is radically rewritten, child node wrappers are marked as "forgotten". Calling methods on forgotten nodes throws a runtime error. Re-query nodes after heavy structural rewrites.
2. **Performance on Massive Projects**: Loading an entire monorepo into a single `ts-morph` `Project` instance parses hundreds of files into memory at once. Add only specific files using `project.addSourceFileAtPath()` when analyzing focused subtrees.
3. **`save()` vs `saveSync()`**: In serverless functions or CLI scripts, always remember to call `await project.save()` or `project.saveSync()`; in-memory modifications do not write to disk automatically.
4. **Formatting Preservation**: Mutating code programmatically can sometimes alter indentation. Call `sourceFile.formatText()` before saving to apply Prettier/TypeScript default formatting rules.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Storing node references across deletions | Throws "node was removed or forgotten" error | Re-fetch nodes from source file after mutations |
| Loading entire monorepo `node_modules` | Out of memory crash in ts-morph | Use `skipAddingFilesFromTsConfig: true` |
| Calling `.saveSync()` in async servers | Blocks Node event loop | Use `await project.save()` |
| Manual string concatenation for interfaces | Syntax errors, unformatted code | Use `sf.addInterface(...)` structured API |
| Forgetting to call `formatText()` | Emitted code has erratic indentation | Call `sf.formatText()` before saving |

---

## 8. Performance & Type-Checker Complexity
- `ts-morph` uses lazy wrapper instantiation: AST node wrappers are only created when explicitly accessed.
- For maximum speed in read-only analysis tasks, set `compilerOptions: { skipLibCheck: true }` and pass `skipAddingFilesFromTsConfig: true`.

---

## 9. Compiler Internals
Under the hood, `ts-morph`:
- Maintains a reference to the native `ts.Program`.
- Intercepts mutations and calls native internal methods (`ts.textChanges.ChangeTracker`) to calculate text replacements.
- Applies text diffs and re-parses modified node subtrees lazily.

---

## 10. Best Practices & Design Guidelines
- Use `ts-morph` for high-level codemods, code generators, and documentation tools.
- Enable `useInMemoryFileSystem: true` when writing unit tests for your codemods.
- Leverage structured objects (`StructureKind.Interface`, `StructureKind.PropertySignature`) rather than raw string injection.
- Always use `.rename()` when changing symbol names to automatically update all import statements and usages across the project.
- Clean up project references in long-running processes to prevent memory leaks.

---

## 11. Real-World Case Study
A financial services engineering team had to migrate 2,500 Redux action creators to Redux Toolkit `createSlice` definitions. An intern attempted using regular expressions, resulting in broken import paths and corrupted JSX in 200 files.

**Solution with ts-morph:**
The team wrote a 60-line automated codemod script with `ts-morph`:
```typescript
import { Project } from 'ts-morph';

export function migrateActions(projectPath: string) {
  const project = new Project({ tsConfigFilePath: projectPath });

  for (const sf of project.getSourceFiles('src/actions/**/*.ts')) {
    const actions = sf.getFunctions();
    // Analyze action signatures and emit slice reducers
    sf.addImportDeclaration({
      moduleSpecifier: '@reduxjs/toolkit',
      namedImports: ['createSlice'],
    });
    // Transform code cleanly...
  }

  project.saveSync();
}
```
All 2,500 actions were migrated across the repository in 45 seconds with 100% type safety and zero broken imports.

---

## 12. Interview Questions

<details>
<summary>1. What is ts-morph and why is it used instead of the raw TypeScript Compiler API?</summary>

`ts-morph` is an open-source library that wraps the low-level TypeScript Compiler API in a fluent, object-oriented, mutable interface. It simplifies AST querying, manipulation, refactoring, and code generation by abstracting away manual node factory updates, position arithmetic, and printer configuration.
</details>

<details>
<summary>2. What is a "forgotten node" in ts-morph?</summary>

When a node's text is removed or its parent structure is regenerated, `ts-morph` invalidates the wrapper instance to prevent accessing stale or nonexistent AST pointers. Attempting to call methods on an invalidated wrapper throws an error indicating the node was removed or forgotten.
</details>

<details>
<summary>3. How does `ts-morph` handle renaming a symbol across multiple files?</summary>

Calling `.rename("newName")` on a declaration node uses the underlying TypeScript Language Service to locate all references across all project files and updates both the declaration and all usage sites synchronously.
</details>

<details>
<summary>4. How can you write unit tests for ts-morph transformations without writing to disk?</summary>

By initializing the project with `new Project({ useInMemoryFileSystem: true })`. All files are created, modified, and verified in memory.
</details>

<details>
<summary>5. What is the difference between `addSourceFileAtPath` and `createSourceFile`?</summary>

- `addSourceFileAtPath`: Reads an existing file from the filesystem and adds it to the project.
- `createSourceFile`: Creates a new file in memory with specified initial code text.
</details>

<details>
<summary>6. How do you format emitted code in ts-morph?</summary>

By calling `sourceFile.formatText()`, which applies TypeScript's built-in formatting engine (or Prettier) to fix indentation, spacing, and newlines.
</details>

<details>
<summary>7. How do you query all exported types from a source file in ts-morph?</summary>

Using `sourceFile.getExportedDeclarations()`, which returns a `Map<string, ExportedDeclarations[]>`.
</details>

<details>
<summary>8. How do you inspect whether a type is a union in ts-morph?</summary>

```typescript
const type = node.getType();
if (type.isUnion()) {
  const subTypes = type.getUnionTypes();
}
```
</details>

---

## 13. Exercises & Challenges

### Easy
1. **In-Memory File Creation**: Use `ts-morph` to create a virtual source file `math.ts` in memory containing `export const add = (a: number, b: number) => a + b;`.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Project } from 'ts-morph';

   const project = new Project({ useInMemoryFileSystem: true });
   const sf = project.createSourceFile('math.ts', 'export const add = (a: number, b: number) => a + b;');
   console.log(sf.getText());
   ```
   </details>

2. **Add Property to Interface**: Write a function that adds an optional `age: number` property to an existing interface `User` in a source file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { SourceFile } from 'ts-morph';

   export function addAgeProperty(sf: SourceFile): void {
     const userInterface = sf.getInterfaceOrThrow('User');
     userInterface.addProperty({
       name: 'age',
       type: 'number',
       hasQuestionToken: true,
     });
   }
   ```
   </details>

### Medium
3. **Extract All Class Methods**: Write a function that accepts a `SourceFile` and returns a list of all method names across all classes in that file.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { SourceFile } from 'ts-morph';

   export function getAllClassMethods(sf: SourceFile): string[] {
     return sf.getClasses().flatMap(c => c.getMethods().map(m => m.getName()));
   }
   ```
   </details>

4. **Add JSDoc to Exported Functions**: Implement a script that finds all exported functions without JSDoc in a file and adds a standard `@param` template.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { SourceFile } from 'ts-morph';

   export function addMissingJsDocs(sf: SourceFile): void {
     for (const fn of sf.getFunctions()) {
       if (fn.isExported() && fn.getJsDocs().length === 0) {
         const params = fn.getParameters().map(p => `@param ${p.getName()}`);
         fn.addJsDoc({
           description: 'Generated function documentation.',
           tags: params.map(text => ({ tagName: 'param', text })),
         });
       }
     }
   }
   ```
   </details>

### Hard
5. **Full REST Controller to OpenAPI Schema Generator**: Write a `ts-morph` script that inspects a controller class with decorators (`@Get`, `@Post`), extracts endpoint paths, argument types, and return types, and outputs a structured OpenAPI 3.0 path specification.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Project } from 'ts-morph';

   export function extractOpenApiSpecs(filePath: string): Record<string, unknown> {
     const project = new Project();
     const sf = project.addSourceFileAtPath(filePath);
     const specs: Record<string, unknown> = {};

     for (const cls of sf.getClasses()) {
       for (const method of cls.getMethods()) {
         const getDec = method.getDecorator('Get');
         if (getDec) {
           const routeArg = getDec.getArguments()[0]?.getText().replace(/['"]/g, '') || '/';
           const returnType = method.getReturnType().getText();
           specs[routeArg] = {
             get: {
               operationId: method.getName(),
               responses: {
                 '200': { description: 'Success', schema: { type: returnType } },
               },
             },
           };
         }
       }
     }

     return specs;
   }
   ```
   </details>

6. **Automatic Barrel File Generator**: Implement a utility using `ts-morph` that scans a directory of TypeScript modules, identifies all exported public types and functions, and creates a clean `index.ts` re-exporting only non-internal declarations.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { Project } from 'ts-morph';
   import * as path from 'path';

   export function generateBarrelFile(dirPath: string): void {
     const project = new Project();
     const sourceFiles = project.addSourceFilesAtPaths(path.join(dirPath, '**/*.ts'));
     const barrelFile = project.createSourceFile(path.join(dirPath, 'index.ts'), '', { overwrite: true });

     for (const sf of sourceFiles) {
       if (sf.getBaseName() === 'index.ts') continue;
       const relativePath = './' + path.relative(dirPath, sf.getFilePath()).replace(/\.ts$/, '.js');

       const exportedNames: string[] = [];
       for (const [name] of sf.getExportedDeclarations()) {
         if (!name.startsWith('_')) {
           exportedNames.push(name);
         }
       }

       if (exportedNames.length > 0) {
         barrelFile.addExportDeclaration({
           moduleSpecifier: relativePath,
           namedExports: exportedNames,
         });
       }
     }

     barrelFile.formatText();
     barrelFile.saveSync();
     console.log('Barrel file generated successfully.');
   }
   ```
   </details>
