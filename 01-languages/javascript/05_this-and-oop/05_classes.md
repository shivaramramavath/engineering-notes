# Classes

`class` (ES2015) is **syntax over prototypes**. It gives constructors, methods, inheritance and (since ES2022) fields and private members a clearer form.

```js
class User {
  role = "member";                 // public field

  constructor(name) {
    this.name = name;
  }

  greet() {                        // method (on User.prototype)
    return `Hi, ${this.name}`;
  }
}

const u = new User("Ada");
u.greet();                         // "Hi, Ada"
typeof User;                       // "function"
```

## What is different from constructor functions

| | Class | Constructor function |
|---|-------|----------------------|
| Callable without `new` | **No** (`TypeError`) | Yes (with bugs) |
| Hoisting | In the TDZ (not usable early) | Function declarations hoisted |
| Strict mode | Always | Only if enabled |
| Methods enumerable | **No** | Yes when assigned to `prototype` |
| Inheritance keyword | `extends`, `super` | Manual wiring |

## Members at a glance

| Member | Syntax | Lives on |
|--------|--------|----------|
| Constructor | `constructor(...) {}` | the class |
| Instance field | `count = 0;` | each instance |
| Method | `run() {}` | prototype (shared) |
| Accessor | `get x() {}` / `set x(v) {}` | prototype |
| Static method | `static create() {}` | the class itself |
| Static field | `static count = 0;` | the class itself |
| Static block | `static { ... }` | runs once at class definition |
| Private | `#secret`, `#method()` | see `07_private-fields.md` |
| Computed name | `[Symbol.iterator]() {}` | prototype |

## Constructor

```js
class Point {
  constructor(x = 0, y = 0) {
    this.x = x;
    this.y = y;
  }
}
```

- Only one `constructor` per class
- If omitted, a default one is used (`constructor() {}` or `constructor(...args) { super(...args); }`)
- May `return` an object to override the result (rare)

## Fields

```js
class Counter {
  count = 0;                         // initialized per instance, before constructor body
  step = 1;
  handler = () => this.count++;      // arrow field: bound to the instance
}
```

Field initializers run in **definition order**, and can refer to `this`.

## Methods and enumerability

```js
class A { hello() {} }
Object.keys(A.prototype);            // [] (methods are non-enumerable)
```

## Getters and setters

```js
class Circle {
  #r = 1;
  get radius() { return this.#r; }
  set radius(v) {
    if (v <= 0) throw new RangeError("radius must be positive");
    this.#r = v;
  }
  get area() { return Math.PI * this.#r ** 2; }
}
```

## Static members

```js
class MathUtil {
  static PI2 = Math.PI * 2;
  static #instances = 0;

  static create() {
    MathUtil.#instances++;
    return new MathUtil();
  }
  static get count() { return MathUtil.#instances; }
}

MathUtil.create();
MathUtil.count;   // 1
```

Static members are **inherited** by subclasses (`class B extends A` gets `B.create`), and `this` inside a static method is the class it was called on.

## Static initialization blocks

Run once, when the class is defined. Good for complex setup and private static access.

```js
class Config {
  static defaults;
  static #cache = new Map();

  static {
    Config.defaults = Object.freeze({ mode: "prod" });
    Config.#cache.set("boot", Date.now());
  }
}
```

## Class expressions

```js
const Animal = class { speak() {} };            // anonymous
const Dog = class DogClass { whoAmI() { return DogClass.name; } };   // inner name visible only inside
```

## Symbol-based protocols

```js
class Range {
  constructor(from, to) { this.from = from; this.to = to; }

  *[Symbol.iterator]() {
    for (let i = this.from; i <= this.to; i++) yield i;
  }

  get [Symbol.toStringTag]() { return "Range"; }
  toString() { return `${this.from}..${this.to}`; }
  toJSON() { return { from: this.from, to: this.to }; }
}

[...new Range(1, 3)];   // [1, 2, 3]
```

## Method binding reminder

```js
const { greet } = new User("Ada");
greet();   // TypeError: this is undefined (classes are strict)
```

Bind in the constructor (`this.greet = this.greet.bind(this)`), use an arrow field, or call through the instance.

## Checking instances

```js
u instanceof User;                  // true
Object.getPrototypeOf(u) === User.prototype;   // true
u.constructor.name;                 // "User"
User.prototype.isPrototypeOf(u);    // true
```

## Class vs factory function

| Use a class when | Use a factory when |
|------------------|--------------------|
| Many instances sharing methods | You want closure-private state without `#` |
| `instanceof` checks matter | You need flexible returns |
| Inheritance hierarchy is natural | Composition fits better |
| Tooling (TypeScript, decorators) expects classes | Simpler, no `this` issues |

## Class pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Calling a class without `new` | `TypeError` | Always `new` |
| Using a class before its declaration | TDZ `ReferenceError` | Declare first |
| Passing methods as callbacks | `this` lost | Arrow field / `bind` |
| Arrow fields everywhere | One function per instance | Regular methods, bind where needed |
| Deep inheritance trees | Rigid, hard to change | Composition (see file 8) |
| Mutable static fields as global state | Hidden coupling | Modules or dependency injection |
| Expecting fields on the prototype | They are per instance | Use methods for shared behavior |

## Key takeaways

- Classes are prototype-based syntax with stricter, safer defaults
- Fields are per instance; methods and accessors live on the prototype
- `static` members belong to the class; static blocks run once at definition
- Methods are unbound: mind `this` when passing them around

**Next:** [Inheritance](./06_inheritance.md)
