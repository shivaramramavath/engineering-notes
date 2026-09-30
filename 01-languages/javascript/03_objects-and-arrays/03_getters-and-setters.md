# Getters and Setters

**Accessor properties** look like normal properties but run a function when read (`get`) or written (`set`).

```
obj.prop        ─────►  get prop() { ... }
obj.prop = v    ─────►  set prop(v) { ... }
```

## In object literals

```js
const person = {
  first: "Ada",
  last: "Lovelace",

  get fullName() {
    return `${this.first} ${this.last}`;
  },
  set fullName(value) {
    [this.first, this.last] = value.split(" ");
  },
};

person.fullName;              // "Ada Lovelace"
person.fullName = "Grace Hopper";
person.first;                 // "Grace"
```

A getter without a setter is **read-only** (writes are ignored, or throw in strict mode).

## In classes

```js
class Temperature {
  #celsius = 0;

  get celsius() { return this.#celsius; }
  set celsius(value) {
    if (typeof value !== "number") throw new TypeError("number expected");
    this.#celsius = value;
  }

  get fahrenheit() { return this.#celsius * 9 / 5 + 32; }
  set fahrenheit(f) { this.celsius = (f - 32) * 5 / 9; }
}

const t = new Temperature();
t.fahrenheit = 212;
t.celsius;   // 100
```

Class accessors live on the **prototype**, shared by all instances.

## With `defineProperty`

```js
Object.defineProperty(obj, "now", {
  get() { return Date.now(); },
  enumerable: true,
  configurable: true,
});
```

You cannot mix `value`/`writable` with `get`/`set` in one descriptor.

## Static accessors

```js
class Config {
  static #instance;
  static get instance() { return (Config.#instance ??= new Config()); }
}
```

## Common uses

| Use | Example |
|-----|---------|
| Computed values | `fullName`, `area`, `total` |
| Validation on assignment | reject bad values in a setter |
| Read-only views | getter only |
| Lazy evaluation and caching | compute on first access |
| Backwards-compatible renames | old name forwards to new |
| Hiding internal representation | store cents, expose dollars |

## Lazy getter with caching

```js
const report = {
  get data() {
    const value = expensiveCompute();
    Object.defineProperty(this, "data", { value, enumerable: true });   // replace getter
    return value;
  },
};
```

## Behavior with copying and JSON

```js
const copy = { ...person };            // getters are CALLED; the copy gets plain values
JSON.stringify(person);                // own enumerable getters are included as values
Object.getOwnPropertyDescriptor(person, "fullName").get;   // the function itself
```

`Object.assign` also reads getter values. To copy accessors themselves, use `Object.defineProperties(target, Object.getOwnPropertyDescriptors(source))`.

## Getters vs methods

| Getter | Method |
|--------|--------|
| Looks like a property, no parentheses | Explicit call, can take arguments |
| Should be cheap and side-effect free | Can be expensive or have effects |
| Best for derived state | Best for actions |

## Accessor pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Getter that does heavy work | Hidden cost on every read | Cache, or use a method |
| Setter with side effects | Surprising assignments | Explicit method |
| Infinite recursion (`get x() { return this.x; }`) | Stack overflow | Store in `#x` or `_x` |
| Getter that throws | Breaks logging and serialization | Return a safe value |
| Getter without setter in sloppy mode | Writes silently ignored | Strict mode |
| Assuming spread copies accessors | Copies values only | `defineProperties` with descriptors |

## Key takeaways

- `get`/`set` create computed, validated, or read-only properties
- Keep getters cheap and side-effect free
- Store backing data in a private field or different name
- Spread and `Object.assign` copy **values**, not accessors

**Next:** [Object Static Methods](./04_object-static-methods.md)
