# CommonJS (CJS)

> CommonJS is the foundational synchronous module system developed for Node.js server environments, governed by module wrapper functions, dynamic `require()` execution, and object-based export caching.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [Functions & Scope](../02_functions/01_function-declarations.md), [Execution Context](../04_scope-and-execution/01_execution-context.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **Standard** | Node.js CommonJS Module Specification / ServerJS Legacy Standard |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

When Node.js was created in 2009 by Ryan Dahl, JavaScript had no standard module system built into the language. To enable server-side software development—where codebases span hundreds of files interacting with the filesystem and databases—Node.js adopted the **CommonJS (CJS)** module specification (originally created by the ServerJS group).

CommonJS was engineered specifically for server environments where module files reside on local disk drives. Because reading from local disk is orders of magnitude faster than fetching across the internet, CommonJS is strictly **synchronous**: calling `require('./utils')` halts JavaScript execution until the file is read from disk, compiled, executed, and its exported object is returned. Although modern development is transitioning to ES Modules, millions of npm packages and enterprise backend systems still rely fundamentally on CommonJS.

---

## 2. Core Concepts

### Synchronous Runtime Execution
In CommonJS, `require()` is an ordinary JavaScript runtime function. You can call `require()` anywhere: inside functions, `if` statements, or loops. There is no separate compile-time parsing phase for module dependencies; modules are loaded, executed, and evaluated synchronously at the exact line where `require()` is called.

### `module.exports` vs `exports`
Every CommonJS module is provided with a `module` object containing an `exports` property. Node.js also provides a convenience shorthand variable named `exports` that points to `module.exports`:

```javascript
// At the top of every CJS module, Node implicitly initializes:
let exports = module.exports = {};
```

- Mutating `exports` (e.g., `exports.add = fn`) adds properties to `module.exports`.
- Reassigning `exports` (e.g., `exports = fn`) **breaks the reference link** to `module.exports`. What is actually returned to consumers by `require()` is *always* `module.exports`.

```text
Initial State:
 module.exports ─────┐
                     ├──> { } (Shared Object)
        exports ─────┘

Mutating: exports.foo = 1
 module.exports ─────┐
                     ├──> { foo: 1 } (Shared Object)
        exports ─────┘

Reassigning: exports = function() {}
 module.exports ─────────> { } (Returned by require!)
        exports ─────────> [Function] (Lost reference! Discarded!)
```

### Module Caching (`require.cache`)
Modules in CommonJS are cached upon their first execution. If a module is required multiple times across different files, Node executes the module code only once. Subsequent `require()` calls return the cached `module.exports` object from `require.cache`.

---

## 3. How It Works Internally

### The Module Wrapper Function
When Node.js loads a CommonJS file, it does not execute the raw source code directly. Instead, it reads the file contents as a string and wraps it inside an invisible function wrapper:

```javascript
(function (exports, require, module, __filename, __dirname) {
  // YOUR MODULE CODE RESIDES HERE IN THIS CLOSURE
});
```

This explains why:
- Variables declared with `var`, `let`, or `const` at the top level are scoped to the file, not leaked to global scope.
- Variables like `__filename`, `__dirname`, `require`, and `module` appear to be globals, but are actually local function parameters passed into the wrapper closure.

```
 User calls: require('./math.js')
                   |
                   v
 [1] Resolve absolute file path: /app/math.js
                   |
                   v
 [2] Check require.cache['/app/math.js']
     If found, return module.exports immediately!
                   |
             NOT CACHED
                   v
 [3] Create new Module instance:
     const module = { exports: {}, id: '/app/math.js', loaded: false };
     Store in require.cache['/app/math.js'] = module;
                   |
                   v
 [4] Read source text from disk synchronously: fs.readFileSync(...)
                   |
                   v
 [5] Wrap in Module Wrapper Function:
     const wrapper = Module.wrap(sourceText);
                   |
                   v
 [6] Compile and execute wrapper in current V8 context:
     compiledFn.call(module.exports, module.exports, require, module, __filename, __dirname);
                   |
                   v
 [7] Mark module.loaded = true;
     Return module.exports to caller.
```

---

## 4. Syntax & API Reference

### Exporting Syntax

```javascript
// 1. Exporting multiple named properties
exports.add = (a, b) => a + b;
exports.subtract = (a, b) => a - b;

// 2. Exporting a single function, class, or object (REPLACE module.exports)
module.exports = class Database {
  connect() { /* ... */ }
};

// 3. Exporting a factory object
module.exports = {
  version: '1.0.0',
  createUser(name) { return { id: 1, name }; },
};
```

### Importing Syntax

```javascript
// 1. Importing complete module
const fs = require('node:fs');
const math = require('./math');

// 2. Destructuring imports
const { add, subtract } = require('./math');

// 3. Conditional / Dynamic importing
let logger;
if (process.env.NODE_ENV === 'production') {
  logger = require('./prod-logger');
} else {
  logger = require('./dev-logger');
}

// 4. Accessing and clearing module cache
delete require.cache[require.resolve('./config')]; // Force re-execution on next require
```

---

## 5. Practical Examples

### Example 1: `module.exports` vs `exports` Assignment Pitfall
```javascript
// logger.js (INCORRECT: Reassigning exports directly)
exports = function log(msg) {
  console.log(`[LOG]: ${msg}`);
};
// Consumer app.js:
// const logger = require('./logger');
// logger('test'); // TypeError: logger is not a function! (returns empty object {})

// logger-fixed.js (CORRECT)
module.exports = function log(msg) {
  console.log(`[LOG]: ${msg}`);
};
// Consumer app.js:
// const logger = require('./logger-fixed');
// logger('test'); // Logs: [LOG]: test
```

### Example 2: Inspecting and Manipulating `require.cache`
```javascript
// config-reader.js
const fs = require('node:fs');

function loadConfig() {
  const configPath = require.resolve('./app-config.json');

  // Purge the cache entry to force fresh disk reload
  delete require.cache[configPath];

  return require(configPath);
}

module.exports = { loadConfig };
```

### Example 3: Circular Dependency Resolution in CJS
```javascript
// a.js
exports.done = false;
const b = require('./b');
console.log('In a.js, b.done =', b.done);
exports.done = true;
console.log('a.js completed');

// b.js
exports.done = false;
const a = require('./a');
console.log('In b.js, a.done =', a.done); // Returns partial export! (false)
exports.done = true;
console.log('b.js completed');

// main.js
const a = require('./a');
// Output:
// In b.js, a.done = false   <-- b receives incomplete/partial module.exports of a
// b.js completed
// In a.js, b.done = true    <-- a receives fully evaluated exports of b
// a.js completed
```

### Example 4: Using `require.resolve()` for Path Verification
```javascript
function isPackageInstalled(packageName) {
  try {
    require.resolve(packageName);
    return true;
  } catch (err) {
    if (err.code === 'MODULE_NOT_FOUND') {
      return false;
    }
    throw err;
  }
}

console.log('Is lodash installed?', isPackageInstalled('lodash'));
```

### Example 5: Plugin Registration Pattern
```javascript
// plugin-manager.js
class PluginManager {
  #plugins = new Map();

  register(modulePath) {
    // Resolve absolute path and require dynamically
    const resolvedPath = require.resolve(modulePath);
    const pluginFactory = require(resolvedPath);

    if (typeof pluginFactory !== 'function') {
      throw new Error(`Plugin at ${modulePath} must export a factory function`);
    }

    const instance = pluginFactory();
    this.#plugins.set(instance.name, instance);
  }

  execute(name, ...args) {
    const plugin = this.#plugins.get(name);
    if (!plugin) throw new Error(`Plugin ${name} not found`);
    return plugin.run(...args);
  }
}

module.exports = PluginManager;
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Reassigning `exports` Disconnects It**: Setting `exports = fn` discards the reference to `module.exports`. Always write `module.exports = fn`.
2. **Circular Dependencies Return Incomplete Copies**: Unlike ESM (which uses live bindings and TDZ errors), CommonJS resolves circular references by returning whatever partial properties were attached to `module.exports` at the moment of the circular call.
3. **Synchronous File Blocking**: `require()` executes synchronously using blocking filesystem I/O (`readFileSync`). Calling `require()` inside high-traffic request handlers degrades server throughput.
4. **Resolution Algorithm Traversal**: `require('pkg')` searches the current directory's `node_modules`, then traverses up every parent directory up to the filesystem root (`/node_modules`), creating disk lookup overhead on deep folder hierarchies.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Reassigning `exports`** | `exports = function() {}` | `module.exports = function() {}` | `exports` loses its reference to `module.exports`. |
| **Mixing `exports` and `module.exports`** | `module.exports = fn; exports.a = 1;` | `module.exports = { fn, a: 1 };` | `exports.a` is attached to old object, discarded when `module.exports` was reassigned. |
| **Calling `require()` in Request Handlers**| `app.get('/', (req, res) => { require('./handler'); })` | Require all dependencies at top of file | Blocks the Node.js event loop with synchronous filesystem reads. |
| **Expecting Live Value Updates** | Modifying exported scalar variable inside module | Export an object or getter method | CJS copies primitive values upon require; changes are not reflected. |
| **Mutating Cached Modules Globally** | Modifying imported config object directly | Clone objects or export immutable getters | Mutating a required module alters state for all other modules in the process. |
| **Using `import` syntax in CJS file** | `import fs from 'fs'` in `.cjs` file | Use `const fs = require('fs')` | SyntaxError: Cannot use import statement outside a module. |

---

## 8. Performance Characteristics

- **First-Load Latency**: The initial `require()` call incurs synchronous disk I/O, parsing, compilation, and execution.
- **Cache Hit Overhead**: Subsequent `require()` calls for the same path bypass disk reads and compilation completely, performing a fast $O(1)$ dictionary lookup in `require.cache`.
- **Tree-Shaking Inability**: Because CJS modules export dynamic JavaScript objects whose properties can be modified at runtime, bundlers cannot reliably tree-shake unused exports without conservative bailouts.

---

## 9. Engine Internals

1. **`Module._load` Execution**: In Node.js source (`lib/internal/modules/cjs/loader.js`), `require(path)` invokes `Module._load(request, parent, isMain)`.
2. **Resolution Algorithm**:
   - Core modules (`node:fs`, `node:path`) are loaded immediately from memory.
   - If path starts with `./` or `/`, it attempts matching `.js`, `.json`, `.node`, or directory `index.js`.
   - If bare specifier (`lodash`), it searches candidate `node_modules` paths up the filesystem tree.
3. **The Wrapper Closure**: The string source is wrapped via `Module.wrap()` and compiled into a V8 `Script` using `vm.runInThisContext()`.

---

## 10. Best Practices & Design Guidelines

1. **Keep `module.exports` Consistent**: Choose either named exports on `module.exports` (`module.exports.x = ...`) or export a single primary entity (`module.exports = ClassName`).
2. **Never Call `require()` in Critical Paths**: Hoist all `require()` calls to the top of files to execute them during process startup.
3. **Avoid Mutating `require.cache` in Production**: Modifying `require.cache` introduces unpredictable memory leaks and state desynchronization.
4. **Use Explicit Prefixes for Built-ins**: Always use the `node:` prefix (`require('node:fs')`) to prevent typosquatting and improve resolution performance.

---

## 11. Real-World Case Study

### Hot-Reloading Configuration Daemon in Enterprise Microservice

A backend microservice reads JSON or JavaScript configuration files that operations teams can update on disk without restarting the entire server process.

```javascript
// dynamic-config-loader.js
const path = require('node:path');
const fs = require('node:fs');

class ConfigReloader {
  #configPath;
  #currentConfig;

  constructor(relativeConfigPath) {
    this.#configPath = path.resolve(__dirname, relativeConfigPath);
    this.#load();
    this.#watch();
  }

  #load() {
    try {
      // 1. Resolve absolute path
      const resolved = require.resolve(this.#configPath);

      // 2. Invalidate cache
      delete require.cache[resolved];

      // 3. Re-require fresh module
      this.#currentConfig = require(resolved);
      console.log(`[Config] Successfully loaded configuration from ${this.#configPath}`);
    } catch (err) {
      console.error(`[Config] Failed to reload configuration:`, err.message);
    }
  }

  #watch() {
    fs.watchFile(this.#configPath, { interval: 1000 }, (curr, prev) => {
      if (curr.mtime !== prev.mtime) {
        console.log(`[Config] Change detected on disk. Hot-reloading...`);
        this.#load();
      }
    });
  }

  get(key) {
    return this.#currentConfig ? this.#currentConfig[key] : undefined;
  }
}

