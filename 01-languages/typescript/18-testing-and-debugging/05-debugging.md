# Debugging TypeScript: Source Maps, VS Code Launch, Node Inspector, and Chrome DevTools

> Modern debugging workflows for TypeScript: source maps (`sourceRoot`, `inlineSources`), launch configurations, Node.js `--inspect` and `--inspect-brk`, Chrome DevTools, and heap profiling.

| Metadata | Details |
| :--- | :--- |
| **Category** | Testing and Debugging |
| **Difficulty** | Intermediate |
| **Prerequisites** | [13-compiler-and-tsconfig/00-compiler-options.md](../13-compiler-and-tsconfig/00-compiler-options.md), [13-compiler-and-tsconfig/02-target-module-and-lib.md](../13-compiler-and-tsconfig/02-target-module-and-lib.md) |
| **TypeScript Baseline** | TS 4.5+, TS 5.0+ |
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

Because browsers and the Node.js runtime execute JavaScript rather than raw TypeScript, debugging TypeScript requires mapping runtime JavaScript execution back to the original TypeScript source lines. Without proper source maps and debugger configurations, developers are forced into frustrating "blind debugging" practices:
- Setting breakpoints in compiled, minified, or bundled `.js` files with mangled variable names.
- Littering code with ad-hoc `console.log()` statements that pollute git commits.
- Misinterpreting asynchronous call stacks when exceptions throw.

Modern TypeScript debugging bridges runtime execution to TypeScript sources using **Source Maps (V3)**, **VS Code Debugger Launch Configurations (`.vscode/launch.json`)**, and the **Node.js Chrome DevTools Protocol Inspector**.

```
+-------------------+      Emits .js + .map      +-------------------+
|  source/app.ts    |  ----------------------->  |    dist/app.js    |
| (Breakpoint at L5)|                            | (Executes on V8)  |
+-------------------+                            +-------------------+
          ^                                                |
          |           Source Map Mapping (VLQ)             |
          +------------------------------------------------+
                                V8 Inspector
```

---

## 2. Core Concepts

### 1. Source Maps V3 Specification
A source map (`.js.map`) is a JSON file conforming to the Source Map V3 specification:
```json
{
  "version": 3,
  "file": "app.js",
  "sourceRoot": "",
  "sources": ["app.ts"],
  "names": ["myVar", "calculate"],
  "mappings": "AAAA,IAAMA,QAAQ,SAAd..."
}
```
- `mappings`: Variable-Length Quantity (VLQ) base64-encoded strings mapping emitted bytecode lines/columns to original TypeScript source lines/columns.

### 2. VS Code Launch Configuration (`.vscode/launch.json`)
Allows one-click debugging with step-through execution, call stack inspection, and variable watch expressions:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "node",
      "request": "launch",
      "name": "Debug Current TS File",
      "runtimeExecutable": "tsx",
      "args": ["${file}"],
      "sourceMaps": true
    }
  ]
}
```

### 3. Node.js Inspector CLI
Node supports remote debugging out of the box:
- `node --inspect index.js`: Starts the V8 debugging WebSocket server on port 9229.
- `node --inspect-brk index.js`: Starts the inspector and immediately pauses at the very first line of code.

---

## 3. How It Works Internally

When Node.js encounters an unhandled exception or hits a `debugger;` statement:
1. V8 generates a runtime stack trace pointing to `dist/app.js:42:15`.
2. Node looks for the source map comment at the end of the file:
   `//# sourceMappingURL=app.js.map`
3. Node parses the VLQ mappings in `app.js.map` to decode the original file `src/app.ts` and line `15:8`.
4. Node translates the stack trace and highlights the breakpoint directly in the TypeScript IDE.

---

## 4. Syntax & API Reference

