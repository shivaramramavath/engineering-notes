# Module Pattern

The **module pattern** groups related code behind a small public API while keeping the rest **private**. Before ES modules existed, JavaScript developers used closures to get this. Today the language has real modules, but the underlying idea (private state, public interface) still matters, and the closure-based forms are still useful inside a file.

See also: [Closures](../06_closures/00_README.md), [Closure Use Cases](../06_closures/02_closure-use-cases.md), [ES Modules](../13_modules/01_es-modules.md), [Module Patterns](../13_modules/05_module-patterns.md).

## The problem

Without encapsulation, everything is global or freely mutable:

```js
let count = 0;                       // anyone can change this
function increment() { count++; }
function reset() { count = 0; }

count = 'oops';                      // nothing stops this
```

Global variables collide across files, and callers can corrupt state that they should not be able to touch.

## Form 1: IIFE module (the classic)

An **immediately invoked function expression** creates a private scope. It returns an object containing only what you want to expose.

```js
const counter = (function () {
  let count = 0;                     // private: unreachable from outside

  function log(msg) {                // private helper
    console.log(`[counter] ${msg}`);
  }

  return {                           // public API
    increment() {
      count++;
      log(`now ${count}`);
      return count;
    },
    reset() {
      count = 0;
    },
    get value() {
      return count;
    },
  };
})();

counter.increment();   // 1
counter.value;         // 1
counter.count;         // undefined (private)
```

Variables inside the IIFE live as long as the returned functions do (that is the closure at work). See [IIFE and Function Properties](../02_functions/07_iife-and-function-properties.md).

### The revealing module variant

Define everything privately, then **reveal** chosen members at the end. The public API is visible in one place:

```js
const cart = (function () {
  const items = [];

  function add(item) { items.push(item); }
  function remove(id) {
    const i = items.findIndex((x) => x.id === id);
    if (i !== -1) items.splice(i, 1);
  }
  function total() { return items.reduce((sum, x) => sum + x.price, 0); }
  function count() { return items.length; }

  return { add, remove, total, count };    // reveal
})();
```

Benefit: internal calls use plain function names (`total()`), so replacing a public method on the returned object cannot break internal logic.

### Passing in dependencies

```js
const app = (function (window, $) {
  // use window and $ as local names; minifiers can shorten them
  return { init() { /* ... */ } };
})(window, jQuery);
```

## Form 2: Factory function with closure

A function that returns an object, with private state per call. Unlike the IIFE, you can create **many** independent instances:

```js
function createCounter(start = 0) {
  let count = start;

  return {
    increment: () => ++count,
    decrement: () => --count,
    get value() { return count; },
  };
}

const a = createCounter();
const b = createCounter(10);
a.increment();
b.decrement();
console.log(a.value, b.value);   // 1 9
```

This overlaps with the [factory pattern](./02_factory-pattern.md). The difference in emphasis: the module pattern is about **encapsulation**, the factory pattern is about **creation**.

## Form 3: ES modules (the modern way)

A file **is** a module. Anything not exported is private to the file:

```js
// counter.js
let count = 0;                       // private (module scope)

function log(msg) {                  // private
  console.log(`[counter] ${msg}`);
}

export function increment() {
  count++;
  log(`now ${count}`);
  return count;
}

export function reset() {
  count = 0;
}

export function getCount() {
  return count;
}
```

```js
// main.js
import { increment, getCount } from './counter.js';

increment();
getCount();          // 1
// count is not accessible here
```

Properties of ES modules:

| Property | Effect |
|----------|--------|
| Own scope | No accidental globals; top-level variables are module-private |
| Evaluated once | Every importer shares the same module instance and state |
| Strict mode by default | Safer semantics |
| Static structure | Bundlers can tree-shake unused exports |
| Live bindings | Exports reflect later changes in the exporting module |

Group the public API with a namespace import when it reads better:

```js
import * as counter from './counter.js';
counter.increment();
```

