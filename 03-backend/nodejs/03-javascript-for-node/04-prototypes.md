# Prototypes

JavaScript's inheritance model is fundamentally different from classical class-based languages, even though `class` syntax makes it look similar. This file covers what's actually happening underneath `class`, since it explains a range of behaviors (`instanceof`, method sharing, `this` binding) that otherwise seem arbitrary.

## Every object has a prototype

```js
const obj = {};
console.log(Object.getPrototypeOf(obj) === Object.prototype); // true
```

When you access a property that doesn't exist directly on an object, JavaScript looks up its **prototype chain** — checking the object's prototype, then that prototype's prototype, and so on, until it finds the property or runs out of chain.

```js
const arr = [1, 2, 3];
arr.push(4); // push isn't "on" arr directly — it's found via Array.prototype
```

```js
console.log(arr.hasOwnProperty("push")); // false — arr itself doesn't have it
console.log(Array.prototype.hasOwnProperty("push")); // true — found here, up the chain
```

---

## Prototype-based inheritance, without `class`

```js
function Animal(name) {
  this.name = name;
}

Animal.prototype.speak = function () {
  return `${this.name} makes a sound.`;
};

const dog = new Animal("Rex");
console.log(dog.speak()); // "Rex makes a sound."
```

- `new Animal("Rex")` creates a new object, sets its prototype to `Animal.prototype`, and runs `Animal` as a constructor with `this` bound to the new object
- `speak` is defined once, on `Animal.prototype` — every instance shares the **same** function via the prototype chain, rather than each instance carrying its own copy

### Why put methods on the prototype instead of directly on the instance?

```js
// ❌ every instance gets its own copy of this function — wasteful
function Animal(name) {
  this.name = name;
  this.speak = function () {
    return `${this.name} makes a sound.`;
  };
}

// ✅ one shared function on the prototype, referenced by every instance
function Animal(name) {
  this.name = name;
}
Animal.prototype.speak = function () {
  return `${this.name} makes a sound.`;
};
```

With a thousand `Animal` instances, the second version stores `speak` in memory exactly once; the first stores it a thousand times.

---

## `class` — the same mechanism, cleaner syntax

```js
class Animal {
  constructor(name) {
    this.name = name;
  }

  speak() {
    return `${this.name} makes a sound.`;
  }
}

const dog = new Animal("Rex");
console.log(dog.speak());
```

This compiles down to exactly the same prototype mechanism as the function-based version above — `class` is **syntax sugar**, not a fundamentally different inheritance model. Every method defined in a `class` body is automatically placed on the prototype, not copied onto each instance.

```js
console.log(typeof Animal); // "function" — classes are functions underneath
console.log(dog.speak === Animal.prototype.speak); // true — confirms it's on the prototype
```

---

## Inheritance with `extends`

```js
class Dog extends Animal {
  speak() {
    return `${this.name} barks.`;
  }
}

const rex = new Dog("Rex");
console.log(rex.speak()); // "Rex barks."
console.log(rex instanceof Animal); // true
```

`extends` sets up the prototype chain automatically: `Dog.prototype`'s prototype is `Animal.prototype`. `Dog`'s `speak` **shadows** `Animal`'s — JavaScript finds `Dog.prototype.speak` first while walking the chain, and never reaches `Animal.prototype.speak` for a `Dog` instance.

### Calling the parent's version with `super`

```js
class Dog extends Animal {
  speak() {
    return `${super.speak()} Specifically, it barks.`;
  }
}
```

`super.speak()` explicitly calls up the prototype chain to the parent's version, rather than being shadowed entirely.

---

## `instanceof`: checking the prototype chain

```js
console.log(rex instanceof Dog); // true
console.log(rex instanceof Animal); // true — Dog's prototype chain includes Animal
console.log(rex instanceof Object); // true — everything ultimately chains to Object
```

`instanceof` works by checking whether a constructor's `.prototype` appears anywhere in the object's prototype chain — this is the actual mechanism underneath what feels like a simple type check.

---

## Private fields (modern JavaScript, not prototype-based)

```js
class BankAccount {
  #balance = 0; // truly private — not accessible outside the class at all

  deposit(amount) {
    this.#balance += amount;
  }

  getBalance() {
    return this.#balance;
  }
}

const account = new BankAccount();
account.deposit(100);
console.log(account.getBalance()); // 100
console.log(account.#balance); // SyntaxError — cannot access from outside
```

The `#` prefix marks a field as genuinely private, enforced by the language itself — a more direct and safer alternative to the closure-based privacy pattern shown in `03-closures.md`, without needing a factory function.

---

## Where this matters in real Node code

- Node's own core classes — `EventEmitter` (`02-core-modules/04-events.md`), streams (`02-core-modules/05-streams.md`) — are built with exactly this prototype/class mechanism; `class MyStream extends Readable` relies on everything covered above
- ORMs and database libraries (`07-databases/`) often model records as class instances with shared methods
- Custom error classes (`05-error-handling.md`) rely on `extends Error`, using the same inheritance mechanism

## Quick summary

- Objects look up missing properties via a chain of prototypes — this is what makes shared methods like `Array.prototype.push` work
- `class` is syntax sugar over the exact same prototype mechanism, not a separate system — methods defined in a class body land on the prototype, shared across all instances
- `extends`/`super` set up and navigate the prototype chain between parent and child classes
- `instanceof` checks whether a constructor's prototype appears in an object's chain
- Private class fields (`#field`) are a newer, language-enforced alternative to closure-based privacy

## Next

**`05-error-handling.md`** covers custom error classes — a direct, practical application of `extends` from this file.
