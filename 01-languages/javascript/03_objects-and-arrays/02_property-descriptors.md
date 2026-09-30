# Property Descriptors

Every property has hidden **attributes** that control how it behaves. A **property descriptor** describes them.

## Data property attributes

| Attribute | Meaning | Default via literal | Default via `defineProperty` |
|-----------|---------|---------------------|------------------------------|
| `value` | the value | given | `undefined` |
| `writable` | can be reassigned | `true` | `false` |
| `enumerable` | shows in `for...in`, `Object.keys`, spread | `true` | `false` |
| `configurable` | can be deleted or redefined | `true` | `false` |

```js
const user = { name: "Ada" };
Object.getOwnPropertyDescriptor(user, "name");
// { value: "Ada", writable: true, enumerable: true, configurable: true }
```

## `Object.defineProperty`

```js
const account = {};

Object.defineProperty(account, "id", {
  value: 42,
  writable: false,
  enumerable: true,
  configurable: false,
});

account.id = 99;          // ignored (TypeError in strict mode)
delete account.id;        // false (TypeError in strict mode)
account.id;               // 42
```

Several at once:

```js
Object.defineProperties(obj, {
  a: { value: 1, enumerable: true },
  b: { value: 2 },          // non-enumerable
});
```

Read all: `Object.getOwnPropertyDescriptors(obj)` (useful for accurate cloning).

## Hidden (non-enumerable) properties

```js
Object.defineProperty(user, "_internal", { value: 1, enumerable: false });

Object.keys(user);            // ["name"]
JSON.stringify(user);         // no _internal
user._internal;               // 1 (still accessible)
```

## Accessor descriptors

Use `get`/`set` instead of `value`/`writable`. See the next file.

```js
Object.defineProperty(user, "label", {
  get() { return `User ${this.name}`; },
  enumerable: true,
});
```

## Locking down objects

| Method | Add props | Delete props | Change values | Redefine attributes | Check |
|--------|-----------|--------------|---------------|---------------------|-------|
| `Object.preventExtensions(o)` | No | Yes | Yes | Yes | `Object.isExtensible` |
| `Object.seal(o)` | No | No | Yes | No | `Object.isSealed` |
| `Object.freeze(o)` | No | No | **No** | No | `Object.isFrozen` |

```js
const config = Object.freeze({ mode: "prod", nested: { a: 1 } });

config.mode = "dev";      // ignored (TypeError in strict mode)
config.nested.a = 2;      // works! freeze is shallow
```

## Deep freeze

```js
function deepFreeze(obj) {
  Object.getOwnPropertyNames(obj).forEach((key) => {
    const value = obj[key];
    if (value && typeof value === "object") deepFreeze(value);
  });
  return Object.freeze(obj);
}
```

## Strict mode behavior

In sloppy mode, invalid writes **fail silently**. In strict mode and ES modules they throw `TypeError`. Always use strict mode so mistakes are visible.

## Arrays and freezing

```js
const list = Object.freeze([1, 2, 3]);
list.push(4);     // TypeError
list[0] = 9;      // ignored / TypeError in strict mode
```

## Use cases

| Use | Approach |
|-----|----------|
| Constants / config | `Object.freeze` |
| Read-only fields on a class | `defineProperty` with `writable: false`, or private fields |
| Hide internals from JSON/loops | `enumerable: false` |
| Prevent adding typos | `Object.seal` |
| Library-internal metadata | Symbols or non-enumerable properties |

## Descriptor pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming `freeze` is deep | Nested data stays mutable | `deepFreeze` or immutable copies |
| Silent failures in sloppy mode | Bugs go unnoticed | Strict mode / modules |
| `configurable: false` too early | Cannot undo | Only when truly permanent |
| Forgetting default `false` in `defineProperty` | Property hidden and read-only | Set attributes explicitly |
| Freezing large objects in hot paths | Slower | Freeze once at startup |

## Key takeaways

- Properties have `value`, `writable`, `enumerable`, `configurable`
- `defineProperty` defaults everything to `false`
- `preventExtensions` < `seal` < `freeze` in strictness, all shallow
- Turn on strict mode so invalid writes throw

**Next:** [Getters and Setters](./03_getters-and-setters.md)
