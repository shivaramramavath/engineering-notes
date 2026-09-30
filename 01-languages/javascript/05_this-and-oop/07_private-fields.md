# Private Fields

**Private class members** (ES2022) use a `#` prefix and are truly inaccessible outside the class body, enforced by the language, not by convention.

```js
class Account {
  #balance = 0;                       // private field

  deposit(amount) {
    if (amount <= 0) throw new RangeError("positive amount required");
    this.#balance += amount;
  }
  get balance() { return this.#balance; }
}

const acc = new Account();
acc.deposit(50);
acc.balance;        // 50
acc.#balance;       // SyntaxError (outside the class)
acc["#balance"];    // undefined (not a normal property)
```

## What can be private

| Member | Syntax |
|--------|--------|
| Instance field | `#count = 0;` |
| Instance method | `#validate() { ... }` |
| Private accessor | `get #secret() {}` / `set #secret(v) {}` |
| Static field | `static #instances = 0;` |
| Static method | `static #helper() {}` |

## Rules

- Must be **declared** in the class body before use; you cannot add `#x` dynamically
- Accessible only inside the class body (including nested functions and arrows within it)
- Not inherited: a subclass cannot read its parent's `#x`
- Not visible to `Object.keys`, `for...in`, `JSON.stringify`, `Object.getOwnPropertyNames`, spread, `structuredClone`
- Cannot be deleted; `delete this.#x` is a `SyntaxError`
- Two classes can have the same private name without conflict

```js
class Base { #id = 1; getId() { return this.#id; } }
class Child extends Base {
  peek() { return this.#id; }    // SyntaxError: #id is not declared in Child
}
new Child().getId();             // 1 (through the Base method)
```

## Private methods

```js
class Parser {
  #source;
  constructor(source) { this.#source = source; }

  parse() { return this.#tokenize().map(this.#classify); }

  #tokenize() { return this.#source.split(/\s+/); }
  #classify = (token) => ({ token, isNumber: !Number.isNaN(Number(token)) });   // arrow field keeps `this`
}
```

## Brand checks: `#x in obj`

Test whether an object was created by this class (has the private member).

```js
class Point {
  #x;
  constructor(x) { this.#x = x; }

  static isPoint(obj) { return #x in obj; }   // safe, no exception
}

Point.isPoint(new Point(1));   // true
Point.isPoint({});             // false
```

Accessing `#x` on an object without it throws a `TypeError`, so `#x in obj` is the safe check.

## Static private

```js
class Singleton {
  static #instance = null;
  static get() { return (Singleton.#instance ??= new Singleton()); }
  constructor() { if (Singleton.#instance) throw new Error("use get()"); }
}
```

Use the **class name** (`Singleton.#instance`), not `this`, when subclasses might call the static method: `this.#instance` would fail on subclasses.

## Private vs other approaches

| Approach | Truly private | Works with `class` | Notes |
|----------|---------------|--------------------|-------|
| `#field` | **Yes** | Yes | Language level, best default |
| `_field` naming | No | Yes | Convention only |
| Closures | Yes | Factories | No `this`, but methods per instance |
| `WeakMap` | Yes | Yes | Pre-ES2022 technique |
| `Symbol` keys | Mostly | Yes | Discoverable via `getOwnPropertySymbols` |
| `Object.defineProperty` non-enumerable | No | Yes | Hidden from loops only |

WeakMap example (legacy):

```js
const priv = new WeakMap();
class Legacy {
  constructor() { priv.set(this, { secret: 1 }); }
  reveal() { return priv.get(this).secret; }
}
```

## Interaction with other features

| Feature | Behavior |
|---------|----------|
| `Object.freeze(instance)` | Does **not** freeze private fields |
| `JSON.stringify` | Ignores private fields (add `toJSON` if needed) |
| `Proxy` around an instance | Private access through the proxy fails (`this` is the proxy) |
| Debugging | DevTools can show private fields, code cannot |
| Testing | Test through public behavior, not private members |

```js
class Box {
  #v = 1;
  get v() { return this.#v; }
}
new Proxy(new Box(), {}).v;   // TypeError: private member on the proxy
```

## Designing with private state

```js
class Stack {
  #items = [];
  push(x) { this.#items.push(x); return this; }
  pop()   { return this.#items.pop(); }
  get size() { return this.#items.length; }
  toJSON() { return { items: [...this.#items] }; }   // deliberate serialization
}
```

Expose intentions (methods), hide representation (fields).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Expecting subclasses to access `#x` | `SyntaxError` | Protected-style accessor method or shared `_x` |
| Using `this.#x` in static methods on subclasses | `TypeError` | `ClassName.#x` |
| Accessing `#x` on a non-instance | `TypeError` | `#x in obj` check |
| Assuming frozen instance protects `#x` | It does not | Guard through methods |
| Private fields with proxies | Access fails | Avoid proxying such objects, or bind |
| Forgetting `toJSON` | Private state missing in output | Provide `toJSON` |
| Over-privatizing everything | Hard to test/extend | Private for invariants only |

## Key takeaways

- `#name` members are enforced by the language and invisible from outside
- They are not inherited, not enumerable, not serializable
- `#x in obj` is the safe brand check
- Use `ClassName.#staticX` in statics to work with subclasses

**Next:** [Mixins and Composition](./08_mixins-and-composition.md)
