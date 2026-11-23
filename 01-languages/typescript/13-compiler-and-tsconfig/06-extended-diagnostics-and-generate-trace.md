# Extended Diagnostics and Generate Trace in TypeScript

> Master profiling and debugging the TypeScript compiler: `--extendedDiagnostics`, `--explainFiles`, `--generateTrace`, `@typescript/analyze-trace`, identifying type-checker bottlenecks, union explosions, and slow types.

| Metadata | Details |
| :--- | :--- |
| **Category** | Compiler & Configuration |
| **Difficulty** | Advanced |
| **Prerequisites** | [13-compiler-and-tsconfig/00-compiler-options.md](./00-compiler-options.md), [10-advanced-types/00-conditional-types.md](../10-advanced-types/00-conditional-types.md) |
| **TypeScript Baseline** | TS 4.1+ (`--generateTrace`), TS 5.0+ |
| **Tested tsconfig** | Profiled against TS 5.4+ with Chromium Perfetto trace viewers |

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
As TypeScript codebases expand and adopt deep type-level libraries (e.g. Prisma, tRPC, Zod, TanStack Query), compilation and editor autocomplete times can degrade from milliseconds to several seconds or minutes. Without profiling tools, developers have no way of knowing which file, type alias, or generic recursive union is causing the slowdown.

TypeScript provides built-in profiling diagnostics:
- `--extendedDiagnostics`: High-level summary of memory usage, check time, parse time, and symbol counts.
- `--explainFiles`: Explains exactly *why* a particular file was included in the compilation.
- `--generateTrace <dir>`: Generates detailed Chromium-compatible trace files (`trace.json`) that can be visualized in Perfetto or analyzed with `@typescript/analyze-trace` to pinpoint exact type-checking bottlenecks down to specific lines of code.

---

## 2. Core Concepts
- **`--extendedDiagnostics` Output**:
  - `Files`: Total source files parsed.
  - `Lines of Library / Program code`: Scale of code analyzed.
  - `Symbols`: Total identifier symbols allocated in the symbol table.
  - `Types`: Total unique type instances constructed by the Type Checker.
  - `Check time`: Milliseconds spent exclusively inside the type checker (`checker.ts`).
  - `Total time`: End-to-end process duration.
- **`--explainFiles`**: Emits the dependency justification chain explaining why every single `.ts` and `.d.ts` file in `node_modules` entered the program.
- **`--generateTrace`**: Emits `trace.json` and `types.json`. Every type evaluation, file parse, and check operation is recorded as an event with nanosecond timestamps.
- **`@typescript/analyze-trace`**: Official CLI tool that parses trace directories and highlights the "hot spots" (types taking $>500\text{ms}$ to check, deep recursion trees).
- **Union Explosions**: When combining multi-member unions (e.g. $A \times B \times C$), the compiler creates Cartesian products ($20 \times 20 \times 20 = 8000$ types) that overwhelm the type cache.

---

## 3. How It Works Internally

```
       tsc --generateTrace ./perf-trace
                      |
                      v
       +-------------------------------+
       | Chromium Trace Event Protocol |
       | - Type construction spans     |
       | - Check source file spans     |
       | - Type serialization IDs      |
       +--------------+----------------+
                      |
        Generates ./perf-trace/
        ├── trace.json  (Event timing timeline)
        └── types.json  (Type IDs to string representations)
                      |
                      v
       npx @typescript/analyze-trace ./perf-trace
                      |
                      v
   Hot spots identified:
   - Line 142 in src/schema.ts: Deep recursive type took 2.4s!
```

---

## 4. Syntax & API Reference

```bash
# 1. High-level diagnostic summary
tsc --noEmit --extendedDiagnostics

# 2. Investigating file inclusion reasons
tsc --noEmit --explainFiles

# 3. Generating full execution trace
tsc --noEmit --generateTrace ./trace-output

# 4. Analyzing the generated trace
npx @typescript/analyze-trace ./trace-output
```

---

## 5. Practical Examples

