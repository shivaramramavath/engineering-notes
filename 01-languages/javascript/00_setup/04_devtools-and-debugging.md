# DevTools and Debugging

Debugging is finding **why** the code does what it does. Reading errors and pausing execution beats guessing.

## The console API

| Method                               | Use                                  |
| ------------------------------------ | ------------------------------------ |
| `console.log` / `info`               | general output                       |
| `console.warn` / `error`             | highlighted messages                 |
| `console.table(data)`                | arrays and objects as a table        |
| `console.dir(obj, { depth: null })`  | full object tree                     |
| `console.group` / `groupEnd`         | indent related logs                  |
| `console.time("x")` / `timeEnd("x")` | measure duration                     |
| `console.count("x")`                 | count calls                          |
| `console.assert(cond, msg)`          | log only when the condition is false |
| `console.trace()`                    | print the call stack                 |

```js
console.table([
  { id: 1, name: "Ada" },
  { id: 2, name: "Grace" },
]);
console.log({ user }); // label values by wrapping in an object
```

## Browser DevTools panels

| Panel           | What it shows                       |
| --------------- | ----------------------------------- |
| **Elements**    | live DOM and CSS                    |
| **Console**     | logs, errors, run code              |
| **Sources**     | files, breakpoints, stepping        |
| **Network**     | requests, timing, headers, payloads |
| **Performance** | flame charts, long tasks            |
| **Memory**      | heap snapshots, leaks               |
| **Application** | storage, cookies, service workers   |

Open with `F12` or `Ctrl/Cmd + Shift + I`.

## Breakpoints

| Type        | How                                         |
| ----------- | ------------------------------------------- |
| Line        | click a line number in Sources              |
| Conditional | right click, "Add conditional breakpoint"   |
| Logpoint    | logs a value without editing code           |
| `debugger;` | statement that pauses when DevTools is open |
| Exception   | pause on caught or uncaught errors          |
| DOM / event | pause on node change or event type          |

```js
function total(items) {
  debugger; // execution pauses here with DevTools open
  return items.reduce((sum, i) => sum + i.price, 0);
}
```

## Stepping controls

| Action    | Meaning                                 |
| --------- | --------------------------------------- |
| Resume    | run to the next breakpoint              |
| Step over | run the current line, skip inside calls |
| Step into | enter the called function               |
| Step out  | finish the current function             |

Watch the **Scope** panel (variables), **Call Stack**, and add **Watch** expressions.

## Debugging Node

```bash
node --inspect app.js          # then open chrome://inspect
node --inspect-brk app.js      # pause on the first line
```

In VS Code use the **Run and Debug** panel with a launch configuration, or the built-in JavaScript Debug Terminal.

## Reading an error

```
TypeError: Cannot read properties of undefined (reading 'name')
    at getName (app.js:12:20)
    at main (app.js:20:5)
```

1. **Type**: `TypeError`, `ReferenceError`, `SyntaxError`, ...
2. **Message**: what went wrong
3. **Stack**: read from the top; the first line in **your** code is usually the place to look

## Common errors

| Error                                            | Usual cause                              |
| ------------------------------------------------ | ---------------------------------------- |
| `ReferenceError: x is not defined`               | typo, or variable out of scope           |
| `TypeError: x is not a function`                 | wrong type, or method missing            |
| `TypeError: Cannot read properties of undefined` | a value is missing upstream              |
| `SyntaxError`                                    | invalid code, missing bracket            |
| `RangeError`                                     | invalid array length, infinite recursion |

## Source maps

Bundlers and transpilers output code that differs from what you wrote. **Source maps** map it back so breakpoints and stack traces point to your original files. Keep them enabled in development.

## Debugging method

1. Reproduce the bug reliably
2. Read the full error and stack trace
3. Narrow down: comment out, bisect, minimal example
4. Inspect actual values at a breakpoint
5. Fix, then add a test so it stays fixed

## Debugging pitfalls

| Pitfall                           | Why it hurts                | Better                                     |
| --------------------------------- | --------------------------- | ------------------------------------------ |
| Only `console.log` everywhere     | Slow, cluttered             | Breakpoints and logpoints                  |
| Logging objects that mutate later | Console shows current state | `structuredClone(obj)` or `JSON.stringify` |
| Ignoring the first stack line     | Miss the real location      | Read the stack top down                    |
| Leaving `debugger;` in code       | Pauses users' browsers      | Lint rule `no-debugger`                    |
| Changing many things at once      | Unknown fix                 | One change at a time                       |

## Key takeaways

- Read errors: type, message, then the stack
- Breakpoints and stepping beat print debugging for complex flow
- Node debugs with `--inspect`, the same tools as Chrome
- Source maps connect built code back to your source

**Next:** [Tooling](./05_tooling.md)
