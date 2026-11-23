# JavaScript Scope: Global, Function, Block & Module Resolution

> Comprehensive exploration of JavaScript scope mechanisms, variable visibility boundaries, shadowing, lexical lookup chains, and engine-level scope resolution.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [01_fundamentals/01_variables.md](../01_fundamentals/01_variables.md), [02_functions/01_declarations-and-expressions.md](../02_functions/01_declarations-and-expressions.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES1 (Global/Function) / ES2015 (Block & Module Scope) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The 4 Types of Scope](#2-core-concepts-the-4-types-of-scope)
3. [How It Works: The Scope Chain & Identifier Resolution](#3-how-it-works-the-scope-chain--identifier-resolution)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Scope Analysis & ScopeInfo Objects](#9-engine-internals-v8-scope-analysis--scopeinfo-objects)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Secure Multi-Tenant Context Isolation](#11-real-world-case-study-secure-multi-tenant-context-isolation)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In computer programming, **scope** defines the spatial boundaries within source code where an identifier (variable, function, or class name) is visible, accessible, and referenceable. Scope governs how the engine resolves names to memory references during parsing and execution.

Before ECMAScript 2015 (ES6), JavaScript had only two scopes: **Global Scope** and **Function Scope**. This lack of block scoping caused widespread bugs, such as loop index leaks and accidental global variable pollution. Modern ECMAScript features four distinct scope levels:
1. **Global Scope**
2. **Function Scope**
3. **Block Scope** (introduced with `let`, `const`, and class declarations)
4. **Module Scope** (introduced with ES Modules)

Understanding scope rules allows developers to control variable lifecycles, encapsulate private logic, avoid global namespace pollution, and debug memory retention issues.

---

## 2. Core Concepts: The 4 Types of Scope

```
                             The Scope Hierarchy
                                     │
         ┌───────────────────────────┼───────────────────────────┐
         ▼                           ▼                           ▼
   Global Scope                 Module Scope               Function Scope
   Accessible everywhere;       Top-level in .mjs or       Local to function body;
   attached to globalThis       isolated module file       var, let, const, fn
   in non-module scripts                                         │
                                                                 ▼
                                                            Block Scope
                                                            Bounded by { ... };
                                                            let, const, class
```

### 1. Global Scope
Variables declared outside any function or block in non-module scripts belong to the global scope. In browsers, global `var` declarations become properties on `window`. In modern JavaScript, `globalThis` provides a universal reference to the global object across Node.js, browsers, and Web Workers.

### 2. Module Scope
In ES Modules (`<script type="module">` or `.mjs` files), top-level declarations are scoped strictly to the module file. They do not pollute `globalThis` and can only be accessed elsewhere via explicit `export` and `import` declarations.

### 3. Function Scope
Variables declared with `var`, function declarations, and function parameters are scoped to the containing function body. They cannot be accessed from outside the function.

### 4. Block Scope
Introduced in ES2015, statements enclosed in curly braces `{ ... }` (such as `if`, `for`, `while`, or standalone blocks) establish a block scope for `let`, `const`, and `class` declarations.

---

## 3. How It Works: The Scope Chain & Identifier Resolution

When code reads an identifier `x`, the engine resolves the reference through a static **Lexical Scope Chain**:

```
[ Inner Function Scope ]  ──►  [ Outer Function Scope ]  ──►  [ Global Scope ]
   Does it have 'x'?              Does it have 'x'?              Does it have 'x'?
   YES -> Return value            YES -> Return value            YES -> Return value
   NO  ──(walk outer)────────────►NO  ──(walk outer)────────────►NO  ──► ReferenceError!
```

1. **Local Check**: The engine checks the current innermost execution environment.
2. **Outer Traversal**: If the identifier is not found, the engine follows the `[[OuterEnv]]` reference to the enclosing lexical environment.
3. **Global Termination**: The walk continues outward until the global environment is reached. If the identifier is not found in the global environment, a `ReferenceError: x is not defined` is thrown.
4. **Lexical (Static) Determination**: Scope resolution is determined at **compile/parse time** based on where functions are physically authored in code, *not* where they are called at runtime.

---

## 4. Syntax & API Reference

### Declarations Across Scopes

| Keyword | Scope Level | Hoisted? | Initialized on Hoist? | Re-declarable? | Re-assignable? |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `var` | Function / Global | Yes | `undefined` | Yes | Yes |
| `let` | Block | Yes (TDZ) | No (Uninitialized) | No | Yes |
| `const` | Block | Yes (TDZ) | No (Uninitialized) | No | No |
| `function` | Function / Block | Yes | Initialized to Function | Yes (sloppy) | Yes |
| `class` | Block | Yes (TDZ) | No (Uninitialized) | No | Yes |

---

## 5. Practical Examples

### Example 1: Block Scope vs Function Scope in Loops
```javascript
// Function scope leak with var:
for (var i = 0; i < 3; i++) {
  // i is scoped to outer function / global
}
console.log("var leaked:", i); // 3

// Block scope containment with let:
for (let j = 0; j < 3; j++) {
  // j is re-bound to each loop iteration block
}
try {
  console.log(j); // Throws ReferenceError
} catch (err) {
  console.log("let contained:", err.message); // j is not defined
}
```

### Example 2: Real-World Identifier Shadowing
```javascript
const config = { env: "production", timeout: 5000 };

function createHttpClient(endpoint) {
  // Shadowing 'config' within function scope
  const config = { env: "staging", timeout: 1000 };

  return function request(path) {
    // Nested block shadowing
    const endpoint = "https://api.internal.local";
    console.log(`Connecting to ${endpoint}${path} [env: ${config.env}]`);
  };
}

const client = createHttpClient("https://public.api.com");
client("/users");
// Output: Connecting to https://api.internal.local/users [env: staging]
```

### Example 3: Edge Case with Catch Clause Block Scope
```javascript
// The catch parameter creates its own block scope
const error = "Outer error string";

try {
  throw new Error("Caught network failure");
} catch (error) {
  // 'error' shadows the outer 'error' ONLY inside this catch block
  console.log("Inner catch scope:", error.message); // Caught network failure
}

console.log("Outer scope remains:", error); // Outer error string
```

### Example 4: Failure Mode with Accidental Global Assignment
```javascript
function calculateMetrics() {
  // Failure mode: Omitting var/let/const in non-strict mode creates an accidental global!
  // In strict mode ('use strict'), this throws ReferenceError immediately.
  accidentalGlobal = 42; 
}

calculateMetrics();
console.log(globalThis.accidentalGlobal); // 42 (Leaked to global object!)
delete globalThis.accidentalGlobal;      // Cleanup
```

### Example 5: Idiomatic Standalone Block Scoping for Resource Isolation
```javascript
// Clean isolation without IIFE overhead using standalone block {}
{
  const dbConnection = { id: "conn_123", active: true };
  const query = "SELECT * FROM telemetry WHERE status = 'pending'";
  
  // Perform temporary processing...
  console.log(`Processing with ${dbConnection.id}: ${query}`);
}

// dbConnection and query are immediately out of scope and eligible for GC
console.log(typeof dbConnection); // "undefined"
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Lexical Scope is NOT Dynamic Scope**: The scope of a function depends on where it was *defined*, not where it is *invoked*.
   ```javascript
   const x = 10;
   function foo() { console.log(x); }
   function bar() { const x = 20; foo(); }
   bar(); // Logs 10, NOT 20!
   ```
2. **Block-Scoped Functions in ES2015**: In strict mode, function declarations inside blocks (`{ function f() {} }`) are block-scoped. In non-strict mode, browser compatibility quirks may hoist them into the enclosing function.
3. **`eval()` Creates Dynamic Scope**: Non-strict `eval("var a = 1;")` injects variables into the enclosing scope, defeating engine static scope analysis. Strict mode `eval` runs in its own private scope.
4. **With Statement Deprecation**: The `with (obj)` statement dynamically injects object properties into the front of the scope chain. It is strictly prohibited in ES5 strict mode.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Variable leakage via `var` | `for (var i = 0; i < n; i++)` | `for (let i = 0; i < n; i++)` | Leaks index variable `i` to outer function or global scope. |
| Global pollution without declaration | `function run() { result = 1; }` | `function run() { const result = 1; }` | Omitting declaration creates an implicit property on `globalThis`. |
| Unintended variable shadowing | Shadowing outer parameter with local `let` | Use descriptive distinct names | Makes debugging difficult and causes logic bugs. |
| Over-reliance on global state | Storing app state in `window.appData` | Use ES modules or dependency injection | Hard to test, prone to race conditions and name clashes. |
| Modifying globals in libraries | `Array.prototype.custom = ...` | Use utility functions | Breaks compatibility with other libraries and web standards. |
| Using `with` statements | `with (obj) { prop = 1; }` | `obj.prop = 1;` | Prevents JIT optimization; causes severe scope ambiguity. |

---

## 8. Performance Characteristics

- **Local Variable Access**: Constant time ($O(1)$) indexed CPU register read. In TurboFan, local variables are assigned to hardware CPU registers or fixed stack frame offsets.
- **Deep Scope Chain Walk**: Traversal incurs minimal overhead during JIT-optimized execution because the compiler statically resolves the depth offset of lexical environments.
- **Global Variable Access**: Slightly slower than local reads because global properties may involve dictionary lookups or IC (Inline Cache) checks on `globalThis`.
- **`eval()` and `with`**: Completely disable compiler scope optimization, forcing the engine to fall back to deoptimized dynamic dictionary lookups.

---

## 9. Engine Internals: V8 Scope Analysis & ScopeInfo Objects

During the initial parsing phase:
1. **AstBuilding**: V8 builds an Abstract Syntax Tree (AST) while simultaneously constructing a hierarchy of `Scope` objects (`ModuleScope`, `FunctionScope`, `BlockScope`).
2. **Variable Allocation**: V8 categorizes every variable:
   - **Stack-allocated**: Variables that do not escape their containing function are assigned to stack slots (`VariableMode::kStack`).
   - **Context-allocated**: Variables that are referenced by an inner nested function (closures) must survive after the function returns. V8 allocates a heap object called a **Context** (`VariableLocation::CONTEXT`).
3. **ScopeInfo**: When a function is serialized into bytecode, V8 attaches a `ScopeInfo` descriptor containing the names and positions of all in-scope identifiers.

---

## 10. Best Practices & Design Guidelines

1. **Default to `const`**: Declare every variable with `const` by default. Only switch to `let` when reassignment is explicitly required. Never use `var`.
2. **Minimize Scope Footprint**: Declare variables in the narrowest possible block scope (e.g. inside `if` or `for` blocks rather than at the top of functions).
3. **Always Use Strict Mode or ES Modules**: ES modules are strictly evaluated in strict mode by default, preventing accidental global creation.

---

## 11. Real-World Case Study: Secure Multi-Tenant Context Isolation

In a microservice plugin host, executing user scripts requires strict scope isolation to ensure plugins cannot access or modify the host runtime or peer plugin state.

```javascript
class TenantContextSandbox {
  #isolatedGlobals;

  constructor(tenantId) {
    // Whitelist safe global APIs, strictly omitting process, require, and globalThis
    this.#isolatedGlobals = Object.freeze({
      tenantId,
      console: Object.freeze({
        log: (...args) => console.log(`[Tenant ${tenantId}]`, ...args)
      }),
      Math,
      Date,
      JSON
    });
  }

  // Execute untrusted code with restricted lexical scope
  execute(codeString) {
    // Generate isolated function wrapper passing whitelisted keys as parameters
    const paramNames = Object.keys(this.#isolatedGlobals);
    const paramValues = Object.values(this.#isolatedGlobals);

    // Strict mode prevents variable leakage
    const sandboxFn = new Function(
      ...paramNames,
      `"use strict"; return (() => { ${codeString} })();`
    );

    return sandboxFn(...paramValues);
  }
}

// Verification
const sandbox = new TenantContextSandbox("acme_corp");

const result = sandbox.execute(`
  const taxRate = 0.08;
  const total = 100 * (1 + taxRate);
  console.log("Calculated total:", total);
  return { tenantId, total };
`);

console.log("Sandbox Return:", result);
// Output: [Tenant acme_corp] Calculated total: 108
// Output: Sandbox Return: { tenantId: 'acme_corp', total: 108 }
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between Lexical Scope and Dynamic Scope?</strong></summary>

Lexical scope (static scope) determines variable resolution based on where functions and blocks are authored in source code at compile time. Dynamic scope resolves variables based on the runtime call stack (where functions are invoked). JavaScript uses lexical scope for all identifiers (though `this` exhibits dynamic-like binding behavior).
</details>

<details>
<summary><strong>2. How does block scoping work for <code>let</code> inside a <code>for</code> loop?</strong></summary>

The ECMAScript specification mandates that for `for (let i = 0; ...)` loops, a **fresh lexical environment binding** is created for every single iteration. Any closure created inside the loop body captures the unique binding of `i` for that specific iteration, eliminating the classic `var` loop indexing bug.
</details>

<details>
<summary><strong>3. Why does <code>var</code> ignore block boundaries?</strong></summary>

In the original JavaScript grammar (ES1–ES5), `var` was designed to be scoped exclusively to execution contexts (either the enclosing function or the global object). Blocks (`{}`) were purely structural control flow containers and did not instantiate a new lexical environment for `var`.
</details>

<details>
<summary><strong>4. What is variable shadowing, and does it destroy the outer variable?</strong></summary>

Variable shadowing occurs when an identifier declared within an inner scope has the identical name as an identifier in an enclosing outer scope. The inner declaration temporarily hides (shadows) the outer variable within that block. The outer variable is not destroyed or mutated; it remains intact and resumes visibility once the inner scope exits.
</details>

<details>
<summary><strong>5. How does ES Module scope differ from Script scope?</strong></summary>

In traditional scripts, top-level variables declared with `var` or `function` are bound to the global object (`window` or `globalThis`). In ES Modules (`import`/`export`), the top level of the file is evaluated within its own private **Module Environment Record**. Top-level variables never pollute `globalThis` and are completely isolated unless explicitly exported.
</details>

<details>
<summary><strong>6. How does the <code>catch</code> block in a <code>try...catch</code> statement exhibit unique scoping?</strong></summary>

The `catch (err)` clause creates an explicit block scope whose parameter (`err`) is only bound inside that catch block. Even in ES3/ES5 prior to `let` and `const`, the catch clause parameter was block-scoped and shadowed outer variables with the same name.
</details>

<details>
<summary><strong>7. What happens when an undeclared variable is assigned in non-strict mode vs strict mode?</strong></summary>

In non-strict mode, assigning to an identifier not found anywhere in the scope chain causes JavaScript to implicitly create a property on the global object (`globalThis[name] = val`). In strict mode (`"use strict"`), it throws a `ReferenceError: <name> is not defined`.
</details>

<details>
<summary><strong>8. How does V8 determine whether a variable belongs on the CPU stack or on the heap?</strong></summary>

During AST parsing, V8 performs **Scope Analysis**. If an inner function references a variable from an outer scope (creating a closure), that variable "escapes" the stack lifetime of the outer function. V8 flags the variable as `kContextAllocated` and allocates it inside a heap-allocated `Context` object. Otherwise, it allocates the variable in a fast stack slot.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Scope Chain Tracing (Easy)
Trace the exact output of the following nested code without executing it in a runtime.

```javascript
const a = 1;
function outer() {
  const b = 2;
  function inner() {
    const c = 3;
    console.log(a, b, c);
  }
  inner();
}
outer();
```

<details>
<summary>View Solution</summary>

Output: `1 2 3`.
- `c` is resolved locally in `inner`.
- `b` is resolved in `outer`'s lexical environment.
- `a` is resolved in the global lexical environment.
</details>

### Exercise 2: Block Scope Loop Fix (Easy)
Fix the following code so that it logs `0`, `1`, `2` sequentially after 50ms rather than `3`, `3`, `3`.

```javascript
for (var i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 50);
}
```

<details>
<summary>View Solution</summary>

```javascript
// Replace var with let to create a per-iteration block binding
for (let i = 0; i < 3; i++) {
  setTimeout(() => console.log(i), 50);
}
```
</details>

### Exercise 3: Scope Guard Pattern (Medium)
Implement a function `createScope()` that returns an object allowing temporary key-value overrides that are automatically restored when a disposal callback is invoked.

<details>
<summary>View Solution</summary>

```javascript
function createScope(initialContext) {
  const context = { ...initialContext };
  return {
    get: key => context[key],
    withOverride: (key, tempValue, action) => {
      const originalValue = context[key];
      context[key] = tempValue;
      try {
        return action(context);
      } finally {
        context[key] = originalValue; // Guaranteed restoration
      }
    }
  };
}

const scope = createScope({ theme: "dark" });
scope.withOverride("theme", "light", ctx => {
  console.log("Temporary:", ctx.theme); // light
});
console.log("Restored:", scope.get("theme")); // dark
```
</details>

### Exercise 4: Shadowing Analysis Inspector (Medium)
Write a static analyzer function `detectShadowing(codeString)` using basic regex parsing that identifies whether any inner `let`/`const` shadows a parameter name.

<details>
<summary>View Solution</summary>

```javascript
function detectShadowing(paramName, functionBodyString) {
  const pattern = new RegExp(`(?:let|const|var)\\s+${paramName}\\s*=`, "g");
  return pattern.test(functionBodyString);
}

const fnCode = "function test(id) { const id = 99; return id; }";
console.log("Shadows parameter:", detectShadowing("id", fnCode)); // true
```
</details>

### Exercise 5: Emulating Block Scope via IIFE (Hard)
Given a legacy ES5 environment without `let` or `const`, rewrite the following block-scoped code preserving exact variable isolation semantics.

```javascript
{
  const secretKey = "api_sec_100";
  const encrypt = data => `${secretKey}:${data}`;
  console.log(encrypt("payload"));
}
```

<details>
<summary>View Solution</summary>

```javascript
(function () {
  var secretKey = "api_sec_100";
  var encrypt = function (data) {
    return secretKey + ":" + data;
  };
  console.log(encrypt("payload"));
})();

// Verify isolation:
console.log(typeof secretKey); // "undefined"
```
</details>

### Exercise 6: Dynamic Context Virtual Machine (Hard)
Build a recursive lexical environment evaluator `evaluateExpression(exprTree, env)` that resolves variables by traversing linked environment frames (`{ bindings, parent }`).

<details>
<summary>View Solution</summary>

```javascript
class LexicalEnvironment {
  constructor(bindings = {}, parent = null) {
    this.bindings = new Map(Object.entries(bindings));
    this.parent = parent;
  }

  get(name) {
    if (this.bindings.has(name)) {
      return this.bindings.get(name);
    }
    if (this.parent !== null) {
      return this.parent.get(name);
    }
    throw new ReferenceError(`Identifier '${name}' not found in scope chain`);
  }

  set(name, value) {
    this.bindings.set(name, value);
  }
}

// Verification
const globalEnv = new LexicalEnvironment({ x: 10, y: 20 });
const functionEnv = new LexicalEnvironment({ y: 99, z: 30 }, globalEnv);
const blockEnv = new LexicalEnvironment({ z: 100 }, functionEnv);

console.log(blockEnv.get("z")); // 100 (from local block)
console.log(blockEnv.get("y")); // 99 (from functionEnv, shadows global y)
console.log(blockEnv.get("x")); // 10 (from globalEnv)
```
</details>
