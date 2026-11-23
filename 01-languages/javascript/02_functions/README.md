# Module 02: Functions & Execution Mechanics

> First-class functions, lexical scopes, arrow function semantics, dynamic parameters, higher-order functions, recursion, and V8 JIT compilation pipelines.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Language Reference & Engineering Notes |
| **Module** | `02_functions` |
| **Level** | Beginner to Advanced |
| **Target Runtime** | Node.js v20+ / v22+ LTS, ECMAScript 2015 to ES2024/ES2025 |
| **Status** | Complete (9 of 9 notes) |

---

## Module Overview

Functions are first-class values in JavaScript: they can be bound to identifiers, passed as callback arguments, dynamically returned from higher-order factories, and inspected as heap objects.

This module details function definitions, lexical binding rules, parameter evaluation scopes, functional composition, call stack mechanics, and modern V8 engine JIT compiler optimizations.

```
02_functions/
├── 01_declarations-and-expressions.md       # Declarations vs expressions, NFE, [[Call]], [[Construct]]
├── 02_arrow-functions.md                    # Lexical this, concise returns, omission of arguments/prototype
├── 03_parameters-and-arguments.md           # Default call-time evaluation, rest params, parameter scope TDZ
├── 04_return-values.md                      # Implicit undefined, constructor return overrides, finally traps
├── 05_callbacks.md                          # Sync vs async callbacks, Node error-first, Zalgo defense
├── 06_higher-order-functions.md             # HOFs, currying, pipe/compose, memoization, decorators
├── 07_iife-and-function-properties.md       # IIFE patterns, fn.name, fn.length, V8 eager parsing
├── 08_recursion.md                          # Base cases, call stack depth limits, trampolining, DFS
├── 09_function-internals-and-tail-calls.md  # V8 JIT tiers (Ignition/Sparkplug/Maglev/TurboFan), ICs, TCO
└── README.md                                # Module roadmap and curriculum index
```

---

## Curriculum Roadmap

| # | Note | Description | Key Topics |
| :-: | :--- | :--- | :--- |
| **01** | [Declarations, Expressions & NFE](./01_declarations-and-expressions.md) | The core forms of function definition and memory representation. | Statement hoisting vs expression evaluation, Named Function Expressions (NFE), internal slots. |
| **02** | [Arrow Functions & Lexical `this`](./02_arrow-functions.md) | Syntax and semantic differences of ES2015 arrow functions. | Lexical `this`, absence of `arguments`/`prototype`/`[[Construct]]`, concise vs block bodies. |
| **03** | [Parameters & Arguments](./03_parameters-and-arguments.md) | Argument contracts, arity mechanics, and parameter scoping. | Default parameters, call-time evaluation, parameter TDZ, rest parameters (`...rest`), `arguments` detachment. |
| **04** | [Return Values & Control Unwinding](./04_return-values.md) | Execution termination, stack unwinding, and return value mechanics. | Implicit `undefined`, constructor return override rule, `finally` block hijacking, tuple returns. |
| **05** | [Callbacks & Asynchronous Execution](./05_callbacks.md) | Inversion of control and callback architectures. | Sync vs async timing, Node error-first convention, avoiding Zalgo with `queueMicrotask`, promisification. |
| **06** | [Higher-Order Functions & Currying](./06_higher-order-functions.md) | Functions operating on functions and functional combinators. | Built-in array HOFs, currying vs partial application, `pipe` and `compose`, decorators, memoization. |
| **07** | [IIFE & Function Properties](./07_iife-and-function-properties.md) | Immediate execution patterns and function object metadata. | IIFE variations, Revealing Module Pattern, `fn.name`, `fn.length`, V8 parenthesized eager parsing. |
| **08** | [Recursion & Call Stack Limits](./08_recursion.md) | Self-referential problem decomposition and stack safety. | Base cases, call stack overflow boundaries, trampolining patterns, iterative stack conversion. |
| **09** | [Function Internals & Tail Calls](./09_function-internals-and-tail-calls.md) | Engine execution architecture, JIT compilation, and tail calls. | V8 4-tier pipeline (Ignition, Sparkplug, Maglev, TurboFan), Inline Caches (IC), deoptimizations, TCO realities. |

---

## Key Learning Outcomes

Upon completing this module, you will be able to:
1. Choose the optimal function construct (declaration, expression, NFE, arrow, concise method) for any architectural context.
2. Master lexical `this` resolution and eliminate callback context-binding bugs.
3. Design robust, type-checked function signatures using destructured parameters and dynamic call-time defaults.
4. Construct production-grade function decorators (`debounce`, `throttle`, `memoize`, `retry`).
5. Build stack-safe recursive algorithms using trampolining or iterative explicit stacks.
6. Write monomorphic, JIT-friendly code that maximizes V8 TurboFan inlining and avoids deoptimization storms.

---

## Next Steps

After mastering functions, proceed to [Module 03: Objects & Arrays](../03_objects-and-arrays/README.md) to explore property descriptors, prototypal inheritance, structured cloning, and modern array methods.
