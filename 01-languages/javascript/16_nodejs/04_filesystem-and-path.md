# Filesystem and Path

The `fs` module reads and writes files and directories. The `path` module builds and parses file paths in a cross-platform way. Prefer the **promise API** (`node:fs/promises`) for application code.

## Three API styles

```js
import fs from 'node:fs';
import fsp from 'node:fs/promises';

// 1. Promises (preferred)
const text = await fsp.readFile('a.txt', 'utf8');

// 2. Callbacks
fs.readFile('a.txt', 'utf8', (err, text) => { /* ... */ });

// 3. Sync (blocks the event loop: fine for scripts and startup code, not request handlers)
const text2 = fs.readFileSync('a.txt', 'utf8');
```

## Reading and writing files

```js
import fs from 'node:fs/promises';

// Read
const text = await fs.readFile('notes.txt', 'utf8');   // string
const buf  = await fs.readFile('image.png');           // Buffer (no encoding)
const data = JSON.parse(await fs.readFile('data.json', 'utf8'));

// Write (replaces the file)
await fs.writeFile('out.txt', 'hello\n');
await fs.writeFile('data.json', JSON.stringify(data, null, 2));

// Append
await fs.appendFile('app.log', `${new Date().toISOString()} started\n`);
```

Control how a file is opened with `flag`:

```js
await fs.writeFile('lock', pid, { flag: 'wx' });   // fail if it already exists
```

| Flag | Meaning |
|------|---------|
| `r` | Read; error if missing (default for reading) |
| `w` | Write; create or truncate (default for writing) |
| `a` | Append; create if missing |
| `wx` / `ax` | Like `w` / `a` but fail if the path exists |
| `r+` | Read and write; error if missing |

Atomic-ish update (write a temp file, then rename):

```js
await fs.writeFile('config.json.tmp', json);
await fs.rename('config.json.tmp', 'config.json');   // rename is atomic on the same filesystem
```

## Directories

```js
await fs.mkdir('a/b/c', { recursive: true });      // like mkdir -p; no error if it exists

const names = await fs.readdir('src');             // ['a.js', 'lib']
const entries = await fs.readdir('src', { withFileTypes: true });
for (const e of entries) {
  console.log(e.name, e.isDirectory() ? 'dir' : 'file');
}

const all = await fs.readdir('src', { recursive: true });   // every nested path

const tmp = await fs.mkdtemp('/tmp/app-');         // unique temp directory
```

Glob (Node 22+):

```js
for await (const file of fs.glob('src/**/*.js')) console.log(file);
```

## Metadata and existence

```js
const s = await fs.stat('file.txt');
s.size;               // bytes
s.mtime;              // modified Date
s.isFile();
s.isDirectory();
s.isSymbolicLink();   // use lstat() to inspect a link itself
```

Checking existence:

```js
async function exists(p) {
  try { await fs.access(p); return true; }
  catch { return false; }
}
```

Avoid "check then act" (`if (exists) read`): the file can change in between. Just attempt the operation and handle `ENOENT`:

```js
try {
  return await fs.readFile(file, 'utf8');
} catch (err) {
  if (err.code === 'ENOENT') return null;
  throw err;
}
```

Common error codes:

| Code | Meaning |
|------|---------|
| `ENOENT` | No such file or directory |
| `EEXIST` | File already exists |
| `EACCES` / `EPERM` | Permission denied |
| `EISDIR` / `ENOTDIR` | Expected file but got dir, or the reverse |
| `ENOTEMPTY` | Directory not empty |
| `EMFILE` | Too many open files |

## Copy, move, delete

```js
await fs.copyFile('a.txt', 'b.txt');
await fs.cp('src', 'backup', { recursive: true });       // copy a directory tree
await fs.rename('old.txt', 'new.txt');                   // move or rename
await fs.unlink('file.txt');                             // delete a file
await fs.rm('build', { recursive: true, force: true });  // like rm -rf (force ignores missing)
await fs.rmdir('empty-dir');                             // only empty directories
```

`rename` fails across different filesystems (`EXDEV`); copy then delete instead.

## Large files: use streams

`readFile` loads the **entire file** into memory. For big files, stream:

```js
import { createReadStream, createWriteStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';

await pipeline(createReadStream('big.csv'), createWriteStream('copy.csv'));
```

Read line by line:

```js
import { createReadStream } from 'node:fs';
import readline from 'node:readline';

const rl = readline.createInterface({ input: createReadStream('big.log'), crlfDelay: Infinity });
for await (const line of rl) {
  if (line.includes('ERROR')) console.log(line);
}
```

See [Streams](./06_streams.md).

## File handles

For several operations on one open file:

```js
const fh = await fs.open('data.bin', 'r');
try {
  const { bytesRead, buffer } = await fh.read(Buffer.alloc(16), 0, 16, 0);
  const { size } = await fh.stat();
} finally {
  await fh.close();    // always close
}
```

`await using` closes file handles automatically in runtimes that support explicit resource management.

## Watching files

```js
const ac = new AbortController();

setTimeout(() => ac.abort(), 60_000);

try {
  for await (const event of fs.watch('src', { recursive: true, signal: ac.signal })) {
    console.log(event.eventType, event.filename);   // 'rename' | 'change'
  }
} catch (err) {
  if (err.name !== 'AbortError') throw err;
}
```

`fs.watch` behavior varies by OS (duplicate events, missing filenames). Debounce events and consider a library such as `chokidar` for robust watching. For simple dev reloads, `node --watch` is enough.

## The `path` module

```js
import path from 'node:path';

path.join('src', 'lib', '..', 'index.js');    // 'src/index.js' (normalizes)
path.resolve('src', 'index.js');              // absolute path from cwd
path.resolve('/a', '/b', 'c');                // '/b/c' (an absolute segment resets)

path.basename('/a/b/file.txt');               // 'file.txt'
path.basename('/a/b/file.txt', '.txt');       // 'file'
path.dirname('/a/b/file.txt');                // '/a/b'
path.extname('archive.tar.gz');               // '.gz'

path.parse('/home/u/file.txt');
// { root: '/', dir: '/home/u', base: 'file.txt', ext: '.txt', name: 'file' }
path.format({ dir: '/home/u', base: 'file.txt' });

path.relative('/a/b/c', '/a/d');              // '../../d'
path.isAbsolute('/a');                        // true
path.sep;                                     // '/' on POSIX, '\\' on Windows
```

| Function | Use |
|----------|-----|
| `join` | Combine segments and normalize |
| `resolve` | Make an absolute path (right to left, until absolute) |
| `normalize` | Clean `.`, `..`, duplicate separators |
| `relative` | Path from one location to another |
| `path.posix` / `path.win32` | Force one style regardless of OS |

Never build paths with string concatenation (`dir + '/' + name`): separators differ between operating systems.

## Locating your own files

```js
// ES modules (Node 20.11+)
const dir  = import.meta.dirname;
const file = import.meta.filename;
const data = path.join(import.meta.dirname, 'data', 'seed.json');

// Older ES modules
import { fileURLToPath } from 'node:url';
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// CommonJS: __dirname and __filename exist already
```

Relative paths like `'./data.json'` are resolved against `process.cwd()` (where the user ran the command), **not** against the file containing the code. Use `import.meta.dirname` for files that ship with your code.

## URLs and paths

```js
import { fileURLToPath, pathToFileURL } from 'node:url';

fileURLToPath('file:///home/u/a.txt');   // '/home/u/a.txt'
pathToFileURL('/home/u/a.txt').href;     // 'file:///home/u/a.txt'

await fs.readFile(new URL('./data.json', import.meta.url), 'utf8');   // URL objects work
```

## Path traversal (security)

Never join untrusted input to a base directory without checking where it lands:

```js
function safeJoin(base, userInput) {
  const root = path.resolve(base);
  const target = path.resolve(root, userInput);
  if (target !== root && !target.startsWith(root + path.sep)) {
    throw new Error('Path traversal blocked');
  }
  return target;
}

safeJoin('/srv/public', '../../etc/passwd');   // throws
```

Symlinks can still point outside the base; use `fs.realpath` on the result when that matters. See [Security](../22_security/00_README.md).

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `readFileSync` in a request handler | Blocks the loop | `fs/promises` |
| `readFile` on huge files | Memory blowup | Streams |
| Check-then-use (`exists` then `read`) | Race condition | Try, then handle `ENOENT` |
| Concatenating paths with `/` | Breaks on Windows | `path.join` |
| Relative paths assuming script location | Depends on the cwd | `import.meta.dirname` |
| Joining user input into paths | Path traversal | Resolve and verify the prefix |
| Forgetting to close file handles | Descriptor leaks, `EMFILE` | `try/finally` or streams |
| Forgetting the encoding | You get a `Buffer`, not a string | Pass `'utf8'` |

## Key takeaways

- Use `node:fs/promises`; reserve `*Sync` for scripts and startup
- Stream big files instead of reading them whole
- Handle errors by `err.code` (`ENOENT`, `EEXIST`, `EACCES`)
- Use `path.join` / `path.resolve`, never string concatenation
- Use `import.meta.dirname` to locate files next to your code
- Validate any path built from user input

**Next:** [Events](./05_events.md)
