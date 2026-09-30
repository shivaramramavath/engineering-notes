# Closures

A **closure** is a function together with a reference to the **lexical environment** in which it was defined. The function keeps access to those variables even after the outer function has finished running.

```
function + its surrounding variables  =  closure
```

## The simplest closure

```js
function makeCounter() {
  let count = 0;               // lives in makeCounter's environment

  return function increment() {
    count++;                   // still reachable after makeCounter returns
    return count;
  };
}

const counter = makeCounter();
counter();   // 1
counter();   // 2
counter();   // 3

const other = makeCounter();   // separate environment, separate count
other();     // 1
```

`makeCounter()` finished long ago, but `count` survives because `increment` still references it.

## How it works

1. Each function **call** creates a new environment (its variables)
2. A function created inside remembers the environment it was born in (`[[Environment]]`)
3. When called later, name lookup walks: own scope → remembered outer scope → ... → global
4. The engine keeps a captured environment alive as long as some function can still reach it

```
counter (function object)
   └─[[Environment]]─► { count: 3 }  ─outer─►  global
```

Every function in JavaScript is technically a closure. The term matters when a function is used **outside** the scope where it was created.

## Closures capture variables, not values

The function sees the **current** value of the variable, not a snapshot.

```js
function demo() {
  let message = "first";
  const show = () => console.log(message);
  message = "second";
  return show;
}
demo()();   // "second"
```

## Sharing an environment

Functions created in the same call share the same variables.

```js
function createAccount(initial) {
  let balance = initial;

  return {
    deposit(n)  { balance += n; return balance; },
    withdraw(n) { balance -= n; return balance; },
    get balance() { return balance; },
  };
}

const acc = createAccount(100);
acc.deposit(50);    // 150
acc.withdraw(20);   // 130
acc.balance;        // 130
acc.balance = 0;    // ignored, no setter; the real variable is unreachable
```

## Closures over parameters

Parameters are variables too.

```js
const multiplier = (factor) => (n) => n * factor;

const double = multiplier(2);
const triple = multiplier(3);
double(5);   // 10
triple(5);   // 15
```

## Nested closures

```js
function a() {
  const x = 1;
  return function b() {
    const y = 2;
    return function c() {
      return x + y;     // reaches both outer scopes
    };
  };
}
a()()();   // 3
```

## Closures in callbacks

```js
function delayedGreeting(name) {
  setTimeout(() => console.log(`Hello, ${name}`), 1000);   // name is remembered
}
delayedGreeting("Ada");

button.addEventListener("click", () => {
  clicks++;   // closure over an outer variable
});
```

Any callback that uses variables from where it was written is a closure.

## Seeing closures in DevTools

Set a breakpoint inside the inner function. The **Scope** panel shows a **Closure (makeCounter)** section listing captured variables. Engines capture only the variables actually referenced.

```js
function outer() {
  const used = 1, unused = 2;
  return () => used;    // only `used` is captured (in most engines)
}
```

## Closures vs classes

| | Closure | Class with `#private` |
|---|---------|----------------------|
| Privacy | Natural, via scope | Language feature `#field` |
| Method sharing | New function per instance | Shared on prototype |
| `instanceof` | No | Yes |
| `this` issues | None | Possible |
| Memory per instance | Higher | Lower |

## What is NOT a closure

```js
const x = 1;
function show() { return x; }   // just global scope access
```

Technically it still is (global environment), but the "closure" behavior people care about shows when a function **outlives** its defining scope.

## Mental model checklist

- Where was the function **defined**? That decides what it can see
- Where is it **called**? Irrelevant for variable lookup (relevant for `this`)
- Each call of the outer function creates a **new independent** environment
- Variables are captured **by reference**

## Common questions

| Question | Answer |
|----------|--------|
| Do closures copy variables? | No, they reference them |
| Can two closures share state? | Yes, when created in the same outer call |
| Are closures slow? | Slight memory cost; rarely matters |
| Do arrows create closures? | Yes, same as regular functions |
| Can a closure change captured variables? | Yes, if they are not `const` |
| When is captured memory freed? | When no reachable function references the environment |

## Pitfalls (quick list, details in file 3)

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `var` in loops with callbacks | All closures share one variable | `let` |
| Capturing large objects in long-lived callbacks | Memory retained | Capture only needed values |
| Expecting a snapshot | Later mutations show through | Copy the value into a new variable |
| Forgetting each outer call is separate | Confusion about shared state | Draw the environments |

## Key takeaways

- A closure is a function plus the environment where it was created
- It captures **variables by reference** and keeps them alive
- Each outer call creates a fresh, independent environment
- Closures give you private state without classes

**Next:** [Closure Use Cases](./02_closure-use-cases.md)
