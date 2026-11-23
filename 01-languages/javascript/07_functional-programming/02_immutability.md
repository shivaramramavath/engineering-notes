# JavaScript Immutability: Deep Freeze, Structural Sharing & Immer Mechanics

> Complete guide to immutability in JavaScript, comparing primitive immutability, `Object.freeze()`, persistent data structures with structural sharing, and Immer copy-on-write proxies.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [03_objects-and-arrays/01_objects.md](../03_objects-and-arrays/01_objects.md), [03_objects-and-arrays/02_property-descriptors.md](../03_objects-and-arrays/02_property-descriptors.md), [07_functional-programming/01_pure-functions-and-side-effects.md](./01_pure-functions-and-side-effects.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES5 (`Object.freeze`) / ES2015 (Spread & Proxies) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Mutability vs Immutability](#2-core-concepts-mutability-vs-immutability)
3. [How It Works: Structural Sharing Architecture](#3-how-it-works-structural-sharing-architecture)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Read-Only Objects & Fast Properties](#9-engine-internals-v8-read-only-objects--fast-properties)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Production Redux-Style State Store with Immer](#11-real-world-case-study-production-redux-style-state-store-with-immer)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, all primitive values (strings, numbers, booleans, symbols, bigints) are naturally **immutable**: you cannot alter the characters of a string or change the integer value of `5`. Objects, arrays, and functions, however, are **mutable reference types**: any part of a codebase holding a reference to an object can mutate its properties, triggering silent side effects across unrelated modules.

**Immutability** is the principle that once a data structure is created, its state can never be modified. To represent a change, a new data structure is produced with the desired modifications.

Immutability enables:
- **Instant Change Detection via Reference Equality**: `prevProps.user === nextProps.user` checks whether data changed in $O(1)$ time rather than performing expensive deep object traversals.
- **Time-Travel Debugging & Undo/Redo**: Preserving historical snapshots without defensive deep copying.
- **Thread & Concurrency Safety**: Eliminates race conditions in asynchronous worker architectures.

---

## 2. Core Concepts: Mutability vs Immutability

```
                             Mutability in JavaScript
                                        │
         ┌──────────────────────────────┴──────────────────────────────┐
         ▼                                                             ▼
   Primitives (Immutable)                                        Objects (Mutable)
   • Strings, Numbers, Booleans, Symbols                         • Objects, Arrays, Maps, Sets
   • str[0] = 'X' fails silently or throws                       • arr.push(), obj.x = 2 mutates in-place
   • Operations return new primitive values                      • Mutating one reference affects all references
```

### Shallow vs. Deep Immutability
- **Shallow Immutability**: `Object.freeze(obj)` locks only top-level properties. Nested objects remain fully mutable.
- **Deep Immutability**: Recursively freezing or validating all child structures so that no descendant property can be altered.

---

## 3. How It Works: Structural Sharing Architecture

A common objection to immutability is memory efficiency: "If I have an array of 100,000 items, doesn't creating a new array on every change waste memory?"

Modern functional programming solves this via **Structural Sharing**. When modifying a nested branch of a tree or persistent data structure, only the path from the root to the modified node is duplicated; all unaffected sibling branches share the **exact same memory references** with the original structure.

```
Structural Sharing: Modifying Node C to C'
Original Tree:                     New Tree (Sharing Node B):
      Root                               Root'
     /    \                             /     \
   [A]    [B] (Shared!)               [A']    [B] (Reused Reference!)
    │                                  │
   [C]                                [C'] (Modified Node)
```

---

## 4. Syntax & API Reference

### Built-in Immutability Methods
- `Object.freeze(obj)`: Makes top-level properties non-writable and non-configurable (`writable: false, configurable: false`). Prevents extensions.
- `Object.isFrozen(obj)`: Checks whether an object is frozen.
- `Object.seal(obj)`: Prevents adding or deleting properties, but existing writable properties can still be modified.
- `arr.toSorted()`, `arr.toReversed()`, `arr.toSpliced()`, `arr.with()`: ES2023 non-mutating array operations.

---

## 5. Practical Examples

### Example 1: Shallow Freeze Pitfall vs Deep Freeze
```javascript
// 1. Shallow Freeze
const config = Object.freeze({
  env: "production",
  db: { host: "127.0.0.1", port: 5432 }
});

config.env = "staging"; // Fails silently (or throws TypeError in strict mode)
console.log(config.env); // "production" (Protected!)

// Pitfall: Nested object is NOT frozen!
config.db.port = 9999;
console.log(config.db.port); // 9999! (Corrupted!)

// 2. Recursive Deep Freeze Implementation
function deepFreeze(obj) {
  Object.freeze(obj);
  for (const key of Object.getOwnPropertyNames(obj)) {
    const val = obj[key];
    if (val !== null && (typeof val === "object" || typeof val === "function") && !Object.isFrozen(val)) {
      deepFreeze(val);
    }
  }
  return obj;
}

const safeConfig = deepFreeze({
  db: { host: "localhost", port: 3306 }
});

try {
  "use strict";
  safeConfig.db.port = 8080; // Throws TypeError!
} catch (e) {
  console.log("Deep freeze protected nested property:", e.message);
}
```

### Example 2: Real-World Nested State Update via Spread
```javascript
const initialAppState = {
  theme: "dark",
  user: {
    id: "usr_101",
    profile: { firstName: "Ada", lastName: "Lovelace" },
    preferences: { notifications: true }
  }
};

// Pure immutable update changing ONLY firstName:
const nextAppState = {
  ...initialAppState,
  user: {
    ...initialAppState.user,
    profile: {
      ...initialAppState.user.profile,
      firstName: "Augusta"
    }
  }
};

// Structural Sharing Verification:
console.log("Root changed:", initialAppState === nextAppState); // false
console.log("User changed:", initialAppState.user === nextAppState.user); // false
console.log("Profile changed:", initialAppState.user.profile === nextAppState.user.profile); // false
// Unchanged sub-tree shares exact memory reference!
console.log("Preferences preserved:", initialAppState.user.preferences === nextAppState.user.preferences); // true
```

### Example 3: Edge Case with Array In-Place Mutation
```javascript
const originalScores = [88, 92, 75, 100];

// ES2023 Pure non-mutating update with .with()
const updatedScores = originalScores.with(2, 95);

console.log("Original untouched:", originalScores); // [88, 92, 75, 100]
console.log("Updated copy:", updatedScores);       // [88, 92, 95, 100]
console.log("Reference equality:", originalScores === updatedScores); // false
```

### Example 4: Failure Mode with Accidental In-Place Splice
```javascript
// Failure mode: Calling splice() on props or state directly
function removeTodoBad(todos, index) {
  // BUG: splice() mutates todos in place!
  todos.splice(index, 1);
  return todos;
}

const list = ["A", "B", "C"];
const nextList = removeTodoBad(list, 1);
console.log("Reference equality broken:", list === nextList); // true! (Same instance, React won't re-render!)

// Correct non-mutating approach:
function removeTodoGood(todos, index) {
  return todos.toSpliced(index, 1);
}
```

### Example 5: Idiomatic Copy-On-Write Implementation (Minimal Immer Pattern)
```javascript
function produce(baseState, recipe) {
  const copies = new Map();

  function getProxy(target) {
    return new Proxy(target, {
      get(obj, prop) {
        if (copies.has(obj)) {
          return getProxy(copies.get(obj)[prop]);
        }
        const val = obj[prop];
        return (val && typeof val === "object") ? getProxy(val) : val;
      },
      set(obj, prop, value) {
        let copy = copies.get(obj);
        if (!copy) {
          copy = Array.isArray(obj) ? [...obj] : { ...obj };
          copies.set(obj, copy);
        }
        copy[prop] = value;
        return true;
      }
    });
  }

  const draft = getProxy(baseState);
  recipe(draft);

  // Return copy if modified, or original if untouched
  return copies.get(baseState) || baseState;
}

const state = { count: 0, items: ["a", "b"] };
const next = produce(state, draft => {
  draft.count = 1;
});

console.log("Original state:", state); // { count: 0, items: ['a', 'b'] }
console.log("Next state:", next);       // { count: 1, items: ['a', 'b'] }
console.log("Items shared:", state.items === next.items); // true (Structural sharing!)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **`Object.freeze()` is Strictly Shallow**: Never assume an object is safe just because `Object.freeze()` was called; child arrays and sub-objects remain fully mutable unless recursively frozen.
2. **`const` Does NOT Mean Immutable**: `const x = { a: 1 }` only prevents reassignment of the variable binding `x`. The underlying object is completely mutable (`x.a = 2`).
3. **Circular References in Deep Freeze**: Naive recursive `deepFreeze()` algorithms crash with a call stack overflow on circular references. Always track visited objects with a `Set` or `WeakSet`.
4. **Performance of Deep Spreads**: Deeply nested spread syntax (`{ ...a, b: { ...a.b, c: { ...a.b.c } } }`) becomes unreadable and error-prone beyond 3 levels deep. Use Immer or lenses for complex trees.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Assuming `const` creates immutability | `const a = [1]; a.push(2);` | Use `Object.freeze([1])` | `const` only locks the identifier reference. |
| Shallow freeze on nested object | `Object.freeze({ sub: {} })` | `deepFreeze({ sub: {} })` | Child objects remain silently mutable. |
| In-place sorting in Redux reducer | `state.items.sort()` | `state.items.toSorted()` | Breaks change detection and causes UI bugs. |
| Using `JSON.parse(JSON.stringify())` | Deep clone via JSON serialization | Structural sharing / Immer | Discards Dates, Functions, Symbols, and is slow. |
| Over-cloning untouched branches | Deep cloning entire 10MB tree | Spread only modified branch | Wastes CPU and breaks memoized components. |
| Modifying state in React render | `state.count++` in component | Dispatch action / `setState` | Violates React purity and breaks concurrent mode. |

---

## 8. Performance Characteristics

- **Shallow Reference Checking**: Checking `prev === next` takes $O(1)$ constant time (a single CPU pointer equality check), enabling fast React `shouldComponentUpdate` / `memo`.
- **Structural Sharing**: Modifying a tree of size $N$ requires copying only $O(\log N)$ nodes along the modified path, rather than $O(N)$ for full deep copying.
- **`Object.freeze` Overhead**: Freezing an object marks its internal V8 descriptors as non-writable and non-configurable, which can slightly slow down subsequent dynamic property additions if the engine transitions away from fast shapes.

---

## 9. Engine Internals: V8 Read-Only Objects & Fast Properties

Inside Google V8:
1. When `Object.freeze()` is invoked, V8 walks the object's `DescriptorArray`.
2. It sets the `ReadOnly` and `DontDelete` attributes on every property descriptor.
3. If the object was in fast-property mode, V8 transitions its `Map` (Hidden Class) to an immutable `Map`.
4. If code in strict mode attempts to write to a frozen property, Ignition emits `ThrowTypeErrorIfReadOnly`, raising a runtime exception without entering slow dictionary lookup mode.

---

## 10. Best Practices & Design Guidelines

1. **Leverage ES2023 Non-Mutating Methods**: Use `toSorted()`, `toReversed()`, `toSpliced()`, and `with()` for all array operations.
2. **Use Immer for Deep State Mutations**: In Redux, Zustand, or complex domain state, use Immer's `produce()` to write readable imperative-looking code that produces mathematically pure immutable updates with structural sharing.
3. **Rely on Reference Equality for Caching**: Design state trees so that unchanged subtrees preserve referential equality (`===`), enabling seamless memoization.

---

## 11. Real-World Case Study: Production Redux-Style State Store with Immer

An enterprise application state container executing pure actions with structural sharing and rollback history.

```javascript
class ImmutableStore {
  #state;
  #listeners = new Set();
  #history = [];

  constructor(initialState) {
    this.#state = Object.freeze(initialState);
    this.#history.push(this.#state);
  }

  getState() {
    return this.#state;
  }

  subscribe(listener) {
    this.#listeners.add(listener);
    return () => this.#listeners.delete(listener);
  }

  // Pure updater enforcing structural sharing
  dispatch(updaterFn) {
    const prevState = this.#state;
    // Execute updater pure recipe
    const nextState = Object.freeze(updaterFn(prevState));

    if (nextState !== prevState) {
      this.#state = nextState;
      this.#history.push(nextState);
      this.#notify(prevState, nextState);
    }
  }

  rollback() {
    if (this.#history.length > 1) {
      this.#history.pop(); // Remove current
      const prevState = this.#state;
      this.#state = this.#history[this.#history.length - 1];
      this.#notify(prevState, this.#state);
    }
  }

  #notify(prev, next) {
    for (const listener of this.#listeners) {
      listener(next, prev);
    }
  }
}

// Verification
const store = new ImmutableStore({
  session: { user: "Ada", loggedIn: true },
  ui: { theme: "dark", sidebarOpen: false }
});

store.subscribe((next, prev) => {
  console.log(`[Store Update] Theme changed: ${prev.ui.theme} -> ${next.ui.theme}`);
  // Verify structural sharing: session was NOT touched!
  console.log("Session reference shared:", prev.session === next.session); // true
});

store.dispatch(state => ({
  ...state,
  ui: { ...state.ui, theme: "light" }
}));

store.rollback();
console.log("Rolled back theme:", store.getState().ui.theme); // "dark"
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the difference between <code>const</code> and <code>Object.freeze()</code>?</strong></summary>

`const` creates an immutable variable binding (the identifier cannot be reassigned to a different value), but the contents of the object or array it points to remain fully mutable. `Object.freeze()` operates on the object value in memory, making its properties non-writable and non-configurable, but does not prevent a `let` variable holding it from being reassigned.
</details>

<details>
<summary><strong>2. Why is reference equality (<code>===</code>) important for frontend performance?</strong></summary>

Reference equality allows frameworks like React to verify whether component props or state changed in $O(1)$ constant time. If state is immutable and structural sharing is used, `prevProps.data === nextProps.data` proves instantly that neither `data` nor any of its descendants changed, allowing the component to completely skip re-rendering.
</details>

<details>
<summary><strong>3. What is structural sharing, and how does it prevent memory explosion in immutable architectures?</strong></summary>

Structural sharing is a technique where an updated data structure reuses (shares) references to all nodes and branches that were not modified. Instead of cloning an entire 100,000-element structure ($O(n)$ memory), only the modified node and the path leading to it are copied ($O(\log n)$ memory), while all other branches share pointers with the original structure.
</details>

<details>
<summary><strong>4. How does Immer implement the "copy-on-write" pattern using Proxies?</strong></summary>

Immer wraps the state tree in JavaScript `Proxy` objects (a "draft"). When properties are merely read, the proxy returns data from the original state. When a property is modified, the proxy creates a shallow copy of that specific node and marks it as modified. When the recipe completes, Immer finalizes the tree by merging the modified copies with the untouched original branches.
</details>

<details>
<summary><strong>5. Can <code>Object.freeze()</code> be reversed on an object?</strong></summary>

No. The ECMAScript specification provides no API to "unfreeze" an object. Once an object is frozen, its internal descriptors are permanently non-configurable. To modify a frozen object, you must create a new object copying its properties.
</details>

<details>
<summary><strong>6. What happens if you try to mutate a frozen object in strict mode vs sloppy mode?</strong></summary>

In sloppy mode, the mutation fails silently: the property does not change, and execution continues without warning. In strict mode (`"use strict"`), attempting to mutate a property on a frozen object throws a `TypeError: Cannot assign to read only property`.
</details>

<details>
<summary><strong>7. How does <code>Array.prototype.with()</code> differ from bracket assignment?</strong></summary>

Bracket assignment (`arr[0] = value`) mutates the array in place. `arr.with(0, value)` is an ES2023 pure method that returns a brand-new array with the element at the specified index replaced, leaving the original array completely untouched.
</details>

<details>
<summary><strong>8. Why is <code>JSON.parse(JSON.stringify(obj))</code> considered an anti-pattern for deep cloning?</strong></summary>

1. It strips functions, `undefined`, and Symbol properties.
2. It converts `Date` objects into ISO strings.
3. It converts `Map` and `Set` into empty objects.
4. It crashes on circular references with `TypeError: Converting circular structure to JSON`.
5. It duplicates the entire structure in memory, completely bypassing structural sharing.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Recursive Deep Freeze with Circular Safety (Easy)
Write a function `safeDeepFreeze(obj)` that recursively freezes an object, handling circular references without crashing.

<details>
<summary>View Solution</summary>

```javascript
function safeDeepFreeze(obj, visited = new WeakSet()) {
  if (obj === null || typeof obj !== "object" || visited.has(obj)) {
    return obj;
  }

  visited.add(obj);
  Object.freeze(obj);

  for (const key of Object.getOwnPropertyNames(obj)) {
    safeDeepFreeze(obj[key], visited);
  }

  return obj;
}

const circular = { a: 1 };
circular.self = circular;
safeDeepFreeze(circular);
console.log("Is frozen:", Object.isFrozen(circular)); // true
```
</details>

### Exercise 2: Pure Nested Object Update (Easy)
Given a nested user object, write a pure function `updatePostalCode(user, newZip)` that returns an updated user using spread syntax.

<details>
<summary>View Solution</summary>

```javascript
function updatePostalCode(user, newZip) {
  return {
    ...user,
    address: {
      ...user.address,
      zip: newZip
    }
  };
}

const original = { name: "Grace", address: { city: "Boston", zip: "02101" } };
const updated = updatePostalCode(original, "02108");
console.log(updated.address.zip); // "02108"
console.log(original.address.zip); // "02101"
```
</details>

### Exercise 3: Immutable Array Manipulation Suite (Medium)
Implement three pure functions for arrays: `insertAt(arr, index, item)`, `removeAt(arr, index)`, and `replaceAt(arr, index, item)` without using ES2023 methods.

<details>
<summary>View Solution</summary>

```javascript
function insertAt(arr, index, item) {
  return [...arr.slice(0, index), item, ...arr.slice(index)];
}

function removeAt(arr, index) {
  return [...arr.slice(0, index), ...arr.slice(index + 1)];
}

function replaceAt(arr, index, item) {
  return [...arr.slice(0, index), item, ...arr.slice(index + 1)];
}

const list = ["A", "B", "D"];
console.log(insertAt(list, 2, "C"));  // ['A', 'B', 'C', 'D']
console.log(removeAt(list, 1));       // ['A', 'D']
console.log(replaceAt(list, 0, "Z")); // ['Z', 'B', 'D']
```
</details>

### Exercise 4: Immutable Record Class (Medium)
Create an `ImmutableRecord` base class where all instances are deeply frozen upon construction and subclass fields cannot be modified.

<details>
<summary>View Solution</summary>

```javascript
class ImmutableRecord {
  constructor(fields = {}) {
    Object.assign(this, fields);
    this.#deepFreeze(this);
  }

  #deepFreeze(obj) {
    Object.freeze(obj);
    for (const key of Object.getOwnPropertyNames(obj)) {
      const val = obj[key];
      if (val && typeof val === "object" && !Object.isFrozen(val)) {
        this.#deepFreeze(val);
      }
    }
  }

  with(updates) {
    return new this.constructor({ ...this, ...updates });
  }
}

class UserRecord extends ImmutableRecord {}
const user1 = new UserRecord({ id: 1, name: "Ada" });
const user2 = user1.with({ name: "Grace" });

console.log(user1.name); // "Ada"
console.log(user2.name); // "Grace"
console.log(Object.isFrozen(user1)); // true
```
</details>

### Exercise 5: Structural Sharing Verification Utility (Hard)
Write an assertion function `assertStructuralSharing(oldObj, newObj, modifiedPath)` that verifies that the modified path was updated while all sibling paths preserved identical memory references (`===`).

<details>
<summary>View Solution</summary>

```javascript
function assertStructuralSharing(oldObj, newObj, modifiedKeys) {
  const modSet = new Set(modifiedKeys);

  for (const key of Object.keys(oldObj)) {
    if (modSet.has(key)) {
      if (oldObj[key] === newObj[key]) {
        throw new Error(`Path '${key}' was supposed to be modified, but references are identical!`);
      }
    } else {
      if (oldObj[key] !== newObj[key]) {
        throw new Error(`Unmodified path '${key}' broke structural sharing!`);
      }
    }
  }
  return true;
}

const stateA = { a: { x: 1 }, b: { y: 2 } };
const stateB = { ...stateA, a: { x: 99 } };

console.log("Sharing verified:", assertStructuralSharing(stateA, stateB, ["a"])); // true
```
</details>

### Exercise 6: Persistent Linked List with Structural Sharing (Hard)
Implement a persistent immutable singly-linked list `PersistentList` with `prepend(val)` and `tail()` that shares all downstream nodes across multiple list versions.

<details>
<summary>View Solution</summary>

```javascript
class PersistentNode {
  constructor(value, next = null) {
    this.value = value;
    this.next = next;
    Object.freeze(this);
  }
}

class PersistentList {
  #head;

  constructor(head = null) {
    this.#head = head;
    Object.freeze(this);
  }

  prepend(value) {
    return new PersistentList(new PersistentNode(value, this.#head));
  }

  head() {
    return this.#head?.value;
  }

  tail() {
    return new PersistentList(this.#head?.next);
  }

  toArray() {
    const res = [];
    let curr = this.#head;
    while (curr) {
      res.push(curr.value);
      curr = curr.next;
    }
    return res;
  }
}

// Verification of structural sharing
const list1 = new PersistentList().prepend(3).prepend(2).prepend(1); // [1, 2, 3]
const list2 = list1.prepend(0); // [0, 1, 2, 3]

console.log("List 1:", list1.toArray()); // [1, 2, 3]
console.log("List 2:", list2.toArray()); // [0, 1, 2, 3]
console.log("Shared tail nodes:", list2.tail().head() === list1.head()); // true
```
</details>