### Key tsconfig.json Debugging Options
```json
{
  "compilerOptions": {
    "sourceMap": true,             // Generates external .js.map files
    "inlineSourceMap": false,       // Inlines base64 source map in .js
    "inlineSources": true,          // Embeds original .ts source inside the map
    "declarationMap": true          // Maps .d.ts files to .ts for Go-to-Definition
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Node.js TSX Direct Debugging in VS Code
```json
{
  "name": "Launch Server via TSX",
  "type": "node",
  "request": "launch",
  "program": "${workspaceFolder}/src/server.ts",
  "runtimeExecutable": "node",
  "runtimeArgs": ["--import", "tsx"],
  "skipFiles": ["<node_internals>/**", "**/node_modules/**"],
  "console": "integratedTerminal"
}
```

### Example 2: Real-World Remote Docker Container Debugging
Debugging a TypeScript application running inside a Docker container:
```json
{
  "name": "Attach to Docker Node",
  "type": "node",
  "request": "attach",
  "port": 9229,
  "address": "localhost",
  "localRoot": "${workspaceFolder}/src",
  "remoteRoot": "/usr/src/app/src",
  "sourceMaps": true
}
```

### Example 3: Edge Case - Inspecting Memory Leaks with Heap Snapshots
```typescript
import v8 from 'node:v8';
import fs from 'node:fs';

export function dumpHeapSnapshot(filename: string): void {
  const snapshotStream = v8.getHeapSnapshot();
  const fileStream = fs.createWriteStream(filename);
  snapshotStream.pipe(fileStream);
  console.log(`Heap snapshot written to ${filename}`);
}
```

### Example 4: Failure Case - Missing Source Maps Causing Unmapped Stacks
```typescript
// Without "sourceMap": true in tsconfig.json:
// Error: Invalid operation
//     at calculate (E:\app\dist\bundle.js:1420:12)  <-- Useless minified location!
//
// With "sourceMap": true:
// Error: Invalid operation
//     at calculate (E:\app\src\math\calculate.ts:24:5) <-- Exact TypeScript line!
```

### Example 5: Idiomatic Conditional Debug Logger with Namespace
```typescript
import { debuglog } from 'node:util';

const debug = debuglog('app:billing');

