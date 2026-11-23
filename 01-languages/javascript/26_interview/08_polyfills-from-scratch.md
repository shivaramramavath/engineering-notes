# JavaScript Interview Mastery: Standard Library Polyfills From Scratch

> Complete specification-compliant polyfill implementations for ECMAScript built-in methods: `Function.prototype.bind/call/apply`, `Array.prototype.map/filter/reduce/flat`, and `Promise.all/allSettled/race/any`.

| Metadata | Details |
| :--- | :--- |
| **Track** | JavaScript Interview Preparation (`26_interview`) |
| **Target Role** | Senior / Staff Frontend & Full-Stack Engineer |
| **Baseline** | ECMAScript 2024 / Node.js 22 LTS |
| **Prerequisites** | [02_functions](file:///e:/Learning/engineering-notes/01-languages/javascript/02_functions), [11_asynchronous-javascript](file:///e:/Learning/engineering-notes/01-languages/javascript/11_asynchronous-javascript) |

---

## Table of Contents
1. [Polyfill Engineering Standards](#1-polyfill-engineering-standards)
2. [Function Prototype Polyfills](#2-function-prototype-polyfills)
   - [2.1 `Function.prototype.myCall`](#21-functionprototypemycall)
   - [2.2 `Function.prototype.myApply`](#22-functionprototypemyapply)
   - [2.3 `Function.prototype.myBind`](#23-functionprototypemybind)
3. [Array Prototype Polyfills](#3-array-prototype-polyfills)
   - [3.1 `Array.prototype.myMap`](#31-arrayprototypemymap)
   - [3.2 `Array.prototype.myFilter`](#32-arrayprototypemyfilter)
   - [3.3 `Array.prototype.myReduce`](#33-arrayprototypemyreduce)
   - [3.4 `Array.prototype.myFlat`](#34-arrayprototypemyflat)
4. [Promise Combinator Polyfills](#4-promise-combinator-polyfills)
   - [4.1 `Promise.myAll`](#41-promisemyall)
   - [4.2 `Promise.myAllSettled`](#42-promisemyallsettled)
   - [4.3 `Promise.myRace`](#43-promisemyrace)
   - [4.4 `Promise.myAny`](#44-promisemyany)
5. [Verification & Test Harness](#5-verification--test-harness)

---

## 1. Polyfill Engineering Standards

Implementing standard library polyfills in senior interviews requires strict adherence to ECMAScript specifications:
1. **Defensive Validation**: Validate that `this` is callable (`typeof this === 'function'`) and throw appropriate `TypeError` exceptions.
2. **Context Coercion**: Convert primitives passed to `call`/`apply` to Objects via `Object(context)`. Fall back to `globalThis` if `null` or `undefined`.
3. **Symbol Isolation**: When attaching temporary methods to objects, use unique `Symbol()` keys to prevent property collisions or prototype pollution.
4. **Sparse Array Handling**: Skip empty holes in array iterations.

---

## 2. Function Prototype Polyfills

### 2.1 `Function.prototype.myCall`

```javascript
Function.prototype.myCall = function (context, ...args) {
  if (typeof this !== 'function') {
    throw new TypeError(`${this} is not callable`);
  }

  // Handle null or undefined context by falling back to globalThis
  const ctx = context !== null && context !== undefined ? Object(context) : globalThis;
  const fnSymbol = Symbol('fnCall');

  ctx[fnSymbol] = this;
  try {
    return ctx[fnSymbol](...args);
  } finally {
    delete ctx[fnSymbol];
  }
};
```

### 2.2 `Function.prototype.myApply`

```javascript
Function.prototype.myApply = function (context, argsArray) {
  if (typeof this !== 'function') {
    throw new TypeError(`${this} is not callable`);
  }

  const ctx = context !== null && context !== undefined ? Object(context) : globalThis;
  const fnSymbol = Symbol('fnApply');
  const args = Array.isArray(argsArray) ? argsArray : [];

  ctx[fnSymbol] = this;
  try {
    return ctx[fnSymbol](...args);
  } finally {
    delete ctx[fnSymbol];
  }
};
```

### 2.3 `Function.prototype.myBind`

```javascript
Function.prototype.myBind = function (context, ...boundArgs) {
  if (typeof this !== 'function') {
    throw new TypeError('Bind must be called on a function');
  }

  const targetFn = this;

  function boundFunction(...callArgs) {
    // If invoked with 'new', 'this' points to the newly allocated instance
    const isNew = this instanceof boundFunction;
    const effectiveContext = isNew ? this : context;
    return targetFn.apply(effectiveContext, boundArgs.concat(callArgs));
  }

  // Maintain prototype chain linkage for new invocations
  if (targetFn.prototype) {
    boundFunction.prototype = Object.create(targetFn.prototype);
  }

  return boundFunction;
};
```

---

## 3. Array Prototype Polyfills

### 3.1 `Array.prototype.myMap`

```javascript
Array.prototype.myMap = function (callback, thisArg) {
  if (this == null) throw new TypeError('Cannot read property of null or undefined');
  if (typeof callback !== 'function') throw new TypeError(`${callback} is not a function`);

  const list = Object(this);
  const len = list.length >>> 0; // ToUint32
  const result = new Array(len);

  for (let i = 0; i < len; i++) {
    // Only process existing own properties to respect sparse array holes
    if (i in list) {
      result[i] = callback.call(thisArg, list[i], i, list);
    }
  }

  return result;
};
```

### 3.2 `Array.prototype.myFilter`

```javascript
Array.prototype.myFilter = function (callback, thisArg) {
  if (this == null) throw new TypeError('Cannot read property of null or undefined');
  if (typeof callback !== 'function') throw new TypeError(`${callback} is not a function`);

  const list = Object(this);
  const len = list.length >>> 0;
  const result = [];

  for (let i = 0; i < len; i++) {
    if (i in list) {
      const element = list[i];
      if (callback.call(thisArg, element, i, list)) {
        result.push(element);
      }
    }
  }

  return result;
};
```

### 3.3 `Array.prototype.myReduce`

```javascript
Array.prototype.myReduce = function (callback, initialValue) {
  if (this == null) throw new TypeError('Cannot read property of null or undefined');
  if (typeof callback !== 'function') throw new TypeError(`${callback} is not a function`);

  const list = Object(this);
  const len = list.length >>> 0;
  let accumulator;
  let startIndex = 0;

  if (arguments.length >= 2) {
    accumulator = initialValue;
  } else {
    // Find first non-empty element
    let foundFirst = false;
    while (startIndex < len) {
      if (startIndex in list) {
        accumulator = list[startIndex];
        startIndex++;
        foundFirst = true;
        break;
      }
      startIndex++;
    }

    if (!foundFirst) {
      throw new TypeError('Reduce of empty array with no initial value');
    }
  }

  for (let i = startIndex; i < len; i++) {
    if (i in list) {
      accumulator = callback(accumulator, list[i], i, list);
    }
  }

  return accumulator;
};
```

### 3.4 `Array.prototype.myFlat`

```javascript
Array.prototype.myFlat = function (depth = 1) {
  if (this == null) throw new TypeError('Cannot read property of null or undefined');

  function flatten(arr, d) {
    if (d <= 0) return arr.slice();

    const result = [];
    for (let i = 0; i < arr.length; i++) {
      if (i in arr) {
        const val = arr[i];
        if (Array.isArray(val)) {
          result.push(...flatten(val, d - 1));
        } else {
          result.push(val);
        }
      }
    }
    return result;
  }

  return flatten(this, depth);
};
```

---

## 4. Promise Combinator Polyfills

### 4.1 `Promise.myAll`

```javascript
Promise.myAll = function (iterable) {
  return new Promise((resolve, reject) => {
    if (iterable == null || typeof iterable[Symbol.iterator] !== 'function') {
      return reject(new TypeError('Argument must be iterable'));
    }

    const items = Array.from(iterable);
    if (items.length === 0) return resolve([]);

    const results = new Array(items.length);
    let resolvedCount = 0;

    items.forEach((item, index) => {
      Promise.resolve(item).then(
        (val) => {
          results[index] = val;
          resolvedCount++;
          if (resolvedCount === items.length) {
            resolve(results);
          }
        },
        (err) => {
          reject(err); // Short-circuit on first rejection
        }
      );
    });
  });
};
```

### 4.2 `Promise.myAllSettled`

```javascript
Promise.myAllSettled = function (iterable) {
  return new Promise((resolve, reject) => {
    if (iterable == null || typeof iterable[Symbol.iterator] !== 'function') {
      return reject(new TypeError('Argument must be iterable'));
    }

    const items = Array.from(iterable);
    if (items.length === 0) return resolve([]);

    const results = new Array(items.length);
    let settledCount = 0;

    items.forEach((item, index) => {
      Promise.resolve(item).then(
        (value) => {
          results[index] = { status: 'fulfilled', value };
          settledCount++;
          if (settledCount === items.length) resolve(results);
        },
        (reason) => {
          results[index] = { status: 'rejected', reason };
          settledCount++;
          if (settledCount === items.length) resolve(results);
        }
      );
    });
  });
};
```

### 4.3 `Promise.myRace`

```javascript
Promise.myRace = function (iterable) {
  return new Promise((resolve, reject) => {
    if (iterable == null || typeof iterable[Symbol.iterator] !== 'function') {
      return reject(new TypeError('Argument must be iterable'));
    }

    for (const item of iterable) {
      Promise.resolve(item).then(resolve, reject);
    }
  });
};
```

### 4.4 `Promise.myAny`

```javascript
Promise.myAny = function (iterable) {
  return new Promise((resolve, reject) => {
    if (iterable == null || typeof iterable[Symbol.iterator] !== 'function') {
      return reject(new TypeError('Argument must be iterable'));
    }

    const items = Array.from(iterable);
    if (items.length === 0) {
      return reject(new AggregateError([], 'All promises were rejected'));
    }

    const errors = new Array(items.length);
    let rejectedCount = 0;

    items.forEach((item, index) => {
      Promise.resolve(item).then(
        (val) => {
          resolve(val); // Short-circuit on first fulfillment
        },
        (err) => {
          errors[index] = err;
          rejectedCount++;
          if (rejectedCount === items.length) {
            reject(new AggregateError(errors, 'All promises were rejected'));
          }
        }
      );
    });
  });
};
```

---

## 5. Verification & Test Harness

```bash
$ node --test -e "
import assert from 'node:assert/strict';
import './01-languages/javascript/26_interview/08_polyfills-from-scratch.md';

// Verify Function.myBind
function greet(prefix, suffix) { return prefix + ' ' + this.name + ' ' + suffix; }
const bound = greet.myBind({ name: 'Sam' }, 'Hello');
assert.equal(bound('!'), 'Hello Sam !');

// Verify Array.myReduce
assert.equal([1, 2, 3, 4].myReduce((a, b) => a + b, 0), 10);
console.log('All polyfill assertions passed cleanly!');
"
```
