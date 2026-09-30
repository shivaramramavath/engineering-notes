# Destructuring

Destructuring **unpacks** values from arrays and properties from objects into variables using a pattern that mirrors the data.

```
const { name, age } = user;      // object pattern
const [first, second] = list;    // array pattern
```

## Object destructuring

```js
const user = { name: "Ada", age: 36, city: "London" };

const { name, age } = user;              // name = "Ada", age = 36
const { name: userName } = user;         // rename
const { country = "UK" } = user;         // default if undefined
const { city: town = "n/a" } = user;     // rename + default
const { name: n, ...rest } = user;       // rest: { age, city }
```

Nested:

```js
const order = { id: 1, customer: { name: "Ada", address: { city: "London" } } };
const { customer: { address: { city } } } = order;   // city = "London"
```

`customer` and `address` are **not** created as variables here, only `city`.

## Array destructuring

```js
const [a, b] = [1, 2, 3];          // a = 1, b = 2
const [first, , third] = [1, 2, 3];// skip with a comma
const [head, ...tail] = [1, 2, 3]; // tail = [2, 3]
const [x = 0, y = 0] = [5];        // x = 5, y = 0

// swap without a temp variable
let p = 1, q = 2;
[p, q] = [q, p];
```

Works with **any iterable**:

```js
const [c1, c2] = "hi";                  // "h", "i"
const [[k, v]] = new Map([["a", 1]]);   // k = "a", v = 1
const [first2] = new Set([9, 8]);       // 9
```

## Function parameters

```js
function connect({ host = "localhost", port = 80, secure = false } = {}) { ... }
connect({ port: 8080 });

const total = ({ price, qty }) => price * qty;
orders.map(({ id, total }) => `${id}: ${total}`);

Object.entries(obj).map(([key, value]) => `${key}=${value}`);
```

The trailing `= {}` protects against calling with no argument.

## Destructuring in loops and imports

```js
for (const { id, name } of users) { ... }
for (const [key, value] of Object.entries(obj)) { ... }

import { readFile, writeFile } from "node:fs/promises";   // similar syntax, NOT destructuring
```

## Assigning to existing variables

```js
let a, b;
({ a, b } = { a: 1, b: 2 });   // parentheses required
[a, b] = [3, 4];
```

## Computed keys

```js
const key = "name";
const { [key]: value } = { name: "Ada" };   // value = "Ada"
```

## Default values gotchas

```js
const { a = 10 } = { a: undefined };   // 10
const { b = 10 } = { b: null };        // null (only undefined triggers defaults)
const { c = compute() } = {};          // default expression runs only when needed
```

## Destructuring errors

```js
const { x } = null;       // TypeError: Cannot destructure 'null'
const [y] = undefined;    // TypeError
const [z] = 5;            // TypeError: not iterable
```

Guard with a default or optional value: `const { x } = obj ?? {};`

## Practical patterns

```js
// Return multiple values
function stats(list) { return { min: Math.min(...list), max: Math.max(...list) }; }
const { min, max } = stats([3, 1, 4]);

// Omit a property
const { password, ...safeUser } = user;

// Pick with defaults
const { theme = "light", lang = "en" } = settings;

// Regex match groups
const [, year, month] = /(\d{4})-(\d{2})/.exec("2026-09") ?? [];
const { groups: { y, m } } = /(?<y>\d{4})-(?<m>\d{2})/.exec("2026-09");
```

## Destructuring pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Destructuring `null`/`undefined` | `TypeError` | `?? {}` or defaults |
| Expecting defaults for `null` | Only `undefined` triggers them | `??` after destructuring |
| `{ a, b } = obj;` as a statement | Parsed as a block | `({ a, b } = obj);` |
| Deep nesting patterns | Unreadable | Destructure in steps |
| Unused variables from rest | Lint noise | Prefix with `_` or restructure |
| Shallow copy assumption with rest | Nested values are shared | `structuredClone` |

## Key takeaways

- Patterns mirror the data shape for objects and arrays
- Rename with `:`, default with `=`, collect with `...`
- Works on any iterable for arrays, and in function parameters
- Only `undefined` triggers defaults; `null` does not

**Next:** [Spread and Rest](./10_spread-and-rest.md)
