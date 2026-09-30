# Modern Syntax Cheatsheet

A one-page reference of **old way → modern way**, grouped by task. Follow the links in the chapter README for full explanations.

## Variables

```js
var x = 1;                    // old
let y = 1;  const z = 2;      // modern: const by default, let when reassigning
```

## Strings

```js
"Hi " + name + "!"                    // old
`Hi ${name}!`                         // template literal
`line 1
line 2`                               // multi-line
"abc".includes("b");  "abc".startsWith("a");  "a-b".replaceAll("-", "+");
"5".padStart(3, "0");  " x ".trim();  "abc".at(-1);
```

## Functions

```js
function add(a, b) { return a + b; }           // declaration
const add2 = (a, b) => a + b;                  // arrow
const obj = { method() {}, async load() {} };  // shorthand methods
function f(a, b = 10, ...rest) {}              // default + rest
f(...args);                                    // spread call
```

## Objects

```js
const o = { name, age };                       // shorthand
const k = { [key]: 1, [`${key}_2`]: 2 };       // computed keys
const merged = { ...a, ...b };                 // spread merge
const { x, y: renamed = 5, ...others } = obj;  // destructure
Object.entries(o);  Object.fromEntries(pairs);  Object.hasOwn(o, "x");
Object.groupBy(items, (i) => i.type);
structuredClone(obj);
```

## Arrays

```js
const [first, , third, ...rest] = list;        // destructure
const copy = [...list];  const joined = [...a, ...b];
list.at(-1);                                   // last item
list.includes(x);
list.find(fn);  list.findLast(fn);
list.flat(2);  list.flatMap(fn);
list.toSorted();  list.toReversed();  list.with(1, "x");  list.toSpliced(1, 1);
Array.from({ length: 3 }, (_, i) => i);
```

## Optional and nullish

```js
user?.address?.city;          user.fn?.();       list?.[0];
value ?? "default";           // only null/undefined
obj.a ??= 1;  obj.b ||= 2;  obj.c &&= 3;
```

## Numbers

```js
1_000_000;                    // numeric separator
2 ** 10;                      // exponent
123n;                         // BigInt
Number.isInteger(5);  Number.isNaN(x);  Math.trunc(4.7);
0.1 + 0.2 === 0.3;            // false: mind float precision
```

## Classes

```js
class User extends Base {
  #secret = 1;                 // private field
  role = "member";             // public field
  static count = 0;
  static { User.count = 1; }   // static block

  constructor(name) { super(); this.name = name; }
  get label() { return `${this.name}`; }
  #hidden() {}
  static create() { return new User("x"); }
}
```

## Modules

```js
import fs from "node:fs";
import { a, b as c } from "./mod.js";
import * as utils from "./utils.js";
export const x = 1;
export default function () {}
export { a, b };
const mod = await import("./lazy.js");      // dynamic import
import data from "./data.json" with { type: "json" };
```

## Async

```js
async function load() {
  try {
    const res = await fetch(url, { signal });
    return await res.json();
  } catch (err) {
    throw new Error("load failed", { cause: err });
  }
}

await Promise.all([a(), b()]);
await Promise.allSettled([a(), b()]);
await Promise.race([a(), timeout]);
await Promise.any([a(), b()]);
const { promise, resolve, reject } = Promise.withResolvers();
for await (const chunk of stream) {}
```

## Iteration

```js
for (const x of iterable) {}
for (const [i, x] of arr.entries()) {}
for (const [k, v] of Object.entries(obj)) {}
for (const [k, v] of map) {}
[...set];  [...map.keys()];
function* gen() { yield 1; yield* other(); }
```

## Collections

```js
const m = new Map([["a", 1]]);  m.set("b", 2);  m.get("a");  m.has("a");
const s = new Set([1, 2, 2]);   s.add(3);  s.has(1);
new WeakMap();  new WeakSet();  new WeakRef(obj);
s1.union(s2);  s1.intersection(s2);  s1.difference(s2);   // ES2025
```

## Errors

```js
try { risky(); } catch { /* optional binding */ } finally { cleanup(); }
class HttpError extends Error {
  constructor(status, message, options) { super(message, options); this.name = "HttpError"; this.status = status; }
}
throw new Error("failed", { cause: original });
```

## Symbols and protocols

```js
const id = Symbol("id");   Symbol.for("app.key");
class C { *[Symbol.iterator]() {} get [Symbol.toStringTag]() { return "C"; } }
```

## RegExp

```js
/(?<year>\d{4})-(?<month>\d{2})/.exec("2026-09").groups.year;
/(?<=\$)\d+/;  /\p{L}+/u;  /a.b/s;  /x/d;  /[\p{L}--[a-z]]/v;
"a1b2".matchAll(/\d/g);
```

## Dates and Intl

```js
new Intl.NumberFormat("en-US", { style: "currency", currency: "USD" }).format(9.5);
new Intl.DateTimeFormat("en-GB", { dateStyle: "medium" }).format(new Date());
new Intl.RelativeTimeFormat("en").format(-1, "day");
// Temporal.Now.plainDateISO()   (check runtime support)
```

## Globals and misc

```js
globalThis;  queueMicrotask(fn);  structuredClone(v);
new AbortController();  AbortSignal.timeout(5000);
new URL("https://x.dev/a?b=1").searchParams.get("b");
Symbol.dispose;   // explicit resource management (verify support)
```

## Quick decisions

| Need | Use |
|------|-----|
| Default value for missing data | `??` |
| Safe deep read | `?.` |
| Copy an object/array (shallow) | spread |
| Deep copy | `structuredClone` |
| Non-mutating sort | `toSorted` |
| Membership test | `includes` / `Set.has` |
| Group items | `Object.groupBy` |
| Lazy sequence | generator / iterator helpers |
| Run in parallel | `Promise.all` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Using new syntax without a build target check | Runtime `SyntaxError` | Transpile, or check support |
| `||` for defaults | Drops `0`, `""`, `false` | `??` |
| Spread as a deep copy | Nested data shared | `structuredClone` |
| `async` callbacks in `forEach` | Not awaited | `for...of`, `Promise.all` |
| Assuming all "modern" APIs exist in Node LTS | Missing methods | Check `node.green`, MDN |

## Key takeaways

- Default to `const`, arrows, template literals, destructuring, spread, `?.` and `??`
- Prefer non-mutating array methods and `structuredClone`
- Use `async`/`await`, `Promise.all`, and `for await` for async work
- Verify support for the newest features before shipping

**Next:** [Built-in Objects](../09_built-in-objects/00_README.md)
