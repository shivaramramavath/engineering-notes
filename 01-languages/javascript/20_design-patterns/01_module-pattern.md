# Module Pattern

The module pattern uses a function scope and a closure to bundle state and behavior behind a small public API. Everything declared inside the function is private; only what you return is public.

It is the classic answer to "how do I get encapsulation in JavaScript without classes?" Today ES modules cover most of that need, but the pattern still matters for reading older code and for creating **per-instance** private state.

**Prerequisites:** [Scope](../04_scope-and-execution/01_scope.md), [Closures](../06_closures/01_closures.md), [IIFE](../02_functions/07_iife-and-function-properties.md)

> File-level modules (`import`/`export`) are covered in [ES Modules](../13_modules/01_es-modules.md), and older module styles in [Module Patterns](../13_modules/05_module-patterns.md). This note is about the design pattern itself.

---

## The Core Idea

```js
const cart = (() => {
  const items = []; // private: not reachable from outside

  function total() {
    return items.reduce((sum, i) => sum + i.price * i.qty, 0);
  }

  function add(item) {
    items.push({ ...item }); // copy so the caller can't mutate our state
  }

  return Object.freeze({
    add,
    total,
    get count() {
      return items.length;
    },
  });
})();

cart.add({ name: "pen", price: 10, qty: 3 });
cart.total(); // 30
cart.count;   // 1
cart.items;   // undefined: no way in
```

Two things make this work:

1. The **IIFE** runs once and creates a fresh scope.
2. The returned functions are **closures**, so they keep access to `items` after the IIFE finishes.

---

## Revealing Module Variant

Define everything as plain private functions, then "reveal" the public ones in a single `return`. The public API is visible in one place, and the functions call each other directly without `this`.

```js
const logger = (() => {
  let level = "info";
  const levels = ["debug", "info", "warn", "error"];

  const enabled = (l) => levels.indexOf(l) >= levels.indexOf(level);
  const setLevel = (l) => {
    if (!levels.includes(l)) throw new RangeError(`Unknown level: ${l}`);
    level = l;
  };
  const log = (l, ...args) => enabled(l) && console[l](...args);

  return { setLevel, info: (...a) => log("info", ...a), error: (...a) => log("error", ...a) };
})();
```

---

## Per-Instance Modules (Factory Form)

Drop the IIFE and return the object from a function. Each call creates its own private state.

```js
function createCounter(start = 0) {
  let count = start;
  return {
    increment: () => ++count,
    reset: () => { count = start; },
  };
}

const a = createCounter();
const b = createCounter(100);
a.increment(); // 1
b.increment(); // 101
```

This is the form you will actually write today. It is also the bridge to the [Factory Pattern](./02_factory-pattern.md).

---

## Module Pattern vs ES Modules

```js
// cart.js: the same idea as the first example
const items = [];

export function add(item) {
  items.push({ ...item });
}

export function total() {
  return items.reduce((sum, i) => sum + i.price * i.qty, 0);
}
```

| Need | Use |
| --- | --- |
| Private state shared by the whole app | An ES module with module-level variables |
| Private state per instance | A factory function (closure) or a class with `#private` fields |
| Encapsulation in a plain `<script>` without module support | IIFE module pattern |

The ES module version has a side effect worth knowing: module-level state is shared by every importer, so the module behaves like a [Singleton](./03_singleton-pattern.md).

---

## Common Mistakes

- **Leaking internals through the return value.** `return { getItems: () => items }` hands out the private array. Return a copy, or expose operations instead of data.
- **Assuming it is real privacy.** It is lexical privacy. Anything the module returns can still be replaced or monkey-patched, which is why the examples use `Object.freeze`.
- **Using `this` in the revealed functions.** Destructured functions (`const { add } = cart`) lose their `this`. Closures over private variables don't have that problem, so avoid `this` here.
- **Hard-to-test private state.** You cannot reset a private variable from a test. Prefer the factory form so each test gets a fresh instance.
- **Mixing it with classes for no reason.** If you need inheritance or `instanceof`, use a class ([Private Fields](../05_this-and-oop/07_private-fields.md)).

## Debugging

Private variables are still inspectable. In DevTools, pause inside one of the returned functions and open **Scope → Closure** to see them ([DevTools](../00_setup/04_devtools-and-debugging.md)). Don't rely on this for logic, but it is useful when state looks wrong.

---

## Quick Summary

- A module is an IIFE or factory function plus a returned public API.
- Privacy comes from closures, not from the language having a `private` keyword.
- For app-wide state, ES modules replace the IIFE. For per-instance state, use a factory function.
- Never return private mutable objects directly.

**Next:** [Factory Pattern](./02_factory-pattern.md)