# Inheritance

Inheritance lets one class reuse and extend another. In JavaScript it is built on the **prototype chain**; `extends` wires that chain for you.

```
dog instance ─► Dog.prototype ─► Animal.prototype ─► Object.prototype ─► null
Dog (class)  ─► Animal (class) ─► Function.prototype                      (static inheritance)
```

## `extends` and `super`

```js
class Animal {
  constructor(name) { this.name = name; }
  speak() { return `${this.name} makes a sound`; }
  static kingdom() { return "Animalia"; }
}

class Dog extends Animal {
  constructor(name, breed) {
    super(name);              // must run before using `this`
    this.breed = breed;
  }
  speak() {
    return `${super.speak()} - Woof`;   // call the parent method
  }
}

const d = new Dog("Rex", "Lab");
d.speak();                    // "Rex makes a sound - Woof"
Dog.kingdom();                // "Animalia" (statics inherited)
d instanceof Dog;             // true
d instanceof Animal;          // true
```

## Rules for `super`

| Use | Meaning |
|-----|---------|
| `super(...)` in a constructor | call the parent constructor; **required** before `this` in derived classes |
| `super.method()` | call the parent version of a method |
| `super.prop` in static methods | refers to the parent class |
| `super` outside methods | `SyntaxError` |

```js
class Bad extends Animal {
  constructor() {
    this.x = 1;        // ReferenceError: must call super() first
    super();
  }
}
```

A derived class with **no** constructor gets `constructor(...args) { super(...args); }`.

## Overriding and polymorphism

```js
class Shape { area() { return 0; } describe() { return `${this.constructor.name}: ${this.area()}`; } }
class Circle extends Shape {
  constructor(r) { super(); this.r = r; }
  area() { return Math.PI * this.r ** 2; }
}
class Square extends Shape {
  constructor(s) { super(); this.s = s; }
  area() { return this.s ** 2; }
}

[new Circle(1), new Square(2)].map(s => s.describe());   // each uses its own area()
```

## Abstract-style base classes

JavaScript has no `abstract` keyword. Use `new.target` and throwing methods.

```js
class Repository {
  constructor() {
    if (new.target === Repository) throw new TypeError("abstract class");
  }
  find() { throw new Error("find() not implemented"); }
}
```

## Fields and initialization order

1. Parent constructor runs (parent fields initialize first)
2. `super()` returns; **then** child fields initialize
3. Rest of the child constructor runs

```js
class A { constructor() { this.init(); } init() { console.log("A"); } }
class B extends A {
  value = 1;
  init() { console.log("B", this.value); }   // prints "B undefined": child fields not set yet
}
new B();
```

Do not call overridable methods from constructors.

## Extending built-ins

```js
class Stack extends Array {
  peek() { return this[this.length - 1]; }
}
const s = new Stack(1, 2, 3);
s.peek();                    // 3
s.map(x => x) instanceof Stack;   // true (Symbol.species controls this)

class HttpError extends Error {
  constructor(status, message, options) {
    super(message, options);      // options: { cause }
    this.name = "HttpError";
    this.status = status;
  }
}
```

Set `name` for readable stack traces. See `10_error-handling/03_custom-errors.md`.

## Inheriting from plain objects and null

```js
class Base extends null {}              // no prototype chain (rare)
const proto = { hi() {} };
class X { constructor() { Object.setPrototypeOf(this, proto); } }
```

## Checking the chain

```js
Object.getPrototypeOf(Dog) === Animal;                 // true (static chain)
Object.getPrototypeOf(Dog.prototype) === Animal.prototype;   // true
Animal.prototype.isPrototypeOf(d);                     // true
```

## Composition over inheritance

Inheritance models **"is a"**; composition models **"has a"** or **"can do"**. Deep hierarchies become rigid: one change ripples through every subclass.

```js
// Fragile: Bird extends Animal, Penguin extends Bird (cannot fly!)
// Better: give objects capabilities
const canFly  = (o) => ({ fly: () => `${o.name} flies` });
const canSwim = (o) => ({ swim: () => `${o.name} swims` });
```

See file 8.

## When inheritance is a good fit

| Good | Poor |
|------|------|
| Real "is-a" relationships (`HttpError` is an `Error`) | Reusing code between unrelated things |
| Shallow (1 to 2 levels) hierarchies | Deep trees |
| Framework extension points (`Component`) | Behavior that varies by combination |
| Polymorphic collections | Data that is just configuration |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using `this` before `super()` | `ReferenceError` | Call `super()` first |
| Calling overridable methods in constructors | Child fields not ready | Init in a separate `init()` after construction |
| Deep hierarchies | Fragile base class problem | Composition |
| Forgetting `super.method()` when extending behavior | Parent logic lost | Call `super` |
| Not setting `name` on custom errors | Unhelpful traces | `this.name = new.target.name` |
| Overriding a method with different semantics | Breaks callers | Keep the contract |
| Relying on `instanceof` across realms (iframes) | Different constructors | Duck typing, `Symbol.hasInstance` |

## Key takeaways

- `extends` links both the instance chain and the static chain
- Call `super()` before touching `this`
- Parent fields and constructor run before child fields
- Keep hierarchies shallow; prefer composition for shared capabilities

**Next:** [Private Fields](./07_private-fields.md)