module.exports = ConfigReloader;
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is the Module Wrapper Function in Node.js, and what parameters does it supply?</strong></summary>

It is an invisible function wrapper that Node.js places around every CommonJS module before executing it:
`function (exports, require, module, __filename, __dirname) { ... }`. It isolates the module's scope from the global namespace and supplies module-specific utilities.
</details>

<details>
<summary><strong>2. What is the difference between <code>exports</code> and <code>module.exports</code>?</strong></summary>

`module.exports` is the actual object returned by `require()`. `exports` is merely a variable initialized to reference `module.exports`. If you mutate properties on `exports`, `module.exports` reflects them. If you reassign `exports` (`exports = function() {}`), you break the reference link, and `require()` will still return `module.exports`.
</details>

<details>
<summary><strong>3. How does CommonJS handle circular dependencies?</strong></summary>

CommonJS does not throw an error on circular dependencies. Instead, when module $B$ requires module $A$ while $A$ is in the middle of executing, $B$ receives a reference to whatever properties have been attached to $A$'s `module.exports` up to that point. The exports received may be incomplete.
</details>

<details>
<summary><strong>4. Why is CommonJS unsuitable for native in-browser execution without a bundler?</strong></summary>

CommonJS relies on synchronous `require()`, which blocks thread execution while reading from disk. Over the web, fetching modules synchronously across HTTP would freeze browser rendering and script execution.
</details>

