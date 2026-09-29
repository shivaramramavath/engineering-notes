# What is JavaScript

JavaScript is a **high-level, dynamically typed, multi-paradigm** language. It started in browsers and now runs almost everywhere: servers, phones, desktops, embedded devices.

```
Specification (ECMAScript)  ─────►  Engine (V8, SpiderMonkey, JavaScriptCore)  ─────►  Runtime (Browser, Node, Deno, Bun)
     the rules                          parses and executes code                       engine + APIs (DOM, fs, fetch)
```

## JavaScript vs ECMAScript

| Term                | Meaning                                                    |
| ------------------- | ---------------------------------------------------------- |
| **ECMAScript (ES)** | The language specification, written by Ecma TC39           |
| **JavaScript**      | The most common implementation of ECMAScript               |
| **Engine**          | Program that runs JS (parser, compiler, garbage collector) |
| **Runtime**         | Engine plus host APIs (timers, network, DOM, file system)  |

`document`, `window`, `fetch`, `setTimeout` and `fs` are **not** part of the language. The host provides them.

## Engines

| Engine             | Used by                     |
| ------------------ | --------------------------- |
| **V8**             | Chrome, Edge, Node.js, Deno |
| **SpiderMonkey**   | Firefox                     |
| **JavaScriptCore** | Safari, Bun                 |

Modern engines use **JIT compilation**: code starts in an interpreter and hot paths get compiled to machine code.

## Language traits

- **Dynamic typing**: values have types, variables do not
- **First-class functions**: functions are values you can pass around
- **Prototype-based objects** with a `class` syntax on top
- **Single-threaded** main execution with an **event loop** for async work
- **Garbage collected** memory
- **Multi-paradigm**: procedural, object-oriented and functional styles all work

## Version history

| Version          | Year | Highlights                                                                    |
| :--------------- | :--- | :---------------------------------------------------------------------------- |
| ES5              | 2009 | strict mode, JSON, `Array.prototype.map/filter`                               |
| **ES2015 (ES6)** | 2015 | `let`/`const`, arrow functions, classes, modules, promises, template literals |
| ES2016           | 2016 | `**`, `Array.prototype.includes`                                              |
| ES2017           | 2017 | `async`/`await`, `Object.entries`                                             |
| ES2018           | 2018 | object spread, async iteration                                                |
| ES2019           | 2019 | `flat`, `flatMap`, `Object.fromEntries`                                       |
| ES2020           | 2020 | `?.`, `??`, `BigInt`, dynamic `import()`                                      |
| ES2021           | 2021 | `??=`, `\|=`, `&&=`, `Promise.any`, `replaceAll`                              |
| ES2022           | 2022 | private fields, `at()`, top-level `await`, `Object.hasOwn`                    |
| ES2023           | 2023 | `toSorted`, `toReversed`, `findLast`                                          |
| ES2024           | 2024 | `Object.groupBy`, `Promise.withResolvers`                                     |

Later editions keep arriving yearly. Check MDN and caniuse for support before using very new features.

## How features get added (TC39 stages)

| Stage | Meaning                                    |
| ----- | ------------------------------------------ |
| 0     | Idea                                       |
| 1     | Proposal, problem defined                  |
| 2     | Draft syntax                               |
| 3     | Candidate, waiting for implementations     |
| 4     | Finished, ships in the next yearly edition |

## Compatibility

- **Transpilers** (Babel, TypeScript, SWC) rewrite new syntax for older targets
- **Polyfills** add missing runtime APIs (for example `Array.prototype.at`)
- **Browserslist** or `target` settings tell your tools which environments to support

## Your first program

```js
console.log("Hello, JavaScript");
console.log(typeof null, typeof undefined); // "object" "undefined"
```

## Common misunderstandings

| Myth                             | Reality                                         |
| -------------------------------- | ----------------------------------------------- |
| JavaScript is Java               | Unrelated languages, similar name for marketing |
| JavaScript only runs in browsers | Node, Deno, Bun and others run it outside       |
| JS is interpreted only           | Engines JIT-compile hot code                    |
| `setTimeout` is part of JS       | It is a host API                                |

## Key takeaways

- ECMAScript is the spec, JavaScript is the implementation, engines execute it
- Runtimes add APIs such as the DOM or the file system
- ES2015 was the big modernization; new editions arrive every year
- Features go through TC39 stages before shipping

**Next:** [Runtimes: Browser and Node](./02_runtimes-browser-and-node.md)