export function chargeCustomer(userId: string, amount: number): void {
  debug('Initiating charge for %s: $%d', userId, amount);
  // Executes only when NODE_DEBUG=app:billing is set in environment!
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`sourceRoot` Path Mismatch in Docker**:
   When debugging in Docker, if `localRoot` and `remoteRoot` in `launch.json` do not map 1:1, VS Code will display gray, unverified breakpoints ("Breakpoint set but not yet bound").
2. **Bundler Minification Discrepancies**:
   Aggressive esbuild or Terser minification with variable mangling can make it impossible for debuggers to evaluate variable names in the Watch pane. Set `minify: false` in development debug builds.
3. **Async Stack Traces**:
   Node.js 14+ supports async stack traces natively. However, unhandled promise rejections inside detached callbacks can still produce truncated stacks unless `--trace-warnings` or `--unhandled-rejections=strict` is supplied.
4. **`declarationMap` Importance in Monorepos**:
   Without `"declarationMap": true`, using "Go to Definition" in VS Code jumps to compiled `.d.ts` declaration files instead of the original source `.ts` file.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Code | Good Code | Compiler / Runtime Impact |
| :--- | :--- | :--- | :--- |
| **`console.log` Debugging** | 50 `console.log('here 1')` in code | VS Code breakpoints or `node:util.debuglog` | Commits clutter; slows down production performance |
| **Missing Declaration Maps** | `"declarationMap": false` in monorepo | `"declarationMap": true` | "Go to Definition" jumps to unreadable `.d.ts` |
| **Hardcoding Breakpoint `debugger;`** | Leaving `debugger;` in production code | IDE conditional breakpoints | Halts entire production process in browser DevTools |
| **Exposing Port 9229 to Public Internet** | `node --inspect=0.0.0.0` in production | Localhost binding or SSH tunnel | Remote Code Execution (RCE) vulnerability |
| **Missing `skipFiles`** | Stepping into Node runtime internals | `"skipFiles": ["<node_internals>/**"]` | Step-into enters thousands of lines of V8 code |
| **Mismatched Docker Paths** | Missing `remoteRoot` mapping | Explicit `localRoot` / `remoteRoot` | Breakpoints remain unverified and never hit |

---

## 8. Performance & Type-Checker Complexity

- **Source Map Build Overhead**: Generating external source maps (`"sourceMap": true`) increases build times by ~15-20%. In production, generate separate `.map` files and upload them to Sentry/Datadog without deploying `.map` files to public CDN servers.
- **Node.js Inspector Overhead**: Running with `--inspect` disables certain V8 inline optimizations, reducing execution speed by ~5-10% while attached.

---

## 9. Compiler Internals

When the TypeScript compiler emits source maps:
The emitter tracks AST node positions (`node.pos` and `node.end`). For each emitted JavaScript token, it writes a tuple into the VLQ mapping buffer representing:
1. Generated column offset.
2. Source file index.
3. Original line offset.
4. Original column offset.
5. Name index (optional).

---

## 10. Best Practices & Design Guidelines

1. **Enable `"declarationMap": true`**: Essential for monorepos so developers navigate to source `.ts` files rather than `.d.ts` files.
2. **Use Conditional Breakpoints**: In VS Code, right-click breakpoints to pause only when conditions hold (e.g. `userId === 'u_42'`).
3. **Keep `debugger;` Out of Production**: Enforce via ESLint rule `no-debugger: error`.
4. **Never Bind Inspector to `0.0.0.0`**: Protect Node debugging ports from remote execution exploits.

---

## 11. Real-World Case Study

### Diagnosing Event Loop Latency Spikes in High-Throughput Service
A WebSocket service experienced intermittent 500ms latency spikes under load. Using Node.js CPU Profiling:

```typescript
import inspector from 'node:inspector';
import fs from 'node:fs';

const session = new inspector.Session();
session.connect();

export function profileFunction(fn: () => void): void {
  session.post('Profiler.enable', () => {
    session.post('Profiler.start', () => {
      fn();
      session.post('Profiler.stop', (err, { profile }) => {
        if (!err) {
          fs.writeFileSync('latency-profile.cpuprofile', JSON.stringify(profile));
          console.log('Profile saved. Load into Chrome DevTools to analyze flame graph.');
        }
      });
    });
  });
}
```
Opening `latency-profile.cpuprofile` in Chrome DevTools revealed an unindexed regular expression (`RegExp`) performing catastrophic backtracking in an auth token validator!

---

## 12. Interview Questions

<details>
<summary>1. How do Source Maps work under the hood in TypeScript?</summary>

Source maps use the Source Map V3 specification. They contain a JSON structure with base64 VLQ (Variable-Length Quantity) encoded mappings. When V8 executes JavaScript, runtime line and column coordinates are looked up in the mapping table, translating them back to the original TypeScript file, line, and column.
</details>

<details>
<summary>2. What is the difference between `sourceMap`, `inlineSourceMap`, and `declarationMap` in `tsconfig.json`?</summary>

- `sourceMap`: Emits separate `.js.map` files alongside emitted `.js` files.
- `inlineSourceMap`: Inlines the source map as a base64 Data URI directly at the bottom of the `.js` file.
- `declarationMap`: Emits `.d.ts.map` files, allowing IDEs to navigate from a type definition back to the original `.ts` source file during "Go to Definition".
</details>

<details>
<summary>3. Why is leaving `debugger;` statements in production dangerous?</summary>

If a client opens Chrome DevTools or a Node.js process is started with `--inspect`, encountering a `debugger;` statement immediately halts JavaScript thread execution, freezing the page or blocking the entire Node event loop.
</details>

<details>
<summary>4. What does the "Breakpoint set but not yet bound" error in VS Code mean?</summary>

It means VS Code was unable to match the breakpoint's file path to the loaded scripts. Common causes include: missing source maps, incorrect `localRoot`/`remoteRoot` path mappings in Docker, or the target code file has not yet been loaded by the Node runtime.
</details>

<details>
<summary>5. How do you debug a TypeScript test running in Vitest using VS Code?</summary>

Add a launch configuration with `runtimeExecutable: "node"`, `runtimeArgs: ["./node_modules/vitest/vitest.mjs", "run", "${file}"]`.
</details>

<details>
<summary>6. Why is binding the Node.js inspector to `0.0.0.0` a critical security vulnerability?</summary>

The V8 debugging protocol allows arbitrary code evaluation and command execution over a WebSocket connection without built-in authentication. Binding to `0.0.0.0` exposes this port to the local network or internet, allowing remote attackers to achieve Remote Code Execution (RCE).
</details>

<details>
<summary>7. What are Logpoints in VS Code debugging?</summary>

Logpoints allow developers to log messages to the debug console without modifying the source code or pausing thread execution. They can interpolate expressions like `{user.id}`.
</details>

<details>
<summary>8. How do you take a memory heap snapshot of a running Node.js TypeScript process?</summary>

By calling `node:v8.getHeapSnapshot()` from code, or connecting Chrome DevTools to the inspector port (`chrome://inspect`) and clicking "Take Snapshot" in the Memory tab.
</details>

---

## 13. Exercises & Challenges

<details>
<summary>Exercise 1 (Easy): Basic `launch.json` Setup</summary>

**Problem:** Create a minimal `.vscode/launch.json` configuration that launches `src/index.ts` using `tsx`.

**Solution:**
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "node",
      "request": "launch",
      "name": "Launch TS",
      "runtimeExecutable": "tsx",
      "args": ["${workspaceFolder}/src/index.ts"],
      "sourceMaps": true
    }
  ]
}
```
</details>

<details>
<summary>Exercise 2 (Easy): Conditional Breakpoint Expression</summary>

**Problem:** In a loop of 10,000 items, write the breakpoint condition expression to pause only when `index === 9999`.

**Solution:**
Right click the breakpoint red dot in VS Code, select "Edit Breakpoint" -> "Expression", and enter:
`index === 9999`
</details>

<details>
<summary>Exercise 3 (Medium): Source Map Inliner Script</summary>

**Problem:** Write a Node.js utility script that reads a `.js` file and its `.js.map` file and writes an inlined source map `.js` file.

**Solution:**
```typescript
import fs from 'node:fs';

