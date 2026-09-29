# Runtimes: Browser and Node.js

A **runtime** is an engine plus the APIs of its host. The same language behaves the same, but the available globals differ.

```
Browser  = V8/SpiderMonkey/JSC + DOM + fetch + storage + Web APIs
Node.js  = V8 + fs + http + process + streams + npm ecosystem
```

## Comparison

|                  | Browser                     | Node.js                    |
| ---------------- | --------------------------- | -------------------------- |
| Global object    | `window` / `globalThis`     | `global` / `globalThis`    |
| DOM (`document`) | Yes                         | No                         |
| File system      | No (sandboxed)              | Yes (`node:fs`)            |
| Modules          | ES modules                  | ES modules and CommonJS    |
| Main use         | UIs, web apps               | servers, CLIs, build tools |
| Security model   | sandbox, same-origin policy | full OS access             |

`globalThis` works in **every** runtime.

## Running code in the browser

```html
<script type="module" src="./app.js"></script>
```

Or type directly into the DevTools **Console**. Use `type="module"` for modern code (strict mode and `import`/`export` by default).

## Running code in Node

```bash
node app.js          # run a file
node                 # open the REPL
node -e "console.log(1 + 1)"
node --watch app.js  # restart on changes
```

```js
// app.mjs  (or set "type": "module" in package.json)
import { readFile } from "node:fs/promises";

const text = await readFile("./notes.txt", "utf8");
console.log(text.length);
```

## Runtime-specific globals

| Available in | Examples                                                                      |
| ------------ | ----------------------------------------------------------------------------- |
| Both         | `console`, `setTimeout`, `fetch`, `URL`, `AbortController`, `structuredClone` |
| Browser only | `document`, `window`, `localStorage`, `alert`                                 |
| Node only    | `process`, `Buffer`, `require` (CommonJS), `__dirname` (CommonJS)             |

## Other runtimes

| Runtime                           | Notes                                                           |
| --------------------------------- | --------------------------------------------------------------- |
| **Deno**                          | Secure by default, built-in TypeScript, permissions flags       |
| **Bun**                           | Fast startup, built-in bundler, test runner and package manager |
| Cloudflare Workers, edge runtimes | Browser-like APIs, no file system                               |

## Managing Node versions

Use a version manager so projects can pin their own version.

```bash
nvm install 22
nvm use 22
```

Alternatives: `fnm`, `volta`. Add an `.nvmrc` or `"engines"` field to document the required version.

## Detecting the environment

```js
const isBrowser =
  typeof window !== "undefined" && typeof document !== "undefined";
const isNode = typeof process !== "undefined" && process.versions?.node != null;
```

## Runtime pitfalls

| Pitfall                               | Why it hurts          | Better                                   |
| ------------------------------------- | --------------------- | ---------------------------------------- |
| Using `document` in Node              | `ReferenceError`      | Guard with a check or keep code separate |
| Mixing `require` and `import`         | Module errors         | Pick ESM, set `"type": "module"`         |
| Unpinned Node version                 | "Works on my machine" | `.nvmrc` and `engines`                   |
| Assuming browser support for new APIs | Runtime failures      | Check MDN compatibility tables           |

## Key takeaways

- Language is shared; APIs depend on the runtime
- Use `globalThis` for portable global access
- Prefer ES modules in both browser and Node
- Pin your Node version per project

**Next:** [npm and Package Managers](./03_npm-and-package-managers.md)