Or re-export as a single entry point (a "barrel" file):

```js
// index.js
export { increment, reset } from './counter.js';
export { createStore } from './store.js';
```

(Large barrel files can hurt tree-shaking and build speed. Use them for a small public surface, not for everything.)

## Form 4: Classes with private fields

Classes now have real private members with `#`:

```js
class Counter {
  #count = 0;                        // private field

  #log(msg) {                        // private method
    console.log(`[counter] ${msg}`);
  }

  increment() {
    this.#count++;
    this.#log(`now ${this.#count}`);
    return this.#count;
  }

  get value() {
    return this.#count;
  }
}

const c = new Counter();
c.increment();
c.#count;                            // SyntaxError: private field
```

See [Private Fields](../05_this-and-oop/07_private-fields.md). Unlike the old `_underscore` convention, `#` is enforced by the language.

## Choosing a form

| Form | Best for | Notes |
|------|----------|-------|
| **ES module** | Almost all new code | Standard, tooling-friendly, shared singleton state |
| **Factory function + closure** | Many independent instances with private state | No `this` pitfalls, easy to compose |
| **Class with `#private`** | Instances with methods and inheritance | Familiar to OOP developers |
| **IIFE module** | Legacy scripts, one-off scopes, browser `<script>` tags without a bundler | Largely replaced by ES modules |

## Namespacing and avoiding globals

The older purpose of the pattern was avoiding global pollution. Compare:

```js
// Many globals
var userName, userAge;
function saveUser() {}
function loadUser() {}

// One global namespace (legacy approach)
var MyApp = MyApp || {};
MyApp.user = (function () {
  /* ... */
  return { save() {}, load() {} };
})();
```

With ES modules you do not need a global namespace at all: each file imports what it needs.

## Augmenting and extending modules

The classic pattern allowed one module to extend another:

```js
const base = (function () {
  const api = { a() {} };
  return api;
})();

const extended = (function (api) {
  api.b = function () { /* new method */ };
  return api;
})(base);
```

Mutating shared objects makes dependencies hard to follow. Prefer composition: create a new object that spreads or wraps the original.

```js
const extended = { ...base, b() { /* ... */ } };
```

## Testing modules

Private members cannot be tested directly, and that is intentional: test the **public behavior**.

```js
import assert from 'node:assert/strict';
import { createCounter } from './counter.js';

const c = createCounter();
c.increment();
assert.equal(c.value, 1);
```

Module-level state persists between tests in the same process. Expose a `reset()` for tests only if needed, or prefer the factory form so each test creates a fresh instance.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Exposing the internal array or object directly | Callers can mutate private state | Return copies, frozen objects, or methods |
| Overusing module-level mutable state | Hidden coupling; tests interfere | Factory functions, dependency injection |
| Giant modules with unrelated responsibilities | Hard to understand and change | Split by responsibility |
| Barrel files re-exporting everything | Slower builds, poor tree-shaking, circular imports | Small, deliberate public entry points |
| Circular imports between modules | Undefined values at load time | Extract shared code into a third module |
| Using the `_private` naming convention as if it were enforced | Anyone can still access it | `#private` fields or closures |
| Relying on IIFE modules in new code | Verbose and tool-unfriendly | ES modules |
| Returning `this` state through getters that expose mutable objects | Leaks internals | Defensive copies |

```js
// Leaks the internal array
getItems() { return items; }

// Safe
getItems() { return [...items]; }
```

## Key takeaways

- The module pattern provides **private state** and a **public API**
- Closures make it possible: variables in an outer function stay private but remain usable by returned functions
- The revealing variant lists the public API in one place
- ES modules are the modern, standard form: unexported names are private, and a module is evaluated once
- Use factory functions for multiple instances and `#private` class fields for class-based designs
- Return copies or safe views so private data does not leak

**Next:** [Factory Pattern](./02_factory-pattern.md)
