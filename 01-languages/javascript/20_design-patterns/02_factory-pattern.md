# Factory Pattern

A factory is a function (or static method) whose job is to **create and return objects**, so callers don't need to know which constructor, class, or setup is involved.

It matters when creation logic is non-trivial (choosing between types, applying defaults, wiring dependencies) and you want it in one place instead of scattered across the codebase.

**Prerequisites:** [Closures](../06_closures/01_closures.md), [Classes](../05_this-and-oop/05_classes.md), [Module Pattern](./01_module-pattern.md)

---

## Factory Function

The simplest form: a function that returns an object.

```js
function createUser({ name, role = "viewer" }) {
  if (!name) throw new TypeError("name is required");

  return {
    name,
    role,
    can(action) {
      return role === "admin" || action === "read";
    },
  };
}

const u = createUser({ name: "Asha" });
u.can("write"); // false
```

Compared with `new User(...)`:

| | Factory function | Constructor / class |
| --- | --- | --- |
| Needs `new` | No (can't forget it) | Yes |
| Can return different shapes | Yes | No, always an instance of that class |
| Private state | Closures | `#private` fields |
| `instanceof` works | No (plain objects) | Yes |
| Memory | Methods are recreated per object unless shared | Methods live on the prototype |

The last row matters if you create thousands of objects. Closures-based factories duplicate each method per instance. For a handful of objects it makes no practical difference.

---

## Choosing a Type at Runtime

The more useful case is when the factory decides **what** to create.

```js
class EmailNotifier {
  send(msg) { console.log("email:", msg); }
}
class SmsNotifier {
  send(msg) { console.log("sms:", msg); }
}
class PushNotifier {
  send(msg) { console.log("push:", msg); }
}

const notifiers = {
  email: () => new EmailNotifier(),
  sms: () => new SmsNotifier(),
  push: () => new PushNotifier(),
};

function createNotifier(type) {
  const make = notifiers[type];
  if (!make) throw new Error(`Unknown notifier type: "${type}"`);
  return make();
}

createNotifier("sms").send("Your code is 4821");
```

Using a lookup object instead of a `switch` keeps the factory open to extension: adding a type means adding one entry.

Guard against inherited keys if `type` can come from user input. `notifiers["constructor"]` would find `Object.prototype.constructor`. Use `Object.hasOwn(notifiers, type)` or build the registry with `new Map()`.

### Registry variant

```js
const registry = new Map();

export function register(type, make) {
  registry.set(type, make);
}

export function create(type, ...args) {
  const make = registry.get(type);
  if (!make) throw new Error(`Unknown type: "${type}"`);
  return make(...args);
}
```

Plugins can now register their own types without editing the factory.

---

## Static Factory Methods

Classes can expose named creation methods. This is useful when a constructor can't express intent well, or when creation can fail.

```js
class Money {
  #cents;

  constructor(cents) {
    this.#cents = cents;
  }

  static fromDollars(dollars) {
    return new Money(Math.round(dollars * 100));
  }

  static zero() {
    return new Money(0);
  }

  toString() {
    return `$${(this.#cents / 100).toFixed(2)}`;
  }
}

String(Money.fromDollars(12.5)); // "$12.50"
```

`Array.from()`, `Array.of()`, `Promise.resolve()` and `Object.create()` follow this idea.

---

## When to Use It

- Creation involves branching on a type, config, or environment.
- You want defaults and validation applied in exactly one place.
- You want callers to depend on an interface (`send()`), not on concrete classes.
- You need to swap implementations in tests. See [Dependency Injection](./09_dependency-injection.md).

When not to: if `new Thing(args)` is all you do, a factory is just a wrapper. Don't add one until creation logic exists.

---

## Common Mistakes

- **A growing `switch`/`if` chain** in the factory. Move to a lookup object or registry.
- **Returning inconsistent shapes** for different types. All products should share the same interface, or the caller ends up branching again.
- **Unvalidated type keys** from user input (see the prototype-key note above).
- **Factory that hides required config.** If creating a client needs an API key, make it a required parameter and throw early, not deep inside the first call.
- **Using a factory when you meant a singleton.** A factory returns a new object each call. If you need one shared instance, see [Singleton](./03_singleton-pattern.md).

---

## Quick Summary

- A factory centralizes object creation and hides which concrete type you get.
- In JS, a function returning an object is usually enough. A class is not required.
- Use a lookup object or `Map` registry instead of a `switch`.
- Closure-based factories give private state but copy methods per instance.

**Next:** [Singleton Pattern](./03_singleton-pattern.md)