### Example 1: Sample Extended Diagnostics Output
```text
Files:                         842
Lines of Library code:      34,120
Lines of Program code:      42,890
Lines of Output code:            0
Identifiers:                89,400
Symbols:                    72,310
Types:                     142,500
Memory used:              284,100K
Assignability cache size:   42,100
Identity cache size:         3,200
Subtype cache size:          1,800
Strict subtype cache size:     950
I/O Read time:               0.08s
Parse time:                  0.42s
ResolveModule time:          0.12s
ResolveTypeReference time:   0.02s
Program time:                0.68s
Bind time:                   0.22s
Check time:                  1.84s
Total time:                  2.74s
```

### Example 2: Analyzing Hotspots with `@typescript/analyze-trace`
```bash
$ npx @typescript/analyze-trace ./trace-output

Hot Spots:
  - 1450ms checking src/api/router.ts:45
    Type ID: 12048
    Representation: AppRouter<Context, { query: ... }, { mutation: ... }>

  - 820ms checking src/db/schema.ts:88
    Type ID: 8940
    Representation: InferModel<typeof usersTable>
```

### Example 3: Edge Case - Identifying Union Cartesian Explosion
```typescript
// SENSITIVE TYPE: Unconstrained generic cartesian product
type Color = 'red' | 'green' | 'blue' | 'yellow' | 'purple';
type Size = 'xs' | 'sm' | 'md' | 'lg' | 'xl' | '2xl';
type Weight = 'light' | 'normal' | 'bold' | 'bolder';
type Variant = 'solid' | 'outline' | 'ghost' | 'link';

// 5 * 6 * 4 * 4 = 480 permutations
type ButtonClass = `${Color}-${Size}-${Weight}-${Variant}`;
// Combining this into a mapped type multiplies exponentially and stalls tsc!
```

### Example 4: Compiler Failure Case - Type Instantiation Depth Limit
```typescript
type InfiniteTuple<T, Acc extends any[] = []> =
  InfiniteTuple<T, [T, ...Acc]>;

// @ts-expect-error TS2589: Type instantiation is excessively deep and possibly infinite.
type Deep = InfiniteTuple<string>;
```

### Example 5: Idiomatic Trace Generation Script
```json
// package.json scripts:
{
  "scripts": {
    "typecheck": "tsc --noEmit",
    "typecheck:diag": "tsc --noEmit --extendedDiagnostics",
    "typecheck:trace": "tsc --noEmit --generateTrace ./trace && npx @typescript/analyze-trace ./trace"
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **Trace Files Can Be Massive**: In large projects, `trace.json` can exceed 1GB in size. Opening it directly in Chrome DevTools (`chrome://tracing` or `ui.perfetto.dev`) may crash the browser tab. Use `@typescript/analyze-trace` first to filter hot spots.
2. **Assignability Cache Thrashing**: If `Assignability cache size` in `--extendedDiagnostics` exceeds 200,000, your codebase likely has polymorphic union checks that cannot be cached, multiplying check time exponentially.
3. **Accidental Duplicate Library Inclusions**: `--explainFiles` often reveals that two different versions of `@types/node` or `react` were loaded from different nested `node_modules` paths, doubling the symbol count.
4. **Anonymous Type Stringification**: In `types.json`, anonymous mapped types display as `{ [x: string]: any }`, making them harder to identify without cross-referencing line numbers from `trace.json`.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Consequence | Correction |
| :--- | :--- | :--- |
| Guessing which types are slow | Wasting hours refactoring the wrong code | Run `--generateTrace` and analyze with CLI |
| Recursive template literals with multiple unions | Cartesian explosion; compiler hangs | Break into intermediate interfaces |
| Multiple versions of `@types/react` loaded | Slow build; phantom type mismatch errors | Deduplicate dependencies via package manager |
| Overly broad `include: ["**/*"]` | Parses build outputs and test fixtures | Scope `include` to `["src/**/*"]` |
| Deep conditional recursion in tail position | Hits 1,000 iteration compiler limit | Use mapped types or limit recursion depth |

---

