# Conditionals

Conditionals choose which code runs based on a value.

## `if` / `else if` / `else`

```js
if (score >= 90) {
  grade = "A";
} else if (score >= 80) {
  grade = "B";
} else {
  grade = "C";
}
```

The condition is converted to boolean (truthy/falsy). Always use braces, even for one line.

## Ternary `? :`

An **expression**, good for choosing a value.

```js
const label = count === 1 ? "item" : "items";
const msg = `${count} ${count === 1 ? "item" : "items"}`;
```

Avoid nesting more than one level.

## `switch`

Compares with **strict equality** (`===`).

```js
switch (status) {
  case "pending":
  case "queued": // fall through on purpose
    handleWaiting();
    break;
  case "done": {
    // braces allow let/const inside a case
    const at = Date.now();
    handleDone(at);
    break;
  }
  default:
    handleUnknown();
}
```

| Rule       | Detail                                                    |
| ---------- | --------------------------------------------------------- |
| `break`    | Without it execution **falls through** to the next case   |
| `default`  | Runs when nothing matches, can be anywhere (usually last) |
| Comparison | `===`, so `"1"` does not match `1`                        |
| Scope      | One block for all cases unless you add `{}`               |

The `switch (true)` pattern evaluates each `case` expression:

```js
switch (true) {
  case age < 13:
    label = "child";
    break;
  case age < 20:
    label = "teen";
    break;
  default:
    label = "adult";
}
```

## Short-circuit conditionals

```js
isLoggedIn && showDashboard(); // run if truthy
const name = user?.name ?? "Guest"; // default when missing
```

Prefer `if` when the result is a side effect. Short-circuiting reads best for values.

## Guard clauses (early return)

Flatten nested code by handling bad cases first.

```js
function pay(order) {
  if (!order) return "no order";
  if (order.paid) return "already paid";
  if (order.total <= 0) return "nothing to pay";

  charge(order);
  return "ok";
}
```

## Lookup objects instead of long chains

```js
const handlers = {
  create: createUser,
  update: updateUser,
  remove: removeUser,
};

const run = handlers[action] ?? handleUnknown;
run(payload);
```

Use `Map` when keys are not strings, and `Object.hasOwn(handlers, action)` if keys come from users (avoids `"constructor"` and `"toString"` matches).

## Choosing the right tool

| Situation                      | Use                       |
| ------------------------------ | ------------------------- |
| Two branches with side effects | `if` / `else`             |
| Pick one of two values         | ternary                   |
| Many exact-value branches      | `switch` or lookup object |
| Validate inputs at the top     | guard clauses             |
| Default for missing value      | `??`                      |

## Conditional pitfalls

| Pitfall                              | Why it hurts             | Better                                |
| ------------------------------------ | ------------------------ | ------------------------------------- |
| `if (a = 5)`                         | Assigns, always truthy   | `if (a === 5)`                        |
| Missing `break`                      | Accidental fall-through  | `break` or a comment when intentional |
| `let` inside a `case` without braces | Shared scope, TDZ errors | Wrap the case in `{}`                 |
| `if (value)` when `0` is valid       | Falsy check hides it     | `value !== undefined` / `??`          |
| Deep `else` nesting                  | Hard to read             | Guard clauses                         |
| Nested ternaries                     | Unreadable               | `if` chain or lookup                  |

## Key takeaways

- Conditions use truthy/falsy; `switch` uses `===`
- Ternary is for values, `if` is for actions
- Early returns beat deep nesting
- Objects or maps can replace long `switch` statements

**Next:** [Loops](./07_loops.md)
