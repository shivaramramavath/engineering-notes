# Module 04: Scope & Execution

> Architectural guide to JavaScript execution context mechanics, lexical environment records, identifier resolution algorithms, call stack frames, hoisting, Temporal Dead Zone (TDZ), and strict mode enforcement.

## Overview & Curriculum Map

Understanding how JavaScript executes source code requires stripping away casual metaphors and inspecting the concrete specification mechanics: Execution Contexts, Environment Records, and Call Stack frames. Every identifier lookup traverses an internal lexical chain, and every declaration undergoes compilation phase registration before a single line of bytecode evaluates.

This module details the runtime engine architecture behind variable visibility, stack frame management, and error containment across all four JavaScript scope levels.

---

## Complete Note Index

| Order | Note | Level | Topics Covered | Status |
| :---: | :--- | :---: | :--- | :---: |
| 01 | [01_scope.md](./01_scope.md) | Core / Inter | Global, Function, Block, and Module scope; identifier shadowing; lexical vs dynamic scope; multi-tenant context isolation | Complete |
| 02 | [02_lexical-environment.md](./02_lexical-environment.md) | Advanced | ECMAScript §9.1 Lexical Environments, Declarative vs Object vs Module Environment Records, `[[OuterEnv]]` pointers, V8 heap Context objects | Complete |
| 03 | [03_execution-context-and-call-stack.md](./03_execution-context-and-call-stack.md) | Core / Inter | Global vs Function vs Eval execution contexts, Creation Phase vs Execution Phase, LIFO Call Stack, `RangeError` stack overflow, stack trace sanitization | Complete |
| 04 | [04_hoisting-and-tdz.md](./04_hoisting-and-tdz.md) | Core / Inter | Function vs `var` vs `let`/`const`/`class` hoisting, Temporal Dead Zone boundaries, `typeof` TDZ semantics, parameter TDZ, V8 `the_hole` sentinel | Complete |
| 05 | [05_strict-mode.md](./05_strict-mode.md) | Core / Inter | `"use strict"` directive, automatic class and module strictness, silent failures converted to `TypeError`/`ReferenceError`, `eval` isolation, undefined `this` | Complete |

---

## Learning Objectives & Mastery Checklist

By completing this module, you will be able to:
- [x] Trace variable lookup across complex nested block and function scopes using the `[[OuterEnv]]` specification algorithm.
- [x] Differentiate Declarative Environment Records from Object Environment Records and explain how V8 allocates Context objects on the heap for closures.
- [x] Break down the two-phase lifecycle of Execution Contexts (Creation Phase hoisting and binding vs. Execution Phase evaluation).
- [x] Predict and prevent Temporal Dead Zone (`ReferenceError`) runtime crashes across block scopes and default function parameter lists.
- [x] Sanitize production error stack traces and implement trampolining to execute deeply recursive algorithms without exhausting the Call Stack.
- [x] Enforce strict mode contracts to convert legacy silent assignment failures into explicit runtime exceptions.

---

## Verification & Execution Baseline

All code examples, edge cases, and performance benchmarks in this module are tested and verified against:
- **Node.js**: `v24.11.1` / `v22.x LTS`
- **ECMAScript Standards**: ES5 through ES2024
- **Engine Targets**: Google V8 (Chromium / Node.js), JavaScriptCore (Safari), SpiderMonkey (Firefox)