<details>
<summary><strong>5. What is <code>require.cache</code>, and how can it be cleared?</strong></summary>

`require.cache` is an in-memory dictionary mapping resolved absolute file paths to their `Module` instances. To clear the cache for a module and force Node to re-execute it on the next `require`, use: `delete require.cache[require.resolve('./my-module')]`.
</details>

<details>
<summary><strong>6. What is the return value of <code>require.resolve()</code>?</strong></summary>

It returns the exact absolute filesystem path that would be loaded by `require()`, following all directory and extension resolution rules, without executing the module.
</details>

<details>
<summary><strong>7. Are CommonJS module exports live bindings?</strong></summary>

No. CommonJS exports are evaluated object properties. Once a primitive value is exported, changing the local variable inside the exporting module has no effect on the value already imported by the consumer.
</details>

<details>
<summary><strong>8. Why is tree-shaking ineffective for CommonJS modules?</strong></summary>

Because CommonJS modules can conditionally export properties, mutate `module.exports` dynamically at runtime, and allow dynamic `require(variable)` imports. Build tools cannot statically prove that an export will never be accessed at runtime.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: The Broken Exports Refactor
A junior developer wrote the following CommonJS utility module, but callers report that `require('./auth')` returns an empty object `{}`. Explain why and fix it.