## 8. Performance & Type-Checker Complexity
- In `--extendedDiagnostics`, the most critical metric is **Check time**. If Check time accounts for $>80\%$ of Total time, optimization should focus on simplifying types, not optimizing I/O.
- If `Types` exceeds 500,000, the type checker is instantiating excessive anonymous types. Replacing mapped conditional types with named interfaces or explicit generics frequently drops type counts by 70%.

---

## 9. Compiler Internals
In TypeScript's `src/compiler/tracing.ts`:
- The compiler wraps core functions (`checkSourceElement`, `isTypeAssignableTo`, `createTypeNode`) in performance trace scopes using `tracing?.push(phase, name, args)`.
- Tracing uses high-resolution monotonic timestamps (`performance.now()` / `process.hrtime()`).
- Data is written to `trace.json` in Chromium Trace Event Format (`Event Object` format with `ph: "X"` complete duration events).

---

## 10. Best Practices & Design Guidelines
- Add `typecheck:diag` to your team's toolkit to monitor compiler health.
- Run `--generateTrace` whenever TypeScript compilation time increases by $>20\%$ between PRs.
- Use `ui.perfetto.dev` to inspect flame graphs of slow type evaluations.
- Use `--explainFiles` to identify unwanted packages pulled into the build.
- Prefer interfaces over large intersections (`&`) to enable the compiler's cached type-lookup optimizations.

---

## 11. Real-World Case Study
A financial SaaS monorepo experienced a type-checking regression where `tsc --noEmit` went from 8 seconds to 55 seconds after merging a database schema update. Developers assumed it was a TypeScript version bug.

**Investigation with `--generateTrace`:**
1. Ran `tsc --noEmit --generateTrace ./trace`.
2. Ran `npx @typescript/analyze-trace ./trace`.
3. The tool immediately flagged a single line in `src/db/filters.ts`:
   ```typescript
   // A generic SQL filter helper was performing a 4-way distributive conditional type
   // across 80 database table models:
   type TableFilters<T> = ...
   ```
4. Checking that single type required 42 seconds and generated 820,000 intermediate types!
5. The team refactored the type to use a non-distributive indexed lookup.
Compilation time dropped back to 7.8 seconds immediately.

---

## 12. Interview Questions

<details>
<summary>1. What is the difference between `--extendedDiagnostics` and `--generateTrace`?</summary>

`--extendedDiagnostics` prints a high-level summary of compiler performance (total check time, symbol counts, memory used) directly in the terminal. `--generateTrace` outputs microsecond-level execution traces (`trace.json` and `types.json`) recording every individual type evaluation and file parse for visualization in Perfetto or automated analysis.
</details>

<details>
<summary>2. How can you identify which line of code is causing slow type checking?</summary>

By generating a trace (`tsc --generateTrace ./dir`) and analyzing it with `@typescript/analyze-trace ./dir`, which automatically flags hot spots with exact file names, line numbers, and duration.
</details>

<details>
<summary>3. What does `--explainFiles` do?</summary>

It lists every file in the compilation program and prints an explanatory trace indicating why the file was included (e.g. matched by `include`, imported by another file, or loaded as a global type reference).
</details>

<details>
<summary>4. What causes a "Cartesian explosion" in the TypeScript type checker?</summary>

Combining multiple large union types inside template literal types or distributive conditional types (e.g. `${UnionA}_${UnionB}_${UnionC}`). The compiler must instantiate every permutation, which can generate hundreds of thousands of types and freeze the compiler.
</details>

<details>
<summary>5. Why do interfaces check faster than type intersections (`&`)?</summary>

Interfaces create named, flat property tables that the compiler can easily check and cache by identity. Intersections (`TypeA & TypeB`) require computing recursive property conflicts and assignability across every member on demand without caching.
</details>

<details>
<summary>6. How do you view `trace.json` visually?</summary>

Open Google Chrome, navigate to `chrome://tracing` or visit `https://ui.perfetto.dev/`, and drag and drop the `trace.json` file to inspect the interactive flame graph.
</details>

<details>
<summary>7. What does the "Check time" metric in `--extendedDiagnostics` measure?</summary>

