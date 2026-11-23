# Node.js Core Modules Cheatsheet

> Quick-reference guide covering modern Node.js native standard library modules with the `node:` protocol prefix: `fs/promises`, `path`, `crypto`, `util`, `test`, `stream`, and `worker_threads`.

| Metadata | Details |
| :--- | :--- |
| **Track** | Reference Cheatsheets (`27_cheatsheets`) |
| **Baseline** | Node.js 22 LTS / Node.js 24 |
| **Level** | Reference / Quick Lookup |

---

## 1. Filesystem (`node:fs/promises`)

```javascript
import fs from 'node:fs/promises';

// Reading and Writing Files:
const text = await fs.readFile('config.json', 'utf-8');
await fs.writeFile('output.txt', 'Content payload', 'utf-8');

// Directory Management:
await fs.mkdir('dist/assets', { recursive: true });
const entries = await fs.readdir('src', { withFileTypes: true });
for (const entry of entries) {
  console.log(entry.name, entry.isDirectory() ? '[DIR]' : '[FILE]');
}

// Verification & Deletion:
const exists = await fs.access('target.log').then(() => true).catch(() => false);
await fs.unlink('target.log'); // Delete file
await fs.rm('dist', { recursive: true, force: true }); // Delete folder
```

---

## 2. Path Manipulation (`node:path`)

```javascript
import path from 'node:path';

// Joining and Resolving (Platform-Agnostic):
path.join('dir', 'sub', 'file.js');       // 'dir/sub/file.js' (or 'dir\sub\file.js' on Win)
path.resolve('src', 'index.js');          // Absolute filesystem path

// Decomposition:
path.basename('/var/www/index.html');     // 'index.html'
path.dirname('/var/www/index.html');      // '/var/www'
path.extname('/var/www/index.html');      // '.html'

// Cross-Platform Separators:
path.sep; // '/' on POSIX, '\' on Windows
path.posix.join('a', 'b'); // Explicitly POSIX forward-slashes
```

---

## 3. Cryptography & UUIDs (`node:crypto`)

```javascript
import crypto from 'node:crypto';

// Fast UUID Generation:
const id = crypto.randomUUID(); // 'c9a646d3-9c61-4cc9-bc55-8...

// Cryptographic Hashing:
const hash = crypto.createHash('sha256').update('secret data').digest('hex');

// HMAC Authentication:
const signature = crypto.createHmac('sha256', 'api-secret').update('payload').digest('hex');

// Cryptographically Secure Random Bytes:
const token = crypto.randomBytes(32).toString('hex');
```

---

## 4. CLI Argument Parsing & Utilities (`node:util`)

```javascript
import { parseArgs, promisify } from 'node:util';

// Native CLI Flags & Arguments Parsing:
const { values, positionals } = parseArgs({
  args: process.argv.slice(2),
  options: {
    port: { type: 'string', short: 'p', default: '8080' },
    verbose: { type: 'boolean', short: 'v', default: false }
  },
  allowPositionals: true
});

console.log(values.port, values.verbose, positionals);
```

---

## 5. Native Test Runner (`node:test` & `node:assert/strict`)

```javascript
import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert/strict';

describe('Calculator', () => {
  it('adds two numbers correctly', () => {
    assert.equal(1 + 2, 3);
    assert.deepEqual({ a: 1 }, { a: 1 });
    assert.throws(() => { throw new Error('Boom'); }, /Boom/);
  });

  it('handles async resolutions', async () => {
    const res = await Promise.resolve(42);
    assert.equal(res, 42);
  });
});
```

---

## 6. Streaming Pipelines (`node:stream/promises`)

```javascript
import { pipeline } from 'node:stream/promises';
import fs from 'node:fs';
import zlib from 'node:zlib';

// Stream Gzip Compression Pipeline with Automatic Error Cleanup:
await pipeline(
  fs.createReadStream('input.txt'),
  zlib.createGzip(),
  fs.createWriteStream('input.txt.gz')
);
console.log('Compression complete.');
```
