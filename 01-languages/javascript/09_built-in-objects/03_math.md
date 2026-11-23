# JavaScript Math: Constants, Rounding Semantics, PRNG Mechanics & Geometry Calculations

> In-depth technical reference on ECMAScript `Math` namespace, rounding algorithms (`floor`, `ceil`, `trunc`, `round`), trigonometric vectors, PRNG internals (`xorshift128+`), and TurboFan assembly intrinsics.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Numerical Computing |
| **Prerequisites** | [09_built-in-objects/02_number-and-bigint.md](02_number-and-bigint.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 (Math.trunc, cbrt, hypot, clz32, sign, log2, log10) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Static Namespace & Mathematical Constants](#2-core-concepts-static-namespace--mathematical-constants)
3. [How It Works: Engine Implementation & Math Intrinsics](#3-how-it-works-engine-implementation--math-intrinsics)
4. [Comprehensive Syntax & API Reference](#4-comprehensive-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 xorshift128+ PRNG & TurboFan Inlining](#9-engine-internals-v8-xorshift128-prng--turbofan-inlining)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: 2D Vector Physics Engine & Spatial Clamping](#11-real-world-case-study-2d-vector-physics-engine--spatial-clamping)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

In JavaScript, all common mathematical constants, trigonometry, logarithms, power functions, and rounding utilities are encapsulated inside the **`Math`** namespace object.

Unlike `Number` or `Date`, `Math` is **not a constructor**. It cannot be instantiated with `new Math()`, nor can it be invoked as a function `Math()`. Doing either throws a `TypeError`. Instead, `Math` is a static namespace object (similar to `JSON` and `Atomics`) containing pure, immutable mathematical utility functions.

Modern JavaScript applications rely heavily on `Math` for:
1. **Accurate Numerical Rounding:** Distinguishing between standard mathematical rounding, flooring, ceiling, and integer truncation.
2. **Computational Geometry & Physics:** Calculating Euclidean distances, angles, vectors, and bounding-box coordinates via `Math.hypot` and `Math.atan2`.
3. **Data Science & Statistics:** Logarithmic transforms, exponential scaling, and clamping algorithms.

---

## 2. Core Concepts: Static Namespace & Mathematical Constants

### 2.1 Static Constants
The `Math` object exposes eight immutable floating-point mathematical constants:

| Property | Value | Description |
| :--- | :--- | :--- |
| `Math.PI` | $\approx 3.141592653589793$ | Ratio of a circle's circumference to its diameter |
| `Math.E` | $\approx 2.718281828459045$ | Euler's constant; base of natural logarithms |
| `Math.LN2` | $\approx 0.6931471805599453$ | Natural logarithm of 2 |
| `Math.LN10` | $\approx 2.302585092994046$ | Natural logarithm of 10 |
| `Math.LOG2E` | $\approx 1.4426950408889634$ | Base-2 logarithm of $e$ |
| `Math.LOG10E` | $\approx 0.4342944819032518$ | Base-10 logarithm of $e$ |
| `Math.SQRT2` | $\approx 1.4142135623730951$ | Square root of 2 |
| `Math.SQRT1_2`| $\approx 0.7071067811865476$ | Square root of $1/2$ ($\frac{1}{\sqrt{2}}$) |

### 2.2 Rounding Strategies
Choosing the correct rounding function is critical for financial and game-loop engineering:
- **`Math.floor(x)`:** Returns the largest integer $\le x$ (rounds towards $-\infty$).
- **`Math.ceil(x)`:** Returns the smallest integer $\ge x$ (rounds towards $+\infty$).
- **`Math.trunc(x)`:** Strips fractional digits, rounding strictly towards $0$.
- **`Math.round(x)`:** Rounds to the nearest integer. Note: ties ($0.5$) round towards $+\infty$.

```
 Value     floor()    ceil()    trunc()    round()
--------------------------------------------------
  3.7        3          4          3          4
  3.5        3          4          3          4
  3.2        3          4          3          3
 -3.2       -4         -3         -3         -3
 -3.5       -4         -3         -3         -3  <-- Gotcha! Rounds towards +inf
 -3.7       -4         -3         -3         -4
```

---

## 3. How It Works: Engine Implementation & Math Intrinsics

Modern engines (such as V8) do not evaluate `Math` methods using interpreted JavaScript loops. Instead, TurboFan maps most `Math` methods directly to **hardware CPU intrinsics**:

```
 JavaScript Call: Math.sqrt(x)
              |
              v
 V8 TurboFan Optimization Pass
              |
              v
 Machine Operator: Float64Sqrt
              |
              v
 Native CPU Assembly (x86_64 SSE2):
 SQRTSD xmm0, xmm1    ; Single-cycle hardware instruction!
```

- Methods like `Math.sqrt`, `Math.abs`, `Math.min`, and `Math.max` compile down to single CPU instructions (`SQRTSD`, `ANDPD`, `MINSD`, `MAXSD`).
- Complex transcendental functions (`Math.sin`, `Math.cos`, `Math.log`) link directly to highly optimized C libm implementations or vectorized assembly routines (FDLIBM).

---

## 4. Comprehensive Syntax & API Reference

### 4.1 Powers, Roots & Exponents
```javascript
console.log(Math.pow(2, 10)); // 1024 (Equivalent to 2 ** 10)
console.log(Math.sqrt(64));    // 8
console.log(Math.cbrt(27));    // 3 (Cube root)
console.log(Math.exp(1));      // 2.718281828459045 (e^1)
console.log(Math.expm1(0));    // 0 (e^x - 1, high precision for small x)
console.log(Math.log(Math.E)); // 1 (Natural log)
console.log(Math.log10(100));  // 2 (Base-10 log)
console.log(Math.log2(8));     // 3 (Base-2 log)
```

### 4.2 Geometry & Trigonometry
```javascript
// Euclidean distance in N-dimensions: sqrt(x^2 + y^2 + ...)
console.log(Math.hypot(3, 4));       // 5
console.log(Math.hypot(1, 2, 2));    // 3 (3D distance)

// Angles in radians:
const rad = Math.PI / 4; // 45 degrees
console.log(Math.sin(rad)); // ~0.7071
console.log(Math.cos(rad)); // ~0.7071
console.log(Math.tan(rad)); // ~1.0

// Atan2: Computes angle from origin to (x, y) preserving quadrant
console.log(Math.atan2(1, 1)); // 0.785398 (PI / 4)
```

### 4.3 Sign & Bitwise Math
```javascript
console.log(Math.sign(50));   // 1
console.log(Math.sign(-50));  // -1
console.log(Math.sign(0));    // 0
console.log(Math.sign(-0));   // -0
console.log(Math.sign(NaN));  // NaN

// Count leading zero bits in 32-bit int representation:
console.log(Math.clz32(1));   // 31
console.log(Math.clz32(1000));// 22
```

---

## 5. Practical Examples

### Example 1: Cryptographically Secure vs Pseudo-Random Numbers
```javascript
// Pseudo-random (Fast, non-cryptographic):
function getRandomInt(min, max) {
  min = Math.ceil(min);
  max = Math.floor(max);
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

console.log("PRNG Dice Roll:", getRandomInt(1, 6));

// Cryptographically Secure PRNG (For tokens, passwords, security):
function getCryptoRandomInt(min, max) {
  const range = max - min + 1;
  const bytesNeeded = Math.ceil(Math.log2(range) / 8) || 1;
  const maxValid = Math.pow(256, bytesNeeded) - (Math.pow(256, bytesNeeded) % range);
  const buffer = new Uint8Array(bytesNeeded);

  while (true) {
    crypto.getRandomValues(buffer);
    let val = 0;
    for (let i = 0; i < bytesNeeded; i++) val = (val << 8) | buffer[i];
    if (val < maxValid) return min + (val % range);
  }
}

console.log("Crypto-safe Dice Roll:", getCryptoRandomInt(1, 6));
```

### Example 2: Spatial Clamping & Normalization
```javascript
// Restrict value within [min, max] boundary:
function clamp(value, min, max) {
  return Math.min(Math.max(value, min), max);
}

// Map value from one range [inMin, inMax] to [outMin, outMax]:
function mapRange(value, inMin, inMax, outMin, outMax) {
  const clampedVal = clamp(value, inMin, inMax);
  return outMin + ((clampedVal - inMin) / (inMax - inMin)) * (outMax - outMin);
}

console.log(clamp(150, 0, 100)); // 100
console.log(clamp(-25, 0, 100)); // 0
console.log(mapRange(75, 0, 100, 0, 1)); // 0.75 (e.g., volume slider normalization)
```

### Example 3: 2D Rotation & Direction Vector
```javascript
class Vector2D {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }

  magnitude() {
    return Math.hypot(this.x, this.y);
  }

  angle() {
    return Math.atan2(this.y, this.x); // Radians
  }

  rotate(radians) {
    const cos = Math.cos(radians);
    const sin = Math.sin(radians);
    return new Vector2D(
      this.x * cos - this.y * sin,
      this.x * sin + this.y * cos
    );
  }
}

const v = new Vector2D(1, 0); // Unit vector pointing right
const rotated = v.rotate(Math.PI / 2); // Rotate 90 degrees CCW
console.log(Math.round(rotated.x), Math.round(rotated.y)); // 0, 1 (Points up)
```

### Example 4: Linear Interpolation (Lerp) with Damping
```javascript
function lerp(start, end, t) {
  return start + (end - start) * t;
}

// Smooth camera motion / animation damping:
let cameraX = 0;
const targetX = 100;

for (let frame = 1; frame <= 5; frame++) {
  cameraX = lerp(cameraX, targetX, 0.2);
  console.log(`Frame ${frame}: cameraX = ${cameraX.toFixed(2)}`);
}
// Outputs:
// Frame 1: cameraX = 20.00
// Frame 2: cameraX = 36.00
// Frame 3: cameraX = 48.80
// Frame 4: cameraX = 59.04
// Frame 5: cameraX = 67.23
```

### Example 5: High-Precision Hypotenuse without Overflow
```javascript
// In naive Math.sqrt(x*x + y*y), huge numbers overflow to Infinity:
const huge = 1e200;
console.log(Math.sqrt(huge * huge + huge * huge)); // Infinity (Overflow!)

// Math.hypot scales internally to prevent intermediate overflow:
console.log(Math.hypot(huge, huge)); // 1.414213562373095e+200 (Exact!)
```

---

## 6. Edge Cases, Pitfalls & Gotchas

### 6.1 `Math.min()` and `Math.max()` with No Arguments
When invoked with zero arguments, the identity elements defined by IEEE-754 are returned:
```javascript
console.log(Math.min()); //  Infinity
console.log(Math.max()); // -Infinity
```
This is mathematically sound ($x > -\infty$ for any $x$, so $-\infty$ is the base accumulator for `max`), but frequently catches developers off guard when spreading empty arrays:
```javascript
const items = [];
const largest = Math.max(...items); // Evaluates to -Infinity!
```

### 6.2 Stack Overflow Spreading Huge Arrays into `Math.max`
The JavaScript call stack has a fixed argument limit (~65,536 elements in V8). Spreading large arrays into `Math.min` or `Math.max` crashes the process with `RangeError: Maximum call stack size exceeded`:
```javascript
const hugeArray = new Array(200_000).fill(1);
// Throws RangeError:
// const maxVal = Math.max(...hugeArray);

// Safe alternative:
const maxVal = hugeArray.reduce((max, val) => (val > max ? val : max), -Infinity);
```

### 6.3 Negative Number Rounding Invariant in `Math.round`
`Math.round(-1.5)` evaluates to `-1`, **not** `-2`. This is because the ECMAScript specification defines `Math.round(x)` as rounding half-values strictly towards $+\infty$:
```javascript
console.log(Math.round(1.5));  // 2  (1.5 + 0.5 = 2)
console.log(Math.round(-1.5)); // -1 (-1.5 + 0.5 = -1)
```

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Bad Practice | Idiomatic Solution | Reason |
| :--- | :--- | :--- | :--- |
| **Using `new Math()`** | `const m = new Math();` | Call static methods: `Math.sqrt(x)` | `Math` is a namespace, not a constructor; `new` throws `TypeError`. |
| **Using `Math.random()` for Security** | Generating API tokens or passwords via `Math.random()` | `crypto.getRandomValues()` or `crypto.randomUUID()` | `Math.random` uses a deterministic PRNG; vulnerable to prediction attacks. |
| **Spreading Unbounded Arrays into `Math.max`** | `Math.max(...largeArray)` | `largeArray.reduce(...)` or a standard loop | Causes `RangeError: Maximum call stack size exceeded`. |
| **Manual Hypotenuse Calculation** | `Math.sqrt(x*x + y*y)` | `Math.hypot(x, y)` | Manual multiplication overflows to `Infinity` on large coordinates. |
| **Using `Math.trunc` via `\| 0` on large floats** | `val \| 0` | `Math.trunc(val)` | Bitwise `\| 0` truncates to 32 bits, overflowing numbers $> 2^{31}-1$. |
| **Assuming `Math.round` is symmetric** | Assuming `-2.5` rounds to `-3` | Check sign or use custom round-half-away-from-zero | `Math.round` always rounds ties towards $+\infty$. |

---

## 8. Performance Characteristics

1. **TurboFan Inlining:**
   - Methods like `Math.abs`, `Math.min`, `Math.max`, `Math.floor`, and `Math.sqrt` are directly inlined by TurboFan into native assembly registers, with zero function call overhead.
2. **`Math.hypot` vs `Math.sqrt(x*x + y*y)`:**
   - For small coordinates where overflow is impossible, `Math.sqrt(x * x + y * y)` can be $1.5\times$ faster than `Math.hypot` because `hypot` includes internal scaling checks to prevent overflow and underflow.
3. **Array Bounds Checking:**
   - In tight game rendering loops, avoid spreading arrays into `Math.min(...points)`. Iterating with a direct `for` loop is an order of magnitude faster and allocates zero stack frames.

---

## 9. Engine Internals: V8 xorshift128+ PRNG & TurboFan Inlining

### V8's `Math.random()` Engine:
Prior to 2015, V8 used MWC1616 (Multiply-With-Carry), which suffered from short period cycles and flawed randomness distributions.
Modern V8 implements **xorshift128+**:
- Maintains two 64-bit state words ($128$ bits total state) per V8 isolate.
- Cycle period is $2^{128} - 1$ steps before repeating.
- Generates 64 bits of pseudo-random data per step and converts the lowest 52 bits into an IEEE-754 mantissa divided by $2^{52}$ to produce a float in $[0, 1)$.

```
   State S0, S1 (128 bits)
             |
   xorshift operations:
   s1 ^= s1 << 23;
   s1 ^= s1 >> 17;
   s1 ^= s0 ^ (s0 >> 26);
             |
   Result = (S0 + S1) >> 12  --> Float in [0, 1)
```
*(Note: Because the 128-bit internal state can be deduced from observing a small sequence of outputs, `Math.random` must never be used for encryption or authentication tokens).*

---

## 10. Best Practices & Design Guidelines

1. **Use `Math.trunc()` for Pure Integer Extraction:** Prefer `Math.trunc(x)` over `Math.floor(x)` or `x | 0` when you want to discard decimal places from positive and negative numbers safely.
2. **Never Use `Math.random` for Authentication:** Always use the Web Crypto API (`crypto.getRandomValues()` or `crypto.randomUUID()`) for session tokens, password generation, and cryptographic nonces.
3. **Use `Math.hypot()` for Distance Computations:** Avoid manual square-root summation to prevent intermediate floating-point overflow.
4. **Guard Empty Spreads in `Math.min`/`Math.max`:** Always provide a default or fallback when calculating bounds across collections that may be empty.

---

## 11. Real-World Case Study: 2D Vector Physics Engine & Spatial Clamping

### Problem
A canvas-based 2D gaming simulation suffered from entity tunneling (projectiles passing straight through boundaries) and erratic behavior when entities rotated near axis boundaries. The developer used manual trigonometry with `Math.sin`/`Math.cos` without quadrant correction, causing inverted trajectories.

### Solution
Implement an encapsulated 2D vector kinematics module using `Math.hypot`, `Math.atan2`, and safe spatial clamping:

```javascript
class KinematicBody {
  constructor(x, y, vx = 0, vy = 0) {
    this.x = x;
    this.y = y;
    this.vx = vx;
    this.vy = vy;
    this.maxSpeed = 15;
  }

  accelerateTowards(targetX, targetY, force) {
    const dx = targetX - this.x;
    const dy = targetY - this.y;
    
    // Exact angle towards target preserving quadrant:
    const angle = Math.atan2(dy, dx);
    
    this.vx += Math.cos(angle) * force;
    this.vy += Math.sin(angle) * force;

    // Clamp speed to prevent tunneling:
    const currentSpeed = Math.hypot(this.vx, this.vy);
    if (currentSpeed > this.maxSpeed) {
      const scale = this.maxSpeed / currentSpeed;
      this.vx *= scale;
      this.vy *= scale;
    }
  }

  update(bounds) {
    this.x += this.vx;
    this.y += this.vy;

    // Safe spatial clamping inside boundary box:
    this.x = Math.min(Math.max(this.x, bounds.minX), bounds.maxX);
    this.y = Math.min(Math.max(this.y, bounds.minY), bounds.maxY);
  }
}

const bounds = { minX: 0, maxX: 800, minY: 0, maxY: 600 };
const drone = new KinematicBody(100, 100);

// Accelerate towards coordinate (500, 400) with force = 2.5
drone.accelerateTowards(500, 400, 2.5);
drone.update(bounds);

console.log(`Drone Pos: (${drone.x.toFixed(1)}, ${drone.y.toFixed(1)})`);
console.log(`Speed: ${Math.hypot(drone.vx, drone.vy).toFixed(2)} px/frame`);
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: Why does <code>Math.min()</code> return <code>Infinity</code> and <code>Math.max()</code> return <code>-Infinity</code>?</strong></summary>

In mathematical reductions, the identity element of an operation is the neutral starting value. For a minimum operation, any real number $x$ is less than $+\infty$ ($\min(x, \infty) = x$), making $+\infty$ the neutral accumulator. For a maximum operation, any real number $x$ is greater than $-\infty$ ($\max(x, -\infty) = x$).
</details>

<details>
<summary><strong>Q2: Why does <code>Math.round(-2.5)</code> evaluate to -2 instead of -3?</strong></summary>

According to the ECMAScript specification (ECMA-262), `Math.round(x)` adds $0.5$ and performs `Math.floor()`. For $-2.5$:
$$\text{floor}(-2.5 + 0.5) = \text{floor}(-2.0) = -2$$
Ties are always rounded towards $+\infty$, which makes negative half-values round towards zero rather than away from zero.
</details>

<details>
<summary><strong>Q3: What advantage does <code>Math.hypot(x, y)</code> offer over <code>Math.sqrt(x*x + y*y)</code>?</strong></summary>

`Math.hypot` protects against intermediate overflow and underflow. If $x = 10^{200}$, computing $x \times x$ produces $10^{400}$, which exceeds IEEE-754 max float ($1.79 \times 10^{308}$) and overflows to `Infinity`. `Math.hypot` scales the coordinates by their maximum component before squaring, allowing exact calculations on astronomically large or small numbers.
</details>

<details>
<summary><strong>Q4: Why is <code>Math.atan2(y, x)</code> preferred over <code>Math.atan(y / x)</code> in geometry?</strong></summary>

`Math.atan(y / x)` loses the sign information of the individual $x$ and $y$ components (e.g., $-1 / -1$ evaluates to $1$, identical to $1 / 1$), making it impossible to determine the correct quadrant of the angle. Furthermore, if $x = 0$, $y / x$ causes division by zero. `Math.atan2(y, x)` accepts both coordinates separately, safely handles $x = 0$, and returns the angle in the full range $[-\pi, \pi]$ across all four quadrants.
</details>

<details>
<summary><strong>Q5: Can <code>Math.random()</code> be safely used to generate session IDs or UUIDs? Why or why not?</strong></summary>

No. `Math.random()` uses the `xorshift128+` pseudo-random number generator, which is deterministic and not cryptographically secure. An attacker who observes several consecutive generated values can reconstruct the 128-bit internal engine state and predict all past and future values. Cryptographic generation requires `crypto.getRandomValues()` or `crypto.randomUUID()`.
</details>

<details>
<summary><strong>Q6: What is the purpose of <code>Math.clz32(x)</code>?</strong></summary>

`Math.clz32(x)` counts the number of leading zero bits in the 32-bit binary representation of $x$. It maps directly to native CPU instructions (e.g., `BSR` / `LZCNT` on x86) and is used for fast bitwise algorithms, integer logarithms (`31 - Math.clz32(x)` equals $\lfloor\log_2(x)\rfloor$), and binary compression.
</details>

<details>
<summary><strong>Q7: How does <code>Math.trunc()</code> differ from <code>Math.floor()</code>?</strong></summary>

For positive numbers, both behave identically. However, for negative numbers, `Math.floor(-2.8)` rounds down towards $-\infty$ (yielding `-3`), whereas `Math.trunc(-2.8)` discards the decimal fraction towards $0$ (yielding `-2`).
</details>

<details>
<summary><strong>Q8: Why does TurboFan compile <code>Math.abs()</code> to a single assembly instruction?</strong></summary>

In IEEE-754 floating-point representation, the absolute value of a number is simply the number with its sign bit cleared to `0`. On x86_64, TurboFan emits a bitwise AND instruction (`ANDPD`) with a mask clearing bit 63, executing in a single CPU cycle without branching.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Symmetric Round-Half-Away-From-Zero
Write a function `roundSymmetric(x)` that rounds ties ($0.5$) away from zero symmetrically for both positive and negative numbers (e.g., `2.5 -> 3`, `-2.5 -> -3`).

<details>
<summary><strong>Solution</strong></summary>

```javascript
function roundSymmetric(x) {
  const sign = Math.sign(x) || 1;
  return sign * Math.floor(Math.abs(x) + 0.5);
}

console.log(roundSymmetric(2.5));  // 3
console.log(roundSymmetric(-2.5)); // -3
console.log(roundSymmetric(2.4));  // 2
console.log(roundSymmetric(-2.4)); // -2
```
</details>

### Exercise 2: Safe Maximum Across Huge Arrays
Implement a function `safeMax(array)` that returns the maximum number in an array of any length (even millions of elements) without throwing a call stack size exceeded error.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function safeMax(array) {
  if (array.length === 0) return -Infinity;
  let max = array[0];
  for (let i = 1; i < array.length; i++) {
    const val = array[i];
    if (val > max) max = val;
  }
  return max;
}

const huge = new Array(1_000_000).fill(10);
huge[500_000] = 999;
console.log(safeMax(huge)); // 999 (Zero stack overflow!)
```
</details>

### Exercise 3: Circular Distance & Shortest Angle Difference
Implement `angleDifference(angleA, angleB)` that calculates the minimum angular distance in radians between two angles, correctly wrapping across the $\pi / -\pi$ boundary.

<details>
<summary><strong>Solution</strong></summary>

```javascript
function angleDifference(target, current) {
  const diff = (target - current) % (2 * Math.PI);
  // Normalize into [-PI, PI]:
  return ((diff + 3 * Math.PI) % (2 * Math.PI)) - Math.PI;
}

const angleA = Math.PI * 0.9;  // 162 deg
const angleB = -Math.PI * 0.9; // -162 deg

// Direct difference would be 324 deg, but shortest path across wrap is -36 deg:
const shortestDiff = angleDifference(angleB, angleA);
console.log((shortestDiff * 180 / Math.PI).toFixed(1)); // "-36.0" degrees
```
</details>
