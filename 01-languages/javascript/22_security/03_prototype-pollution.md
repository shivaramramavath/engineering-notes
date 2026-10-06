# Prototype Pollution

Prototype pollution is a JavaScript-specific vulnerability: an attacker causes your code to **add or change properties on `Object.prototype`**, so every plain object in the process suddenly inherits properties they chose.

It usually comes from an unsafe recursive merge, deep clone, or path-based setter that processes untrusted keys.

## Prerequisites

- [Prototypes and the prototype chain](../05_this-and-oop/03_prototypes-and-prototype-chain.md)
- [Objects](../03_objects-and-arrays/01_objects.md) and [Copying and cloning](../03_objects-and-arrays/05_copying-and-cloning.md)
- [JSON](../09_built-in-objects/08_json.md)

---

## The Mechanism

Property lookup walks the prototype chain. Every plain object inherits from `Object.prototype`:

```js
const user = {};
user.isAdmin;                       // undefined (not on the object, not on the prototype)

Object.prototype.isAdmin = true;    // pollution
user.isAdmin;                       // true, inherited!
({}).isAdmin;                       // true for every object created or already existing
```

Two special keys give access to the prototype from ordinary property access:

- `obj.__proto__` (accessor that returns/sets the prototype)
- `obj.constructor.prototype`

If attacker-controlled keys flow into code that does `target[key] = value` recursively, a key path like `__proto__` → `isAdmin` writes onto `Object.prototype`.

---

## A Vulnerable Merge

```js
// Vulnerable: recursively copies every key, including "__proto__"
function merge(target, source) {
  for (const key in source) {
    if (typeof source[key] === 'object' && source[key] !== null) {
      target[key] ??= {};
      merge(target[key], source[key]);   // for key "__proto__", target[key] is Object.prototype
    } else {
      target[key] = source[key];
    }
  }
  return target;
}

const input = JSON.parse('{"__proto__": {"isAdmin": true}}');
merge({}, input);

({}).isAdmin; // true: every object in the process is now "admin"
```

A subtle point: `JSON.parse` creates `__proto__` as a normal **own property** (so parsing alone is harmless). The damage happens when your merge code then *reads* `target['__proto__']`, which resolves to the real prototype, and writes into it.

Typical vulnerable shapes:

| Pattern | Example |
|---|---|
| Recursive merge/extend/defaults | Merging request bodies or config into an object |
| Deep clone implemented by hand | Cloning user-provided JSON |
| Path setters | `set(obj, 'a.b.c', value)` where the path comes from users |
| Query-string parsers that build nested objects | `a[b][c]=1` style parsing |

---

## Why It Matters

Pollution is a **global, process-wide state change**, and its impact depends on what code later reads missing properties:

- **Auth/authorization bypass:** `if (user.isAdmin)` is `true` when the property is inherited.
- **Denial of service:** polluting a property like `toString` or `hasOwnProperty` can crash code that calls it.
- **Logic changes:** option objects (`{}` passed to a library) pick up polluted defaults.
- In Node, pollution has been chained into more severe issues in some libraries through options that reach code execution paths; treat it as serious on servers, not only in browsers.

In Node servers the pollution persists until the process restarts and affects **all users**, not just the attacker.

---

## Defenses

### 1. Block dangerous keys in any generic merge/set

```js
const FORBIDDEN = new Set(['__proto__', 'constructor', 'prototype']);

function safeMerge(target, source) {
  for (const key of Object.keys(source)) {      // own enumerable keys only
    if (FORBIDDEN.has(key)) continue;
    const value = source[key];
    if (value && typeof value === 'object' && !Array.isArray(value)) {
      if (!Object.hasOwn(target, key) || typeof target[key] !== 'object') target[key] = {};
      safeMerge(target[key], value);
    } else {
      target[key] = value;
    }
  }
  return target;
}
```

Key points: iterate with `Object.keys` (not `for...in`, which includes inherited keys), and only descend into **own** properties via `Object.hasOwn`.

### 2. Use objects without a prototype, or a `Map`

```js
const dict = Object.create(null);   // no prototype → no inherited properties
dict.__proto__ = 'x';               // just a normal key here

const cache = new Map();            // keys can be anything; no prototype lookup surprises
```

For key/value data coming from users (lookup tables, counters, caches), `Map` is the safest default.

### 3. Check own-property when reading

```js
if (Object.hasOwn(options, 'isAdmin') && options.isAdmin) { /* ... */ }
```

Never treat "property is truthy" as proof it was set deliberately, especially for security flags.

### 4. Validate input against a schema

Reject unknown keys instead of passing the raw body into merge/assign. See [Input validation](./04_input-validation.md). A strict schema that only allows the fields you expect makes `__proto__` payloads fail before reaching dangerous code.

### 5. Harden the runtime (defense in depth)

```js
Object.freeze(Object.prototype);    // writes to it silently fail (or throw in strict mode)
```

Freezing built-in prototypes can break libraries that legitimately extend them, so test before using it in production. Node also offers a flag, `--disable-proto=delete` (or `throw`), that removes or restricts the `__proto__` accessor; check the Node docs for your version and its compatibility impact.

### 6. Keep dependencies patched

Many real-world cases were in popular utility libraries (deep merge, `set`, query parsers, templating helpers). Run audits regularly, see [Dependency security](./05_dependency-security.md).

---

## Client-Side Variant

In browsers, pollution plus code that reads missing properties (analytics config, framework options, "gadgets") can lead to XSS. For example, a script that reads `config.url` from a polluted prototype and injects it into the DOM. So prototype pollution and [XSS](./01_xss.md) can be linked.

---

## Common Mistakes

| Mistake | Fix |
|---|---|
| Writing your own deep merge/clone for user data | Use a vetted library, or `structuredClone` for cloning (no merge semantics, no prototype-key issue) |
| Using `for...in` on untrusted objects | `Object.keys` / `Object.entries` |
| Using plain `{}` as a dictionary with user keys | `Map` or `Object.create(null)` |
| Assuming `JSON.parse` output is safe to merge | The parse is safe; the merge isn't |
| Only blocking `__proto__` | Also block `constructor` and `prototype` |
| Truthiness checks on security flags | Explicit own-property checks / schema-validated objects |
| Trusting library versions blindly | Keep them updated; audit transitive deps |

---

## Debugging and Detection

```js
// A quick test: after exercising a suspected code path, is the prototype dirty?
console.log(Object.keys(Object.prototype));   // should be []
console.log(({}).polluted);                   // should be undefined
```

- Add a **regression test** that feeds `{"__proto__": {"polluted": true}}` (and the `constructor.prototype` form) through every merge/set/parse path, then asserts `({}).polluted === undefined`. Reset state afterward. See [Testing patterns](../21_testing/06_testing-patterns.md).
- Look for unexpected inherited properties in logs or when debugging odd "phantom" values on fresh objects.
- Static analysis (ESLint security plugins, `npm audit`) can flag known vulnerable packages.

---

## Quick Summary

- Prototype pollution = attacker-controlled keys modify `Object.prototype`, affecting every object.
- Root cause: **unsafe recursive merge/clone/set** that walks `__proto__` / `constructor.prototype`.
- Defend with: key blocklists in generic helpers, `Object.keys` + `Object.hasOwn`, `Map`/`Object.create(null)`, strict schema validation, and patched dependencies.
- `Object.freeze(Object.prototype)` and `--disable-proto` are extra layers, with compatibility trade-offs.
- Test your merge/parse paths with pollution payloads.

**Next:** [Input Validation](./04_input-validation.md)
