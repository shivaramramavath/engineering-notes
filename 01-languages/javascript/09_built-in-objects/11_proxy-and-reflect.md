# Proxy and Reflect

A **`Proxy`** wraps an object and intercepts fundamental operations (get, set, delete, `in`, function calls, ...). **`Reflect`** exposes those same operations as functions, so a proxy can forward to the original behavior.

```
code ──► Proxy ──► handler traps ──► target object
```

## Basic Proxy

```js
const target = { name: "Ada" };

const proxy = new Proxy(target, {
  get(target, prop, receiver) {
    console.log("get", String(prop));
    return Reflect.get(target, prop, receiver);
  },
  set(target, prop, value, receiver) {
    console.log("set", String(prop), value);
    return Reflect.set(target, prop, value, receiver);   // must return true to signal success
  },
});

proxy.name;          // logs "get name"
proxy.age = 36;      // logs "set age 36"
```

## Traps

| Trap | Intercepts |
|------|-----------|
| `get(target, prop, receiver)` | property read |
| `set(target, prop, value, receiver)` | property write |
| `has(target, prop)` | `prop in proxy` |
| `deleteProperty(target, prop)` | `delete proxy.prop` |
| `ownKeys(target)` | `Object.keys`, `for...in`, `Reflect.ownKeys` |
| `getOwnPropertyDescriptor(target, prop)` | descriptors, `hasOwnProperty` |
| `defineProperty(target, prop, desc)` | `Object.defineProperty` |
| `getPrototypeOf` / `setPrototypeOf` | prototype access |
| `isExtensible` / `preventExtensions` | extensibility |
| `apply(target, thisArg, args)` | function call |
| `construct(target, args, newTarget)` | `new proxy()` |

## Reflect

`Reflect` methods mirror the traps: `Reflect.get`, `set`, `has`, `deleteProperty`, `ownKeys`, `getOwnPropertyDescriptor`, `defineProperty`, `getPrototypeOf`, `setPrototypeOf`, `apply`, `construct`, `isExtensible`, `preventExtensions`.

Why use `Reflect` instead of the operators:

| Reason | Example |
|--------|---------|
| Returns booleans instead of throwing | `Reflect.set(...)`, `Reflect.defineProperty(...)` |
| Correct `receiver` for getters/setters and inheritance | `Reflect.get(target, prop, receiver)` |
| Function form of operators | `Reflect.has(obj, "x")` is `"x" in obj` |
| Complete key listing | `Reflect.ownKeys(obj)` includes symbols |

## Practical use cases

### Validation

```js
const validator = {
  set(target, prop, value) {
    if (prop === "age" && (!Number.isInteger(value) || value < 0)) {
      throw new TypeError("age must be a non-negative integer");
    }
    return Reflect.set(target, prop, value);
  },
};
const person = new Proxy({}, validator);
person.age = 30;
person.age = -1;       // TypeError
```

### Default values

```js
const withDefault = (obj, fallback) => new Proxy(obj, {
  get: (t, p, r) => (p in t ? Reflect.get(t, p, r) : fallback),
});
const scores = withDefault({ a: 1 }, 0);
scores.a;   // 1
scores.z;   // 0
```

### Negative array indexes

```js
const neg = (arr) => new Proxy(arr, {
  get(t, p, r) {
    const i = typeof p === "string" && /^-\d+$/.test(p) ? t.length + Number(p) : p;
    return Reflect.get(t, i, r);
  },
});
neg([1, 2, 3])[-1];   // 3
```

### Hiding properties

```js
const hide = (obj, hidden) => new Proxy(obj, {
  has: (t, p) => !hidden.includes(p) && p in t,
  ownKeys: (t) => Reflect.ownKeys(t).filter((k) => !hidden.includes(k)),
  get: (t, p, r) => (hidden.includes(p) ? undefined : Reflect.get(t, p, r)),
});
```

### Observable state (reactivity)

```js
function reactive(obj, onChange) {
  return new Proxy(obj, {
    set(t, p, v, r) {
      const old = t[p];
      const ok = Reflect.set(t, p, v, r);
      if (old !== v) onChange(p, v, old);
      return ok;
    },
    deleteProperty(t, p) { const ok = Reflect.deleteProperty(t, p); onChange(p); return ok; },
  });
}
```

Vue 3 builds its reactivity system on Proxy.

### Function proxies

```js
const traced = new Proxy(fn, {
  apply(target, thisArg, args) {
    console.time(target.name);
    try { return Reflect.apply(target, thisArg, args); }
    finally { console.timeEnd(target.name); }
  },
});
```

### Dynamic API / method missing

```js
const api = new Proxy({}, {
  get: (_, name) => (...args) => fetch(`/api/${String(name)}`, { method: "POST", body: JSON.stringify(args) }),
});
api.getUser(1);   // calls /api/getUser
```

### Lazy loading and virtual objects

Return values computed on access; keep large data out of memory until requested.

## Revocable proxies

```js
const { proxy, revoke } = Proxy.revocable({ secret: 1 }, {});
proxy.secret;   // 1
revoke();
proxy.secret;   // TypeError
```

Useful for granting temporary access.

## Invariants (rules a proxy cannot break)

The engine enforces consistency with the target:

- A non-configurable, non-writable data property must report its real value from `get`
- `ownKeys` must include all non-configurable own keys
- `has` cannot hide a non-configurable property
- `set` returning `false` throws in strict mode

Violations throw `TypeError`.

## Interaction with other features

| Feature | Behavior |
|---------|----------|
| `#private` fields | Access through the proxy throws (`this` is the proxy, not the instance) |
| `Map`/`Set`/`Date` internals | Methods called on a proxy fail; bind methods to the target |
| `===` | Proxy and target are different objects |
| `typeof`, `Array.isArray` | Reflect the target type (`Array.isArray(proxyOfArray)` is `true`) |
| `JSON.stringify` | Works via `get`/`ownKeys` traps |
| `structuredClone` | Cannot clone proxies |

```js
const m = new Proxy(new Map(), {
  get(t, p) { const v = Reflect.get(t, p, t); return typeof v === "function" ? v.bind(t) : v; },
});
m.set("a", 1);   // works thanks to bind
```

## Performance

Proxies add overhead to every intercepted operation and can block engine optimizations. Fine for framework state, validation and tooling; avoid in hot inner loops. Deep proxying of large object trees should be **lazy** (wrap nested objects on access).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Forgetting to return `true` from `set` | `TypeError` in strict mode | `return Reflect.set(...)` |
| Not passing `receiver` | Broken inheritance and getters | `Reflect.get(t, p, receiver)` |
| Wrapping objects with private fields | `TypeError` | Bind methods or avoid proxying |
| Proxying `Map`/`Set`/`Date` naively | "incompatible receiver" errors | Bind functions to the target |
| Infinite recursion inside traps | Stack overflow (e.g., `proxy.x` inside `get`) | Use `target` or `Reflect` |
| Using proxies in hot paths | Slow | Plain objects or explicit code |
| Security by proxy | Original target still reachable if leaked | Keep the target private |
| Debugging opacity | Hard to see real state | Log inside traps, DevTools shows `[[Target]]` |

## Key takeaways

- `Proxy` intercepts operations with handler traps; `Reflect` performs the default behavior
- Forward with `Reflect.*` and pass `receiver` to keep semantics correct
- Great for validation, reactivity, defaults, tracing and virtual APIs
- Mind private fields, built-in internals, invariants and performance

**Next:** [Global Objects](./12_global-objects.md)
