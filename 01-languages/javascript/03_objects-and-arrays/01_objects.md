# Objects

An object is a collection of **key-value pairs** (properties). Keys are strings or symbols. Values can be anything, including functions (methods).

```
object  ─────►  { key: value, key: value, ... }
user           { name: "Ada", age: 36, greet() {...} }
```

## Creating objects

```js
const user = { name: "Ada", age: 36 };          // literal (most common)
const empty = {};
const fromClass = new Date();                    // constructor
const bare = Object.create(null);                // no prototype (safe dictionary)
const child = Object.create(user);               // inherits from user
```

## Accessing properties

```js
user.name;            // dot notation
user["name"];         // bracket notation (needed for dynamic or odd keys)

const key = "age";
user[key];            // 36

const odd = { "first-name": "Ada", 1: "one" };
odd["first-name"];    // "Ada"
odd[1];               // "one" (numeric keys become strings)
```

## Adding, changing, deleting

```js
user.email = "ada@example.com";   // add
user.age = 37;                    // change
delete user.email;                // remove (returns true)
user.missing;                     // undefined (no error)
```

## Shorthand and computed keys

```js
const name = "Ada", age = 36;

const a = { name, age };                 // shorthand: { name: name, age: age }
const field = "score";
const b = { [field]: 10, [`${field}Max`]: 100 };   // computed keys
const c = { greet() { return "hi"; } };            // method shorthand
```

## Checking for properties

| Check | Own | Inherited | Value `undefined` counts |
|-------|-----|-----------|--------------------------|
| `"key" in obj` | Yes | Yes | Yes |
| `Object.hasOwn(obj, "key")` | Yes | No | Yes |
| `obj.key !== undefined` | Yes | Yes | No |
| `obj.hasOwnProperty("key")` | Yes | No | Yes (fails on `Object.create(null)`) |

```js
"toString" in user;                // true (inherited)
Object.hasOwn(user, "toString");   // false
```

Prefer `Object.hasOwn`.

## Property order

1. Integer-like keys in ascending order
2. String keys in insertion order
3. Symbols in insertion order

```js
Object.keys({ b: 1, 2: 1, a: 1, 1: 1 });   // ["1", "2", "b", "a"]
```

## Methods and `this`

```js
const counter = {
  count: 0,
  inc() { this.count++; return this; },
};
counter.inc().inc();
counter.count;   // 2
```

Details in `05_this-and-oop/01_this.md`.

## Object as a dictionary vs `Map`

| Need | Use |
|------|-----|
| Fixed, known fields (records) | object |
| Dynamic keys, frequent add/remove | `Map` |
| Non-string keys | `Map` |
| Safe dictionary with untrusted keys | `Map` or `Object.create(null)` |
| JSON serialization | object |

## Nested objects

```js
const order = {
  id: 7,
  customer: { name: "Ada", address: { city: "London" } },
  items: [{ sku: "A1", qty: 2 }],
};
order.customer.address.city;   // "London"
order.customer?.address?.zip;  // undefined (safe, see file 11)
```

## Comparing objects

```js
({}) === ({});            // false (different references)
const a = { x: 1 };
const b = a;
a === b;                  // true (same reference)
```

To compare content, compare fields, use `JSON.stringify` (order-sensitive, limited), or a deep-equal helper.

## Iterating

```js
for (const key in user) { ... }                        // includes inherited enumerable keys
for (const [k, v] of Object.entries(user)) { ... }     // own enumerable, preferred
```

## Object pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `obj[userInput]` on plain objects | Prototype keys like `__proto__`, `constructor` | `Map`, `Object.create(null)`, `Object.hasOwn` |
| `for...in` for own keys | Includes inherited keys | `Object.entries` |
| Comparing with `===` | Compares references | Compare fields or deep equal |
| Assuming key order for numeric keys | Integers sort first | Use arrays or `Map` |
| Mutating shared objects | Action at a distance | Copy before changing |
| Arrow function as a method | Wrong `this` | Method shorthand |

## Key takeaways

- Objects map string/symbol keys to values; use shorthand and computed keys
- Use `Object.hasOwn` for own-property checks
- Objects are compared and copied by reference
- Use `Map` for dynamic dictionaries

**Next:** [Property Descriptors](./02_property-descriptors.md)
