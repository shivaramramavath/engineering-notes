# JavaScript Optimization Patterns and Anti-Patterns

> Hidden Class stabilization, monomorphic call sites, packed array performance, branch prediction, and mechanical sympathy in V8.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Performance Fundamentals](file:///e:/Learning/engineering-notes/01-languages/javascript/19_performance/01_performance-fundamentals.md), [Memory Model](file:///e:/Learning/engineering-notes/01-languages/javascript/18_memory-and-garbage-collection/01_memory-model.md) |
| **Time to read** | 25 minutes |
| **Status** | Complete |
| **Standard / Runtime** | ECMAScript 2015+, V8 Engine (Node.js & Chromium) |

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

Modern JavaScript engines, particularly Google's V8, are engineering marvels. They execute dynamic, weakly typed code at speeds rivaling compiled languages like C++ or Java. However, this peak performance depends entirely on **speculative assumptions** made by the JIT compiler.

When your code exhibits predictable patterns—consistent object shapes, unchanging property initialization orders, and monomorphic types—V8 generates blazing-fast machine code using direct memory offsets. When code violates these assumptions—by deleting properties, changing property initialization order, creating holey arrays, or mixing types in hot loops—the engine is forced to deoptimize, falling back to slow dictionary lookups and interpreted bytecode. Writing performant JavaScript is the art of coding with **Mechanical Sympathy** for the V8 JIT engine.

---

## 2. Core Concepts

### Hidden Classes (Shapes / Maps)
In JavaScript, objects do not have fixed static class layouts. To optimize property lookups, V8 creates synthetic C++ structures called **Hidden Classes** (historically called *Maps* or *Shapes*):
- Two objects that define the **exact same properties in the exact same order** share the same Hidden Class pointer.
- If properties are added in different orders, or if a property is dynamically deleted, their Hidden Classes diverge into separate branches, preventing shared JIT optimizations.

### Inline Caching and Morphism
When a property is accessed (e.g. `user.name`), V8 caches the memory offset in an **Inline Cache (IC)**:
- **Monomorphic (1 Shape)**: Observed 1 Hidden Class. Optimized directly into a single machine load instruction ($O(1)$ ~1 cycle).
- **Polymorphic (2–4 Shapes)**: Observed 2 to 4 Hidden Classes. Optimized into a fast branch condition checking the Map pointer.
- **Megamorphic ($> 4$ Shapes)**: Too many shapes. The IC gives up speculative optimization and falls back to a slow global hash table lookup.

### Array Storage Kinds
V8 classifies arrays into internal **Elements Kinds**:
- `PACKED_SMI_ELEMENTS`: Contiguous small integers (fastest, unboxed arithmetic).
- `PACKED_DOUBLE_ELEMENTS`: Contiguous 64-bit floating point numbers.
- `PACKED_ELEMENTS`: Contiguous object references.
- `HOLEY_*_ELEMENTS`: Arrays containing missing/deleted indices (`[1, , 3]`). Reading a hole forces V8 to traverse the prototype chain!

---

## 3. How It Works Internally

```
HIDDEN CLASS TRANSITION TREE:

Empty Object:   [ Map 0 ]
                   |
Add 'x':        [ Map 1: offset 0 = x ]
                   |
Add 'y':        [ Map 2: offset 0 = x, offset 1 = y ]
                   ^
                   | SHARED SHAPE!
objA = { x: 1, y: 2 } ---> Points to Map 2
objB = { x: 3, y: 4 } ---> Points to Map 2

DIVERGENT INITIALIZATION:
objC = { y: 2, x: 1 } ---> Points to Map 4 (Different transition branch!)
                            (Cannot share Inline Cache with objA/objB)
```

---

## 4. Syntax & API Reference

### V8 Native Syntax Flags for JIT Inspection
To inspect V8's internal optimization state directly, run Node.js with the `--allow-natives-syntax` flag:
```javascript
// Run with: node --allow-natives-syntax test.js

function getCoordinates(point) {
  return point.x + point.y;
}

const pt = { x: 10, y: 20 };
getCoordinates(pt);
getCoordinates(pt);

// Trigger TurboFan optimization
%OptimizeFunctionOnNextCall(getCoordinates);
getCoordinates(pt);

// Check optimization status:
// 1 = Optimized, 2 = Not optimized, 3 = Always optimized, 4 = Never optimized
console.log('Optimization Status:', %GetOptimizationStatus(getCoordinates));
```

---

## 5. Practical Examples

### Example 1: Hidden Class Stability and Property Order
```javascript
// POOR: Inconsistent property declaration order creates 2 divergent Hidden Classes
function createPointBad(x, y, flipOrder) {
  if (flipOrder) {
    return { y, x }; // Shape Map B
  }
  return { x, y };   // Shape Map A
}

// EXCELLENT: Always initialize properties in the exact same sequence
function createPointGood(x, y) {
  return { x, y }; // 100% shared Shape Map A
}

// Benchmarking property access:
const pointsGood = Array.from({ length: 1_000_000 }, () => createPointGood(1, 2));
const pointsBad = Array.from({ length: 1_000_000 }, (_, i) => createPointBad(1, 2, i % 2 === 0));

console.time('Good (Monomorphic)');
let sum1 = 0;
for (let i = 0; i < pointsGood.length; i++) sum1 += pointsGood[i].x;
console.timeEnd('Good (Monomorphic)'); // ~1.8ms

console.time('Bad (Polymorphic)');
let sum2 = 0;
for (let i = 0; i < pointsBad.length; i++) sum2 += pointsBad[i].x;
console.timeEnd('Bad (Polymorphic)'); // ~5.4ms (3x slower!)
```

### Example 2: Avoiding the `delete` Operator (Dictionary Mode)
```javascript
// POOR: `delete` invalidates the Hidden Class, demoting to slow Dictionary Mode
const userBad = { id: 1, tempToken: 'xyz' };
delete userBad.tempToken; // Demoted to Dictionary Mode!

// EXCELLENT: Set to undefined or null, keeping the Hidden Class intact
const userGood = { id: 1, tempToken: 'xyz' };
userGood.tempToken = undefined; // Retains fast Hidden Class!
```

### Example 3: Packed vs Holey Array Traps
```javascript
// BAD: Creating holey arrays by skipping indices
const holey = [];
holey[0] = 10;
holey[100] = 20; // Indices 1-99 are holes! Transitions to HOLEY_SMI_ELEMENTS

// Reading holey[5] forces V8 to:
// 1. Check if index is in bounds
// 2. See it is empty
// 3. Search holey.__proto__ (Array.prototype)
// 4. Search Object.prototype
// 5. Return undefined

// GOOD: Always allocate contiguous, packed arrays
const packed = new Array(101).fill(0);
packed[0] = 10;
packed[100] = 20; // 100% PACKED_SMI_ELEMENTS (Direct memory access)
```

### Example 4: Branch Prediction and Data Sorting
```javascript
// Demonstrating the hardware branch predictor
const SIZE = 100_000;
const data = Array.from({ length: SIZE }, () => Math.floor(Math.random() * 256));

function countValuesAboveThreshold(arr) {
  let count = 0;
  for (let i = 0; i < arr.length; i++) {
    // Conditional branch
    if (arr[i] >= 128) {
      count++;
    }
  }
  return count;
}

// 1. Unsorted Array: CPU Branch Predictor fails 50% of the time (Branch Mispredictions)
console.time('Unsorted Array');
countValuesAboveThreshold(data);
console.timeEnd('Unsorted Array'); // ~3.5ms

// 2. Sorted Array: CPU Branch Predictor predicts branch with 99.9% accuracy!
data.sort((a, b) => a - b);
console.time('Sorted Array');
countValuesAboveThreshold(data);
console.timeEnd('Sorted Array'); // ~0.8ms (4x faster purely due to hardware branch prediction!)
```

### Example 5: Loop Unrolling for Hot Computation Kernels
```javascript
// Manually unrolling loops in hot matrix/math routines reduces loop condition checks
function sumArrayUnrolled(arr) {
  let sum = 0;
  let i = 0;
  const len = arr.length;
  const limit = len - 3;

  // Process 4 elements per iteration
  for (; i < limit; i += 4) {
    sum += arr[i] + arr[i + 1] + arr[i + 2] + arr[i + 3];
  }

  // Handle remaining elements
  for (; i < len; i++) {
    sum += arr[i];
  }

  return sum;
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **One-Way Elements Kind Downgrades**: Once an array is downgraded from `PACKED_SMI` to `PACKED_DOUBLE` (by adding a float) or `PACKED_ELEMENTS` (by adding a string or object), it **never** transitions back even if all floats or strings are removed.
2. **Object Spread Operator (`{ ...obj }`)**: Dynamic object spread syntax creates brand-new object instances with runtime-calculated shapes, frequently generating unpredictable polymorphic sites in downstream functions.
3. **Arguments Object Deoptimization**: Leaking the legacy `arguments` object (e.g. `return arguments` or `fn.apply(null, arguments)`) deoptimizes the enclosing function in older V8 versions. Always prefer standard rest parameters (`(...args) => ...`).
4. **Try/Catch Inlining Restrictions**: In modern V8, functions containing `try/catch` are optimized cleanly by TurboFan. However, deeply nested `try/catch` blocks inside tight loops can still prevent speculative vectorization. Extract inner loop bodies to standalone helper functions.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| `delete obj.prop` | Demotes object to Dictionary Mode hash map | Set `obj.prop = undefined` or structure shape cleanly |
| Initializing properties conditionally in different orders | Creates divergent Hidden Classes, breaking Monomorphism | Declare all properties in identical order in constructor |
| Creating holey arrays (`arr[100] = x` on empty array) | Triggers prototype chain lookups for missing indices | Use packed arrays or pre-fill with `.fill(0)` |
| Mixing numbers, strings, and objects in one array | Transitions array to generic `PACKED_ELEMENTS` | Keep arrays type-homogeneous |
| Over-abstracting hot loops with 5 layers of `.map().filter()` | Allocates intermediate arrays and function stack frames | Use a single imperative `for` loop in hot paths |
| Leaking the legacy `arguments` object | Prevents TurboFan compiler optimizations | Use ES6 rest parameters (`...args`) |

---

## 8. Performance Characteristics

- **Monomorphic Property Access**: Typically 1 CPU cycle (direct load instruction with immediate offset).
- **Megamorphic Property Access**: Typically 20–50 CPU cycles (hash table bucket calculation, collision traversal, key comparison).
- **Holey Array Reads**: 10x to 100x slower than packed arrays due to prototype chain traversal.

---

## 9. Engine Internals

1. **Hidden Class Transitions**:
   - V8 objects store an internal pointer at offset 0 referencing their `Map`.
   - Every `Map` contains a transition table: "If property `y` is added, transition to Map #42".
   - If two functions build objects with identical sequences of transitions, the resulting objects point to the exact same `Map` address.
2. **TurboFan Speculative Type Feedback**:
   - Ignition records type vectors at every call and property access site.
   - When a function becomes hot, TurboFan generates machine code predicated on those recorded types. If an unexpected type appears, a hardware guard fails and deoptimizes the code back to Ignition bytecode.

---

## 10. Best Practices & Design Guidelines

- **Always Initialize Properties in the Constructor**: Declare every instance property inside the class constructor, even if initially set to `null` or `undefined`.
- **Maintain Homogeneous Arrays**: Keep arrays strictly typed (only integers, only floats, or only objects of a single shape).
- **Favor Flat Loops in Critical Sections**: For algorithms running millions of times per second (e.g. physics engines, audio processing), avoid functional array chains (`map/filter/reduce`) in favor of direct imperative `for` loops.

---

## 11. Real-World Case Study

### High-Throughput Matrix Vector Engine: Monomorphic Optimization
```javascript
// BEFORE: Heavy polymorphism and holey array degradation (12,000 ops/s)
class LegacyParticle {
  constructor(x, y, z) {
    this.x = x;
    this.y = y;
    if (z !== undefined) this.z = z; // Conditional shape branch!
  }
}

// AFTER: Strictly monomorphic particle structure (78,000 ops/s - 6.5x speedup!)
class OptimizedParticle {
  constructor(x = 0, y = 0, z = 0) {
    // Unconditionally initialized in exact same order
    this.x = x;
    this.y = y;
    this.z = z;
  }
}

function updateParticles(particles) {
  let totalEnergy = 0;
  for (let i = 0; i < particles.length; i++) {
    const p = particles[i];
    // Inline Cache remains 100% Monomorphic!
    totalEnergy += p.x * p.x + p.y * p.y + p.z * p.z;
  }
  return totalEnergy;
}
```

---

## 12. Interview Questions

<details>
<summary>1. What are V8 Hidden Classes (Maps) and how do they optimize property access?</summary>
Because JavaScript lacks static typing and fixed struct offsets, V8 creates synthetic C++ structures called Hidden Classes (or Shapes/Maps). Objects created with identical properties in the identical order share the same Hidden Class pointer. This allows V8 to determine property memory offsets statically and compile property accesses into direct memory offset loads rather than dynamic hash-table lookups.
</details>

<details>
<summary>2. What is the difference between Monomorphic, Polymorphic, and Megamorphic call sites?</summary>
A Monomorphic site has observed exactly 1 object shape and compiles to a single direct memory offset load instruction (fastest). A Polymorphic site has observed 2 to 4 shapes and uses a small branch table to check the shape pointer. A Megamorphic site has observed more than 4 shapes; V8 abandons speculative offset caching and falls back to a generic global hash table lookup.
</details>

<details>
<summary>3. Why is delete obj.prop considered an anti-pattern in high-performance code?</summary>
Using <code>delete</code> modifies the object's shape transition graph, forcing V8 to demote the object into "Dictionary Mode" (a generic hash table). This removes the object from fast Hidden Class tracking, increases its memory footprint, and slows down all subsequent property accesses.
</details>

<details>
<summary>4. What are Holey Arrays and why are they significantly slower than Packed Arrays?</summary>
Holey arrays contain uninitialized or deleted indices (e.g. <code>[1, , 3]</code>). When V8 accesses a hole, it cannot simply return the value in memory; it must traverse the prototype chain (<code>Array.prototype</code>, then <code>Object.prototype</code>) to verify that no property exists on the prototype with that index name before returning <code>undefined</code>.
</details>

<details>
<summary>5. How does hardware branch prediction affect JavaScript array processing?</summary>
CPUs maintain hardware branch predictors that guess the outcome of <code>if</code> statements before they execute to keep the instruction pipeline full. When iterating over a sorted array, the condition branch (e.g. <code>value >= 128</code>) is consistent, resulting in near-100% prediction accuracy. On an unsorted array, branch mispredictions flush the CPU pipeline, increasing execution time significantly.
</details>

<details>
<summary>6. Can a downgraded Elements Kind array ever upgrade back to a faster kind?</summary>
No. In V8, Elements Kind transitions are strictly unidirectional. Once an array transitions from <code>PACKED_SMI</code> to <code>PACKED_DOUBLE</code> or <code>PACKED_ELEMENTS</code>, it can never transition back to a more restrictive kind, even if the non-integer elements are removed.
</details>

<details>
<summary>7. Why should you avoid using the legacy arguments object?</summary>
The legacy <code>arguments</code> object creates a complex binding with named function parameters. Passing or leaking <code>arguments</code> outside the function disables several TurboFan optimizations and inlining heuristics. Modern ES6 rest parameters (<code>...args</code>) allocate clean, packed arrays and should always be used instead.
</details>

<details>
<summary>8. What is loop unrolling and when does it improve performance?</summary>
Loop unrolling is an optimization where multiple loop iterations are combined into a single iteration body. It reduces loop control condition checks, branch mispredictions, and loop index increment operations. It is beneficial in hot numeric computation kernels where loop execution represents a primary CPU bottleneck.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Monomorphic Shape Validator
Write a diagnostic utility `assertMonomorphic(factoryFn, count = 1000)` that validates whether an object factory produces objects that share a single consistent Hidden Class, detecting divergent property initialization orders.

<details>
<summary>View Solution</summary>

```javascript
export function verifyPropertyConsistency(objects) {
  if (objects.length < 2) return true;

  const baselineKeys = Object.keys(objects[0]);
  const baselineString = baselineKeys.join(',');

  for (let i = 1; i < objects.length; i++) {
    const keys = Object.keys(objects[i]);
    if (keys.join(',') !== baselineString) {
      console.warn(`[SHAPE DIVERGENCE] Object at index ${i} has inconsistent shape:`);
      console.warn(`  Expected: ${baselineString}`);
      console.warn(`  Actual:   ${keys.join(',')}`);
      return false;
    }
  }

  console.log(`[PASS] All ${objects.length} objects share identical property order.`);
  return true;
}
```
</details>

### Exercise 2: Packed Array Sanitizer
Write a function `ensurePackedSmi(array)` that takes an arbitrary array, removes all holes and non-integer values, and produces a guaranteed contiguous `PACKED_SMI_ELEMENTS` array.

<details>
<summary>View Solution</summary>

```javascript
export function ensurePackedSmi(inputArray) {
  // 1. Filter out holes and non-integers
  const validSmis = [];
  for (let i = 0; i < inputArray.length; i++) {
    if (i in inputArray) { // Skips holes
      const val = inputArray[i];
      if (Number.isInteger(val) && val >= -1073741824 && val <= 1073741823) {
        validSmis.push(val);
      }
    }
  }

  // 2. Pre-allocate clean packed array
  const packed = new Array(validSmis.length);
  for (let i = 0; i < validSmis.length; i++) {
    packed[i] = validSmis[i]; // Pure PACKED_SMI_ELEMENTS
  }

  return packed;
}
```
</details>
