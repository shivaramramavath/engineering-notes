# Optional Chaining and Nullish

Three operators make working with **missing values** safe and readable: `?.`, `??` and `??=`.

## Optional chaining `?.`

Stops and returns `undefined` if the value before `?.` is `null` or `undefined`.

```js
const user = { profile: { address: null } };

user.profile.address.city;       // TypeError
user.profile.address?.city;      // undefined
user?.profile?.address?.city;    // undefined
```

Three forms:

```js
obj?.prop          // property
obj?.[expr]        // computed property or index
fn?.(args)         // call only if fn exists
```

```js
list?.[0];
user.getName?.();
document.querySelector("#x")?.classList.add("on");
```

### Short-circuiting

If the left side is nullish, the **rest of the chain is skipped**, including calls and side effects.

```js
let n = 0;
user?.profile?.save(n++);   // if user is null, n stays 0
```

### What it does NOT do

```js
null?.x;              // undefined
0?.toFixed(1);        // "0.0"  (only null and undefined trigger it)
"".length;            // 0, not affected
undeclaredVar?.x;     // ReferenceError (variable must exist)
user?.name = "x";     // SyntaxError (cannot assign to an optional chain)
```

## Nullish coalescing `??`

Returns the right side only when the left is `null` or `undefined`.

```js
const port = config.port ?? 3000;

0 ?? 10;        // 0
"" ?? "n/a";    // ""
false ?? true;  // false
null ?? 10;     // 10
```

### `||` vs `??`

| Value | `value || "d"` | `value ?? "d"` |
|-------|----------------|----------------|
| `null` / `undefined` | `"d"` | `"d"` |
| `0` | `"d"` | `0` |
| `""` | `"d"` | `""` |
| `false` | `"d"` | `false` |
| `NaN` | `"d"` | `NaN` |

Use `??` when `0`, `""` and `false` are valid values.

Mixing with `||` or `&&` needs parentheses: `(a ?? b) || c`.

## Nullish assignment `??=`

```js
options.retries ??= 3;       // set only if null/undefined
cache[key] ??= compute();    // memoize

user.name ||= "Guest";       // set if falsy
user.admin &&= checkRole();  // set only if truthy
```

## Combining them

```js
const city = user?.address?.city ?? "Unknown";
const first = items?.[0]?.name ?? "none";
const total = order?.items?.reduce((s, i) => s + i.price, 0) ?? 0;
```

## Alternatives

```js
// Guard clauses
if (!user?.profile) return;

// Default with destructuring
const { theme = "light" } = settings ?? {};

// Explicit check for both null and undefined
value == null;      // true for null or undefined (the one accepted use of ==)
```

## Where `?.` helps most

| Situation | Example |
|-----------|---------|
| API responses with optional fields | `res.data?.user?.email` |
| Optional callbacks | `props.onSave?.(data)` |
| DOM queries that may miss | `el?.dataset.id` |
| Optional array items | `arr?.[i]` |
| Config trees | `config?.db?.host ?? "localhost"` |

## Don't hide real bugs

Overusing `?.` can swallow mistakes: if `user` should **always** exist, a missing user is a bug and should fail loudly, not silently produce `undefined`.

```js
function getEmail(user) {
  if (!user) throw new Error("user required");   // required data: validate
  return user.contact?.email ?? null;            // optional data: chain
}
```

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `||` for defaults | Drops `0`, `""`, `false` | `??` |
| `?.` on required values | Hides bugs | Validate and throw |
| Long chains everywhere | Signals poor data modeling | Normalize data at the boundary |
| `a ?? b || c` | `SyntaxError` | Add parentheses |
| Expecting `?.` to catch errors | It only checks null/undefined | `try/catch` |
| Assigning through `?.` | `SyntaxError` | `if (obj) obj.prop = v` |
| Using it on undeclared variables | `ReferenceError` | `typeof x !== "undefined"` or declare it |

## Key takeaways

- `?.` short-circuits on `null`/`undefined` for properties, indexes and calls
- `??` gives defaults without discarding `0`, `""` or `false`
- `??=` assigns only when the target is nullish
- Use them for optional data, and validate required data explicitly

**Next:** [Scope and Execution](../04_scope-and-execution/00_README.md)
