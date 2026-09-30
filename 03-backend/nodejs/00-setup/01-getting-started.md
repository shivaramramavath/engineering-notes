# Getting Started with Node.js

Installing Node, verifying it works, and running your first script.

---

## Install Node — via a version manager, not a single global install

You _can_ download an installer directly from [nodejs.org](https://nodejs.org), but the better default for real development is a **version manager**, which lets you install multiple Node versions and switch between them per project.

### nvm (Node Version Manager) — macOS/Linux

```bash
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.1/install.sh | bash
```

Restart your terminal, then:

```bash
nvm install 20        # install Node 20
nvm install --lts       # install the current Long-Term Support version
nvm use 20                # switch to it for this shell session
nvm alias default 20        # make it the default for new shells
```

### nvm-windows — Windows

Download the installer from [nvm-windows releases](https://github.com/coreybutler/nvm-windows/releases) — the Unix `nvm` script doesn't run natively on Windows, `nvm-windows` is a separate, compatible tool with the same core commands (`nvm install`, `nvm use`).

### fnm — a faster cross-platform alternative

```bash
curl -fsSL https://fnm.vercel.app/install | bash
fnm install 20
fnm use 20
```

Functionally similar to `nvm`, written in Rust, noticeably faster to switch versions — a common modern alternative worth knowing about even if `nvm` remains more widely used.

---

## Why a version manager, not a single install?

- Different projects often require different Node versions — a version manager lets you switch per project rather than fighting one global version
- Upgrading Node system-wide risks breaking an older project that hasn't been tested against the new version
- A project can pin its required version in an `.nvmrc` file:

```
20.11.0
```

```bash
nvm use    # reads .nvmrc in the current directory automatically
```

Committing `.nvmrc` to a repo means every teammate (and CI) can run one command to match the exact version the project expects.

---

## Verify the install

```bash
node --version
npm --version
```

```
v20.11.0
10.2.4
```

npm comes bundled with Node — installing Node via nvm/fnm installs a matching npm version automatically, no separate step needed.

---

## Running your first script

```js
// hello.js
console.log("Hello from Node.js!");
```

```bash
node hello.js
```

```
Hello from Node.js!
```

### The Node REPL

```bash
node
```

```
> const x = 5
undefined
> x * 2
10
> .exit
```

An interactive prompt (Read-Eval-Print Loop) for quickly testing snippets of JavaScript without creating a file — useful for a quick check, not for real development.

---

## Starting a real project

```bash
mkdir my-project
cd my-project
npm init -y
```

This creates a `package.json` — covered in full in `02-npm-and-package-json.md` — which is what turns a folder of scripts into an actual Node _project_ that npm, and every tool built on top of it, understands.

---

## Choosing an editor

Not required, but worth mentioning: **VS Code** is the overwhelmingly common choice for Node development, with strong built-in JavaScript/TypeScript support and a large extension ecosystem (ESLint, Prettier — see `04-npm-ecosystem/`). Any editor works, but most Node tutorials and tooling assume VS Code's conventions (like `.vscode/settings.json`) by default.

## You're set up when you can:

- [ ] Run `node --version` and `npm --version` and see version numbers
- [ ] Switch Node versions with `nvm use <version>` (or `fnm use`)
- [ ] Run a `.js` file with `node filename.js`
- [ ] Run `npm init -y` in a new folder and see a `package.json` appear

## Next

**`02-npm-and-package-json.md`** covers what that `package.json` file actually means, and the basics of npm you'll use constantly from here on.
