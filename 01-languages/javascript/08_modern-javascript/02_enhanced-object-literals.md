# Enhanced Object Literals

ES2015 and later added shorter and more powerful ways to write object literals.

## Property shorthand

```js
const name = "Ada", age = 36;

const old = { name: name, age: age };
const modern = { name, age };            // { name: "Ada", age: 36 }

function point(x, y) { return { x, y }; }
```

## Method shorthand

```js
const counter = {
  count: 0,
  inc() { this.count++; },                // instead of inc: function () {}
  async load() { return fetch("/data"); },
  *ids() { yield 1; yield 2; },           // generator method
  async *stream() { yield await get(); }, // async generator method
};
```

Method shorthand functions:

- Can use `super` (see below)
- Are **not constructors** (cannot be used with `new`)
- Get a proper `name` for stack traces

## Computed property names

```js
const key = "id";
const obj = {
  [key]: 1,                                  // { id: 1 }
  [`${key}_max`]: 100,
  [Symbol.iterator]: function* () { yield 1; },
  ["a" + "b"]() { return "ab"; },
};

const dynamic = { [user.id]: user };         // lookup tables
```

## Getters and setters

```js
const person = {
  first: "Ada",
  last: "Lovelace",
  get full() { return `${this.first} ${this.last}`; },
  set full(v) { [this.first, this.last] = v.split(" "); },
};
```

## Spread and rest in objects (ES2018)

```js
const defaults = { theme: "light", lang: "en" };
const settings = { ...defaults, lang: "fr" };       // later wins
const { theme, ...others } = settings;              // rest
```

Conditional properties:

```js
const request = {
  method: "POST",
  ...(token && { headers: { Authorization: `Bearer ${token}` } }),
};
```

## `__proto__` in literals

```js
const animal = { eats: true };
const dog = { __proto__: animal, barks: true };     // sets the prototype
dog.eats;   // true
```

Only the **literal** form `__proto__: value` is a standard way to set the prototype at creation. Prefer `Object.create(animal, ...)` for clarity. Never merge untrusted objects that may contain `__proto__` (see `22_security/03_prototype-pollution.md`).

## `super` in object methods

```js
const base = { hello() { return "base hello"; } };

const child = {
  __proto__: base,
  hello() { return `${super.hello()} + child`; },   // works in method shorthand only
};
child.hello();   // "base hello + child"
```

`super` does not work in `hello: function () {}` or arrow properties.

## Numeric and string keys

```js
const o = { 1: "one", "two-words": 2, [3 + 1]: "four" };
Object.keys(o);   // ["1", "4", "two-words"]  (integer keys first, ascending)
```

## Trailing commas

```js
const config = {
  a: 1,
  b: 2,      // allowed: cleaner diffs
};
function f(a, b,) {}       // allowed in parameters and calls (ES2017)
```

## Object utility syntax that pairs well

```js
const { a, b: { c = 5 } = {}, ...rest } = obj;              // destructuring
const entries = Object.entries(obj).map(([k, v]) => [k, v * 2]);
const copy = Object.fromEntries(entries);
const picked = (({ id, name }) => ({ id, name }))(user);    // pick with an IIFE
```

## Symbols as keys

```js
const ID = Symbol("id");
const user = { name: "Ada", [ID]: 123 };
Object.keys(user);              // ["name"] (symbols are hidden)
Object.getOwnPropertySymbols(user);   // [Symbol(id)]
```

## Before and after

```js
// ES5
var api = {
  name: name,
  greet: function () { return "hi " + this.name; },
};
api[prefix + "Id"] = 1;

// modern
const api2 = {
  name,
  greet() { return `hi ${this.name}`; },
  [`${prefix}Id`]: 1,
};
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Arrow function as method | Wrong `this` | Method shorthand |
| Using method shorthand with `new` | `TypeError` | `function` or `class` |
| `super` in non-shorthand functions | `SyntaxError` | Method shorthand |
| Duplicate keys | Later silently wins | Lint rule `no-dupe-keys` |
| Computed key from user input into plain objects | `__proto__` / collisions | `Map` or validate keys |
| Spread order mistakes | Defaults override user values | Defaults first |
| Expecting spread to copy getters as getters | Values are copied | `Object.defineProperties` |

## Key takeaways

- Use shorthand for properties and methods, computed keys for dynamic names
- Method shorthand gives `super` access and clean names, but is not constructible
- Spread merges own enumerable properties, later keys win
- Symbols make hidden, collision-free keys

**Next:** [Symbols](./03_symbols.md)
