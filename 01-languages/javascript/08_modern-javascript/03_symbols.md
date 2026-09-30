# Symbols

A **symbol** (ES2015) is a primitive whose every instance is **unique**. Its main uses are collision-free property keys and hooks into the language itself (well-known symbols).

```js
const a = Symbol("id");
const b = Symbol("id");
a === b;              // false, always unique
typeof a;             // "symbol"
a.description;        // "id"
a.toString();         // "Symbol(id)"
```

Never use `new Symbol()` (it throws).

## Symbols as property keys

```js
const ID = Symbol("id");

const user = { name: "Ada", [ID]: 123 };
user[ID];                           // 123

Object.keys(user);                  // ["name"]      (not listed)
JSON.stringify(user);               // '{"name":"Ada"}'
for (const k in user) {}            // skips symbols
Object.getOwnPropertySymbols(user); // [Symbol(id)]
Reflect.ownKeys(user);              // ["name", Symbol(id)]
```

Use cases:

- Hide metadata from normal enumeration and serialization
- Avoid key collisions between libraries
- Add properties to objects you do not own without breaking them

Symbols are **not private**: anyone can discover them via `getOwnPropertySymbols`. For true privacy use `#private` fields.

## Global symbol registry

```js
const s1 = Symbol.for("app.user");   // creates or reuses a global symbol
const s2 = Symbol.for("app.user");
s1 === s2;                           // true
Symbol.keyFor(s1);                   // "app.user"
Symbol.keyFor(Symbol("local"));      // undefined
```

Use namespaced keys (`"myapp.feature"`) to avoid clashes.

## Symbols as `WeakMap` keys (ES2023)

Non-registered symbols can be `WeakMap` keys and `WeakRef` targets.

```js
const wm = new WeakMap();
wm.set(Symbol("temp"), 1);
```

## Well-known symbols

Built-in symbols let your objects customize language behavior.

| Symbol | Effect |
|--------|--------|
| `Symbol.iterator` | makes an object iterable (`for...of`, spread, destructuring) |
| `Symbol.asyncIterator` | makes it async iterable (`for await...of`) |
| `Symbol.toPrimitive` | controls conversion to primitive |
| `Symbol.toStringTag` | customizes `Object.prototype.toString` output |
| `Symbol.hasInstance` | customizes `instanceof` |
| `Symbol.species` | constructor used by derived methods (`map` on subclasses) |
| `Symbol.isConcatSpreadable` | controls `concat` flattening |
| `Symbol.unscopables` | properties excluded from `with` |
| `Symbol.match`, `matchAll`, `replace`, `search`, `split` | make objects behave like regexes for string methods |

### `Symbol.iterator`

```js
const range = {
  from: 1, to: 3,
  *[Symbol.iterator]() { for (let i = this.from; i <= this.to; i++) yield i; },
};
[...range];   // [1, 2, 3]
```

### `Symbol.toPrimitive`

```js
const money = {
  amount: 42,
  [Symbol.toPrimitive](hint) {          // hint: "number" | "string" | "default"
    if (hint === "number") return this.amount;
    if (hint === "string") return `$${this.amount}`;
    return this.amount;
  },
};
+money;          // 42
`${money}`;      // "$42"
money + 1;       // 43
```

### `Symbol.toStringTag`

```js
class Queue { get [Symbol.toStringTag]() { return "Queue"; } }
Object.prototype.toString.call(new Queue());   // "[object Queue]"
```

### `Symbol.hasInstance`

```js
class Even { static [Symbol.hasInstance](n) { return n % 2 === 0; } }
2 instanceof Even;   // true
3 instanceof Even;   // false
```

## Symbols in classes

```js
const _cache = Symbol("cache");
class Api {
  constructor() { this[_cache] = new Map(); }
  static [Symbol.hasInstance](x) { return typeof x?.get === "function"; }
}
```

## Enum-like constants

```js
const Status = Object.freeze({
  PENDING: Symbol("pending"),
  DONE: Symbol("done"),
});
task.status = Status.DONE;
```

Symbols are unique and cannot be compared by value from JSON or storage. For serializable enums use strings or numbers.

## Conversion rules

```js
String(Symbol("a"));        // "Symbol(a)" (explicit is fine)
`${Symbol("a")}`;           // TypeError (implicit string conversion)
Symbol("a") + "";           // TypeError
Number(Symbol("a"));        // TypeError
!!Symbol("a");              // true
```

## Comparing key strategies

| Need | Use |
|------|-----|
| Hidden, non-colliding metadata | Symbol keys |
| Real privacy | `#private` |
| Cross-file shared key | `Symbol.for("ns.name")` |
| Serializable identifiers | strings |
| Object-keyed metadata without touching the object | `WeakMap` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming symbols are private | Discoverable via reflection | `#private` |
| Expecting symbol keys in JSON/`Object.keys` | They are skipped | `Object.getOwnPropertySymbols` |
| Implicit string conversion | `TypeError` | `.toString()` / `.description` |
| `Symbol.for` without namespace | Global collisions | Prefix keys |
| Using symbols for data that must persist | Cannot serialize | Strings or numbers |
| Forgetting spread copies enumerable symbols | Unexpected copies | Know what you copy |

## Key takeaways

- Symbols are unique primitives, ideal for hidden keys and protocols
- `Symbol.for` shares symbols through a global registry
- Well-known symbols let objects customize iteration, conversion and `instanceof`
- Symbols are hidden from normal enumeration, but not truly private

**Next:** [Iterators and Iterables](./04_iterators-and-iterables.md)
