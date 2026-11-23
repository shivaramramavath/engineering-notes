# Class Expressions and Structural Typing

> Class expressions provide first-class constructor values, while TypeScript's structural type system balances duck-typing flexibility with nominal privacy boundaries.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [00-classes.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/00-classes.md), [01-access-modifiers.md](file:///E:/Learning/engineering-notes/01-languages/typescript/05-classes/01-access-modifiers.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **TypeScript Baseline** | Since TS 0.9 / TS 5.x / TS 7.0 Native |
| **Tested tsconfig** | `strict: true`, `noImplicitAny: true` |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance & Type-Checker Complexity](#8-performance--type-checker-complexity)
9. [Compiler Internals](#9-compiler-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why
- **Problem Statement:** In dynamic application architectures, classes are not always declared statically at the top level of a file. Frameworks and libraries need to generate class constructors dynamically (e.g. factory functions, anonymous mixins, scoped models). Furthermore, developers coming from nominal languages (Java, C#) struggle when unrelated classes are treated as identical by TypeScript's type checker.
- **Historical Context:** JavaScript ES2015 standardized class expressions (`const C = class {}`). TypeScript supports class expressions with full type inference. Crucially, TypeScript compares classes structurally for public members, but nominally whenever private or protected members are involved.
- **Real-World Relevance:** Understanding class expressions and structural typing prevents unexpected assignability bugs, enables dynamic class factories, and teaches developers how to enforce nominal boundaries when domain safety demands it.

---

## 2. Core Concepts
- **Formal Invariants:**
  - Class Expression: An expression that evaluates to a new constructor function value. Can be anonymous (`const C = class {}`) or named (`const C = class Named {}`).
  - Scoped Name Binding: In a named class expression `const C = class InternalName {}`, the name `InternalName` is visible ONLY within the class body itself.
  - Public Structural Equivalence: If `ClassA` and `ClassB` possess identical public members, their instances are 100% interchangeable in type space.
  - Nominal Privacy Boundary: If `ClassA` declares a `private` or `protected` member `foo`, it is ONLY assignable to types whose `foo` originates from the exact same class declaration.
  - Type Query on Class Expressions: To reference the instance type of an anonymous class expression, use `InstanceType<typeof MyClass>`.
- **Mental Model:** A class declaration is like a named function declaration; a class expression is like an anonymous function assigned to a variable. Structural typing cares about what an object can *do*, not what its badge says—unless the object holds private secrets.
- **Soundness vs Ergonomics:** Structural typing allows mocking and duck-typing without complex inheritance hierarchies, while private fields allow selective nominal enforcement.

---

## 3. How It Works Internally
- **Anonymous Class Symbol Creation:** For class expressions, the binder allocates an anonymous class symbol. If assigned to a variable (`const C = class {}`), TypeScript assigns the variable's identifier name as the class's display name in diagnostics.
- **Structural Assignability Checker:** In `src/compiler/checker.ts`, `checkTypeRelatedTo` walks public members. If both types match, assignability succeeds. If private members are encountered, it checks `symbol.declarations[0] === targetSymbol.declarations[0]`.
- **Architectural Flow Diagram:**

```
+--------------------------------------------------------------------------+
|                  Structural vs Nominal Class Assignability               |
|                                                                          |
|   Case 1: Public Members Only                                            |
|   class Foo { id = 1; }  <=== IDENTICAL SHAPES ===> class Bar { id = 1; }|
|   foo = new Bar();  // LEGAL: 100% Structural Equivalence                |
|                                                                          |
|   Case 2: Private Members Present                                        |
|   class SecretFoo { private tag = 1; }        class SecretBar { private tag = 1; }
|   foo = new SecretBar(); // REJECTED TS2322!                             |
|   (Private field 'tag' originates from different declaration symbols!)   |
+--------------------------------------------------------------------------+
```

---

## 4. Syntax & API Reference
- **Anonymous and Named Class Expressions:**
  ```typescript
  // Anonymous class expression
  const UserFactory = class {
    constructor(public name: string) {}
  };

  // Named class expression
  const RecursiveTree = class TreeNode {
    children: TreeNode[] = []; // 'TreeNode' accessible inside body!
  };
  ```
- **Extracting Instance Types:**
  ```typescript
  type User = InstanceType<typeof UserFactory>;
  ```
- **tsconfig Dependencies:** Operates under standard configurations.

---

## 5. Practical Examples

### Example 1: Basic / Foundational Usage
- **Goal:** Author a dynamic class factory returning a class expression.
- **Code:**
```typescript
export function makeEntityClass(tableName: string) {
  return class Entity {
    public static readonly TABLE = tableName;
    public id: string = Math.random().toString(36).slice(2);

    describe(): string {
      return `Table: ${Entity.TABLE}, ID: ${this.id}`;
    }
  };
}

const OrderClass = makeEntityClass('orders');
type OrderInstance = InstanceType<typeof OrderClass>;

const order: OrderInstance = new OrderClass();
console.log(order.describe()); // "Table: orders, ID: ..."
```
- **Output / Type Evaluation:**
```typescript
// Fully typed dynamic class with static TABLE and instance describe() method.
```
- **Explanation:** Class expressions allow generating specialized constructor values on the fly while retaining static type checking.

### Example 2: Real-World Enterprise Pattern (Scoped Model Isolation in Multi-Tenant Apps)
- **Goal:** Create tenant-scoped model classes with isolated tenant IDs.
- **Code:**
```typescript
export function createTenantModel(tenantId: string) {
  return class ScopedModel {
    public readonly tenantId = tenantId;
    constructor(public entityId: string) {}

    getPartitionKey(): string {
      return `${this.tenantId}:${this.entityId}`;
    }
  };
}

const TenantA_User = createTenantModel('tenant_alpha');
const TenantB_User = createTenantModel('tenant_beta');

const userA = new TenantA_User('usr_1');
console.log(userA.getPartitionKey()); // "tenant_alpha:usr_1"
```
- **Output / Type Evaluation:**
```typescript
// Partitions models per tenant dynamically.
```
- **Explanation:** Class factories dynamically bind tenant configuration into constructor closures.

### Example 3: Edge Case / Boundary Condition (Accidental Structural Equivalence)
- **Goal:** Demonstrate structural duck-typing between unrelated classes and fix with branding.
- **Code:**
```typescript
class CustomerId {
  constructor(public value: string) {}
}

class OrderId {
  constructor(public value: string) {}
}

function processCustomer(id: CustomerId): void {
  console.log('Customer:', id.value);
}

const myOrder = new OrderId('ord_999');

// Flaw: Unrelated OrderId is accepted where CustomerId expected!
processCustomer(myOrder); // Compiles without error due to structural typing!

// Defensive nominal fix via unique symbol:
declare const CustomerBrand: unique symbol;
class BrandedCustomerId {
  readonly [CustomerBrand] = true;
  constructor(public value: string) {}
}

// Now protected against parameter transposition!
```
- **Output / Type Evaluation:**
```typescript
// Pure structural typing allows interchangeable classes; branding enforces nominal safety.
```
- **Explanation:** Classes with identical public shapes are structurally interchangeable unless branded or encapsulated with private members.

### Example 4: Negative Test / Type-Check Failure Case
- **Goal:** Demonstrate compiler rejection when named class expression internal name leaks.
- **Code:**
```typescript
const MyComponent = class InternalName {
  greet() {
    return 'Hello from ' + InternalName.name;
  }
};

// @ts-expect-error TS2304: Cannot find name 'InternalName'.
const test = new InternalName();
```
- **Real Compiler Error:**
```
error TS2304: Cannot find name 'InternalName'.
```
- **Explanation:** In named class expressions, the internal identifier is scoped exclusively to the class body and is not bound in the outer scope.

### Example 5: Idiomatic Modern Usage (Generic Mixin Factory with Class Expression)
- **Goal:** Combine generic constraints with class expressions for mixin composition.
- **Code:**
```typescript
type AnyConstructor = new (...args: any[]) => object;

export function WithUuid<TBase extends AnyConstructor>(Base: TBase) {
  // Returns anonymous class expression extending Base
  return class extends Base {
    public readonly uuid = 'uuid_' + Math.random().toString(36).slice(2);
  };
}

class BaseItem {
  constructor(public name: string) {}
}

const UuidItem = WithUuid(BaseItem);
const item = new UuidItem('Book');
console.log(item.name, item.uuid);
```
- **Output / Type Evaluation:**
```typescript
// Dynamic class expression successfully inherits BaseItem properties.
```
- **Explanation:** Anonymous class expressions returning derived classes are the foundation of TypeScript mixins.

---

## 6. Edge Cases, Pitfalls & Gotchas
1. **No Direct Type Declaration for Anonymous Classes:** If you declare `const C = class {}`, `C` is a value identifier. To use it as a type annotation, you must write `let inst: InstanceType<typeof C>` or declare an interface.
2. **Circular References in Named Expressions:** Inside a named class expression `const C = class Node { parent?: Node }`, the identifier `Node` refers to the class instance type, enabling clean self-referential tree modeling.
3. **Empty Classes Accept Everything:** A class with zero properties (`class Empty {}`) is structurally compatible with any non-nullish value in TypeScript (strings, numbers, objects). Always give classes at least one property or brand.

---

## 7. Common Mistakes & Anti-Patterns

| Anti-Pattern (Naive / Broken) | Real Compiler Error / Bug | Idiomatic Solution | Why It Matters |
| :--- | :--- | :--- | :--- |
| Using internal class expression name externally | `TS2304: Cannot find name 'InternalName'` | Use variable identifier name | Internal name is scoped to class body only |
| Declaring empty classes (`class Empty {}`) | Accepts any non-nullish primitive | Add fields or nominal brand | Prevents unexpected assignability |
| Relying on class names for security | Classes with same shape interchangeable | Use private fields or branded symbols | Enforces true nominal type boundaries |
| Forgetting `InstanceType<typeof C>` | Cannot use class expression as type | Use `InstanceType<typeof C>` | Extracts instance type from constructor value |
| Deeply nested class expressions | High memory usage; non-cacheable JIT shapes | Hoist shared class expressions | Optimizes V8 monomorphic hidden classes |
| Checking `instanceof` on dynamically generated classes | Separate constructor identities fail check | Compare by structure or symbol brand | Dynamic class factories produce unique constructors |

---

## 8. Performance & Type-Checker Complexity
- **JIT Hidden Class Allocation:** Every time a class factory (`makeClass()`) runs, it creates a new constructor function with a distinct hidden class in V8. Avoid invoking class factories inside hot loops.
- **Type Checker Caching:** Anonymous class expressions generate synthetic symbol names (`__class`), which the checker memoizes during assignment checks.
- **Isolated Declarations:** Exported class expressions must have explicit types under `--isolatedDeclarations`.

---

## 9. Compiler Internals
- **AST Nodes:** `ts.ClassExpression` (`SyntaxKind.ClassExpression`).
- **Symbol Synthesis:** The binder binds `ClassExpression` as an anonymous symbol, linking internal name identifiers to local block scopes.
- **Diagnostic Generation:** Flags `Diagnostics.Cannot_find_name_0` if internal class expression names are referenced in outer scopes.

---

## 10. Best Practices & Design Guidelines
- **Use Class Expressions for Dynamic Mixins and Factories:** Ideal when class creation depends on runtime parameters.
- **Brand Value Objects to Prevent Transposition:** Use `unique symbol` brands to prevent structurally identical classes (like `UserId` and `OrderId`) from being interchanged.
- **Avoid Empty Classes:** Give classes substantive fields to maintain meaningful structural guarantees.

---

## 11. Real-World Case Study
- **Scenario:** A logistics tracking platform passed entity IDs: `shipment.assignDriver(driverId, vehicleId)`. Both `DriverId` and `VehicleId` were classes with a single `constructor(public id: string)`. Due to structural typing, a developer transposed the arguments: `shipment.assignDriver(vehicleId, driverId)`. The compiler detected zero errors. In production, vehicles were assigned as drivers, crashing the dispatch algorithm and stranding 40 freight deliveries.
- **Implementation:** Added private nominal brand fields:
  ```typescript
  class DriverId {
    #driverBrand = true;
    constructor(public readonly id: string) {}
  }

  class VehicleId {
    #vehicleBrand = true;
    constructor(public readonly id: string) {}
  }
  ```
- **Outcome:** The compiler immediately threw `TS2322: Type 'VehicleId' is not assignable to type 'DriverId'`, permanently preventing parameter transposition bugs.

---

## 12. Interview Questions

### Q1: What is the scope of the name in a named class expression?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In a named class expression (`const Outer = class Inner {}`), the identifier `Inner` is scoped exclusively to the internal body of the class. It is available inside methods for recursive references or static lookups (`Inner.name`). It is completely invisible in the outer enclosing scope; any attempt to reference `Inner` outside the class body throws `TS2304: Cannot find name 'Inner'`. Outside code must use the variable identifier `Outer`.
</details>

### Q2: Why does an empty class `class Empty {}` accept almost any value in TypeScript?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript uses structural typing. An empty class has zero required properties. In structural subtyping, an object `A` is assignable to `B` if `A` possesses all properties required by `B`. Because `Empty` requires zero properties, any object or boxed primitive that can have properties read from it (i.e. any non-nullish value) satisfies the empty shape. Only `null` and `undefined` are rejected under `strictNullChecks`.
</details>

### Q3: How can you extract the instance type of an anonymous class expression?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Using the `InstanceType` and `typeof` type operators:
```typescript
const MyClass = class { constructor(public count: number) {} };
type MyClassInstance = InstanceType<typeof MyClass>;
```
`typeof MyClass` extracts the constructor type in type space, and `InstanceType<T>` unwraps the construct signature to yield the instance shape.
</details>

### Q4: When does TypeScript enforce nominal typing on classes instead of structural typing?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

TypeScript enforces nominal typing whenever a class contains:
1. TypeScript `private` fields or methods.
2. TypeScript `protected` fields or methods.
3. ECMAScript `#private` fields.
When private or protected members are present, two classes are only assignable if those members originate from the exact same declaration (i.e. one class inherits from the other).
</details>

### Q5: What is the performance implication of defining class expressions inside a loop or function?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Every time a class expression is evaluated, the JavaScript runtime allocates a brand new constructor function object and a fresh prototype object. The V8 JIT engine treats instances created from different evaluation passes as belonging to different hidden classes (shapes). If executed frequently, this causes inline caches (ICs) to become megamorphic, preventing compiler optimization and increasing memory consumption.
</details>

### Q6: Can a class expression extend another class expression?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

Yes. Class expressions can extend other class expressions or class declarations:
```typescript
const Base = class { baseProp = 1; };
const Derived = class extends Base { derivedProp = 2; };
```
This pattern is foundational to the functional mixin architecture.
</details>

### Q7: How do you achieve nominal branding on a class without using private fields?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

By adding a property keyed by a unique symbol:
```typescript
declare const BrandSymbol: unique symbol;
class BrandedEntity {
  readonly [BrandSymbol] = true;
  constructor(public id: string) {}
}
```
Because the symbol is unique, no other class can accidentally satisfy the type shape without explicitly inheriting or referencing that symbol.
</details>

### Q8: What is the difference between `class A {}` and `const A = class {}` in terms of hoisting?
<details>
<summary><strong>View Detailed Senior Answer</strong></summary>

In JavaScript, neither class declarations nor class expressions are hoisted (both reside in the Temporal Dead Zone until execution reaches their declaration). However:
- A class declaration `class A {}` registers `A` in both type space and value space under the same identifier.
- A class expression `const A = class {}` declares a `const` variable in value space; its type must be queried via `InstanceType<typeof A>` if used as an annotation.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Anonymous Class Factory (Easy)
- **Problem Statement:** Write a function `makeCounter(initial: number)` that returns a class expression with `count` and `inc()` method.
- **Test Case:**
```typescript
const Ctr = makeCounter(10);
const c = new Ctr();
c.inc();
console.log(c.count); // 11
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function makeCounter(initial: number) {
  return class Counter {
    count = initial;
    inc(): void {
      this.count++;
    }
  };
}
```
</details>

### Exercise 2: Named Recursive Class Expression (Easy)
- **Problem Statement:** Author a self-referential linked list node using a named class expression.
- **Test Case:**
```typescript
const ListNode = class Node {
  constructor(public val: number, public next: Node | null = null) {}
};
const n = new ListNode(1, new ListNode(2));
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
const ListNode = class Node {
  val: number;
  next: Node | null;

  constructor(val: number, next: Node | null = null) {
    this.val = val;
    this.next = next;
  }
};
```
</details>

### Exercise 3: Branded Nominal Entity Classes (Medium)
- **Problem Statement:** Create two classes `Meter` and `Foot` holding numerical `value`, proving they cannot be assigned to each other by implementing unique symbol brands.
- **Test Case:**
```typescript
let m: Meter = new Foot(10); // Must fail compilation!
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
declare const MeterBrand: unique symbol;
declare const FootBrand: unique symbol;

class Meter {
  readonly [MeterBrand] = true;
  constructor(public value: number) {}
}

class Foot {
  readonly [FootBrand] = true;
  constructor(public value: number) {}
}
```
</details>

### Exercise 4: Dynamic Scoped Logger Factory (Medium)
- **Problem Statement:** Write a function `createLogger(prefix: string)` returning a class expression where every instance prepends `prefix` to logged messages.
- **Test Case:**
```typescript
const AuthLogger = createLogger('[AUTH]');
const logger = new AuthLogger();
logger.log('Login attempt'); // "[AUTH] Login attempt"
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function createLogger(prefix: string) {
  return class ScopedLogger {
    log(message: string): void {
      console.log(`${prefix} ${message}`);
    }
  };
}
```
</details>

### Exercise 5: Compile-Time Structural Equivalence Asserter (Hard)
- **Problem Statement:** Author a utility type `AreClassesStructurallyEqual<C1, C2>` evaluating to `true` if instances of two classes can be assigned in both directions.
- **Test Case:**
```typescript
class A { x = 1; }
class B { x = 1; }
type T1 = AreClassesStructurallyEqual<A, B>; // true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
type AreClassesStructurallyEqual<A, B> =
  [A] extends [B]
    ? [B] extends [A]
      ? true
      : false
    : false;
```
</details>

### Exercise 6: Polymorphic Class Expression Builder (Hard)
- **Problem Statement:** Implement a function `buildRecordClass<Fields extends Record<string, any>>(defaultValues: Fields)` returning a class whose constructor initializes all specified fields.
- **Test Case:**
```typescript
const UserClass = buildRecordClass({ id: '0', active: true });
const u = new UserClass();
console.log(u.id, u.active); // "0", true
```
<details>
<summary><strong>View Solution</strong></summary>

```typescript
function buildRecordClass<Fields extends Record<string, any>>(defaults: Fields) {
  type Instance = { [K in keyof Fields]: Fields[K] };

  const DynamicClass = class {
    constructor(overrides: Partial<Fields> = {}) {
      Object.assign(this, defaults, overrides);
    }
  };

  return DynamicClass as new (overrides?: Partial<Fields>) => Instance;
}
```
</details>
