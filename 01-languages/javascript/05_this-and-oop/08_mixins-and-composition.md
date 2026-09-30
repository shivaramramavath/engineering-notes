# Mixins and Composition

JavaScript classes have **single inheritance**: a class can extend only one parent. Mixins and composition let you combine behavior from several sources without deep hierarchies.

```
inheritance:   "a Dog IS an Animal"
composition:   "a Dog HAS a Walker and a Barker"
```

## Why prefer composition

| Inheritance | Composition |
|-------------|-------------|
| Tight coupling to the parent | Loose coupling |
| One parent only | Combine many capabilities |
| Hierarchies get rigid (the "gorilla and banana" problem) | Swap parts freely |
| Hard to test in isolation | Parts test independently |

Rule of thumb: use inheritance for a true "is-a" with a shallow tree; use composition for reuse.

## Object composition with functions

```js
const canWalk  = (state) => ({ walk: () => `${state.name} walks` });
const canSwim  = (state) => ({ swim: () => `${state.name} swims` });
const canQuack = (state) => ({ quack: () => `${state.name} quacks` });

function createDuck(name) {
  const state = { name };
  return { ...canWalk(state), ...canSwim(state), ...canQuack(state) };
}

const duck = createDuck("Donald");
duck.swim();   // "Donald swims"
```

Behavior is assembled from small pieces. Private data stays in the closure.

## Object mixins with `Object.assign`

```js
const Serializable = {
  serialize() { return JSON.stringify(this); },
};
const Comparable = {
  equals(other) { return this.serialize() === other.serialize(); },
};

class Point {
  constructor(x, y) { this.x = x; this.y = y; }
}
Object.assign(Point.prototype, Serializable, Comparable);

new Point(1, 2).serialize();   // '{"x":1,"y":2}'
```

Caveats: `assign` copies **values** (getters are evaluated), and copied methods have no `super` link to the target.

## Class mixins (subclass factories)

A mixin is a function that takes a class and returns a subclass.

```js
const Timestamped = (Base) => class extends Base {
  createdAt = new Date();
  touch() { this.updatedAt = new Date(); }
};

const Loggable = (Base) => class extends Base {
  log(msg) { console.log(`[${this.constructor.name}] ${msg}`); }
};

class Entity { constructor(id) { this.id = id; } }

class Product extends Loggable(Timestamped(Entity)) {
  constructor(id, name) {
    super(id);
    this.name = name;
  }
}

const p = new Product(1, "Book");
p.log("created");     // [Product] created
p.touch();
p instanceof Entity;  // true
```

The prototype chain is: `Product` → `Loggable(...)` → `Timestamped(...)` → `Entity`. Each mixin adds one link.

## Mixin design guidelines

- Keep mixins **small and focused** (one capability)
- Avoid state name collisions: prefix fields or use private fields
- Do not depend on the order of mixins unless documented
- Call `super` methods to cooperate: `touch() { super.touch?.(); ... }`
- Prefer explicit dependencies: `(Base) => class extends Base` and check required methods

```js
const Validatable = (Base) => class extends Base {
  validate() {
    if (typeof super.rules !== "function") throw new Error("rules() required");
    return super.rules().every(Boolean);
  }
};
```

## Delegation

An object forwards work to a helper it owns.

```js
class Logger {
  info(msg) { console.log("[info]", msg); }
}

class OrderService {
  #logger;
  constructor(logger = new Logger()) { this.#logger = logger; }   // dependency injection

  place(order) {
    this.#logger.info(`placing ${order.id}`);
    return { ok: true };
  }
}

new OrderService({ info: () => {} });   // easy to swap in tests
```

## Strategy via composition

```js
const strategies = {
  flat: (amount) => amount - 5,
  percent: (amount) => amount * 0.9,
};

class Checkout {
  constructor(discount = strategies.flat) { this.discount = discount; }
  total(amount) { return this.discount(amount); }
}
```

More in `20_design-patterns/05_strategy-pattern.md` and `09_dependency-injection.md`.

## Symbol-based capabilities and duck typing

Check for behavior, not ancestry.

```js
const isIterable = (x) => typeof x?.[Symbol.iterator] === "function";
const isThenable = (x) => typeof x?.then === "function";
const canSave = (x) => typeof x?.save === "function";
```

## Comparison

| Technique | Best for | Trade-off |
|-----------|----------|-----------|
| Class inheritance | Shallow "is-a" hierarchies | Rigid when deep |
| Class mixins | Adding capabilities to classes | Chain depth, name collisions |
| `Object.assign` mixins | Quick sharing of plain methods | No `super`, copies values |
| Factory composition | Objects with private closure state | No `instanceof`, methods per object |
| Delegation / injection | Swappable collaborators, testing | More wiring |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Mixins that silently overwrite each other's methods | Hard-to-find bugs | Unique names, call `super` |
| Deep mixin chains | Same problems as deep inheritance | Keep 2 to 3 layers |
| Mixins with hidden state assumptions | Fragile coupling | Document and validate |
| Using `Object.assign` with getters | Values copied, accessors lost | `defineProperties` with descriptors |
| Testing mixins only through a big class | Slow feedback | Test with a small stub base |
| Reaching for `extends` by habit | Rigid design | Ask "is-a or has-a?" |

## Key takeaways

- JavaScript has single inheritance; mixins and composition fill the gap
- Class mixins are functions returning subclasses of a base
- Prefer delegation and small capability functions over deep hierarchies
- Check for behavior (duck typing), not just class ancestry

**Next:** [Closures](../06_closures/00_README.md)