export function inlineMap(jsPath: string, mapPath: string): void {
  const jsCode = fs.readFileSync(jsPath, 'utf-8');
  const mapCode = fs.readFileSync(mapPath, 'utf-8');
  const base64 = Buffer.from(mapCode).toString('base64');
  const inlined = `${jsCode}\n//# sourceMappingURL=data:application/json;base64,${base64}`;
  fs.writeFileSync(jsPath, inlined);
}
```
</details>

<details>
<summary>Exercise 4 (Medium): Custom Async Stack Tracer</summary>

**Problem:** Write an error wrapper that captures `Error.captureStackTrace` and formats a clean filtered stack trace omitting `node_modules`.

**Solution:**
```typescript
export function getCleanStack(err: Error): string {
  if (!err.stack) return '';
  return err.stack
    .split('\n')
    .filter((line) => !line.includes('node_modules'))
    .join('\n');
}
```
</details>

<details>
<summary>Exercise 5 (Hard): Profiler Flame Graph Generator</summary>

**Problem:** Use the `node:inspector` API to record a 5-second CPU profile and save it to disk.

**Solution:**
```typescript
import inspector from 'node:inspector';
import fs from 'node:fs';

export function recordCpuProfile(durationMs: number, outPath: string): Promise<void> {
  return new Promise((resolve) => {
    const session = new inspector.Session();
    session.connect();
    session.post('Profiler.enable', () => {
      session.post('Profiler.start', () => {
        setTimeout(() => {
          session.post('Profiler.stop', (_, { profile }) => {
            fs.writeFileSync(outPath, JSON.stringify(profile));
            session.disconnect();
            resolve();
          });
        }, durationMs);
      });
    });
  });
}
```
</details>

<details>
<summary>Exercise 6 (Hard): Heap Leak Diagnostic Watchdog</summary>

**Problem:** Create a background monitoring class that checks Node's heap memory usage every 10 seconds and automatically dumps a heap snapshot if heap usage exceeds 500 MB.

**Solution:**
```typescript
import v8 from 'node:v8';
import fs from 'node:fs';

export class MemoryWatchdog {
  private timer?: NodeJS.Timeout;

  public start(maxBytes = 500 * 1024 * 1024) {
    this.timer = setInterval(() => {
      const stats = process.memoryUsage();
      if (stats.heapUsed > maxBytes) {
        console.warn(`Heap threshold exceeded: ${(stats.heapUsed / 1e6).toFixed(1)}MB! Dumping snapshot...`);
        const stream = v8.getHeapSnapshot();
        stream.pipe(fs.createWriteStream(`heap-leak-${Date.now()}.heapsnapshot`));
      }
    }, 10000);
  }

  public stop() { clearInterval(this.timer); }
}
```
</details>
