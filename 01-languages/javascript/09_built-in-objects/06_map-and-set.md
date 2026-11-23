# JavaScript Map & Set: Keyed Collections, ES2024 Set Operations & V8 Hash Tables

> Authoritative technical guide to ECMAScript `Map` and `Set`, `SameValueZero` equality, ES2024 native Set methods (`union`, `intersection`, `difference`), and V8 `OrderedHashTable` memory architecture.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Engine Internals |
| **Prerequisites** | [08_modern-javascript/04_iterators-and-iterables.md](../08_modern-javascript/04_iterators-and-iterables.md), [03_objects-and-arrays/01_object-basics.md](../03_objects-and-arrays/01_object-basics.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Map, Set) / ES2024 (Set Methods: union, intersection, difference, etc.) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: SameValueZero Equality & Collection Invariants](#2-core-concepts-samevaluezero-equality--collection-invariants)
3. [How It Works: Engine Hash Tables & Insertion Order](#3-how-it-works-engine-hash-tables--insertion-order)
4. [Syntax & API Reference (Including ES2024 Set Methods)](#4-syntax--api-reference-including-es2024-set-methods)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics: Map vs Object vs Array](#8-performance-characteristics-map-vs-object-vs-array)
9. [Engine Internals: V8 OrderedHashTable & Tombstone Compaction](#9-engine-internals-v8-orderedhashtable--tombstone-compaction)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: High-Throughput LRU Cache & RBAC Permission Engine](#11-real-world-case-study-high-throughput-lru-cache--rbac-permission-engine)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), JavaScript lacked dedicated collection primitives. Developers were forced to misuse plain Objects (`{}`) as hash maps and Arrays (`[]`) as sets:

```javascript
// Pre-ES6 clumsy hash map:
const userMap = {};
userMap[userId] = user; // Key coerced to string! Prototype pollution risk!
```

### Why Plain Objects Fall Short as Hash Maps:
1. **String/Symbol Key Limitation:** Object keys are strictly coerced to strings or symbols. You cannot use DOM elements, functions, or objects as keys.
2. **Prototype Pollution:** Plain objects inherit built-in properties (`toString`, `valueOf`, `constructor`, `__proto__`). If a user inputs `"constructor"`, lookups can corrupt application logic.
3. **No Direct Size Property:** Determining the number of entries requires `Object.keys(obj).length`, which takes $O(n)$ time.
4. **Unreliable Iteration Order:** Although ES2015 defined traversal order, integer-like string keys sort numerically before strings, breaking deterministic insertion ordering.

**`Map`** and **`Set`** provide clean, high-performance data structures:
- **`Map`:** A hash table supporting keys of **any type**, guaranteed $O(1)$ size tracking, and strict insertion-order iteration.
- **`Set`:** A collection of **guaranteed unique values**, eliminating duplicate filtering boilerplate. In ES2024, `Set` gained native mathematical set methods (`union`, `intersection`, `difference`).

---

## 2. Core Concepts: SameValueZero Equality & Collection Invariants

### 2.1 Key Equality: SameValueZero
Both `Map` and `Set` check key equality using the internal **`SameValueZero`** abstract operation.
`SameValueZero` behaves almost identically to strict equality (`===`), with two critical differences:
1. `NaN` equals `NaN`: In standard JavaScript, `NaN === NaN` is `false`. In a `Map` or `Set`, `NaN` is treated as a single, identical key:
   ```javascript
   const set = new Set();
   set.add(NaN);
   set.add(NaN);
   console.log(set.size); // 1 (Only one NaN is stored)
   ```
2. `+0` equals `-0`: `+0` and `-0` are treated as identical keys.

### 2.2 Object Identity as Keys
When an object is used as a `Map` key or stored in a `Set`, equality is determined by **memory reference pointer**, not structural equality:
```javascript
const map = new Map();
const keyA = { id: 1 };
const keyB = { id: 1 };

map.set(keyA, "Admin");
console.log(map.get(keyB));       // undefined (different reference in heap!)
console.log(map.get(keyA));       // "Admin"
```

---

## 3. How It Works: Engine Hash Tables & Insertion Order

Per ECMA-262, both `Map` and `Set` guarantee that elements are iterated in the **exact order they were inserted**.

To satisfy both $O(1)$ amortized lookups and strictly preserved insertion order without allocating two separate data structures, modern engines (V8, JavaScriptCore, SpiderMonkey) use an **`OrderedHashTable`**:

```
+---------------------------------------------------------------------------------+
|                         V8 OrderedHashTable Layout                              |
+---------------------------------------------------------------------------------+
| Bucket Table:       [Hash(key) % BucketCount] ---> Points to Data Table Index   |
+---------------------------------------------------------------------------------+
| Data Table (Dense array preserving insertion order):                            |
| Index 0: [Key: "a", Value: 10, NextInBucket: -1]                                |
| Index 1: [Key: "b", Value: 20, NextInBucket: -1]                                |
| Index 2: [Key: "c", Value: 30, NextInBucket:  0] (Collision linked list)        |
+---------------------------------------------------------------------------------+
```

- When iterating (`for...of map`), the engine scans sequentially through the **Data Table** from index $0$ to $N - 1$, guaranteeing true FIFO insertion ordering.
- When looking up a key (`map.get(k)`), the engine hashes the key, inspects the **Bucket Table**, and follows bucket links in $O(1)$ amortized time.

---

## 4. Syntax & API Reference (Including ES2024 Set Methods)

### 4.1 Map API Reference
```javascript
const map = new Map([
  ["role", "admin"],
  [{ id: 10 }, "user-10"]
]);

// Mutation:
map.set("theme", "dark"); // Returns map (chainable)
console.log(map.get("role")); // "admin"
console.log(map.has("theme")); // true
console.log(map.delete("role")); // true (returns boolean indicating success)
console.log(map.size); // 2

// Iteration:
for (const [key, value] of map) {}
for (const key of map.keys()) {}
for (const value of map.values()) {}
for (const [k, v] of map.entries()) {}
map.forEach((val, key) => {});
map.clear(); // Clears all entries
```

### 4.2 Set API Reference
```javascript
const set = new Set([1, 2, 2, 3, 4]);
console.log(set.size); // 4 (Duplicates dropped)

set.add(5); // Chainable
console.log(set.has(3)); // true
set.delete(2); // Returns boolean
```

### 4.3 Native Mathematical Set Methods (ES2024 / Node 22+)
Standardized in ECMAScript 2024, these methods operate between sets without manual iteration:

| Method | Mathematical Notation | Description |
| :--- | :--- | :--- |
| `a.union(b)` | $A \cup B$ | Returns a new set containing all elements from both $A$ and $B$. |
| `a.intersection(b)` | $A \cap B$ | Returns a new set containing elements present in both $A$ and $B$. |
| `a.difference(b)` | $A \setminus B$ | Returns a new set with elements in $A$ that are not in $B$. |
| `a.symmetricDifference(b)` | $A \triangle B$ | Returns a new set with elements in either $A$ or $B$, but not both. |
| `a.isSubsetOf(b)` | $A \subseteq B$ | Returns `true` if all elements of $A$ are in $B$. |
| `a.isSupersetOf(b)` | $A \supseteq B$ | Returns `true` if all elements of $B$ are in $A$. |
| `a.isDisjointFrom(b)` | $A \cap B = \emptyset$ | Returns `true` if $A$ and $B$ share zero common elements. |

```javascript
// Native ES2024 Set operations:
const setA = new Set([1, 2, 3]);
const setB = new Set([3, 4, 5]);

console.log([...setA.union(setB)]);               // [1, 2, 3, 4, 5]
console.log([...setA.intersection(setB)]);        // [3]
console.log([...setA.difference(setB)]);          // [1, 2]
console.log([...setA.symmetricDifference(setB)]); // [1, 2, 4, 5]
console.log(new Set([1, 2]).isSubsetOf(setA));    // true
console.log(setA.isDisjointFrom(new Set([6, 7])));// true
```

---

## 5. Practical Examples

### Example 1: High-Performance LRU (Least Recently Used) Cache via Map
Because `Map` iterates in insertion order, deleting and re-inserting an item moves it to the very back (most recently used):
```javascript
class LRUCache {
  constructor(capacity) {
    this.capacity = capacity;
    this.cache = new Map();
  }

  get(key) {
    if (!this.cache.has(key)) return undefined;
    const value = this.cache.get(key);
    // Refresh access order:
    this.cache.delete(key);
    this.cache.set(key, value);
    return value;
  }

  set(key, value) {
    if (this.cache.has(key)) {
      this.cache.delete(key);
    } else if (this.cache.size >= this.capacity) {
      // Evict oldest item (first key in iteration order):
      const oldestKey = this.cache.keys().next().value;
      this.cache.delete(oldestKey);
    }
    this.cache.set(key, value);
  }
}

const lru = new LRUCache(2);
lru.set("a", 1);
lru.set("b", 2);
lru.get("a");    // "a" becomes most recent
lru.set("c", 3); // "b" is evicted!

console.log(lru.get("b")); // undefined
console.log(lru.get("a")); // 1
console.log(lru.get("c")); // 3
```

### Example 2: Role-Based Access Control (RBAC) with ES2024 Set Methods
```javascript
class AccessControl {
  static ALL_PERMISSIONS = new Set(["read", "write", "delete", "audit"]);

  static hasRequiredPermissions(userPerms, requiredPerms) {
    // Check if required is a subset of user's permissions:
    return requiredPerms.isSubsetOf(userPerms);
  }

  static findMissingPermissions(userPerms, requiredPerms) {
    return requiredPerms.difference(userPerms);
  }
}

const editorPerms = new Set(["read", "write"]);
const adminActions = new Set(["read", "write", "delete"]);

console.log("Can editor perform admin action?", AccessControl.hasRequiredPermissions(editorPerms, adminActions)); // false
console.log("Missing perms:", [...AccessControl.findMissingPermissions(editorPerms, adminActions)]); // ['delete']
```

### Example 3: Bidirectional Two-Way Map
```javascript
class BiMap {
  constructor() {
    this.forward = new Map();
    this.backward = new Map();
  }

  set(key, value) {
    this.forward.set(key, value);
    this.backward.set(value, key);
    return this;
  }

  getByValue(value) {
    return this.backward.get(value);
  }

  getByKey(key) {
    return this.forward.get(key);
  }
}

const ipHostMap = new BiMap();
ipHostMap.set("127.0.0.1", "localhost");
console.log(ipHostMap.getByKey("127.0.0.1"));   // "localhost"
console.log(ipHostMap.getByValue("localhost")); // "127.0.0.1"
```

### Example 4: Deduplicating Object Arrays by Unique Compound Key
```javascript
function deduplicateBy(array, keySelector) {
  const seen = new Set();
  return array.filter(item => {
    const key = keySelector(item);
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });
}

const users = [
  { id: 1, email: "ada@example.com" },
  { id: 2, email: "bob@example.com" },
  { id: 3, email: "ada@example.com" } // Duplicate email!
];

const unique = deduplicateBy(users, u => u.email);
console.log(unique.map(u => u.id)); // [1, 2]
```

### Example 5: Frequency Counter / Histogram
```javascript
function wordHistogram(text) {
  const counts = new Map();
  const words = text.toLowerCase().match(/\b\w+\b/g) || [];

  for (const word of words) {
    counts.set(word, (counts.get(word) || 0) + 1);
  }
  return counts;
}

const counts = wordHistogram("JavaScript is fast. Node is fast. JavaScript is fun.");
console.log(Object.fromEntries(counts));
// { javascript: 2, is: 3, fast: 2, node: 1, fun: 1 }
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 Accidental Object Key Lookup Failures
Because objects are matched by reference, passing a newly created literal to `.get()` or `.has()` always returns `undefined` or `false`:
```javascript
const map = new Map();
map.set({ role: "admin" }, true);

// Fails! Different heap address:
console.log(map.has({ role: "admin" })); // false
```
**Fix:** Store and reuse the variable reference, or serialize complex keys to strings.

### 6.2 Array Spread Conversion Performance on Sets
Converting a large `Set` back into an Array using `[...set]` or `Array.from(set)` allocates a new array and copies all elements. When checking for existence, do not convert to array; query `set.has(item)` directly ($O(1)$ vs $O(n)$).

### 6.3 Memory Leaks with Retained Keys
Unlike `WeakMap`, a standard `Map` holds **strong references** to both its keys and its values. Storing large DOM nodes or objects in a global `Map` prevents them from being garbage-collected, even after they are removed from the DOM.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `map[key] = val`** | `myMap["key"] = 100;` | `myMap.set("key", 100);` | Bracket notation sets properties on the wrapper object, bypassing the hash table. |
| **Object Key Coercion** | Using `{}` when keys are non-strings | Use `new Map()` | Objects coerce non-string keys to `"[object Object]"`. |
| **Array Deduplication via `indexOf`** | `arr.filter((v, i) => arr.indexOf(v) === i)` | `[...new Set(arr)]` | Array `indexOf` takes $O(n^2)$ time; Set deduplication takes $O(n)$ time. |
| **Checking Set Membership via Array** | `[...set].includes(item)` | `set.has(item)` | Spreading to array incurs $O(n)$ allocation; `has()` is $O(1)$. |
| **Converting Map to Object with Non-String Keys** | `Object.fromEntries(mapWithObjKeys)` | Use native Map iteration | `Object.fromEntries` coerces keys to strings, losing object identity. |
| **Checking Map emptiness via `size` incorrectly** | `if (map.size)` when checking empty | `if (map.size === 0)` | Map size is a number; `0` is falsy, which is easy to misread. |

---

## 8. Performance Characteristics: Map vs Object vs Array

| Operation | `Map` | Plain `Object` | `Array` |
| :--- | :--- | :--- | :--- |
| **Key Lookup** | $O(1)$ amortized | $O(1)$ amortized | $O(n)$ search |
| **Insertion** | $O(1)$ amortized | $O(1)$ amortized (Triggers Hidden Class transitions) | $O(1)$ append / $O(n)$ unshift |
| **Deletion** | $O(1)$ | $O(1)$ (Transitions object to dictionary mode) | $O(n)$ splice |
| **Size Determination** | $O(1)$ constant property | $O(n)$ via `Object.keys().length` | $O(1)$ `.length` |
| **Memory Footprint** | Higher (~3–4x overhead for hash pointers) | Lower (compact V8 in-object layout) | Compact contiguous buffer |

---

## 9. Engine Internals: V8 OrderedHashTable & Tombstone Compaction

When entries are deleted from a `Map` or `Set`:
```javascript
map.delete("a");
```
1. V8 **does not shift** array elements in the internal Data Table (which would take $O(n)$ time and break existing iterator index pointers).
2. Instead, V8 marks the entry as a **Tombstone** (a special deleted sentinel value `the_hole`).
3. Active iterators continue scanning without interruption.
4. When the number of tombstone holes exceeds a threshold (typically $> 33\%$ of the table capacity) or when the table needs to grow, V8 triggers **Table Compaction & Rehashing**:
   - It allocates a new, clean Data Table.
   - It copies surviving non-deleted entries into contiguous slots.
   - It recomputes bucket hash links.

---

## 10. Best Practices & Design Guidelines

1. **Use `Map` when Keys are Dynamic or Unknown:** If keys originate from user input, JSON configs, or external APIs, always use `Map` to prevent prototype pollution and key collisions.
2. **Use Plain Objects for Known Static Records:** For small, fixed-shape entities (e.g., `{ x: 10, y: 20 }`), plain objects remain faster because V8 optimizes them with monomorphic Hidden Classes.
3. **Leverage ES2024 Set Methods for Business Logic:** Replace custom iteration loops with native `intersection()`, `difference()`, and `union()`.
4. **Use `Map.prototype.forEach` or `for...of`:** Both respect insertion order and avoid allocating intermediate arrays.

---

## 11. Real-World Case Study: High-Throughput LRU Cache & RBAC Permission Engine

### Problem
An authentication gateway validated access permissions for 50,000 requests per second. The permission verifier converted database permission arrays into plain objects and used lodash `_.intersection()`, causing 80 MB/sec of heap allocations and triggering frequent Garbage Collection latency spikes.

### Solution
Migrate permission stores to native `Set` instances utilizing ES2024 `intersection()` and `isSubsetOf()`:

```javascript
class GatewayAuthorizer {
  constructor() {
    this.rolePermissions = new Map();
    this.initRoles();
  }

  initRoles() {
    this.rolePermissions.set("USER", new Set(["profile:read", "orders:read"]));
    this.rolePermissions.set("SUPPORT", new Set(["profile:read", "orders:read", "orders:write"]));
    this.rolePermissions.set("ADMIN", new Set(["profile:read", "orders:read", "orders:write", "users:manage"]));
  }

  isAuthorized(userRole, requiredPermissions) {
    const userPerms = this.rolePermissions.get(userRole);
    if (!userPerms) return false;

    // Zero array allocation; direct C++ set subset check:
    return requiredPermissions.isSubsetOf(userPerms);
  }
}

const auth = new GatewayAuthorizer();
const checkoutAction = new Set(["orders:read", "orders:write"]);

console.log("Can USER checkout?", auth.isAuthorized("USER", checkoutAction));       // false
console.log("Can SUPPORT checkout?", auth.isAuthorized("SUPPORT", checkoutAction)); // true
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What are the primary advantages of <code>Map</code> over a plain Object as a dictionary?</strong></summary>

1. **Key Types:** `Map` supports keys of any type (including objects and functions); Objects only support strings and symbols.
2. **Prototype Safety:** `Map` has no default keys or prototypes, preventing prototype pollution.
3. **Size Tracking:** `map.size` evaluates in $O(1)$; objects require $O(n)$ `Object.keys(obj).length`.
4. **Guaranteed Ordering:** `Map` strictly preserves FIFO insertion order across all key types.
5. **Garbage Collection Optimization:** V8 optimizes `Map` with dedicated `OrderedHashTable` allocations.
</details>

<details>
<summary><strong>Q2: How does <code>SameValueZero</code> equality work in Map and Set?</strong></summary>

`SameValueZero` is an internal specification algorithm. It matches strict equality (`===`), except that:
1. `NaN` is considered equal to `NaN` (allowing `NaN` to be used as a valid key).
2. `+0` is considered equal to `-0`.
</details>

<details>
<summary><strong>Q3: How does V8 maintain insertion order in Map while still providing $O(1)$ lookups?</strong></summary>

V8 uses an internal `OrderedHashTable`. It separates storage into a **Bucket Table** (which maps hashed keys to indices in $O(1)$ time) and a contiguous **Data Table** (which appends new entries sequentially). Traversal simply reads the Data Table linearly, guaranteeing exact insertion order.
</details>

<details>
<summary><strong>Q4: What happens internally in V8 when an element is deleted from a Map?</strong></summary>

V8 does not shift remaining elements (which would take $O(n)$ time). Instead, it replaces the key and value slots in the Data Table with a special sentinel marker called a **tombstone** (`the_hole`). The tombstone preserves existing iteration pointers. When enough tombstones accumulate, V8 compacts and rehashes the table into a new buffer.
</details>

<details>
<summary><strong>Q5: What are the new mathematical Set methods standardized in ECMAScript 2024?</strong></summary>

ES2024 standardized 7 native methods:
- Transformations: `union()`, `intersection()`, `difference()`, and `symmetricDifference()`.
- Predicates: `isSubsetOf()`, `isSupersetOf()`, and `isDisjointFrom()`.
</details>

<details>
<summary><strong>Q6: Why does <code>new Map().set({}, 1).get({})</code> return undefined?</strong></summary>

JavaScript objects are compared by memory reference identity, not value. The object literal passed to `.set({}, 1)` is allocated at a distinct memory address from the new object literal passed to `.get({})`. Because their pointer addresses differ, `SameValueZero` returns `false`.
</details>

<details>
<summary><strong>Q7: Can a Map cause a memory leak if used as a cache? Why?</strong></summary>

Yes. A standard `Map` holds **strong references** to both its keys and its values. Even if a key object is removed from the rest of the application, as long as it exists as a key in the `Map`, the garbage collector cannot free it. To avoid memory leaks with object keys, use `WeakMap`.
</details>

<details>
<summary><strong>Q8: What is the time complexity of deduplicating an array with <code>new Set(arr)</code> vs <code>arr.filter()</code> with <code>indexOf</code>?</strong></summary>

`new Set(arr)` takes **$O(n)$** time because adding to a Set is an $O(1)$ hash table operation per element. In contrast, `arr.filter((v, i) => arr.indexOf(v) === i)` takes **$O(n^2)$** time because `indexOf` performs an $O(n)$ scan for every element in the array.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Inverting a Multimap
Write a function `invertMap(map)` that takes a `Map` where values are arrays of strings and returns a new `Map` where each string maps back to an array of keys that contained it.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function invertMap(sourceMap) {
  const inverted = new Map();

  for (const [key, values] of sourceMap) {
    for (const val of values) {
      if (!inverted.has(val)) {
        inverted.set(val, []);
      }
      inverted.get(val).push(key);
    }
  }

  return inverted;
}

const userRoles = new Map([
  ["Alice", ["Admin", "Editor"]],
  ["Bob", ["Editor"]],
  ["Charlie", ["Viewer"]]
]);

console.log(invertMap(userRoles));
// Map(3) {
//   'Admin' => [ 'Alice' ],
//   'Editor' => [ 'Alice', 'Bob' ],
//   'Viewer' => [ 'Charlie' ]
// }
```
</details>

### Exercise 2: Implementing Set Symmetric Difference Polyfill
Write a standalone polyfill function `symmetricDifference(setA, setB)` that returns a new `Set` containing elements that are in either `setA` or `setB`, but not both.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function symmetricDifference(setA, setB) {
  const result = new Set(setA);
  for (const elem of setB) {
    if (result.has(elem)) {
      result.delete(elem);
    } else {
      result.add(elem);
    }
  }
  return result;
}

const a = new Set([1, 2, 3]);
const b = new Set([3, 4, 5]);
console.log([...symmetricDifference(a, b)]); // [1, 2, 4, 5]
```
</details>

### Exercise 3: Grouping Consecutive Map Entries
Implement a function `groupAdjacentKeys(map, predicate)` that collapses consecutive entries whose values satisfy `predicate` into a single combined array.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function groupAdjacentKeys(map) {
  const groups = [];
  let currentGroup = null;

  for (const [key, val] of map) {
    if (currentGroup && currentGroup.type === val) {
      currentGroup.keys.push(key);
    } else {
      currentGroup = { type: val, keys: [key] };
      groups.push(currentGroup);
    }
  }

  return groups;
}

const logMap = new Map([
  [1, "INFO"],
  [2, "INFO"],
  [3, "WARN"],
  [4, "INFO"]
]);

console.log(groupAdjacentKeys(logMap));
// [
//   { type: 'INFO', keys: [ 1, 2 ] },
//   { type: 'WARN', keys: [ 3 ] },
//   { type: 'INFO', keys: [ 4 ] }
// ]
```
</details>
