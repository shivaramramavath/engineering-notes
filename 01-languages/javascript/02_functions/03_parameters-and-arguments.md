# Parameters and Arguments

**Parameters** are the names in the definition. **Arguments** are the values you pass in the call.

```js
function greet(name, punctuation) {
  // parameters
  return `Hello, ${name}${punctuation}`;
}
greet("Ada", "!"); // arguments
```

## Mismatched counts are allowed

```js
function add(a, b) {
  return a + b;
}

add(1); // NaN   (b is undefined)
add(1, 2, 3); // 3     (extra argument ignored)
```

## Default parameters

Used when the argument is `undefined` (not for `null`, `0` or `""`).

```js
function connect(host = "localhost", port = 5432) { ... }

connect();               // localhost, 5432
connect(undefined, 3306);// localhost, 3306
connect(null);           // host is null (no default applied)
```

Defaults are evaluated **at call time** and can use earlier parameters:

```js
function makeId(prefix = "id", n = Date.now()) {
  return `${prefix}-${n}`;
}
function box(w, h = w) {
  return { w, h };
}
```

## Rest parameters

Collect the remaining arguments into a real array. Must be last.

```js
function sum(...nums) {
  return nums.reduce((a, b) => a + b, 0);
}
sum(1, 2, 3);        // 6

function log(level, ...messages) { ... }
```

## Spread in calls

```js
const nums = [3, 9, 4];
Math.max(...nums); // 9
log("info", ...["a", "b"]);
```

## The `arguments` object

Array-like (not an array), available in regular functions only.

```js
function old() {
  console.log(arguments.length);
  return Array.from(arguments); // or [...arguments]
}
```

Prefer rest parameters. `arguments` does not exist in arrows.

## Destructured parameters and options objects

Use one object when a function has more than 2 or 3 parameters.

```js
function createUser({ name, age = 18, role = "user" } = {}) {
  return { name, age, role };
}

createUser({ name: "Ada", role: "admin" });
createUser(); // the `= {}` default prevents a TypeError
```

Array destructuring works too: `function first([head, ...tail]) { ... }`.

## Pass by value, objects by shared reference

```js
function rename(user) {
  user.name = "Grace"; // mutates the caller's object
  user = { name: "New" }; // rebinding does NOT affect the caller
}
```

Primitives are copied. Objects pass a **copy of the reference**.

## Validating arguments

```js
function divide(a, b) {
  if (typeof a !== "number" || typeof b !== "number") {
    throw new TypeError("divide expects numbers");
  }
  if (b === 0) throw new RangeError("division by zero");
  return a / b;
}
```

## Required parameters trick

```js
const required = (name) => { throw new Error(`Missing: ${name}`); };
function save(data = required("data")) { ... }
```

## Parameter design guidelines

| Guideline                                      | Reason                                                |
| ---------------------------------------------- | ----------------------------------------------------- |
| At most 3 positional params                    | Easy to remember order                                |
| Use an options object beyond that              | Named, order-free, extendable                         |
| Required first, optional last                  | Natural call sites                                    |
| Avoid boolean flag params (`run(true, false)`) | Unreadable at call site; use options or two functions |
| Do not mutate arguments                        | Side effects surprise callers                         |

## Parameter pitfalls

| Pitfall                       | Why it hurts                         | Better          |
| ----------------------------- | ------------------------------------ | --------------- |
| Expecting defaults for `null` | Only `undefined` triggers them       | Use `??` inside |
| Mutating object arguments     | Hidden side effects                  | Copy first      |
| Destructuring without `= {}`  | `TypeError` when called with nothing | Add the default |
| Using `arguments`             | Not an array, absent in arrows       | Rest parameters |
| Long positional lists         | Order mistakes                       | Options object  |

## Key takeaways

- Defaults apply only to `undefined`
- Rest gives a real array; spread expands one in calls
- Use an options object for many parameters
- Objects are shared by reference, so avoid mutating inputs

**Next:** [Return Values](./04_return-values.md)
