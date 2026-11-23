# Prototype Pollution in JavaScript

> Prototypal inheritance vulnerabilities, recursive merge exploitation, Remote Code Execution gadget chains, and architectural defenses.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Prototypes and Prototype Chain](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/03_prototypes-and-prototype-chain.md), [Object Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/03_objects-and-arrays/01_object-fundamentals.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, Node.js & Evergreen Browsers |

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

**Prototype Pollution** is a critical security vulnerability unique to JavaScript and prototypal inheritance languages. It occurs when an attacker tricks an application into injecting or modifying properties on `Object.prototype`.

Because virtually all plain JavaScript objects inherit from `Object.prototype`, polluting this shared root prototype instantly propagates the attacker's injected properties across **every existing and future object** in the runtime.

The fallout of Prototype Pollution can be catastrophic:
- **Authorization Bypass**: Injecting `{ "isAdmin": true }` into `Object.prototype` causes authorization checks (`if (user.isAdmin)`) to evaluate to `true` for all unprivileged users.
- **Denial of Service (DoS)**: Overwriting core prototype methods like `toString` or `valueOf` causes application-wide crashes.
- **Remote Code Execution (RCE)**: When polluted properties match configuration flags read by native sub-process functions (such as `child_process.fork()` reading `NODE_OPTIONS` or `shell`), attackers can achieve arbitrary code execution on the host server.

---

## 2. Core Concepts

### Prototypal Traversal Mechanics
When JavaScript code attempts to access a property on an object (e.g. `user.role`):
1. V8 checks if `user` possesses its own property named `role`.
2. If not found, V8 follows the internal `[[Prototype]]` pointer up to `user.__proto__` (`Object.prototype`).
3. If `Object.prototype.role` has been polluted, the engine returns that value instead of `undefined`.

### The Vulnerable Injection Vectors
Prototype pollution almost always occurs in three patterns when processing untrusted JSON inputs:
1. **Recursive Object Merging (`deepMerge`)**: Functions that recursively copy nested properties without checking for reserved property keys.
2. **Path-Based Object Setters (`set(obj, "a.b.c", val)`)**: Utilities that dynamically split dot-delimited paths and assign values without sanitization.
3. **Deep Cloning Utilities**: Custom cloning routines that traverse `__proto__` or `constructor.prototype`.

### Attack Payload Keys
Attackers exploit two primary entry points:
- `__proto__`: Directly points to the object's prototype (`Object.prototype`).
- `constructor.prototype`: Reaches `Object.prototype` via the constructor reference: `obj.constructor.prototype`.

---

## 3. How It Works Internally

```
ATTACK PAYLOAD (JSON parsed from network):
{
  "__proto__": {
    "isAdmin": true
  }
}
                         |
                         | Unsanitized recursive merge into target object
                         v
+--------------------------------------------------------------------------+
| UNSAFE DEEP MERGE EXECUTION                                              |
| target['__proto__']['isAdmin'] = true;                                   |
| (Traverses pointer and mutates Object.prototype directly in V8 heap!)    |
+------------------------------------+-------------------------------------+
                                     |
                                     v
+--------------------------------------------------------------------------+
| V8 HEAP: Object.prototype (Shared by ALL objects in this runtime)        |
| - toString: [Function]                                                   |
| - hasOwnProperty: [Function]                                             |
| - isAdmin: true  <========================== POLLUTED!                  |
+------------------------------------+-------------------------------------+
                                     |
                                     v
UNSUSPECTING REGULAR OBJECT CREATION:
const regularUser = {};
console.log(regularUser.isAdmin); // TRUE! Privilege Escalation!
```

---

## 4. Syntax & API Reference

### Demonstrating Prototype Pollution
```javascript
// A vulnerable deep merge utility
function unsafeMerge(target, source) {
  for (const key of Object.keys(source)) {
    if (typeof source[key] === 'object' && source[key] !== null) {
      if (!target[key]) target[key] = {};
      unsafeMerge(target[key], source[key]);
    } else {
      target[key] = source[key];
    }
  }
  return target;
}

// Attacker supplies parsed JSON payload
const maliciousPayload = JSON.parse('{"__proto__": {"polluted": "pwned"}}');
unsafeMerge({}, maliciousPayload);

// Every object in the process is now compromised:
const emptyObject = {};
console.log(emptyObject.polluted); // 'pwned'!
```

---

## 5. Practical Examples

### Example 1: Secure Deep Merge Implementation (Safe from Pollution)
```javascript
export function safeDeepMerge(target, source) {
  // Guard against null/primitive inputs
  if (typeof target !== 'object' || target === null) return source;
  if (typeof source !== 'object' || source === null) return target;

  for (const key of Object.keys(source)) {
    // CRITICAL: Block prototype pollution attack vectors
    if (key === '__proto__' || key === 'constructor' || key === 'prototype') {
      continue;
    }

    if (
      typeof source[key] === 'object' &&
      source[key] !== null &&
      !Array.isArray(source[key])
    ) {
      if (!target[key] || typeof target[key] !== 'object') {
        target[key] = {};
      }
      safeDeepMerge(target[key], source[key]);
    } else {
      target[key] = source[key];
    }
  }

  return target;
}
```

### Example 2: Prototypeless Objects (`Object.create(null)`)
```javascript
// Objects created with `Object.create(null)` have NO prototype chain!
// Their `[[Prototype]]` is null, meaning they are completely immune to prototype pollution.
export function createSafeDictionary() {
  const dict = Object.create(null);
  dict.user1 = 'Alice';
  return dict;
}

// Even if Object.prototype is polluted:
Object.prototype.pollutedKey = 'danger';

const safeDict = createSafeDictionary();
console.log(safeDict.pollutedKey); // undefined! Protected!
console.log(safeDict.__proto__);    // undefined!
```

### Example 3: Freezing the Root Prototype at Application Startup
```javascript
// One of the most powerful defense-in-depth mitigations:
// Freezing Object.prototype makes it physically impossible to mutate or pollute.
export function hardenRuntime() {
  Object.freeze(Object.prototype);
  Object.freeze(Array.prototype);
  Object.freeze(Function.prototype);
}

// Attempting to pollute now throws in strict mode or silently fails:
hardenRuntime();

try {
  const payload = JSON.parse('{"__proto__":{"hacked":true}}');
  Object.assign({}, payload);
} catch (err) {
  // Mutation blocked!
}

console.log(({}).hacked); // undefined!
```

### Example 4: Safe Property Lookup with `Object.hasOwn()`
```javascript
const user = { name: 'Bob' };

// POLLUTION ATTACK:
Object.prototype.isAdmin = true;

// VULNERABLE CHECK: Evaluates true because it traverses prototype chain!
if (user.isAdmin) {
  console.log('Access granted (Vulnerable!)');
}

// SECURE CHECK: Only evaluates true if property is on the object directly!
if (Object.hasOwn(user, 'isAdmin')) {
  console.log('Access granted (Secure)');
} else {
  console.log('Access denied (Secure)'); // Executes!
}
```

### Example 5: Path-Based Property Setter with Validation
```javascript
export function safeSetPath(obj, path, value) {
  const segments = path.split('.');
  let current = obj;

  for (let i = 0; i < segments.length - 1; i++) {
    const key = segments[i];

    // Reject dangerous path traversal tokens
    if (key === '__proto__' || key === 'constructor' || key === 'prototype') {
      throw new Error(`Dangerous path traversal detected: "${key}"`);
    }

    if (!current[key] || typeof current[key] !== 'object') {
      current[key] = {};
    }
    current = current[key];
  }

  const finalKey = segments[segments.length - 1];
  if (finalKey === '__proto__' || finalKey === 'constructor' || finalKey === 'prototype') {
    throw new Error(`Dangerous property assignment: "${finalKey}"`);
  }

  current[finalKey] = value;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`constructor.prototype` Bypass**: Many naive sanitizers only check for `key !== '__proto__'`. Attackers easily bypass this by submitting `{"constructor": {"prototype": {"polluted": true}}}`. You must block `__proto__`, `constructor`, **and** `prototype`.
2. **Third-Party Dependency Vulnerabilities**: Even if your application code never performs merges, transitive npm dependencies (like vulnerable older versions of `lodash.merge`, `minimist`, or `dot-prop`) may be vulnerable. Always audit dependencies with `npm audit`.
3. **`Object.freeze(Object.prototype)` Side Effects**: While freezing `Object.prototype` completely neutralizes prototype pollution, poorly written third-party libraries that attempt to polyfill or monkey-patch `Object.prototype` at runtime will crash. Test thoroughly before deploying in production.
4. **Node.js `--frozen-intrinsics` Flag**: Node.js provides a built-in flag `node --frozen-intrinsics app.js` that recursively freezes all built-in JavaScript prototypes and constructors at startup.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Checking only `key !== '__proto__'` | Bypassed via `constructor.prototype` | Block `__proto__`, `constructor`, and `prototype` |
| Using plain objects as arbitrary user-keyed dictionaries | Keys like `toString` or polluted properties shadow dictionary lookups | Use native `new Map()` or `Object.create(null)` |
| Checking authorization via `if (user.role)` directly | Prototype pollution injects default fallback roles | Check `Object.hasOwn(user, 'role')` or validate schema |
| Copying objects via unvalidated recursive loops | Traverses prototype references and mutates root prototype | Use `structuredClone()` or validated merge routines |
| Parsing untrusted JSON directly into system settings | Injects unexpected configuration flags | Validate parsed JSON with Zod or JSON Schema |

---

## 8. Performance Characteristics

- **Key Filtering Overhead**: Checking `key === '__proto__'` adds $< 2$ nanoseconds per property in V8.
- **`Map` Lookups**: Using `new Map()` for dynamic user keys avoids prototype chain lookups, executing property lookups at near-instantaneous $O(1)$ speeds without pollution risks.

---

## 9. Engine Internals

1. **Prototype Chain Invalidation**:
   - In V8, when a property is added to `Object.prototype`, V8 must invalidate the Hidden Classes (Shapes) and Inline Caches of **every object in the entire V8 isolate**.
   - Consequently, prototype pollution not only creates a security disaster; it causes massive engine-wide performance deoptimizations, demoting property access across the runtime into slow dictionary mode.

---

## 10. Best Practices & Design Guidelines

- **Use `Map` for User-Generated Keys**: For caches, dictionaries, and key-value tables where keys originate from users, use `new Map()` instead of plain objects.
- **Sanitize Recursive Merging**: Always reject `__proto__`, `constructor`, and `prototype` in merge utilities.
- **Validate Schemas on API Boundaries**: Use schema validators (like Zod, Valibot, or Ajv) to strictly strip unknown properties before passing payloads to business logic.
- **Deploy `--frozen-intrinsics` in Node**: Run mission-critical microservices with `node --frozen-intrinsics`.

---

## 11. Real-World Case Study

### Remote Code Execution via Child Process Prototype Pollution
```javascript
// THE SCENARIO: An express endpoint accepted JSON configuration and merged it unsafely:
// const config = safeDeepMerge({}, req.body);
//
// Elsewhere in the backend, an admin task executed a script:
// child_process.fork('./worker.js');

// THE EXPLOIT:
// An attacker posted:
// {
//   "__proto__": {
//     "execArgv": ["--eval=require('child_process').execSync('rm -rf /')"]
//   }
// }
//
// Because `child_process.fork()` checks `options.execArgv || process.execArgv`,
// and `options` is an empty object `{}`, it traversed the prototype chain,
// found the polluted `execArgv` array, and executed the attacker's shell code!

// THE REMEDY:
// 1. Sanitize all deep merges.
// 2. Pass explicit, frozen options objects to child_process primitives:
// child_process.fork('./worker.js', [], { execArgv: [] });
```

---

## 12. Interview Questions

<details>
<summary>1. What is Prototype Pollution and why is it unique to JavaScript?</summary>
Prototype Pollution is a vulnerability where an attacker injects properties into <code>Object.prototype</code>. It is unique to JavaScript because of its prototypal inheritance model: almost all objects inherit from a single shared prototype object. Modifying properties on <code>Object.prototype</code> instantly alters the behavior and attributes of every plain object across the entire runtime.
</details>

<details>
<summary>2. What are the two primary property paths used to execute a prototype pollution attack?</summary>
1. <code>__proto__</code>: Directly references the object's prototype (<code>Object.prototype</code>).
2. <code>constructor.prototype</code>: Navigates through the object's constructor reference to access the constructor's prototype property.
</details>

<details>
<summary>3. How does Object.create(null) prevent prototype pollution?</summary>
<code>Object.create(null)</code> creates an object whose internal <code>[[Prototype]]</code> is <code>null</code>. Because it has no prototype chain, it does not inherit from <code>Object.prototype</code>, making it completely immune to prototype pollution lookups.
</details>

<details>
<summary>4. Why is Object.hasOwn() safer than accessing properties directly on an object?</summary>
Accessing a property directly (<code>if (user.isAdmin)</code>) traverses the entire prototype chain. If <code>Object.prototype.isAdmin</code> has been polluted, the check evaluates to <code>true</code>. <code>Object.hasOwn(user, 'isAdmin')</code> verifies that the property exists directly on the <code>user</code> object instance itself, ignoring inherited prototype properties.
</details>

<details>
<summary>5. How can Prototype Pollution lead to Remote Code Execution (RCE) in Node.js?</summary>
By polluting properties that are used as configuration options in Node core modules. For example, polluting <code>shell</code>, <code>NODE_OPTIONS</code>, or <code>execArgv</code> causes subsequent invocations of <code>child_process.spawn()</code> or <code>child_process.fork()</code> to execute arbitrary shell commands specified in the polluted prototype.
</details>

<details>
<summary>6. What does Node.js's --frozen-intrinsics flag do?</summary>
It recursively freezes all built-in JavaScript objects, prototypes, and constructors (such as <code>Object.prototype</code>, <code>Array.prototype</code>, <code>Function.prototype</code>) at application startup, making it impossible to mutate or pollute them during execution.
</details>

<details>
<summary>7. Why should you avoid using plain objects as dynamic dictionaries for untrusted keys?</summary>
Plain objects inherit properties from <code>Object.prototype</code> (like <code>toString</code>, <code>valueOf</code>, <code>constructor</code>). If an attacker supplies a key like <code>"toString"</code>, accessing <code>dict[key]</code> returns a function rather than undefined. Furthermore, if the prototype is polluted, malicious keys appear in the dictionary. Use <code>new Map()</code> instead.
</details>

<details>
<summary>8. How does Object.freeze(Object.prototype) affect third-party libraries?</summary>
While it completely neutralizes prototype pollution, it can break older or poorly designed libraries that attempt to polyfill missing features or attach custom utility methods to <code>Object.prototype</code> or <code>Array.prototype</code> at runtime.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Build a Prototype Pollution Vulnerability Scanner
Write a function `detectPrototypePollution()` that audits the global runtime environment and returns a list of any unexpected enumerable or non-enumerable properties that have been injected into `Object.prototype`.

<details>
<summary>View Solution</summary>

```javascript
export function detectPrototypePollution() {
  const standardObjectProps = new Set([
    'constructor',
    '__defineGetter__',
    '__defineSetter__',
    'hasOwnProperty',
    '__lookupGetter__',
    '__lookupSetter__',
    'isPrototypeOf',
    'propertyIsEnumerable',
    'toString',
    'valueOf',
    '__proto__',
    'toLocaleString',
  ]);

  const currentProps = Object.getOwnPropertyNames(Object.prototype);
  const polluted = currentProps.filter(prop => !standardObjectProps.has(prop));

  if (polluted.length > 0) {
    console.error(`[CRITICAL] Prototype Pollution Detected! Injected keys:`, polluted);
    return { isCompromised: true, pollutedKeys: polluted };
  }

  return { isCompromised: false, pollutedKeys: [] };
}
```
</details>

### Exercise 2: Sanitized Recursive Object Cloner
Implement a deep clone utility `safeDeepClone(source)` that recursively duplicates nested objects and arrays while stripping any `__proto__`, `constructor`, or `prototype` keys from the clone.

<details>
<summary>View Solution</summary>

```javascript
export function safeDeepClone(value) {
  if (value === null || typeof value !== 'object') {
    return value;
  }

  if (Array.isArray(value)) {
    return value.map(item => safeDeepClone(item));
  }

  const clone = {};
  for (const key of Object.keys(value)) {
    if (key === '__proto__' || key === 'constructor' || key === 'prototype') {
      continue; // Strip pollution vectors
    }
    clone[key] = safeDeepClone(value[key]);
  }

  return clone;
}
```
</details>
