# Operators

Operators combine values into new values. Most are binary (`a + b`), some are unary (`!a`), and one is ternary (`a ? b : c`).

## Arithmetic

| Operator        | Meaning                                | Example                              |
| --------------- | -------------------------------------- | ------------------------------------ |
| `+` `-` `*` `/` | basic math                             | `7 / 2` → `3.5`                      |
| `%`             | remainder (sign follows the left side) | `-7 % 3` → `-1`                      |
| `**`            | exponent                               | `2 ** 10` → `1024`                   |
| `++` `--`       | increment / decrement                  | `i++` returns old, `++i` returns new |
| unary `+` `-`   | convert / negate                       | `+"5"` → `5`                         |

```js
let i = 5;
console.log(i++); // 5 (then i = 6)
console.log(++i); // 7

(-2) ** 2; // 4   (parentheses required: -2 ** 2 is a SyntaxError)
```

## Comparison

`<` `>` `<=` `>=` `==` `!=` `===` `!==`. Prefer the strict forms (see the previous file).

## Logical and short-circuit

```js
a && b; // returns a if a is falsy, otherwise b
a || b; // returns a if a is truthy, otherwise b
!a; // boolean negation
```

They return **operands**, not booleans:

```js
"" || "default"; // "default"
"hi" && "there"; // "there"
user && user.name; // guard against null user
```

## Nullish coalescing `??`

Falls back only for `null` and `undefined`.

```js
0 || 10; // 10   (0 is falsy)
0 ?? 10; // 0
"" ?? "n/a"; // ""
null ?? "n/a"; // "n/a"
```

`??` cannot be mixed with `||` or `&&` without parentheses: `a ?? b || c` is a SyntaxError.

## Optional chaining `?.`

Stops and returns `undefined` if the left side is `null` or `undefined`.

```js
user?.address?.city; // safe property access
user.getName?.(); // call only if the method exists
list?.[0]; // safe index access
```

Combine with `??`: `user?.age ?? "unknown"`.

## Assignment

| Operator                       | Same as        |
| ------------------------------ | -------------- | --- | --- | --- | -------- |
| `=`                            | assign         |
| `+=` `-=` `*=` `/=` `%=` `**=` | `a = a op b`   |
| `                              |                | =`  | `a  |     | (a = b)` |
| `&&=`                          | `a && (a = b)` |
| `??=`                          | `a ?? (a = b)` |

```js
config.retries ??= 3; // set only if null or undefined
cache.list ||= []; // set if falsy
```

## Bitwise

`&` `|` `^` `~` `<<` `>>` `>>>`. They work on 32-bit integers. Useful for flags and low-level work.

```js
5 & 3; // 1
5 | 3; // 7
1 << 4; // 16
~~3.7; // 3  (truncate, use Math.trunc for clarity)
```

## Other operators

| Operator     | Purpose                      | Example             |
| ------------ | ---------------------------- | ------------------- |
| `typeof`     | type as string               | `typeof 5`          |
| `instanceof` | prototype check              | `d instanceof Date` |
| `in`         | property exists              | `"name" in user`    |
| `delete`     | remove property              | `delete obj.key`    |
| `void`       | evaluate, return `undefined` | `void 0`            |
| `,`          | evaluate all, return last    | `(a, b)`            |
| `...`        | spread / rest                | `[...arr]`          |
| `? :`        | ternary                      | `ok ? "yes" : "no"` |

## Precedence (high to low, simplified)

| Level | Operators                                  |
| ----- | ------------------------------------------ | --- | ------ |
| 1     | `()` grouping, `.` `?.` `[]` member access |
| 2     | `!` `~` unary `+` `-` `typeof` `++` `--`   |
| 3     | `**`                                       |
| 4     | `*` `/` `%`                                |
| 5     | `+` `-`                                    |
| 6     | `<` `>` `<=` `>=` `in` `instanceof`        |
| 7     | `==` `!=` `===` `!==`                      |
| 8     | `&&`                                       |
| 9     | `                                          |     | ` `??` |
| 10    | `? :` and assignment                       |

When unsure, add parentheses.

## Operator pitfalls

| Pitfall                         | Why it hurts        | Better               |
| ------------------------------- | ------------------- | -------------------- | ------------------------ | --------------- |
| `                               |                     | ` for defaults       | Drops `0`, `""`, `false` | `??`            |
| `a ?? b                         |                     | c`                   | SyntaxError              | Add parentheses |
| `-2 ** 2`                       | SyntaxError         | `(-2) ** 2`          |
| `i++` inside larger expressions | Hard to read        | Separate statements  |
| Relying on precedence           | Readers guess wrong | Explicit parentheses |

## Key takeaways

- `&&` and `||` return operands and short-circuit
- Use `??` and `?.` for "missing value" logic, `||` only for "falsy" logic
- Logical assignment (`??=`, `||=`, `&&=`) shortens defaults
- Parentheses beat memorizing precedence

**Next:** [Expressions and Statements](./05_expressions-and-statements.md)