```javascript
// auth.js
const users = ['Alice', 'Bob'];

function authenticate(user) {
  return users.includes(user);
}

exports = authenticate;
```

<details>
<summary>View Solution & Explanation</summary>

**Explanation:**
Assigning directly to `exports` (`exports = authenticate`) merely breaks the local variable reference to `module.exports`. Node's `require()` returns `module.exports`, which remains an empty object `{}`.

**Fix:**
```javascript
// auth.js
const users = ['Alice', 'Bob'];

function authenticate(user) {
  return users.includes(user);
}

module.exports = authenticate;
```
</details>

### Challenge 2: Implement a Miniature `require` Function
Write a simplified version of Node's `miniRequire(filePath)` using Node's `fs` and `vm` core modules that implements the module wrapper and basic caching.

<details>
<summary>View Solution</summary>

```javascript
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const cache = new Map();

function miniRequire(relativeFilePath) {
  const absolutePath = path.resolve(__dirname, relativeFilePath);

  if (cache.has(absolutePath)) {
    return cache.get(absolutePath).exports;
  }

  const code = fs.readFileSync(absolutePath, 'utf8');

  const module = {
    exports: {},
    id: absolutePath,
  };

  cache.set(absolutePath, module);

  const wrapper = [
    '(function (exports, require, module, __filename, __dirname) {',
    code,
    '\n});',
  ].join('');

  const compiledScript = new vm.Script(wrapper, { filename: absolutePath });
  const wrapperFn = compiledScript.runInThisContext();

  wrapperFn.call(
    module.exports,
    module.exports,
    miniRequire,
    module,
    absolutePath,
    path.dirname(absolutePath)
  );

  return module.exports;
}
```
</details>
