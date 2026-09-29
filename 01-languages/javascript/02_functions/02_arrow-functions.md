# Arrow Functions

Arrow functions (ES2015) are a shorter syntax with one big semantic difference: they **do not have their own `this`**.

```
(params) => expression        // implicit return
(params) => { statements }    // explicit return
```

## Syntax variations

```js
const one = () => 42; // no params
const two = (x) => x * 2; // single param, parentheses optional
const three = (a, b) => a + b; // multiple params
const four = (a, b) => {
  // block body needs return
  const sum = a + b;
  return sum;
};
const obj = () => ({ ok: true }); // returning an object literal needs ()
```

`() => { ok: true }` is a block with a label and returns `undefined`.

## Lexical `this`

Arrow functions capture `this` from the surrounding scope.

```js
class Timer {
  seconds = 0;

  start() {
    setInterval(() => {
      this.seconds++; // `this` is the Timer instance
    }, 1000);
  }
}
```

With a regular function, `this` inside the callback would not be the instance.

## Arrow vs regular functions

| Feature                               | Regular               | Arrow                   |
| ------------------------------------- | --------------------- | ----------------------- |
| Own `this`                            | Yes (depends on call) | No, lexical             |
| Own `arguments`                       | Yes                   | No (use rest `...args`) |
| Usable with `new`                     | Yes                   | No (`TypeError`)        |
| Has `prototype`                       | Yes                   | No                      |
| Can be a generator                    | Yes (`function*`)     | No                      |
| `call`, `apply`, `bind` change `this` | Yes                   | No effect on `this`     |
| Method shorthand / object methods     | Good                  | Usually a bad fit       |

## When to use arrows

- Short callbacks: `items.map(x => x.id)`
- Preserving `this` inside methods (timers, event handlers in classes)
- Small pure helpers and inline transformations
- Returning functions from functions (currying)

## When NOT to use arrows

```js
const user = {
  name: "Ada",
  bad: () => this.name, // `this` is NOT user (outer scope)
  good() {
    return this.name;
  }, // regular method
};

const Person = (n) => {
  this.n = n;
};
new Person("x"); // TypeError: Person is not a constructor
```

Also avoid arrows for DOM handlers that rely on `this` being the element, and for prototype methods.

## No `arguments`

```js
const f = () => arguments; // ReferenceError in modules (or outer function's arguments)
const g = (...args) => args; // use rest parameters
```

## Chaining and readability

```js
const total = orders
  .filter((o) => o.paid)
  .map((o) => o.amount)
  .reduce((sum, n) => sum + n, 0);
```

If a body needs several lines, use a block and a name:

```js
const isAdult = (person) => person.age >= 18;
people.filter(isAdult);
```

## Arrow pitfalls

| Pitfall                                   | Why it hurts        | Better                                 |
| ----------------------------------------- | ------------------- | -------------------------------------- |
| Arrow as object method                    | Wrong `this`        | Method shorthand                       |
| Missing parentheses around object literal | Returns `undefined` | `() => ({ ... })`                      |
| Using `arguments`                         | Not available       | `...args`                              |
| Long one-liners with nested ternaries     | Unreadable          | Block body or named function           |
| Anonymous arrows in stack traces          | Harder debugging    | Assign to a `const` (name is inferred) |

## Key takeaways

- Arrows have lexical `this`, no `arguments`, and cannot be constructors
- Use them for callbacks and short helpers
- Use regular functions or method shorthand for object methods and constructors
- Wrap returned object literals in parentheses

**Next:** [Parameters and Arguments](./03_parameters-and-arguments.md)
