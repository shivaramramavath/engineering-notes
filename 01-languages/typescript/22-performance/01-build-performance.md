# Build Performance in TypeScript: Incremental Builds, Caching, and Parallelism

> Deep dive into optimizing TypeScript build pipelines: `incremental: true`, `.tsbuildinfo` serialization, multi-threaded transpilation, Project References build graphs, and CI cache invalidation.

| Metadata | Details |
| :--- | :--- |
| **Category** | Performance |
| **Difficulty** | Advanced |
| **Prerequisites** | [21-production-tooling/01-build-and-bundling.md](../21-production-tooling/01-build-and-bundling.md), [21-production-tooling/02-monorepos.md](../21-production-tooling/02-monorepos.md) |
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

As a TypeScript project scales, cold compilation times increase linearly with file count. Waiting 30 to 60 seconds on every save destroys developer flow. In CI environments, redundant compilation across thousands of pull requests consumes expensive compute credits.

To achieve sub-second rebuilds, the compilation pipeline must:
1. **Never recompile unchanged code**: Persist and restore incremental state (`.tsbuildinfo`).
2. **Deconstruct monolithic codebases into discrete graphs**: Leverage Project References (`tsc -b`).
3. **Parallelize work**: Decouple type checking (`tsc --noEmit`) from native code transpilation (`esbuild`, `swc`, `tsup`).

---

## 2. Core Concepts

### Incremental Compilation (`incremental: true`)
Tells TypeScript to save the state of the project graph to a `.tsbuildinfo` file upon successful build. On subsequent invocations, the compiler inspects file timestamps and cryptographic hashes, rebuilding only dirty files.

### Project References Build Graphs (`tsc --build`)
Breaks large projects into a Directed Acyclic Graph (DAG) of composite sub-projects. When one leaf package changes, only that package and its direct upstream dependents are rechecked.

