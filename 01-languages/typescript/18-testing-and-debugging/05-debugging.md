# Debugging

Debugging TypeScript is debugging JavaScript, with one extra layer: the code that runs is **not** the code you wrote. Types are erased and syntax may be rewritten, so a debugger needs **source maps** to show you your `.ts` lines, variable names, and stack traces. This note covers getting source maps working, stepping through code in VS Code and the browser, attaching to tests, practical logging, and a method for hunting bugs that is not just guessing.

**Prerequisites:**
- [Type erasure and runtime](../14-type-system-internals/00-type-erasure-and-runtime.md)
- [Compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)
- [Catching and narrowing errors](../11-error-handling/00-catching-and-narrowing-errors.md)

---

## Source maps: connecting running code to your source

A **source map** (`.js.map`) records which line and column of the emitted JavaScript came from which line of your TypeScript. Debuggers and stack-trace printers use it to translate back.

```jsonc
// tsconfig.json
{
  "compilerOptions": {
    "sourceMap": true,          // emit .js.map files
    "inlineSources": true       // embed the TS source in the map (handy when deploying or bundling)
  }
}
```

Without source maps, breakpoints in `.ts` files never hit, and stack traces point at generated JavaScript. With them, you debug your real code.

### Readable stack traces in Node

Node does not apply source maps to error stack traces unless you ask:

```bash
node --enable-source-maps dist/index.js
```

Now an uncaught error shows `src/service.ts:42:11` instead of `dist/service.js:61:15`. Bundlers and runtimes that run TypeScript directly (such as `tsx`) usually handle this for you. If you use a bundler, make sure it emits maps (`sourcemap: true` or the equivalent) and keep them available where errors are collected.

## Debugging in VS Code

### Quickest: the JavaScript Debug Terminal

Open the command palette, run **"Debug: JavaScript Debug Terminal"**, and start your program in that terminal as usual:

```bash
npx tsx src/index.ts
npm run dev
npx vitest run
```

Any Node process started there is attached to the debugger automatically. Set breakpoints in `.ts` files, and they work when source maps are available (tools like `tsx` and Vitest provide them). This avoids hand-written launch configuration for most cases.

### A launch configuration for built output

If you compile with `tsc` and run `dist/`:

```jsonc
// .vscode/launch.json
{
  "version": "0.2.0",
  "configurations": [
    {
      "type": "node",
      "request": "launch",
      "name": "Run built app",
      "program": "${workspaceFolder}/dist/index.js",
      "preLaunchTask": "tsc: build - tsconfig.json",
      "sourceMaps": true,
      "outFiles": ["${workspaceFolder}/dist/**/*.js"]
    }
  ]
}
```

`outFiles` tells the debugger where compiled files live, so it can map them back. If breakpoints show as unbound (grey), the usual causes are: no source maps emitted, wrong `outFiles` pattern, or a stale build.

### What you can do once stopped

- **Breakpoints:** click the gutter. **Conditional breakpoints** (`user.id === "42"`) stop only when a condition is true. **Logpoints** print a message without editing code or stopping.
- **Step:** over, into, out. Step *into* a function to follow it, *out* to return to the caller.
- **Inspect:** hover variables, use the Variables panel, and evaluate expressions in the Debug Console. You are looking at **runtime values**, so you see the real shape of data, which is often what the types missed.
- **Call stack:** see how execution got here, including `async` frames in modern Node.
- **Exception breakpoints:** pause when an error is thrown (all, or only uncaught), right at the origin.

## Debugging with Node's inspector directly

```bash
node --inspect-brk dist/index.js         # pause on the first line, wait for a debugger
node --inspect dist/index.js             # run, allow a debugger to attach
```

Then attach from VS Code ("Attach" configuration) or open `chrome://inspect` in Chrome and click **inspect** on the target. Chrome DevTools gives you the same stepping, breakpoints, console, and also memory and CPU profiling ([runtime performance](../22-performance/02-runtime-performance.md)).

