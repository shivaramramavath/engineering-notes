# JavaScript Prototypes & The Prototype Chain: Delegation, Shapes & Security

> Comprehensive exploration of JavaScript's prototypal inheritance model, prototype chain traversal, `Object.create`, prototype pollution vulnerabilities, and V8 Hidden Class optimizations.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [03_objects-and-arrays/01_objects.md](../03_objects-and-arrays/01_objects.md), [05_this-and-oop/01_this.md](./01_this.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES1 (1997) / ES5 (`Object.create`, `getPrototypeOf`) / ES2015 (`Object.setPrototypeOf`) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Delegation over Class Copying](#2-core-concepts-delegation-over-class-copying)
3. [How It Works: The [[Prototype]] Chain Architecture](#3-how-it-works-the-prototype-chain-architecture)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Prototype Transitions & Inline Caches](#9-engine-internals-v8-prototype-transitions--inline-caches)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Secure Deep Merge Defending Against Prototype Pollution](#11-real-world-case-study-secure-deep-merge-defending-against-prototype-pollution)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Unlike classical object-oriented languages such as Java or C++—where classes act as static blueprints that copy behavior into distinct instances—JavaScript employs a **prototypal delegation model**. In JavaScript, objects inherit directly from other objects through an internal link known as the prototype chain.

When a property or method is requested on an object, the runtime engine looks at the object's own properties first. If the property is absent, it walks up the prototype chain until the property is found or the root (`null`) is reached.

Understanding prototypes is essential for mastering JavaScript memory optimization, implementing clean class hierarchies, diagnosing shadowing bugs, and securing applications against **Prototype Pollution** vulnerabilities.

---

## 2. Core Concepts: Delegation over Class Copying

```
                            Classical vs Prototypal
                                       │
         ┌─────────────────────────────┴─────────────────────────────┐
         ▼                                                           ▼
   Classical OOP (Java, C++)                                   Prototypal OOP (JavaScript)
   • Class is a static blueprint                               • Objects delegate directly to objects
   • Methods/fields are copied into instances                  • Methods are shared via live prototype links
   • Modification requires re-instantiation                    • Live mutation of prototype affects all instances
```

### Essential Distinctions: `prototype` vs `[[Prototype]]` vs `__proto__`
- **`[[Prototype]]`**: The internal, hidden specification slot on an object pointing to its parent prototype.
- **`Function.prototype`**: A public property present on constructor functions (e.g. `Array.prototype`, `Object.prototype`). When a function is called with `new`, the newly instantiated object's `[[Prototype]]` is set to point to this object.
- **`__proto__`**: The legacy accessor property on `Object.prototype` exposing the `[[Prototype]]` slot. Modern code should use `Object.getPrototypeOf()` and `Object.setPrototypeOf()` instead.

---

## 3. How It Works: The [[Prototype]] Chain Architecture

```
Property Lookup: child.greet()
┌────────────────────────────────────────────────────────┐
│                      child Object                      │
│ Own Properties: { name: "Alice" }                      │
│ [[Prototype]] Pointer ─────────────────────────────────┼──┐
└────────────────────────────────────────────────────────┘  │
                                                            │
┌────────────────────────────────────────────────────────┐  │
│                     parent Object                      │◄─┘
│ Own Properties: { greet() { return "Hi!" } }           │
│ [[Prototype]] Pointer ─────────────────────────────────┼──┐
└────────────────────────────────────────────────────────┘  │
                                                            │
┌────────────────────────────────────────────────────────┐  │
│                    Object.prototype                    │◄─┘
│ Own Properties: { toString(), hasOwnProperty(), ... }  │
│ [[Prototype]] Pointer ──► null (End of Chain)          │
└────────────────────────────────────────────────────────┘
```

1. **Step 1**: Check `child` for own property `greet`. (Not found).
2. **Step 2**: Traverse `child.[[Prototype]]` to `parent`. Check `parent` for `greet`. (Found!).
3. **Step 3**: Invoke `parent.greet()` with `this` bound to `child` (the call receiver).
4. If `parent` did not have `greet`, traversal would continue to `Object.prototype`, and finally fail with `undefined` (or throw `TypeError: child.greet is not a function`).

---

## 4. Syntax & API Reference

### Modern Standard APIs
- `Object.create(proto, descriptors?)`: Creates a new object with specified `[[Prototype]]`.
- `Object.getPrototypeOf(obj)`: Reads the internal `[[Prototype]]` of `obj`.
- `Object.setPrototypeOf(obj, proto)`: Modifies the `[[Prototype]]` of an existing object (use with caution: deoptimizes engine shapes).
- `protoObj.isPrototypeOf(targetObj)`: Returns boolean indicating whether `protoObj` exists anywhere in `targetObj`'s prototype chain.
- `Object.prototype.hasOwnProperty(prop)` / `Object.hasOwn(obj, prop)`: Checks whether `prop` is an **own** property, bypassing the prototype chain.

---

## 5. Practical Examples

### Example 1: Basic Prototypal Inheritance via Object.create
```javascript
const animal = {
  isAlive: true,
  makeSound() {
    return `${this.name} makes a generic sound`;
  }
};

// Create dog delegating to animal
const dog = Object.create(animal);
dog.name = "Rex";
dog.bark = function () {
  return `${this.name} says Woof!`;
};

console.log(dog.bark());      // "Rex says Woof!" (Own property)
console.log(dog.makeSound()); // "Rex makes a generic sound" (Delegated to animal)
console.log(dog.isAlive);     // true (Delegated to animal)
console.log(Object.getPrototypeOf(dog) === animal); // true
```

### Example 2: Real-World Safe Dictionaries with `Object.create(null)`
```javascript
// Plain objects inherit from Object.prototype:
const standardDict = {};
console.log("toString" in standardDict); // true (Leaked from Object.prototype!)

// Prototype-free dictionary:
const cleanDict = Object.create(null);
console.log(Object.getPrototypeOf(cleanDict)); // null
console.log("toString" in cleanDict);         // false
console.log("constructor" in cleanDict);      // false

// Safe from prototype pollution and unwanted prototype property collisions
cleanDict["user_count"] = 42;
console.log(cleanDict["user_count"]); // 42
```

### Example 3: Edge Case with Property Shadowing
```javascript
const vehicle = {
  wheels: 4,
  accelerate() {
    this.speed = (this.speed || 0) + 10;
  }
};

const sportsCar = Object.create(vehicle);
sportsCar.speed = 100; // Shadows prototype lookup

// Invoking method sets property on sportsCar (this), not vehicle!
sportsCar.accelerate();

console.log("Car speed:", sportsCar.speed);       // 110 (Own property)
console.log("Vehicle speed:", vehicle.speed);     // undefined (Prototype was not mutated!)
console.log(Object.hasOwn(sportsCar, "speed"));   // true
```

### Example 4: Failure Mode with Dangerous Prototype Mutation
```javascript
// Failure mode: Mutating a shared prototype affects EVERY instance in the application
const baseService = {
  timeout: 5000
};

const serviceA = Object.create(baseService);
const serviceB = Object.create(baseService);

// Bug: Mutating property directly on prototype
Object.getPrototypeOf(serviceA).timeout = 1000;

console.log(serviceB.timeout); // 1000! Accidental side effect in serviceB!
```

### Example 5: Idiomatic Prototype Chain Walk
```javascript
function getPrototypeChain(obj) {
  const chain = [];
  let current = obj;

  while (current !== null) {
    chain.push(current);
    current = Object.getPrototypeOf(current);
  }

  return chain;
}

const list = [1, 2, 3];
const chain = getPrototypeChain(list);

console.log(chain.map(o => o.constructor?.name || "null prototype"));
// Output: [ 'Array', 'Array', 'Object' ] (list -> Array.prototype -> Object.prototype)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`Object.setPrototypeOf` Causes Severe Deoptimization**: Modifying an object's prototype after instantiation deoptimizes all V8 inline caches associated with that object shape. Always use `Object.create()` during creation instead.
2. **Assigning Through Read-Only Prototype Properties**: If a prototype defines a property with `writable: false`, attempting assignment on a child object (`child.prop = 42`) throws a `TypeError` in strict mode or fails silently in sloppy mode, rather than creating an own shadowing property!
3. **`in` Operator vs `Object.hasOwn()`**: `'prop' in obj` traverses the entire prototype chain. `Object.hasOwn(obj, 'prop')` checks only own properties.
4. **Circular Prototype Chains Prohibited**: Calling `Object.setPrototypeOf(a, b)` when `a` is already in `b`'s prototype chain throws `TypeError: Cyclic __proto__ value`.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using legacy `__proto__` | `obj.__proto__ = proto` | `Object.create(proto)` | `__proto__` is legacy, non-standard in ECMAScript core, and slow. |
| Using `setPrototypeOf` at runtime | `Object.setPrototypeOf(o, p)` | `const o = Object.create(p)` | Deoptimizes V8 Hidden Classes and Inline Caches. |
| Mutating `Object.prototype` | `Object.prototype.custom = fn` | Use independent utility modules | Pollutes every object in runtime; causes library clashes. |
| Assuming plain object is empty | `for (let k in {})` | `Object.create(null)` | Plain objects inherit methods like `toString` and `isPrototypeOf`. |
| Shadowing with read-only prototype | Inheriting from `freeze()` object | Override via `Object.defineProperty` | Direct assignment fails if prototype property is `writable: false`. |
| Vulnerable deep merge routines | Blindly copying nested keys | Block `__proto__` and `constructor` | Leaves application vulnerable to Prototype Pollution attacks. |

---

## 8. Performance Characteristics

- **Own Property Reads**: $O(1)$ fast indexed read via V8 Hidden Class in-object offsets.
- **Prototype Property Reads**: $O(k)$ where $k$ is the depth of the prototype chain. However, TurboFan JIT caches the resolved prototype slot in the call-site's **Inline Cache (IC)**, making repeated lookups as fast as own property reads.
- **Prototype Invalidation**: Mutating any prototype along an active chain invalidates all Inline Caches across all functions referencing that prototype, causing JIT deoptimization.

---

## 9. Engine Internals: V8 Prototype Transitions & Inline Caches

Inside Google V8:
1. **Map (Hidden Class) Prototype Slot**: Every V8 `Map` contains a direct pointer to the object's `prototype`. If an object's prototype is changed via `Object.setPrototypeOf()`, V8 must allocate a completely new `Map`.
2. **Prototype Validity Cell**: V8 attaches a `PrototypeValidityCell` to the root `Map`. When any property is added, deleted, or modified on a prototype object, the engine invalidates the validity cell.
3. This invalidation forces TurboFan to discard optimized machine code for all methods that made assumptions about that prototype, reverting them to unoptimized Ignition bytecode.

---

## 10. Best Practices & Design Guidelines

1. **Use `Object.create(null)` for Key-Value Hash Maps**: When building lookups from user input, eliminate the prototype to prevent property injection attacks.
2. **Never Mutate Built-in Prototypes**: Modifying `Array.prototype` or `Object.prototype` is strictly prohibited in production software.
3. **Prefer ES6 `class` Syntax Over Manual Prototype Wiring**: Classes provide cleaner syntax while adhering to optimal engine prototype structures under the hood.

---

## 11. Real-World Case Study: Secure Deep Merge Defending Against Prototype Pollution

**Prototype Pollution** occurs when an attacker injects properties into `Object.prototype` via insecure recursive object merging, compromising the entire Node.js runtime.

```javascript
// Secure Deep Merge Function Defending Against Prototype Pollution
class SafeMerger {
  // Disallow forbidden prototype keys
  static #FORBIDDEN_KEYS = new Set(["__proto__", "constructor", "prototype"]);

  static merge(target, source) {
    if (source === null || typeof source !== "object") return target;
    if (target === null || typeof target !== "object") return source;

    for (const key of Object.keys(source)) {
      // 1. Defend against Prototype Pollution: reject dangerous keys
      if (SafeMerger.#FORBIDDEN_KEYS.has(key)) {
        console.warn(`[Security Alert] Prototype pollution attempt blocked on key: "${key}"`);
        continue;
      }

      const sourceValue = source[key];

      if (sourceValue && typeof sourceValue === "object" && !Array.isArray(sourceValue)) {
        if (!target[key] || typeof target[key] !== "object") {
          // Ensure child object does not inherit tainted prototypes
          target[key] = Object.create(null);
        }
        SafeMerger.merge(target[key], sourceValue);
      } else {
        target[key] = sourceValue;
      }
    }

    return target;
  }
}

// Verification of Defense
const payloadFromAttacker = JSON.parse('{"__proto__": {"isAdmin": true}, "name": "Eve"}');
const appState = {};

SafeMerger.merge(appState, payloadFromAttacker);

// Verify that Object.prototype was NOT polluted:
console.log("Is appState admin?", appState.isAdmin);         // undefined
console.log("Is plain object admin?", ({}).isAdmin);         // undefined (Pollution blocked!)
console.log("User name copied:", appState.name);             // "Eve"
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between an object's <code>[[Prototype]]</code> and a function's <code>prototype</code>?</strong></summary>

`[[Prototype]]` is the internal link that every object has pointing to the parent object from which it inherits properties. `prototype` is a standard property that exists on constructor functions; it is the blueprint object that will be assigned as the `[[Prototype]]` of any instance created via `new Constructor()`.
</details>

<details>
<summary><strong>2. What is Prototype Pollution, and how can an attacker exploit it?</strong></summary>

Prototype Pollution is a vulnerability where an attacker exploits recursive object assignment or parsing routines to inject properties onto `Object.prototype` (typically by passing payload keys like `"__proto__"` or `"constructor.prototype"`). Because almost all objects in JavaScript inherit from `Object.prototype`, the injected property becomes visible across all objects in the runtime, potentially allowing privilege escalation, remote code execution, or denial of service.
</details>

<details>
<summary><strong>3. Why is <code>Object.setPrototypeOf()</code> considered bad for performance?</strong></summary>

JavaScript engines optimize property access by generating Hidden Classes (Shapes) and caching property offsets in Inline Caches (ICs). Changing an object's prototype invalidates the object's shape and all downstream inline caches across any functions that have accessed it, forcing the engine to deoptimize to slow dictionary mode lookups.
</details>

<details>
<summary><strong>4. How does property shadowing work in the prototype chain?</strong></summary>

When setting a property `obj.prop = value`, if `prop` is not an own property but exists on the prototype chain:
1. If the prototype's property is normal and writable, JavaScript creates an own property `prop` on `obj`, shadowing the prototype property.
2. If the prototype's property is read-only (`writable: false`), setting it directly on `obj` throws a `TypeError` in strict mode or fails silently in sloppy mode.
3. If the prototype has a setter, the setter is invoked rather than creating a property on `obj`.
</details>

<details>
<summary><strong>5. What is the top of the prototype chain?</strong></summary>

The top of the standard prototype chain is `Object.prototype`. The internal `[[Prototype]]` of `Object.prototype` is `null`. Any lookup that fails to find a property on `Object.prototype` halts and evaluates to `undefined`.
</details>

<details>
<summary><strong>6. Why would you create an object using <code>Object.create(null)</code>?</strong></summary>

`Object.create(null)` creates an object whose internal `[[Prototype]]` is `null`. It inherits no properties or methods from `Object.prototype` (no `toString`, no `hasOwnProperty`, no `__proto__`). This makes it ideal for clean, high-performance hash maps that are completely immune to prototype collisions or prototype pollution.
</details>

<details>
<summary><strong>7. How does the <code>instanceof</code> operator utilize the prototype chain?</strong></summary>

`obj instanceof Constructor` checks whether `Constructor.prototype` exists anywhere along `obj`'s prototype chain. It repeatedly evaluates `Object.getPrototypeOf(obj)` in a loop, comparing it against `Constructor.prototype` until it finds a match (returning `true`) or reaches `null` (returning `false`).
</details>

<details>
<summary><strong>8. What is the difference between <code>Object.getPrototypeOf(obj)</code> and <code>obj.__proto__</code>?</strong></summary>

`Object.getPrototypeOf(obj)` is the official ECMAScript 5 standard method to inspect an object's prototype. `obj.__proto__` is an accessor property defined on `Object.prototype` that was adopted into Annex B of ES2015 for browser compatibility. `__proto__` can be shadowed, does not exist on `Object.create(null)` objects, and is deprecated for production use.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Prototype Verification (Easy)
Verify whether `Array.prototype` inherits from `Object.prototype` using both `isPrototypeOf` and `Object.getPrototypeOf`.

<details>
<summary>View Solution</summary>

```javascript
console.log(Object.prototype.isPrototypeOf(Array.prototype)); // true
console.log(Object.getPrototypeOf(Array.prototype) === Object.prototype); // true
```
</details>

### Exercise 2: Custom Prototype Chain Inspector (Easy)
Write a function `countPrototypeDepth(obj)` that returns the number of prototypes between `obj` and `null`.

<details>
<summary>View Solution</summary>

```javascript
function countPrototypeDepth(obj) {
  let depth = 0;
  let current = Object.getPrototypeOf(obj);
  while (current !== null) {
    depth++;
    current = Object.getPrototypeOf(current);
  }
  return depth;
}

console.log("Plain object depth:", countPrototypeDepth({})); // 1 (Object.prototype -> null)
console.log("Array depth:", countPrototypeDepth([]));       // 2 (Array.prototype -> Object.prototype -> null)
console.log("Null proto depth:", countPrototypeDepth(Object.create(null))); // 0
```
</details>

### Exercise 3: Custom Object.create Polyfill (Medium)
Implement `myObjectCreate(proto, propertiesObject)` reproducing `Object.create` without calling native `Object.create`.

<details>
<summary>View Solution</summary>

```javascript
function myObjectCreate(proto, propertiesObject) {
  if (typeof proto !== "object" && typeof proto !== "function") {
    throw new TypeError("Object prototype may only be an Object or null");
  }

  function TempConstructor() {}
  TempConstructor.prototype = proto;
  const instance = new TempConstructor();

  if (proto === null) {
    // Legacy __proto__ clearing for null prototype emulation
    instance.__proto__ = null;
  }

  if (propertiesObject !== undefined) {
    Object.defineProperties(instance, propertiesObject);
  }

  return instance;
}

const parent = { greet() { return "Hello from parent"; } };
const child = myObjectCreate(parent, {
  name: { value: "ChildInstance", enumerable: true }
});

console.log(child.greet()); // "Hello from parent"
console.log(child.name);    // "ChildInstance"
```
</details>

### Exercise 4: Custom instanceof Operator (Medium)
Implement `myInstanceOf(obj, constructorFn)` that traverses the prototype chain manually.

<details>
<summary>View Solution</summary>

```javascript
function myInstanceOf(obj, constructorFn) {
  if (obj === null || (typeof obj !== "object" && typeof obj !== "function")) {
    return false;
  }
  if (typeof constructorFn !== "function" || !constructorFn.prototype) {
    throw new TypeError("Right-hand side of 'instanceof' is not callable or has no prototype");
  }

  let current = Object.getPrototypeOf(obj);
  while (current !== null) {
    if (current === constructorFn.prototype) {
      return true;
    }
    current = Object.getPrototypeOf(current);
  }
  return false;
}

console.log(myInstanceOf([], Array));  // true
console.log(myInstanceOf([], Object)); // true
console.log(myInstanceOf([], RegExp)); // false
```
</details>

### Exercise 5: Freeze Prototype Tree (Hard)
Write a security function `deepFreezePrototypes(obj)` that freezes `obj` and all prototypes along its chain all the way to `Object.prototype`, preventing prototype tampering.

<details>
<summary>View Solution</summary>

```javascript
function deepFreezePrototypes(obj) {
  let current = obj;
  while (current !== null) {
    Object.freeze(current);
    current = Object.getPrototypeOf(current);
  }
}

const appData = { config: "active" };
deepFreezePrototypes(appData);

try {
  // @ts-ignore
  Object.prototype.polluted = true; // Throws TypeError in strict mode!
} catch (e) {
  console.log("Prototype mutation successfully prevented:", e.message);
}
```
</details>

### Exercise 6: Property Lookup Simulator with Shadowing Rules (Hard)
Build an engine simulation function `simulatePropertyGet(obj, propName)` that models exact ECMAScript `[[Get]]` mechanics: inspecting own properties, getters, and prototype traversal.

<details>
<summary>View Solution</summary>

```javascript
function simulatePropertyGet(obj, propName) {
  let current = obj;

  while (current !== null) {
    const desc = Object.getOwnPropertyDescriptor(current, propName);
    if (desc !== undefined) {
      if (desc.get) {
        // Execute getter with receiver bound to initial target
        return desc.get.call(obj);
      }
      return desc.value;
    }
    current = Object.getPrototypeOf(current);
  }

  return undefined;
}

const grandParent = {
  get greeting() { return `Hello, ${this.user}`; }
};
const parent = Object.create(grandParent);
const child = Object.create(parent);
child.user = "Grace";

console.log("Simulated Get:", simulatePropertyGet(child, "greeting")); // "Hello, Grace"
```
</details>
