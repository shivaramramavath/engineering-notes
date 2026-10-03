# 04 · Scope and Execution

This chapter explains **where** a variable can be seen (scope), **how** the engine finds it (lexical environments), **how** code actually runs (execution contexts and the call stack), and **why** some names work before their declaration (hoisting). It ends with strict mode, the safer dialect of the language.

## Reading order

| # | File | You will learn |
|---|------|----------------|
| 1 | [Scope](./01_scope.md) | Global, function, block and module scope, shadowing |
| 2 | [Lexical Environment](./02_lexical-environment.md) | Scope chain, environment records, how lookup works |
| 3 | [Execution Context and Call Stack](./03_execution-context-and-call-stack.md) | Creation vs execution phase, stack frames, overflow |
| 4 | [Hoisting and TDZ](./04_hoisting-and-tdz.md) | What is hoisted, the temporal dead zone |
| 5 | [Strict Mode](./05_strict-mode.md) | `"use strict"`, what changes, why it matters |

## Goal

By the end you can predict which variable a name refers to, explain what the engine does before and while code runs, and read stack traces with confidence.

**Next:** [Scope](./01_scope.md)
