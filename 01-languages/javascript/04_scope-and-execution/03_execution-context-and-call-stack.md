# Execution Context and Call Stack

An **execution context** is the environment in which a piece of code is evaluated. The **call stack** tracks which contexts are active, since JavaScript runs one thing at a time on a single thread.

```
Call stack (top runs now)
┌──────────────────┐
│ inner()          │  ◄── currently executing
├──────────────────┤
│ outer()          │
├──────────────────┤
│ global / module  │
└──────────────────┘
```

## Types of execution context

| Type | Created when |
|------|--------------|
| Global | script starts |
| Module | module starts evaluating |
| Function | a function is called |
| `eval` | `eval` runs code (avoid) |

## What a context holds

| Component | Meaning |
|-----------|---------|
| Lexical environment | `let`, `const`, functions, parameters |
| Variable environment | `var` declarations |
| `this` binding | the value of `this` |
| Reference to outer environment | for the scope chain |

## Two phases

**1. Creation (memory) phase**

- Create the environment
- Register declarations: `var` set to `undefined`, function declarations fully set, `let`/`const`/`class` registered but uninitialized (TDZ)
- Determine `this`

**2. Execution phase**

- Run line by line, assign values, call functions

```js
console.log(a);      // undefined  (var registered in creation phase)
console.log(f());    // "hi"       (function declaration fully hoisted)
var a = 1;
function f() { return "hi"; }
```

## Walk-through

```js
function first() {
  console.log("first start");
  second();
  console.log("first end");
}
function second() {
  console.log("second");
}
first();
```

| Step | Stack (top → bottom) | Output |
|------|----------------------|--------|
| 1 | global | |
| 2 | first, global | `first start` |
| 3 | second, first, global | `second` |
| 4 | first, global (second returned) | `first end` |
| 5 | global (first returned) | |

## Stack overflow

The stack has limited size.

```js
function forever() { forever(); }
forever();   // RangeError: Maximum call stack size exceeded
```

Typical depth is around 10,000 frames, varying by engine and frame size.

Fixes: add a base case, convert to a loop, use an explicit stack, or a trampoline (`02_functions/08_recursion.md`).

## Reading a stack trace

```
Error: boom
    at c (app.js:3:9)
    at b (app.js:7:3)
    at a (app.js:11:3)
    at Object.<anonymous> (app.js:14:1)
```

The **top line** is where the error was created; each line below is its caller.

```js
console.trace("where am I?");   // prints the current stack without an error
```

Async stack traces in modern DevTools link across `await` and promises, but timers and event callbacks start a **fresh stack**.

## Synchronous code blocks the stack

```js
function blockFor(ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) {}     // nothing else can run: UI freezes
}
```

Long tasks stop rendering, clicks and timers. Split work, use `setTimeout`/`requestIdleCallback`, or a worker.

## Stack and the event loop

The stack runs **synchronous** code. Async callbacks wait in queues and only run when the stack is **empty**.

```js
console.log("A");
setTimeout(() => console.log("B"), 0);
Promise.resolve().then(() => console.log("C"));
console.log("D");
// A D C B
```

Details in `12_event-loop/01_event-loop.md`.

## `this` and the context

The `this` binding is set when the function context is created:

| Call form | `this` |
|-----------|--------|
| `fn()` | `undefined` in strict mode, global in sloppy |
| `obj.fn()` | `obj` |
| `new Fn()` | new object |
| `fn.call(x)` | `x` |
| arrow function | from the outer context |

## Context and memory

When a function returns, its context is popped. Its variables are freed **unless** a closure still references its environment.

## Execution context pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Unbounded recursion | Stack overflow | Base case, iteration, trampoline |
| Long synchronous loops | Freezes UI or server | Chunk work, workers |
| Assuming async stack equals sync stack | Traces can be shorter | Use async-aware DevTools |
| Confusing hoisting with code moving | It is the creation phase | Think "registered early" |
| Deep call chains in hot paths | Overhead | Flatten or inline |

## Key takeaways

- A context is created per script, module and function call
- Creation phase registers declarations; execution phase runs code
- The call stack is LIFO and single-threaded
- Async callbacks run only after the stack is empty
- Too many nested calls cause `RangeError`

**Next:** [Hoisting and TDZ](./04_hoisting-and-tdz.md)
