# Arrays & Objects API Cheatsheet

> Quick-reference guide covering modern ECMAScript array transformation methods, non-mutating copy utilities, object reflection, and structured cloning.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Level** | Reference / Quick Lookup |

---

## 1. Non-Mutating Array Methods (ES2023+)

| Method | Syntax | Return Value | Original Array Mutated? |
| :--- | :--- | :--- | :--- |
| `toSorted()` | `arr.toSorted(compareFn)` | New sorted array | **No** (Non-mutating) |
| `toReversed()` | `arr.toReversed()` | New reversed array | **No** (Non-mutating) |
| `toSpliced()` | `arr.toSpliced(start, deleteCount, ...items)` | New spliced array | **No** (Non-mutating) |
| `with()` | `arr.with(index, value)` | New array with index replaced | **No** (Non-mutating) |

```javascript
const numbers = [3, 1, 4, 1, 5];
const sorted = numbers.toSorted((a, b) => a - b); // [1, 1, 3, 4, 5]
const replaced = numbers.with(0, 99);             // [99, 1, 4, 1, 5]
console.log(numbers);                             // [3, 1, 4, 1, 5] (Unchanged!)
```

---

## 2. Array Search & Traversal Methods

| Method | Signature | Purpose | Time Complexity |
| :--- | :--- | :--- | :--- |
| `at()` | `arr.at(index)` | Supports negative indices (`arr.at(-1)`) | $O(1)$ |
| `findLast()` | `arr.findLast(predicate)` | Finds last matching element from right | $O(N)$ |
| `findLastIndex()` | `arr.findLastIndex(predicate)`| Finds last matching index from right | $O(N)$ |
| `includes()` | `arr.includes(val, fromIndex)` | Value presence check (detects `NaN`) | $O(N)$ |
| `flat()` | `arr.flat(depth = 1)` | Flattens nested arrays | $O(N)$ |
| `flatMap()` | `arr.flatMap(fn)` | Map followed by flat of depth 1 | $O(N)$ |

```javascript
const items = [{ id: 1 }, { id: 2 }, { id: 1 }];
const lastOne = items.findLast((i) => i.id === 1); // { id: 1 } (at index 2)
const lastElement = items.at(-1);                 // Equivalent to items[items.length - 1]
```

---

## 3. Object Grouping & Transformation (ES2024+)

| Static Method | Syntax | Output Structure |
| :--- | :--- | :--- |
| `Object.groupBy()` | `Object.groupBy(iterable, callback)` | Plain object with prototype `null` |
| `Map.groupBy()` | `Map.groupBy(iterable, callback)` | Keyed `Map` instance (supports object keys) |
| `Object.hasOwn()` | `Object.hasOwn(obj, prop)` | Safe replacement for `hasOwnProperty` |
| `Object.fromEntries()` | `Object.fromEntries(iterable)` | Object from key-value pairs |
| `structuredClone()` | `structuredClone(value)` | Native deep-clone (supports circular refs) |

```javascript
const inventory = [
  { name: 'asparagus', type: 'vegetables', quantity: 5 },
  { name: 'bananas', type: 'fruit', quantity: 0 },
  { name: 'cherries', type: 'fruit', quantity: 5 }
];

// Grouping by type:
const byType = Object.groupBy(inventory, (item) => item.type);
/*
{
  vegetables: [{ name: 'asparagus', ... }],
  fruit: [{ name: 'bananas', ... }, { name: 'cherries', ... }]
}
*/

// Deep Clone:
const original = { a: 1, nested: { b: 2 } };
original.self = original;
const copy = structuredClone(original);
console.log(copy !== original && copy.nested !== original.nested); // true
```

---

## 4. Key Object Utilities & Reflection

```javascript
// Safe Own-Property Verification:
Object.hasOwn(obj, 'key'); // true or false (Safe against Object.create(null))

// Property Iterators:
Object.keys(obj);          // Array of own enumerable string keys
Object.values(obj);        // Array of own enumerable property values
Object.entries(obj);       // Array of [key, value] pairs
Reflect.ownKeys(obj);      // Includes non-enumerable and Symbol keys

// Freeze & Seal:
Object.freeze(obj);        // Prevents adding, removing, or mutating properties
Object.seal(obj);          // Prevents adding/removing; allows mutating existing values
Object.isFrozen(obj);      // true if frozen
```
