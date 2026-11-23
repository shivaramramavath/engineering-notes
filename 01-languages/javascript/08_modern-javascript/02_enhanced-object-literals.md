# JavaScript Enhanced Object Literals: Shorthands, Computed Keys & [[HomeObject]]

> Comprehensive guide to ECMAScript enhanced object literal syntax, property value shorthands, method shorthands, `[[HomeObject]]` super delegation, and computed property keys.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [03_objects-and-arrays/01_objects.md](../03_objects-and-arrays/01_objects.md), [05_this-and-oop/03_prototypes-and-prototype-chain.md](../05_this-and-oop/03_prototypes-and-prototype-chain.md) |
| **Time to Read** | 20 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2015 Enhanced Object Literals |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: The 4 Key Enhancements](#2-core-concepts-the-4-key-enhancements)
3. [How It Works: Method Shorthand & [[HomeObject]] Binding](#3-how-it-works-method-shorthand--homeobject-binding)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 Literal Boilerplates & Shape Allocation](#9-engine-internals-v8-literal-boilerplates--shape-allocation)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Dynamic Redux-Style Action & Reducer Map](#11-real-world-case-study-dynamic-redux-style-action--reducer-map)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Before ECMAScript 2015 (ES6), object literal syntax was repetitive and rigid:
```javascript
// Pre-ES6 verbose syntax:
var x = 10, y = 20;
var obj = {
  x: x,
  y: y,
  add: function () { return this.x + this.y; }
};
// Computed keys had to be assigned post-instantiation:
obj["dynamic_" + key] = true;
```

ES2015 modernized object literals with **Enhanced Object Literals**, introducing:
1. **Property Value Shorthand** (`{ x, y }`)
2. **Method Shorthand** (`{ add() {} }`)
3. **Computed Property Keys** (`{ [dynamicKey]: value }`)
4. **Direct Prototype Assignment in Literals** (`{ __proto__: customProto }`)

These features eliminate boilerplate, enable dynamic key computation directly inside literal definitions, and grant methods access to the `super` keyword via internal `[[HomeObject]]` binding.

---

## 2. Core Concepts: The 4 Key Enhancements

```
                         Enhanced Object Literals
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
  Property Shorthand         Method Shorthand           Computed Keys
  { name, age }              { greet() {} }             { [prefix + "_id"]: 42 }
  Matches local variable     Omits ': function'         Evaluates runtime expr
  name to key & value        Enables `super` keyword    as string or Symbol key
```

---

## 3. How It Works: Method Shorthand & [[HomeObject]] Binding

The difference between `{ fn: function() {} }` and `{ fn() {} }` is not merely cosmetic; it fundamentally alters the function's internal specification slots:

```
Method Definition Comparison:
1. Classic:  { fn: function() {} }
   - Has a `prototype` property.
   - CAN be invoked with `new obj.fn()`.
   - Has NO `[[HomeObject]]` slot (CANNOT use `super`).

2. Shorthand: { fn() {} }
   - Does NOT have a `prototype` property!
   - CANNOT be invoked with `new obj.fn()` (Throws TypeError!).
   - Has an internal `[[HomeObject]]` slot pointing to the object literal.
   - CAN use `super.parentMethod()`!
```

---

## 4. Syntax & API Reference

### Grammar Forms
```javascript
const id = 101;
const dynamicKey = "createdAt";

const entity = {
  // 1. Property Shorthand
  id,

  // 2. Computed Property Key
  [dynamicKey]: new Date().toISOString(),

  // 3. Method Shorthand
  describe() {
    return `Entity ID: ${this.id}`;
  },

  // 4. Computed Method Name
  [`action_${id}`]() {
    return "Action executed";
  },

  // 5. Accessor Shorthand
  get active() { return true; },
  set active(v) { /* ... */ },

  // 6. Direct Prototype Assignment
  __proto__: customParentProto
};
```

---

## 5. Practical Examples

### Example 1: Basic Shorthands and Computed Keys
```javascript
const firstName = "Grace";
const lastName = "Hopper";
const roleKey = "role_permission";

const user = {
  firstName,
  lastName,
  [roleKey]: "SUPERADMIN",
  getFullName() {
    return `${this.firstName} ${this.lastName}`;
  }
};

console.log(user.getFullName()); // "Grace Hopper"
console.log(user.role_permission); // "SUPERADMIN"
```

### Example 2: Real-World Super Delegation in Object Literals
```javascript
const baseLogger = {
  log(message) {
    return `[BASE] ${message}`;
  }
};

// Shorthand methods can delegate to prototypes using 'super'!
const specializedLogger = {
  __proto__: baseLogger,
  log(message) {
    // 'super' resolves statically via [[HomeObject]]!
    return `${super.log(message)} -> [SPECIALIZED]`;
  }
};

console.log(specializedLogger.log("Process started"));
// Output: "[BASE] Process started -> [SPECIALIZED]"
```

### Example 3: Edge Case with Shorthand Method Constructor Invocation
```javascript
const service = {
  classicMethod: function () {},
  shorthandMethod() {}
};

// Classic method has prototype and is constructible:
console.log(typeof service.classicMethod.prototype); // "object"
new service.classicMethod(); // Safe!

// Shorthand method has NO prototype and is NOT constructible:
console.log(service.shorthandMethod.prototype); // undefined
try {
  // Throws TypeError: service.shorthandMethod is not a constructor
  new service.shorthandMethod();
} catch (err) {
  console.log("Shorthand constructor check:", err.message);
}
```

### Example 4: Failure Mode with Duplicate `__proto__` Keys
```javascript
// Failure mode: Multiple __proto__ in object literal throws SyntaxError!
try {
  // eval('({ __proto__: {}, __proto__: [] })');
  // SyntaxError: Duplicate __proto__ fields are not allowed in object literals
} catch (e) {}

// Notice: Computed __proto__ is treated as a regular property, NOT prototype assignment!
const computedProto = {
  ["__proto__"]: { custom: true }
};
console.log(Object.getPrototypeOf(computedProto) === Object.prototype); // true!
console.log(computedProto.custom); // undefined (it's an own property named "__proto__")
```

### Example 5: Idiomatic Dynamic State Dispatcher Map
```javascript
function createActionHandlers(eventPrefix) {
  return {
    [`${eventPrefix}_START`](state) {
      return { ...state, status: "loading" };
    },
    [`${eventPrefix}_SUCCESS`](state, payload) {
      return { ...state, status: "success", data: payload };
    },
    [`${eventPrefix}_ERROR`](state, error) {
      return { ...state, status: "error", error };
    }
  };
}

const userHandlers = createActionHandlers("USER_FETCH");
console.log(Object.keys(userHandlers));
// Output: [ 'USER_FETCH_START', 'USER_FETCH_SUCCESS', 'USER_FETCH_ERROR' ]
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Computed Keys Coerce to String**: `{ [{}]: "test" }` evaluates the object expression to string, resulting in the literal key `"[object Object]"`. Only strings and Symbols form distinct keys.
2. **`__proto__` vs Computed `["__proto__"]`**: Writing `__proto__: p` sets the object's prototype. Writing `["__proto__"]: p` creates a normal own string property named `"__proto__"` without modifying the prototype chain.
3. **Execution Order of Computed Keys**: Computed keys are evaluated **sequentially in source code order** during object literal construction. Side effects inside computed brackets execute before subsequent keys are evaluated.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Redundant property assignment | `{ x: x, y: y }` | `{ x, y }` | Unnecessary boilerplate in modern ES6+. |
| Calling `new` on shorthand method | `new obj.method()` | Use constructor or class | Shorthand methods lack `prototype` and throw TypeError. |
| Using object as computed key | `{ [objKey]: val }` | Use a `Map` for object keys | Objects coerce to `"[object Object]"` strings. |
| Computed `__proto__` for prototype | `{ ["__proto__"]: proto }` | `{ __proto__: proto }` or `Object.create` | Computed `["__proto__"]` creates a regular property, not prototype. |
| Multiple `__proto__` declarations | `{ __proto__: a, __proto__: b }` | Single prototype link | Throws compile-time `SyntaxError`. |
| Missing brackets for computed keys | `{ dynamicVar: 42 }` | `{ [dynamicVar]: 42 }` | Creates literal property `"dynamicVar"`, not value of variable. |

---

## 8. Performance Characteristics

- **Hidden Class Creation**: Object literals with static keys enable V8 to pre-allocate an optimized `Map` (Hidden Class) at compile time.
- **Computed Keys Overhead**: Using computed property keys prevents V8 from creating a compile-time shape; the engine must dynamically compute keys at runtime and transition shapes step by step.

---

## 9. Engine Internals: V8 Literal Boilerplates & Shape Allocation

Inside Google V8:
1. When V8 encounters an object literal with static keys (`{ a: 1, b: 2 }`), Ignition creates an `ObjectBoilerplate`.
2. V8 pre-computes the final Hidden Class (`Map`) and pre-allocates slots.
3. At runtime, Ignition clones the boilerplate in a single C++ memory operation (`CloneObjectLiteral`), which is substantially faster than adding properties one-by-one.
4. When computed keys are present, V8 falls back to creating an empty object and executing runtime dynamic assignments.

---

## 10. Best Practices & Design Guidelines

1. **Use Property Shorthand Universally**: Always write `{ a, b }` when local variable names match target property keys.
2. **Use Method Shorthand for All Object Methods**: Method shorthand `{ method() {} }` is cleaner, lighter (no useless `.prototype` property), and supports `super`.
3. **Avoid Runtime Prototype Modification via `__proto__`**: Prefer `Object.create(proto)` over literal `__proto__` for better engine shape predictability.

---

## 11. Real-World Case Study: Dynamic Redux-Style Action & Reducer Map

A microservice event router using enhanced object literals to dynamically register command handlers and sanitize incoming domain actions.

```javascript
class CommandRouter {
  static createAggregateHandlers(entityType) {
    const prefix = entityType.toUpperCase();

    // Enhanced object literal with computed keys and shorthand methods
    return {
      // Shorthand method definitions
      defaultHandler(state) {
        return state;
      },

      // Computed handler methods
      [`${prefix}_CREATED`](state, payload) {
        return { ...state, ...payload, createdAt: Date.now() };
      },

      [`${prefix}_UPDATED`](state, payload) {
        return { ...state, ...payload, updatedAt: Date.now() };
      },

      [`${prefix}_DELETED`](state) {
        return { ...state, isDeleted: true };
      }
    };
  }
}

// Verification
const userHandlers = CommandRouter.createAggregateHandlers("user");
const initialState = { id: 101, name: "Ada" };

const createdState = userHandlers["USER_CREATED"](initialState, { role: "admin" });
console.log("Created State:", createdState);
// Output: { id: 101, name: 'Ada', role: 'admin', createdAt: 1774... }
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the internal specification difference between <code>foo() {}</code> and <code>foo: function() {}</code>?</strong></summary>

1. Method shorthand `foo() {}` sets the internal `[[HomeObject]]` slot to the enclosing object literal, allowing the use of `super`. It does not have a `.prototype` property and throws a `TypeError` if invoked with `new`.
2. Classic syntax `foo: function() {}` has a `.prototype` property, can be invoked with `new`, and lacks `[[HomeObject]]` (cannot use `super`).
</details>

<details>
<summary><strong>2. How do Computed Property Names work under the hood?</strong></summary>

Any valid expression can be placed inside square brackets on the left side of an object property definition. The engine evaluates the expression at runtime and converts the result to a string or Symbol key via `ToPropertyKey()`.
</details>

<details>
<summary><strong>3. What happens if you define <code>__proto__</code> multiple times in an object literal?</strong></summary>

Under the ECMAScript specification (Annex B), defining `__proto__` more than once using standard colon syntax (`{ __proto__: a, __proto__: b }`) throws an early compile-time `SyntaxError`.
</details>

<details>
<summary><strong>4. What is the difference between <code>{ __proto__: proto }</code> and <code>{ ["__proto__"]: proto }</code>?</strong></summary>

- `{ __proto__: proto }` is an explicit syntax form that sets the internal `[[Prototype]]` of the newly created object to `proto`.
- `{ ["__proto__"]: proto }` is a computed property key that creates an own enumerable string property named `"__proto__"` without altering the object's prototype chain.
</details>

<details>
<summary><strong>5. In what order are computed properties evaluated in an object literal?</strong></summary>

Computed property expressions are evaluated sequentially in the exact order they appear in the source text, before the object literal is finalized.
</details>

<details>
<summary><strong>6. How does V8 optimize static object literals vs computed object literals?</strong></summary>

For static object literals, V8 pre-computes an `ObjectBoilerplate` and a final Hidden Class (`Map`) at compile time. At runtime, it clones the boilerplate with a fast memory copy. For computed literals, V8 must evaluate expressions dynamically, triggering incremental shape transitions.
</details>

<details>
<summary><strong>7. Can you use computed property syntax for method names and getters/setters?</strong></summary>

Yes. Computed syntax works on methods (`[methodName]() {}`) and accessors (`get [propName]() {}` and `set [propName](v) {}`).
</details>

<details>
<summary><strong>8. Why is shorthand method syntax preferred over arrow functions in object literals?</strong></summary>

Arrow functions do not bind `this` to the object literal (they inherit `this` lexically from the enclosing scope, usually `globalThis`). Shorthand methods bind `this` dynamically to the calling object receiver at runtime and enable `super` delegation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Property and Method Shorthands (Easy)
Refactor the following pre-ES6 object into modern enhanced object literal syntax:

```javascript
var title = "Architecture";
var pages = 300;
var book = {
  title: title,
  pages: pages,
  summary: function () {
    return this.title + " (" + this.pages + " pages)";
  }
};
```

<details>
<summary>View Solution</summary>

```javascript
const title = "Architecture";
const pages = 300;
const book = {
  title,
  pages,
  summary() {
    return `${this.title} (${this.pages} pages)`;
  }
};

console.log(book.summary()); // "Architecture (300 pages)"
```
</details>

### Exercise 2: Computed Environment Configuration (Easy)
Create an object using computed property syntax that maps dynamic environment variables (`PORT_DEV`, `HOST_DEV`) based on `const env = "DEV"`.

<details>
<summary>View Solution</summary>

```javascript
const env = "DEV";
const config = {
  [`PORT_${env}`]: 3000,
  [`HOST_${env}`]: "localhost"
};

console.log(config.PORT_DEV); // 3000
console.log(config.HOST_DEV); // "localhost"
```
</details>

### Exercise 3: Super Method Delegation in Literal (Medium)
Create an object `child` with prototype `parent` using literal `__proto__`, and override a method using `super`.

<details>
<summary>View Solution</summary>

```javascript
const parent = {
  calculateTotal(amount) {
    return amount * 1.10; // Base tax
  }
};

const child = {
  __proto__: parent,
  calculateTotal(amount) {
    return super.calculateTotal(amount) + 5.00; // Super call + shipping
  }
};

console.log(child.calculateTotal(100)); // (100 * 1.10) + 5 = 115
```
</details>

### Exercise 4: Dynamic Matrix Dictionary Builder (Medium)
Write a function `buildMatrixDict(rows, cols)` that uses computed properties to generate an object with coordinate keys like `"r0_c0"`, `"r0_c1"`, etc.

<details>
<summary>View Solution</summary>

```javascript
function buildMatrixDict(rows, cols) {
  const dict = {};
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      dict[`r${r}_c${c}`] = r * cols + c;
    }
  }
  return dict;
}

console.log(buildMatrixDict(2, 2));
// { r0_c0: 0, r0_c1: 1, r1_c0: 2, r1_c1: 3 }
```
</details>

### Exercise 5: Method Constructibility Guard (Hard)
Write an assertion function that empirically verifies that an object method defined with shorthand syntax throws a `TypeError` when called with `new`, while a classic function succeeds.

<details>
<summary>View Solution</summary>

```javascript
function verifyMethodConstructibility() {
  const obj = {
    classic: function () {},
    shorthand() {}
  };

  let classicWorked = false;
  let shorthandFailed = false;

  try {
    new obj.classic();
    classicWorked = true;
  } catch {}

  try {
    new obj.shorthand();
  } catch (err) {
    shorthandFailed = err instanceof TypeError;
  }

  return classicWorked && shorthandFailed;
}

console.log("Constructibility invariant verified:", verifyMethodConstructibility()); // true
```
</details>

### Exercise 6: Complete Fluent Query Literal Builder (Hard)
Build a query builder object that uses computed method names and method chaining to construct dynamic SQL WHERE clauses.

<details>
<summary>View Solution</summary>

```javascript
function createQueryBuilder(table) {
  const conditions = [];

  const builder = {
    where(column, operator, value) {
      conditions.push(`${column} ${operator} '${value}'`);
      return this;
    },
    // Dynamic convenience methods
    [`whereEquals`](col, val) {
      return this.where(col, "=", val);
    },
    [`whereLike`](col, val) {
      return this.where(col, "LIKE", val);
    },
    toSQL() {
      const whereClause = conditions.length > 0 ? ` WHERE ${conditions.join(" AND ")}` : "";
      return `SELECT * FROM ${table}${whereClause};`;
    }
  };

  return builder;
}

const sql = createQueryBuilder("users")
  .whereEquals("role", "admin")
  .whereLike("email", "%@domain.com")
  .toSQL();

console.log(sql);
// Output: "SELECT * FROM users WHERE role = 'admin' AND email LIKE '%@domain.com';"
```
</details>