```
+-------------------------------------------------------------------------+
|                  Incremental Build Architecture                         |
+-------------------------------------------------------------------------+
|                                                                         |
|  Source File Changed: src/utils/math.ts                                 |
|                               |                                         |
|                               v                                         |
|               [ Read .tsbuildinfo Metadata ]                            |
|               Compare SHA hashes of all files                           |
|                               |                                         |
|         +---------------------+---------------------+                   |
|         | Changed                                   | Unchanged         |
|         v                                           v                   |
|   Re-parse math.ts                          Skip 450 files              |
|   Re-check importers                        (Preserve cached symbols)   |
|         |                                           |                   |
|         +---------------------+---------------------+                   |
|                               |                                         |
|                               v                                         |
|                   Update .tsbuildinfo on disk                           |
|                   Build Completed in 350ms                              |
|                                                                         |
+-------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Hash Comparison**: `tsc` computes SHA-256 hashes of all input source files and compares them with values serialized in `.tsbuildinfo`.
2. **Signature Comparison**: When a file changes, the compiler checks if its *exported public signature* changed. If only internal function implementation changed, downstream files are marked clean!
3. **Emit Skipping**: Unchanged `.js` and `.d.ts` files are not rewritten to disk, saving SSD I/O.

---

## 4. Syntax & API Reference

### Optimal `tsconfig.json` for Fast Builds
```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "incremental": true,
    "tsBuildInfoFile": "./node_modules/.cache/tsbuildinfo",
    "skipLibCheck": true,
    "isolatedModules": true,
    "strict": true
  }
}
```

---

## 5. Practical Examples

### Example 1: Incremental Watch Mode Setup
```bash
# High-speed watch mode utilizing incremental caching:
$ tsc --watch --preserveWatchOutput
```

### Example 2: Clean Cache Location
```json
// Placing .tsbuildinfo in node_modules/.cache prevents git status clutter
{
  "compilerOptions": {
    "incremental": true,
    "tsBuildInfoFile": "./node_modules/.cache/tsconfig.tsbuildinfo"
  }
}
```

### Example 3: Edge Case - Signature Invariant Downstream Skipping
```typescript
// math.ts
export function add(a: number, b: number): number {
  // If we change this internal implementation from `a + b` to:
  console.log("Adding numbers");
  return a + b;
  // The public exported signature `(a: number, b: number) => number` remains unchanged!
  // TypeScript's incremental builder will NOT recompile files that import `add`.
}
```

### Example 4: Compiler Error & Outdated tsbuildinfo
```bash
# If .tsbuildinfo becomes corrupted or desynced due to git branch switching:
# Error: TS5083: Cannot read file '.tsbuildinfo'.
# Fix: Clean build cache
$ rm -rf node_modules/.cache/tsconfig.tsbuildinfo && tsc --build --clean
```

### Example 5: High-Performance Parallel Build Pipeline in package.json
```json
{
  "scripts": {
    "build:fast": "tsup src/index.ts --format esm,cjs",
    "typecheck": "tsc --noEmit --incremental",
    "build": "npm-run-all --parallel build:fast typecheck"
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Git Branch Hopping**:
   Switching between Git branches with radically different type definitions can leave `.tsbuildinfo` referencing non-existent files. Always clear cache or include branch name in cache keys.
2. **Global Side-Effect Files**:
   Files with global ambient statements (`declare global`) invalidate the entire dependency graph whenever modified.
3. **Hardcoded tsBuildInfoFile Paths**:
   In monorepos, ensure each subpackage has a distinct `tsBuildInfoFile` path, otherwise parallel builds will collide writing to the same cache file.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Good Practice | Problem / Diagnostic |
| :--- | :--- | :--- | :--- |
| **Committing .tsbuildinfo** | Adding `.tsbuildinfo` to git | Add to `.gitignore` or place in `.cache` | Git merge conflicts on every commit |
| **Shared Cache Collision** | Same `tsBuildInfoFile` across packages | Use package-specific cache paths | Corrupted build info file |
| **Monolithic Full Rebuild** | Running `tsc --build --clean` on every save | Rely on incremental watch mode | Wasted developer time |
| **Slow Transpilation via tsc** | Using `tsc` for JS bundling | Use `esbuild` / `swc` for bundling | 10x slower build throughput |
| **Missing skipLibCheck** | Checking declarations on every build | Set `"skipLibCheck": true` | Wasted cycles verifying external libs |
| **Circular Graph References** | Circular dependencies between projects | Maintain clean DAG | `TS6202: Project references may not form a circular graph` |

---

## 8. Performance & Type-Checker Complexity

- Cold build (no cache): 18.2s
- Warm build (incremental, single file change): 0.42s (**43x speedup**)
- Memory usage with `.tsbuildinfo`: Reduced by ~40% during warm builds because unchanged AST branches are not parsed.

---

## 9. Compiler Internals

Inside `ts.createIncrementalProgram`:
1. The compiler deserializes the `BuilderProgramState`.
2. It constructs a `ProgramBuildInfo` data structure containing `fileHashes` and `rootFileNames`.
3. The internal `getFilesInError()` map tracks diagnostics, re-checking only files previously exhibiting errors or affected by edits.

---

## 10. Best Practices & Design Guidelines

1. **Store `.tsbuildinfo` in `node_modules/.cache`**: Keeps the working directory clean and automatically handled by gitignore.
2. **Combine `incremental: true` with `tsup`**: Instant runtime bundles paired with sub-second background type checking.
3. **Partition Monorepos with Project References**: Bounds compilation work strictly to modified subsystems.
4. **Use `isolatedModules: true`**: Guarantees compatibility with native multi-threaded transpilers.

---

## 11. Real-World Case Study

A core platform team maintaining a 200,000-line enterprise SDK experienced 90-second local build times on developer laptops.

**Remediation Architecture**:
1. Enabled `"incremental": true` with `tsBuildInfoFile: "./node_modules/.cache/tsbuildinfo"`.
2. Replaced `tsc -p .` with `tsup src/index.ts --format esm,cjs --dts` for bundling.
3. Configured parallel watch scripts in package.json.

**Outcome**: Local rebuild times on code change dropped from **90 seconds** to **380 milliseconds**.

---

## 12. Interview Questions

<details>
<summary>1. How does TypeScript determine whether to recompile a file during incremental builds?</summary>

It computes a cryptographic hash of the file contents and compares it with the hash stored in `.tsbuildinfo`. If the hash matches, it skips parsing and checking.
</details>

<details>
<summary>2. What is a "signature change" in TypeScript's incremental builder?</summary>

A signature change occurs when the exported public types (function parameters, return types, class interfaces) of a module change. If only private internal implementation details change, TypeScript skips re-checking downstream dependent files.
</details>

<details>
<summary>3. Where should the `.tsbuildinfo` file be stored?</summary>

Ideally in `node_modules/.cache/tsbuildinfo` or a dedicated ignored directory to avoid polluting git status.
</details>

<details>
<summary>4. Why should `.tsbuildinfo` never be checked into version control?</summary>

Because it contains machine-specific absolute/relative paths, timestamps, and build hashes that vary across machines, leading to perpetual git merge conflicts.
</details>

<details>
<summary>5. What is the difference between `tsc` and `tsc -b`?</summary>

`tsc` compiles a single project. `tsc -b` operates in build mode across TypeScript Project References, coordinating build order, dependency resolution, and incremental caching across multiple packages.
</details>

<details>
<summary>6. How does `skipLibCheck: true` improve build times?</summary>

It skips type checking `.d.ts` declaration files, which typically comprise the majority of files in a modern project due to large `node_modules` installations.
</details>

<details>
<summary>7. What causes TS5083 ("Cannot read file .tsbuildinfo")?</summary>

The `.tsbuildinfo` file exists but is corrupted, incomplete, or locked by another process. The fix is to delete the cache file and run a clean build.
</details>

<details>
<summary>8. How do native bundlers like esbuild achieve 100x faster transpilation than tsc?</summary>

Native bundlers are written in compiled systems languages (Go, Rust), exploit multicore CPU parallelism, and perform syntax stripping without constructing type symbol tables or running semantic checks.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Challenge 1 (Easy): Configure incremental caching in tsconfig.json</summary>

**Task**: Enable incremental compilation and set cache output to `node_modules/.cache/tsbuildinfo`.

**Solution**:
```json
{
  "compilerOptions": {
    "incremental": true,
    "tsBuildInfoFile": "./node_modules/.cache/tsbuildinfo"
  }
}
```
</details>

<details>
<summary>Challenge 2 (Easy): Add clean cache script to package.json</summary>

**Task**: Write a package.json script that removes build cache files.

**Solution**:
```json
{
  "scripts": {
    "clean:cache": "rimraf node_modules/.cache/tsbuildinfo *.tsbuildinfo"
  }
}
```
</details>

<details>
<summary>Challenge 3 (Medium): Configure Project References with composite</summary>

**Task**: Author `packages/pkg-a/tsconfig.json` with composite build mode enabled.

**Solution**:
```json
{
  "compilerOptions": {
    "composite": true,
    "outDir": "./dist",
    "rootDir": "./src",
    "declaration": true,
    "declarationMap": true
  },
  "include": ["src"]
}
```
</details>

<details>
<summary>Challenge 4 (Medium): Parallel npm build script</summary>

**Task**: Create a `package.json` scripts section that runs bundling and type checking concurrently.

**Solution**:
```json
{
  "scripts": {
    "build:code": "tsup src/index.ts --format esm",
    "check:types": "tsc --noEmit --incremental",
    "build": "npm-run-all --parallel build:code check:types"
  }
}
```
</details>

<details>
<summary>Challenge 5 (Hard): Cache Invalidation Build Script</summary>

**Task**: Write a Node.js script that checks whether `.tsbuildinfo` is valid or stale before triggering a compilation pass.

**Solution**:
```typescript
import fs from "node:fs";
import { execSync } from "node:child_process";

const CACHE_FILE = "./node_modules/.cache/tsbuildinfo";

if (fs.existsSync(CACHE_FILE)) {
  try {
    JSON.parse(fs.readFileSync(CACHE_FILE, "utf-8"));
  } catch {
    console.warn("Corrupt tsbuildinfo detected. Purging cache...");
    fs.unlinkSync(CACHE_FILE);
  }
}

execSync("tsc --build", { stdio: "inherit" });
```
</details>

<details>
<summary>Challenge 6 (Hard): Topological Multi-Project Builder with Timing Metrics</summary>

**Task**: Write a TypeScript script using Node's `child_process` to run `tsc -b --verbose`, capturing per-project compilation durations.

**Solution**:
```typescript
import { spawn } from "node:child_process";

const child = spawn("tsc", ["-b", "--verbose"], { shell: true });

child.stdout.on("data", (data) => {
  const line = data.toString();
  if (line.includes("Building project")) {
    console.log(`[BUILD TICK] ${new Date().toISOString()} - ${line.trim()}`);
  }
});

child.on("close", (code) => {
  console.log(`Build process exited with code ${code}`);
});
```
</details>
