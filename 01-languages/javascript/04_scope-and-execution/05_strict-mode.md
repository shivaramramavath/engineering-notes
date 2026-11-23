# JavaScript Strict Mode: Semantics, Constraints & Engine Enforcement

> In-depth guide to ECMAScript Strict Mode (`"use strict"`), comparing sloppy mode semantics with strict error-throwing behaviors, automatic module strictness, and JIT optimization paths.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [01_fundamentals/01_variables.md](../01_fundamentals/01_variables.md), [04_scope-and-execution/01_scope.md](./01_scope.md) |
| **Time to Read** | 21 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 (2009) / Automatically enabled in ES2015 Modules & Classes |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Enabling Strict Mode](#2-core-concepts-enabling-strict-mode)
3. [How It Works: Engine Behavior & Semantic Differences](#3-how-it-works-engine-behavior--semantic-differences)
4. [Syntax & API Reference: The 10 Core Strict Rules](#4-syntax--api-reference-the-10-core-strict-rules)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Parser Flags & Static Bailouts](#9-engine-internals-v8-parser-flags--static-bailouts)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Defending Domain Logic Against Silent Failures](#11-real-world-case-study-defending-domain-logic-against-silent-failures)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

JavaScript was designed in 10 days in 1995 with an emphasis on forgiveness: operations that were logically flawed (such as assigning to undeclared variables or modifying read-only properties) failed silently rather than throwing runtime errors. While this prevented early web pages from crashing, it made large-scale application development notoriously brittle.

Introduced in ECMAScript 5 (2009), **Strict Mode** is a restricted variant of JavaScript that intentionally opts out of sloppy, forgiving behaviors. Strict mode converts silent failures into explicit, actionable exceptions, eliminates engine-confusing syntax like `with`, and enforces cleaner scoping rules. 

In modern JavaScript, all **ES Modules (`import`/`export`)** and **ES6 `class` bodies** are evaluated in strict mode automatically.

---

## 2. Core Concepts: Enabling Strict Mode

Strict mode is enabled by placing the string directive `"use strict";` (or `'use strict';`) at the very beginning of a script or function body before any executable statements.

```javascript
// File-level strict mode (affects entire script):
"use strict";
let a = 1;

// Function-level strict mode:
function sloppyOuter() {
  function strictInner() {
    "use strict";
    // Strictly evaluated here
  }
}
```

```
                          Where Strict Mode Applies
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
   Automatic Strict Mode                                  Manual Opt-In ("use strict")
   • All ES Modules (.mjs, type="module")                 • Traditional scripts (.js)
   • Entire bodies of ES6 Classes                         • Individual classic functions
   • Class methods, constructors, getters                 • Node.js CommonJS files
```

---

## 3. How It Works: Engine Behavior & Semantic Differences

```
Sloppy Mode vs. Strict Mode Execution Path
┌───────────────────────────────────────────────┐
│ Attempt to assign to non-writable property    │
└───────────────────────┬───────────────────────┘
                        │
                Is Strict Mode On?
               /                  \
             NO                    YES
            /                        \
           ▼                          ▼
  Silent Failure!             Throw TypeError!
  (Value is not changed,      Execution halts immediately;
   no error reported)         developer alerted to logic bug.
```

When V8 parses source text, it tracks a boolean flag `is_strict_mode`. If `true`, the parser rejects deprecated grammar at compile time and emits runtime bytecode instructions that assert safety contracts before executing assignments.

---

## 4. Syntax & API Reference: The 10 Core Strict Rules

1. **No Accidental Globals**: Assigning to an undeclared identifier throws `ReferenceError`.
2. **Assignments Throw on Failure**: Assigning to a non-writable property, a getter-only accessor, or extending a sealed/frozen object throws `TypeError`.
3. **No Deleting Plain Identifiers**: `delete x` throws `SyntaxError`. Deleting non-configurable properties throws `TypeError`.
4. **No Duplicate Parameter Names**: `function f(a, a, b)` throws `SyntaxError`.
5. **No Octal Numeric Literals**: Legacy octal notation (`0777`) throws `SyntaxError` (use `0o777`).
6. **No `with` Statements**: Using `with (...)` throws `SyntaxError`.
7. **Undefined `this` in Plain Calls**: Invoking a standalone function sets `this` to `undefined`, rather than coercing it to `globalThis`.
8. **Isolated `eval()` Scope**: Variables declared inside `eval("var x = 1;")` do not leak into the calling lexical environment.
9. **No `arguments.callee` or `arguments.caller`**: Accessing these properties throws `TypeError`.
10. **Reserved Future Identifiers**: Words like `implements`, `interface`, `package`, `private`, `protected`, and `public` cannot be used as variable names.

---

## 5. Practical Examples

### Example 1: Eliminating Accidental Global Variables
```javascript
"use strict";

function computeMetrics() {
  // Sloppy mode: attaches 'metricVal' to globalThis!
  // Strict mode: throws ReferenceError immediately.
  try {
    // @ts-ignore
    metricVal = 100;
  } catch (err) {
    console.log("Strict mode caught global leak:", err.name); // ReferenceError
  }
}

computeMetrics();
```

### Example 2: Real-World Safe Assignment on Frozen Objects
```javascript
"use strict";

const appConfig = Object.freeze({
  apiEndpoint: "https://api.domain.com",
  timeoutMs: 5000
});

try {
  // In sloppy mode, this silently does nothing!
  // In strict mode, it throws TypeError: Cannot assign to read only property
  appConfig.timeoutMs = 10000;
} catch (err) {
  console.log("Protected config mutation caught:", err.message);
}
```

### Example 3: Edge Case with Standalone Function `this`
```javascript
function sloppyCheck() {
  return this; // Returns globalThis (window or global)
}

function strictCheck() {
  "use strict";
  return this; // Returns undefined!
}

console.log("Sloppy this is global:", sloppyCheck() === globalThis); // true
console.log("Strict this is undefined:", strictCheck() === undefined); // true
```

### Example 4: Failure Mode with Eval Scope Leaks
```javascript
// In sloppy mode:
function sloppyEval() {
  eval("var injected = 42;");
  console.log(injected); // 42! Injected into surrounding function
}
sloppyEval();

// In strict mode:
function strictEval() {
  "use strict";
  eval("var isolated = 99;");
  try {
    // @ts-ignore
    console.log(isolated); // Throws ReferenceError: isolated is not defined
  } catch (err) {
    console.log("Strict eval prevents scope leakage:", err.message);
  }
}
strictEval();
```

### Example 5: Idiomatic Classes with Automatic Strict Mode
```javascript
class BankingAccount {
  #balance;

  constructor(initialDeposit) {
    this.#balance = initialDeposit;
    // Classes are automatically in strict mode! No "use strict" needed.
  }

  deposit(amount) {
    if (amount <= 0) throw new RangeError("Invalid deposit");
    this.#balance += amount;
  }

  get balance() {
    return this.#balance;
  }
}

const account = new BankingAccount(100);
try {
  // Setting a getter-only property throws TypeError in classes
  account.balance = 500;
} catch (err) {
  console.log("Class automatic strictness caught accessor mutation:", err.message);
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Concatenation Hazard with File-Level `"use strict"`**: If multiple legacy JavaScript files are bundled by simple text concatenation, a file starting with `"use strict"` will accidentally force all subsequent files in the bundle into strict mode, potentially breaking legacy libraries that depend on sloppy mode.
2. **Duplicate Object Keys Were Once Prohibited**: In ES5, duplicate keys (`{ a: 1, a: 2 }`) threw a `SyntaxError` in strict mode. ECMAScript 2015 relaxed this rule; duplicate keys are now valid in strict mode (the latter key overwrites earlier keys).
3. **`delete` on Global Variables**: Calling `delete globalVar` throws a `SyntaxError` in strict mode. Properties can only be deleted if they are explicitly configured on an object (`delete globalThis.prop`).

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Omitting `"use strict"` in CommonJS | Files without directive | Add `"use strict";` at top | Leaves Node.js CommonJS files in error-prone sloppy mode. |
| Depending on `arguments.callee` | `arguments.callee(n - 1)` | Use named function expressions | `callee` is completely prohibited in strict mode. |
| Using `with` statements | `with (context) { run(); }` | Destructure or explicit access | Prohibited; causes severe performance deoptimizations. |
| Using legacy octals | `const perm = 0755;` | `const perm = 0o755;` | Legacy octals starting with 0 throw SyntaxError in strict mode. |
| Invoking methods as detached fn | `const f = obj.m; f();` | `const f = obj.m.bind(obj);` | In strict mode, `this` is `undefined`, causing crashes on `this.prop`. |
| Unintentional bundle strictness | Appending `"use strict"` globally | Wrap files in IIFEs or use ESM | Affects third-party legacy scripts concatenated after your file. |

---

## 8. Performance Characteristics

- **Better TurboFan Optimization**: In strict mode, because the engine knows variables cannot be injected dynamically via `with` or sloppy `eval()`, V8 can perform static variable binding and emit unboxed register loads without falling back to slow dictionary lookups.
- **Immediate Catching of Bailouts**: Strict mode prevents the engine from silently allocating hidden prototype properties or global objects, reducing heap churn and GC pressure.

---

## 9. Engine Internals: V8 Parser Flags & Static Bailouts

Inside V8:
1. **Parser State**: The V8 parser maintains a bitmask of parse flags. When the lexer encounters `"use strict";` as an `ExpressionStatement` consisting solely of a string literal directive, it sets `Parser::is_strict_mode = true`.
2. **Bytecode Generation**:
   - Sloppy property write: Emits `SetNamedProperty` with `LanguageMode::kSloppy`. If the descriptor is non-writable, the C++ runtime silently returns.
   - Strict property write: Emits `SetNamedProperty` with `LanguageMode::kStrict`. If the descriptor is non-writable, V8 invokes `Factory::NewTypeError` immediately.

---

## 10. Best Practices & Design Guidelines

1. **Author Exclusively in ES Modules or TypeScript**: ES modules and TypeScript compilers enforce strict mode by default.
2. **Never Rely on `this` Coercion**: Always bind functions explicitly or use arrow functions rather than expecting `this` to default to `window` or `globalThis`.
3. **Lint Against Sloppy Mode**: Configure ESLint rules (`strict: ["error", "global"]`) to catch files missing the directive.

---

## 11. Real-World Case Study: Defending Domain Logic Against Silent Failures

An e-commerce order calculator demonstrating how strict mode prevents critical accounting bugs that would otherwise silently corrupt transaction totals.

```javascript
"use strict";

class OrderCalculator {
  // Defensive freezing of base rates
  static RATES = Object.freeze({
    STANDARD_SHIPPING: 9.99,
    TAX_RATE: 0.08
  });

  static calculateInvoice(items, discountCode) {
    // 1. Calculate subtotal
    const subtotal = items.reduce((acc, item) => acc + item.price * item.quantity, 0);

    // 2. Resolve discount (Notice: attempting to mutate frozen RATES throws TypeError)
    let discount = 0;
    if (discountCode === "VIP") {
      discount = subtotal * 0.15;
    }

    const taxableAmount = Math.max(0, subtotal - discount);
    const tax = taxableAmount * OrderCalculator.RATES.TAX_RATE;
    const total = taxableAmount + tax + OrderCalculator.RATES.STANDARD_SHIPPING;

    return {
      subtotal,
      discount,
      tax: parseFloat(tax.toFixed(2)),
      shipping: OrderCalculator.RATES.STANDARD_SHIPPING,
      total: parseFloat(total.toFixed(2))
    };
  }
}

// Verification
const cart = [
  { name: "Mechanical Keyboard", price: 120.00, quantity: 1 },
  { name: "USB-C Cable", price: 15.00, quantity: 2 }
];

const invoice = OrderCalculator.calculateInvoice(cart, "VIP");
console.log("Calculated Invoice:", invoice);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What happens to <code>this</code> inside a standalone function call in strict mode vs sloppy mode?</strong></summary>

In sloppy mode, invoking a standalone function (`f()`) coerces `this` to the global object (`window` in browsers, `global` in Node.js). In strict mode, `this` remains `undefined` unless explicitly bound via `call`, `apply`, `bind`, or as an object method.
</details>

<details>
<summary><strong>2. Why is Strict Mode automatically enabled in ES Modules and ES6 Classes?</strong></summary>

The TC39 standards committee recognized that sloppy mode contained legacy design flaws. Starting with ES6, new language features (ES Modules and Classes) were specified to run exclusively in strict mode, phasing out sloppy mode for all modern syntax without breaking legacy script backwards compatibility.
</details>

<details>
<summary><strong>3. How does <code>eval()</code> behavior change under Strict Mode?</strong></summary>

In sloppy mode, `eval("var x = 10;")` creates a variable `x` in the surrounding function or global scope. In strict mode, `eval()` executes within its own private Lexical Environment; variables declared inside the eval string cannot leak into the enclosing scope.
</details>

<details>
<summary><strong>4. What error is thrown when assigning to a read-only property in strict mode vs sloppy mode?</strong></summary>

In sloppy mode, the assignment fails silently: the value does not change, and execution continues without warning. In strict mode, it throws a `TypeError: Cannot assign to read only property`.
</details>

<details>
<summary><strong>5. Can you use <code>with</code> statements in Strict Mode?</strong></summary>

No. The `with` statement throws a `SyntaxError` at parse time in strict mode. `with` creates dynamic scope ambiguity, which prevents JIT compilers like TurboFan from optimizing identifier resolution.
</details>

<details>
<summary><strong>6. Why did Strict Mode disallow legacy octal literals (e.g. <code>0755</code>)?</strong></summary>

Legacy octals starting with `0` were frequently confused with decimals by developers (e.g. treating `010` as 10 instead of 8). Strict mode prohibits leading zero octals and mandates the explicit `0o` prefix (e.g. `0o755`).
</details>

<details>
<summary><strong>7. What happens if you try to <code>delete</code> a variable declared with <code>var</code> in strict mode?</strong></summary>

In sloppy mode, `delete myVar` returns `false`. In strict mode, attempting to delete an unqualified identifier (`delete x`) throws a `SyntaxError: Delete of an unqualified identifier in strict mode`.
</details>

<details>
<summary><strong>8. How does strict mode affect the <code>arguments</code> object when parameters are reassigned?</strong></summary>

In sloppy mode, the `arguments` object shares a dynamic binding with named parameters: reassigning a parameter changes `arguments[i]`, and vice versa. In strict mode, the `arguments` object is decoupled: modifying a parameter does not alter `arguments`, and modifying `arguments[i]` does not alter the parameter.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Strict Mode Detection (Easy)
Write a function `isStrictMode()` that returns `true` if currently executing in strict mode and `false` otherwise, without relying on error catching.

<details>
<summary>View Solution</summary>

```javascript
function isStrictMode() {
  return (function () {
    return this === undefined;
  })();
}

console.log("Is strict mode:", isStrictMode());
```
</details>

### Exercise 2: Converting Silent Failures to Explicit Errors (Easy)
Identify the three silent failures in this sloppy code that would throw errors in strict mode:

```javascript
// Sloppy code
nonExistent = 10;
const obj = Object.freeze({ x: 1 });
obj.x = 2;
delete Object.prototype;
```

<details>
<summary>View Solution</summary>

1. `nonExistent = 10`: Throws `ReferenceError` (undeclared variable).
2. `obj.x = 2`: Throws `TypeError` (modifying read-only property of frozen object).
3. `delete Object.prototype`: Throws `TypeError` (deleting non-configurable property).
</details>

### Exercise 3: Dynamic Strict Mode Runner (Medium)
Create a function `runStrictly(codeString)` that executes arbitrary JavaScript code strings guaranteed to run under strict mode semantics.

<details>
<summary>View Solution</summary>

```javascript
function runStrictly(codeString) {
  const runner = new Function(`"use strict"; return (() => { ${codeString} })();`);
  return runner();
}

try {
  runStrictly("accidentalGlobal = 42;");
} catch (err) {
  console.log("Caught strict violation:", err.message);
  // accidentalGlobal is not defined
}
```
</details>

### Exercise 4: Arguments Decoupling Verification (Medium)
Write a test that demonstrates that parameter reassignment does not affect `arguments` in strict mode.

<details>
<summary>View Solution</summary>

```javascript
function verifyArgumentsDecoupling(val) {
  "use strict";
  val = "mutated";
  return {
    paramVal: val,
    argVal: arguments[0],
    isDecoupled: val !== arguments[0]
  };
}

console.log(verifyArgumentsDecoupling("initial"));
// { paramVal: 'mutated', argVal: 'initial', isDecoupled: true }
```
</details>

### Exercise 5: Safe Concatenation Wrapper (Hard)
Write a Node.js build utility function that wraps legacy JavaScript files into IIFEs to prevent accidental strict mode contamination when bundling.

<details>
<summary>View Solution</summary>

```javascript
function wrapFileForBundle(filename, sourceCode) {
  return `
/* --- Start: ${filename} --- */
(function() {
${sourceCode}
})();
/* --- End: ${filename} --- */
`;
}

const safeBundle = wrapFileForBundle("legacy.js", 'var a = 1; console.log("Legacy loaded");');
console.log(safeBundle);
```
</details>

### Exercise 6: Strict Environment Validator (Hard)
Build an automated test suite that accepts an arbitrary object and validates that all properties adhere to strict encapsulation contracts (no accidental globals, frozen configuration immutability, no `with` usage).

<details>
<summary>View Solution</summary>

```javascript
class StrictContractValidator {
  static validateFrozen(obj) {
    if (!Object.isFrozen(obj)) {
      throw new Error("Contract violation: object must be frozen");
    }
    // Attempt modification under strict mode
    try {
      "use strict";
      const keys = Object.keys(obj);
      if (keys.length > 0) {
        obj[keys[0]] = "test";
      }
      return false; // Should not reach here
    } catch (err) {
      return err instanceof TypeError;
    }
  }
}

const frozen = Object.freeze({ secure: true });
console.log("Strict contract verified:", StrictContractValidator.validateFrozen(frozen)); // true
```
</details>
