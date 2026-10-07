# Compiler Architecture

Knowing roughly how the TypeScript compiler is organized makes many things click: why type-checking is slow while stripping types is fast, why the editor can show types instantly, why `isolatedModules` exists, and why some errors appear in one tool and not another. This note is a map of the pipeline, not a guide to compiler internals. You do not need it to use TypeScript, but it helps when diagnosing performance, building tooling, or reading the compiler's API.

**Prerequisites:**
- [Type erasure and runtime](./00-type-erasure-and-runtime.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
- [Assignability and subtyping](./01-assignability-and-subtyping.md)

---

## The pipeline

```text
 source text (.ts)
        |
        v
  +-----------+      tokens
  |  Scanner  | ---------------+
  +-----------+                |
                               v
                        +-------------+     AST (SourceFile)
                        |   Parser    | -----------------------+
                        +-------------+                        |
                                                               v
                                                        +-------------+     symbols, scopes,
                                                        |   Binder    | --- flow graph
                                                        +-------------+            |
                                                                                   v
                                                                          +----------------+    types,
                                                                          |    Checker     | -- diagnostics
                                                                          +----------------+        |
                                                                                                    v
                                      .js, .d.ts, .map  <--  Emitter  <--  Transformers  <---------+
```

Each stage in turn:

1. **Scanner (lexer):** turns text into tokens (keywords, identifiers, punctuation, literals).
2. **Parser:** builds an **abstract syntax tree** (AST), one `SourceFile` per file. It produces syntax errors but knows nothing about types.
3. **Binder:** walks the AST and creates **symbols** (a name together with all its declarations) and scopes. This is where **declaration merging** happens (interfaces with the same name, namespace plus class), because merged declarations share a symbol ([declaration merging](../09-declaration-files/03-declaration-merging.md)). It also builds the **control-flow graph** used for narrowing.
4. **Checker:** the biggest part. It computes the **type** of every expression and declaration, checks assignability, resolves generics, and reports semantic errors. Most of the compiler's time and code live here.
5. **Transformers:** rewrite the AST for output: remove types, downlevel syntax to the target, convert modules to the target format, lower decorators and class fields.
6. **Emitter:** prints the transformed AST as `.js`, plus `.d.ts` declarations and source maps.

Syntax-only tools can stop after step 2 (or 5). That is why they are fast.

## The `Program`

A **Program** is the unit that ties things together: the set of root files from `tsconfig`, the options, every file reached by imports (including `lib.*.d.ts` and `@types`), and the results of **module resolution**. `tsc` builds one Program, asks the checker for diagnostics, then emits.

```ts
import ts from "typescript";

const program = ts.createProgram(["src/index.ts"], {
  strict: true,
  noEmit: true,
});

const diagnostics = ts.getPreEmitDiagnostics(program);
for (const d of diagnostics) {
  console.log(ts.flattenDiagnosticMessageText(d.messageText, "\n"));
}
```

This is the same API editors and linters use. The **type checker** is available from it:

```ts
const checker = program.getTypeChecker();
const source = program.getSourceFile("src/index.ts")!;

ts.forEachChild(source, function visit(node) {
  if (ts.isVariableDeclaration(node)) {
    const type = checker.getTypeAtLocation(node);
    console.log(node.name.getText(), ":", checker.typeToString(type));
  }
  ts.forEachChild(node, visit);
});
```

Tools such as `typescript-eslint`, documentation generators, and codemod tools are built on this API. The [TypeScript AST Viewer](https://ts-ast-viewer.com) site shows the AST for a snippet, which helps when writing such tools.

## How types are represented

Inside the checker, a type is an object with **flags** that say what it is (string, number, literal, object, union, intersection, type parameter, conditional, and so on) plus kind-specific data.

Things worth knowing:

- **Interning:** many types are created once and reused. For example, a union of the same members is the same object, with members sorted by internal type id. This makes identity comparison cheap and union operations fast, but it is also why large unions are costly to build.
- **Laziness:** types of members, base types, and signatures are often resolved **on demand**, and cached. This lets recursive types exist without infinite expansion, until something forces them.
- **Instantiation:** a generic like `Array<T>` is instantiated per use (`Array<string>`), with caching. Heavy use of generics, conditional types, and mapped types multiplies instantiations, which is the main cost of "type-level programming" ([type-level programming](../10-advanced-types/08-type-level-programming.md)).
- **Deferred conditional types:** a conditional with an unresolved type parameter stays as a conditional type object until the parameter is known ([conditional types](../10-advanced-types/00-conditional-types.md)).
- **Variance is measured and cached** per generic type ([variance](./02-variance.md)).
- **Limits** exist to protect against runaway work (instantiation depth, union size). Hitting them is what produces "excessively deep" and "union too complex" errors.

## `tsc` vs the language service

| | `tsc` | Language service (`tsserver`, used by editors) |
|---|---|---|
| Purpose | batch check and emit | interactive features: completions, hover, go to definition, quick fixes, rename |
| Lifetime | one run | long-lived process |
| Work | whole Program | checks what you are looking at, on demand |
| Updates | rebuilds (or incremental via `tsbuildinfo`) | **incremental**: re-parses only changed files and reuses the rest |

This is why your editor can show a type instantly while a full `tsc` run takes seconds: the language service keeps the Program in memory and updates it as you type, and it checks lazily. It is also why editor and CLI can disagree: they may use different TypeScript versions or different tsconfig files ([compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)).

Editor features such as completions and refactors are built on the same checker and AST, not a separate analysis.

## Single-file transpilation

Because the **parser and transformers** do not need type information for most syntax, a file can be converted to JavaScript **alone**:

```ts
const out = ts.transpileModule("const x: number = 1;", {
  compilerOptions: { module: ts.ModuleKind.ESNext },
});
// out.outputText: "const x = 1;"
```

This is what fast tools do: esbuild, SWC, Babel's TypeScript preset, and runtimes that strip types all convert without running the checker. It is also why the `isolatedModules` and `verbatimModuleSyntax` options exist: they make sure your code uses only features whose emit can be decided from a single file (for example, you must mark type-only re-exports with `export type`, since a lone file cannot tell whether `export { X } from "./a"` re-exports a type).

The trade-off: these tools **do not type-check**. A common setup is a fast transpiler for building and `tsc --noEmit` in CI and the editor for checking ([build and bundling](../21-production-tooling/01-build-and-bundling.md)).

## Why type-checking is the expensive part

- Checking needs the **whole Program**: resolving imports, loading `lib` and `@types`, relating types across files.
- Work scales with **types and relationships**, not lines of code: large unions, deep generics, intersections, and conditional types dominate.
- `skipLibCheck` skips re-checking `.d.ts` files, which often saves a surprising amount.
- Incremental builds and project references reduce repeated work ([incremental builds](../13-compiler-and-tsconfig/04-project-references-and-incremental-builds.md)).

To see where time goes:

```bash
tsc --extendedDiagnostics          # counts and phase timings (parse, bind, check, emit)
tsc --generateTrace ./trace        # produces a trace you can open in a performance viewer
tsc --listFiles                    # how many files are in the Program
```

The phase timings tell you whether the cost is in loading files (too many included), checking (types too heavy), or emit. See [type-checking performance](../22-performance/00-type-checking-performance.md).

## Plugins and extensions

- **Language service plugins** (configured under `compilerOptions.plugins`) can add editor diagnostics and completions, for example for CSS modules or template strings. They affect the editor, not `tsc` output.
- **Transformers** can be applied by some build tools to change emit, but the official `tsc` does not support custom transformers through tsconfig.
- **Declaration emit** is separate from checking in structure: `.d.ts` output reuses the checker's inferred types, which is why explicit return types on exports help ([declaration files](../09-declaration-files/00-declaration-files.md)).

## The compiler is evolving

The TypeScript team has been developing a **native-code port** of the compiler and language service (referred to as TypeScript 7 in their announcements), motivated by speed. Its pipeline follows the same shape described here, but internal APIs and some tooling integration points differ from the JavaScript-based compiler. Check the TypeScript team's current announcements and release notes for status, compatibility, and which `tsc` your project is using, since this part of the ecosystem changes quickly.

## Important rules and misconceptions

- **"TypeScript compiles to JavaScript by checking types."** Emit does not depend on checking. You can emit with errors (unless `noEmitOnError`), and many tools emit without checking at all.
- **"The editor and `tsc` are the same thing."** They share the compiler but run differently and may use different versions and configs.
- **"Slow builds mean too much code."** Often they mean expensive types or too many included files.
- **"Type errors come from the parser."** Syntax errors come from the parser. Type errors come from the checker.
- **"Transpile-only tools are equivalent to `tsc`."** They produce JavaScript but never report type errors.

## Common mistakes

- Relying on a transpile-only build and never running `tsc --noEmit`.
- Using features that cannot be compiled per-file when the toolchain does per-file transpilation (`const enum` across files, value `namespace` merging) without `isolatedModules` to warn you.
- Blaming the editor for errors that come from a different TypeScript version or tsconfig.
- Optimizing the wrong thing: check `--extendedDiagnostics` before restructuring.

## Debugging

- **"Which TypeScript am I using?"** `npx tsc -v` for the CLI. In the editor, check the selected workspace TypeScript version.
- **"Why is this file included?"** `tsc --explainFiles`.
- **Slow check:** `--extendedDiagnostics`, then `--generateTrace` if the check phase dominates, and look for the files and types with the longest checks.
- **Odd emit:** compile a minimal file and read the output, or use the Playground to see the transformed result at your `target`.
- **Tooling that inspects code:** start with the AST Viewer to see node kinds before writing visitors.

## Quick summary

- Pipeline: scanner (tokens), parser (AST), binder (symbols, scopes, flow graph, declaration merging), checker (types and errors), transformers (strip and downlevel), emitter (`.js`, `.d.ts`, maps).
- A `Program` is the root files plus everything they import, with options and resolution results. The type checker and diagnostics hang off it.
- Types are interned, lazily resolved, instantiated per use, and cached. Generic-heavy and union-heavy code is what makes checking slow.
- The language service keeps a Program in memory and updates it incrementally. `tsc` runs in batch.
- Per-file transpilers skip the checker entirely, which is fast and why they need `isolatedModules`-style restrictions. Always type-check separately.
- Use `--extendedDiagnostics` and `--generateTrace` to measure instead of guessing.

**Next:** [15 Runtime Validation](../15-runtime-validation/README.md)
