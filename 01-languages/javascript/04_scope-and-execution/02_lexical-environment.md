# Lexical Environment

A **Lexical Environment** is the specification's model for scope. Understanding it explains variable lookup, closures and hoisting.

```
Lexical Environment = Environment Record (the names and values)
                    + Outer reference (link to the parent environment)
```

## The scope chain

When you use a name, the engine looks in the **current** environment, then follows the **outer** link until it finds the name or reaches the global environment.

```js
const a = "global";

function outer() {
  const b = "outer";

  function inner() {
    const c = "inner";
    console.log(a, b, c);   // finds c, then b, then a
  }
  inner();
}
outer();
```

```
inner env  { c }     ─outer─►
outer env  { b, inner } ─outer─►
global env { a, outer } ─outer─► null
```

If the name is not found anywhere: `ReferenceError` (for reads) or an implicit global (sloppy-mode writes).

## Environment records

| Record type | Holds |
|-------------|-------|
| Declarative | `let`, `const`, `class`, function parameters and `var` inside functions |
| Object | Global `var` and function declarations, mirrored on the global object (`window`) |
| Function | Declarative record plus `this` binding and `new.target` |
| Module | Import bindings (live) plus module-level declarations |

## When environments are created

| Event | New environment |
|-------|-----------------|
| Script starts | global |
| Module evaluates | module |
| Function is **called** | function environment |
| Block `{ }` is entered (with `let`/`const`/`class`) | block environment |
| `catch` runs | catch environment |
| `for (let ...)` iteration | per-iteration environment |

## Function objects remember their birthplace

When a function is **created**, it stores a hidden reference (`[[Environment]]`) to the environment where it was defined. When it is **called**, a new environment is created whose outer link points to that stored environment.

```js
function makeAdder(x) {
  return function (y) { return x + y; };   // remembers the makeAdder environment
}
const add5 = makeAdder(5);
add5(3);   // 8
```

That stored link is what a **closure** is.

## Lexical vs dynamic scope

```js
const level = "global";
function show() { console.log(level); }

function test() {
  const level = "test";
  show();   // JavaScript: "global"  (lexical). A dynamic-scope language would print "test".
}
```

`this` is the exception: it is **dynamic** (depends on the call), except in arrow functions where it is lexical.

## Variable lookup cost

Engines resolve most names at compile time and optimize them to direct slots. Deep chains are not a problem in practice, but heavy use of `eval` or `with` disables these optimizations.

## Closures keep environments alive

```js
function big() {
  const data = new Array(1e6).fill("x");
  return () => data.length;    // data stays in memory while this function lives
}
const size = big();
```

Variables referenced by a live closure cannot be garbage collected. See `18_memory-and-garbage-collection/03_memory-leaks.md`.

## Inspecting the chain

DevTools **Scope** panel (in Sources while paused) shows: **Local**, **Closure**, **Script**, **Global**. That is the scope chain.

```js
function outer() {
  const secret = 1;
  return function inner() {
    debugger;               // check the Closure section in DevTools
    return secret;
  };
}
outer()();
```

## Global environment details

| Declared with | Where it lives (classic script) |
|---------------|--------------------------------|
| `var`, function declaration | Object record: property on `globalThis` |
| `let`, `const`, `class` | Declarative record: **not** on `globalThis` |
| Implicit global (sloppy) | Property on `globalThis`, deletable |

## Lexical environment pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Assuming scope depends on call site | Wrong mental model | Read where the code is written |
| Big data captured by long-lived closures | Memory retention | Null it out or narrow the capture |
| Expecting `let`/`const` on `window` | They are not properties | Use module exports |
| Assuming `this` follows lexical rules | It is call-based (except arrows) | See `05_this-and-oop/01_this.md` |
| Creating closures in hot loops | Extra allocations | Hoist the function out |

## Key takeaways

- Scope is modeled as environment records linked by outer references
- Name lookup walks outward along that chain
- A function remembers the environment where it was created: that is a closure
- Every call, block, and module gets its own environment
- `this` is dynamic; ordinary variables are lexical

**Next:** [Execution Context and Call Stack](./03_execution-context-and-call-stack.md)
