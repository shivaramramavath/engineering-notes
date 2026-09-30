# Constructor Functions

A **constructor function** is a regular function meant to be called with `new` to create objects with the same shape. It is the pre-ES2015 way of doing what `class` now does.

```js
function Person(name, age) {
  this.name = name;
  this.age = age;
}

Person.prototype.greet = function () {
  return `Hi, I'm ${this.name}`;
};

const ada = new Person("Ada", 36);
ada.greet();   // "Hi, I'm Ada"
```

Convention: **PascalCase** names for constructors.

## What `new` does

`new Person("Ada", 36)` performs these steps:

1. Create an empty object
2. Set its `[[Prototype]]` to `Person.prototype`
3. Call `Person` with `this` bound to that object
4. If `Person` returns an **object**, use that; otherwise return the new object

Equivalent code:

```js
function myNew(Constructor, ...args) {
  const obj = Object.create(Constructor.prototype);   // steps 1 and 2
  const result = Constructor.apply(obj, args);        // step 3
  return result instanceof Object ? result : obj;     // step 4
}
```

## Forgetting `new`

```js
const p = Person("Ada", 36);   // sloppy: sets globalThis.name; strict: TypeError (this undefined)
```

Guard:

```js
function Person(name) {
  if (!new.target) return new Person(name);
  this.name = name;
}
```

`new.target` is the constructor that was invoked with `new`, and `undefined` for plain calls.

## Returning from constructors

```js
function A() { this.x = 1; return { y: 2 }; }
new A();   // { y: 2 }   (returned object replaces this)

function B() { this.x = 1; return 42; }
new B();   // { x: 1 }   (primitives are ignored)
```

## Where to put things

| Put on | For | Why |
|--------|-----|-----|
| `this` inside the constructor | per-instance data | each object gets its own copy |
| `Constructor.prototype` | methods | one shared function |
| `Constructor` itself | static helpers | `Person.create()` |

```js
Person.species = "human";                  // static
Person.fromJSON = (json) => new Person(json.name, json.age);
```

## The `constructor` property

```js
ada.constructor === Person;      // true (inherited from Person.prototype)
```

If you replace `prototype` wholesale, restore it:

```js
Person.prototype = { greet() {} };                 // constructor now points to Object!
Object.defineProperty(Person.prototype, "constructor", {
  value: Person, writable: true, configurable: true, enumerable: false,
});
```

## Classic inheritance with constructors

```js
function Animal(name) { this.name = name; }
Animal.prototype.speak = function () { return `${this.name} makes a sound`; };

function Dog(name) {
  Animal.call(this, name);                       // inherit instance fields
}
Dog.prototype = Object.create(Animal.prototype); // inherit methods
Dog.prototype.constructor = Dog;
Dog.prototype.speak = function () {
  return `${this.name} barks`;
};

new Dog("Rex").speak();   // "Rex barks"
```

This is what `class Dog extends Animal` does for you.

## Built-in constructors

```js
new Date(); new Map(); new Set(); new Error("x"); new Promise(() => {});
new RegExp("a+", "g");
```

Avoid `new String("x")`, `new Number(1)`, `new Boolean(false)`: they create wrapper **objects**, which are truthy and compare by reference.

## Which functions can be constructors?

| Function kind | Usable with `new`? |
|---------------|-------------------|
| Regular `function` | Yes |
| `class` | Yes (only with `new`) |
| Arrow function | **No** |
| Method shorthand (`{ m() {} }`) | **No** |
| Generator, async function | **No** |
| Bound function | Yes (bound `this` ignored) |

## Constructors vs factories

```js
function createUser(name) {
  return { name, greet() { return `Hi, ${this.name}`; } };   // no new, no this issues from callers
}
```

| | Constructor / class | Factory function |
|---|---------------------|------------------|
| Needs `new` | Yes | No |
| `instanceof` works | Yes | No (unless prototype set) |
| Shared methods on prototype | Yes | Only with `Object.create` |
| Private state via closures | Awkward | Natural |
| Flexible return type | No | Yes |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Forgetting `new` | Pollutes global or throws | `new.target` guard, or use `class` |
| Methods created in the constructor | One function per instance | Put them on `prototype` |
| Shared arrays/objects on the prototype | Mutations leak across instances | Initialize in the constructor |
| Overwriting `prototype` without `constructor` | Wrong `constructor` | Restore it, or use `Object.create` |
| Arrow function as constructor | `TypeError` | Regular function or class |
| Returning an object from a constructor by accident | Breaks `instanceof` | Return nothing |

## Key takeaways

- `new` creates an object, links its prototype, runs the function with `this`, returns the object
- Put data on `this`, methods on `prototype`
- Legacy inheritance uses `Parent.call(this)` plus `Object.create(Parent.prototype)`
- Prefer `class` syntax in new code; the mechanism underneath is the same

**Next:** [Classes](./05_classes.md)
