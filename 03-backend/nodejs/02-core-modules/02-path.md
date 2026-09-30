# `path` — File Path Handling

Node's built-in module for building and manipulating file paths in a way that works correctly across operating systems. Almost always used alongside `fs`.

```js
import path from "node:path";
```

## Why not just use string concatenation?

```js
// ❌ breaks on Windows (uses backslashes) and is fragile in general
const filePath = folder + "/" + filename;
```

Windows uses `\` as its path separator; macOS/Linux use `/`. Manually building paths with string concatenation works until it doesn't — `path` handles this transparently.

```js
// ✅ correct on every OS
const filePath = path.join(folder, filename);
```

---

## `path.join()` — the one you'll use constantly

```js
path.join("/app", "uploads", "photo.png");
// → "/app/uploads/photo.png" (or "\app\uploads\photo.png" on Windows)
```

Joins path segments using the correct separator for the current OS, and normalizes the result (collapsing redundant slashes, resolving `..` and `.`):

```js
path.join("/app", "uploads/", "../logs", "app.log");
// → "/app/logs/app.log"
```

---

## `path.resolve()` — get an absolute path

```js
path.resolve("uploads", "photo.png");
// → "/current/working/directory/uploads/photo.png"
```

Unlike `join`, `resolve` builds an **absolute** path, treating the arguments as if you `cd`'d through them from the current working directory (or from an initial absolute segment, if one is given):

```js
path.resolve("/app", "uploads"); // → "/app/uploads"
path.resolve("app", "uploads"); // → "<cwd>/app/uploads"
```

### `join` vs `resolve`

|                     | Result                                                         | Use when                                                         |
| ------------------- | -------------------------------------------------------------- | ---------------------------------------------------------------- |
| `path.join(...)`    | Combines segments, keeps them relative if inputs were relative | You just need to combine path pieces                             |
| `path.resolve(...)` | Always returns an absolute path                                | You need a guaranteed absolute path, e.g. before passing to `fs` |

---

## Extracting parts of a path

```js
const filePath = "/app/uploads/photo.png";

path.basename(filePath); // "photo.png"
path.basename(filePath, ".png"); // "photo" (extension stripped)
path.dirname(filePath); // "/app/uploads"
path.extname(filePath); // ".png"
```

### `path.parse()` — everything at once

```js
path.parse("/app/uploads/photo.png");
```

```js
{
  root: "/",
  dir: "/app/uploads",
  base: "photo.png",
  ext: ".png",
  name: "photo"
}
```

### `path.format()` — the reverse of `parse`

```js
path.format({ dir: "/app/uploads", name: "photo", ext: ".png" });
// → "/app/uploads/photo.png"
```

---

## `__dirname` equivalent in ES Modules

CommonJS has `__dirname` built in; ES Modules don't, since a module's file location isn't automatically exposed the same way. The standard replacement:

```js
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const configPath = path.join(__dirname, "config.json");
```

This is one of the most common "how do I..." questions when migrating a project to ES Modules (see `11-module-system.md`).

---

## Relative paths between two locations

```js
path.relative("/app/uploads", "/app/logs/error.log");
// → "../logs/error.log"
```

Useful for generating a portable relative reference between two absolute paths — e.g. when writing paths into a config file that shouldn't hardcode machine-specific absolute locations.

---

## Cross-platform separators

```js
path.sep; // "/" on POSIX, "\\" on Windows
path.delimiter; // ":" on POSIX (used in PATH env var), ";" on Windows
```

You rarely need these directly since `join`/`resolve` handle separators for you — mainly useful if you're parsing a raw `PATH`-style environment variable yourself.

### Forcing POSIX or Windows behavior regardless of the current OS

```js
path.posix.join("a", "b"); // always uses "/", regardless of OS
path.win32.join("a", "b"); // always uses "\\", regardless of OS
```

Rarely needed, but useful for things like generating a URL path (always `/`-separated) using `path.posix` even on a Windows dev machine.

---

## A common real pattern: safe upload paths

```js
import path from "node:path";

const uploadsDir = path.join(__dirname, "uploads");
const safeFilename = path.basename(userProvidedFilename); // strip any directory traversal
const destination = path.join(uploadsDir, safeFilename);
```

`path.basename()` on user-provided input strips out any `../` a malicious filename might contain, which is an important defense against **path traversal** — without it, a filename like `../../etc/passwd` could let a file write escape the intended uploads folder entirely.

## Quick summary

- `path.join()` combines segments correctly for the current OS; `path.resolve()` guarantees an absolute result
- `basename`/`dirname`/`extname`/`parse`/`format` extract or rebuild the pieces of a path
- ES Modules need `fileURLToPath(import.meta.url)` + `path.dirname()` to replicate CommonJS's `__dirname`
- `path.basename()` on user input is a simple, important defense against path traversal when building file paths from user-supplied filenames
- `path.posix`/`path.win32` force a specific separator style regardless of the current OS

## Next

**`03-http.md`** covers Node's built-in HTTP module — the foundation Express and most Node web frameworks are actually built on top of.
