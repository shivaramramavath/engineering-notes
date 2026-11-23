# The Strategy Pattern in JavaScript

> Encapsulating interchangeable algorithms, eliminating branching anti-patterns, and leveraging first-class functions as dynamic strategies.

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [Functions](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions/01_declarations-and-expressions.md), [Classes](file:///e:/Learning/engineering-notes/01-languages/javascript/05_this-and-oop/05_classes.md) |
| **Time to read** | 22 minutes |
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

In software systems, a core operation often requires different algorithmic implementations depending on user context, data format, or runtime environment. Examples include calculating shipping costs, applying discounts, authenticating users, compressing payloads, or sorting datasets.

Without a structured pattern, code inevitably degenerates into monolithic functions dominated by brittle, deeply nested conditional statements (`if (type === 'A') ... else if (type === 'B') ...`). Adding a new algorithm requires modifying existing code, violating the **Open-Closed Principle (OCP)** and introducing high regression risks.

The **Strategy Pattern** defines a family of algorithms, encapsulates each one inside a separate structure, and makes them interchangeable at runtime. In classical languages like C++ or Java, this requires defining abstract interfaces and concrete strategy classes. In JavaScript, where functions are **first-class citizens**, strategies can be implemented as simple standalone functions or objects, making the pattern extraordinarily expressive and concise.

---

## 2. Core Concepts

### Core Components
1. **Context**: The class or function that maintains a reference to a Strategy object and delegates the algorithmic work to it.
2. **Strategy Interface**: The shared contract (method signature) that all concrete strategies must fulfill.
3. **Concrete Strategies**: Independent implementations of the algorithm that adhere to the strategy contract.

### Functional Strategies vs Class-Based Strategies
- **Class-Based Strategies**: Useful when strategies maintain internal state, caching, or require complex lifecycle methods (e.g., Passport.js authentication strategies).
- **Functional Strategies**: Idiomatic in JavaScript. A strategy is simply a pure function matching a signature `(data) => result`. The Context simply calls the function passed to it.

---

## 3. How It Works Internally

```
+--------------------------------------------------------------------------+
| CONTEXT (CheckoutService)                                                |
| - Holds current strategy reference: this.shippingStrategy                |
| - calculateTotal(order) => this.shippingStrategy.calculate(order)        |
+------------------------------------+-------------------------------------+
                                     | Delegates Execution
                                     v
+--------------------------------------------------------------------------+
| STRATEGY INTERFACE: calculate(order) => number                           |
+------------------------------------+-------------------------------------+
                                     |
              +----------------------+----------------------+
              |                      |                      |
              v                      v                      v
+----------------------+ +----------------------+ +----------------------+
| StandardShipping     | | ExpressShipping      | | OvernightShipping    |
| - 5-7 business days  | | - 2 business days    | | - Next morning air   |
| - $5.00 flat         | | - $15.00 flat        | | - Weight * $2.50     |
+----------------------+ +----------------------+ +----------------------+
```

---

## 4. Syntax & API Reference

### Functional Strategy Map (Idiomatic JavaScript)
```javascript
// Strategies defined as pure functions
const strategies = {
  fixedDiscount: (amount) => amount - 10,
  percentageDiscount: (amount) => amount * 0.8,
  noDiscount: (amount) => amount,
};

// Context function selecting strategy dynamically
function applyDiscount(strategyName, totalAmount) {
  const strategy = strategies[strategyName] || strategies.noDiscount;
  return strategy(totalAmount);
}
```

### Class-Based Strategy (Object-Oriented)
```javascript
class PaymentContext {
  #strategy;

  constructor(strategy) {
    this.setStrategy(strategy);
  }

  setStrategy(strategy) {
    if (!strategy || typeof strategy.pay !== 'function') {
      throw new TypeError('Strategy must implement pay(amount)');
    }
    this.#strategy = strategy;
  }

  executePayment(amount) {
    return this.#strategy.pay(amount);
  }
}
```

---

## 5. Practical Examples

### Example 1: E-Commerce Shipping Cost Strategies
```javascript
// Concrete Strategy 1: Standard Ground
export const standardShipping = {
  name: 'Standard',
  calculate: (order) => (order.weight * 0.5) + 5.0,
};

// Concrete Strategy 2: Expedited Air
export const expeditedShipping = {
  name: 'Expedited',
  calculate: (order) => (order.weight * 1.2) + 15.0,
};

// Concrete Strategy 3: Free Shipping (Promotional)
export const freeShipping = {
  name: 'Free',
  calculate: (order) => (order.totalPrice > 100 ? 0 : standardShipping.calculate(order)),
};

// Context
export class ShippingCalculator {
  #strategy;

  constructor(strategy = standardShipping) {
    this.#strategy = strategy;
  }

  setStrategy(strategy) {
    this.#strategy = strategy;
  }

  calculateShipping(order) {
    return this.#strategy.calculate(order);
  }
}
```

### Example 2: Data Serialization / Export Strategies
```javascript
export const exportStrategies = {
  json: (data) => JSON.stringify(data, null, 2),

  csv: (data) => {
    if (!data.length) return '';
    const headers = Object.keys(data[0]);
    const rows = data.map(row => headers.map(h => JSON.stringify(row[h] ?? '')).join(','));
    return [headers.join(','), ...rows].join('\n');
  },

  ndjson: (data) => data.map(item => JSON.stringify(item)).join('\n'),
};

export function exportDataset(dataset, format = 'json') {
  const strategy = exportStrategies[format.toLowerCase()];
  if (!strategy) {
    throw new Error(`Unsupported export format: ${format}`);
  }
  return strategy(dataset);
}
```

### Example 3: Form Field Validator Strategies
```javascript
export const validationStrategies = {
  required: (val) => (val !== null && val !== undefined && val !== '' ? null : 'Field is required'),
  email: (val) => (/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(val) ? null : 'Invalid email address'),
  numeric: (val) => (!isNaN(Number(val)) ? null : 'Must be a numeric value'),
  minLength: (min) => (val) => (String(val).length >= min ? null : `Minimum length is ${min}`),
};

export class FormValidator {
  constructor(schema) {
    this.schema = schema; // { [field: string]: Array<StrategyFunction> }
  }

  validate(formData) {
    const errors = {};

    for (const [field, strategies] of Object.entries(this.schema)) {
      const value = formData[field];
      for (const strategy of strategies) {
        const error = strategy(value);
        if (error) {
          errors[field] = error;
          break; // Stop at first failing validation rule for this field
        }
      }
    }

    return {
      isValid: Object.keys(errors).length === 0,
      errors,
    };
  }
}
```

### Example 4: Compression Strategy with Dynamic Selection
```javascript
import zlib from 'node:zlib';
import { promisify } from 'node:util';

const gzip = promisify(zlib.gzip);
const brotli = promisify(zlib.brotliCompress);

export const compressionStrategies = {
  br: async (buffer) => ({
    encoding: 'br',
    data: await brotli(buffer),
  }),
  gzip: async (buffer) => ({
    encoding: 'gzip',
    data: await gzip(buffer),
  }),
  identity: async (buffer) => ({
    encoding: 'identity',
    data: buffer,
  }),
};

export async function compressResponse(acceptEncodingHeader, payloadBuffer) {
  let strategy = compressionStrategies.identity;

  if (acceptEncodingHeader.includes('br')) {
    strategy = compressionStrategies.br;
  } else if (acceptEncodingHeader.includes('gzip')) {
    strategy = compressionStrategies.gzip;
  }

  return strategy(payloadBuffer);
}
```

### Example 5: Passport-Style Authentication Strategies
```javascript
export class AuthContext {
  #strategies = new Map();

  use(name, strategy) {
    this.#strategies.set(name, strategy);
  }

  async authenticate(name, req) {
    const strategy = this.#strategies.get(name);
    if (!strategy) {
      throw new Error(`Authentication strategy "${name}" not registered`);
    }

    return new Promise((resolve, reject) => {
      strategy.authenticate(req, {
        success: (user) => resolve({ status: 'success', user }),
        fail: (info) => resolve({ status: 'fail', info }),
        error: (err) => reject(err),
      });
    });
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Strategy State Leakage**: If a concrete strategy stores state across invocations (e.g., maintaining an internal counter or accumulator), concurrent requests can corrupt that state. Ensure strategies are either stateless pure functions or instantiated fresh per execution context.
2. **Missing Strategies**: Accessing a strategy dynamically using user input (`strategies[format]`) will return `undefined` if the input is unexpected, throwing `TypeError: strategy is not a function`. Always provide a default fallback strategy or throw a clean error.
3. **Inconsistent Parameter Signatures**: If Strategy A expects `(order, user)` and Strategy B expects `(order)`, the Context must strictly define the contract and pass identical parameters to all strategies.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern | Why It Breaks | Correct Pattern |
| :--- | :--- | :--- |
| Giant 200-line `switch-case` in business logic | Violates Open-Closed Principle; hard to test and extend | Extract cases into a Strategy Map |
| Storing mutable request state on shared strategy objects | Cross-request data leakage in concurrent servers | Keep strategies purely stateless |
| Over-complicating functional code with abstract classes | Unnecessary boilerplate in JavaScript | Use pure functions as strategies |
| Strategies with incompatible argument signatures | Throws errors or receives `undefined` when swapped | Enforce a strict shared parameter contract |
| Modifying input arguments inside strategies | Causes unpredictable side effects for subsequent strategies | Return new transformed values immutably |

---

## 8. Performance Characteristics

- **Functional Strategy Lookup**: Looking up a strategy in an object literal or `Map` is an $O(1)$ constant-time hash lookup, outperforming long sequential `else-if` chains.
- **TurboFan Optimization**: When a Context repeatedly executes the same functional strategy, TurboFan inlines the strategy directly into the call site.

---

## 9. Engine Internals

1. **Polymorphic Call Sites in Strategies**:
   - If a Context alternates frequently between 5 different strategy functions, the call site transitions to **Megamorphic**.
   - While still fast, V8 cannot inline the strategy. In performance-critical loops running millions of iterations, try to process data in batches using the same strategy before switching.

---

## 10. Best Practices & Design Guidelines

- **Favor Pure Functions for Stateless Algorithms**: In JavaScript, a strategy does not need to be a class. A simple `(input) => output` function is cleaner, easier to test, and composable.
- **Use TypeScript or Duck-Type Validation**: Ensure strategies implement required methods before executing them in the Context.
- **Default Strategies**: Always provide a sensible default strategy (such as a no-op or pass-through strategy) to prevent runtime exceptions.

---

## 11. Real-World Case Study

### High-Volume Routing Cost Optimizer (Logistics Platform)
```javascript
// Dynamic routing optimization strategy selector based on fleet constraints
export const routingStrategies = {
  // Strategy 1: Prioritize lowest monetary toll cost
  cheapest: (routeCandidates) => {
    return routeCandidates.reduce((min, r) => (r.tollCost < min.tollCost ? r : min));
  },

  // Strategy 2: Prioritize lowest carbon emissions (EV fleet)
  eco: (routeCandidates) => {
    return routeCandidates.reduce((min, r) => (r.carbonKg < min.carbonKg ? r : min));
  },

  // Strategy 3: Prioritize fastest arrival time
  fastest: (routeCandidates) => {
    return routeCandidates.reduce((min, r) => (r.durationMinutes < min.durationMinutes ? r : min));
  },
};

export class DeliveryDispatcher {
  constructor(strategy = routingStrategies.fastest) {
    this.strategy = strategy;
  }

  setOptimizationGoal(goalName) {
    const selected = routingStrategies[goalName.toLowerCase()];
    if (!selected) throw new Error(`Unknown optimization goal: ${goalName}`);
    this.strategy = selected;
  }

  dispatch(packageItem, routes) {
    const optimalRoute = this.strategy(routes);
    console.log(`Dispatched package ${packageItem.id} via route: ${optimalRoute.id}`);
    return optimalRoute;
  }
}
```

---

## 12. Interview Questions

<details>
<summary>1. What is the Strategy Pattern and what problem does it solve?</summary>
The Strategy Pattern defines a family of interchangeable algorithms, encapsulates each one, and allows client code to switch between them at runtime. It eliminates large, brittle conditional structures (<code>if-else</code> or <code>switch-case</code>) and adheres to the Open-Closed Principle by allowing new algorithms to be added without modifying existing code.
</details>

<details>
<summary>2. Why is the Strategy Pattern particularly elegant in JavaScript compared to Java or C++?</summary>
In Java or C++, implementing the Strategy pattern requires defining an abstract interface and creating multiple concrete classes. In JavaScript, because functions are first-class citizens, a strategy can simply be a plain function. A strategy map can be represented as a simple object literal or <code>Map</code> where keys are strategy names and values are the functions themselves.
</details>

<details>
<summary>3. What is the difference between the Strategy Pattern and the State Pattern?</summary>
While both patterns share similar class diagrams (delegating execution to a composition object), their intent is different:
- <b>Strategy</b>: Deals with <i>how</i> an operation is performed (interchangeable algorithms selected externally by client code).
- <b>State</b>: Deals with <i>what</i> an object does based on its internal state, where state transitions are typically handled automatically by the state objects themselves.
</details>

<details>
<summary>4. How does the Strategy Pattern enforce the Open-Closed Principle (SOLID)?</summary>
The Context is closed for modification because its core logic does not change when new algorithms are introduced. It is open for extension because developers can register or pass new strategy implementations without touching the Context's source code.
</details>

<details>
<summary>5. How can you prevent Megamorphic deoptimization when using multiple strategies?</summary>
If a single function call site dynamically switches between more than 4 different strategy functions, V8's Inline Cache becomes Megamorphic and cannot inline the call. You can mitigate this by batching work: sort operations by strategy type so the function executes the same strategy thousands of times in a row before switching.
</details>

<details>
<summary>6. When should you use a class-based strategy instead of a functional strategy?</summary>
Use a class-based strategy when the strategy requires complex internal state, persistent caching, configuration properties, or multi-step lifecycle hooks (e.g., Passport.js strategies with initialization, verification, and teardown).
</details>

<details>
<summary>7. What happens if a client passes an invalid or missing strategy to a Context?</summary>
If not guarded, calling <code>this.strategy.execute()</code> throws <code>TypeError: this.strategy is undefined</code> or <code>not a function</code>. The Context should validate the strategy in its setter or provide a safe default fallback strategy.
</details>

<details>
<summary>8. How does Array.prototype.sort() use the Strategy Pattern?</summary>
<code>Array.prototype.sort()</code> accepts a custom comparator function <code>(a, b) => number</code>. The sort method acts as the Context (implementing the Timsort algorithm), while the comparator function passed by the developer is the Strategy determining element ordering.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Multi-Currency Price Formatter Strategy
Implement a pricing formatter `formatPrice(amount, currencyCode)` using a strategy map supporting `'USD'`, `'EUR'`, `'GBP'`, and `'JPY'` with localized currency symbols, decimal precision, and formatting rules.

<details>
<summary>View Solution</summary>

```javascript
const currencyStrategies = {
  USD: (val) => `$${val.toFixed(2)}`,
  EUR: (val) => `${val.toFixed(2).replace('.', ',')} €`,
  GBP: (val) => `£${val.toFixed(2)}`,
  JPY: (val) => `¥${Math.round(val).toLocaleString()}`,
};

export function formatPrice(amount, currencyCode = 'USD') {
  const strategy = currencyStrategies[currencyCode.toUpperCase()];
  if (!strategy) {
    throw new Error(`Unsupported currency code: ${currencyCode}`);
  }
  return strategy(amount);
}
```
</details>

### Exercise 2: Text Formatting and Sanitization Pipeline Strategy
Create a string sanitizer utility `SanitizerContext` that allows configuring and chaining multiple formatting strategies (e.g., `trim`, `stripHtml`, `toLowerCase`, `truncate(max)`) to process raw input strings.

<details>
<summary>View Solution</summary>

```javascript
export const stringStrategies = {
  trim: (str) => str.trim(),
  stripHtml: (str) => str.replace(/<[^>]*>?/gm, ''),
  toLowerCase: (str) => str.toLowerCase(),
  truncate: (max) => (str) => str.length > max ? `${str.slice(0, max)}...` : str,
};

export class SanitizerContext {
  constructor(strategies = []) {
    this.strategies = strategies;
  }

  addStrategy(strategy) {
    this.strategies.push(strategy);
    return this;
  }

  sanitize(input) {
    return this.strategies.reduce((result, strategy) => strategy(result), String(input));
  }
}
```
</details>