## Debugging tests

Debug a failing test at the line, instead of adding logs:

- **VS Code:** run the test command in the JavaScript Debug Terminal, or use the test extension's "Debug" action.
- **From the command line:** start the runner under the inspector. The flags vary by runner and version: for Vitest, run it with `--inspect-brk` (typically with a single worker or `--no-file-parallelism`), and attach. For Jest, run Node directly on the Jest binary with `--inspect-brk` and `--runInBand`.

Running tests in a **single process** (no parallel workers) is usually required for the debugger to attach to the code you care about ([test runners](./03-test-runners.md)).

## Debugging in the browser

Modern dev servers (Vite, Next.js, webpack dev mode) serve source maps, so DevTools shows your `.ts` and `.tsx` files in the Sources panel. Set breakpoints there. Other useful features:

- **The `debugger;` statement** pauses execution when DevTools is open. Remove it before committing (a lint rule can catch it).
- **Network panel:** inspect actual requests and responses, the best way to settle "what did the server really send?" ([request and response types](../16-type-safe-apis/01-request-response-types.md)).
- **Console and `$0`:** inspect the selected element, evaluate expressions in the page's context.
- **React DevTools:** inspect component props, state, and re-renders ([React and frontend](../19-react-and-frontend/README.md)).

Production builds usually ship minified code, so keep source maps available to your error-reporting tooling (uploaded privately, or served only to authorized users) rather than exposing them publicly by accident.

## Logging that helps

When a debugger is impractical (production, intermittent bugs, distributed systems), logs are what you have.

```ts
console.log("user", user);                        // quick look
console.dir(deepObject, { depth: null });         // print nested objects fully
console.table(rows);                              // arrays of objects as a table
console.trace("who called this?");                // print the current stack
console.time("query"); await run(); console.timeEnd("query");   // measure
```

Habits that make logs useful:

- **Log structured data** (objects with fields), not interpolated strings, so logs can be searched and filtered ([logging and observability](../21-production-tooling/06-logging-and-observability.md)).
- **Include identifiers:** request id, user id, operation, so you can follow one flow.
- **Log at boundaries:** inputs received, outputs sent, external calls and results.
- **Log the error object** (with its `cause` chain), not just `error.message` ([custom errors](../11-error-handling/01-custom-errors.md)).
- **Never log secrets or personal data** (tokens, passwords, full card numbers).
- **Remove or lower** temporary debug logs before committing.

A common trap: logging an object **and then mutating it** shows the mutated state in some consoles. Log a copy (`structuredClone(obj)` or `JSON.stringify`) when timing matters.

## A method for finding bugs

Random changes are slow. A systematic approach:

1. **Reproduce it reliably.** Get a failing case you can run on demand: a test, a script, a saved request. If you cannot reproduce it, you cannot know you fixed it.
2. **Shrink it.** Remove inputs, code, and configuration until the smallest example that still fails remains. The bug is often obvious by then.
3. **Form a hypothesis** about the cause, and decide what observation would confirm or refute it.
4. **Observe, do not assume.** Use a breakpoint or log to look at the *actual* value at the suspected point, because the bug is usually where reality differs from your belief.
5. **Bisect.** Halve the search space: comment out half the pipeline, check a value in the middle, or run `git bisect` to find the commit that introduced the failure.
6. **Fix the cause, not the symptom,** then **write a test** that fails without the fix.
7. **Look for siblings:** the same mistake elsewhere.

```bash
git bisect start
git bisect bad                 # the current commit is broken
git bisect good v1.4.0         # this older one worked
# git checks out a middle commit: run your repro, then mark it good or bad
git bisect good                # or: git bisect bad
git bisect reset               # when done
```

### When types and runtime disagree