The time spent strictly by the Type Checker (`checker.ts`) verifying types, resolving assignability, and evaluating conditionals, excluding file I/O, AST parsing, and code emit.
</details>

<details>
<summary>8. How can duplicate `@types` packages be diagnosed and eliminated?</summary>

Run `tsc --explainFiles | grep @types/`. If duplicate versions appear from nested paths, use your package manager's `resolutions` or `overrides` field (npm/pnpm/yarn) to deduplicate them to a single version.
</details>

---

## 13. Exercises & Challenges

### Easy
1. **Run Diagnostics**: Write the command to run `tsc` with extended diagnostics without emitting files.
   <details>
   <summary>Solution</summary>

   ```bash
   tsc --noEmit --extendedDiagnostics
   ```
   </details>

2. **File Justification**: Write the command to verify why `@types/lodash` is being loaded into your project.
   <details>
   <summary>Solution</summary>

   ```bash
   tsc --noEmit --explainFiles | grep lodash
   ```
   </details>

### Medium
3. **Trace Generation Script**: Add a custom npm script in `package.json` that outputs performance traces into `.tmp/trace` and opens analysis.
   <details>
   <summary>Solution</summary>

   ```json
   {
     "scripts": {
       "trace:check": "tsc --noEmit --generateTrace .tmp/trace && npx @typescript/analyze-trace .tmp/trace"
     }
   }
   ```
   </details>

4. **Cartesian Explosion Refactoring**: Refactor a slow template literal type generating 500 permutations into a structured object configuration.
   <details>
   <summary>Solution</summary>

   ```typescript
   // SLOW:
   // type Permutation = `${'a'|'b'|'c'|'d'|'e'}-${'1'|'2'|'3'|'4'|'5'}-${'x'|'y'|'z'}`;

   // FAST:
   interface ElementConfig {
     prefix: 'a' | 'b' | 'c' | 'd' | 'e';
     level: 1 | 2 | 3 | 4 | 5;
     axis: 'x' | 'y' | 'z';
   }
   ```
   </details>

### Hard
5. **Automated CI Compiler Performance Regression Gate**: Write a Node.js script that runs `tsc --extendedDiagnostics`, parses the check time from stdout, and fails the build if check time exceeds 10 seconds.
   <details>
   <summary>Solution</summary>

   ```typescript
   import { execSync } from 'child_process';

   export function assertCompilerPerformance(maxCheckSeconds = 10): void {
     try {
       const stdout = execSync('npx tsc --noEmit --extendedDiagnostics', { encoding: 'utf8' });
       const match = stdout.match(/Check time:\s+([\d.]+)s/);

       if (match) {
         const checkTime = parseFloat(match[1]);
         console.log(`TypeScript Check Time: ${checkTime}s`);
         if (checkTime > maxCheckSeconds) {
           console.error(`PERFORMANCE REGRESSION: Check time ${checkTime}s exceeds limit of ${maxCheckSeconds}s!`);
           process.exit(1);
         }
       }
     } catch (err) {
       console.error('Failed to run diagnostics:', err);
       process.exit(1);
     }
   }
   ```
   </details>

6. **Automated Trace Parser for Slowest Types**: Write a Node.js script that reads a `types.json` file generated by `--generateTrace`, extracts all types whose string representation exceeds 1,000 characters, and prints them as candidates for union simplification.
   <details>
   <summary>Solution</summary>

   ```typescript
   import * as fs from 'fs';

   interface TraceTypeEntry {
     id: number;
     display?: string;
   }

   export function findComplexTypes(typesJsonPath: string, maxLength = 1000): void {
     const data: TraceTypeEntry[] = JSON.parse(fs.readFileSync(typesJsonPath, 'utf8'));
     const suspicious = data.filter(t => t.display && t.display.length > maxLength);

     console.log(`Found ${suspicious.length} complex types exceeding ${maxLength} characters:`);
     suspicious.slice(0, 10).forEach(t => {
       console.log(`- Type ID ${t.id} (${t.display?.length} chars): ${t.display?.substring(0, 120)}...`);
     });
   }
   ```
   </details>
