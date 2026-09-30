# this

`this` is a special value available inside functions. It refers to **the object a function is working on**, and it is decided by **how the function is called**, not where it is written. (Arrow functions are the exception.)

```
obj.method()   ─────►  this = obj
method()       ─────►  this = undefined (strict) / globalThis (sloppy)
```

## The binding rules (in priority order)

| # | Rule | Call form | `this` |
|---|------|-----------|--------|
| 1 | **new** binding | `new Fn()` | the newly created object |
| 2 | **Explicit** binding | `fn.call(x)`, `fn.apply(x)`, `fn.bind(x)()` | `x` |
| 3 | **Implicit** binding | `obj.fn()` | `obj` (the object left of the dot) |
| 4 | **Default** binding | `fn()` | `undefined` in strict mode, `globalThis` in sloppy mode |
| - | **Arrow functions** | anything | `this` of the enclosing scope (lexical) |

## Implicit binding

```js
const user = {
  name: "Ada",
  greet() { return `Hi, ${this.name}`; },
};

user.greet();      // "Hi, Ada"  (this = user)
```

Only the **immediate** object counts:

```js
const app = { user };
app.user.greet();  // this = user, not app
```

## Losing `this`

```js
const greet = user.greet;
greet();           // strict: TypeError. sloppy: this = globalThis, name is undefined

setTimeout(user.greet, 0);          // this lost
button.addEventListener("click", user.greet);   // this = the button (DOM sets it)

const { greet: g } = user;          // destructuring detaches too
g();
```

Fixes:

```js
setTimeout(() => user.greet(), 0);         // wrapper arrow
setTimeout(user.greet.bind(user), 0);      // bind
```

## Default binding

```js
function show() { return this; }

show();   // sloppy: globalThis   |   strict / modules: undefined
```

Because modules and classes are strict, `this` is `undefined` in detached calls, which fails loudly.

## Explicit binding

```js
function intro(greeting) { return `${greeting}, ${this.name}`; }
intro.call({ name: "Ada" }, "Hello");    // "Hello, Ada"
```

Details in the next file.

## `new` binding

```js
function Person(name) { this.name = name; }
const p = new Person("Ada");   // this = the new object
```

## Arrow functions

Arrows have **no own `this`**. They capture it when created.

```js
class Timer {
  seconds = 0;
  start() {
    setInterval(() => { this.seconds++; }, 1000);   // this = the Timer instance
  }
}
```

Arrows ignore `call`, `apply` and `bind` for `this`:

```js
const arrow = () => this;
arrow.call({ x: 1 });   // still the outer this
```

## Class methods and `this`

Methods are **not** bound automatically.

```js
class Counter {
  count = 0;
  inc() { this.count++; }
  incBound = () => { this.count++; };   // class field arrow: bound per instance
}

const c = new Counter();
const { inc, incBound } = c;
inc();        // TypeError
incBound();   // works
```

Trade-off: per-instance arrow fields cost memory (one function per instance) and cannot be overridden via the prototype.

## `this` at the top level

| Where | `this` |
|-------|--------|
| Browser classic script | `window` |
| ES module | `undefined` |
| Node CommonJS file | `module.exports` |
| Node ES module | `undefined` |
| Inside a function (strict) | `undefined` unless bound |

## `this` in callbacks and array methods

```js
[1, 2].forEach(function () { console.log(this); }, { tag: "ctx" });   // thisArg
[1, 2].forEach(() => console.log(this));                              // outer this
```

## DOM and event handlers

```js
el.addEventListener("click", function () { this === el; });        // true
el.addEventListener("click", () => { /* this is NOT el */ });
el.addEventListener("click", (event) => event.currentTarget);      // reliable alternative
```

## Method chaining

```js
const calc = {
  value: 0,
  add(n) { this.value += n; return this; },
  mul(n) { this.value *= n; return this; },
};
calc.add(2).mul(5).value;   // 10
```

## Quick decision flow

1. Called with `new`? → the new object
2. Called with `call` / `apply` / `bind`? → that object
3. Called as `obj.method()`? → `obj`
4. Arrow function? → outer `this`
5. Otherwise → `undefined` (strict) / `globalThis` (sloppy)

## this pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Passing methods as callbacks | `this` lost | `bind`, arrow wrapper |
| Arrow function as an object method | `this` is the outer scope | Method shorthand |
| Assuming `this` follows definition site | It follows the call site | Follow the four rules |
| Using `this` in nested regular functions | Default binding | Arrow function or `const self = this` |
| Relying on sloppy `this === window` | Breaks in modules | `globalThis` explicitly |
| Class field arrows everywhere | Memory per instance | Bind only where needed |

## Key takeaways

- `this` is determined at call time: `new` > explicit > implicit > default
- Arrow functions inherit `this` lexically and cannot be rebound
- Detached methods lose `this`; use `bind` or arrow wrappers
- Strict mode makes lost `this` fail loudly

**Next:** [call, apply, bind](./02_call-apply-bind.md)