If the type says `User` but the value at runtime is missing a field, the bug is upstream where the claim entered: an `as` assertion, an `any`, or unvalidated external data ([trust boundaries](../15-runtime-validation/00-trust-boundaries.md), [soundness and escape hatches](../14-type-system-internals/04-soundness-and-escape-hatches.md)). Search for those, then validate at the boundary.

### Debugging type errors and configuration

Compile-time problems have their own tools:

- Reading and untangling error messages: [reading type errors](./06-reading-type-errors.md).
- `tsc --showConfig`, `--listFiles`, `--explainFiles`, and `--traceResolution` for "why is this file or module or setting behaving this way" ([compiler options](../13-compiler-and-tsconfig/00-compiler-options.md)).
- The TypeScript Playground to isolate and share a minimal example.

### Async bugs

- **Missing `await`:** the value is a `Promise`, or the step ran out of order. The linter rule `no-floating-promises` finds most of them.
- **Unhandled rejections:** listen with `process.on("unhandledRejection", ...)` while developing to find the source.
- **Race conditions:** add timestamps and request ids to logs and look at the ordering, then use cancellation or "latest wins" guards ([concurrency patterns](../12-async-and-iteration/05-concurrency-patterns.md)).
- Async stack traces in modern Node include `await` frames, so name your functions rather than relying on anonymous arrows.

### Performance and memory

For slowness or growth over time, measure before guessing: CPU profiles (`node --cpu-prof`, DevTools Performance), heap snapshots (DevTools Memory), and event loop delay measurements show where time and memory go ([runtime performance](../22-performance/02-runtime-performance.md), [event loop](../12-async-and-iteration/00-event-loop.md)).

## Important rules and misconceptions

- **Source maps are not optional for debugging TypeScript.** No maps, no useful breakpoints or stack traces.
- **A breakpoint shows runtime truth.** Types are what you hoped. The debugger shows what is.
- **`console.log` debugging is fine.** It is a legitimate tool, best when combined with structured output and a reliable repro.
- **Stack traces are not source-mapped by default in Node.** Enable `--enable-source-maps` (or use a runner that does).
- **A bug you cannot reproduce is not fixed,** even if it stops appearing.
- **Shipping source maps publicly exposes your source.** Decide deliberately who can access them.

## Common mistakes

- Debugging generated JavaScript because source maps are off.
- Wrong `outFiles` or a stale build, leaving breakpoints unbound.
- Debugging tests with parallel workers, so the debugger attaches to the wrong process.
- Changing code at random instead of forming and testing a hypothesis.
- Leaving `debugger;` or noisy `console.log` calls in committed code.
- Logging secrets or full personal data.
- Trusting the type of a value instead of inspecting it.
- Fixing the symptom (a null check) without finding why the value was null.
- Not writing a regression test for a fixed bug.

## Debugging checklist when a breakpoint does not hit

- Is `sourceMap: true` set, and are `.map` files being emitted next to the output?
- Is the code you are running the freshly built output (not a stale `dist`)?
- Does `outFiles` / the debugger's source map path match where files actually are?
- Is the process attached (started under the debug terminal, or with `--inspect`)?
- For tests: single worker, correct config, and the file is actually executed?
- For bundled code: does the bundler emit source maps, and do they include the sources?

## Quick summary

- Debugging TypeScript needs **source maps**. Enable `sourceMap`, use `--enable-source-maps` for Node stack traces, and ensure your bundler emits maps.
- Use the **JavaScript Debug Terminal** or a launch config in VS Code, `--inspect` with DevTools, and the browser Sources panel for front-end code.
- Debug tests in a single process under the inspector.
- Log structured data with identifiers, never secrets, and prefer breakpoints and logpoints when you can reproduce the bug.
- Work systematically: reproduce, shrink, hypothesize, observe, bisect, fix the cause, add a regression test.
- When types and reality disagree, look for the assertion, `any`, or unvalidated input upstream.

**Next:** [Reading type errors](./06-reading-type-errors.md)
