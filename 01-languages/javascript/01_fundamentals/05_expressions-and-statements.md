# Expressions and Statements

Every piece of JavaScript is either an **expression** (produces a value) or a **statement** (performs an action).

```
expression  ─────►  value        3 + 4,  user.name,  fn(),  a ? b : c
statement   ─────►  action       let x = 1;   if (...) {...}   for (...) {...}
```

## Quick test

If you can put it inside `console.log( ... )` or a template literal `${ ... }`, it is an **expression**.

```js
console.log(3 + 4);              // ok, expression
console.log(if (x) {});          // SyntaxError, statement
`${ok ? "yes" : "no"}`;          // ok, ternary is an expression
```

## Expressions

| Kind             | Examples                                |
| ---------------- | --------------------------------------- |
| Literal          | `42`, `"hi"`, `[1, 2]`, `{ a: 1 }`      |
| Identifier       | `count`                                 |
| Operator         | `a + b`, `!done`, `x ?? y`              |
| Call             | `Math.max(1, 2)`                        |
| Function / class | `function () {}`, `() => 1`, `class {}` |
| Assignment       | `x = 5` (yes, it evaluates to `5`)      |

## Statements

| Kind         | Examples                                             |
| ------------ | ---------------------------------------------------- |
| Declaration  | `let`, `const`, `var`, `function`, `class`           |
| Control flow | `if`, `switch`, `try`, `throw`                       |
| Loops        | `for`, `while`, `do...while`, `for...of`, `for...in` |
| Jumps        | `break`, `continue`, `return`                        |
| Block        | `{ ... }`                                            |
| Empty        | `;`                                                  |

An **expression statement** is an expression used as a statement: `doWork();`

## Blocks

A block groups statements and creates a scope for `let`/`const`.

```js
{
  const temp = compute();
  console.log(temp);
}
// temp is not visible here
```

## Function declaration vs expression

```js
sayHi(); // works: declarations are hoisted
function sayHi() {}

sayBye(); // TypeError: not a function (var hoisted as undefined)
var sayBye = function () {};
```

At the start of a statement, `function` is read as a declaration. Wrap it in parentheses to make an expression (an IIFE):

```js
(function () {
  console.log("runs now");
})();
(() => console.log("arrow IIFE"))();
```

## Object literal vs block

```js
const fn = () => ({ ok: true }); // parentheses: returns an object
const bad = () => {
  ok: true;
}; // block with a label: returns undefined
```

## Semicolons and ASI

**Automatic Semicolon Insertion** adds `;` in some places. It can surprise you.

```js
function getUser() {
  return;
  {
    name: "Ada";
  } // ASI inserts ; after return: returns undefined
}

const a = 1;
const b = (2)[(a, b)].forEach(console.log); // parsed as: 2[a, b].forEach(...) (TypeError)
```

Pick one style and enforce it with Prettier/ESLint. If you skip semicolons, start risky lines with `;`.

## Comments

```js
// single line
/* multi
   line */
/** JSDoc: describes types and params for editors */
```

## Expression and statement pitfalls

| Pitfall                                 | Why it hurts            | Better                          |
| --------------------------------------- | ----------------------- | ------------------------------- |
| `return` followed by a newline          | ASI returns `undefined` | Keep the value on the same line |
| `=> { key: value }`                     | Parsed as a block       | `=> ({ key: value })`           |
| Statement where expression is needed    | SyntaxError             | Ternary, or extract a function  |
| Relying on ASI                          | Silent wrong parse      | Consistent semicolons           |
| Assignment in conditions (`if (a = b)`) | Usually a typo          | `===`                           |

## Key takeaways

- Expressions produce values; statements do things
- Blocks scope `let` and `const`
- Wrap object literals in parentheses when returning them from arrows
- Use a formatter so ASI never bites

**Next:** [Conditionals](./06_conditionals.md)
