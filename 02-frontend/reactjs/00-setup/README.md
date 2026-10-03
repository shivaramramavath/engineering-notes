# 00 — Setup

Everything you need before writing your first component: the JavaScript React assumes you know, a working toolchain, and an editor that catches mistakes as you type. Nothing here is React-specific theory — it's the groundwork that makes every later chapter smoother.

## What you'll be able to do after this chapter

- Read modern JavaScript (destructuring, spread, array methods, modules) without stumbling
- Install and manage Node.js and npm packages
- Create, run, and build a React + TypeScript project with Vite
- Explain what every file in a fresh project is for
- Inspect a running app with React DevTools
- Have TypeScript, ESLint, and Prettier working together in your editor

## Reading order

| #   | File                                                                       | What it covers                                      | Prerequisite     |
| --- | -------------------------------------------------------------------------- | --------------------------------------------------- | ---------------- |
| 00  | [00-javascript-for-react.md](./00-javascript-for-react.md)                 | The JS features React code uses constantly          | Basic JavaScript |
| 01  | [01-node-and-npm.md](./01-node-and-npm.md)                                 | Installing Node, `package.json`, scripts, lockfiles | None             |
| 02  | [02-vite.md](./02-vite.md)                                                 | Creating and running a project, config, build       | 01               |
| 03  | [03-project-structure.md](./03-project-structure.md)                       | What the starter template generates                 | 02               |
| 04  | [04-react-devtools.md](./04-react-devtools.md)                             | Inspecting components, state, and renders           | 02               |
| 05  | [05-typescript-and-linting-setup.md](./05-typescript-and-linting-setup.md) | `tsconfig`, ESLint, Prettier, editor setup          | 02               |

If you're already comfortable with modern JavaScript and Node, skim `00` and `01` and start at `02`.

## Exercises

1. Create a new Vite + React + TypeScript project, run the dev server, and change the text in `App.tsx` to see hot reload.
2. Delete everything inside `App.tsx` except a single `<h1>`, then remove the unused imports and assets.
3. Open React DevTools and find your `App` component in the Components tab.
4. Introduce a deliberate type error and an unused variable; confirm your editor flags both.

## Next

**[`../01-fundamentals/README.md`](../01-fundamentals/README.md)** starts with how to think about UIs as components, then moves into JSX, props, events, and lists.
