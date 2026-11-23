# JavaScript Private Class Features: Hard Privacy, '#field' Syntax & Brand Checking

> Complete guide to ECMAScript private instance fields, private methods, private accessors, private static members, the '#field in obj' brand-check operator, and V8 PrivateName internals.

| Metadata | Details |
| :--- | :--- |
| **Level** | Core / Intermediate |
| **Prerequisites** | [05_this-and-oop/05_classes.md](./05_classes.md), [05_this-and-oop/06_inheritance.md](./06_inheritance.md) |
| **Time to Read** | 22 minutes |
| **Status** | Complete |
| **ECMAScript Baseline** | ES2022 (Private Fields, Private Methods & `#field in` Brand Checking) |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts: Hard Privacy vs Soft Privacy](#2-core-concepts-hard-privacy-vs-soft-privacy)
3. [How It Works: Lexical Scoping & Syntax Enforcements](#3-how-it-works-lexical-scoping--syntax-enforcements)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals: V8 PrivateName Symbols & Ephemeron Tables](#9-engine-internals-v8-privatename-symbols--ephemeron-tables)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study: Secure Cryptographic Key Store with Zero Leakage](#11-real-world-case-study-secure-cryptographic-key-store-with-zero-leakage)
12. [Interview Questions & Deep Dives](#12-interview-questions--deep-dives)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Encapsulation—the ability to hide internal state and implementation details from outside tampering—is a cornerstone of robust software architecture. For over two decades, JavaScript lacked true language-level private properties. Developers resorted to workarounds:
- **Naming Conventions**: Prefixing properties with an underscore (`this._secret`), which provided zero enforcement.
- **Closures**: Storing state inside constructor variables, which prevented prototype method sharing.
- **Symbols**: Using `Symbol()` property keys, which were still inspectable via `Object.getOwnPropertySymbols()`.

With ECMAScript 2022 (ES13), JavaScript finalized **Class Private Features**. Prefixing an identifier with a hash sign (`#`) establishes **Hard Privacy**: private fields and methods are completely inaccessible, uninspectable, and invisible from outside the declaring class body.

---

## 2. Core Concepts: Hard Privacy vs Soft Privacy

```
                         Evolution of Privacy in JavaScript
┌─────────────────────────┬─────────────────────────┬─────────────────────────┐
│ Underscore Prefix       │ Symbols                 │ Private Fields (#field) │
│ this._key = "val"       │ this[sym] = "val"       │ #key = "val"            │
├─────────────────────────┼─────────────────────────┼─────────────────────────┤
│ Soft Privacy            │ Soft Privacy            │ HARD Privacy            │
│ Easily read & modified: │ Readable via:           │ Inaccessible outside:   │
│ obj._key                │ Object.getOwnProperty   │ SyntaxError at parse!   │
│                         │   Symbols(obj)          │ Hidden from all APIs    │
└─────────────────────────┴─────────────────────────┴─────────────────────────┘
```

### The 4 Pillars of Private Class Features
1. **Private Instance Fields**: `#name = "value";`
2. **Private Instance Methods**: `#helperMethod() { /* ... */ }`
3. **Private Accessors**: `get #secret() {}` and `set #secret(v) {}`
4. **Private Static Members**: `static #connectionPool = null;`

---

## 3. How It Works: Lexical Scoping & Syntax Enforcements

Private identifiers are **lexically scoped** to the class body where they are declared:

```
Class Body Scope
┌────────────────────────────────────────────────────────┐
│ class User {                                           │
│   #passwordHash; ◄─── Declared inside class body       │
│                                                        │
│   validate(input) {                                    │
│     return hash(input) === this.#passwordHash; // OK!  │
│   }                                                    │
│ }                                                      │
└────────────────────────────────────────────────────────┘
Outside Class Body:
const user = new User();
user.#passwordHash; ──► Early SyntaxError: Private field '#passwordHash' must be declared in an enclosing class!
```

### The `#field in obj` Brand Check Operator
To verify whether an unknown object is a legitimate instance possessing a specific private field without triggering exceptions, ES2022 introduced the `#field in obj` operator:
```javascript
if (#passwordHash in targetObject) {
  // targetObject is guaranteed to be a valid User instance!
}
```

---

## 4. Syntax & API Reference

### Declarations
```javascript
class BankAccount {
  // 1. Private instance field
  #balance = 0;

  // 2. Private static field
  static #totalAccounts = 0;

  constructor(initialDeposit) {
    this.#balance = initialDeposit;
    BankAccount.#totalAccounts++;
  }

  // 3. Private method
  #audit(action) {
    console.log(`[Audit] ${action} on balance: ${this.#balance}`);
  }

  // 4. Private accessor
  get #isOverdrawn() {
    return this.#balance < 0;
  }

  // 5. Brand-checking predicate
  static isBankAccount(obj) {
    return Boolean(obj && #balance in obj);
  }
}
```

---

## 5. Practical Examples

### Example 1: Basic Private Fields & Private Methods
```javascript
class EncryptedSession {
  #secretToken;
  #createdAt;

  constructor(token) {
    this.#secretToken = token;
    this.#createdAt = Date.now();
  }

  #generateSignature(payload) {
    return `${payload}::${this.#secretToken}`;
  }

  signRequest(endpoint) {
    return this.#generateSignature(endpoint);
  }
}

const session = new EncryptedSession("sec_987654321");
console.log(session.signRequest("/api/v1/checkout"));
// Output: "/api/v1/checkout::sec_987654321"

// Verified invisible from introspection:
console.log(Object.keys(session));                   // []
console.log(Object.getOwnPropertyNames(session));    // []
console.log(Object.getOwnPropertySymbols(session));  // []
console.log(JSON.stringify(session));                // "{}"
```

### Example 2: Real-World Safe Brand Checking with `#field in`
```javascript
class SecureContainer {
  #sealedValue;

  constructor(val) {
    this.#sealedValue = val;
  }

  // Safely extract value from peer instance using brand check
  unseal(other) {
    if (!other || !(#sealedValue in other)) {
      throw new TypeError("Cannot unseal: target is not a valid SecureContainer instance");
    }
    // Cross-instance private access is VALID within the same class body!
    return other.#sealedValue;
  }
}

const containerA = new SecureContainer("Top Secret Document");
const containerB = new SecureContainer("Backup Document");

console.log(containerA.unseal(containerB)); // "Backup Document"

try {
  containerA.unseal({ fake: "object" });
} catch (err) {
  console.log("Brand check caught fake:", err.message);
}
```

### Example 3: Edge Case with Cross-Instance Private Access
```javascript
class Wallet {
  #balance;

  constructor(balance) {
    this.#balance = balance;
  }

  // A method in class Wallet can access the private fields of ANY Wallet instance!
  transferTo(targetWallet, amount) {
    if (!(#balance in targetWallet)) {
      throw new TypeError("Target must be a Wallet");
    }
    if (this.#balance < amount) throw new Error("Insufficient funds");

    this.#balance -= amount;
    targetWallet.#balance += amount; // Valid! Accessing peer private field
  }

  get balance() {
    return this.#balance;
  }
}

const w1 = new Wallet(500);
const w2 = new Wallet(100);

w1.transferTo(w2, 200);
console.log("W1 Balance:", w1.balance); // 300
console.log("W2 Balance:", w2.balance); // 300
```

### Example 4: Failure Mode with Dynamic Access & Bracket Notation
```javascript
class TestPrivate {
  #data = 42;

  readDynamic(key) {
    // Failure mode: bracket notation obj[#key] or obj["#data"] is INVALID
    // return this[key]; // Evaluates this["#data"], which is undefined!
  }
}

const t = new TestPrivate();
// @ts-ignore
console.log(t["#data"]); // undefined (Bracket lookup treats "#data" as plain string)
```

### Example 5: Idiomatic Static Private Counter
```javascript
class MetricTracker {
  static #totalCount = 0;
  static #allTrackers = new WeakSet();

  constructor(name) {
    this.name = name;
    MetricTracker.#totalCount++;
    MetricTracker.#allTrackers.add(this);
  }

  static get totalActiveTrackers() {
    return MetricTracker.#totalCount;
  }

  static verifyInstance(instance) {
    return MetricTracker.#allTrackers.has(instance);
  }
}

new MetricTracker("Alpha");
new MetricTracker("Beta");
console.log("Total registered:", MetricTracker.totalActiveTrackers); // 2
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Private Fields Cannot Be Deleted**: Attempting `delete this.#field` throws a compile-time `SyntaxError`.
2. **Private Fields Are Not Inherited**: Subclasses do not inherit or have access to superclass private fields. If `Parent` has `#secret`, `Child` cannot access `this.#secret` (doing so throws a `SyntaxError`).
3. **No Dynamic Declaration**: Every private field must be explicitly declared in the class header. You cannot create a private field on the fly inside a method.
4. **Accessing Missing Private Field Throws TypeError**:
   ```javascript
   class C {
     #x = 1;
     static read(obj) { return obj.#x; }
   }
   C.read({}); // Throws TypeError: Cannot read private member #x from an object whose class did not declare it
   ```

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Bad Code | Good Code | Why It Matters |
| :--- | :--- | :--- | :--- |
| Bracket access on private field | `this["#key"]` | `this.#key` | Bracket notation accesses plain strings; returns `undefined`. |
| Subclass accessing parent private field | `super.#field` or `this.#field` | Use protected getter method | Private fields are strictly scoped to declaring class. |
| Expecting private fields in JSON | `JSON.stringify(instance)` | Implement `.toJSON()` method | `JSON.stringify` completely ignores all private fields. |
| Using `instanceof` for brand check | `if (obj instanceof MyClass)` | `if (#field in obj)` | `instanceof` fails across iframes; brand check is realm-safe. |
| Attempting to delete private field | `delete this.#field;` | `this.#field = null;` | `delete` on private identifier throws `SyntaxError`. |
| Missing field declaration | Using `#x` without header decl | Declare `#x;` at class top | Undeclared private fields throw compile-time `SyntaxError`. |

---

## 8. Performance Characteristics

- **Field Reads & Writes**: In modern V8, accessing `this.#field` compiles to direct memory offset access on the object's backing store, achieving **identical performance** to public fields ($O(1)$ constant time).
- **Brand Checking (`#field in obj`)**: $O(1)$ fast check verifying the object's Hidden Class / Shape against the internal PrivateName table.
- **Memory Footprint**: Private fields consume memory inside the object instance identical to public properties, but with zero overhead in property descriptor dictionaries.

---

## 9. Engine Internals: V8 PrivateName Symbols & Ephemeron Tables

Inside Google V8:
1. **`PrivateName` Internal Type**: V8 represents private identifiers internally as unforgeable, engine-internal `PrivateName` symbols.
2. Unlike JavaScript `Symbol()` primitives, `PrivateName` symbols:
   - Are completely invisible to `Object.getOwnPropertySymbols()`.
   - Cannot be intercepted by `Proxy` handlers (`has`, `get`, `set` traps are completely bypassed!).
3. **Shape Association**: When an object is instantiated, the private fields are registered on the object's `Map` (Hidden Class).
4. If an object is not of the expected `Map`, V8 throws `TypeError: Cannot read private member #field`.

---

## 10. Best Practices & Design Guidelines

1. **Use `#` for True Secrets & Internal Invariants**: Use `#field` when internal state must be protected against external corruption (e.g. database handles, cryptographic keys, token salts).
2. **Use `#field in obj` for Robust Brand Checking**: Prefer `#field in obj` over `instanceof` when verifying incoming arguments in utility methods.
3. **Provide Explicit Serialization (`toJSON`)**: Because `JSON.stringify` omits private fields, define an explicit `toJSON()` method if you wish to serialize specific subsets of data.

---

## 11. Real-World Case Study: Secure Cryptographic Key Store with Zero Leakage

A secure credential manager storing private encryption keys in memory while guaranteeing that memory dumps, object inspection, proxies, and JSON serializers cannot leak the private material.

```javascript
class CryptographicKeyStore {
  // Hard private fields
  #symmetricKey;
  #salt;
  #iterations;

  constructor(secretKey, salt = "default_salt", iterations = 100000) {
    if (typeof secretKey !== "string" || secretKey.length < 16) {
      throw new Error("Key must be a string with at least 16 characters");
    }
    this.#symmetricKey = secretKey;
    this.#salt = salt;
    this.#iterations = iterations;
  }

  // Derive encryption token without exposing master key
  deriveToken(challenge) {
    // Simulated HMAC derivation
    const raw = `${challenge}:${this.#salt}:${this.#iterations}:${this.#symmetricKey}`;
    // Simple deterministic hash simulation
    let hash = 0;
    for (let i = 0; i < raw.length; i++) {
      hash = (hash << 5) - hash + raw.charCodeAt(i);
      hash |= 0;
    }
    return `tok_${Math.abs(hash).toString(16)}`;
  }

  // Safe realm-proof validation
  static isKeyStore(obj) {
    return Boolean(obj && typeof obj === "object" && #symmetricKey in obj);
  }

  // Explicit safe serialization: omits key!
  toJSON() {
    return {
      salt: this.#salt,
      iterations: this.#iterations,
      secured: true
    };
  }
}

// Verification
const store = new CryptographicKeyStore("super_secret_master_key_123");
console.log("Token derived:", store.deriveToken("challenge_99"));
console.log("Safe JSON output:", JSON.stringify(store));
// Output: '{"salt":"default_salt","iterations":100000,"secured":true}'

// Proxy tampering test: Proxies CANNOT intercept private fields
const proxied = new Proxy(store, {
  get(target, prop) {
    console.log(`[Proxy] Trapped get on: ${String(prop)}`);
    return Reflect.get(target, prop);
  }
});
console.log("Token through proxy:", proxied.deriveToken("challenge_99"));
```

---

## 12. Interview Questions & Deep Dives

<details>
<summary><strong>1. What is the fundamental difference between <code>_privateVar</code>, <code>Symbol()</code>, and <code>#privateField</code>?</strong></summary>

- `_privateVar` is purely a developer naming convention providing zero runtime protection.
- `Symbol()` provides soft privacy (properties are hidden from `Object.keys()` and `for...in`), but can be inspected via `Object.getOwnPropertySymbols()` or `Reflect.ownKeys()`.
- `#privateField` provides **Hard Privacy**: enforced at the engine grammar and memory level. It is completely inaccessible, unforgeable, invisible to reflection APIs, and throws a syntax error if referenced outside the class body.
</details>

<details>
<summary><strong>2. Can a subclass access a private field declared in its superclass?</strong></summary>

No. Private fields are strictly scoped to the lexical body of the class that declared them. A subclass cannot access superclass private fields (`this.#field` in a derived class throws a `SyntaxError`). If a subclass needs access, the superclass must expose an accessor method.
</details>

<details>
<summary><strong>3. How does the <code>#field in obj</code> brand check work?</strong></summary>

`#field in obj` checks whether `obj` is an instance that has been initialized with the private identifier `#field`. It returns `true` if `obj` possesses the private field, and `false` if `obj` is a non-object or does not possess the field. Unlike direct private field access (`obj.#field`), which throws a `TypeError` if missing, the `in` operator evaluates safely to a boolean.
</details>

<details>
<summary><strong>4. Why does <code>this["#field"]</code> not work for private fields?</strong></summary>

In JavaScript, bracket notation (`obj[expr]`) performs dynamic property lookup using string or symbol keys. Private identifiers are **not** properties on the object in the standard property registry; they are lexical bindings managed via internal engine PrivateName slots. Attempting `this["#field"]` evaluates the literal string `"#field"`, which returns `undefined`.
</details>

<details>
<summary><strong>5. Can you declare a private field dynamically inside a method?</strong></summary>

No. The ECMAScript specification mandates that all private identifiers must be declared ahead of time in the class body. Attempting to introduce an undeclared private identifier inside a method results in a compile-time `SyntaxError`.
</details>

<details>
<summary><strong>6. How do JavaScript Proxies interact with private fields?</strong></summary>

Proxies **cannot** intercept private fields. When a method accesses `this.#field`, if `this` is a `Proxy` wrapping the target instance, the private access fails and throws a `TypeError: Cannot read private member #field from an object whose class did not declare it`, because the proxy object is not the actual raw instance possessing the internal PrivateName slot.
</details>

<details>
<summary><strong>7. Are private methods defined on the instance or on the prototype?</strong></summary>

Private methods (`#method() {}`) are shared across instances and stored in internal engine structures associated with the class description, but they are **not attached to the prototype object**. They cannot be modified, deleted, or shadowed via `Class.prototype`.
</details>

<details>
<summary><strong>8. What is the performance impact of private fields compared to public fields in V8?</strong></summary>

In modern V8, private fields have identical performance to public fields. TurboFan assigns private fields to fixed in-object memory offsets and compiles reads/writes to direct CPU load and store instructions.
</details>

---

## 13. Exercises & Challenges

### Exercise 1: Basic Private Encapsulation (Easy)
Create a class `PasswordManager` that stores a `#password` and provides `changePassword(oldPass, newPass)` and `verify(pass)` methods.

<details>
<summary>View Solution</summary>

```javascript
class PasswordManager {
  #password;

  constructor(initialPassword) {
    this.#password = initialPassword;
  }

  verify(attempt) {
    return this.#password === attempt;
  }

  changePassword(oldPass, newPass) {
    if (!this.verify(oldPass)) {
      throw new Error("Invalid existing password");
    }
    this.#password = newPass;
    return true;
  }
}

const pm = new PasswordManager("secret123");
console.log(pm.verify("secret123")); // true
console.log(pm.verify("wrong"));     // false
pm.changePassword("secret123", "newSecret456");
console.log(pm.verify("newSecret456")); // true
```
</details>

### Exercise 2: Safe Brand Checking (Easy)
Write a class `Token` with private `#val` and a static method `isToken(obj)` that returns boolean using `#val in obj`.

<details>
<summary>View Solution</summary>

```javascript
class Token {
  #val;

  constructor(val) {
    this.#val = val;
  }

  static isToken(obj) {
    return Boolean(obj && typeof obj === "object" && #val in obj);
  }
}

const t = new Token("abc");
console.log(Token.isToken(t));            // true
console.log(Token.isToken({ #val: 1 }));  // SyntaxError if authored, or false
console.log(Token.isToken(null));         // false
```
</details>

### Exercise 3: Cross-Instance Private Access (Medium)
Create a `Vector` class with private `#x` and `#y` fields, and implement an `add(otherVector)` method that adds coordinates from another `Vector`.

<details>
<summary>View Solution</summary>

```javascript
class Vector {
  #x;
  #y;

  constructor(x, y) {
    this.#x = x;
    this.#y = y;
  }

  add(other) {
    if (!other || !(#x in other) || !(#y in other)) {
      throw new TypeError("Argument must be a Vector instance");
    }
    return new Vector(this.#x + other.#x, this.#y + other.#y);
  }

  get coordinates() {
    return [this.#x, this.#y];
  }
}

const v1 = new Vector(2, 5);
const v2 = new Vector(3, 7);
const v3 = v1.add(v2);
console.log(v3.coordinates); // [5, 12]
```
</details>

### Exercise 4: Private Static Singleton Gate (Medium)
Implement an `ApplicationConfig` class that uses a private static `#instance` field to guarantee that only one instance ever exists.

<details>
<summary>View Solution</summary>

```javascript
class ApplicationConfig {
  static #instance = null;
  #settings;

  constructor(settings = {}) {
    if (ApplicationConfig.#instance !== null) {
      throw new Error("ApplicationConfig is a singleton. Use ApplicationConfig.getInstance()");
    }
    this.#settings = Object.freeze({ ...settings });
    ApplicationConfig.#instance = this;
  }

  static getInstance(defaultSettings = {}) {
    if (!ApplicationConfig.#instance) {
      new ApplicationConfig(defaultSettings);
    }
    return ApplicationConfig.#instance;
  }

  get(key) {
    return this.#settings[key];
  }
}

const cfg1 = ApplicationConfig.getInstance({ env: "production" });
const cfg2 = ApplicationConfig.getInstance();
console.log(cfg1 === cfg2); // true
console.log(cfg2.get("env")); // "production"
```
</details>

### Exercise 5: Proxy Unwrapping Pattern for Private Fields (Hard)
Because Proxies break private field access, build an `unwrapProxy` pattern that enables methods to execute safely on the underlying instance even when called through a Proxy.

<details>
<summary>View Solution</summary>

```javascript
const RAW_INSTANCE = Symbol("raw_instance");

class ProtectedModel {
  #internalScore = 100;

  constructor() {
    return new Proxy(this, {
      get(target, prop, receiver) {
        if (prop === RAW_INSTANCE) return target;
        const val = Reflect.get(target, prop, receiver);
        // Bind functions directly to underlying raw target
        return typeof val === "function" ? val.bind(target) : val;
      }
    });
  }

  getScore() {
    return this.#internalScore; // Works because 'this' is bound to raw target!
  }

  incrementScore() {
    return ++this.#internalScore;
  }
}

const model = new ProtectedModel();
console.log(model.getScore()); // 100 (No TypeError!)
console.log(model.incrementScore()); // 101
```
</details>

### Exercise 6: Complete Private State Membrane (Hard)
Implement an authorization wrapper `grantPrivilegedAccess(SecretClass)` that uses static blocks to share access to private fields exclusively with authorized handler modules.

<details>
<summary>View Solution</summary>

```javascript
let internalBridge;

class EnterpriseVault {
  #vaultKey;

  constructor(key) {
    this.#vaultKey = key;
  }

  static {
    // Expose accessor function only to internalBridge closure
    internalBridge = (instance) => {
      if (!instance || !(#vaultKey in instance)) {
        throw new TypeError("Invalid instance");
      }
      return instance.#vaultKey;
    };
  }
}

// Privileged module consumer
function privilegedKeyAudit(vaultInstance) {
  return `Audited key: ${internalBridge(vaultInstance)}`;
}

const vault = new EnterpriseVault("SEC_TOKEN_999");
console.log(privilegedKeyAudit(vault)); // "Audited key: SEC_TOKEN_999"
// Direct access outside is impossible:
// vault.#vaultKey -> SyntaxError
```
</details>
