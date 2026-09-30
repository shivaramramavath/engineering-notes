# call, apply, bind

Every regular function has three methods (from `Function.prototype`) for controlling `this` explicitly.

| Method | Calls now? | Arguments | Returns |
|--------|-----------|-----------|---------|
| `fn.call(thisArg, a, b)` | Yes | listed | the function's result |
| `fn.apply(thisArg, [a, b])` | Yes | array (or array-like) | the function's result |
| `fn.bind(thisArg, a)` | **No** | preset (partial) | a new bound function |

```js
function intro(greeting, punctuation) {
  return `${greeting}, I am ${this.name}${punctuation}`;
}
const ada = { name: "Ada" };

intro.call(ada, "Hi", "!");        // "Hi, I am Ada!"
intro.apply(ada, ["Hi", "!"]);     // "Hi, I am Ada!"
const bound = intro.bind(ada, "Hi");
bound("?");                        // "Hi, I am Ada?"
```

Mnemonic: **A**pply takes an **A**rray, **C**all takes **C**ommas, **B**ind gives you a **B**ound function for later.

## `call`: borrowing methods

```js
const arrayLike = { 0: "a", 1: "b", length: 2 };
Array.prototype.map.call(arrayLike, (x) => x.toUpperCase());   // ["A", "B"]

Object.prototype.toString.call([]);      // "[object Array]"
Object.prototype.hasOwnProperty.call(obj, "x");   // safe own-property check
```

## `apply`: arrays as arguments

```js
Math.max.apply(null, [3, 9, 4]);   // 9
Math.max(...[3, 9, 4]);            // modern spread (preferred)
```

## `bind`: permanent `this` and partial application

```js
const user = {
  name: "Ada",
  greet() { return `Hi, ${this.name}`; },
};

const greet = user.greet.bind(user);
greet();                       // "Hi, Ada", even when detached

setTimeout(user.greet.bind(user), 100);
```

Partial application:

```js
const add = (a, b) => a + b;
const add10 = add.bind(null, 10);
add10(5);   // 15
```

Rules for `bind`:

- The bound `this` **cannot be overridden** by later `call`/`apply`/`bind`
- `new` on a bound function ignores the bound `this` (but keeps preset args)
- `boundFn.name` becomes `"bound originalName"`
- Arrow functions ignore the `this` you bind

## What `thisArg` becomes

| `thisArg` given | Sloppy mode | Strict mode |
|-----------------|-------------|-------------|
| object | that object | that object |
| primitive (`5`) | boxed (`Number` object) | stays `5` |
| `null` / `undefined` | `globalThis` | stays `null` / `undefined` |

## Writing your own `bind` (learning version)

```js
Function.prototype.myBind = function (context, ...preset) {
  const fn = this;
  return function bound(...args) {
    return fn.apply(context, [...preset, ...args]);
  };
};
```

## Own `call` idea

```js
Function.prototype.myCall = function (context, ...args) {
  context = context ?? globalThis;
  const key = Symbol();
  context[key] = this;
  const result = context[key](...args);
  delete context[key];
  return result;
};
```

## Practical patterns

```js
// Reusing a method on different objects
const speak = function () { return `${this.name} says ${this.sound}`; };
speak.call({ name: "Rex", sound: "woof" });

// Constructor chaining (legacy inheritance)
function Animal(name) { this.name = name; }
function Dog(name) { Animal.call(this, name); }

// Event handler with bound context
this.onClick = this.onClick.bind(this);

// Fixing arguments length
const first = (fn) => (x) => fn(x);   // guards against extra args
["1", "2"].map(first(parseInt));      // [1, 2]
```

## Alternatives

| Need | Modern option |
|------|---------------|
| Keep `this` in a callback | arrow function |
| Pass array as arguments | spread `fn(...args)` |
| Partial application | closure or arrow: `(x) => add(10, x)` |
| Borrow array methods | `Array.from(arrayLike)` |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `bind` inside `render` or loops | New function each time | Bind once (constructor) |
| Expecting `call` on arrows to change `this` | No effect | Regular function |
| Forgetting `bind` does not call | Function never runs | Call the result |
| Double `bind` | First one wins | Bind once |
| `apply` with huge arrays | Argument limit / stack overflow | Loop or `reduce` |
| `call(null)` in sloppy mode | `this` becomes global | Strict mode |

## Key takeaways

- `call` and `apply` invoke immediately; `bind` returns a new function
- `bind` fixes `this` permanently and can preset arguments
- Arrows ignore all three for `this`
- Modern code usually prefers arrows, spread and closures

**Next:** [Prototypes and the Prototype Chain](./03_prototypes-and-prototype-chain.md)
