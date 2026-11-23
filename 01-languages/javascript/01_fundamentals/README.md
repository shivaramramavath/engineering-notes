# Module 01: JavaScript Language Fundamentals

> Core language primitives, lexical structure, dynamic typing, abstract coercion algorithms, expressions, control flow, IEEE 754 precision, and language gotchas.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Language Reference & Engineering Notes |
| **Module** | `01_fundamentals` |
| **Level** | Beginner to Intermediate |
| **Target Runtime** | Node.js v20+ / v22+ LTS, ECMAScript 2015 to ES2024/ES2025 |
| **Status** | Complete (9 of 9 notes) |

---

## Module Overview

This module establishes the foundational mechanics of the JavaScript programming language. Understanding dynamic typing, scope lifecycles, abstract type conversions, and IEEE 754 arithmetic eliminates the unpredictable "WAT" moments that plague developers and ensures predictable, high-performance software.

```
01_fundamentals/
├── 01_variables.md                           # var, let, const, hoisting, TDZ, lexical scope
├── 02_data-types.md                          # 7 primitives + Object, auto-boxing, Symbol, BigInt, V8 SMI
├── 03_type-conversion-and-equality.md        # ToPrimitive, ToBoolean, == vs === vs Object.is, SameValueZero
├── 04_operators.md                           # Arithmetic, bitwise Int32 cast, short-circuit, ??, ?., precedence
├── 05_expressions-and-statements.md          # Expressions vs statements, ASI spec rules, restricted productions
├── 06_conditionals.md                        # if/else, strict switch, guard clauses, strategy dispatch maps
├── 07_loops.md                               # for, while, for...of (iteration protocol), for...in hazards, labels
├── 08_floating-point-and-number-precision.md # IEEE 754 double precision, 0.1+0.2, EPSILON, safe integer limits
├── 09_common-javascript-gotchas.md           # Catalog of 26 JavaScript quirks, engine reasons & solutions
└── README.md                                 # Module roadmap and curriculum index
```

---

## Curriculum Roadmap

| # | Note | Description | Key Topics |
| :-: | :--- | :--- | :--- |
| **01** | [Variables & Scopes](./01_variables.md) | Variable declarations, memory bindings, and environment records. | `var` vs `let` vs `const`, hoisting, Temporal Dead Zone (TDZ), block vs function scope. |
| **02** | [Data Types & References](./02_data-types.md) | JavaScript's 8 types, memory representation, and auto-boxing. | 7 primitives, object reference pointers, auto-boxing wrappers, `Symbol`, `BigInt`, V8 SMIs. |
| **03** | [Type Conversion & Equality](./03_type-conversion-and-equality.md) | Explicit conversion and engine abstract operations. | `ToPrimitive`, `ToNumber`, `ToBoolean`, loose `==`, strict `===`, `Object.is()`, `SameValueZero`. |
| **04** | [Operators & Precedence](./04_operators.md) | Arithmetic, bitwise, logical short-circuit, and chaining operators. | 32-bit integer cast, `&&`, `\|\|`, nullish `??`, optional chaining `?.`, logical assignment, precedence. |
| **05** | [Expressions & Statements](./05_expressions-and-statements.md) | The dual syntactic grammar of ECMAScript and ASI rules. | Expression statements, block statements, Automatic Semicolon Insertion (ASI), restricted productions. |
| **06** | [Conditionals & Branching](./06_conditionals.md) | Control flow routing, switch statements, and pattern matching. | `if/else`, strict equality `switch`, intentional fallthrough, guard clauses, dictionary dispatch. |
| **07** | [Loops & Iteration Protocols](./07_loops.md) | Repetition constructs and the ECMAScript Iteration Protocol. | Classic `for`, `while`, `for...of` (`Symbol.iterator`), `for...in` prototype pitfalls, labels. |
| **08** | [Floating-Point & Number Precision](./08_floating-point-and-number-precision.md) | IEEE 754 binary64 double-precision arithmetic deep dive. | Binary fractions, `0.1 + 0.2`, `Number.EPSILON`, `MAX_SAFE_INTEGER`, signed zeroes, currency math. |
| **09** | [Common JavaScript Gotchas](./09_common-javascript-gotchas.md) | Exhaustive catalog of 26 language quirks and counter-intuitive behaviors. | Type traps, array sorting quirks, sparse holes, async forEach, parser gotchas, and ESLint defenses. |

---

## Key Learning Outcomes

Upon completing this module, you will be able to:
1. Explain the exact differences between `var`, `let`, and `const` and predict Temporal Dead Zone errors.
2. Accurately trace the `ToPrimitive` and `IsLooselyEqual` abstract operations through complex coerced expressions.
3. Use optional chaining (`?.`) and nullish coalescing (`??`) to build bulletproof defensive accessors.
4. Diagnose and prevent Automatic Semicolon Insertion (ASI) bugs in production code.
5. Structure loops and collections using the native ECMAScript Iteration Protocol (`Symbol.iterator`).
6. Perform safe high-precision numerical and currency calculations without IEEE 754 floating-point rounding errors.

---

## Next Steps

After mastering fundamentals, proceed to [Module 02: Functions & Functional Mechanics](../02_functions/README.md) to explore first-class functions, closures, arrow functions, default/rest parameters, and execution contexts.
